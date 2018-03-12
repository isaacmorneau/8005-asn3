#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <getopt.h>

#include "port_server.h"
#include "pairs.h"
#include "wrappers/wrapper.h"

#define SOCKOPTS "p:"

static inline void print_help(void){
    puts("usage options:\n"
            "\t-[-p]airs <address@inport[:outport]> - multiple flags for each forwarding pair\n"
            "\t\t-p example.com@3000:80 - redirects traffic locally from port 3000 to port 80 on example.com\n"
            "\t-[-c]onfig <default ./config> - the config for setting up ports\n"
            "\t-[-h]elp - this message");
}

int main (int argc, char *argv[]) {
    if (argc == 1) {
        print_help();
        return 0;
    }

    int c;
    pairs * pairs_list = NULL;
    char * config = 0;

    int option_index = 0;

    static struct option long_options[] = {
        {"pairs",  required_argument, 0, 'p' },
        {"config", optional_argument, 0, 'c' },
        {"help",   no_argument,       0, 'h' },
        {0,        0,                 0, 0   }
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
        case 'c':
            config = optarg;
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
    port_server(pairs_list);
    free_pairs(pairs_list);
    return 0;
}
