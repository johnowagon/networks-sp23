#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

typedef struct Request {
    char* method; // GET, UNSUPPORTED
    char* reqURI;     // Name of file on server disk.
    char* httpversion;    // http/1.0 or http/1.1
    char* reqHost;        // Client requested host.
    char* connection;
    char* reqPortno;        // Requested port number
} Request;

typedef struct Response {
    int responsecode;
    char* uri;
    char* content_type;
    char* httpversion;
    long int content_length;
    char* tarHost;
    char* connection;
    char* content;
} Response;

struct Request* parse(char* requestbuf); // Creates a new response struct from the given http response
struct Response* verify(Request* clientreq); // Main verification function, returns 0 if valid request, 1 if invalid and populates 
int blocked(char* address); // Checks ./blocklist to see if the requested host is forbidden or not.
int fdsendres(int sockfd, struct Request* crequest); // Sends headers and file contents, verification must be done prior.
void sigchld_handler();
void *get_in_addr(struct sockaddr *sa);
int isDirectory(char *path);
void replaceTrailingNewline(char* str);
char* getstatus(int responsecode);
int formatresheaders(char* buf, struct Response* sresponse);
int getfilesize(char* filename);
char* getmimetype(char* filename);
void printresheaders(struct Response* res);
int sendftosock(int sockfd, char* filename, int filepos);
int forward_and_respond(int from_sockfd, int to_sockfd, char* request);
int isIP(char* hostname);
char* getIp(char* hostname);
char* getHostname(char* IP);
int getcontentlength(char* response);