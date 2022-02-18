//Program 1 of the sequential consistency check

#include <stdlib.h>
#include <stdio.h>
#include "psu_dsm_system.h"

int a __attribute__ ((aligned (4096)));
int b __attribute__ ((aligned (4096)));
int c __attribute__ ((aligned (4096)));

int main(int argc, char* argv[])
{
	myport = getHostName()+":8080";
	char *port = const_cast<char*>(myport.c_str());
	pthread_t thread_server;
	int iret1 = pthread_create(&thread_server, NULL, Run_Server, (void *)port);		
	psu_dsm_register_datasegment(&a, 4096*2);
	a = 1;
	std::cout<<"Initial value  >>>>"<<a<<"\n";
	while(a==1);
	std::cout<<"Final value of a --"<<a<<"\n";
	pthread_join(thread_server, NULL);
	return 0;
}
