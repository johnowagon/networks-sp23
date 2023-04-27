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
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    //struct Request* crequest; // Client request
    socklen_t sin_size;
    struct sigaction sa;
    char* request = malloc(REQUEST_BUFFER_SIZE);
    char* response = malloc(1024);
    if (response != NULL)
        memset(response, '\0', sizeof(*response));
    int bytes_recvd;
    int ttl;
    int yes=1;
    int rv;

    if (argc < 2 || argc > 3){
        printf("./server <port number> <ttl>\n");
        return 1;
    }

    // Time to live, 10 as default
    if (argc == 3){
        ttl = atoi(argv[2]);
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

        if (!fork()) { // this is the child process
            printf("in child\n");
            close(sockfd); // child doesn't need the listener
            bzero(request, REQUEST_BUFFER_SIZE);
            do{
                bytes_recvd += recv(new_fd, request+bytes_recvd, sizeof(request), 0);
                if (bytes_recvd < 0){
                    perror("first recv");
                }else if(bytes_recvd == 0){
                    break;
                }
            }while(doneReceiving(request, REQUEST_BUFFER_SIZE) == 0);

            remove_header(request, "If-Modified-Since: ");
            //printf("%s", request);
            handle_req(request, new_fd);
            
            //close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
        //free(request);
    }

    return 0;
}