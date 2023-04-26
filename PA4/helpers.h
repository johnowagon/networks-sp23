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
unsigned long hash(char *str);
int getfilesize(char* filename);
int handle_cmd(char* cmd, int sockfd);
char* sendchunks(char* filename, struct Tuple chunk_location);
int get_fd_from_ip_port(char* IP, char* port);
void *get_in_addr(struct sockaddr *sa);
void sigchld_handler(int s);
int checkservs();
typedef struct Tuple Tuple;
enum action resolvecmd(char* cmd);
enum serv_status resolvestatus(char* status);

typedef enum action {LIST, GET, PUT, STATUS, UNSUPPORTED} action;
typedef enum serv_status {SENDING, OK, DOWN} serv_status;
int get_fd_from_ip_port(char* IP, char* port);
