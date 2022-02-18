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
	printf("ucontext = %p\n", ucontext);
	printf("fault addr = %p\n", info->si_addr);
	if( ((ucontext_t *)ucontext)->uc_mcontext.gregs[REG_ERR] & 0x2){
		// write fault
		
		printf("WRITE FAULT DETECTED IN SEGV HANDLER >> \n");
		// If write fault detected
		// Tell the directory - you want to write in that particular page
		// and after ACK - change mprotect

		int pg_id;
		std::string page_name;
		//Check which page has fault
		int fault_addr = *((int*)(&info->si_addr));

		for (int i=0; i<my_pages.size(); i++){
			int s_addr =*((int*)(&my_pages[i].pg_addr));
			if(s_addr <= fault_addr && s_addr+PAGE_SIZE > fault_addr){
				pg_id = my_pages[i].page_id;
				page_name = my_pages[i].page_name;
				break;
			}
		}

		std::cout<<"Write Fault in page number <<"<<pg_id<<" of port id "<< portMap.at(myport)<<"\n";

		//Send write request to directory
		int resp = -1;
		while (resp == -1){
			std::cout<<"doing write page in psu_dsm_system ...."<<"\n";
			resp = Write_Page(pg_id, portMap.at(myport), page_name);
		}
		
	} else {
		// read fault
		printf("READ FAULT DETECTED IN SEGV HANDLER >>");
		int pg_id;
		std::string page_name;
		void* start_addr;

		//Check which page has fault
		int fault_addr = *((int*)(&info->si_addr));

		for (int i=0; i<my_pages.size(); i++){
			int s_addr =*((int*)(&my_pages[i].pg_addr));
			if(s_addr <= fault_addr && s_addr+PAGE_SIZE > fault_addr){
				start_addr = my_pages[i].pg_addr;
				pg_id = my_pages[i].page_id;
				page_name = my_pages[i].page_name;
				break;
			}
		}

		std::cout<<"Read Fault in page number <<"<<pg_id<<page_name<<"of port "<<portMap.at(myport)<<"\n";
		std::cout<<"Fault addr --"<<fault_addr<<"\n";
		std::cout<<"Start addr --"<<start_addr<<"\n";
		int sd = *((int*)(&start_addr));
		std::cout<<"start aasddd -- "<<sd<<"\n";

		std::cout<<"Now sending read page request ..... \n";
		std::string resp_page = "";
		bool copy_page = true;
		while (resp_page.size() != PAGE_SIZE){
			resp_page = Read_Page(pg_id, portMap.at(myport), page_name);
			if (resp_page.compare("NoChange") == 0){
				copy_page = false;
				break;
			}
		}

		if (copy_page){
			std::cout<<"\nNow copying page \n";

			//First make the page write-able
			mprotect(start_addr, PAGE_SIZE, PROT_READ | PROT_WRITE);

			const char* p_data = resp_page.c_str();
			memcpy(start_addr, p_data, PAGE_SIZE);
		}
		std::cout<<"Now mprotecting .....\n";

		mprotect(start_addr, PAGE_SIZE, PROT_READ);	
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
	if(!initialized){
		init_ports();
		psu_dsm_init();
		initialized = true;
	}
	

	// Store info locally
	printf("Storing page info locally\n");
	int num_pages = psu_ds_size / PAGE_SIZE;
	std::string seg_name = "";
	for (int i = 0; i < num_pages; i++){
		page_info_t page;
		page = {
			psu_ds_start + i*PAGE_SIZE,
			psu_ds_size,
			i,
			seg_name
		};
		my_pages.push_back(page);
		std::cout<<"addr pushed .>>"<<psu_ds_start+i*PAGE_SIZE;
	}
	
	pg_addr = psu_ds_start;
	std::cout<<"pg addr ..."<<pg_addr<<"\n";
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
			resp = Send_Register(i, portMap.at(myport), "");
		}
	}
	printf("Now getting out of register segment \n");
}

void barrier(){
	int resp = -1;
	while(resp == -1){
		resp = Iam_Ready(portMap.at(myport));
	}
}

void barrier2(){
	int resp = -1;
	while(resp == -1){
		resp = Barrier2(portMap.at(myport));
	}
}

void barrier3(){
	int resp = -1;
	while(resp == -1){
		resp = Barrier3(portMap.at(myport));
	}
}

void psu_lock(int lock_num){
	int resp = -1;
	while(resp == -1){
		resp = Lock(portMap.at(myport));
	}
}

void psu_unlock(int lock_num){
	int resp = -1;
	while(resp == -1){
		resp = Unlock(portMap.at(myport));
	}
}

void* psu_dsm_malloc(std::string name, size_t size){
	void* buffer;
	if(!name.size()){
		std::cout<<"Empty string not allowed \n";
		return buffer;
	}
	if(!initialized){
		init_ports();
		psu_dsm_init();
		initialized = true;
	}

	std::cout<<"Registering malloc "<<name<<size<<"\n";
	posix_memalign(&buffer, PAGE_SIZE, size);
	int num_pages = size/PAGE_SIZE;

	for (int i = 0; i < num_pages; i++){
		page_info_t page;
		page = {
			buffer + i*PAGE_SIZE,
			size,
			i,
			name,
		};
		my_pages.push_back(page);
		std::cout<<"addr pushed .>>"<<buffer+i*PAGE_SIZE;
	}

	std::cout<<"Making all the pages read-only initially >>\n";
	for (int i=0; i<num_pages; i++){
		mprotect(buffer + i*PAGE_SIZE, PAGE_SIZE, PROT_READ);
	}

	printf("Now registering %d pages in directory >>; \n", num_pages);
	//Register each page with the directory
	//Send the tag, and r/w status
	
	for (int i = 0; i < num_pages; i++){
		int resp = -1;
		while (resp == -1){
			resp = Send_Register(i, portMap.at(myport), name);
		}
	}
	printf("Now getting out of malloc \n");
	return buffer;

}

