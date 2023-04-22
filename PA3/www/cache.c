#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct Cache_node {
    // Linked list
    char* key; // Encode
    char* page; // Page content
    time_t fetched; // Time since fetched
    struct Cache_node *next;
}

typedef struct Cache {
    int ttl; // Time to live
    int max_pages; // max pages in the cache
    struct Cache_node *head;
}
