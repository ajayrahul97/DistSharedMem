//Program 1 of the sequential consistency check
#include <stdlib.h>
#include <stdio.h>
#include "psu_dsm_system.h"

int main(int argc, char* argv[])
{
	myport =getHostName()+":8080";
	char *port = const_cast<char*>(myport.c_str());
	pthread_t thread_server;
	int iret1 = pthread_create(&thread_server, NULL, Run_Server, (void *)port);
	
	int* b = (int *)psu_dsm_malloc("bar", 4096);
	while(*b !=20);
	char* c = (char *)psu_dsm_malloc("foo", 4096);
	std::cout<<"c = "<<c<<std::endl;
	std::cout<<"Done with p3\n";
	pthread_join(thread_server, NULL);
	return 0;
}
