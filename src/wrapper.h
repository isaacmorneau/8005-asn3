#ifndef WRAPPER_H
#define WRAPPER_H

#include <stdlib.h>
#include "common.h"
#define MAXEVENTS 256

/*
 * [inside] [forwarder] [outside]
 *      out_socket<in_socket
 *      out_socket>in_socket
 * */
typedef struct connection {
    int in_sockfd;
    int out_sockfd;
    int in_pipe[2];
    int out_pipe[2];
} connection;

typedef struct pairs {
    const char * addr;
    const char * i_port;
    const char * o_port;
} pairs;

#define init_pair(pair, addr, port) do {\
    ((pairs *)pair)->addr = addr;\
    ((pairs *)pair)->i_port = port;\
    ((pairs *)pair)->o_port = port;\
    } while(0)

#define init_pair2(pair, addr, in_port, out_port) do {\
    ((pairs *)pair)->addr = addr;\
    ((pairs *)pair)->i_port = in_port;\
    ((pairs *)pair)->o_port = out_port;\
    } while(0)

#define close_connection(con) do {\
    close(((connection*)con)->sockfd);\
    } while(0)

#define init_connection(con, in_fd, out_fd) do {\
    ensure((pipe(((connection*)con)->in_pipe) == 0);\
    ensure((pipe(((connection*)con)->out_pipe) == 0);\
    ((connection*)con)->in_sockfd = in_fd;\
    ((connection*)con)->out_sockfd = out_fd;\
    } while(0)

void set_fd_limit();
void set_non_blocking(int sfd);
void set_recv_window(int sockfd);
void enable_keepalive(int sockfd);

int make_bound(const char * port);
int make_connected(const char * address, const char * port);

int forward_in_read(const connection * con);
int forward_in_flush(const connection * con);

int forward_out_read(const connection * con);
int forward_out_flush(const connection * con);
#endif
