// Boilerplate code was inspired by Beej's programming guide:
// https://beej.us/guide/bgnet/html/split/client-server-background.html#a-simple-stream-server
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
#include "helpers.c"

/* Handle partial header reads: maybe read bytes into REQUEST_BUF until \r\n\r\n is found, when that happens,
 * parse the buffer.
 * 
 * 
 *
*/

#ifdef _GNU_SOURCE
# define basename __basename_gnu
#else
# define basename __basename_nongnu
#endif

int main(int argc, char** argv){
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    struct Request* crequest; // Client request
    struct Response* sresponse; // Server response
    socklen_t sin_size;
    struct sigaction sa;
    char request[REQUEST_BUFFER_SIZE];
    char* response = malloc(RESPONSE_BUFFER_SIZE);
    memset(response, '\0', sizeof(response));
    int bytes_recvd;
    int bytes_written = 0; // To keep track of how many bytes are in the response buffer.
    int bytes_sent;        // To keep track of the bytes sent to the client for error checking.
    int filesize;          // File size
    int filepos;           // File position
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
            perror("setsockopt");
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
            parse2(request);
            exit(0);
            // do something with request
            sresponse = construct(crequest);
            printresheaders(sresponse);
            if(crequest != NULL)
                free(crequest);
            if(sresponse->responsecode != 200){
                // Error occured due to file, not found, forbidden, etc.
                bytes_written += formatresheaders(response, sresponse);
                send(new_fd, response, bytes_written, 0);
                exit(0);
            }else if(sresponse == NULL){
                printf("error occurred with construct().\n");
                break;
            }else{
                filesize = getfilesize(sresponse->objectname);
                bytes_written += formatresheaders(response, sresponse);
                bytes_sent = send(new_fd, response, bytes_written, 0);
                bytes_written = 0;
                //response += bytes_written; // Move pointer over headers
                while(filepos < sresponse->content_length){
                    memset(response, '\0', sizeof(response));
                    bytes_written = saveFileContents(sresponse->objectname, response, filepos, RESPONSE_BUFFER_SIZE);
                    filepos += bytes_written;
                    bytes_sent = send(new_fd, response, bytes_written, 0);
                    if (bytes_sent == -1){
                        perror("send");
                        break;
                    }else if (bytes_sent != bytes_written){
                        // Handle it?
                        printf("Error sending!\n");
                        printf("bytes sent: %d\n", bytes_sent);
                        printf("bytes written: %d\n", bytes_written); 
                    }
                    // We only send file contents after 1024 bytes, no headers.
                    bytes_written = 0;
                }
            }
            free(sresponse);
            close(new_fd);
            memset(response, '\0', sizeof(response));
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}