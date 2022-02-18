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

#define SIZE_OF_DB 35000

using namespace std;

int process_num;
int total_processes_num;

struct keyvalue {
	char word[16];
	int value;
};
typedef struct keyvalue keyvalue;
keyvalue kv[SIZE_OF_DB] __attribute__ ((aligned (4096)));
int c __attribute__ ((aligned (4096)));


void setup(std::string);
void *temp_hist(void *inp);
void *temp_cum_hist(void *inp);
void psu_mr_map(void *(*mapper)(void *), void *inpdata, void *outdata);
void psu_mr_reduce(void *(*reducer)(void *), void *inpdata, void *outdata);


void Tokenize(string line, vector<string> &tokens, string delimiters){
		string token = "";
        string str = " ";
        string scol = ";";
        string col = ":";
        string fs = ".";
        string com = ",";
        string bo = "(";
        string bc = ")";
  
        for(int i=0; i<line.size(); i++) 
                        if(find(delimiters.begin(), delimiters.end(), line[i])!=delimiters.end()){
                        if (token != ""){
                                tokens.push_back(token);
						}
                        token = "";
                }
                else{
                	str[0] = line[i];
                	if (str.compare(col) && str.compare(fs) && str.compare(com) && str.compare(bo) && str.compare(bc) && str.compare(scol)){
                		token += str;
                	}
                }
                
        if (token != ""){
                tokens.push_back(token);
	}
	
}

vector<string> inputParser(string inputFileName){
        ifstream inf(inputFileName);
        vector<string> returnVec;
        string line;
        while(getline(inf,line).good()){
                vector<string> tokens;
                Tokenize(line, tokens, " \t");

                for (int i=0; i<tokens.size(); i++){
                        returnVec.push_back(tokens[i]);
                }
        }
        return returnVec;
}


int main(int argc, char* argv[])
{
    if(argc < 4)
	{
		printf("Format is <sort> <Process_num> <total_num_Proceses> <input_filename>\n");
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
	
	psu_dsm_register_datasegment(&kv, SIZE_OF_DB*sizeof(keyvalue));
	if (process_num == 0)
	{
		setup(filename);
	}
	
	barrier();
	psu_mr_map(&temp_hist, NULL, NULL);
	psu_mr_reduce(&temp_cum_hist, NULL, NULL);
	std::cout<<"DONE WITH THE PROGRAM..\n";	

    pthread_join(thread_server, NULL);
    return 0;
}



void setup(std::string file_name)
{
	vector<string> v_s;
	v_s = inputParser(file_name);
	std::cout<<"Total word count --- "<<v_s.size(); 
	for(int i = 0; i < v_s.size(); ++i)
	{
		strcpy(kv[i].word, v_s[i].c_str());
		kv[i].value = 0;
	}
}


// histogram functions
void *temp_hist(void* inp)
{
	std::cout<<"MAPPER IS WORKING ---- \n";
	//psu_lock();
	int l = process_num;
	int a = l*SIZE_OF_DB/total_processes_num;
	int b = (l+1)*SIZE_OF_DB/total_processes_num;
	int i;
	std::cout<<"MAPPER WORKING ON INDEX - "<<a<<" - TO -"<<b<<" \n";
	for(i= a; i< b; ++i)
	{	
	//	cout<<" WORD at index"<<i<<" : "<<kv[i].word<<"\n";
		if(kv[i].word[0] != '\0'){
			kv[i].value = 1;
//			std::cout<<"mapper : "<<kv[i].word<<" : "<<kv[i].value<<"\n";
		}
	}
	//psu_unlock();
	std::cout<<"MAPPER FINISHED ---- \n";
}

void *temp_cum_hist(void* inp)
{
	int l = process_num;
	int a = l*SIZE_OF_DB/total_processes_num;
	int b = (l+1)*SIZE_OF_DB/total_processes_num;

	std::cout<<"Reducer is working now ---- from INDEX - "<<a<<" -- TO --"<<b<<"\n";

	psu_lock(0);
	std::cout<<"I have acquired the lock !!!!\n";
	std::ofstream ofile;
	ofile.open("op_file.txt", ios::out | ios::app);
	int sum = 0;
	for(int i= a; i< b; ++i)
	{	
	//	std::cout<<"in word num -- "<<i<<"\n";
		if(kv[i].word[0] != '\0'){
			char word[16];
			strncpy(word, kv[i].word,16);
			//std::cout<<"checking word - "<<word;
			for (int j =0; j < SIZE_OF_DB; j++){
				if((strcmp(word, kv[j].word) == 0) && (kv[j].value==1)){
					sum++;
					kv[j].value = 0;
				}
			}
			if(sum){
	//			cout<<kv[i].word<<" : "<<sum<<"\n";
				ofile<<kv[i].word<<" : "<<sum<<"\n";
			}
		}
		sum = 0;
	}
	
	ofile.close();
	std::cout<<"I have closed the file and going to release the lock\n";
	psu_unlock(0);
	std::cout<<"I am getting out of reducer..\n";
	
}


// map function
void psu_mr_map(void *(*mapper)(void*), void *inpdata, void *outdata)
{

	mapper(inpdata);
	std::cout<<"Now calling barrier -- \n";
	barrier2();
	std::cout<<"Returned from  barrier -- \n";
}

// reduce function
void psu_mr_reduce(void *(*reducer)(void*), void *inpdata, void *outdata)
{
	reducer(inpdata);
	std::cout<<"Now calling barrier -- \n";
	barrier3();
	std::cout<<"Returned from barrier -- \n";

}



