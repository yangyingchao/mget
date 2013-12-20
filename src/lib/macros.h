#ifndef _MACROS_H_
#define _MACROS_H_

#include <string.h>
#include <stdlib.h>

#define ZALLOC1(T) (T*)calloc(1, sizeof(T))
#define ZALLOC(T, N)       (T*)calloc(N, sizeof(T))
#define FIF(X)  if((X)) free((X))
#define FIFZ(X) if(*X) {free(*(X)), *(X) = NULL;}





#endif /* _MACROS_H_ */
