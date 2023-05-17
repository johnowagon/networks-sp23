#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <signal.h>
#include <fcntl.h>
#include "s_helpers.h"
#include "util.h"


void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int handlecmd(int sockfd, char* dirname){
    char recv_buf[1024];
    char *filesize;
    char *cmd, *filename;
    if(recv(sockfd, recv_buf, 1024, 0) < 0)
        perror("handlecmd");
    
    cmd = strtok(recv_buf, " ");
    filename = strtok(NULL, " ");
    filesize = strtok(NULL, "\0");
    if(strcmp(cmd, "LIST") == 0){
        list(sockfd, dirname, filename);
    }else if(strcmp(cmd, "GET") == 0){
        get(sockfd, dirname, filename);
    }else if(strcmp(cmd, "PUT") == 0){
        put(sockfd, dirname, filename, atoi(filesize));
    }else if(strcmp(cmd, "STATUS") == 0){
        if(send(sockfd, "OK", 2, 0) < 0)
            perror("status");
    }else {
        printf("Unknown command.\n");
        return 1;
    }

    return 0;
}

int list(int sockfd, char* dirname, char* filename){
    // send contents of dirname to sockfd
    int bytes_read;
    char *buf = NULL;
    char cmdbuf[256];
    //printf("FNAME: %s\n", filename);
    if (filename){
        sprintf(cmdbuf, "ls -m ./%s/%s | tr ' ' ','", dirname, filename);
    }else{
        sprintf(cmdbuf, "ls -m ./%s | tr -d '\\n '", dirname);
    }
    //printf("cmd: %s\n", cmdbuf);
    FILE *cmd = popen(cmdbuf, "r");
    if (cmd == NULL)
        perror("server list");
    
    size_t buf_size = 0;
    ssize_t result = getdelim(&buf, &buf_size, EOF, cmd);
    pclose(cmd);
    //printf("bytes: %ld res: %s\n",result, buf);
    buf[result+1] = '\0';
    if (result < 0) {
        perror("ls failed");
        return -1;
    } else {
        if(send(sockfd, buf, strlen(buf)+1, 0) < 0)
            perror("server list");
    }
    //printf("done sending.\n");
    free(buf);
    return 0;
}

int get(int sockfd, char* dirname, char* filename){
    char filepath[256];
    int fd; // file descriptor
    sprintf(filepath, "./%s/%s", dirname, filename);
    int filesize = getfilesize(filepath);

    fd = open(filepath, O_RDONLY);

    sendfile(sockfd, fd, 0, filesize);

    close(fd);
    return 0;
}

int put(int sockfd, char* dirname, char* filename, int filesize){
    int bufsize = 4096;
    char fbuf[bufsize];
    char filepath[256];
    long int bytes_recvd = 0, written = 0;
    sprintf(filepath, "./%s/%s", dirname, filename);
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL)
        perror("server put");
    //printf("fsize: %d\n", filesize);
    if (send(sockfd, "OK", 2, 0) < 0)
        perror("server put");
    while(bytes_recvd < filesize){
        bytes_recvd = recv(sockfd, fbuf, sizeof(fbuf), 0);
        if (bytes_recvd == 0){
            printf("client disconnect\n");
            break;
        }
        if (fwrite(fbuf, 1, bytes_recvd, fp) != bytes_recvd)
            printf("fwrite failed\n");
        bzero(fbuf, 4096);
        written += bytes_recvd;
        //printf("bytes recvd: %d\n", bytes_recvd);
    }
    //printf("recvd %d\n", written);
    fclose(fp);
    return 0;
}

int init(char* port){
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    
    struct sigaction sa;
    int yes=1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
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

    if (listen(sockfd, 10) == -1) {
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


    return sockfd;
}

