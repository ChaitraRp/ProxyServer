//NETSYS PROGRAMMING ASSIGNMENT 2
//Chaitra Ramachandra

//libraries
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>

//other values
#define DEFAULT_CACHE_TIMEOUT 10



//global variable
int cacheTimeout;

//-----------------------------------------MAIN---------------------------------------
int main(int argc, char *argv[]){
	struct sockaddr_in serverAddress;
	bzero(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(argv[1]));
    serverAddress.sin_addr.s_addr = INADDR_ANY;
	
	//Usage
	if(argc < 2){
		printf("Usage: ./proxyserver <PORT NUMBER> [<CACHE TIMEOUT VALUE>]\n");
		exit(1);
	}
	
	//Cache timeout settings
	if(argc < 3)
		cacheTimeout = DEFAULT_CACHE_TIMEOUT;
	else
		cacheTimeout = atoi(argv[2]);
	printf("Cache timeout value: %d\n", cacheTimeout);
}