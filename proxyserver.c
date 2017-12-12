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
#define MAXRECVBUFSIZE 10000
#define MAXREQBUFFERSIZE 1000

//----------------------------------GLOBAL VARIABLES-------------------------------
long int cacheTimeout;
int proxySocket;
int debug = 1;
struct timeval timeout;


//----------------------------------STRUCT FOR URL---------------------------------
typedef struct URL {
    char *SERVICE, *DOMAIN, *PORT, *PATH;
} URL;


//--------------------------------STRUCT FOR HTTP REQUEST--------------------------
typedef struct http_request {
    char *HTTP_COMMAND, *COMPLETE_PATH, *HTTP_VERSION, *HTTP_BODY;
    URL* HTTP_REQ_URL;
} HTTP_REQUEST;



//------------------------------RECEIVE DATA FUNCTION------------------------------
//Ref: https://stackoverflow.com/questions/28098563/errno-after-accept-in-linux-socket-programming
int receiveData(int clientsockfd, char** data){
	int dataLength = 0;
    int dataReceived = 0;
	int bytesReceived;
	char tempData[MAXRECVBUFSIZE];
    bzero(tempData, sizeof(tempData));
	
    //timeout part
    bzero(&timeout, sizeof(timeout));
    timeout.tv_sec = 60;
    setsockopt(clientsockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));

    *data = calloc(MAXRECVBUFSIZE, sizeof(char));
    dataLength = MAXRECVBUFSIZE;
    bzero(*data, dataLength);

	//recv()
    if((bytesReceived = recv(clientsockfd, *data, dataLength-1, 0)) < 0){
        if(errno == EAGAIN || errno == EWOULDBLOCK){
            perror("Error in recv(). Unable to receive data");
            return -1;
        }
    }
    dataReceived = bytesReceived;

	//timeout part
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    setsockopt(clientsockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));

    //collect all the data
    while((bytesReceived = recv(clientsockfd, tempData, MAXRECVBUFSIZE-1, 0)) > 0){
        *data = realloc(*data, (dataLength + bytesReceived)*sizeof(char));
        dataLength = dataLength + bytesReceived;
        memcpy(*data + dataReceived, tempData, bytesReceived+1);
        dataReceived += bytesReceived;
        bzero(tempData, sizeof(tempData));
    }

    if(bytesReceived == -1 && errno != EAGAIN && errno != EWOULDBLOCK){
        perror("Error in recv(). Unable to receive data. Timeout");
        return -1;
    }
    return dataReceived;
}



//----------------------------PARSE HTTP_REQUEST-----------------------------------
int parseHTTPRequest(char* reqBuf, int reqBufLength, HTTP_REQUEST* httpStruct)
{
    char tempBuf[reqBufLength];
	char *temp1;
	char *temp2;
	char* reqBody;
    bzero(tempBuf, sizeof(tempBuf)*sizeof(char));
	
	if(reqBuf == NULL){
        printf("Received empty request buffer\n");
        return -1;
    }

    memcpy(tempBuf, reqBuf, reqBufLength*sizeof(char));
    char* reqLine = strtok_r(tempBuf, "\r\n", &temp1);
    char* reqVal = strtok_r(reqLine, " ", &temp2);
    
	if(reqVal == NULL){
        printf("Command not found\n");
        return -1;
    }
    httpStruct->HTTP_COMMAND = strdup(reqVal);

    reqVal = strtok_r(NULL, " ", &temp2);
    if(reqVal != NULL){
        httpStruct->COMPLETE_PATH = strdup(reqVal);
        httpStruct->HTTP_REQ_URL = calloc(1, sizeof(URL));
        parse_url(httpStruct->COMPLETE_PATH, httpStruct->HTTP_REQ_URL);
    }

    reqVal = strtok_r(NULL, " ", &temp2);
    if(reqVal == NULL){
        printf("HTTP version not found\n");
        return -1;
    }
    httpStruct->HTTP_VERSION = strdup(reqVal);

    
    if((reqBody = strstr(reqBuf, "\r\n\r\n")) != NULL && strlen(reqBody) > 4)
        httpStruct->HTTP_BODY = strdup(reqBody);

    return 0;
}




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