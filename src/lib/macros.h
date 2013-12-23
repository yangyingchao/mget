#ifndef _MACROS_H_
#define _MACROS_H_

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>

#define ZALLOC(T, N)       (T*)calloc(N, sizeof(T))
#define ZALLOC1(T) ZALLOC(T, 1)
#define FIF(X)  if((X)) free((X))
#define FIFZ(X) if(*X) {free(*(X)), *(X) = NULL;}





#endif /* _MACROS_H_ */
