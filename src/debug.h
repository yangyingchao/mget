#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>

#define PDEBUG(fmt, args...)                                            \
    do {                                                                \
        char msg[256] = {'\0'};                                         \
        const char* file = __FILE__;                                    \
        const char* ptr = file;                                         \
        const char* sep = "/";                                          \
        while ((ptr = strstr(file, sep)) != NULL) {                     \
            file = ptr;                                                 \
            file = ++ptr;                                               \
        }                                                               \
                                                                        \
        sprintf(msg, "TDEBUG:  - %s(%d)-%s\t: ",file, __LINE__,__FUNCTION__); \
        fprintf(stderr, strcat(msg, fmt ), ##args);                     \
        } while(0)

#endif /* _DEBUG_H_ */
