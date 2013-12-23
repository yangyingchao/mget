#ifndef _MGET_SOCK_H_
#define _MGET_SOCK_H_

// #ifdef __cplusplus
// extern "C" {
// #endif

#include "netutils.h"

typedef struct _msock msock;

typedef void* (*sock_read_func)(msock*);
typedef void* (*sock_write_func)(msock*);

struct _msock
{
    int             sock;
    sock_read_func  rf;
    sock_write_func wf;
};

msock* socket_get(url_info* ui, sock_read_func rf, sock_write_func wf);
void socket_put(msock* sock);
int  socket_perform(msock* sock);


// #ifdef __cplusplus
// }
// #endif

#endif /* _MGET_SOCK_H_ */
