#include <string>
#include <iostream>
#include <fstream>
#include <iterator>
#include <iomanip>
#include <string>
#include <algorithm>
#include <grpcpp/grpcpp.h>
#include "psulock.grpc.pb.h"
#include <pthread.h>
#include <stdio.h>
using std::to_string;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using psulock::PsuLock;
using psulock::LockMessage;
using psulock::LockReply;

using namespace std;

int highest_seqnum = 0;
int myseqnum = 0;
string myport;
int myid;
string Host_name;
list <string> ip_addresses;
//char *ip_addresses[2];
//int ports_list[3] = {5000,5001,5002};
int NUM_PORTS;
int deferred_req[3] = {0, 0 ,0};
bool requesting_cs;
int outstanding_reply;


   
class LockClient {
    public:
        LockClient(std::shared_ptr<Channel> channel) : stub_(PsuLock::NewStub(channel)) {}

    int sendRequest(int message_type, int seq_num, int node_id) {
       	LockMessage msg;

		msg.set_myseqno(seq_num);
        msg.set_message_type(message_type);
        msg.set_nodeid(node_id);

        LockReply reply;
        ClientContext context;

        Status status = stub_->sendRequest(&context, msg, &reply);

        if(status.ok()){
            return reply.result();
        } else {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return -1;
        }
    }

    private:
        std::unique_ptr<PsuLock::Stub> stub_;
};

class PsuLockService final : public PsuLock::Service {
    Status sendRequest(
	ServerContext* context,
	const LockMessage* msg,
	LockReply* reply
	) override {

		reply->set_result(1);
		
		int msg_type = msg->message_type();
		int recv_seqnum = msg->myseqno();
		int node_id = msg->nodeid();
		
		if (msg_type == 1){
			// REQUEST
			std::cout <<"HIGHEST SEQ NUM BEFORE :  "<<highest_seqnum<<"\n";
			highest_seqnum = std::max(highest_seqnum, recv_seqnum);
			std::cout <<"HIGHEST SEQ NUM AFTER : "<<highest_seqnum<<"\n";
			if (requesting_cs && ((recv_seqnum > myseqnum) ||
			 ( recv_seqnum == myseqnum) && (node_id > myid)))
			{
				//DEFER THE REPLY
				std::cout<<"Deferring the reply : "<<node_id<<"\n";
				int  j = 0;
				for (int i = 0; i < NUM_PORTS; i++){
                    std::list<std::string>::iterator it = ip_addresses.begin();
                    std::string ip_add = *it;
                    if (i==0) { }
                    else {
                        std::advance(it, i);
                        ip_add = *it;
                    }

					if (Host_name == ip_add){
						j = i;
						break;
					}
				}
				deferred_req[j] = 1;
				
			} else {
				// SEND REPLY
				std::cout<<"Sending back reply to - "<<node_id<<"\n";
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

int Run_Client(int message_type, int myseqnum, std::string send_ip, int id) {
    std::string address(send_ip);
    char ch = ':';
    address.push_back(ch); 
    address += to_string(8080);
    
    LockClient client(
        grpc::CreateChannel(
            address, 
            grpc::InsecureChannelCredentials()
        )
    );
    
    int response;
    response = client.sendRequest(message_type, myseqnum, id);
    //std::cout << "Response received " << response << std::endl;
   	return response;
};

void *Run_Server(void *ptr){
	std::string address(Host_name);
	char *port = (char *)ptr;

    char ch = ':';
    address.push_back(ch); 
    address += to_string(8080);
	PsuLockService service;

	ServerBuilder builder;

	builder.AddListeningPort(address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);


	std::unique_ptr<Server> server(builder.BuildAndStart());
	std::cout << "Server listening on port: " << address << std::endl;

	server->Wait();
};

// int main(int argc, char** argv){
	// 
	// /*
	// if (atoi(argv[1]) == 0){
		// Run_Server();
	// } else {
		// Run_Client();
	// }
	// return 0;
	// */
	// int ptr1 =1;
	// int ptr2 =2;
	// int iret1, iret2;
	// pthread_t thread1, thread2;
	// 
	// iret1 = pthread_create(&thread1, NULL, Run_Server, (void *) ptr1);
	// iret2 = pthread_create(&thread2, NULL, Run_Client, (void *) ptr2);
// 
	// pthread_join(thread1, NULL);
	// pthread_join(thread2, NULL);
// 
	// return 0;
// };

