#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define TCP_WINDOW_CAP 4096

#ifdef NDEBUG
#define ensure(expr) (expr)
#define ensure_nonblock(expr) (expr)
#else
#define ensure(expr)\
    do {\
        if (!(expr)) {\
            fprintf(stderr, "%s::%s::%d\n\t", __FILE__, __FUNCTION__, __LINE__);\
            perror(#expr);\
            exit(1);\
        }\
    } while(0)

#define ensure_nonblock(expr)\
    do {\
        if (!(expr) && errno != EAGAIN && errno != EWOULDBLOCK) {\
            fprintf(stderr, "%s::%s::%d\n\t", __FILE__, __FUNCTION__, __LINE__);\
            perror(#expr);\
            exit(1);\
        }\
    } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif
