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

static pthread_cond_t * thread_cvs;
static pthread_mutex_t * thread_mts;
int * epollfds;

void * sock_handler(void * pass_pos) {
    int pos = *((int*)pass_pos);
    int efd = epollfds[pos];
    struct epoll_event *events;
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(pos, &cpuset);

    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    // Buffer where events are returned
    events = make_epoll_events();

    pthread_cond_wait(&thread_cvs[pos], &thread_mts[pos]);

    int n, i;

waiting:
    n = epoll_wait(efd, events, MAXEVENTS, -1);
    for (i = 0; i < n; i++) {
        if (EVENT_ERR(events, i) || EVENT_HUP(events, i)) {
            // A socket got closed
            close_directional_buffer(EVENT_PTR(events, i));
            goto waiting;
        } else {
            if(EVENT_IN(events, i)) {
                //regular incomming message echo it back
                forward_read(EVENT_PTR(events, i));
            }
            if(EVENT_OUT(events, i)) {
                //we are now notified that we can send the rest of the data
                //possible but unlikely, handle it anyway
                forward_flush(EVENT_PTR(events, i));
            }
        }
    }
    goto waiting;

    free(events);
    free(pass_pos);
    return 0;
}

void udp_port_server(pairs * restrict head) {
    int efd;
    int total_threads = get_nprocs();
    epollfds = calloc(total_threads, sizeof(int));
    thread_cvs = calloc(total_threads, sizeof(pthread_cond_t));
    thread_mts = calloc(total_threads, sizeof(pthread_mutex_t));
    struct epoll_event *events;
    int epoll_pos = 0;

    //make the epolls for the threads
    //then pass them to each of the threads
    for (int i = 0; i < total_threads; ++i) {
        epollfds[i] = make_epoll();
        pthread_cond_init(&thread_cvs[i], NULL);
        pthread_mutex_init(&thread_mts[i], NULL);

        pthread_attr_t attr;
        pthread_t tid;

        ensure(pthread_attr_init(&attr) == 0);
        int * thread_num = malloc(sizeof(int));
        *thread_num = i;
        ensure(pthread_create(&tid, &attr, &sock_handler, (void *)thread_num) == 0);
        ensure(pthread_attr_destroy(&attr) == 0);
        ensure(pthread_detach(tid) == 0);//be free!!
        printf("worker thread %d started\n", i);
    }

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

    //cleanup all fds and memory
    for (int i = 0; i < total_threads; ++total_threads) {
        close(epollfds[i]);
    }
    close(efd);

    free(epollfds);
    free(thread_cvs);
    free(thread_mts);

    free(events);
}

