#include "psu_dsm_system.h"
//#include "psu_lock.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <unordered_map>
#include <fstream>
#include <math.h>
#include <utility>
#include <iterator>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <bits/stdc++.h>
#include <iomanip>
#include <cmath>

using namespace std;

int process_num;
int total_processes_num;

typedef std::pair<float, float> point;

point cluster_pts[6][411] __attribute__ ((aligned (4096)));
int c __attribute__ ((aligned (4096)));
int d __attribute__ ((aligned (4096)));

void setup(std::string);
void *map_kmeans(void *k);
void *reduce_kmeans(void *k);

void psu_mr_map(void *(*mapper)(void *), void *inpdata, void *outdata);
void psu_mr_reduce(void *(*reducer)(void *), void *inpdata, void *outdata);

std::pair<float,float> operator+(const std::pair<float,float> & l,const std::pair<float,float> & r) {   
    return {l.first+r.first,l.second+r.second};                                    
} 


int main(int argc, char* argv[])
{
    if(argc < 4)
	{
		printf("Format is <sort> <Process_num> <total_num_Proceses> <filename>\n");
		return 0;
	}

	process_num = atoi(argv[1]);
	total_processes_num = atoi(argv[2]);
	std::string filename = argv[3];
	myport = getHostName()+":8080";
	char *port = const_cast<char*>(myport.c_str());

	int iret1=1;
	pthread_t thread_server;
	iret1 = pthread_create(&thread_server, NULL, Run_Server, (void *)port);

	//psu_init_lock(0);
	
	psu_dsm_register_datasegment(&cluster_pts, 401*6*sizeof(point)+4096);
	if (process_num == 0)
	{
		setup(filename);
	}
	
	barrier();
	psu_mr_map(&map_kmeans, NULL, NULL);
	psu_mr_reduce(&reduce_kmeans, NULL, NULL);
	std::cout<<"DONE WITH THE PROGRAM..\n";	
    pthread_join(thread_server, NULL);
    return 0;
}

double calculateDistance(std::pair<float, float> &x, std::pair<float, float> &y)
{
    return sqrt(pow(x.first - y.first, 2) +
                pow(x.second - y.second, 2));
}

pair <float, float> getpairs(string str)
{
    string word = "";
    list <string> data_pt;
    pair <float, float> p1 = {0.0, 0.0};
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
    p1.first = stof(*it);

    std::advance(it, 1);
    p1.second = stof(*it);

    return p1;
}

void setup(std::string file_name)
{
    ifstream myfile(file_name);
    if (myfile.is_open())
    {
        string firstline;
        getline (myfile,firstline);
        int num_points;
        num_points = stoi(firstline);
        string secondline;
        getline (myfile,secondline);
        int num_centroids;
        num_centroids = stoi(secondline);

        point num = {num_points, num_centroids};
        cluster_pts[1][0] = num;

        point a = {1, 0};
		
        string line;
        for(int i = 0; i < num_points; ++i)
        {
            getline(myfile,line);
            cluster_pts[0][i] = getpairs(line);
        }

        for(int i = 0; i < num_centroids; ++i)
        {
            getline(myfile,line);
            cluster_pts[1][i+1] = getpairs(line);
            cluster_pts[2+i][0] = a;
        }
    }
    myfile.close();

}

// histogram functions
void *map_kmeans(void *k)
{
	std::cout<<"MAPPER IS WORKING ---- \n";
	psu_lock(0);
	int l = process_num;
	while((int)cluster_pts[1][0].first==0);
	while((int)cluster_pts[1][0].second==0);

    int num_points = (int)cluster_pts[1][0].first;
	int num_centroids = 4;
	
    int a = l*num_points/total_processes_num;
	int b = (l+1)*num_points/total_processes_num;
	std::cout<<"num count -- "<<num_points<<" - "<<a<<" - "<<b<<"\n";

    for (int i=a; i<b; i++){
        point data_pt = cluster_pts[0][i];
        std::cout<<"Doing map for --"<<data_pt.first<<" -- "<<data_pt.second<<" point "<<i<<"\n";
        int idx = 0;
        float mindist = 10.00;

        for(int j = 0; j < num_centroids; j++){
            point ctrd = cluster_pts[1][j+1];
            std::cout<<"centroid --- "<<j<<" -- "<<ctrd.first<<" -- "<<ctrd.second<<"\n";
            float dist = calculateDistance(data_pt, ctrd);
            if (dist < mindist){
                idx = j;
                mindist = dist;
            }
        }
        std::cout<<"which cluster -- "<<idx<<"\n";
        while((int)cluster_pts[idx+2][0].first==0);
        int next_index_apnd =  (int)cluster_pts[idx+2][0].first;
        std::cout<<"index to be appended -- "<<next_index_apnd<<"\n";
        cluster_pts[idx+2][next_index_apnd] = data_pt;
        next_index_apnd++;
        cluster_pts[idx+2][0].first = next_index_apnd;
        std::cout<<"updated val of next idx"<<cluster_pts[idx+2][0].first;
    }
	psu_unlock(0);
	std::cout<<"MAPPER FINISHED ---- \n";
}

void *reduce_kmeans(void *k)
{
	int l = process_num;
	int a = l*4/total_processes_num;
	int b = (l+1)*4/total_processes_num;

	std::cout<<"Reducer is working now ---- from INDEX - "<<a<<" -- TO --"<<b<<"\n";
	psu_lock(0);
	std::cout<<"I have acquired the lock !!!!\n";
	std::ofstream ofile;
	ofile.open("kmeans_op.txt", ios::out | ios::app);
	for(int i= a; i< b; ++i)
	{	
        point new_centroid = {0.0, 0.0};
        int num_pts = (int)cluster_pts[i+2][0].first - 1;
        std::cout<<"Doing reduce for cluster -- "<<i<<" -- "<<num_pts<<"\n";
        for(int j =0; j< num_pts; j++){
            new_centroid = new_centroid + cluster_pts[i+2][j+1];
        }
        new_centroid = {new_centroid.first/num_pts, new_centroid.second/num_pts};
        ofile<<new_centroid.first<<" "<<new_centroid.second<<"\n";
	}
	
	ofile.close();
	std::cout<<"I have closed the file and going to release the lock\n";
	psu_unlock(0);
	std::cout<<"I am getting out of reducer..\n";
	
}


// map function
void psu_mr_map(void *(*mapper)(void *), void *inpdata, void *outdata)
{
	mapper(inpdata);
	std::cout<<"Now calling barrier 1\n";
	barrier2();
	std::cout<<"Returned from  barrier 1\n";

}

// reduce function
void psu_mr_reduce(void *(*reducer)(void *), void *inpdata, void *outdata)
{
	reducer(inpdata);
	std::cout<<"Now calling barrier 1\n";
	barrier3();
	std::cout<<"Returned from  barrier 1\n";

}

