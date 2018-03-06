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

#include "wrapper.h"
#include "common.h"
#include "logging.h"
#include "epoll_server.h"

static pthread_cond_t * thread_cvs;
static pthread_mutex_t * thread_mts;
int * epollfds;

static volatile int running = 1;
static void handler(int sig) {
    running = 0;
}

void * epoll_handler(void * pass_pos) {
    int pos = *((int*)pass_pos);
    int efd = epollfds[pos];
    struct epoll_event *events;
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(pos, &cpuset);

    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    // Buffer where events are returned
    events = calloc(MAXEVENTS, sizeof(struct epoll_event));

    pthread_cond_wait(&thread_cvs[pos], &thread_mts[pos]);

    while (running) {
        int n, i;
        n = epoll_wait(efd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                // A socket got closed
                close_connection(events[i].data.ptr);
                continue;
            } else {
                if((events[i].events & EPOLLIN)) {
                    //regular incomming message echo it back
                    echo((connection *)events[i].data.ptr);
                }

                if((events[i].events & EPOLLOUT)) {
                    //we are now notified that we can send the rest of the data
                    //possible but unlikely, handle it anyway
                    echo_harder((connection *)events[i].data.ptr);
                }
            }
        }
    }
    free(events);
    free(pass_pos);

    return 0;
}

void epoll_server(const char * port, bool m) {
    int sfd;
    int total_threads = get_nprocs();
    epollfds = calloc(total_threads, sizeof(int));
    thread_cvs = calloc(total_threads, sizeof(pthread_cond_t));
    thread_mts = calloc(total_threads, sizeof(pthread_mutex_t));
    int efd;
    connection * con;
    struct epoll_event event;
    struct epoll_event *events;
    int epoll_pos = 0;

    //signal(SIGINT, handler);

    //make the epolls for the threads
    //then pass them to each of the threads
    for (int i = 0; i < total_threads; ++i) {
        ensure((epollfds[i] = epoll_create1(0)) != -1);
        pthread_cond_init(&thread_cvs[i], NULL);
        pthread_mutex_init(&thread_mts[i], NULL);

        pthread_attr_t attr;
        pthread_t tid;

        ensure(pthread_attr_init(&attr) == 0);
        int * thread_num = malloc(sizeof(int));
        *thread_num = i;
        ensure(pthread_create(&tid, &attr, &epoll_handler, (void *)thread_num) == 0);
        ensure(pthread_attr_destroy(&attr) == 0);
        ensure(pthread_detach(tid) == 0);//be free!!
        printf("thread %d on epoll fd %d\n", i, epollfds[i]);
    }

    //make and bind the socket
    sfd = make_bound(port);
    set_non_blocking(sfd);

    ensure(listen(sfd, SOMAXCONN) != -1);

    //listening epoll
    ensure((efd = epoll_create1(0)) != -1);

    con = (connection *)calloc(1, sizeof(connection));
    init_connection(con, sfd);
    event.data.ptr = con;

    event.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ensure(epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event) != -1);

    // Buffer where events are returned
    events = calloc(MAXEVENTS, sizeof(event));

    //threads will handle the clients, the main thread will just add new ones
    while (running) {
        int n, i;
        //printf("current scale: %d\n",scaleback);
        n = epoll_wait(efd, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++) {
            if (events[i].events & EPOLLERR) {
                perror("epoll_wait, listen error");
            } else if (events[i].events & EPOLLHUP) {
                close_connection(events[i].data.ptr);
            } else { //EPOLLIN
                while (1) {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd;

                    in_len = sizeof(in_addr);

                    infd = laccept(sfd, &in_addr, &in_len);
                    if (infd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    // Make the incoming socket non-blocking and add it to the
                    // list of fds to monitor.
                    set_non_blocking(infd);
                    //enable_keepalive(infd);
                    set_recv_window(infd);

                    ensure(con = calloc(1, sizeof(connection)));
                    init_connection(con, infd);

                    event.data.ptr = con;

                    //round robin client addition
                    event.events = EPOLLET | EPOLLEXCLUSIVE | (!m||epoll_pos<1024?EPOLLIN | EPOLLOUT:0);
                    ensure(epoll_ctl(epollfds[epoll_pos % total_threads], EPOLL_CTL_ADD, infd, &event) != -1);
                    if (epoll_pos < total_threads) {
                        pthread_cond_signal(&thread_cvs[epoll_pos]);
                    }
                    ++epoll_pos;
                }
            }
        }
    }

    //cleanup all fds and memory
    for (int i = 0; i < total_threads; ++total_threads) {
        close(epollfds[i]);
    }
    close(efd);
    close(sfd);

    free(epollfds);
    free(events);
    free(con);
}

