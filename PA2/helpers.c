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


#define BACKLOG 10   // how many pending connections queue will hold
#define REQUEST_BUFFER_SIZE 65536
#define RESPONSE_BUFFER_SIZE 65536

// Error Response header template
const char* errTemplate= "%s %d %s\r\nContent-length: %ld\r\n\r\n";

// Normal Response header template
const char* resTemplate = "%s %d %s\r\nContent-type: %s\r\nContent-length: %ld\r\nConnection: %s\r\n\r\n";

typedef enum httpverb {GET, UNSUPPORTED} httpverb;
typedef enum contenttype {HTML, TXT, PNG, GIF, JPG, ICO, CSS, JS, UNKNOWN} contenttype;
typedef enum httpversion {HTTP1_1, HTTP1_0, WRONG} httpversion;

typedef struct Request {
    enum httpverb method; // GET, UNSUPPORTED
    char* objectname;     // Name of file on server disk.
    httpversion httpversion;    // http/1.0 or http/1.1
    int keepalive;        // keep-alive or null (1 or 0)
    int malformed;        // To indicate any failure of parsing. Will be used to determine a 400 response.
} Request;

typedef struct Response {
    struct Request* crequest;
    int responsecode;
    char* objectname;
    httpversion httpversion;
    contenttype content_type;
    long int content_length;
    char* content;
} Response;

void replaceTrailingNewline(char* str){
    // Replaces trailing newline on str if present.
    int len = strlen(str);
    if(str[len-1] == '\n'){
        str[len-1] = '\0';
    }
}

int isDirectory(const char *path) {
    // Inspired by https://stackoverflow.com/questions/4553012/checking-if-a-file-is-a-directory-or-just-a-file
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

int saveFileContents(char* filename, char* buffer, long int filepos, int maxmsglength){
    // Loads file contents in a buffer using a file position
    // Returns the number of bytes written to buffer.
    // code inspired by https://stackoverflow.com/questions/2029103/correct-way-to-read-a-text-file-into-a-buffer-in-c
    size_t newpos; // Position of fp
    replaceTrailingNewline(filename); // Removes newline from end of string if present.
    FILE *fp = fopen(filename, "rb");
    if (fp != NULL) {
        fseek(fp, filepos, SEEK_SET);
        newpos = fread(buffer, sizeof(char), maxmsglength, fp); // Read maxmsglength bytes from the file
        if ( ferror( fp ) != 0 ) {
            fputs("Error reading file", stderr);
        }
        fclose(fp);
    }else{
        return 0;
    }
    return newpos;
}

struct Request* parse2(char* requestbuf){
    // Parses a given http request and populates a request struct with information.
    // Only parses http version, connection type, and method parameters.
    size_t bytesToCarriage; // Will be set to read the amount of bytes from the given request header.
    size_t bytesToSpace; // Each header has a trailing space after it.
    char* tmpbuf;       // Temporary buffer to hold header contents
    // Initialize return struct
    struct Request* crequest;
    crequest = malloc(sizeof(struct Request));
    memset(crequest, 0, sizeof(Request));
    int i;
    int bytecounter = 0;

    while((bytesToCarriage = strcspn(requestbuf, "\r\n")) != 0){
        tmpbuf = malloc(bytesToCarriage);
        memcpy(tmpbuf, requestbuf, bytesToCarriage);
        bytesToSpace = strcspn(tmpbuf, " ");
        if (bytesToSpace == 0){
            // Malformed
        }
        if(memcmp(tmpbuf, "GET", bytesToSpace) == 0){
            // In first header
            memcpy()
        }

        // Move past examined contents and carriage
        requestbuf += bytesToCarriage + 2;
    }
    printf("%ld\n%d",bytesToCarriage, bytecounter);
    free(crequest);
    return NULL;
}

struct Request* parse(char* requestbuf){
    // Parses a given http request and populates a request struct with information.
    // Only parses http version, connection type, and method parameters.
    size_t bytesToRead; // Will be set to read the amount of bytes from the
                        // given request header.
    char* tmpbuf;       // Temporary buffer to hold header contents
    // Initialize return struct
    struct Request* crequest;
    crequest = malloc(sizeof(struct Request));
    memset(crequest, 0, sizeof(Request));


    // Get method from client request
    bytesToRead = strcspn(requestbuf, " ");
    tmpbuf = malloc(bytesToRead + 1);
    memcpy(tmpbuf, requestbuf, bytesToRead);
    if (strstr(tmpbuf, "GET") != 0){
        crequest->method = GET;
    }else{
        crequest->method = UNSUPPORTED;
    }
    requestbuf += bytesToRead + 1; // Move pointer contents along X bytes

    // Get URI from request method
    bytesToRead = strcspn(requestbuf, " ");
    if (bytesToRead == 0){
        // Malformed policy
        free(crequest);
        return NULL;
    }else{
        tmpbuf = realloc(tmpbuf, bytesToRead);
        crequest->objectname = malloc(bytesToRead); // C string
        memcpy(tmpbuf, requestbuf, bytesToRead);
        if (tmpbuf == NULL){
            // Malformed policy
            free(crequest);
            return NULL;
        }else{
            memcpy(crequest->objectname, tmpbuf, bytesToRead);
            crequest->objectname += 1; // Remove leading slash from objectname string.
            if(strlen(crequest->objectname) == 0){
                crequest->objectname = "index.html";
            }else if(isDirectory(crequest->objectname)){
                sprintf(tmpbuf, "%s%s", crequest->objectname, "index.html");
                memcpy(crequest->objectname, tmpbuf, bytesToRead+10); // plus 10 to fit index/html string in.
            }
        }
    }
    
    requestbuf += bytesToRead + 1;

    // Get http version from header
    bytesToRead = strcspn(requestbuf, "\r\n");
    tmpbuf = realloc(tmpbuf, bytesToRead);
    memcpy(tmpbuf, requestbuf, bytesToRead);
    requestbuf += bytesToRead + 1;
    // Check http version
    if (memcmp(tmpbuf, "HTTP/", 5) == 0){
        tmpbuf += 5; // Move pointer past http/
        if(memcmp(tmpbuf,"1.1",3) == 0){
            crequest->httpversion = HTTP1_1;
        }else if(memcmp(tmpbuf,"1.0",3) == 0){
            crequest->httpversion = HTTP1_0;
        }else{
            // Unsupported HTTP version
            crequest->httpversion = WRONG;
        }
    }else{
        // Malformed policy
        free(crequest);
        return NULL;
    }

    //free(tmpbuf);

    // Set keep alive if http version is 1.1
    if(crequest->httpversion == HTTP1_1){
        crequest->keepalive = 1;
    }else{
        crequest->keepalive = 0;
    }

    // If not http/1.0, find connection header?

    // Find other header information
    return crequest;
}

int getfilesize(char* filename){
    // Returns filesize in bytes after error checks
    int size;
	FILE *f;
    replaceTrailingNewline(filename);

	f = fopen(filename, "rb");
	if (f == NULL) return -1;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);

	return size;
}

contenttype getcontenttype(char* filename){
    // Return the contenttype from the given filename
    // Code was inspired by https://stackoverflow.com/questions/5309471/getting-file-extension-in-c
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return UNKNOWN;
    dot += 1;

    // Move this into the parse function to check for 400s.

    if(strstr(dot, "html") != 0){
        return HTML;
    }else if(strstr(dot, "txt") != 0){
        return TXT;
    }else if(strstr(dot, "png") != 0){
        return PNG;
    }else if(strstr(dot, "gif") != 0){
        return GIF;
    }else if(strstr(dot, "jpg") != 0){
        return JPG;
    }else if(strstr(dot, "ico") != 0){
        return ICO;
    }else if(strstr(dot, "css") != 0){
        return CSS;
    }else if(strstr(dot, "js") != 0){
        return JS;
    }else{
        return UNKNOWN;
    }
}

struct Response* construct(struct Request* crequest){
    // Construct an HTTP response to send back to the client.
    struct Response* cresponse;
    int filesize;
    cresponse = malloc(sizeof(struct Response));
    memset(cresponse, '\0', sizeof(Response));

    // Lotta duplicated code here?

    if(crequest == NULL){
        // Malformed policy
        cresponse->httpversion = HTTP1_1;
        cresponse->content_length = 0;
        cresponse->responsecode = 400;
        return cresponse;
    }
    cresponse->crequest = crequest;

    if(crequest->method == UNSUPPORTED){
        cresponse->httpversion = HTTP1_1;
        cresponse->content_length = 0;
        cresponse->responsecode = 405;
        return cresponse;
    }
    cresponse->objectname = crequest->objectname;

    // Handling improper http versions.
    if(crequest->httpversion == WRONG){
        cresponse->httpversion = HTTP1_1;
        cresponse->content_length = 0;
        cresponse->responsecode = 505;
        return cresponse;
    }

    // Malformed
    if((cresponse->content_type = getcontenttype(cresponse->objectname)) == UNKNOWN){
        cresponse->httpversion = HTTP1_1;
        cresponse->content_length = 0;
        cresponse->responsecode = 400;
        return cresponse; 
    }

    // Handle any file errors.
    if((filesize = getfilesize(crequest->objectname)) < 0){
        printf("Error: file not found.\n");
        cresponse->content_length = 0;
        
        switch(errno){
            case ENOENT:
                // File not found on server.
                cresponse->httpversion = HTTP1_1;
                cresponse->responsecode = 404;
                return cresponse;
            case EACCES:
                // File permissions do not allow I/O.
                cresponse->httpversion = HTTP1_1;
                cresponse->responsecode = 403;
                return cresponse;
            default:
                cresponse->httpversion = HTTP1_1;
                cresponse->responsecode = 400;
        }
    }

    // Else, request was ok
    cresponse->httpversion = crequest->httpversion;
    cresponse->responsecode = 200;
    cresponse->content_length = filesize;

    return cresponse;
}

char* getversionstring(httpversion ver){
    if(ver == HTTP1_0){
        return "HTTP/1.0";
    }else if(ver == HTTP1_1){
        return "HTTP/1.1";
    }
    return "";
}

char* getcontentstring(contenttype cont){
    switch(cont){
        case HTML:
            return "text/html";
        case TXT:
            return "text/plain";
        case PNG:
            return "image/png";
        case GIF:
            return "image/gif";
        case JPG:
            return "image/jpg";
        case ICO:
            return "image/x-icon";
        case CSS:
            return "text/css";
        case JS:
            return "application/javascript";
        default:
            return "";
    }
}

char* getstatus(int responsecode){
    switch(responsecode){
        case 200:
            return "OK";
        case 405:
            return "Method Not Allowed";
        case 404:
            return "Not Found";
        case 403:
            return "Forbidden";
        case 400:
            return "Bad Request";
        case 505:
            return "HTTP Version Not Supported";
        default:
            return "Unknown";
    }
}

int formatresheaders(char* buf, struct Response* cresponse){
    // Populates buf with the http response headers described by cresponse.
    // Returns number of bytes written to the buffer.
    int filepos = 0;
    int bytes_written = 0;
    char* keepalive;
    if (cresponse->crequest != 0 ){
        keepalive = cresponse->crequest->keepalive == 1 ? "Keep-alive" : "Close";
    }else{
        keepalive = "Close";
    }
    char* httpversion = getversionstring(cresponse->httpversion);
    char* contenttype = getcontentstring(cresponse->content_type);
    char* status = getstatus(cresponse->responsecode);

    if(cresponse->responsecode != 200){
        // File not found.
        bytes_written += sprintf(buf, errTemplate, 
                    httpversion, cresponse->responsecode, status, cresponse->content_length);
        return bytes_written;
    }

    // Write headers to server response
    bytes_written += sprintf(buf, resTemplate, 
                    httpversion, cresponse->responsecode, status, contenttype, cresponse->content_length, keepalive);
    
    return bytes_written;
}

void printresheaders(struct Response* res){
    printf("object-name: %s\n", res->objectname);
    printf("response; %d\n", res->responsecode);
    printf("version: %s\n", getversionstring(res->httpversion));
    printf("content-type: %s\n", getcontentstring(res->content_type));
    printf("content-length: %ld\n", res->content_length);
    printf("connection: %d\n", res->crequest->keepalive);
}

void sigchld_handler(int s){
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}