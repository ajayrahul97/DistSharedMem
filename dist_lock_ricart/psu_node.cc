/*
	Distributed Mutex Lock 
	Ricart Agarwala Implementation Test File
*/
#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <algorithm>
#include "psu_lock.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <map>
#include <utility>
#include <bits/stdc++.h>


using namespace std;


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


pair <string, string> getpairs(string str)
{
    string word = "";
    list <string> data_pt;
    pair <string, string> p1;
    for (auto x : str)
    {
        if (x == ' ' )
        {
            data_pt.push_back(word);
            word = "";
        }
        else {
            word = word + x;
        }
    }
    data_pt.push_back(word);
    std::list<std::string>::iterator it = data_pt.begin();
    p1.first = *it;

    std::advance(it, 1);
    p1.second = *it;

    return p1;
}



int main(int argc, char* argv[]){
	//if (argc != 2){
	//	std::cout <<"Insufficient arguments \n";

    char chostname[100];
    gethostname(chostname,100);
    Host_name = chostname;


    int no_lines = 0;
    string lockno;
    ifstream myfile("node_list.txt");
    vector < pair<string, string>> inputs;
    if (myfile.is_open())
    {
        string line;
        while ( getline (myfile,line) )
        {
            pair<string, string> iter_map = getpairs(line);
            inputs.push_back(iter_map);
            if (iter_map.first.compare(Host_name) == 0)
            {
                lockno = iter_map.second;
            }
            no_lines = no_lines + 1; 
        }
        myfile.close();
    }

    int count = 0;
    for(int i = 0; i < no_lines; ++i)
    {
        string item_lock = inputs[i].second;
        if (item_lock.compare(lockno) == 0)
        {
            if (inputs[i].first.compare(Host_name) == 0) {
                myid = count;   
            }
            ip_addresses.push_back(inputs[i].first);
            count = count + 1;
        }
    }

    NUM_PORTS = count;    
    std::list<std::string>::iterator it = ip_addresses.begin();
    std::cout << *it << std::endl;
    std::advance(it, 1);
    std::cout << *it << std::endl;


    myport = getHostName()+":8080";

    int iret1;
	pthread_t thread_server;
	iret1 = pthread_create(&thread_server, NULL, Run_Server, &myport);

	check_nodes_ready();
	std::cout <<"All nodes are up and running \n";

	psu_init_lock(0);
    ///
    ////
    ///
    ///
    ///
    // while loop to check if it is running correctly
	psu_mutex_lock(0);
	//Critical Section :
	std::cout <<"Inside critical section\n";
	std::cout <<"Node ID : "<<myport<<" --  My Sequence number : "<<myseqnum<<"\n";
	usleep(3000000);
	psu_mutex_unlock(0);

	pthread_join(thread_server, NULL);
	return 0;
	
}

