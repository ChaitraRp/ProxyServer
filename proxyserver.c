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

//all about cache
#define DEFAULT_CACHE_TIMEOUT 10
#define CACHEDIR ".cache"

//other values
#define MAXCONN 25

//global variable
int cacheTimeout;
int sock;

typedef struct URL{
	char *SERVICE;
	char *DOMAIN;
	char *PORT;
	char *PATH;
} url;

typedef struct HTTP_REQUEST{
	char *COMMAND;
	char *COMPLETE_PATH;
	char *VERSION;
	char *BODY;
	url* REQUEST_URL;
} HTTP_REQUEST;

//-----------------------------------------MAIN--------------------------------------
//Ref: https://techoverflow.net/2013/04/05/how-to-use-mkdir-from-sysstat-h/
int main(int argc, char *argv[]){
	struct sockaddr_in serverAddress;
	struct sockaddr_in clientAddress;
	int clientLength, clientSock, serverSock;
	
	bzero(&serverAddress, sizeof(serverAddress));
	bzero(&clientAddress, sizeof(clientAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(argv[1]));
    serverAddress.sin_addr.s_addr = INADDR_ANY;
	
	mkdir(CACHEDIR, S_IRWXU);
	
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
	
	//create socket, bind() and listen()
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(bind(socket, (struct sockaddr*)&sock, (socklen_t)sizeof(serverAddress)) < 0){
		perror("Error in bind()\n");
		exit(1);
	}
	if(listen(sock, MAXCONN) < 0){
		perror("Error in listen()\n");
		exit(1);
	}
	
	while(1){
		//accept()
		clientSock = accept(sock, (struct sockaddr*)&clientAddress, (socklen_t*)&clientLength);
		
		if(fork() == 0){
			char *requestBuffer;
			int recvBytes;
			
			//receive
			recvBytes = receiveData(clientSock, &requestBuffer);
			if(recvBytes  <= 0){
				perror("Error in recv()\n");
				close(clientSock);
				exit(1);
			}
			
			HTTP_REQUEST httpRequest;
			bzero(&httpRequest, sizeof(httpRequest));
			
			
		}
	}
	
}















