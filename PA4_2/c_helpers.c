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
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include "c_helpers.h"
#include "util.h"

struct Tuple file_parts[4][4] = {
    {{0, 1}, {1, 2}, {2, 3}, {3, 4}},
    {{3, 4}, {0, 1}, {1, 2}, {2, 3}},
    {{2, 3}, {3, 4}, {0, 1}, {1, 2}},
    {{1, 2}, {2, 3}, {3, 4}, {0, 1}}
};

struct Tuple* getidx(char* filename){
    int numservers = count_lines("./dfc.conf");
    int idx = hash(filename) % numservers;
    //printf("idx: %d", idx);

    return file_parts[idx];
}

int sortstring( const void *str1, const void *str2 )
{
    char *const *pp1 = str1;
    char *const *pp2 = str2;
    return strcmp(*pp1, *pp2);
}

int handlecmd(char* cmd, char *filename){

    if(strcasecmp(cmd, "GET") == 0){
        get(filename);
    }else if (strcasecmp(cmd, "LIST") == 0){
        list(filename);
    }else if (strcasecmp(cmd, "PUT") == 0){
        put(filename);
    }

    return 0;
}

int checkfile(char* filename){
    // Check each server in dfc.conf file.
    char server[20], name[20], ip_port[10];
    char cmd[256];
    //printf("%s\n", filename);
    sprintf(cmd, "LIST %s", filename);
    int sockfd, bytes_recvd, filecount = 0;
    char recvb[1024];
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
        
        //printf("cmd: %s\n", cmd);
        if(send(sockfd, cmd, strlen(cmd)+1, 0) < 0)
            perror("checkfile");
        
        if((bytes_recvd = recv(sockfd, recvb, 1024, 0)) < 0)
            perror("checkfile");


        if (bytes_recvd != 0){
            // server has file
            filecount += 1;
        }
        
        close(sockfd);
        bzero(recvb, 1024);
    }

    if (filecount == 4)
        return 1;

    return 0;
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
        perror("getaddrinfo");
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "failed to connect\n");
        return -1;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR)");

    struct linger lin;
    lin.l_onoff = 1;
    lin.l_linger = 5;
    if(setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin)) < 0)
        perror("setsockopt(SO_LINGER)");

    return sockfd;
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

int list(char* filename){
    char server[20], name[20], ip_port[10];
    char cmd[256];
    if (filename != NULL){
        sprintf(cmd, "LIST %s", filename);
    }else{
        sprintf(cmd, "LIST");
    }
    int sockfd, bytes_recvd = 0;
    char recvb[1024];
    char *files[4096];
    int num_files = 0;
    int final_items = 0;
    char *ip, *port, *cur_filename, *iterator;
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
            continue;
        }
        if(send(sockfd, cmd, strlen(cmd)+1, 0) < 0)
            perror("list");
        if((bytes_recvd = recv(sockfd, recvb, 1023, 0)) < 0)
            perror("list");
        if (bytes_recvd == 0){
            // server has no files, we ignore them and carry on.
            bzero(recvb, 1024);
            continue;
        }
        //recvb[bytes_recvd+1] = '\0';
        //printf("%s\n", recvb);
        // Get all files into a list
        cur_filename = strtok(recvb, ",");
        while (cur_filename != NULL && num_files < 4096) {
            //printf("filearr: %s\n", cur_filename);
            files[num_files] = strdup(cur_filename);
            num_files++;
            cur_filename = strtok(NULL, ",");
        }
        
        close(sockfd);
        bzero(recvb, 1024);
    }

    if (num_files == 0){
        return 0;
    }

    qsort(files, num_files, sizeof(*files), sortstring);
    iterator = files[0];
    final_items++;
    for (int i = 1; i < num_files; i++) {
        if (strcmp(iterator, files[i]) != 0){
            if (final_items == 4){
                printf("%s\n", iterator);
            }else if (final_items < 4){
                printf("%s [incomplete]\n", iterator);
            }
            final_items = 1;
            iterator = files[i];
        }else{
            final_items++;
        }
        
        //printf("%d, filearr %d: %s\n", final_items, i, files[i]);
    }
    
    //Deal with last one
    if (final_items == 4){
        printf("%s\n", iterator);
    }else if (final_items < 4){
        printf("%s [incomplete]\n", iterator);
    }

    for (int i = 0; i < num_files; i++){
        free(files[i]);
    }

    return 0;
}

int get(char* filename){
    printf("fname: %s\n", filename);
    char server[20], name[20], ip_port[10];
    int sockfd;
    char *recvb = malloc(1024);
    char file_buf[1024];
    int tot = 0;
    int bytes_recvd, bytes_written;
    char *ip, *port;
    FILE *fp, *filename_fp, *tmp_fp;
    char tmp_fname[100], cmd[256];
    struct Tuple* seq = getidx(filename);
    int numservers = count_lines("./dfc.conf");
    sprintf(tmp_fname, "%s.tmp", filename);
    sprintf(cmd, "GET %s", filename);

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

        if(send(sockfd, cmd, strlen(cmd)+1, 0) < 0) // Can use strlen since sprintf uses null char
            perror("get send");

        while((bytes_recvd = recv(sockfd, file_buf, sizeof(file_buf), 0)) > 0){
            if(bytes_recvd < 0)
                perror("get recv");
            if((bytes_written = fwrite(file_buf, sizeof(char), bytes_recvd, filename_fp)) != bytes_recvd)
                perror("get fwrite");
            bzero(file_buf, 1024);
            tot += bytes_written;
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

int send_chunk2(int sockfd, char* filename, struct Tuple loc){
    int filesize = getfilesize(filename);
    int chunksize = filesize / count_lines("./dfc.conf");
    char servermsg[128];
    char cmd[128];
    sprintf(cmd, "PUT %s %d", filename, chunksize);
    if (send(sockfd, cmd, strlen(cmd)+1, 0) < 0)
        perror("client put");
    if (recv(sockfd, servermsg, sizeof(servermsg), 0) < 0)
        perror("client put");

    int bufsize = 1024;
    long int filepos, end;
    size_t size_to_send;
    int fd = open(filename, O_RDONLY);
    if(fd < 0)
        return -1;
    filepos = loc.x*chunksize;
    end = loc.y*chunksize;
    //printf("chunk: %d\n", chunksize);
    //printf("start: %d end %d\n", loc.x*chunksize, loc.y*chunksize);
    // Offset into buffer, this is where sendfile starts reading the buffer
    off_t offset = filepos;
    if (loc.y == 4){ // End of file
        size_to_send = (filesize - filepos);
    }else{
        size_to_send = chunksize; 
    }
    // Loop while there's still some data to send
    while (size_to_send > 0)
    {
        //printf("off: %ld sz: %ld\n", offset, size_to_send);
        ssize_t sent = sendfile(sockfd, fd, &offset, size_to_send);
        //printf("sent bytes: %ld\n", sent);
        //usleep(10000);
        if (sent <= 0)
            perror("sendfile");

        size_to_send -= sent;  // Decrease the length to send by the amount actually sent
    }

    close(fd);
    return 0;
}

int put(char* filename){
    char server[20], name[20], ip_port[10];
    
    int sockfd;
    char *ip, *port;
    FILE *fp, *filename_fp; // For scanning conf file.
    struct Tuple* seq = getidx(filename);
    int numservers = count_lines("./dfc.conf");
    fp = fopen("./dfc.conf", "r");
    filename_fp = fopen(filename, "r");
    if (fp == NULL || filename_fp == NULL){
        printf("%s put failed.\n", filename);
        return -1;
    }
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

        if(send_chunk2(sockfd, filename, seq[i]) == -1)
            perror("send_chunk");
        
        close(sockfd);
    }
    fclose(fp);
    return 0;
}

int checkservs(){
    // Check each server in dfc.conf file.
    char server[20], name[20], ip_port[10];
    int bytes_recvd = 0;
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

        if ((bytes_recvd=recv(sockfd, recvb, sizeof(recvb), 0)) < 0)
            perror("check recv");
        
        recvb[bytes_recvd+1] = '\0';

        if (strcmp(recvb, "OK") != 0){
            return -1;
        }
        
        close(sockfd);
        bzero(recvb, 100);
    }
    free(recvb);
    return 0;
}