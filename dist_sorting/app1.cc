#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <time.h>
#include <fstream>
#include "psu_dsm_system.h"
//#include "psu_lock.h"

using namespace std;
#define COUNT 4*4096
int global_array[COUNT] __attribute__ ((aligned (4096)));
int partition_num[1024] __attribute__ ((aligned (4096)));
int c __attribute__ ((aligned(4096)));

int logbase2(int n);
int partition_num_func(int id, int level, int num);
void partial_sort(int process_num, int total_processes_num);
void merge(int process_num, int total_processes_num);

void wait_partition(int a)
{
	std::cout<<"Partition num before --"<<partition_num[a]<<"\n";
	psu_lock(0);
	partition_num[a]++;
	psu_unlock(0);
	std::cout<<"Partition num --"<<partition_num[a]<<"\n";
	while(partition_num[a] < 2){
//		std::cout<<"Waiting for partition num ...\n";
	}
	std::cout<<"Partition num after --"<<partition_num[a]<<"\n";	

	return;
}

void initialize()
{
	time_t t;
	srand((unsigned) time(&t));
	for(int i = 0; i<COUNT; i++)
	{
		global_array[i] = rand()%500;
	}
	std::cout<<"Done initializing array ... \n";

}
int main(int argc, char* argv[])
{
	if(argc < 3)
	{
		printf("Format is <sort> <Process_num> <total_num_Proceses>\n");
		return 0;
	}


	int process_num;
	int total_processes_num;
	
	process_num = atoi(argv[1]);
	total_processes_num = atoi(argv[2]);
	myport = getHostName()+":8080";
	char *port = const_cast<char*>(myport.c_str());
	
	int iret1=1;
	pthread_t thread_server;
	iret1 = pthread_create(&thread_server, NULL, Run_Server, (void *)port);

	
	psu_dsm_register_datasegment(&global_array, COUNT*sizeof(int)+(1024*sizeof(int)));
//	psu_init_lock(0);
	
	if(process_num == 0)
	{
		initialize();
		std::cout<<"Going into lock -- \n";
		psu_lock(0);
	 	partition_num[0]++;
		psu_unlock(0);
		std::cout<<"Out of the lock --\n";
	}
	else
	{
		while(partition_num[0] < 1);
	}
	partial_sort(process_num, total_processes_num);

	//Do merging based on the process_num

	int p = process_num;
	int n = total_processes_num;
	//Do merging based on the lock for 2 processes
	int i = 0;
	while((1<<i) < n)
	{
		if(p % (1<<i) == 0)
		{
			merge(p/(1<<i),n/(1<<i));
			int b_id = partition_num_func(p,i,n);
			wait_partition(b_id);
		}
		++i;
	}

	if(process_num == 0)
	{
		merge(0,1);
		
		std::cout<<"Now printing array --- \n";
		std::ofstream myfile;
		myfile.open("file.txt");
		for(int i = 0; i<COUNT; i++){
			//std::cout<<global_array[i]<<" ";
			myfile<<global_array[i]<<" ";
		}
		myfile.close();
			//CAT it to a file
		std::cout<<"Done printing array .. \n";
	}

	pthread_join(thread_server, NULL);
	return 0;
}

void partial_sort(int process_num, int total_processes_num)
{
	//choose the offset based on the process_num and total_processes_num
	int offset = process_num * (COUNT/total_processes_num);
	int size = COUNT/total_processes_num;
	int temp;

	std::ofstream myfile;
	if (process_num){
			myfile.open("file_0.txt");

	} else {
			myfile.open("file_1.txt");

	}
		
	std::cout<<"Printing partial array --- before sort -- \n";
	for (int k=offset; k<size+offset-1; k++){
		std::cout<<global_array[k]<<" ";
		myfile<<global_array[k]<<" ";
		
	}
	myfile.close();

	std::cout<<"Done printing partial array -- before sort --\n";

	for (int i = 0; i < size -1 ; i++)
	{	
		for(int j = 0 ; j < size-i-1; j++)
		{
			if(global_array[j+offset] > global_array[j+1+offset])
			{
				temp = global_array[j+offset];
				global_array[j+offset] = global_array[j+1+offset];
				global_array[j+1+offset] = temp;
			}	
		}
	}

	std::cout<<"Printing partial array --- \n";
	for (int k=offset; k<size+offset-1; k++){
		std::cout<<global_array[k]<<" ";
		
	}
	std::cout<<"Done printing partial array -- \n";
	return;
}

void merge(int process_num, int total_processes_num)
{
	int offset = process_num * (COUNT/total_processes_num);
	int size = COUNT/total_processes_num/2;

	int* a = new int[size*2];
	int i = 0;
	int j = 0;
	int k = 0;
	while(i < size && j < size)
	{
		if(global_array[i+offset] < global_array[j+offset+size])
		{
			a[k++] = global_array[i+offset];
			i++;
		}
		else
		{
			a[k++] = global_array[j+offset+size];
			j++;	
		}
	}

	if(j == size)
	{
		while(i<size)
		{
			a[k++] = global_array[i+offset];
			i++;
		}
		
	}
	else
	{	while(j<size)
		{
			a[k++] = global_array[j+offset+size];
			j++;
		}
	}

	for(int i=0; i<size*2; ++i)
		global_array[i+offset] = a[i];

	delete [] a;

}


int logbase2(int n)
{
	int i = 0;
	while(n>1)
	{
		i++;
		n=n/2;
	}
	return i;
}

int partition_num_func(int id, int level, int num)
{
	int k = logbase2(num)-level-1;
	int s_k = 1 << k;
	int idx = id/(1 << (level+1));
	return s_k + idx;
}
