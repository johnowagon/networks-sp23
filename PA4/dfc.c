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

int main (int argc, char** argv){
    char* cmd; //
    char* file;
    enum action c_cmd;
    int res;
    int is_down = 0;
    if (argc < 3 && (strcasecmp(argv[1], "list") != 0)){
        printf("./dfc <cmd> <filename> ... <filename>\n");
        return -1;
    }

    if (strcasecmp(argv[1], "get") != 0 && strcasecmp(argv[1], "put") != 0 && strcasecmp(argv[1], "list") != 0){
        printf("Command must be get, put, or list.\n");
        return -1;
    }

    // Get number of servers from scanning the .conf file
    // Create a data structure to hold info?
    cmd = argv[1];
    is_down = checkservs();
    if (is_down != 0)
        printf("WARNING: Some servers are not started.\nSome commands may not work.\n");
    c_cmd = resolvecmd(cmd);
    switch(c_cmd){
        case PUT:
            for (char **pargv = argv+2; *pargv != argv[argc]; pargv++){
                file = *pargv;
                if (is_down == 0){
                    res = put(file);
                    if (res == 0){
                        printf("Put successful for %s.\n", file);
                    }else{
                        printf("Problem with put for %s.\n", file);
                    }
                }else{
                    printf("%s put failed.\n", file);
                }
            }
            break;
        case GET:
            for (char **pargv = argv+2; *pargv != argv[argc]; pargv++){
                file = *pargv;
                if(is_down == 0){
                    res = get(file);
                    if (res == 0){
                        printf("Get successful for %s.\n", file);
                    }else{
                        printf("Problem with get for %s.\n", file);
                    }
                }else{
                    printf("%s is incomplete.\n", file);
                }
            }
            break;
        case LIST:
            break;
        default:
            break;
    }

    return 0;
}