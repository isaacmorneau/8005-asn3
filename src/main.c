#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <getopt.h>

#include "tcp_port_server.h"
#include "udp_port_server.h"
#include "wrappers/wrapper.h"

#define SOCKOPTS "tup:"

static inline void print_help(void){
    puts("usage options:\n"
            "\t-[-p]airs <address@inport[:outport]> - multiple flags for each forwarding pair\n"
            "\t\t-p example.com@3000:80 - redirects traffic locally from port 3000 to port 80 on example.com\n"
            "\t-[-t]cp - switch to tcp pairs(default)\n"
            "\t-[-u]dp - switch to udp pairs\n"
            "\t-[-h]elp - this message");
}

int main (int argc, char *argv[]) {
    if (argc == 1) {
        print_help();
        return 0;
    }

    int c;
    pairs * pairs_list = NULL;
    int mode = 1;

    int option_index = 0;

    static struct option long_options[] = {
        {"pairs", required_argument, 0, 'p' },
        {"udp",   no_argument,       0, 'u' },
        {"tcp",   no_argument,       0, 't' },
        {"help",  no_argument,       0, 'h' },
        {0,       0,                 0, 0   }
    };
args:
    c = getopt_long(argc, argv, SOCKOPTS, long_options, &option_index);
    if (c == -1) {
        goto start;
    }

    switch (c) {
        case 'p':
            add_pairs(&pairs_list, optarg);
            break;
        case 't':
            mode = 1;
            break;
        case 'u':
            mode = 2;
            break;
        case 'h':
        case '?':
        default:
            print_help();
            return 0;
    }
    goto args;

start:
    if (pairs_list == NULL) {
        print_help();
        return 0;
    }

    print_pairs(pairs_list);

    //set_fd_limit();
    if (mode == 1) {
        tcp_port_server(pairs_list);
    } else {
        udp_port_server(pairs_list);
    }
    free_pairs(pairs_list);
    return 0;
}
