// Client of our uftp protocol.
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.c"

#define MAXMSGLENGTH 1024 // Maximum size of datagram the client can send to the server

int main(int argc, char** argv){
    struct addrinfo hints, *res; // addrinfo structs for making sockets.
    struct sockaddr_in serveraddr; // address of requested server, only IPv4.
    socklen_t server_sockaddr_size; // size of client addr struct.
    struct hostent *server;
    struct timeval recv_timeout; // Max amount of time the server will take to receive a packet.
    char *hostname; 
    int server_sockfd; // Socket file descriptor
    ssize_t bytes_sent; // Number of bytes sent using sendto
    ssize_t bytes_recevied; // Number of bytes received from server.
    int buffer_bytes; // Number of bytes that are significant in the buffer.
    int portno; // Port number of requested server
    int isSending = 1; // var to control while loop
    long int filesize = 0;
    long int filepos = 0; // File position of IO file.
    char buffer[MAXMSGLENGTH]; // Buffer to hold message being sent.
    char filebuffer[MAXMSGLENGTH]; // Buffer to hold file contents.
    char servermsg[50]; //Messages from the server

    if(argc != 3){
        printf("Client must be provided a hostname and port number.\n");
        exit(1);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    
    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    if((getaddrinfo(argv[1], argv[2], &hints, &res)) == -1){
        perror("getaddrinfo");
        exit(1);
    }

    // make a socket:

    if((server_sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1){
        perror("socket");
        exit(1);
    }

    // Set timeout of receive to 5 seconds.
    recv_timeout = (struct timeval){ 0 };
    recv_timeout.tv_sec = 5;

    // Sets the socket receive timeout
    if (setsockopt(server_sockfd,SOL_SOCKET,SO_RCVTIMEO,&recv_timeout,sizeof(recv_timeout)) == -1) {
        perror("setsockopt recvtimeout");
        exit(1);
    }

    server_sockaddr_size = sizeof(serveraddr);
    
    while(isSending){
        printf("Please enter one of the following cmds:\n");
        printf("get [filename], put [filename], delete [filename], ls, exit\n");
        
        if(fgets(buffer, sizeof(buffer), stdin) != NULL){
            clientcmd user_cmd;
            char* token = strtok(buffer, " "); // First word before whitespace in string
            char* first_word = token; // store first word as token will be overwritten
            char* filename = malloc(10);
            if((token = strtok(NULL, " \n")) != NULL){
                // This will copy the filename of certain commands (get, put, delete) but not ls or exit.
                strcpy(filename, token);
            }
            user_cmd = resolveClientCommand(first_word);
            if(user_cmd != null && user_cmd != error){
                // User passed in valid input
                if((bytes_sent = sendto(server_sockfd, first_word, 
                    sizeof(first_word), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr))) != -1){
                    // send first word of command to server
                    switch(user_cmd){
                        case get:
                            // Send filename
                            bytes_sent = sendto(server_sockfd, filename, strlen(filename), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
                            // Receive number of bytes from server, compare with filepos
                            bytes_recevied = recvfrom(server_sockfd, &filesize, sizeof(filesize), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                            printf("%ld\n", filesize);
                            if(filesize < 0){
                                bytes_recevied = recvfrom(server_sockfd, servermsg, sizeof(servermsg), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                                printf("Server msg: %s\n", servermsg);
                                break;
                            }
                            for(;;){ // While EOF hasn't been hit
                                if (filepos >= filesize){
                                    break;
                                }
                                bytes_recevied = recvfrom(server_sockfd, filebuffer, sizeof(filebuffer), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                                buffer_bytes = writeFileContents(filename, filebuffer, filepos, bytes_recevied);
                                filepos += buffer_bytes;
                                if(filepos > 0){
                                    memset(filebuffer, '\0', sizeof(filebuffer));
                                    // Send an acknowledgement packet asking for more data
                                    bytes_sent = sendto(server_sockfd, "READY", sizeof("READY"), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
                                }
                            }
                            // Receive message after EOF indicating success or failure:
                            bytes_recevied = recvfrom(server_sockfd, servermsg, sizeof(servermsg), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                            printf("Server msg: %s", servermsg);
                            break;
                        case put:
                            // Reset vars first
                            bytes_sent = sendto(server_sockfd, filename, sizeof(filename), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
                            filesize = getFileSizeBytes(filename);
                            if(filesize < 0){
                                // Re-prompt if error arises, make sure to send EOF to server.
                                perror("getfilesizebytes");
                                bytes_sent = sendto(server_sockfd, &filesize, sizeof(filesize), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
                                bytes_recevied = recvfrom(server_sockfd, servermsg, sizeof(servermsg), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                                printf("Servermsg: %s\n", servermsg);
                                break;
                            }
                            bytes_sent = sendto(server_sockfd, &filesize, sizeof(filesize), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
                            
                            for(;;){
                                
                                bzero(filebuffer, MAXMSGLENGTH);
                                buffer_bytes = saveFileContents(filename, filebuffer, filepos, MAXMSGLENGTH);
                                filepos += buffer_bytes;
                                bytes_sent = sendto(server_sockfd, filebuffer, buffer_bytes, 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
                                if (filepos >= filesize){
                                    break;
                                }// Wait for acknowledgment packet saying the client is ready for more data
                                bytes_recevied = recvfrom(server_sockfd, servermsg, sizeof(servermsg), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                                if (strstr(servermsg, "READY") != NULL){
                                    memset(servermsg, '\0', sizeof(servermsg));
                                    continue;
                                }else{
                                    printf("Random packet received.\n");
                                }
                            }
                            // Receive packet from server after EOF to display server msg.
                            bytes_recevied = recvfrom(server_sockfd, servermsg, sizeof(servermsg), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                            printf("Server msg: %s\n", servermsg);
                            break;
                        case delete:
                            bytes_sent = sendto(server_sockfd, filename, strlen(filename), 0, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
                            bytes_recevied = recvfrom(server_sockfd, servermsg, sizeof(servermsg), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                            if(bytes_recevied > 0){
                                printf("Server output: %s\n", servermsg);
                                memset(servermsg, '\0', sizeof(servermsg));
                            }else{
                                perror("client delete");
                            }
                            break;
                        case ls:
                            // First receive filesize from server
                            bytes_recevied = recvfrom(server_sockfd, &filesize, sizeof(filesize), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                            //memset(buffer, '\0', sizeof(buffer));
                            printf("dir size: %ld\n", filesize);
                            printf("filepos: %ld\n", filepos);
                            while(filepos < filesize){
                                memset(buffer, '\0', sizeof(buffer));
                                bytes_recevied = recvfrom(server_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                                filepos += bytes_recevied;
                                printf("%s", buffer);
                            }
                            printf("\n");
                            break;
                        case Exit:
                            bytes_recevied = recvfrom(server_sockfd, &servermsg, sizeof(servermsg), 0, (struct sockaddr*)&serveraddr, &server_sockaddr_size);
                            printf("Server msg: %s\n", servermsg);
                            isSending = 0;
                            break;
                        case error:
                            printf("Invalid input.\n");
                            break;
                        default:
                            break;
                    }
                    // Reset vars.
                    filesize = 0;
                    filepos = 0;
                    memset(filebuffer, '\0', sizeof(filebuffer));

                }else{
                    perror("sendto");
                    exit(1);
                }
            }
            free(filename);
        }else{
            printf("Ctrl-D detected.\n");
            break;
        }
        memset(buffer, '\0', sizeof(buffer)); // Clear out buffer for next time.
        memset(servermsg, '\0', sizeof(servermsg));
    }

    close(server_sockfd);
    freeaddrinfo(res);
    return 0;
}