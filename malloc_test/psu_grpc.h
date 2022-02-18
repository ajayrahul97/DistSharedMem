#pragma once
#include <string>
#include <algorithm>
#include <grpcpp/grpcpp.h>
#include <pthread.h>
#include <map>
#include <vector>
#include <tuple>

#include "psudsm.grpc.pb.h"

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

using psudsm::LockMessage;
using psudsm::LockReply;


int highest_seqnum = 0;
int myseqnum = 0;

vector<std::string> l_ports_list;

//#define L_NUM_PORTS 2
//#define L_NUM_PORTS 4
int L_NUM_PORTS = 0;
int NUM_PORTS = 0;
//int deferred_req[2] = {0,0};
int deferred_req[4] = {0,0,0,0};
bool requesting_cs;
int outstanding_reply;

std::string myport;
vector<std::string> ports_list;
map <std::string, int> portMap;
map <int, std::string> rPortMap;

#define PAGE_SIZE 4096

//Starting page address stored globall[O[Iy
int addr;
void* pg_addr;
int page_id = 0;
bool initialized = false;

struct page_info_t{
	void* pg_addr;
	int size;
	int page_id;
	std::string page_name;
};

typedef struct page_info_t page_info_t;
std::vector<page_info_t> my_pages;


class PollClient {
    public:
        PollClient(std::shared_ptr<Channel> channel) : stub_(PollCheck::NewStub(channel)) {}

	std::string sendReadRequest(int page_id, int node_id, std::string name) {
			std::cout<<"Sending page read request to directory >> \n";
	       	PageRead pg_read;
	
	        pg_read.set_page_id(page_id);
	        pg_read.set_from_port_id(node_id);
			pg_read.set_page_name(name);
	
	        PageReply pg_reply;
	        ClientContext context;
			std::cout<<"gonna send read request >>>\n";
	        Status status = stub_->sendReadRequest(&context, pg_read, &pg_reply);
	
	        if(status.ok()){
	        	std::cout<<"Received page from directory >>>> \n";
	            return pg_reply.page_value();
	         } else {
	            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
	            std::string a = "";
	            return a;
	        }
	}
	    
    int sendRequest(int message_type, int page_id, int node_id, std::string name) {
       	PollMessage msg;

        msg.set_message_type(message_type);
        msg.set_page_id(page_id);
        msg.set_from_port_id(node_id);
		msg.set_page_name(name);

        PollReply reply;
        ClientContext context;

        Status status = stub_->sendRequest(&context, msg, &reply);

        if(status.ok()){
            return reply.result();
         } else {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return -1;
        }
    }

    int sendLockRequest(int message_type, int seq_num, int node_id) {
           	LockMessage msg;
    
    		msg.set_myseqno(seq_num);
            msg.set_message_type(message_type);
            msg.set_nodeid(node_id);
    
            LockReply reply;
            ClientContext context;
    
            Status status = stub_->sendLockRequest(&context, msg, &reply);
    
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
		std::string page_name = pg_acc->page_name();
		std::cout<<"Changing the page to write-able .. \n";

		void* start_addr;
		bool page_found = false;
		for (int i=0; i<my_pages.size(); i++){
			if(my_pages[i].page_id == page_id && page_name.compare(my_pages[i].page_name)==0){
				start_addr = my_pages[i].pg_addr;
				page_found = true;
				break;
			}
		}
		if (page_found){
			mprotect(start_addr, PAGE_SIZE, PROT_READ | PROT_WRITE);
			if(page_value.compare("NoChange")) {
				std::cout<<"\nNow copying page\n";
				const char* p_data = page_value.c_str();
				memcpy(start_addr, p_data, PAGE_SIZE);
				
			}
		} else {
			std::cout<<"Page not found !!!\n";
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
		std::string page_name = pg_req->page_name();

		void* start_addr;
		bool page_found = false;
		for (int i=0; i<my_pages.size(); i++){
			if(my_pages[i].page_id == page_id && page_name.compare(my_pages[i].page_name)==0){
				start_addr = my_pages[i].pg_addr;
				page_found = true;
				break;
			}
		}
		if (page_found) {
			int* address = (int *)start_addr;
			char* page = new char[PAGE_SIZE];
			memcpy(page, start_addr, PAGE_SIZE);
			std::string page_data(page, PAGE_SIZE);
			pg_reply->set_page_value(page_data);
			
			//Make the page read only
			if (convert_rw){
				std::cout<<"making myself ro\n";
				mprotect(start_addr, PAGE_SIZE, PROT_READ);
			}

		} else {
			std::cout<<"Page not found \n";
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
		std::string page_name = msg->page_name();

		if (msg_type == 0){
			std::cout<<"Received poll from << "<<recv_port_id<<"\n";
		} else if (msg_type == 4){
			// Invalidate page if it has been registered.
			bool page_found = false;
			void* start_addr;
			for(int i=0; i<my_pages.size(); i++){
				if( my_pages[i].page_id == page_id && page_name.compare(my_pages[i].page_name)==0 ){
					start_addr = my_pages[i].pg_addr;
					page_found = true;
					break;
				}
			}
			if(page_found){
				std::cout<<"Invalidating page <<< "<<page_id<<"\n"<<start_addr<<"\n";
				mprotect(start_addr, PAGE_SIZE, PROT_NONE);
			} else {
				std::cout<<"Invalidate not done : page not found\n";
			}
		}
		return Status::OK;
	}


	Status sendLockRequest(
		ServerContext* context,
		const LockMessage* msg,
		LockReply* reply
		) override {
	
			reply->set_result(1);
			
			int msg_type = msg->message_type();
			int recv_seqnum = msg->myseqno();
			int recv_port_id = msg->nodeid();
			
			if (msg_type == 1){
				// REQUEST
				std::cout <<"HIGHEST SEQ NUM BEFORE :  "<<highest_seqnum<<"\n";
				highest_seqnum = std::max(highest_seqnum, recv_seqnum);
				std::cout <<"HIGHEST SEQ NUM AFTER : "<<highest_seqnum<<"\n";
				if (requesting_cs && ((recv_seqnum > myseqnum) ||
				 ( recv_seqnum == myseqnum) && (recv_port_id > portMap.at(myport))))
				{
					//DEFER THE REPLY
					std::cout<<"Deferring the reply : "<<recv_port_id<<"\n";
					int  j = 0;
					for (int i = 0; i < L_NUM_PORTS; i++){
						if (recv_port_id == portMap.at(l_ports_list[i])){
							j = i;
							break;
						}
					}
					deferred_req[j] = 1;
					
				} else {
					// SEND REPLY
					std::cout<<"Sending back reply to - "<<recv_port_id<<"\n";
					reply->set_result(2);
				}
				
			} else if (msg_type == 2){
				//REPLY
				std::cout<<"Reducing outstanding reply count \n";
				outstanding_reply -= 1;
			} else {
				//PING PONG
				;
			}
			
			return Status::OK;
	
		}
};


int Run_Client(int message_type, int myseqnum, int send_port_num, int recv_port_id) {
    std::string address("0.0.0.0:");
    address += to_string(send_port_num);
    
    PollClient client(
        grpc::CreateChannel(
            address, 
            grpc::InsecureChannelCredentials()
        )
    );
    
    int response;
    response = client.sendLockRequest(message_type, myseqnum, recv_port_id);
    std::cout << "Response received " << response << std::endl;
   	return response;
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
    response = client.sendRequest(message_type, 0, recv_port_id, "");
	std::cout << "Received poll ACK addr"<<address<<"--"<< response << std::endl;
   	return response;
};

int Send_Register(int page_id, int myport_id, std::string page_name){
	std::string dir_address = ports_list.back();
		PollClient dir_client(
			grpc::CreateChannel(
			    dir_address, 
			    grpc::InsecureChannelCredentials()
			)
		    );
	std::cout<<"Sending register page for"<<page_id<<"--"<<myport_id<<"\n";
    int response;
    response = dir_client.sendRequest(1, page_id, myport_id, page_name);
    std::cout << "Received register page ack" << response << std::endl;
   	return response;
}

int Write_Page(int page_id, int myport_id, std::string page_name){
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
    response = dir_client.sendRequest(3, page_id, myport_id, page_name);
	std::cout << "Received write page response" << response << std::endl;
   	return response;
}

std::string Read_Page(int page_id, int myport_id, std::string page_name){
	std::string dir_address = ports_list.back();
	PollClient dir_client(
			grpc::CreateChannel(
				dir_address, 
				grpc::InsecureChannelCredentials()
			)
		);
	std::cout<<"DOING READ PAGE REQ >> \n";
	 std::string response;
	 response =dir_client.sendReadRequest(page_id, myport_id, page_name);
	 std::cout<<"Received some response for page read request of size"<<response.size()<<std::endl;
	 return response;
	 
}

int Lock(int myport_id){
std::string dir_address = ports_list.back();
PollClient dir_client(
        grpc::CreateChannel(
            dir_address, 
            grpc::InsecureChannelCredentials()
        )
    );
	int response;
	response = dir_client.sendRequest(7,0,myport_id, "");
	return response;
}

int Unlock(int myport_id){
std::string dir_address = ports_list.back();
PollClient dir_client(
        grpc::CreateChannel(
            dir_address, 
            grpc::InsecureChannelCredentials()
        )
    );
	int response;
	response = dir_client.sendRequest(8,0,myport_id, "");
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
	response = dir_client.sendRequest(6, 0, myport_id, "");
	return response;
}

int Barrier2(int myport_id){
std::string dir_address = ports_list.back();
PollClient dir_client(
        grpc::CreateChannel(
            dir_address, 
            grpc::InsecureChannelCredentials()
        )
    );
	int response;
	response = dir_client.sendRequest(9, 0, myport_id, "");
	return response;
}


int Barrier3(int myport_id){
std::string dir_address = ports_list.back();
PollClient dir_client(
        grpc::CreateChannel(
            dir_address, 
            grpc::InsecureChannelCredentials()
        )
    );
	int response;
	response = dir_client.sendRequest(10, 0, myport_id, "");
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
