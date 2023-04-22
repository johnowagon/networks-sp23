#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

typedef enum clientcmd{get, put, delete, ls, Exit, error, null} clientcmd; // Available client commands 
clientcmd resolveClientCommand(char* msg); // Helper function to determine client cmd or error.
int loadCurrentDirFiles(char* buffer); // Loads the given buffer with filenames.
int getFileSizeBytes(char* filename); // Returns number of bytes in a given filename 
//after doing some error checking.
int saveFileContents(char* filename, char* buffer, long int filepos, int maxmsglength); // Loads file contents into buffer using a file position.
int writeFileContents(char* filename, char* buffer, long int filepos, int maxmsglength); // Writes file contents to a file on disk specified by filename.
void replaceTrailingNewline(char* str); // Removes a trailing newline if present on the string.
int removefile(char* filename); // Deletes a file from the filesystem.
int checksuccess(int bytes); // Checks the success of a recv or send function by examininng number of bytes,
// will print errorno and message if found.
int popendirfiles(char* buffer); // Uses popen to load directory files instead of using dirents.

clientcmd resolveClientCommand(char* msg){
    // Used to determine client command from received message.
    if (strcmp(msg, "\n") == 0){
        return null;
    }
    enum clientcmd received_message;
    static char* get_str = "get";
    static char* put_str = "put";
    static char* delete_str = "delete";
    static char* ls_str = "ls";
    static char* exit_str = "exit";
    if(strstr(msg, get_str) != NULL){
        received_message = get;
    }
    else if (strstr(msg, put_str) != NULL){
        received_message = put;
    }
    else if(strstr(msg, delete_str) != NULL){
        received_message = delete;
    }
    else if(strstr(msg, ls_str) != NULL){
        received_message = ls;
    }
    else if(strstr(msg, exit_str) != NULL){
        received_message = Exit;
    }else{
        received_message = error;
    }
    return received_message;
}

int loadCurrentDirFiles(char* buffer){
    // Loads buffer with the current directory filenames.
    // Returns the number of bytes written to the buffer.
    // Note: will clear out buffer before filling.
    // code inspired by https://www.geeksforgeeks.org/c-program-list-files-sub-directories-directory/
    
    struct dirent *de;  // Pointer for directory entry
    int bytes_loaded = 0; // Tracking number of bytes loaded
  
    // opendir() returns a pointer of DIR type. 
    DIR *dr = opendir(".");
  
    if (dr == NULL)  // opendir returns NULL if couldn't open directory
    {
        printf("Could not open current directory");
        return 0;
    }

    memset(buffer, '\0', sizeof(buffer)); // Clear contents of buffer.
    for (;;){
        if((de = readdir(dr)) != NULL){
            bytes_loaded += sprintf(buffer+bytes_loaded, "%s ", de->d_name);
        }else{
            break;
        }
    }
    
    closedir(dr);
    return bytes_loaded;
}

int getFileSizeBytes(char* filename){
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

int writeFileContents(char* filename, char* buffer, long int filepos, int maxmsglength){
    // Writes contents of buffer into a file specified by filename.
    // Returns: position of writing in file.
    size_t newpos; // Position of fp
    int write_size = (maxmsglength >= strlen(buffer)) ? strlen(buffer) : maxmsglength; // Choose write size based on which is longer
    replaceTrailingNewline(filename);
    FILE *fp = fopen(filename, "a+");
    if (fp != NULL) {
        fseek(fp, filepos, SEEK_SET);
        newpos = fwrite(buffer, sizeof(char), maxmsglength, fp); // Read maxmsglength bytes from the file
        if ( ferror( fp ) != 0 ) {
            fputs("Error reading file", stderr);
        }
        fclose(fp);
    }else{
        printf("writefilecontents error\n");
        printf("fname: '%s'\n", filename);
        printf("filepos: %ld\n", filepos);
        printf("write size: %d", maxmsglength);
        return 0;
    }
    return newpos;
}

int removefile(char* filename){
    if (remove(filename) == 0)
        return 0;
    else
        return 1;
  
   return 1;
}

void replaceTrailingNewline(char* str){
    // Replaces trailing newline on str if present.
    int len = strlen(str);
    if(str[len-1] == '\n'){
        str[len-1] = 0;
    }
}

int checksuccess(int bytes){
    // Checks the success of a recv or send function.
    if(bytes < 0){
        if(errno == EAGAIN || errno == EWOULDBLOCK){
            printf("Socket timeout.\n");
            return 1;
        }
    }else if(bytes == 0){
        printf("No bytes received.\n");
        return 1;
    }else{
        return 0;
    }
    return 1;
}
