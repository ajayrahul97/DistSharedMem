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


using namespace std;

int process_num;
int total_processes_num;

struct point {
	float x;
    float y;
};

typedef std::pair<float, float> point;

point cluster_pts[6][21] __attribute__ ((aligned (4096)));
int c __attribute__ ((aligned (4096)));


void setup(std::string);
void mapper_func();
void reducer_func();

//void map_function(void *(*mapper)(void *), void *inpdata, void *outdata);
//void reduce_function(void *(*reducer)(void *), void *inpdata, void *outdata);




int main(int argc, char* argv[])
{
    if(argc < 3)
	{
		printf("Format is <sort> <Process_num> <total_num_Proceses>\n");
		return 0;
	}

	process_num = atoi(argv[1]);
	total_processes_num = atoi(argv[2]);
	std::string filename = "input1.txt";
	myport = getHostName()+":8080";
	char *port = const_cast<char*>(myport.c_str());

	int iret1=1;
	pthread_t thread_server;
	iret1 = pthread_create(&thread_server, NULL, Run_Server, (void *)port);

	//psu_init_lock(0);
	
	psu_dsm_register_datasegment(&cluster_pts, 21*6**sizeof(point));
	if (process_num == 0)
	{
		setup(filename);
	}
	
	barrier();
	mapper_func();
	std::cout<<"Now calling barrier -- \n";
	barrier2();
	std::cout<<"Returned from barrier -- \n";
	reducer_func();
	std::cout<<"DONE WITH THE PROGRAM..\n";	
	//map_function(&temp_hist, NULL, NULL);
	//reduce_function(&temp_cum_hist, NULL, NULL);

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

        point num = {num_points, num_centroids}
        cluster_pts[1][0] = num;

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
        }
    }
    myfile.close();

}

// histogram functions
void mapper_func()
{
	std::cout<<"MAPPER IS WORKING ---- \n";
	//psu_lock();
	int l = process_num;
    int num_points = (int)cluster_pts[1][0].first;

    int a = l*num_points/total_processes_num;
	int b = (l+1)*num_points/total_processes_num;

    for (int i=a; i<b; i++){
        point data_pt = cluster_pts[0][i];
        int idx = 0;
        float mindist = 10.00;

        for(int j = 0; j < num_centroids; j++){
            point ctrd = cluster_pts[1][j+1];
            float dist = calculateDistance(data_pt, ctrd);
            if (dist < mindist){
                idx = i;
                mindist = dist;
            }
        }
        int next_index_apnd =  (int)cluster_pts[idx+2][0].first;
        cluster_pts[idx+2][next_index_apnd] = data_pt;
        next_index_apnd++;
        cluster_pts[idx+2][0].first = next_index_apnd;
    }
	std::cout<<"MAPPER FINISHED ---- \n";
}

void reducer_func()
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
        int num_pts = (int)cluster_pts[i+2][0].first;
        for(int j =0; j< num_pts; j++){
            new_centroid = new_centroid + cluster_pts[i+2][j];
        }
        new_centroid = {new_centroid.first/num_pts, new_centroid.second/num_pts};
        ofile<<new_centroid.first<<" "<<new_centroid.second<<"\n";
	}
	
	ofile.close();
	std::cout<<"I have closed the file and going to release the lock\n";
	psu_unlock(0);
	std::cout<<"I am getting out of reducer..\n";
	
}

/*
// map function
void map_function(void *(*mapper)(void *), void *inpdata, void *outdata)
{
	pthread_t tid[NUM_THREADS];

	for(int j= 0; j< NUM_THREADS; ++j)
		pthread_create(&tid[j], NULL, *(mapper), (void *)j);

	for(int j= 0; j< NUM_THREADS; ++j)
		pthread_join(tid[j], NULL);

}

// reduce function
void reduce_function(void *(*reducer)(void *), void *inpdata, void *outdata)
{
	pthread_t tid[NUM_THREADS];
	//Iterative version -- 4 threads
	for(int j= 0; j< NUM_THREADS; ++j)
		pthread_create(&tid[j], NULL, *(reducer), (void *)j);

	for(int j= 0; j< NUM_THREADS; ++j)
		pthread_join(tid[j], NULL);

}


*/
