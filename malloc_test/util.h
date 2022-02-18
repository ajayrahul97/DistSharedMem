#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "psu_grpc.h"


void check_nodes_ready(){
	for (int i=0; i < NUM_PORTS; i++){
		int resp = -1;
		while(resp == -1){
			//std::cout<<"Trying to do ping-pong\n";
			resp = Run_PollClient(0, ports_list[i], portMap.at(myport));
//			std::cout<<"HEY"<<resp<<"--"<<ports_list[i]<<"--"<<myport<<"\n";
		}
	}
	std::cout<<"Done check ready >>> \n";
}


std::string getHostName(){
	std::string host_name="";
	char buffer[100];
	int ret;
	if((ret = gethostname(buffer, sizeof(buffer))) == -1){
		perror("gethostname");
		exit(1);
	}
	for (int i=0; i < 25; i++){
		host_name = host_name + buffer[i];
	}
	std::cout<<" HOST NME --- "<<host_name<<"\n";
	return host_name;
}
