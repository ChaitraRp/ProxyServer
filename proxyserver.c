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
#define MAX_RECV_BUF_SIZE 10000

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

struct timeval timeout;




//--------------------------------RECEIVE DATA FROM SOCKET--------------------------
int receiveData(int sockfd, char** data){
	int dataSize = 0;
	int receivedBytes = 0;
	int recvBytes = 0;
	
	bzero(&timeout, sizeof(timeout));
	timeout.tv_sec = 60;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
	
	*data = calloc(MAX_RECV_BUF_SIZE, sizeof(char));
	dataSize = MAX_RECV_BUF_SIZE;
	bzero(*data, dataSize);
	
	if((recvBytes = recv(sockfd, *data, dataSize-1, 0)) < 0){
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			perror("Timeout");
			return -1;
		}
	}
	
	receivedBytes = recvBytes;
	timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));
	
	char temp[MAX_RECV_BUF_SIZE];
	bzero(temp, sizeof(temp));
	
	while((recvBytes = recv(sockfd, temp, MAX_RECV_BUF_SIZE-1, 0)) > 0){
		*data = realloc(*data, (dataSize+recvBytes)*sizeof(char));
		dataSize += recvBytes;
		memcpy(*data+receivedBytes, temp, recvBytes+1);
		receivedBytes += recvBytes;
		bzero(temp, sizeof(temp));
	}
	
	if(recvBytes == -1 && errno != EAGAIN && errno != EWOULDBLOCK){
		perror("Timeout");
		return -1;
	}
	return receivedBytes;
}





//------------------------------------PARSE URL-------------------------------------
int parseURL(char* bufURL, url* URLSTRUCTURE){
	char *remoteReqUrl;
	char *remoteReqDomain;
	char *copyURL = strdup(bufURL);
	
	char *valURL = strtok_r(copyURL, ":/", &remoteReqUrl);
	if(valURL != NULL)
		URLSTRUCTURE->SERVICE = strdup(valURL);
	
	valURL = strtok_r(NULL, "/", &remoteReqUrl);
	if(valURL != NULL){
		char* domainVal = strtok_r(valURL, ":", &remoteReqDomain);
		if(domainVal != NULL){
			URLSTRUCTURE->DOMAIN = strdup(domainVal);
			char* portVal = NULL;
			while((domainVal = strtok_r(NULL, ":", &remoteReqDomain)) != NULL)
				portVal = domainVal;
			if(portVal != NULL)
				URLSTRUCTURE->PORT = strdup(portVal);
		}
	}
	
	if(remoteReqUrl != NULL)
		URLSTRUCTURE->PATH = strdup(remoteReqUrl);
	
	free(copyURL);
	return 0;
}





//----------------------------------PARSE HTTP REQUEST------------------------------
int parseHTTPRequest(char* reqBuf, int reqLegth, HTTP_REQUEST* REQUEST){
	if(reqBuf == NULL){
		printf("Empty buffer\n");
		return -1;
	}
	
	char temp[reqLegth];
	char *remoteReq;
	char *remoteReqLine;
	char* body;
	
	bzero(temp, sizeof(temp)*sizeof(char));
	memcpy(temp, reqBuf, reqLegth*sizeof(char));
	
	char *reqLine = strtok_r(temp, "\r\n", &remoteReq);
	char *reqVal = strtok_r(reqLine, " ", &remoteReqLine);
	if(reqVal == NULL){
		perror("Command not found");
		return -1;
	}
	
	REQUEST->COMMAND = strdup(reqVal);
	
	reqVal = strtok_r(NULL, " ", &remoteReqLine);
	if(reqVal != NULL){
		REQUEST->COMPLETE_PATH = strdup(reqVal);
		REQUEST->REQUEST_URL = calloc(1,  sizeof(url));
		parseURL(REQUEST->COMPLETE_PATH,  REQUEST->REQUEST_URL);
	}
	
	reqVal = strtok_r(NULL, " ", &remoteReqLine);
	if(reqVal == NULL){
		perror("Version error");
		return -1;
	}
	
	REQUEST->VERSION = strdup(reqVal);
	
	if((body = strstr(reqBuf, "\r\n\r\n")) != NULL && strlen(body) > 4)
		REQUEST->BODY = strdup(body);
	
	return 0;
}



//--------------------------------SEND ERROR MESSAGE------------------------------
int sendErrorMessage(int sockfd, char* error, char* content){
	int errorLength = 0;
	int contentLength = 0;
	if(error != NULL)
		errorLength = strlen(error);
	if(content != NULL)
		contentLength = strlen(content);
	
	char resultError[errorLength + contentLength + 5];
	bzero(resultError, sizeof(resultError));
	
	snprintf(resultError, sizeof(resultError), "%s\r\n%s\r\n", error, content);
	
	send(sockfd, resultError, strlen(resultError)+1, 0);
}





//------------------------------INTERNAL ERROR--------------------------------------
int internalError(int sockfd, char* errorMessage){
	if(sockfd == -1)
		return 1;
	int length = strlen("HTTP/1.0 500 Internal Server Error: %s") + strlen(errorMessage) + 1;
	char error[length];
	bzero(error, sizeof(error));
	snprintf(error, sizeof(error), "HTTP/1.0 500 Internal Server Error: %s", errorMessage);
	sendErrorMessage(sockfd, error, NULL);
	return 0;
}




//-----------------------------REQUEST ERROR----------------------------------------
int otherRequestErrors(int sockfd, HTTP_REQUEST request){
	//method check
	if(strcmp(request.COMMAND, "GET") != 0){
		if(strcmp(request.COMMAND, "HEAD") == 0 || strcmp(request.COMMAND, "POST") == 0
        || strcmp(request.COMMAND, "PUT") == 0 || strcmp(request.COMMAND, "DELETE") == 0 || strcmp(request.COMMAND, "TRACE") == 0 || strcmp(request.COMMAND, "CONNECT") == 0){
			char *response = "UNSUPPORTED METHOD";
			int l1 = strlen("<html><body>501 Not Implemented %s: %s</body></html>");
			int l2 = strlen(response);
			int l3 = strlen(request.COMMAND);
			char responseContent[l1 + l2 + l3 + 1];
			bzero(responseContent, sizeof(responseContent));
			snprintf(responseContent, sizeof(responseContent), "<html><body>501 Not Implemented %s: %s</body></html>", response, request.COMMAND);
			
			int l4 = strlen("HTTP/1.1 501 Not Implemented\r\nContent-Length: %lu");
			char errorHead[l4+10+1];
			bzero(errorHead, sizeof(errorHead));
			snprintf(errorHead, sizeof(errorHead), "HTTP/1.1 501 Not Implemented\r\nContent-Length: %lu", sizeof(responseContent));
			sendErrorMessage(sockfd, errorHead, responseContent);
			return -1;
		}
		else{
			int l1 = strlen("<html><body>400 Bad Request: Invalid Method: %s</body></html>");
			int l2 = strlen(request.COMMAND);
			char responseContent[l1+l2+1];
			bzero(responseContent, sizeof(responseContent));
			snprintf(responseContent, sizeof(responseContent), "<html><body>400 Bad Request: Invalid Method: %s</body></html>", request.COMMAND);
			
			int l3 = strlen("HTTP/1.1 400 Bad Request\r\nContent-Length: %lu");
			char errorHead[l3+10+1];
			bzero(errorHead, sizeof(errorHead));
			snprintf(errorHead, sizeof(errorHead), "HTTP/1.1 400 Bad Request\r\nContent-Length: %lu", sizeof(responseContent));
			sendErrorMessage(sockfd, errorHead, responseContent);
			return -1;
		}
	}
	
	//version check
	else if(!(strcmp(request.VERSION, "HTTP/1.0") == 0 || strcmp(request.VERSION, "HTTP/1.1") == 0)){
		int l1 = strlen("<html><body>400 Bad Request: Invalid HTTP-Version: %s</body></html>");
		int l2 = strlen(request.VERSION);
		char responseContent[l1+l2+1];
		bzero(responseContent, sizeof(responseContent));
		snprintf(responseContent, sizeof(responseContent), "<html><body>400 Bad Request: Invalid HTTP-Version: %s</body></html>", request.VERSION);
		
		int l3 = strlen("HTTP/1.1 400 Bad Request\r\nContent-Length: %lu");
		char errorHead[l3+10+1];
		bzero(errorHead, sizeof(errorHead));
		snprintf(errorHead, sizeof(errorHead), "HTTP/1.1 400 Bad Request\r\nContent-Length: %lu", sizeof(responseContent));
		sendErrorMessage(sockfd, errorHead, responseContent);
		return -1;
	}
	
	else if(strcmp(request.COMPLETE_PATH, "") == 0){
		int l1 = strlen("<html><body>400 Bad Request: Invalid URL: \"%s\"</body></html>");
		int l2 = strlen(request.COMPLETE_PATH);
		char responseContent[l1+l2+1];
		bzero(responseContent, sizeof(responseContent));
		snprintf(responseContent, sizeof(responseContent), "<html><body>400 Bad Request: Invalid URL: \"%s\"</body></html>", request.COMPLETE_PATH);
		
		int l3 = strlen("HTTP/1.1 400 Bad Request\r\nContent-Length: %lu");
		char errorHead[l3+10+1];
		bzero(errorHead, sizeof(errorHead));
		snprintf(errorHead, sizeof(errorHead), "HTTP/1.1 400 Bad Request\r\nContent-Length: %lu", sizeof(responseContent));
		sendErrorMessage(sockfd, errorHead, responseContent);
		sendErrorMessage(sockfd, errorHead, responseContent);
		return -1;
	}
	return 0;
}







//-----------------------------------------MAIN-------------------------------------
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
	if(bind(sock, (struct sockaddr*)&serverAddress, (socklen_t)sizeof(serverAddress)) < 0){
		perror("Error in bind()");
		exit(1);
	}
	if(listen(sock, MAXCONN) < 0){
		perror("Error in listen()");
		exit(1);
	}
	
	while(1){
		//accept()
		clientSock = accept(sock, (struct sockaddr*)&clientAddress, (socklen_t*)&clientLength);
		
		//fork()
		if(fork() == 0){
			char *requestBuffer;
			int recvBytes;
			
			//receive
			recvBytes = receiveData(clientSock, &requestBuffer);
			if(recvBytes  <= 0){
				perror("Error in recv()");
				close(clientSock);
				exit(0);
			}
			
			HTTP_REQUEST httpRequest;
			bzero(&httpRequest, sizeof(httpRequest));
			
			if(parseHTTPRequest(requestBuffer, recvBytes, &httpRequest) < 0){
				perror("Error in parseHTTPRequest()");
				internalError(clientSock, "Unable to parse HTTP Request");
				close(clientSock);
				exit(0);
			}
			
			//gather response
			if(!(otherRequestErrors(clientSock, httpRequest))){
				char *response;
				int responseLength;
				int responseType;
			}
			
			
		}//end of fork()
	}//end of while(1)
}














