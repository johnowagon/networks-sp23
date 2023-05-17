#include <stdio.h>
#include "s_helpers.h"
#include "util.h"

int main(int argc, char** argv){
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    char* dirname;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;

    if(argc < 3){
        printf("./dfs <dir> <portno>\n");
        return -1;
    }

    dirname = argv[1];
    //printf("dir: %s\n", dirname);

    if((sockfd = init(argv[2])) < 0)
        printf("Could not start server.\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            
            //printf("got a connection\n");
            handlecmd(new_fd, dirname);

            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}