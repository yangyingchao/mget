#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>
#include <string.h>

#define PDEBUG(fmt, args...)                                            \
    do {                                                                \
        const char* file = __FILE__;                                    \
        const char* ptr = file;                                         \
        const char* sep = "/";                                          \
        while ((ptr = strstr(file, sep)) != NULL) {                     \
            file = ptr;                                                 \
            file = ++ptr;                                               \
        }                                                               \
                                                                        \
        fprintf(stderr, "TDEBUG: - %s(%d)-%s: ",file, __LINE__,__FUNCTION__); \
        fprintf(stderr, fmt, ##args);                                   \
        } while(0)

#endif /* _DEBUG_H_ */

