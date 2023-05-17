#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c_helpers.h"
#include "util.h"

int main(int argc, char** argv){
    char *cmd, *file;
    int is_down = 0;
    if (argc < 2){
        printf("./dfc <cmd> [filename] ... [filename]\n");
        return -1;
    }

    is_down = checkservs();

    cmd = argv[1];
    if (strcasecmp(cmd, "GET") == 0){
        for (char **pargv = argv+2; *pargv != argv[argc]; pargv++){
            file = *pargv;
            if (checkfile(file) != 1){
                printf("%s is incomplete.\n", file);
                continue;
            }
            handlecmd(cmd, file);
        }
    }else if(strcasecmp(cmd, "PUT") == 0){
        for (char **pargv = argv+2; *pargv != argv[argc]; pargv++){
            file = *pargv;
            if (is_down != 0){
                printf("%s put failed.\n", file);
                continue;
            }
            handlecmd(cmd, file);
        }
    }else if (strcasecmp(cmd, "LIST") == 0){
        if (argc == 3){
            handlecmd(cmd, argv[2]);
        }else{
            handlecmd(cmd, NULL);
        }
    }
    return 0;
}