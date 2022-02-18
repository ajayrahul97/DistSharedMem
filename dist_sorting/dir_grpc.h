#include <string>
#include <algorithm>
#include <grpcpp/grpcpp.h>
#include "psudsm.grpc.pb.h"
#include <pthread.h>
#include <vector>
#include <bits/stdc++.h>
#include <tuple>
#include <mutex>
#include <map>

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
using psudsm::PageRequest;
using psudsm::PageReply;

using namespace std;

string myport;

vector<std::string> ports_list;
map <std::string, int> portMap;
map <int, std::string> rPortMap;


int node_counter = 0;
int barrier_counter = 0;
int barrier2_counter = 0;
int lock_val = 0;
std::mutex mtx, mut, b_lock, b2_lock;
std::mutex lock1, lock2;

int D_NUM_PORTS = 0;

typedef std::tuple<int, std::vector<int>, int, pthread_mutex_t*> page_entry;
std::vector<page_entry> table;

class PollClient {
    public:
        PollClient(std::shared_ptr<Channel> channel) : stub_(PollCheck::NewStub(channel)) {}

    int sendRequest(int message_type, int page_id, int node_id) {
       	PollMessage msg;

        msg.set_message_type(message_type);
        msg.set_page_id(page_id);
        msg.set_from_port_id(node_id);

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

	int sendWriteAccess(int page_id, std::string page_val){
		PageReply pg_acc;

        pg_acc.set_page_id(page_id);
        pg_acc.set_page_value(page_val);

        PollReply reply;
        ClientContext context;

        Status status = stub_->sendWriteAccess(&context, pg_acc, &reply);

        if(status.ok()){
            return reply.result();
        } else {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return -1;
        }

		
	}
	
    std::string sendPageRequest(int page_id, int convert_rw) {
           	PageRequest pg_req;
    
            pg_req.set_page_id(page_id);
            pg_req.set_convert_rw(convert_rw);
            
            PageReply pg_reply;
            ClientContext context;
    
            Status status = stub_->sendPageRequest(&context, pg_req, &pg_reply);
    
            if(status.ok()){
            	std::cout<<"Got the page values from the other node \n";
            	return pg_reply.page_value();

            } else {
                std::cout << status.error_code() << ": " << status.error_message() << std::endl;
                std::string a = "";
                return a;
            }
      }

    private:
        std::unique_ptr<PollCheck::Stub> stub_;
};

class PollService final : public PollCheck::Service {
	Status sendReadRequest(
		ServerContext* context,
		const PageRead* pg_read,
		PageReply* reply
	) override {
		int from_port_id = pg_read->from_port_id();
		int page_id = pg_read->page_id();
		std::cout<<"inside sendReadRequest of directory ------"<<page_id<<"---"<<from_port_id<<" \n";		
		std::string  page_val = get_page(page_id, from_port_id);
		reply->set_page_value(page_val);
		return Status::OK;
	}
	
    Status sendRequest(
	ServerContext* context,
	const PollMessage* msg,
	PollReply* reply
	) override {

		reply->set_result(1);
		
		int msg_type = msg->message_type();
		int recv_port_id = msg->from_port_id();

		if (msg_type == 0){
			// POLL		
			std::cout<<"Received poll from << "<<recv_port_id<<"\n";
			
		} else if (msg_type == 1) {
			// REGISTER
			std::cout<<"Register page request received in directory\n";
			//int ack = register_page(pg_id, portMap(recv_port_id));
			int page_id = msg->page_id();
			int from_port_id = msg->from_port_id();
			std::cout<<"Receieved register request from << "<<from_port_id<<"\n";
			register_page(page_id, from_port_id);
			mtx.lock();
			node_counter--;
			mtx.unlock();
			while(node_counter>0);
			
		} else if (msg_type == 2) {
		
			std::cout<<"Read request received from << "<<recv_port_id<<"\n";
			
		} else if (msg_type == 3) {
			std::cout<<"Write request received from << "<<recv_port_id<<"\n";
			int page_id = msg->page_id();
			int from_port_id = msg->from_port_id();
			write_page(page_id, from_port_id);
			
		} else if (msg_type == 4){
			std::cout<<"Write ACK received from << "<<recv_port_id<<"\n"; 
			int page_id = msg->page_id();
			int from_port_id = msg->from_port_id();
			ack_write_page(page_id, from_port_id);
			
		} else if (msg_type == 5){
			std::cout<<"Page copied ACK received from <<"<<recv_port_id<<"\n";
			int page_id = msg->page_id();
			int from_port_id = msg->from_port_id();
			//ack_write_page(page_id, from_port_id);
		} else if (msg_type == 6){
			std::cout<<"Received a barrier request from --"<<recv_port_id<<"\n";
			b_lock.lock();
			barrier_counter--;
			b_lock.unlock();
			while(barrier_counter > 0);
		} else if (msg_type == 7){
			std::cout<<"Received a lock request from -- "<<recv_port_id<<"\n";
			while(lock_val!=0){
				std::cout<<"Waiting for lock to be 0 again ..\n";
			}
			mut.lock();
			lock_val = 1;
			mut.unlock();
			std::cout<<"Gave the lock to --"<<recv_port_id<<"\n";
		} else if (msg_type == 8){
			std::cout<<"Received a unlock request from -- "<<recv_port_id<<"\n";
			//mut.lock();
			lock_val = 0;
			//mut.unlock();
		}else if (msg_type == 9){
			std::cout<<"Received a barrier2 request from --"<<recv_port_id<<"\n";
			b2_lock.lock();
			barrier2_counter--;
			b2_lock.unlock();
			while(barrier2_counter > 0);
		}
		
		return Status::OK;
	}
	

int register_page(int pg_id, int port_id){
	lock2.lock();
	bool new_entry = true;
	if (table.size()){
		for (int i=0; i<table.size();i++){
			// If a page exists already, and is in read only mode
			// then update the new ports access bit to read only
			// read only bit = 1  in RO
			
			if(std::get<0>(table[i]) == pg_id && std::get<2>(table[i]) == 0){
				std::cout<<"Updating existing entry in the table\n";
				page_entry p = table[i];
				vector<int> v = std::get<1>(p);
				v[port_id] = 1;
				std::get<1>(table[i]) == v;
				new_entry = false;
				break;
			}
		}
	} 

	if (new_entry){
		std::cout<<"Creating new page entry and pushing into table\n";
		vector<int> v {0,0,0,0};
		//std::mutex mtx = new std::mutex();
		page_entry pg_new; 
		pthread_mutex_t *mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
		pg_new = make_tuple(pg_id, v, 0, mutex);
		table.push_back(pg_new);
	}

	std::cout<<"Unlocking page register lock >>> \n";
	lock2.unlock();
	std::cout<<"Now returning from register_page >>> \n";
	return 1;
}

std::string get_page(int pg_id, int port_id){
	// When a node X  wants to read 
		
	// if the pg is in RW mode
		// Check who has read-write access, say Y has it
		// Send that node Y a read-only convert msg
		// And get the page from that node Y
		// Change the presence bits and R/W status of the page in dir
		// Send the value to X
		// After Ack unlock

	// else if the pg is in RO mode
		// go to first node that has the read access, say it is Y
		// request for the page value from that node Y
		// Change the presence bits in dir
		// send the value to X
		// After Ack unlock

	std::string page_val;
	std::cout<<"inside get page ---- \n";
	if (table.size()){
		std::cout<<"size of table --"<<table.size()<<"\n";
		for (int i=0; i<table.size(); i++){
			if (std::get<0>(table[i]) == pg_id){

				//Lock the page
				
				if (pg_id == 0){
					lock1.lock();
				} else {
					lock2.lock();
				}
				
				
				//pthread_mutex_lock(std::get<3>(table[i]));
				std::cout<<"Inside read lock of page <<"<<pg_id<<"requested by --"<<port_id<<"\n";
				vector<int> v = std::get<1>(table[i]);
				std::cout<<"Size of vector -- "<<v.size()<<"of row <<"<<i<<"\n";
				int req_port_id;
				// Find the port having the page
				for (int i=0; i<v.size(); i++){
					if (v[i] == 1){
						req_port_id = i;
						break;
					}
				}
				std::cout<<"Found the port having the page << "<<req_port_id<<"\n";
			
				//If page was in RW mode - convert it into RO mode
				if (std::get<2>(table[i])){
					//Page is in RW mode
					// change to RO mode
					std::cout<<"Page is in RW mode.. requesting value of page\n";
					page_val = Page_Val_Req(pg_id, req_port_id, 1);
					// change the presence bits;
					v[port_id] = 1;
					std::get<2>(table[i]) == 0;
				} else {
					// Page is in RO mode;
					std::cout<<"Page is in RO mode.. requesting value of page\n";
					page_val = Page_Val_Req(pg_id, req_port_id, 0);	
					// change the presence bits -
					v[port_id] = 1;
				}
					
				std::cout<<"Exiting read lock of page <<"<<pg_id<<"requested by"<<port_id<<"\n";
				//Unlock the page
				
				if (pg_id == 0){
					lock1.unlock();
				} else {
					lock2.unlock();
				}
				//pthread_mutex_unlock(std::get<3>(table[i]));
				
				break;			
			}
		}
	}
	if (!page_val.size()){
		std::cout<<"EMPTY PAGE FETCHED >>>>> !! \n";
	}
	return page_val;
}	

// Function to get page val
std::string Page_Val_Req(int pg_id, int port_id, int convert_rw){
	
	std::string address = rPortMap.at(port_id);
	std::cout<<"Requesting page << "<<pg_id<<".. from  <<"<<address<<"\n";
	PollClient client(
        grpc::CreateChannel(
            address, 
            grpc::InsecureChannelCredentials()
        )
    );
    std::string response = client.sendPageRequest(pg_id, convert_rw);
   	return response;
}

void write_page(int pg_id, int port_id){
	// When some node wants to write on a page
	// First send invalidation to others and then
	// Change the bits in the vector
	if (table.size()){
		for (int i=0; i < table.size(); i++){
			if (std::get<0>(table[i]) == pg_id){
				
				if(pg_id ==0){
					lock1.lock();
				}else{
					lock2.lock();
				}
				//pthread_mutex_lock(std::get<3>(table[i]));
				std::cout<<"Inside write lock for page <<"<<pg_id<<" from port"<<port_id;

				std::string page_val = "NoChange";
				vector<int> v = std::get<1>(table[i]);

				if (std::get<2>(table[i])){
					int req_port_id = -1;

					for(int i =0; i<v.size(); i++){
						if(v[i] == 1){
							req_port_id = i;
							break;
						}
					}
					std::cout<<"Found the port having the page "<<req_port_id<<"\n";
					page_val = Page_Val_Req(pg_id, req_port_id, 1);
				}				

				
				// Send invalidation
				invalidate_others(pg_id, port_id);
					
				// Change the bits
				std::cout<<"done sending invalidation\n";
				v = {0, 0, 0, 0};
				v[port_id] = 1;
				std::get<1>(table[i]) = v;
				// Set it to RW mode.
				std::get<2>(table[i]) = 1;

				//Send the node a write access request
				int resp = -1;
				while(resp == -1){
					resp = Send_Write_Access(pg_id, rPortMap.at(port_id), page_val);				
				}
				// Release lock	
				std::cout<<"Releasing write lock for page <<"<<pg_id<<"requested by"<<port_id<<"\n";	
				if(pg_id ==0){
					lock1.unlock();
				}else{
					lock2.unlock();
				}
				//pthread_mutex_unlock(std::get<3>(table[i]));
				break;
			}
		}
				
	} else {
		std::cout<<"ERROR - TABLE EMPTY\n";
	}
}

//Function to release lock after a page has been written or after a page has been read
void ack_write_page(int pg_id, int port_id){
	std::cout<<"ack_write_page >>> \n";
	for (int i=0; i<table.size(); i++){
		if (std::get<0>(table[i]) == pg_id){
			//Release lock
			std::cout<<"Releasing the lock for page id >>>"<<pg_id<<"\n";
			if(pg_id==0){
				lock1.unlock();
			}else{
				lock2.unlock();
			}
			//pthread_mutex_unlock(std::get<3>(table[i]));
			break;
		}		
	}	
}

// Function to send invalidation to other nodes
void invalidate_others(int pg_id, int from_port_id){
	//Send invalidation for pg_id to all ports except port_id
	string port_num = rPortMap.at(from_port_id);
	std::cout<<"My port num >>"<<port_num<<"\n";
	for (int i=0; i< D_NUM_PORTS; i++){
		if (ports_list[i].compare(port_num) && ports_list[i].compare(ports_list.back())){
			std::cout<<"Sending invalidate to << "<<ports_list[i]<<"\n";
			int resp = -1;
			while(resp == -1){
				resp = Send_Invalidate(pg_id, ports_list[i], from_port_id);
			}
		}
	}
}

// Function to send invalidation to one node
int Send_Invalidate(int pg_id, std::string port_num, int from_port_id){
	// Send invalidation message to given port_id
	std::string address = port_num;
	std::cout<<from_port_id<<" Sending invalidate to <<"<<address<<"\n";
	PollClient client(
        grpc::CreateChannel(
            address, 
            grpc::InsecureChannelCredentials()
        )
    );
    
    int response  = client.sendRequest(4, pg_id, portMap.at(port_num));
   	return response;
}

int Send_Write_Access(int pg_id, std::string port_num, std::string page_val){
	std::string address = port_num;
	std::cout<<"Sending write access to <<"<<address<<"\n";
	PollClient client(
        grpc::CreateChannel(
            address, 
            grpc::InsecureChannelCredentials()
        )
    );
    
    int response  = client.sendWriteAccess(pg_id, page_val);
   	return response;
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
//    std::cout << "Received poll ACK" << response << std::endl;
   	return response;
};


void *Run_Server(void *ptr){
	std::string address("");
	char *port = (char *)ptr;
	address.append(port);
	PollService service;

	ServerBuilder builder;

	builder.AddListeningPort(address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);

	std::cout<<"Now creating directory server  >> .. \n";
	std::unique_ptr<Server> server(builder.BuildAndStart());
	std::cout << "Server listening on port: " << address << std::endl;
	server->Wait();
};
