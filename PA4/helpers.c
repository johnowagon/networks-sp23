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

typedef struct Tuple {
    int x;
    int y;
} Tuple;

typedef struct dfs_node {
    struct Tuple chunk;
    char* name;
    char* ip;
    struct dfs_node* next;
} dfs_node;

typedef enum action {LIST, GET, PUT, STATUS, UNSUPPORTED} action;
typedef enum serv_status {SENDING, OK, DOWN} serv_status;

// Write a function to generate this kind of table programmatically?
struct Tuple file_parts[4][4] = {
    {{1, 2}, {2, 3}, {3, 4}, {4, 1}},
    {{4, 1}, {1, 2}, {2, 3}, {3, 4}},
    {{3, 4}, {4, 1}, {1, 2}, {2, 3}},
    {{2, 3}, {3, 4}, {4, 1}, {1, 2}}
};

/*for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
        printf("(%d, %d) ", array[i][j].x, array[i][j].y);
    }
    printf("\n");
}*/

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
    struct addrinfo hints, chints, *servinfo, *p;
    int yes = 1;
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

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }


    return sockfd;
}

int count_lines(FILE* file)
{
    // I got this code from https://stackoverflow.com/questions/12733105/c-function-that-counts-lines-in-file
    char buf[65536];
    int counter = 0;
    for(;;)
    {
        size_t res = fread(buf, 1, 65536, file);
        if (ferror(file))
            return -1;

        int i;
        for(i = 0; i < res; i++)
            if (buf[i] == '\n')
                counter++;

        if (feof(file))
            break;
    }

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



/*char* sendchunks(char* filename, struct Tuple chunk_location){
    int size = getfilesize(filename);
    int chunksize = size / 4;
    char* chunk;
}*/



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
    char full_path[512];

    sprintf(full_path, "%s%s", "./www", filename);
    
	f = fopen(full_path, "r");
	if (f == NULL) return -1;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);

	return size;
}

int handle_cmd(char* cmd, int sockfd){
    enum action req = resolvecmd(cmd);
    switch(req){
        case GET:
            break;
        case PUT:
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