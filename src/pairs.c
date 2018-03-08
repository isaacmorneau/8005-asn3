#include "pairs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void add_pairs(pairs ** restrict head, const char * restrict arg) {
    pairs * current;
    if (*head != NULL) {
        current = *head;
        for(; current->next != NULL; current = current->next);
        current->next = calloc(1, sizeof(pairs) + strlen(arg));
        current = current->next;
    } else {
        current = calloc(1, sizeof(pairs) + strlen(arg));
        *head = current;
    }

    current->addr = ((char *)current) + sizeof(pairs);

    strcpy(current->addr, arg);
    char * temp = strchr(current->addr, '@');

    current->i_port = temp + 1;
    *temp = '\0';

    temp = strchr(current->i_port, ':');
    if (temp == NULL) {
        current->o_port = current->i_port;
    } else {
        current->o_port = temp + 1;
        *temp = '\0';
    }
    current->next = NULL;
}

void print_pairs(const pairs * head) {
    for(const pairs * current = head; current != NULL; current = current->next)
        printf("link %s -> %s at %s\n", current->i_port, current->o_port, current->addr);
}

void free_pairs(pairs * restrict head) {
    if (head->next) {
        free_pairs(head->next);
    }
    free(head);
}
