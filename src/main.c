#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <getopt.h>

#include "port_server.h"
#include "common.h"
#include "wrapper.h"

#define SOCKOPTS "p:"

void print_help(void){
    puts("usage options:\n"
            "\t-[-p]airs <address@inport:[outport||same],...> - comma seperated list of addresss port pairs\n"
            "\t-[-c]onfig <default ./config> - the config for setting up ports\n"
            "\t-[-h]elp - this message");
}

int main (int argc, char *argv[]) {
    if (argc == 1) {
        print_help();
        return 0;
    }

    int c;
    char * pairs = "";
    char * config = 0;

    while (1) {
        int option_index = 0;

        static struct option long_options[] = {
            {"pairs",  required_argument, 0, 'p' },
            {"config", optional_argument, 0, 'c' },
            {"help",   no_argument,       0, 'h' },
            {0,        0,                 0, 0   }
        };

        c = getopt_long(argc, argv, SOCKOPTS, long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
            case 'p':
                pairs = optarg;
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
    }

    set_fd_limit();
    //TODO parse the args into the ports list
    pairs * pairs_list;
    int total = 0;
    port_server(pairs_list, total);
    return 0;
}
