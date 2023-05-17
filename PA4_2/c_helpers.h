typedef struct Tuple {
    int x;
    int y;
} Tuple;

typedef struct{
    char *files;
    size_t size;
} files_struct;

struct Tuple* getidx(char* filename);
int sortstring( const void *str1, const void *str2 );
int get_fd_from_ip_port(char* IP, char* port);
unsigned long hash(char *str);
int checkfile(char* filename); // Sees if a file is present on all available servers.
int append_file(char *f1, char *f2);
int get(char* filename);
int checkservs();
int put(char* filename);
int remove_dups(int n_items, char *arr[]);
int list(char* filename);
int handlecmd(char* cmd, char* filename);
files_struct getfilelist();
