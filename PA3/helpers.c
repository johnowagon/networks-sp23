#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include "helpers.h"
#include "cache.h"

void sigchld_handler(){
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int isDirectory(char *path) {
    // Inspired by https://stackoverflow.com/questions/4553012/checking-if-a-file-is-a-directory-or-just-a-file
    struct stat *statbuf = malloc(sizeof(struct stat));

    if (stat(path, statbuf) != 0){
        free(statbuf);
        return 0;
    }
    free(statbuf);
    return S_ISDIR(statbuf->st_mode);
}

int isIP(char* hostname){
    // Counts number of periods in hostname to determine if its an IP or not.
    // Inspired by https://stackoverflow.com/questions/4235519/counting-number-of-occurrences-of-a-char-in-a-string-in-c
    int i, count;
    for (i=0, count=0; hostname[i]; i++)
        count += (hostname[i] == '.');
    return count == 3;
}

char* getstatus(int responsecode){
    switch(responsecode){
        case 200:
            return "OK";
        case 405:
            return "Method Not Allowed";
        case 404:
            return "Not Found";
        case 403:
            return "Forbidden";
        case 400:
            return "Bad Request";
        case 505:
            return "HTTP Version Not Supported";
        default:
            return "Unknown";
    }
}

int formatresheaders(char* buf, struct Response* sresponse){
    // Populates buf with the http response headers described by sresponse.
    // Returns number of bytes written to the buffer.
    //int filepos = 0;
    int bytes_written = 0;
    
    // Write first header, check for error code.
    bytes_written += sprintf(buf + bytes_written, "%s %d %s\r\n", 
        sresponse->httpversion, sresponse->responsecode, getstatus(sresponse->responsecode));
    if( sresponse->responsecode != 200){
        bytes_written += sprintf(buf+bytes_written, "\r\n");
        return bytes_written;
    }

    bytes_written += sprintf(buf+bytes_written, "Content-type: %s\r\nContent-length: %ld\r\nConnection: %s\r\n\r\n", 
            sresponse->content_type, sresponse->content_length, sresponse->connection);
    return bytes_written;
}

int getfilesize(char* filename){
    // Returns filesize in bytes after error checks
    int size;
	FILE *f;
    char full_path[512];

    sprintf(full_path, "%s%s", "./www", filename);
    
	f = fopen(full_path, "r");
	if (f == NULL) return -1;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);

	return size;
}

char* getmimetype(char* filename){
    // Return the contenttype from the given filename
    // Code was inspired by https://stackoverflow.com/questions/5309471/getting-file-extension-in-c
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    dot += 1;

    // Move this into the parse function to check for 400s.

    if(strstr(dot, "html") != 0){
        return "text/html";
    }else if(strstr(dot, "txt") != 0){
        return "text/plain";
    }else if(strstr(dot, "png") != 0){
        return "image/png";
    }else if(strstr(dot, "gif") != 0){
        return "image/gif";
    }else if(strstr(dot, "jpg") != 0){
        return "image/jpg";
    }else if(strstr(dot, "ico") != 0){
        return "image/x-icon";
    }else if(strstr(dot, "css") != 0){
        return "text/css";
    }else if(strstr(dot, "js") != 0){
        return "application/javascript";
    }else{
        return "";
    }
}

void printresheaders(struct Response* res){
    printf("uri: %s\n", res->uri);
    printf("response; %d\n", res->responsecode);
    printf("version: %s\n", res->httpversion);
    printf("content-type: %s\n", getmimetype(res->uri));
    printf("content-length: %ld\n", res->content_length);
    printf("connection: %s\n", res->connection);
}

char* getHostname(char* IP){
    // Tries to resolve a hostname to an IP.
    struct addrinfo hints, *res;
    char* ip = malloc(NI_MAXHOST);
    int status;

    // Set up the hints structure
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Call getaddrinfo() to get a list of address structures
    if ((status = getaddrinfo(IP, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    // Loop through the address structures and print the hostname
    struct addrinfo *p;
    char hostname[NI_MAXHOST];

    for (p = res; p != NULL; p = p->ai_next) {
        if (getnameinfo(p->ai_addr, p->ai_addrlen, hostname, NI_MAXHOST, NULL, 0, 0) == 0) {
            ip = hostname;
            freeaddrinfo(res);
            return ip;
        }
    }

    // Clean up
    freeaddrinfo(res);
    return "";
}

char* getIp(char* hostname){
    struct hostent *host;
    host = gethostbyname(hostname);
    return inet_ntoa (*(struct in_addr*)host->h_addr);
}

int invalidhttp(char* httpversion){
    // Validates an httpversion, used for 400 response codes.
    return !(strcasecmp(httpversion, "HTTP/0.9") == 0 || strcasecmp(httpversion, "HTTP/1.0") == 0 ||
        strcasecmp(httpversion, "HTTP/1.1") == 0 ||strcasecmp(httpversion, "HTTP/2") == 0 || 
        strcasecmp(httpversion, "HTTP/3") == 0 ||strcasecmp(httpversion, "HTTP/4.2") == 0);
}

int doneReceiving(char* request, int size){
    // See if the server has found the end of the request headers.
    for(int i = 0; i < size; i++){
        if (i > 3){
            if (request[i-3] == '\r' && request[i-2] == '\n' && request[i-1] == '\r' && request[i] == '\n'){
                return 1;
            }
        }
    }
    return 0;
}

int blocked(char* hostname){
    // Scans the ./blocklist file to see if the requested host is blocked.
    // WARNING! this function leaks memory. Maybe try figuring it out
    char* curname; // Max hostname length of 100;
    char* ip;
    char* tmp;
    char curline[100];
    FILE *fp = fopen("./blocklist", "r");
    if (fp == NULL)
        return -1;

    if(isIP(hostname)){
        // If given a IP
        // inspiried by https://stackoverflow.com/questions/28566424/linux-networking-gethostbyaddr
        ip = hostname;
        curname = getHostname(ip);
        
    }else{
        // If given a URL
        curname = hostname;
        ip = getIp(curname);
    }

    tmp = curname;

    //printf("%s\n%s\n", ip, curname);
    for(int i = 0; i < 2; i++){
        while(fgets(curline, sizeof(curline), fp) != NULL){
            if (strstr(curline, tmp) != 0){
                printf("blocked! match %s : %s\n", tmp, curline);
                return 1;
            }
        }
        tmp = ip;
    }
    //free(curname); 
    //free(ip);
    fclose(fp);
    return 0;
}

struct Request* parse(char* requestbuf){
    // Input agnostic parsing function. Will not error out if inputs are wrong
    // Verify function will do the verification.
    size_t bytesToCarriage = 0; // Will be set to read the amount of bytes from thevcgiven request header.
    size_t bytesToSpace = 0;
    char* curline; // Holds a single header
    char* tmpbuf; // Pointer to curline, for pointer arithmetic.
    // Initialize return struct
    struct Request* crequest;
    crequest = calloc(sizeof(Request), sizeof(char));
    int i = 0;
    //int bytecounter = 0;
    //printf("%s\n", requestbuf);

    while((bytesToCarriage = strcspn(requestbuf, "\r\n")) != 0){
        i += 1;
        curline = malloc(bytesToCarriage+1);
        tmpbuf = curline;
        memcpy(curline, requestbuf, bytesToCarriage);
        curline[bytesToCarriage] = '\0';
        bytesToSpace = strcspn(tmpbuf, " ");
        if (bytesToSpace == 0){
            // Malformed
            return NULL;
        }
        if(i == 1){ // Only get these methods on the first iteration
            //crequest->method = calloc(bytesToSpace+1, sizeof(char));
            //memcpy(crequest->method, tmpbuf, bytesToSpace);
            // In GET request header
            tmpbuf += bytesToSpace + 1; // Move past GET 
            //crequest->method[bytesToSpace+1] = '\0';
            //memset(crequest->method+bytesToSpace+1, '\0', 1);
        
            // Next value is object URI
            bytesToSpace = strcspn(tmpbuf, " ");
            crequest->reqURI = calloc(bytesToSpace+1, sizeof(char));
            memcpy(crequest->reqURI, tmpbuf, bytesToSpace);
            crequest->reqURI[bytesToSpace+1] = '\0';
            tmpbuf += bytesToSpace + 1; // Move past URI
            
            // Next value is the http version.
            /*bytesToSpace = strcspn(tmpbuf, "\r\n");
            crequest->httpversion = calloc(bytesToSpace+1, sizeof(char));
            memcpy(crequest->httpversion, tmpbuf, 8);
            crequest->httpversion[bytesToSpace+1] = '\0';*/
        }else if(memcmp(tmpbuf, "Host:", bytesToSpace) == 0){
            tmpbuf += bytesToSpace + 1;
           
            if((bytesToSpace = strcspn(tmpbuf, ":")) != strlen(tmpbuf)){
                // If there is a provided port number
                crequest->reqHost = malloc(bytesToSpace + 1);
                memcpy(crequest->reqHost, tmpbuf, bytesToSpace);
                crequest->reqHost[bytesToSpace + 1] = '\0';

                // Move past hostname, get port number
                tmpbuf += bytesToSpace + 1;
                bytesToSpace = strcspn(tmpbuf, "\r\n");
                crequest->reqPortno = malloc(bytesToSpace + 1);
                memcpy(crequest->reqPortno, tmpbuf, bytesToSpace);
                crequest->reqPortno[bytesToSpace+1] = '\0';
            }else{
                // Extract hostname
                bytesToSpace = strcspn(tmpbuf, "\r\n");
                crequest->reqHost = calloc(bytesToSpace + 1, sizeof(char));
                memcpy(crequest->reqHost, tmpbuf, bytesToSpace);
                crequest->reqHost[bytesToSpace + 1] = '\0';

                crequest->reqPortno = malloc(3); // Default port is 80
                memcpy(crequest->reqPortno, "80\0", 3);
            }
            
        }else if(memcmp(tmpbuf, "Proxy-connection:", bytesToSpace) == 0 || memcmp(tmpbuf, "Connection:", bytesToSpace) == 0){
            // Supposedly these two headers work the same way.
            // Move past header
            tmpbuf += bytesToSpace + 1;

            bytesToSpace = strcspn(tmpbuf, "\r\n");
            crequest->connection = calloc(bytesToSpace+1, sizeof(char));
            memcpy(crequest->connection, tmpbuf, bytesToSpace);
        }
        // Move past examined contents and carriage
        requestbuf += bytesToCarriage + 2;
        free(curline);
    }
    return crequest;
}

int sendftosock(int sockfd, char* filename, int filepos){
    // Sends a files contents to a socket described by sockfd.
    int bytes_sent = 0;
    int bytes_read = 0;
    FILE *fp;
    char data[1024];
    char full_path[512];
    char* curfile;

    if (strstr(filename, "cache") != 0){
        fp = fopen(filename, "r");
        curfile = filename;
    }else{
        sprintf(full_path, "%s%s", "./www", filename);
        fp = fopen(full_path, "r");
        curfile = full_path;
    }

    if (fp != NULL){
        fseek(fp, filepos, SEEK_SET);
        while(!feof(fp)) {
            bytes_read = fread(data, sizeof(char), sizeof(data), fp);
            if (bytes_read < 0)
                perror("fread");
            if ((bytes_sent += send(sockfd, data, bytes_read, 0)) == -1) {
                perror("[-]Error in sending file.");
                exit(1);
            }
            bzero(data, 1024);
        }
    }else{
        printf("Problem opening file %s.\n", curfile);
    }
    
    fclose(fp);
    return bytes_sent;
}

int getreqsize(char* request){
    // Gets size in bytes of a request buffer.
    char* reqcopy = request;
    int bytesToCarriage = 0;
    int byteCount = 0;
    while((bytesToCarriage = strcspn(reqcopy, "\r\n")) != 0){
        byteCount += bytesToCarriage+2;
        reqcopy += bytesToCarriage+2;
    }
    return byteCount + 2;
}

int getcontentlength(char* response){
    // Extracts content length from server response.
    char* reqcopy = response;
    char* curline;
    char* tmpbuf;
    int bytesToSpace = 0;
    int bytesToCarriage = 0;

    while((bytesToCarriage = strcspn(reqcopy, "\r\n")) != 0){
        curline = malloc(bytesToCarriage);
        tmpbuf = curline;
        memcpy(tmpbuf, reqcopy, bytesToCarriage);
        bytesToSpace = strcspn(tmpbuf, " ");
        if(memcmp(tmpbuf, "Content-Length:", bytesToSpace) == 0){
            tmpbuf += bytesToSpace + 1;
            return atoi(tmpbuf);
        }
        reqcopy += bytesToCarriage + 2;
        free(curline);
    }
    return 0;
}

int forward_and_respond3(int from_sockfd, int to_sockfd, char* request, struct Request* req_){
    int bufsize = 65536; // Can lower this 
    char response[bufsize];
    char fname[100];
    int bytes_recvd, bytes_written, content_length, request_size;
    int bytes_sent = 0;
    int bytes_cached = 0;
    char full_length_url[256];
    char* full_path;
    int files = getfileamt(); // Amount of files in cache
    int i = 0;
    // Construct url+uri
    sprintf(full_length_url, "%s%s", req_->reqHost, req_->reqURI);
    unsigned long hashed = hash(full_length_url);
    sprintf(fname,".%s/%lu", DIR, hashed);
    
    // Attempt to ca_get
    //printf("full_length: %s\nfname: %s\n", full_length_url, fname);
    full_path = ca_get(full_length_url);
    if (full_path != NULL){
        printf("page is cached\n");
        printf("%s\n", full_path);
        bytes_sent = sendftosock(from_sockfd, full_path, 0);
        printf("bytes_sent: %d\n", bytes_sent);
    }else{
        printf("page is not cached\n");

        // Send initial request
        if(send(to_sockfd, request, getreqsize(request), 0) < 0){
            perror("fsend");
            return -1;
        }

        while(1){
            i++;
            // While still receiving files
            if ((bytes_recvd = recv(to_sockfd, response, sizeof(response), 0)) < 0){
                perror("recv");
                break;
            }

            printf("%s", response);
            if (i == 1){
                // On first iteration
                content_length = getcontentlength(response);
                request_size = getreqsize(response);
                printf("Cont: %d\nhsize: %d\n", content_length, request_size);
            }

            if((bytes_sent += send(from_sockfd, response, bytes_recvd, 0)) < 0){
                perror("fsend");
                return -1;
            }
            if (bytes_recvd == 0){
                break;
            }
            if (files < CACHE_SIZE)
                bytes_cached += ca_put(full_length_url, response, bytes_recvd, bytes_cached);
            
            printf("Recvd: %d bytes\n", bytes_recvd);
            printf("Sent %d bytes\n", bytes_sent);
            printf("Cached %d bytes\n", bytes_cached);
            printf("tol: %d\n", content_length + request_size);
            
            if (bytes_sent >= content_length + request_size)
                break;

            bzero(response, sizeof(response));
        }
    }
    return 0;
}


int handle_req(char* request, int from){
    struct addrinfo hints, chints, *servinfo, *p;
    struct Request* crequest; // Client request
    int req_fd, rv;
    int ttl = 10; // Time to live for cached pages

    printf("req: %s\nend\n", request);
    crequest = parse(request);

    if (crequest->reqHost == NULL || crequest->reqPortno == NULL){
        send404(from);
        return -1;
    }
    
    memset(&chints, 0, sizeof hints);
    chints.ai_family = AF_UNSPEC;
    chints.ai_socktype = SOCK_STREAM;
    chints.ai_protocol = IPPROTO_TCP;

    printf("info: %s %s %s\n", crequest->reqHost, crequest->reqPortno, crequest->reqURI);

    if ((rv = getaddrinfo(crequest->reqHost, crequest->reqPortno, &chints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((req_fd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(req_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(req_fd);
            perror("client: connect");
            continue;
        }
        
        break;
    }
    freeaddrinfo(servinfo);
    purgeexpired(ttl);
    printf("Forwarding request...\n");
    //printf("%s\n", request);
    if (blocked(crequest->reqHost)){
        send403(from);
        return 0;
    }
    forward_and_respond3(from, req_fd, request, crequest);
    printf("done.\n");
    return 0;
}

int send403(int sockfd){
    printf("sending 403\n");
    if(send(sockfd, "HTTP/1.1 403 Forbidden\r\n\r\n", sizeof("HTTP/1.1 403 Forbidden\r\n\r\n"), 0) < 0){
        perror("fsend");
        return -1;
    }
    return 0;
}

int send404(int sockfd){
    printf("sending 404\n");
    if(send(sockfd, "HTTP/1.1 404 Not Found\r\n\r\n", sizeof("HTTP/1.1 404 Not Found\r\n\r\n"), 0) < 0){
        perror("fsend");
        return -1;
    }
    return 0;
}

void remove_header(char* request, const char* header_name) {
    // This code was generated by ChatGPT. Neat!
    char* start = strstr(request, header_name); // Find the header in the request
    if (start != NULL) {
        // Move the start pointer to the end of the header name
        //start += strlen(header_name);

        // Find the end of the header value
        char* end = strstr(start, "\r\n");
        if (end != NULL) {
            // Move the end pointer to the beginning of the next line
            end += 2;

            // Calculate the length of the header line to remove
            int length = end - start;

            // Remove the header line from the request by shifting the remaining data to the left
            memmove(start, end, strlen(end) + 1);
        }
    }
}