#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#define DEBUG

#ifdef DEBUG
#define LOG(fd, ...)                                                                               \
    do                                                                                             \
    {                                                                                              \
        fprintf((fd), "[LOG] ");                                                                   \
        fprintf((fd), __VA_ARGS__);                                                                \
        fprintf((fd), "\n");                                                                       \
    } while (0)
#else
#define LOG(fd, ...) ((void) 0)
#endif

#endif
