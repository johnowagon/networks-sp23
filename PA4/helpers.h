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
int s_handle_cmd(char* cmd, int sockfd, char* dirname);
int s_save2(int sockfd, char* dirname); // Saves the contents sent by recv.
int s_send2(int sockfd, char* dirname);
int s_send(int sockfd, char* dirname);
int s_list(int sockfd, char* dir);
//int send_chunk(char* filename, struct Tuple chunk_location, int sockfd);
int get_fd_from_ip_port(char* IP, char* port);
void *get_in_addr(struct sockaddr *sa);
int append_file(char* f1, char* f2);
int send_fns(int sockfd, char* filename, int chunksize);
int sendftosock(int sockfd, char* filename, int filepos);
void sigchld_handler(int s);
int checkservs();
typedef struct Tuple Tuple;
enum action resolvecmd(char* cmd);
enum serv_status resolvestatus(char* status);
int send_chunk(int sockfd, char* filename, struct Tuple loc);
int put(char* filename);
int get2(char* filename);
int get(char* filename);
int list();
int distribute(char* filename);
int count_lines(char* file);
typedef struct dfs_node dfs_node;
typedef enum action {LIST, GET, PUT, STATUS, UNSUPPORTED} action;
typedef enum serv_status {SENDING, OK, DOWN} serv_status;
int get_fd_from_ip_port(char* IP, char* port);
