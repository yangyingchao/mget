#ifndef _MACROS_H_
#define _MACROS_H_

#include <string.h>
#include <stdlib.h>

#define ZALLOC(T, X)       (T*)calloc(1, X)
#define FIF(X)  if((X)) free((X))
#define FIFZ(X) if(*X) {free(*(X)), *(X) = NULL;}





#endif /* _MACROS_H_ */
