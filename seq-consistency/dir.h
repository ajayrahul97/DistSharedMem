#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dir_grpc.h"


void check_nodes_ready(){
	for (int i=0; i < D_NUM_PORTS; i++){
		int resp = -1;
		while(resp == -1){
			//std::cout<<"Trying to do ping-pong\n";
			resp = Run_PollClient(0, ports_list[i], portMap.at(myport));
		}
	}
	std::cout<<"Done check ready >>> \n";
}
