#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "helpers.h"
#include "cache.h"

#define BACKLOG 10   // how many pending connections queue will hold
#define REQUEST_BUFFER_SIZE 65536
#define RESPONSE_BUFFER_SIZE 65536

int main(int argc, char** argv){
    int sockfd, new_fd, req_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, chints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    struct Request* crequest; // Client request
    struct Response* sresponse; // Server response
    struct hostent *host; // For host checking
    socklen_t sin_size;
    struct sigaction sa;
    char request[REQUEST_BUFFER_SIZE];
    char* response = malloc(1024);
    if (response != NULL)
        memset(response, '\0', sizeof(*response));
    int bytes_recvd;
    int bytes_written = 0; // To keep track of how many bytes are in the response buffer.
    int bytes_sent = 0;        // To keep track of the bytes sent to the client for error checking.
    int filesize;          // File size
    //int filepos;           // File position
    int ttl;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    if (argc < 2 || argc > 3){
        printf("./server <port number> <ttl>\n");
        return 1;
    }

    // Time to live, 10 as default
    if (argc == 3){
        ttl = atoi(argv[2]);
        printf("%d", ttl);
    }else{
        ttl = 10;
    }

    int portno = atoi(argv[1]);
    
    if(portno < 1024 || portno > 65536){
        printf("Port must be between 1024 and 65536.\n");
        return 1;
    }
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt_b");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        /*
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        */

        bzero(request, sizeof(request));
        while (!doneReceiving(request, REQUEST_BUFFER_SIZE) 
                && (bytes_recvd = recv(new_fd, request, sizeof(request), 0)) != 0){
            if (bytes_recvd < 0) {
                perror("recv");
                return -1;
            }
            crequest = parse(request);
            if (crequest == NULL)
                continue;
            printf("after parse\n");
            printf("%s\n", request);


            // Persistant connections
            if ((crequest->connection != NULL && strcmp(crequest->connection, "Keep-alive") == 0) 
                || strcmp(crequest->httpversion, "HTTP/1.1") == 0){
                int yes = 1;
                if (setsockopt(new_fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) == -1) {
                    perror("setsockopt");
                    break;
                }
            }

            response += bytes_recvd; // Move pointer along to get contents
        }
        /*if (!doneReceiving(request, REQUEST_BUFFER_SIZE)){
            sprintf(response, "%s%s", crequest->httpversion, "400 Bad Request\r\n\r\n");
            if((bytes_sent = send(new_fd, response, strlen(response), 0)) < 0){
                perror("400send");
                break;
            }
            continue;
        }*/
        //request[bytes_recvd] = '\0';
        
        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            printf("in child\n");
            remove_header(request, "If-Modified-Since: ");
            printf("%s", request);

            handle_req(request, new_fd);
            
            free(crequest);
            //free(sresponse);
            close(new_fd);
            bzero(request, sizeof(*request));
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}