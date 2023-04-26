#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "cache.h"

char* ca_get(char* URL){
    // Attempts to fetch a page from the cache directory.
    unsigned long h = hash(URL);
    int length = snprintf( NULL, 0, "%lu", h );
    char* str = malloc( length + 1 + 8);
    snprintf( str, length + 1 + 8, ".%s/%lu", DIR, h );

    FILE* fp = fopen(str, "r");
    if ( fp != NULL){
        fclose(fp);
        return str;
    }else{
        return NULL;
    }
}

int ca_put(char* URL, char* response, int size, int filepos){
    // Save contents of response including headers.
    char fname[100];
    FILE *fp;
    unsigned long hashed = hash(URL);
    int bytes_written = 0;
    int files;
    if (dontcache(URL))
        return -1;
    // Dont save files if cache is full
    printf("caching %s\n", URL);
    printf("hash: %lu\n", hashed);
    files = getfileamt();
    if (files > CACHE_SIZE)
        return -1;
    sprintf(fname,".%s/%lu", DIR, hashed);

    // Move file pointer myself since 'a' option doesnt want to work.
    fp = fopen(fname, "a");
    //fseek(fp, filepos, SEEK_SET);
    if (fp == NULL){
        perror("fopen"); 
        return -1;
    }
    bytes_written = fwrite(response, sizeof(char), size, fp);
    printf("by_wr: %d\n", bytes_written);
    fclose(fp);
    return bytes_written;
}

int getfileamt(){
    // Gets amount of files in ./cache directory.
    FILE *fp;
    int fileamt;
    char* line;
    size_t len = 0;
    fp = popen("ls ./cache | wc -l", "r");
    getline(&line, &len, fp);
    fileamt = atoi(line);
    fclose(fp);

    return fileamt;
}

int dontcache(char* URL){
    if (strstr(URL, "?")){
        return 1;
    }else if(strstr(URL, "push.services")){
        return 1;
    }
    return 0;
}

void purgeexpired(int ttl){
    // Removed dead cache entries.
    char ls_cmd[100];
    char stat_cmd[100];
    char fpath[100];
    char* stupid_date = "date +%s";
    char* stat_txt = "stat -c %Y -- .%s/%s";
    char * line = NULL;
    char * line2 = NULL;
    size_t len = 0;
    ssize_t read;
    unsigned long curtime;
    unsigned long file_created_time;
    int lived = 0;
    FILE *ls;
    FILE *time;
    FILE *stat;

    sprintf(ls_cmd,"ls .%s", DIR);

    // Get current seconds since epoch
    time = popen(stupid_date, "r");
    getline(&line, &len, time);
    curtime = atoi(line);
    fclose(time);

    ls = popen(ls_cmd, "r");
    while ((read = getline(&line, &len, ls)) != -1) {
        // Get current seconds since epoch of files in cache.
        sprintf(stat_cmd,stat_txt,DIR,line);
        stat = popen(stat_cmd, "r");
        getline(&line2, &len, stat);
        file_created_time = atoi(line2);
        // Calculate lived time
        lived = curtime - file_created_time;
        // Get path of file
        sprintf(fpath,".%s/%s",DIR,line);
        if (lived > ttl){
            // Delete file
            if(fpath[strlen(fpath) - 1] == '\n')
                fpath[strlen(fpath) - 1] = '\0';
            printf("removed: %s\n", fpath);
            remove(fpath);
        }
        fclose(stat);
    }
    fclose(ls);
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