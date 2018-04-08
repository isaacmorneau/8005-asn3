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

void * port_handler(void * pass_pos) {
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

void tcp_port_server(pairs * restrict head) {
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
        ensure(pthread_create(&tid, &attr, &port_handler, (void *)thread_num) == 0);
        ensure(pthread_attr_destroy(&attr) == 0);
        ensure(pthread_detach(tid) == 0);//be free!!
        printf("worker thread %d started\n", i);
    }

    //listening epoll
    efd = make_epoll();
    //add all the ports we are listening on
    for(pairs * current = head; current != NULL; current = current->next) {
        //make and bind the socket
        int sfd = make_bound_tcp(current->i_port);
        set_non_blocking(sfd);

        set_listening(sfd);

        current->data.fd = sfd;
        add_epoll_ptr(efd, sfd, current);
    }

    // Buffer where events are returned
    events = make_epoll_events();

    directional_buffer * inbuff, * outbuff;
    //threads will handle the clients, the main thread will just add new ones
    int n, i;
listening:
    //printf("current scale: %d\n",scaleback);
    n = epoll_wait(efd, events, MAXEVENTS, -1);
    for (i = 0; i < n; i++) {
        if (EVENT_ERR(events, i)) {
            perror("epoll_wait, listen error");
        } else if (EVENT_HUP(events, i)) {
            close(((pairs *)EVENT_PTR(events, i))->data.fd);
        } else if (EVENT_IN(events, i)){ //EPOLLIN
            struct sockaddr in_addr;
            socklen_t in_len = sizeof(in_addr);
            int infd, outfd;
            const pairs * pair = (const pairs *)EVENT_PTR(events, i);
accepting:
            infd = accept(pair->data.fd, &in_addr, &in_len);
            if (infd == -1) {
                if (errno != EAGAIN) {
                    perror("accept");
                }
                goto listening;
            }

            // Make the incoming socket non-blocking and add it to the
            // list of fds to monitor.
            set_non_blocking(infd);
            outfd = make_connected(pair->addr, pair->o_port);
            set_non_blocking(outfd);

            ensure(inbuff = calloc(1, sizeof(directional_buffer)));
            ensure(outbuff = calloc(1, sizeof(directional_buffer)));

            init_directional_buffer(inbuff, outbuff, infd, outfd);

            //round robin client addition
            add_epoll_ptr(epollfds[epoll_pos % total_threads], infd, inbuff);
            add_epoll_ptr(epollfds[epoll_pos % total_threads], outfd, outbuff);

            if (epoll_pos < total_threads) {
                pthread_cond_signal(&thread_cvs[epoll_pos]);
            }
            ++epoll_pos;
            goto accepting;
        }
    }
    goto listening;

    //cleanup all fds and memory
    for (int i = 0; i < total_threads; ++total_threads) {
        close(epollfds[i]);
    }
    close(efd);

    free(epollfds);
    free(thread_cvs);
    free(thread_mts);

    free(events);
    free(inbuff);
    free(outbuff);
}

