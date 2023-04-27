#define CACHE_SIZE 25
#define DIR "/cache"

void purgeexpired(int ttl);
char* ca_get(char* URL);
int ca_put(char* URL, char* response, int size, int filepos);
unsigned long hash(char *str); // djb2 hash function
int getfileamt();
int dontcache(char* URL);
