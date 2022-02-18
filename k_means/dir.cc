#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "dir.h"

std::string getHostName(){
	std::string host_name="";
	char buffer[100];
	int ret;
	if((ret = gethostname(buffer, sizeof(buffer))) == -1){
		perror("gethostname");
		exit(1);
	}
//	std::cout<<buffer;
	for(int i=0;i<25;i++){
		if(buffer[i] !='\0'){
			host_name = host_name + buffer[i];
		}
	}
	return host_name;	
}

int main(int argc, char* argv[])
{

	std::ifstream inf("node_list.txt");
	std::string line;
	int num_lines = 0;
	
	while(getline(inf, line).good()){
		if(line.size()){
			line += ":8080";
			ports_list.push_back(line);	
			portMap.insert(std::pair<std::string, int>(line, num_lines));
			rPortMap.insert(std::pair<int, std::string>(num_lines, line));
			num_lines++;
		}
	}

	node_counter = num_lines -1;
	barrier_counter = num_lines - 1;
	barrier2_counter = num_lines - 1;
	barrier3_counter = num_lines - 1;

	D_NUM_PORTS = num_lines;
	myport = getHostName() + ":8080";
	std::cout<<"My host name -- "<<myport<<"\n";
	char* port = const_cast<char*>(myport.c_str());
	
/*
	char* port = "5003";
	myport = 5003;
	*/
	pthread_t thread_server;
	int iret = pthread_create(&thread_server, NULL, Run_Server, (void *)port);
	
	check_nodes_ready();
	std::cout<<"All nodes are up and running \n";
	pthread_join(thread_server, NULL);
	return 0;
}
