#include <string>
#include <algorithm>
#include <grpcpp/grpcpp.h>
#include "psudsm.grpc.pb.h"
#include <pthread.h>
#include <map>
#include <vector>
#include <tuple>
#include <sys/mman.h>
#include <fstream>

using std::to_string;
using std::map;
using std::vector;
using std::tuple;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using psudsm::PollCheck;
using psudsm::PollMessage;
using psudsm::PollReply;

using psudsm::PageRead;
using psudsm::PageReply;
using psudsm::PageRequest;

using namespace std;
string myport;
vector<std::string> ports_list;
map <std::string, int> portMap;
map <int, std::string> rPortMap;
int NUM_PORTS = 0;

#define PAGE_SIZE 4096

//Starting page address stored globally
int addr;
void* pg_addr;
int page_id = 0;

struct page_info_t{
	void* pg_addr;
	int size;
	int page_id;
};

typedef struct page_info_t page_info_t;
std::vector<page_info_t> my_pages;

std::ofstream op_file;
//("psu_node_rpc_log.txt", std::ofstream::app | std::ofstream::out);

class PollClient {
    public:
        PollClient(std::shared_ptr<Channel> channel) : stub_(PollCheck::NewStub(channel)) {}

	std::string sendReadRequest(int page_id, int node_id) {
			std::cout<<"Sending page read request to directory >> \n";
	       	PageRead pg_read;
	
	        pg_read.set_page_id(page_id);
	        pg_read.set_from_port_id(node_id);
	
	        PageReply pg_reply;
	        ClientContext context;
			std::cout<<"gonna send read request >>>\n";
			op_file<<"Sending Page Read Request to Directory : page_id : "<<page_id<<" : from node_id : "<<node_id<<"\n"<<std::flush;
	        Status status = stub_->sendReadRequest(&context, pg_read, &pg_reply);
	
	        if(status.ok()){
	        	std::cout<<"Received page from directory >>>> \n";
	        	//std::vector<int> v(pg_reply.page_value().begin(),pg_reply.page_value().end());
	            return pg_reply.page_value();
	         } else {
	            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
	            std::string a = "";
	            return a;
	        }
	}
	    
    int sendRequest(int message_type, int page_id, int node_id) {
       	PollMessage msg;

        msg.set_message_type(message_type);
        msg.set_page_id(page_id);
        msg.set_from_port_id(node_id);

        PollReply reply;
        ClientContext context;
        
        op_file<<"Sending Request to Directory : page_id : "<<page_id<<" : message_type : "<<message_type<<"\n"<<std::flush;
        Status status = stub_->sendRequest(&context, msg, &reply);

        if(status.ok()){
            return reply.result();
         } else {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return -1;
        }
    }
    
    private:
        std::unique_ptr<PollCheck::Stub> stub_;
};

class PollService final : public PollCheck::Service {

	Status sendWriteAccess(
		ServerContext* context,
		const PageReply* pg_acc,
		PollReply* reply
		) override {
	
			std::cout<<"Received a write access from directory >>> \n";		
			int page_id = pg_acc->page_id();
			std::string page_value = pg_acc->page_value();
			std::cout<<"Changing the page to write-able...\n";
			
			mprotect(pg_addr + page_id*PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE);

			if(page_value.compare("NoChange")){
				void* start_addr;
				for (int i=0; i<my_pages.size(); i++){
					if(my_pages[i].page_id == page_id){
						start_addr = my_pages[i].pg_addr;
						break;
					}
				}

				std::cout<<"\nNow copying page\n";

				const char* p_data = page_value.c_str();
				memcpy(start_addr, p_data, PAGE_SIZE);
			} 	
			
			return Status::OK;
	}
		
	Status sendPageRequest(
	ServerContext* context,
	const PageRequest* pg_req,
	PageReply* pg_reply
	) override {

		std::cout<<"Received a page request from directory >>> \n";		
		int page_id = pg_req->page_id();
		int convert_rw = pg_req->convert_rw();
		
		void* start_addr;
		for (int i=0; i<my_pages.size(); i++){
			if(my_pages[i].page_id == page_id){
				start_addr = my_pages[i].pg_addr;
				break;
			}
		}

		int* address = (int *)start_addr;
		int s_v = (int) *(address);
		std::cout<<"Value at starting address >>>>> "<<s_v<<"\n";
		char* page = new char[PAGE_SIZE];
		memcpy(page, start_addr, PAGE_SIZE);
		std::string page_data(page, PAGE_SIZE);
		pg_reply->set_page_value(page_data);

		//Make the page read only
		if (convert_rw){
			mprotect(start_addr, PAGE_SIZE, PROT_READ);
		}
		
		return Status::OK;
	}
		
    Status sendRequest(
	ServerContext* context,
	const PollMessage* msg,
	PollReply* reply
	) override {

		reply->set_result(1);
		
		int msg_type = msg->message_type();
		int page_id = msg->page_id();
		int recv_port_id = msg->from_port_id();

		if (msg_type == 0){
			std::cout<<"Received poll from << "<<recv_port_id<<"\n";
		} else if (msg_type == 4){
			// Invalidate page
			std::cout<<"Invalidating page <<< "<<page_id<<"\n"<<pg_addr+page_id*PAGE_SIZE<<"\n";
			mprotect(pg_addr + page_id*PAGE_SIZE, PAGE_SIZE, PROT_NONE);
		}
		return Status::OK;
	}


};


int Run_PollClient(int message_type, std::string send_port_num, int recv_port_id) {
    std::string address = send_port_num;

    PollClient client(
        grpc::CreateChannel(
            address, 
            grpc::InsecureChannelCredentials()
        )
    );
    
    int response;
    response = client.sendRequest(message_type, 0, recv_port_id);
	std::cout << "Received poll ACK addr"<<address<<"--"<< response << std::endl;
   	return response;
};

int Send_Register(int page_id, int myport_id){
	std::string dir_address = ports_list.back();
	PollClient dir_client(
		grpc::CreateChannel(
		    dir_address, 
		    grpc::InsecureChannelCredentials()
		)
	    );
    std::cout<<"Sending register page for"<<page_id<<"--"<<myport_id<<"\n";
    int response;
    response = dir_client.sendRequest(1, page_id, myport_id);
    std::cout << "Received register page ack" << response << std::endl;
   	return response;
}

int Write_Page(int page_id, int myport_id){
std::string dir_address = ports_list.back();
PollClient dir_client(
        grpc::CreateChannel(
            dir_address, 
            grpc::InsecureChannelCredentials()
        )
    );
    std::cout<<"DOING WRITE PAGE >>> \n";
    int response;
	std::cout<"Before sending request >>> \n";
    response = dir_client.sendRequest(3, page_id, myport_id);
    std::cout << "Received write page response" << response << std::endl;
   return response;
}

std::string Read_Page(int page_id, int myport_id){
std::string dir_address = ports_list.back();
PollClient dir_client(
        grpc::CreateChannel(
            dir_address, 
            grpc::InsecureChannelCredentials()
        )
    );
	 std::cout<<"DOING READ PAGE REQ >> \n";
	 std::string  response;
	 response = dir_client.sendReadRequest(page_id, myport_id);
	 std::cout<<"Received some response for page read request of size"<<response.size()<<std::endl;
	 return response;
	 
}

int Send_Write_Ack(int page_id, int myport_id){
std::string dir_address = ports_list.back();
PollClient dir_client(
        grpc::CreateChannel(
            dir_address, 
            grpc::InsecureChannelCredentials()
        )
    );
    int response;
    response = dir_client.sendRequest(4, page_id, myport_id);
	std::cout << "Received send write ACK ack" << response << std::endl;
   	return response;
}

int Send_Read_Ack(int page_id, int myport_id){
std::string dir_address = ports_list.back();
PollClient dir_client(
        grpc::CreateChannel(
            dir_address, 
            grpc::InsecureChannelCredentials()
        )
    );
    int response;
    response = dir_client.sendRequest(5, page_id, myport_id);
	std::cout << "Received send read ACK ack" << response << std::endl;
   	return response;
}

int Iam_Ready(int myport_id){
std::string dir_address = ports_list.back();
PollClient dir_client(
        grpc::CreateChannel(
            dir_address, 
            grpc::InsecureChannelCredentials()
        )
    );
	int response;
	response = dir_client.sendRequest(6, 0, myport_id);
	return response;
}

void *Run_Server(void *ptr){
	std::string address("");
	char *port = (char *)ptr;
	address.append(port);
	PollService service;
	std::cout<<"\nDOING RUN SERVER 1 >>>"<<address<<" \n";

	ServerBuilder builder;

	builder.AddListeningPort(address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);

	std::cout<<"Now creating server builder >> .. \n";
	std::unique_ptr<Server> server(builder.BuildAndStart());
	std::cout << "Server listening on port: " << address << std::endl;
	server->Wait();
};
