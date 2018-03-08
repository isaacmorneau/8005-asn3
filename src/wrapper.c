#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "wrapper.h"
#include "common.h"

void set_fd_limit() {
    struct rlimit lim;
    //the kernel patch that allows for RLIM_INFINITY to work breaks stuff
    //so we are restricted to finite values,
    //this was found as the exact max via testing
    lim.rlim_cur = (1UL << 20);
    lim.rlim_max = (1UL << 20);
    ensure(setrlimit(RLIMIT_NOFILE, &lim) != -1);
}

void enable_keepalive(int sockfd) {
    int yes = 1;
    int idle = 1;
    int interval = 1;
    int maxpkt = 10;

    ensure(setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) != -1);
    ensure(setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int)) != -1);
    ensure(setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(int)) != -1);
    ensure(setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(int)) != -1);
}

void set_recv_window(int sockfd) {
    int rcvbuf = TCP_WINDOW_CAP;
    int clamp = TCP_WINDOW_CAP/2;

    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, sizeof(rcvbuf));
    setsockopt(sockfd, SOL_SOCKET, TCP_WINDOW_CLAMP, (char *)&clamp, sizeof(clamp));
}

void set_non_blocking (int sfd) {
    int flags;
    ensure((flags = fcntl(sfd, F_GETFL, 0)) != -1);
    flags |= O_NONBLOCK;
    ensure(fcntl(sfd, F_SETFL, flags) != -1);
}

int make_connected(const char * address, const char * port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd;

    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // Return IPv4 and IPv6 choices
    hints.ai_socktype = SOCK_STREAM; // We want a TCP socket
    hints.ai_flags = AI_PASSIVE;     // All interfaces

    ensure(getaddrinfo(address, port, &hints, &result) == 0);

    for (rp = result; rp != 0; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        s = connect(sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0) {
            break;
        }

        close(sfd);
    }

    ensure(rp);

    freeaddrinfo(result);

    return sfd;
}

int make_bound(const char * port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd;

    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // Return IPv4 and IPv6 choices
    hints.ai_socktype = SOCK_STREAM; // We want a TCP socket
    hints.ai_flags = AI_PASSIVE;     // All interfaces

    ensure(getaddrinfo(0, port, &hints, &result) == 0);

    for (rp = result; rp != 0; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) {
            continue;
        }

        int enable = 1;
        ensure(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != -1);

        if (!bind(sfd, rp->ai_addr, rp->ai_addrlen)) {
            //we managed to bind successfully
            break;
        }

        close(sfd);
    }

    ensure(rp != 0);

    freeaddrinfo(result);

    return sfd;
}

int forward_read(const directional_buffer * con) {
    int total = 0, ret;
    for (;;) {
        //read new data
        ensure_nonblock((ret = splice(con->sockfd, 0, con->pipefd[1], 0, UINT_MAX, SPLICE_F_MORE | SPLICE_F_NONBLOCK)) != -1);
        if (ret == -1) {//eagain all data read
            break;
        } else if (ret == 0) {//actually means the connection was closed
            return 0;//notify caller that the connection closed
        }

        //forward the data out
        for (;;) {
            ensure_nonblock((ret = splice(con->pipefd[0], 0, con->paired->sockfd, 0, UINT_MAX, SPLICE_F_MORE | SPLICE_F_NONBLOCK)) != -1);
            if (ret == -1) break;
            total += ret;
        }
        if (ret <= 0) {
            break;
        }
    }
    return total;
}

int forward_flush(const directional_buffer * con) {
    int total = 0, ret;
    for (;;) {
        ensure_nonblock((ret = splice(con->pipefd[0], 0, con->paired->sockfd, 0, UINT_MAX, SPLICE_F_MORE | SPLICE_F_NONBLOCK)) != -1);
        if (ret == -1) break;
        total += ret;
    }
    return total;
}

void init_directional_buffer(directional_buffer * in_con, directional_buffer * out_con, int in_fd, int out_fd) {
    ensure(pipe(in_con->pipefd) == 0);
    ensure(pipe(out_con->pipefd) == 0);

    in_con->sockfd = in_fd;
    out_con->sockfd = out_fd;

    //link them
    in_con->paired = out_con;
    out_con->paired = in_con;
}

//will exit if the other side is already closed
void close_directional_buffer(directional_buffer * con) {
    if (!con->paired) {
        return;
    }
    close(con->sockfd);
    close(con->pipefd[0]);
    close(con->pipefd[1]);

    close(con->paired->sockfd);
    close(con->paired->pipefd[0]);
    close(con->paired->pipefd[1]);

    con->paired = 0;
}
