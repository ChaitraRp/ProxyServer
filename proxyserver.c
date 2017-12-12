//NETSYS PROGRAMMING ASSIGNMENT 2
//Chaitra Ramachandra

//-----------------------------------LIBRARIES--------------------------------------
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


//--------------------------------------#DEFINE------------------------------------
#define DEFAULT_CACHE_TIMEOUT 10
#define MAXCONN 20

//----------------------------------GLOBAL VARIABLES-------------------------------
long int cacheTimeout;
int proxySocket;
int debug = 1;


//----------------------------------STRUCT FOR URL---------------------------------
typedef struct URL {
    char *SERVICE, *DOMAIN, *PORT, *PATH;
} URL;


//--------------------------------STRUCT FOR HTTP REQUEST--------------------------
typedef struct http_request {
    char *HTTP_COMMAND, *COMPLETE_PATH, *HTTP_VERSION, *HTTP_BODY;
    URL* HTTP_REQ_URL;
} HTTP_REQUEST;



//-----------------------------------------MAIN-------------------------------------
//Ref: https://techoverflow.net/2013/04/05/how-to-use-mkdir-from-sysstat-h/
//Ref: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740496(v=vs.85).aspx
int main(int argc, char* argv[]){
	struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;
	int clientLength, clientSock, serverSock;

	bzero(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(atoi(argv[1]));
    serverAddress.sin_addr.s_addr = INADDR_ANY;
	
	mkdir(CACHEDIR, S_IRWXU);
	
	//Usage rules
	if(argc < 2){
        printf("Usage: ./proxyserver <PORT NUMBER> [<CACHE TIMEOUT VALUE>]\n");
		exit(1);
    }
	
	//Cache timeout settings
    if(argc < 3)
        cacheTimeout = DEFAULT_CACHE_TIMEOUT;
    else
        cacheTimeout = atoi(argv[2]);
	printf("Cache timeout value: %lu\n", cacheTimeout);

	//create socket, bind() and listen()
    proxySocket = socket(PF_INET, SOCK_STREAM, 0);
    if(bind(proxySocket, (struct sockaddr*)&serverAddress, (socklen_t)sizeof(serverAddress)) < 0){
        perror("Error in bind()");
		exit(1);
    }
    if(listen(proxySocket, MAXCONN) < 0){
        perror("Error in listen()");
		exit(1);
    }

    while(1){
		//start accepting connections through proxy
        clientSock = accept(proxySocket, (struct sockaddr*) &clientAddress,(socklen_t*) &clientLength);

		//fork() for multiiple connections
        if(fork() == 0){
            char* requestBuffer;
            int recvBytes;
			
			//receive
			recvBytes = receiveData(clientSock, &requestBuffer);
            if(recvBytes <= 0){
                perror("Error in recv()");
				close(clientSock);
				exit(0);
            }

			//create a HTTP Request struct
            HTTP_REQUEST httprequest;
            bzero(&httprequest, sizeof(httprequest));

            if(parseHTTPRequest(requestBuffer, recvBytes, &httprequest) < 0){
                perror("Error in parseHTTPRequest()");
                internalError(clientSock, "Unable to parse HTTP Request");
                close(clientSock);
                exit(0);
            }

			//create a response
            if(!otherRequestErrors(clientSock, httprequest)){
                char* otherResponse;
                int otherResponseLength;
                
                if(fetchResponse(&serverSock, &httprequest, &otherResponse, &otherResponseLength, clientSock) < 0){
                    cleanHTTPStructure(&httprequest);
                    close(clientSock);
                    exit(0);
                }

                //send response
                if(send(clientSock, otherResponse, otherResponseLength, 0) < 0)
                    perror("Error in send()");

                close(clientSock);

                //link prefetch (extra credit)
                debug = 1;
                prefetch(httprequest, otherResponse, otherResponseLength);
                exit(0);
            }
			
			//clean up
            else{
                cleanHTTPStructure(&httprequest);
                close(clientSock);
                exit(0);
            }
        }
        close(clientSock);
    }
    close(proxySocket);
}