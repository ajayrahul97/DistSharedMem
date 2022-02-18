/*
 * CSE-511 Project 2 Fall 2021
 * Author : Ajay Rahul
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <mutex>
#include "psu_grpc.h"

std::mutex mtx;           // mutex for critical section

/*
	Function to initialize lock
*/

void psu_init_lock(unsigned int lockno){
	std::cout << "Initializing LOCK\n";

}

void psu_mutex_lock(int lockno){
	mtx.lock();
	requesting_cs = true;
	myseqnum = highest_seqnum + 1;
	std::cout<<"INSIDE MUTEX LOCK >  myseqnum : " <<myseqnum <<"\n";
	// Set outstanding reply as N-1
	// Send REQUEST to every other node
	// And wait for the reply
	mtx.unlock();
	outstanding_reply = L_NUM_PORTS - 1;
	
	for (int i = 0; i < L_NUM_PORTS; i++){
		if (ports_list[i] != myport){
			int resp = Run_Client(1, myseqnum, l_ports_list[i], myport);
			if (resp == 2){
				// This means REPLY was received
				// so decrease outstanding reply
				std::cout<<"Received reply immediately here from : "<<l_ports_list[i]<<"\n";
				outstanding_reply -= 1;
				std::cout<<"Now outstanding reply is : "<< outstanding_reply<<"\n";
			}
		}
	}

	while (outstanding_reply != 0){
		//std::cout <<"Waiting for all nodes to send reply ..\n";
	}
	
}

void psu_mutex_unlock(int lockno){
	requesting_cs = false;
	std::cout << "Unlocking mutex lock ----- \n";
	// For each node i, if you deferred a reply => send(REPLY, i)
	for (int i = 0; i< L_NUM_PORTS; i++){
		if (l_ports_list[i] != myport && deferred_req[i]){
			deferred_req[i] = 0;
			std::cout << "Sending defferred reply to : "<<l_ports_list[i]<<"\n";
			int resp = Run_Client(2, myseqnum, l_ports_list[i], myport);
		}
	}
}

/*
// This function does a ping-pong to the other nodes 
// to check they are all up
void check_nodes_ready(){

	for (int i = 0; i < NUM_PORTS; i++){
		int resp = -1;
		while(resp == -1){
			std::cout << "Trying to do ping-pong";
			resp = Run_Client(0, myseqnum, ports_list[i], myport);
		};
	};
}
*/

