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

typedef struct Tuple {
    int x;
    int y;
} Tuple;

typedef struct dfs_node {
    struct Tuple chunk;
    char* port;
    char* ip;
    struct dfs_node* next;
} dfs_node;

struct Tuple file_parts[4][4] = {
    {{0, 1}, {1, 2}, {2, 3}, {3, 4}},
    {{3, 4}, {0, 1}, {1, 2}, {2, 3}},
    {{2, 3}, {3, 4}, {0, 1}, {1, 2}},
    {{1, 2}, {2, 3}, {3, 4}, {0, 1}}
};

struct Tuple* getidx(char* filename){
    int numservers = count_lines("./dfc.conf");
    int idx = hash(filename) % numservers;
    printf("idx: %d", idx);

    return file_parts[idx];
}

int sendftosock(int sockfd, char* filename, int filepos){
    // Sends a files contents to a socket described by sockfd.
    int bytes_sent = 0;
    int bytes_read = 0;
    FILE *fp;
    char data[1024];

    fp = fopen(filename, "r");

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
        printf("Problem opening file.\n");
    }

    fclose(fp);
    return bytes_sent;
}

int send_chunk(int sockfd, char* filename, struct Tuple loc){
    // What if the file is 3 bytes long?
    int bytes_read, bytes_sent;
    int filesize = getfilesize(filename);
    int chunksize = filesize / count_lines("./dfc.conf");
    //printf("start: %d end : %d\n", loc.x*chunksize, loc.y*chunksize);
    char* buf = calloc(sizeof(char), chunksize);
    FILE* fp = fopen(filename, "r");
    if(fp == NULL)
        return -1;
    fseek(fp, loc.x*chunksize, SEEK_SET);
    if((bytes_read = fread(buf, sizeof(char), chunksize, fp)) < 0)
        return -1;
    
    //printf("file sec %d to %d :\n%s\n", loc.x*chunksize, loc.y*chunksize, buf);

    if((bytes_sent = send(sockfd, buf, bytes_read, 0)) < 0)
        return -1;

    if (bytes_sent != bytes_read)
        printf("ZUG ZUG!\n");

    free(buf);
    fclose(fp);
    return bytes_sent;
}

int append_file(char* f1, char* f2){
    FILE *fp1, *fp2;
    int c;

    fp1 = fopen(f1, "a");
    if (fp1 == NULL) {
        perror("Error opening file 1");
        return -1;
    }

    fp2 = fopen(f2, "r");
    if (fp2 == NULL) {
        perror("Error opening file 2");
        fclose(fp1);
        return -1;
    }

    while ((c = fgetc(fp2)) != EOF) {
        fputc(c, fp1);
    }

    fclose(fp1);
    fclose(fp2);
    return 0;
}

int get(char* filename){
    char server[20], name[20], ip_port[10];
    enum serv_status st;
    int sockfd;
    char *recvb = malloc(1024);
    char file_buf[1024];
    int bytes_recvd, bytes_written;
    char *ip, *port;
    FILE *fp, *filename_fp, *tmp_fp;
    char tmp_fname[100];
    struct Tuple* seq = getidx(filename);
    int numservers = count_lines("./dfc.conf");
    sprintf(tmp_fname, "%s.tmp", filename);

    fp = fopen("./dfc.conf", "r");
    //filename_fp = fopen(filename, "w+");
    filename_fp = fopen(tmp_fname, "w+");
    if (fp == NULL || filename_fp == NULL)
        return -1;

    for (int i = 0; i < numservers; i++){
        if (fscanf(fp, "%s %s %s", server, name, ip_port) != 3){
            break;
        }

        // Open a new file so that other contents can be appended.
        if (seq[i].x == 0){
            //tmp_fp = fopen(tmp_fname, "w+");
            tmp_fp = fopen(filename, "w+");
            if (tmp_fp == NULL)
                return -1;
            fclose(filename_fp); // Dont need this one anymore.
            filename_fp = tmp_fp;
        }
        // Split the IP address and port number
        //printf("%s %s\n",name, ip_port);
        ip = strtok(ip_port, ":");
        port = strtok(NULL, ":");

        sockfd = get_fd_from_ip_port(ip, port);
        if (sockfd < 0){
            return -1;
        }

        if(send(sockfd, "GET", 3, 0) < 0) // Can use strlen since sprintf uses null char
            perror("get send");

        if (recv(sockfd, recvb, sizeof(recvb), 0) < 0)
            perror("get recv");
        
        st = resolvestatus(recvb);
        switch(st){
            case OK:
                if(send(sockfd, filename, sizeof(filename), 0) < 0)
                    perror("get send");
                while((bytes_recvd = recv(sockfd, file_buf, sizeof(file_buf), 0)) > 0){
                    if(bytes_recvd < 0)
                        perror("get recv");
                    if((bytes_written = fwrite(file_buf, sizeof(char), bytes_recvd, filename_fp)) != bytes_recvd)
                        perror("get fwrite");
                    bzero(file_buf, 1024);
                }
                break;
            default:
                return -1;
        }
        close(sockfd);
        bzero(recvb, 1024);
    }
    free(recvb);
    fclose(fp); fclose(filename_fp);
    append_file(filename, tmp_fname);
    remove(tmp_fname);
    return 0;
}

int put(char* filename){
    char server[20], name[20], ip_port[10];
    enum serv_status st;
    int sockfd;
    char *recvb = malloc(1024);
    char *ip, *port;
    FILE *fp, *filename_fp; // For scanning conf file.
    struct Tuple* seq = getidx(filename);
    int numservers = count_lines("./dfc.conf");
    fp = fopen("./dfc.conf", "r");
    filename_fp = fopen(filename, "r");
    if (fp == NULL || filename_fp == NULL)
        return -1;

    for (int i = 0; i < numservers; i++){
        if (fscanf(fp, "%s %s %s", server, name, ip_port) != 3){
            break;
        }
        // Split the IP address and port number
        //printf("%s %s\n",name, ip_port);
        ip = strtok(ip_port, ":");
        port = strtok(NULL, ":");

        sockfd = get_fd_from_ip_port(ip, port);
        if (sockfd < 0){
            return -1;
        }

        if(send(sockfd, "PUT", 3, 0) < 0)
            perror("put send");

        if (recv(sockfd, recvb, sizeof(recvb), 0) < 0)
            perror("put recv");
        
        st = resolvestatus(recvb);
        switch(st){
            case OK:
                if(send(sockfd, filename, sizeof(filename), 0) < 0)
                    perror("put send");
                if(send_chunk(sockfd, filename, seq[i]) == -1)
                    send_chunk(sockfd, filename, seq[i]); //retry?
                break;
            default:
                return -1;
        }
        close(sockfd);
        bzero(recvb, 1024);
    }
    free(recvb);
    fclose(fp);
    return 0;
}

enum action resolvecmd(char* cmd){
    if (strcasecmp(cmd, "get") == 0){
        return GET;
    }else if(strcasecmp(cmd, "put") == 0){
        return PUT;
    }else if(strcasecmp(cmd, "list") == 0){
        return LIST;
    }else if(strcasecmp(cmd, "status") == 0){
        return STATUS;
    }
    return UNSUPPORTED;
}

enum serv_status resolvestatus(char* status){
    if(strcasecmp(status, "OK") == 0){
        return OK;
    }else if (strcasecmp(status, "SENDING") == 0){
        return SENDING;
    }else{
        return DOWN;
    }
}

int get_fd_from_ip_port(char* IP, char* port){
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(IP, port, &hints, &servinfo)) != 0) {
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

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        printf("setsockopt(SO_REUSEADDR) failed");

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }


    return sockfd;
}

int count_lines(char* filename)
{
    FILE* file = fopen(filename, "r");
    // I got this code from https://stackoverflow.com/questions/12733105/c-function-that-counts-lines-in-file
    char buf[65536];
    int counter = 0;
    for(;;)
    {
        size_t res = fread(buf, 1, 65536, file);
        if (ferror(file))
            return -1;

        size_t i;
        for(i = 0; i < res; i++)
            if (buf[i] == '\n')
                counter++;

        if (feof(file))
            break;
    }

    fclose(file);
    return counter;
}



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

int checkservs(){
    // Check each server in dfc.conf file.
    char server[20], name[20], ip_port[10];
    enum serv_status st;
    int sockfd;
    char *recvb = malloc(100);
    char *ip, *port;
    FILE *fp; // For scanning conf file.
    fp = fopen("./dfc.conf", "r");
    if (fp == NULL)
        return -1;
    while (fscanf(fp, "%s %s %s", server, name, ip_port) == 3) {
        // Split the IP address and port number
        //printf("%s %s\n",name, ip_port);
        ip = strtok(ip_port, ":");
        port = strtok(NULL, ":");

        sockfd = get_fd_from_ip_port(ip, port);
        if (sockfd < 0){
            return -1;
        }
        if(send(sockfd, "STATUS", 6, 0) < 0)
            perror("check send");

        if (recv(sockfd, recvb, sizeof(recvb), 0) < 0)
            perror("check recv");
        
        st = resolvestatus(recvb);

        if (st != OK && st != SENDING){
            return -1;
        }        
        
        close(sockfd);
        bzero(recvb, 100);
    }
    free(recvb);
    return 0;
}

unsigned long hash(char *str)
{
    // Hash function completely designed by me.
    // Just kidding: http://www.cse.yorku.ca/~oz/hash.html
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

int getfilesize(char* filename){
    // Returns filesize in bytes after error checks
    int size;
	FILE *f;
    
	f = fopen(filename, "r");
	if (f == NULL) return -1;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);

	return size;
}

int s_handle_cmd(char* cmd, int sockfd, char* dirname){
    char fname_buf[100];
    char filepath[150];
    int filesize;
    int bytes_sent = 0;
    enum action req = resolvecmd(cmd);
    switch(req){
        case GET:
            printf("Got a GET request.\n");
            if(send(sockfd, "OK", 2, 0) < 0)
                perror("status send");
            if(recv(sockfd, fname_buf, sizeof(fname_buf), 0) < 0)
                perror("status recv");
            sprintf(filepath, "./%s/%s", dirname, fname_buf);
            filesize = getfilesize(filepath);
            while ((bytes_sent += sendftosock(sockfd, filepath, bytes_sent)) < filesize); // Send entire file
            break;
        case PUT:
            if(send(sockfd, "OK", 2, 0) < 0)
                perror("status send");
            if(recv(sockfd, fname_buf, sizeof(fname_buf), 0) < 0)
                perror("status recv");
            sprintf(filepath, "./%s/%s", dirname, fname_buf);
            if (s_save(sockfd, filepath) < 0)
                perror("s_save");
            break;
        case LIST:
            break;
        case STATUS:
            if(send(sockfd, "OK", 2, 0) < 0)
                perror("status send");
            break;
        case UNSUPPORTED:
            break;
    }
    return 0;
}

int s_save(int sockfd, char* filename){
    char buf[1024];
    int bytes_recvd;
    int bytes_written = 0;
    int tot;
    FILE* fp;
    fp = fopen(filename, "wb");
    while((bytes_recvd = recv(sockfd, buf, sizeof(buf), 0)) > 0){
        if((bytes_written = fwrite(buf, sizeof(char), bytes_recvd, fp)) != bytes_recvd){
            printf("Fwrite failed.\n");
            return -1;
        }
        bzero(buf, 1024);
        tot += bytes_written;
    }
    fclose(fp);
    close(sockfd);
    return tot;
}