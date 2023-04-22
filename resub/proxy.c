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
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    if (argc < 2 || argc > 2){
        printf("./server <port number>\n");
        return 1;
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

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        bytes_recvd = recv(new_fd, request, sizeof(request) - 1, 0);
        if (bytes_recvd < 0) {
            perror("recv");
            return -1;
        }
        
        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            crequest = parse(request);
            sresponse = verify(crequest);
            if (sresponse == NULL){
                printf("Error with verify.\n");
                break;
            }
            // Persistant connections
            if (crequest->connection != NULL && strcmp(crequest->connection, "Keep-alive") == 0){
                int yes = 1;
                if (setsockopt(new_fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) == -1) {
                    perror("setsockopt");
                    exit(1);
                }
            }
            if (crequest->reqHost != NULL && strcmp(crequest->reqHost, "localhost") != 0 && strcmp(crequest->reqHost, "127.0.0.1") != 0){
                // Send request to specified host by opening another socket.

                // If error, do not forward.
                host = gethostbyname(crequest->reqHost);
                if(sresponse->responsecode == 400 || sresponse->responsecode == 403 || host == NULL){
                    bytes_written += formatresheaders(response, sresponse);
                    if(send(new_fd, response, bytes_written, 0) < 0){
                        perror("send");
                        break;
                    }
                    break;
                }
                memset(&chints, 0, sizeof hints);
                chints.ai_family = AF_UNSPEC;
                chints.ai_socktype = SOCK_STREAM;
                chints.ai_protocol = IPPROTO_TCP;

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

                printf("Forwarding request...\n");
                forward_and_respond(new_fd, req_fd, request);

                close(req_fd);

            }else{
                // Otherwise, the request was meant for us.
                printf("%s\n", crequest->reqURI);
                // Send headers first, then send requested file if OK
                bytes_written += formatresheaders(response, sresponse);
                if(send(new_fd, response, bytes_written, 0) < 0){
                    perror("send");
                    break;
                }
                else if (sresponse->responsecode != 200){
                    break;
                }

                //printresheaders(sresponse);
                // Get filesize first to compare the send results.
                filesize = getfilesize(sresponse->uri);
                bytes_sent = sendftosock(new_fd, sresponse->uri, 0);
                //printf("filesize: %d\nbytes_sent: %d\nuri: %s\n", filesize, bytes_sent, sresponse->uri);
                if (bytes_sent < filesize){
                    // Resend rest of file contents.
                    while(bytes_sent < filesize)
                        bytes_sent += sendftosock(new_fd, sresponse->uri, bytes_sent);
                }
            }

            free(crequest);
            free(sresponse);
            close(new_fd);
            if (response != NULL)
                memset(response, '\0', sizeof(*response));
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}