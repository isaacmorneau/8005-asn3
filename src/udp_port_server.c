#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>

#include "wrappers/wrapper.h"
#include "tcp_port_server.h"

void udp_port_server(pairs * restrict head) {
    int efd;
    struct epoll_event *events;

    //listening epoll
    efd = make_epoll();
    //add all the ports we are listening on
    for(pairs * current = head; current != NULL; current = current->next) {
        //make and bind the socket
        int iprt = atoi(current->i_port);
        int oprt = atoi(current->o_port);
        int ifd = make_bound_udp(iprt);
        int ofd = make_bound_udp(oprt);
        set_non_blocking(ifd);
        set_non_blocking(ofd);

        udp_buffer * in_buf = malloc(sizeof(udp_buffer));
        udp_buffer * out_buf = malloc(sizeof(udp_buffer));
        init_udp_buffer(in_buf, out_buf);

        in_buf->sockfd = ifd;
        make_storage(&in_buf->addr, current->addr, iprt);

        out_buf->sockfd = ofd;
        make_storage(&out_buf->addr, current->addr, oprt);

        add_epoll_ptr(efd, ifd, in_buf);
        add_epoll_ptr(efd, ofd, out_buf);
    }

    // Buffer where events are returned
    events = make_epoll_events();

    //threads will handle the clients, the main thread will just add new ones
    int n, i;
waiting:
    //printf("current scale: %d\n",scaleback);
    n = epoll_wait(efd, events, MAXEVENTS, -1);
    for (i = 0; i < n; i++) {
        if (EVENT_ERR(events, i)) {
            perror("epoll_wait, listen error");
        } else if (EVENT_IN(events, i)){ //EPOLLIN
            udp_buffer * buf = EVENT_PTR(events, i);
            udp_buffer_read(buf);
            udp_buffer_flush(buf);
        }
    }
    goto waiting;

    close(efd);

    free(events);
}

