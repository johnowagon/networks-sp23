#include <stdlib.h>
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

int hello(){
    printf("This works in both\n");
    return 0;
}

int count_lines(char* filename)
{
    FILE* file = fopen(filename, "r");
    // I got this code from https://stackoverflow.com/questions/12733105/c-function-that-counts-lines-in-file
    char buf[65536];
    int counter = 0;
    for(;;)
    {
        size_t res = fread(buf, 1, 65536, file);
        if (ferror(file))
            return -1;

        size_t i;
        for(i = 0; i < res; i++)
            if (buf[i] == '\n')
                counter++;

        if (feof(file))
            break;
    }

    fclose(file);
    return counter;
}

int getfilesize(char* filename){
    // Returns filesize in bytes
    int size;
	FILE *f;
    
	f = fopen(filename, "r");
	if (f == NULL) return -1;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);

	return size;
}