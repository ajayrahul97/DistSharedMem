#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ucontext.h>
#include <pthread.h>

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <vector>
#include <thread>
#include <string>
#include <cstring>
#include <fstream>

#include "util.h"


void segv_handler (int signum, siginfo_t *info, void *ucontext)
{
	printf("Recieved signal: %d(SIGSEGV)\n", signum);
	printf("fault addr = %p\n", info->si_addr);
	if( ((ucontext_t *)ucontext)->uc_mcontext.gregs[REG_ERR] & 0x2){
		// write fault
		
		printf("WRITE FAULT DETECTED IN SEGV HANDLER >> \n");
		// If write fault detected
		// Tell the directory - you want to write in that particular page
		// and after ACK - change mprotect

		int pg_id;
		//Check which page has fault
		int addr =	*((int*)(&pg_addr));
		int fault_addr = *((int*)(&info->si_addr));
		pg_id =	(fault_addr - addr) / PAGE_SIZE;	
		std::cout<<"Write Fault in page number <<"<<pg_id<<" of port id "<< portMap.at(myport)<<"\n";

		//Send write request to directory
		int resp = -1;
		while (resp == -1){
			std::cout<<"doing write page in psu_dsm_system ...."<<"\n";
			resp = Write_Page(pg_id, portMap.at(myport));
		}
		
	} else {
		// read fault
		printf("READ FAULT DETECTED IN SEGV HANDLER >>");
		int pg_id;
		//Check which page has fault
		int addr =	*((int*)(&pg_addr));
		int fault_addr = *((int*)(&info->si_addr));
		pg_id =	(fault_addr - addr) / PAGE_SIZE;	
		if (pg_id<0){
			return;
		}
		std::cout<<"Read Fault in page number <<"<<pg_id<<"of port "<<portMap.at(myport)<<"\n";
		std::cout<<"Fault addr --"<<fault_addr<<"\n";
		
		std::cout<<"Now sending read page request ..... \n";
		std::string resp_page = "";
		while (resp_page.size() != PAGE_SIZE){
			resp_page = Read_Page(pg_id, portMap.at(myport));
		}	
		
		void* start_addr;
		for(int i=0; i<my_pages.size(); i++){
			if(my_pages[i].page_id == pg_id){
				start_addr = my_pages[i].pg_addr;
				break;
			}
		}

		std::cout<<"\nNow copying page \n";

		//First make the page write-able
		mprotect(pg_addr + pg_id*PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE);
		const char* p_data = resp_page.c_str();
		memcpy(start_addr, p_data, PAGE_SIZE);
		
		std::cout<<"Now mprotect RO.....\n";
		mprotect(pg_addr + pg_id*PAGE_SIZE, PAGE_SIZE, PROT_READ);
		
	}

	std::cout<<"Now exiting seg fault handler >>> \n";
}

void init_ports(){
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

	NUM_PORTS = num_lines;
	myport = getHostName()+":8080";
	std::cout<<"My host name -- "<<myport<<"\n";
	std::cout<<"opening file --\n";
	std::string file_name = myport.substr(0,13) +"_rpc_log_file.txt";
	op_file.open(file_name, std::ofstream::app | std::ofstream::out);

}

void psu_dsm_init(){
	// Initialize segv handler.
	
	struct sigaction sa;
 	memset (&sa, 0, sizeof (sa));
 	sa.sa_sigaction = &segv_handler;
	sa.sa_flags = SA_SIGINFO;
 	sigaction (SIGSEGV, &sa, NULL);
 	
}


void psu_dsm_register_datasegment(void* psu_ds_start, size_t psu_ds_size){
	printf("Registering Datasegment");
	// First make the page read only
	// Then tell the directory that this page is registered by me
	// Do poll to see if all ready
	// now initialize seg fault
	init_ports();
	psu_dsm_init();
	

	// Store info locally
	printf("Storing page info locally\n");
	int num_pages = psu_ds_size / PAGE_SIZE;
	for (int i = 0; i < num_pages; i++){
		page_info_t page;
		page = {
			psu_ds_start + i*PAGE_SIZE,
			psu_ds_size,
			i,
		};
		my_pages.push_back(page);
		std::cout<<"addr pushed .>>"<<psu_ds_start+i*PAGE_SIZE;
	}
	
	pg_addr = psu_ds_start;
	//Check if all nodes are ready
	printf("Checking if all nodes are ready\n");
	check_nodes_ready();
	
	printf("making all  pages READ only initially >>; \n");
	for (int i=0; i<num_pages; i++){
		mprotect(psu_ds_start + i*PAGE_SIZE, PAGE_SIZE, PROT_READ);
	}
			
	printf("Now registering %d pages in directory >>; \n", num_pages);

	//Register each page with the directory
	//Send the tag, and r/w status
	for (int i = 0; i < num_pages; i++){
		int resp = -1;
		while (resp == -1){
			resp = Send_Register(i, portMap.at(myport));
		}
	}
	printf("Now getting out of register segment \n");
}



void * psu_dsm_malloc( char* name, size_t size){
	
}

