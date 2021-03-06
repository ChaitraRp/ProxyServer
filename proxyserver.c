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
#define DEFAULT_CACHE_TIMEOUT 20
#define MAXCONN 20
#define MAXRECVBUFSIZE 10000
#define MAXREQBUFFERSIZE 1000
#define CACHEDIR ".cache"
#define DEFAULT_PORT "80"


//----------------------------------GLOBAL VARIABLES-------------------------------
long int cacheTimeout;
int proxySocket;
int debug = 1;
int http_debug = 0;
struct timeval timeout;
struct hostent *hp;
struct hostent *host;
char blockedSitesFilename[] = "blocked_sites.txt";
int count = 1;


//----------------------------------STRUCT FOR URL---------------------------------
typedef struct URL{
    char *SERVICE, *DOMAIN, *PORT, *PATH;
} URL;


//--------------------------------STRUCT FOR HTTP REQUEST--------------------------
typedef struct http_request{
    char *HTTP_COMMAND, *COMPLETE_PATH, *HTTP_VERSION, *HTTP_BODY;
    URL* HTTP_REQ_URL;
} HTTP_REQUEST;




//------------------------------RECEIVE DATA FUNCTION------------------------------
//Reference: https://stackoverflow.com/questions/28098563/errno-after-accept-in-linux-socket-programming
//Reference: https://stackoverflow.com/questions/8874021/close-socket-directly-after-send-unsafe
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





//------------------------------SEND ERROR MESSAGE----------------------------------
int sendErrorMessage(int clientsockfd, char* errorMsg, char* errorContent){
    int errorMessageLength;
	int errorContentLength;
	
	if(errorMsg != NULL)
		errorMessageLength = strlen(errorMsg);
	else
		errorMessageLength = 0;
	
	if(errorContent != NULL)
		errorContentLength = strlen(errorContent);
	else
		errorContentLength = 0;

    char responseErrorMessage[errorMessageLength + errorContentLength + 5];
    bzero(responseErrorMessage, sizeof(responseErrorMessage));
    snprintf(responseErrorMessage, sizeof(responseErrorMessage), "%s\r\n%s\r\n", errorMsg, errorContent != NULL ? errorContent : "");

    send(clientsockfd, responseErrorMessage, strlen(responseErrorMessage)+1, 0);
    return 0;
}




//---------------------------------BLOCKED WEBSITE----------------------------------
//Ref: http://www.geekinterview.com/question_details/85833
void blockedWebsites(int clientsockfd, URL* urlData2, struct hostent *h){
	char *address = urlData2->DOMAIN;
	FILE *fp;
	char line[200];
	
	fp = fopen(blockedSitesFilename, "r");
	if(!fp){
		perror("Could not open file");
		exit(0);
	}
	
	while(fgets(line, 200, fp) != NULL){
		//printf("Line: %s\n", line);
		if(strstr(line, address) || strstr(line, h->h_name)){
			printf("--------------------------------------------\n");
			printf("ERROR 403 Forbidden: Blocked URL\n");
			
			char *responseError = "Blocked URL";
            char errorContent[strlen("<html><body>ERROR 403 Forbidden: %s</body></html>") + strlen(responseError)+1];
            bzero(errorContent, sizeof(errorContent));
            snprintf(errorContent, sizeof(errorContent), "<html><body>ERROR 403 Forbidden: %s</body></html>", responseError);

            char errorMessageHead[strlen("HTTP/1.1 403 Forbidden\r\nContent-Length: %lu")+10+1];
            bzero(errorMessageHead, sizeof(errorMessageHead));
            snprintf(errorMessageHead, sizeof(errorMessageHead), "HTTP/1.1 403 Forbidden\r\nContent-Length: %lu", sizeof(errorContent));

            sendErrorMessage(clientsockfd, errorMessageHead, errorContent);
			//exit(1);
		}
	}
	fclose(fp);
}




//----------------------------PARSE URL STRUCT-------------------------------------
//Ref: https://paulschreiber.com/blog/2005/10/28/simple-gethostbyname-example/
int parseURL(int clientsockfd, char* path, URL* urlData){
    char *reqURL;
	char *reqDomain;
	char *urlChunks;
	char* portNum = NULL;
	char *temp = strdup(path);
	FILE *fp;
	
    urlChunks = strtok_r(temp, ":/", &reqURL);
    if(urlChunks != NULL)
        urlData->SERVICE = strdup(urlChunks);

    urlChunks = strtok_r(NULL, "/", &reqURL);
    if(urlChunks != NULL){
        char* urlDomain = strtok_r(urlChunks, ":", &reqDomain);
        if(urlDomain != NULL){
            urlData->DOMAIN = strdup(urlDomain);

            while((urlDomain = strtok_r(NULL, ":", &reqDomain)) != NULL)
                portNum = urlDomain;

            if(portNum != NULL)
                urlData->PORT = strdup(portNum);
        }
    }

    if(reqURL != NULL)
        urlData->PATH = strdup(reqURL);

	if(http_debug == 1){
		printf("****************************************************************\n");
		printf("URL SERVICE: %s\n", urlData->SERVICE);
		printf("URL DOMAIN: %s\n", urlData->DOMAIN);
		printf("URL PORT: %s\n", urlData->PORT);
		printf("URL PATH: %s\n", urlData->PATH);
		printf("****************************************************************\n");
	}
	
	if(clientsockfd != -5){
		if(urlData->DOMAIN != NULL){
			hp = gethostbyname(urlData->DOMAIN);
			//host = gethostbyaddr(urlData->DOMAIN, sizeof(urlData->DOMAIN), AF_INET);
			if(!hp){
				printf("SERVER NOT FOUND\n");
				exit(1);
			}
			else if(strcmp("443", hp->h_name) != 0){
				printf("%s = ", hp->h_name);
				unsigned int i=0;
				printf("%s\n", inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0])));
				
				fp = fopen("DNSCache.txt", "a");
				fseek(fp, 0, SEEK_END);
				fprintf(fp, "%s\t%s\n", urlData->DOMAIN, inet_ntoa(*(struct in_addr*)(hp->h_addr_list[0])));
				
				/*while(hp->h_addr_list[i] != NULL){
					printf( "%s ", inet_ntoa( *( struct in_addr*)( hp -> h_addr_list[i])));
					i++;
				}*/
			}
		}	
		blockedWebsites(clientsockfd, urlData, hp);
	}
    
	free(temp);
    return 0;
}




//----------------------------PARSE HTTP_REQUEST-----------------------------------
int parseHTTPRequest(int clientsockfd, char* reqBuf, int reqBufLength, HTTP_REQUEST* httpStruct){
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
        parseURL(clientsockfd, httpStruct->COMPLETE_PATH, httpStruct->HTTP_REQ_URL);
    }

    reqVal = strtok_r(NULL, " ", &temp2);
    if(reqVal == NULL){
        printf("HTTP version not found\n");
        return -1;
    }
    httpStruct->HTTP_VERSION = strdup(reqVal);

    
    if((reqBody = strstr(reqBuf, "\r\n\r\n")) != NULL && strlen(reqBody) > 4)
        httpStruct->HTTP_BODY = strdup(reqBody);

	if(http_debug == 1){
		printf("****************************************************************\n");
		printf("HTTP COMMAND: %s\n", httpStruct->HTTP_COMMAND);
		printf("HTTP PATH: %s\n", httpStruct->COMPLETE_PATH);
		printf("HTTP VERSION: %s\n", httpStruct->HTTP_VERSION);
		printf("****************************************************************\n");
	}
	
    return 0;
}




//--------------------------------INTERNAL ERROR------------------------------------
int internalError(int clientsockfd, char* errMsg){
    char errorMessage[strlen("500 Internal Server Error: %s") + strlen(errMsg) + 1];
	bzero(errorMessage, sizeof(errorMessage));
	
	if(clientsockfd == -1)
        return 1;
    snprintf(errorMessage, sizeof(errorMessage), "500 Internal Server Error: %s", errMsg);
    sendErrorMessage(clientsockfd, errorMessage, NULL);
    return 0;
}




//-------------------------------ERROR HANDLING------------------------------------
int otherRequestErrors(int clientsockfd, HTTP_REQUEST httpReq){
    if(strcmp(httpReq.HTTP_COMMAND, "GET") != 0){
        if(strcmp(httpReq.HTTP_COMMAND, "HEAD") == 0 || strcmp(httpReq.HTTP_COMMAND, "POST") == 0 || strcmp(httpReq.HTTP_COMMAND, "PUT") == 0 || strcmp(httpReq.HTTP_COMMAND, "DELETE") == 0 || strcmp(httpReq.HTTP_COMMAND, "TRACE") == 0 || strcmp(httpReq.HTTP_COMMAND, "CONNECT") == 0){
			
            char *responseError = "Unsupported method";
            char errorContent[strlen("<html><body>501 Not Implemented %s: %s</body></html>") + strlen(responseError) + strlen(httpReq.HTTP_COMMAND)+1];
            bzero(errorContent, sizeof(errorContent));
            snprintf(errorContent, sizeof(errorContent), "<html><body>501 Not Implemented %s: %s</body></html>", responseError, httpReq.HTTP_COMMAND);

            char errorMessageHead[strlen("HTTP/1.1 501 Not Implemented\r\nContent-Length: %lu")+10+1];
            bzero(errorMessageHead, sizeof(errorMessageHead));
            snprintf(errorMessageHead, sizeof(errorMessageHead), "HTTP/1.1 501 Not Implemented\r\nContent-Length: %lu", sizeof(errorContent));

            sendErrorMessage(clientsockfd, errorMessageHead, errorContent);
            return -1;
        }
        
		else{
            char errorContent[strlen("<html><body>400 Bad Request: %s</body></html>") + strlen(httpReq.HTTP_COMMAND)+1];
            bzero(errorContent, sizeof(errorContent));
            snprintf(errorContent, sizeof(errorContent), "<html><body>400 Bad Request: %s</body></html>", httpReq.HTTP_COMMAND);

            char errorMessageHead[strlen("HTTP/1.1 400 Bad Request\r\nContent-Length: %lu")+10+1];
            bzero(errorMessageHead, sizeof(errorMessageHead));
            snprintf(errorMessageHead, sizeof(errorMessageHead), "HTTP/1.1 400 Bad Request\r\nContent-Length: %lu", sizeof(errorContent));

            sendErrorMessage(clientsockfd, errorMessageHead, errorContent);
            return -1;
        }
    }
    
	else if(!(strcmp(httpReq.HTTP_VERSION, "HTTP/1.0") == 0 || strcmp(httpReq.HTTP_VERSION, "HTTP/1.1") == 0)){
        char errorContent[strlen("<html><body>400 Bad Request - Invalid HTTP-Version: %s</body></html>") + strlen(httpReq.HTTP_VERSION)+1];
        bzero(errorContent, sizeof(errorContent));
        snprintf(errorContent, sizeof(errorContent), "<html><body>400 Bad Request - Invalid HTTP-Version: %s</body></html>", httpReq.HTTP_VERSION);

        char errorMessageHead[strlen("HTTP/1.1 400 Bad Request\r\nContent-Length: %lu")+10+1];
        bzero(errorMessageHead, sizeof(errorMessageHead));
        snprintf(errorMessageHead, sizeof(errorMessageHead), "HTTP/1.1 400 Bad Request\r\nContent-Length: %lu", sizeof(errorContent));

        sendErrorMessage(clientsockfd, errorMessageHead, errorContent);
        return -1;
    }
	
    else if(strcmp(httpReq.COMPLETE_PATH, "") == 0){
        char errorContent[strlen("<html><body>400 Bad Request - Invalid URL: \"%s\"</body></html>") + strlen(httpReq.COMPLETE_PATH)+1];
        bzero(errorContent, sizeof(errorContent));
        snprintf(errorContent, sizeof(errorContent), "<html><body>400 Bad Request - Invalid URL: \"%s\"</body></html>", httpReq.COMPLETE_PATH);

        char errorMessageHead[strlen("HTTP/1.1 400 Bad Request\r\nContent-Length: %lu")+10+1];
        bzero(errorMessageHead, sizeof(errorMessageHead));
        snprintf(errorMessageHead, sizeof(errorMessageHead), "HTTP/1.1 400 Bad Request\r\nContent-Length: %lu", sizeof(errorContent));

        sendErrorMessage(clientsockfd, errorMessageHead, errorContent);
        return -1;
    }
    return 0;
}




//-------------------------------COMPUTE MD5---------------------------------------
void computeMD5(char* cPath, int cPathLen, char *cPageName){
    char MD5CmdBuffer[strlen("echo \"") + strlen("\" | md5sum") + cPathLen + 1];
    FILE * fp;

    bzero(MD5CmdBuffer, sizeof(MD5CmdBuffer));
    strncpy(MD5CmdBuffer, "echo \"", sizeof(MD5CmdBuffer)-strlen(MD5CmdBuffer)-1);
    strncat(MD5CmdBuffer, cPath, sizeof(MD5CmdBuffer)-strlen(MD5CmdBuffer)-1);
    strncat(MD5CmdBuffer, "\" | md5sum", sizeof(MD5CmdBuffer)-strlen(MD5CmdBuffer)-1);

    if(!(fp = popen(MD5CmdBuffer, "r"))){
        perror("MD5 command error");
        return;
    }

    if(fread(cPageName, 1, 32, fp) != 32){
        perror("MD5 command error");
        return;
    }
    pclose(fp);
}




//---------------------------------GET CACHE TIME----------------------------------
long int getTimeElapsedSinceCached(char* cacheFilename){
    char getFileDateCommand[strlen("expr $(date +%s) - $(date -r ") + strlen(cacheFilename) + strlen(" +%s)") + 1];
    char temp[50];
    FILE *fp;

    bzero(getFileDateCommand, sizeof(getFileDateCommand));

    strncpy(getFileDateCommand, "expr $(date +%s) - $(date -r ", sizeof(getFileDateCommand)-strlen(getFileDateCommand)-1);
    strncat(getFileDateCommand, cacheFilename, sizeof(getFileDateCommand)-strlen(getFileDateCommand)-1);
    strncat(getFileDateCommand, " +%s)", sizeof(getFileDateCommand)-strlen(getFileDateCommand)-1);

    if(!(fp = popen(getFileDateCommand, "r"))){
        perror("get cache time error");
        return -1;
    }

    if(fread(temp, 1, sizeof(temp), fp) == 0){
        perror("error in getting cache page time");
        return -1;
    }

    pclose(fp);
    return strtol(temp, NULL, 10);
}




//-----------------------------SERVE DATA FROM SERVER-------------------------------
//Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/ms737530(v=vs.85).aspx
//Reference: https://github.com/angrave/SystemProgramming/wiki/Networking,-Part-2:-Using-getaddrinfo
//Reference: https://stackoverflow.com/questions/40782933/why-are-there-multiple-results-from-getaddrinfo/40785761
int serveDataFromServer(int* serverSocketFd, HTTP_REQUEST* httpRequest){
	struct addrinfo *results;
	struct addrinfo hints;
	int value;
	FILE *fp;
	
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char *portNumber = httpRequest->HTTP_REQ_URL->PORT;
    if(portNumber == NULL)
        portNumber = DEFAULT_PORT;
	
    if((value = getaddrinfo(httpRequest->HTTP_REQ_URL->DOMAIN, portNumber, &hints, &results)) != 0){
        printf("Unable to fetch from %s:%s\n", httpRequest->HTTP_REQ_URL->DOMAIN, portNumber);
        printf("getaddrinfo() error: %s\n", gai_strerror(value));
        return -1;
    }

    if(results !=NULL && results->ai_addr != NULL){
        *serverSocketFd = socket(results->ai_addr->sa_family, results->ai_socktype, results->ai_protocol);

        if(connect(*serverSocketFd, results->ai_addr, results->ai_addrlen) != 0){
            perror("Error in connect()");
            return -1;
        }

        char httpRequestBuffer[MAXREQBUFFERSIZE];
        bzero(httpRequestBuffer, sizeof(httpRequestBuffer));
        
		int httpRequestBufferSize = sizeof(httpRequestBuffer);
		
		if(httpRequest->HTTP_BODY != NULL){
			snprintf(httpRequestBuffer, httpRequestBufferSize, "%s %s %s\r\nHost: %s\r\nContent-Length: %lu\r\n\r\n%s",
			httpRequest->HTTP_COMMAND,
			httpRequest->COMPLETE_PATH,
			httpRequest->HTTP_VERSION,
			httpRequest->HTTP_REQ_URL->DOMAIN,
			strlen(httpRequest->HTTP_BODY),
			httpRequest->HTTP_BODY);
		}
		else{
			snprintf(httpRequestBuffer, httpRequestBufferSize, "%s %s %s\r\nHost: %s\r\n\r\n",
			httpRequest->HTTP_COMMAND,
			httpRequest->COMPLETE_PATH,
			httpRequest->HTTP_VERSION,
			httpRequest->HTTP_REQ_URL->DOMAIN);
		}
		
        if(send(*serverSocketFd, httpRequestBuffer, sizeof(httpRequestBuffer), 0) < 0){
            perror("Error in send()");
            return -1;
        }
    }

    freeaddrinfo(results);
    return 0;
}




//----------------------------FETCH THE REQUESTED PAGE------------------------------
int fetchResponse(int* serversockfd, HTTP_REQUEST* req, char** responseBuf, int* responseBufLength, int clientsockfd){
    char cachedPage[33];
	char cacheExists = 0;
    char isCacheValid = 0;
    int cacheFileSize;
	FILE* cacheFp;
	
    bzero(cachedPage, sizeof(cachedPage));
    computeMD5(req->COMPLETE_PATH, strlen(req->COMPLETE_PATH)+1, cachedPage);

    char cachedPagePath[strlen(CACHEDIR) + strlen(cachedPage) + 2];
    bzero(cachedPagePath, (strlen(CACHEDIR) + strlen(cachedPage) + 2)*sizeof(char));
    snprintf(cachedPagePath, sizeof(cachedPagePath)*sizeof(char), "%s/%s", CACHEDIR, cachedPage);

	//check if cached page exists and find out the cached page time
    if(access(cachedPagePath, F_OK) == 0){
        cacheExists = 1;
        long int cachedPageTime = getTimeElapsedSinceCached(cachedPagePath);
        if(cachedPageTime < cacheTimeout)
            isCacheValid = 1;
    }
	
	//if cache exists and is valid then serve the request from the cache
    if(cacheExists && isCacheValid){
        if(debug)
            printf("Fetching from cached data: %s\n", req->COMPLETE_PATH);

        cacheFp = fopen(cachedPagePath, "rb");
        if(fseek(cacheFp, 0, SEEK_END) < 0){
            fclose(cacheFp);
            internalError(clientsockfd, "Invalid cache file size");
            return -1;
        }

        //check the cached file size
		cacheFileSize = ftell(cacheFp);
        if(cacheFileSize < 0){
            fclose(cacheFp);
            internalError(clientsockfd, "Cache file size not found");
            return -1;
        }
		
        rewind(cacheFp);
        *responseBuf = calloc(cacheFileSize, sizeof(char));

        if((*responseBufLength = fread(*responseBuf, sizeof(char), cacheFileSize, cacheFp)) != cacheFileSize){
            perror("Error in reading data from cached page");
            internalError(clientsockfd, "Error in reading data from cached page");
            fclose(cacheFp);
            return -1;
        }
        fclose(cacheFp);
    }
    
	//This is in case cached page does not exist
	else{
        if(debug)
            printf("Fetching from server: %s\n", req->COMPLETE_PATH);

        if(serveDataFromServer(serversockfd, req) < 0){
            printf("Server request error\n");
            internalError(clientsockfd, "Server request error");
            return -1;
        }

        if((*responseBufLength = receiveData(*serversockfd, responseBuf)) < 0){
            perror("Server response error");
            internalError(clientsockfd, "Server response error");
            return -1;
        }
        close(*serversockfd);

        //add this entry to cache directory
        cacheFp = fopen(cachedPagePath, "wb+");
        fwrite(*responseBuf, sizeof(char), *responseBufLength, cacheFp);
        fclose(cacheFp);
    }
    return 0;
}




//--------------------------------CLEAN URL STRUCTURE------------------------------
void clearURLStruct(URL* tempURL){
    if(tempURL->SERVICE != NULL)
        free(tempURL->SERVICE);

    if(tempURL->DOMAIN != NULL)
        free(tempURL->DOMAIN);

    if(tempURL->PORT != NULL)
        free(tempURL->PORT);

    if(tempURL->PATH != NULL)
        free(tempURL->PATH);
}




//------------------------------CLEAN HTTP STRUCTURE--------------------------------
void cleanHTTPStructure(HTTP_REQUEST* temp){
    if(temp->HTTP_COMMAND != NULL)
        free(temp->HTTP_COMMAND);

    if(temp->COMPLETE_PATH != NULL)
        free(temp->COMPLETE_PATH);

    if(temp->HTTP_VERSION != NULL)
        free(temp->HTTP_VERSION);

    if(temp->HTTP_BODY != NULL)
        free(temp->HTTP_BODY);

    if(temp->HTTP_REQ_URL != NULL){
        clearURLStruct(temp->HTTP_REQ_URL);
        free(temp->HTTP_REQ_URL);
    }
}




//--------------------------LINK PREFETCH HELPER FUNCTION--------------------------
int linkPrefetchData(HTTP_REQUEST mainLink, char* subLink, char** destination){
    int fullPathLength;
	char* portNum;
	
	if(subLink[0] == '/'){
		if(mainLink.HTTP_REQ_URL->PORT != NULL)
			portNum = mainLink.HTTP_REQ_URL->PORT;
		else
			portNum = "";
        
        fullPathLength = strlen(mainLink.HTTP_REQ_URL->SERVICE) + strlen(mainLink.HTTP_REQ_URL->DOMAIN) + strlen(portNum) + strlen(subLink) + 5;
		
        *destination = malloc((fullPathLength)*sizeof(char));

        if(strlen(portNum) != 0){
            snprintf(*destination, fullPathLength, "%s://%s:%s%s", mainLink.HTTP_REQ_URL->SERVICE, mainLink.HTTP_REQ_URL->DOMAIN, portNum, subLink);
        }
        else{
            snprintf(*destination, fullPathLength, "%s://%s%s", mainLink.HTTP_REQ_URL->SERVICE, mainLink.HTTP_REQ_URL->DOMAIN, subLink);
        }
    }
	
    else if(strstr(subLink, "://") != NULL)
        *destination = strdup(subLink);
	
    else{
        int fullPathLength = strlen(mainLink.COMPLETE_PATH) + strlen(subLink) + 2;
        *destination = malloc((fullPathLength)*sizeof(char));
        snprintf(*destination, fullPathLength, "%s/%s", mainLink.COMPLETE_PATH, subLink);
    }
    return 0;
}




//----------------------------------LINK PREFETCH-----------------------------------
int linkPrefetch(HTTP_REQUEST mainPage, char* responseBuff){
    char *mainResponse = strdup(responseBuff);
	char *link;
	char *link2;
	
    while((link = strstr(mainResponse, "href=\"")) != NULL){
        int linkLength = strlen(link);
        int beginPtr = 6; //length of href="
        
		for(; beginPtr <= linkLength && link[beginPtr] != '\"'; beginPtr++);

        link2 = strndup(link+6, beginPtr-6);

        HTTP_REQUEST httpLinkPrefetch;
        bzero(&httpLinkPrefetch, sizeof(httpLinkPrefetch));
        
		httpLinkPrefetch.HTTP_COMMAND = "GET";
        httpLinkPrefetch.HTTP_VERSION = "HTTP/1.1";

        linkPrefetchData(mainPage, link2, &httpLinkPrefetch.COMPLETE_PATH);

        httpLinkPrefetch.HTTP_REQ_URL = calloc(1, sizeof(URL));
        parseURL(-5, httpLinkPrefetch.COMPLETE_PATH, httpLinkPrefetch.HTTP_REQ_URL);

        int serversockfd, responseLen;
        char *responseBuff;
        if(fetchResponse(&serversockfd, &httpLinkPrefetch, &responseBuff, &responseLen, -1) < 0)
            continue;

		//free all buffers
        free(link2);
		if(httpLinkPrefetch.COMPLETE_PATH != NULL)
            free(httpLinkPrefetch.COMPLETE_PATH);
        clearURLStruct(httpLinkPrefetch.HTTP_REQ_URL);
        free(httpLinkPrefetch.HTTP_REQ_URL);

        if(beginPtr + 1 < linkLength)
            mainResponse = link + beginPtr + 1;
        else
            break;
    }//end of while
    return 0;
}




//-----------------------------------------MAIN-------------------------------------
//Reference: https://techoverflow.net/2013/04/05/how-to-use-mkdir-from-sysstat-h/
//Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740496(v=vs.85).aspx
//Reference: http://beej.us/guide/bgnet/
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

            if(parseHTTPRequest(clientSock, requestBuffer, recvBytes, &httprequest) < 0){
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
                linkPrefetch(httprequest, otherResponse);
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