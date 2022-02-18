//Program 1 of the sequential consistency check

#include <stdlib.h>
#include <stdio.h>
#include "psu_dsm_system.h"


int main(int argc, char* argv[])
{
	myport = getHostName()+":8080";
	char *port = const_cast<char*>(myport.c_str());
	pthread_t thread_server;
	int iret1 = pthread_create(&thread_server, NULL, Run_Server, (void *)port);		
	int* a = (int *)psu_dsm_malloc("hey", 4096);
	*a = 50;
	
	std::cout<<"HEre is A >>>>"<<*a<<"\n";

	pthread_join(thread_server, NULL);
	return 0;
}
