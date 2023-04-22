// UDP server implementing an FTP.
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

#define BACKLOG 10 // How many pending connections are available to listen().
#define MAXMSGLENGTH 1024 // Max number of bytes to received per message from client.

int main(int argc, char** argv){
    // Code inspired by https://beej.us/guide/bgnet/html/#intro
    int status; // Status code of getaddrinfo
    struct sockaddr_in server_sockaddr;
    struct sockaddr_in client_sockaddr; // Client side socket address, sockaddr_storage so we can 
    // accept both IPv4 and IPv6 connections.
    FILE *curdir; // Will hold popen results.
    struct timeval recv_timeout; // Max amount of time the server will take to receive a packet.
    socklen_t client_sockaddr_size; // size of client addr struct.
    ssize_t bytes_received; // Amount of bytes received from client.
    ssize_t bytes_sent; // Amount of bytes sent to the client.
    char filename[100]; // Storage for the filename.
    char msgbuffer[MAXMSGLENGTH]; // Buffer to hold client message.
    char servermsg[MAXMSGLENGTH]; // Buffer to hold server messages to client
    char filebuffer[MAXMSGLENGTH]; // Buffer to hold file contents.
    int servermsglength; // To keep track of server message lengths.
    long int filepos = 0; // File position for specified file
    int written_bytes; // To keep track of how many bytes were written to the file.
    // Useful to not send any unnecessary data to the client.
    long int filesize; // Size of file to be sent. 
    clientcmd req_cmd; // Inputted command given by client to server.
    int server_sockid; // socket descriptor for server side socket.
    int client_sockid; // socket descriptor for client side socket.
    int isRunning = 1; // var to control running server.
    int yes; // var for setsockopt call
    int portno; // int representation of CLI argument
    
    if(argc != 2){
        perror("Server must have a port as an argument.");
        exit(1);
    }
    
    portno = atoi(argv[1]); // Port number server will be running on.
    if(portno < 1024 || portno > 65535){
        perror("Invalid port number.");
        exit(1);
    }

    bzero((char *) &server_sockaddr, sizeof(server_sockaddr));
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sockaddr.sin_port = htons((unsigned short)portno);

    // servinfo now points to a linked list of 1 or more struct addrinfos
    if((server_sockid = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        perror("error making udp socket");
        exit(1);
    }
    
    yes=1;

    // Allows the resuse of a previously closed socket
    if (setsockopt(server_sockid,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
        perror("setsockopt resuse");
        exit(1);
    }

    recv_timeout = (struct timeval){ 0 };
    recv_timeout.tv_sec = 5;

    // Sets the socket receive timeout
    if (setsockopt(server_sockid,SOL_SOCKET,SO_RCVTIMEO,&recv_timeout,sizeof(recv_timeout)) == -1) {
        perror("setsockopt recvtimeout");
        exit(1);
    }

    // Bind socket to given port number using the socket descriptor.
    if (bind(server_sockid, (struct sockaddr *) &server_sockaddr, sizeof(server_sockaddr)) < 0){
        perror("bind");
        exit(1);
    }

    client_sockaddr_size = sizeof(client_sockaddr);
    // now accept an incoming connection:
    while(isRunning){
        // Main loop of accepting connections.   
        
        printf("Waiting to receive a packet...");
        if((bytes_received = recvfrom(server_sockid, msgbuffer, sizeof(msgbuffer), 0, 
            (struct sockaddr *)&client_sockaddr, &client_sockaddr_size)) < 0){
            // Server timed out receiving
            // I got rid of the perror output because it was ugly.
            continue;
        }
        printf("Packet received!\n");
        printf("%ld bytes received\n", bytes_received);
        printf("packet: %s\n", msgbuffer);
        req_cmd = resolveClientCommand(msgbuffer);
        memset(msgbuffer, '\0', sizeof(msgbuffer));
        memset(filebuffer, '\0', sizeof(msgbuffer));
        switch(req_cmd){
            case get:
                filepos = 0; // reset filepos for each get.
                // receive filename and load buffer with contents
                bytes_received = recvfrom(server_sockid, msgbuffer, sizeof(msgbuffer), 0, (struct sockaddr *)&client_sockaddr, &client_sockaddr_size);
                // msgbuffer contains the filename of specified file
                strcpy(filename, msgbuffer);
                filesize = getFileSizeBytes(filename);
                // Send filesize to client
                if (filesize < 0){
                    // Handle file not found / other file errors.
                    perror("getfilesize");
                    bytes_sent = sendto(server_sockid, &filesize, sizeof(filesize), 0, (struct sockaddr *)&client_sockaddr, client_sockaddr_size);
                    bytes_sent = sendto(server_sockid, "Error sending file.", sizeof("Error sending file."), 0, 
                        (struct sockaddr *)&client_sockaddr, client_sockaddr_size);
                    break;
                }
                bytes_sent = sendto(server_sockid, &filesize, sizeof(filesize), 0, (struct sockaddr *)&client_sockaddr, client_sockaddr_size);

                for(;;){
                    if (filepos >= filesize){
                        break;
                    }
                    bzero(filebuffer, MAXMSGLENGTH);
                    written_bytes = saveFileContents(filename, filebuffer, filepos, MAXMSGLENGTH);
                    filepos += written_bytes; // Update file pos
                    bytes_sent = sendto(server_sockid, filebuffer, written_bytes, 0, 
                        (struct sockaddr *)&client_sockaddr, client_sockaddr_size);
                    // Wait for acknowledgment packet saying the client is ready for more data
                    bytes_received = recvfrom(server_sockid, msgbuffer, sizeof(msgbuffer), 0, (struct sockaddr *)&client_sockaddr, &client_sockaddr_size);
                    if (strstr(msgbuffer, "READY") != NULL){
                        memset(msgbuffer, '\0', sizeof(msgbuffer));
                        continue;
                    }
                }
                // Send EOF packet.
                bytes_sent = sendto(server_sockid, "File sent successfully.", sizeof("File sent successfully."), 0, 
                    (struct sockaddr *)&client_sockaddr, client_sockaddr_size);
                break;
            case put:
                filepos = 0; // reset filepos for each get.
                // receive filename and load buffer with contents
                bytes_received = recvfrom(server_sockid, msgbuffer, sizeof(msgbuffer), 0, (struct sockaddr *)&client_sockaddr, &client_sockaddr_size);
                // msgbuffer contains the filename of specified file
                
                // Get filesize
                bytes_received = recvfrom(server_sockid, &filesize, sizeof(filesize), 0, (struct sockaddr *)&client_sockaddr, &client_sockaddr_size);

                if (filesize < 0){
                    bytes_sent = sendto(server_sockid, "Error saving file.", strlen("Error saving file."), 0, 
                        (struct sockaddr *)&client_sockaddr, client_sockaddr_size);
                    printf("Error saving file.\n");
                    break;
                } 
                strcpy(filename, msgbuffer);
                for(;;){ // Until filzesize bytes have been written. 
                    if (filepos >= filesize){
                        break;
                    } 
                    bytes_received = recvfrom(server_sockid, filebuffer, sizeof(filebuffer), 0, (struct sockaddr *)&client_sockaddr, &client_sockaddr_size);
                    written_bytes = writeFileContents(filename, filebuffer, filepos, bytes_received);
                    
                    if(written_bytes > 0){
                        filepos += written_bytes;
                        memset(filebuffer, '\0', sizeof(filebuffer));
                        // Send an acknowledgement packet asking for more data
                        bytes_sent = sendto(server_sockid, "READY", strlen("READY"), 0, (struct sockaddr *)&client_sockaddr, client_sockaddr_size);
                    }else{
                        printf("Error writing to file.\n");
                        break;
                    }
                }
                break;
            case delete:
                bytes_received = recvfrom(server_sockid, msgbuffer, sizeof(msgbuffer), 0, (struct sockaddr *)&client_sockaddr, &client_sockaddr_size);
                strcpy(filename, msgbuffer);
                
                if(removefile(filename) != 0)
                    bytes_sent = sendto(server_sockid, "Error deleting file.", strlen("Error deleting file."), 0, (struct sockaddr *)&client_sockaddr, client_sockaddr_size);
                else
                    bytes_sent = sendto(server_sockid, "File deleted.", strlen("File deleted."), 0, (struct sockaddr *)&client_sockaddr, client_sockaddr_size);
                if (bytes_sent < 0){
                    perror("server sendto");
                }
                break;
            case ls:
                // Create a temporary file to hold ls -l contents
                // https://stackoverflow.com/questions/73657134/reading-file-descriptor-size-from-popen-command-for-socket-programming-in-c
                filepos = 0;
                filesize = 0;
                system("ls -l > ls_tmp.txt");
                curdir = fopen("ls_tmp.txt", "r");
                if(curdir == NULL){
                    perror("ls server");
                    break;
                }

                // Send filesize 
                filesize = getFileSizeBytes("ls_tmp.txt");
                bytes_sent = sendto(server_sockid, &filesize, sizeof(filesize), 0, (struct sockaddr *)&client_sockaddr, client_sockaddr_size);

                while( ! feof(curdir) ){
                    memset(msgbuffer, '\0', sizeof(msgbuffer));
                    written_bytes = fread(msgbuffer, 1, sizeof(msgbuffer), curdir);
                    // Send ls contents to client
                    bytes_sent = sendto(server_sockid, msgbuffer, written_bytes, 0, (struct sockaddr *)&client_sockaddr, client_sockaddr_size);
                    if(bytes_sent < 0){
                        perror("server sendto");
                    }
                }
                pclose(curdir);
                removefile("ls_tmp.txt");
                break;
            case Exit:
                bytes_sent = sendto(server_sockid, "Goodbye!", 8, 0, (struct sockaddr *)&client_sockaddr, client_sockaddr_size);
                break;
            case error:
                printf("error");
                break;
            default:
                printf("resolve function error");
                break;
        }
        printf("\nFinished resolving cmd.\n");
        memset(filebuffer, '\0', sizeof(filebuffer));
        memset(msgbuffer, '\0', sizeof(msgbuffer)); // Clear out previous commands / files from buffer.
        memset(servermsg, '\0', sizeof(servermsg));
    }

    close(server_sockid); // close server socket
    return 0;
}
