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
#include <signal.h>

int init(char *port);
int handlecmd(int sockfd, char* dirname);
int list(int sockfd, char* dirname, char* filename);
int get(int sockfd, char* dirname, char* filename);
int put(int sockfd, char* dirname, char* filename, int filesize);

void *get_in_addr(struct sockaddr *sa);
void sigchld_handler(int s);
