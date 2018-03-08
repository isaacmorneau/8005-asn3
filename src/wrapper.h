#ifndef WRAPPER_H
#define WRAPPER_H

#include <stdlib.h>
#include "common.h"
#define MAXEVENTS 256
struct directional_buffer;

typedef struct directional_buffer {
    int sockfd;
    int pipefd[2];
    struct directional_buffer * paired;
} directional_buffer;

void close_directional_buffer(directional_buffer * con);
void init_directional_buffer(directional_buffer * in_con, directional_buffer * out_con, int in_fd, int out_fd);

void set_fd_limit();
void set_non_blocking(int sfd);

int make_bound(const char * port);
int make_connected(const char * address, const char * port);

int forward_read(const directional_buffer * con);
int forward_flush(const directional_buffer * con);
#endif
