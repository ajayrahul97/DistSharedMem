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
	int* p =  (int *)psu_dsm_malloc("hey", 4096);
	while((*p) != 50);
	std::cout<<"\nValue of p after --"<<*p <<std::endl;
	
	int* b = (int *)psu_dsm_malloc("bar", 4096);
	char* c = (char *)psu_dsm_malloc("foo", 4096);
	
	c[0] = 'h';
	c[1] = 'i';
	c[3] = 'y';
	c[4] = 'a';
	*b = 20;
	
	std::cout<<"Done program 2\n";
	pthread_join(thread_server, NULL);
	return 0;
}
