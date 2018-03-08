#ifndef PAIRS_H
#define PAIRS_H

struct pairs;
typedef struct pairs {
    char * addr;
    char * i_port;
    char * o_port;
    int sockfd;
    struct pairs * next;
} pairs;

void add_pairs(pairs ** head, const char * arg);
void print_pairs(const pairs * head);
void free_pairs(pairs * head);


#endif
