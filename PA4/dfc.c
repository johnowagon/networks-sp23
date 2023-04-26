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
    char* cur_filename; // Current file in list of arguments.
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
    if (checkservs() == 0){
        printf("servers are up\n");
    }else{
        printf("Some servers are not started.\n");
    }

    return 0;
}