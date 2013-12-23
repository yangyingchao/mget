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

typedef struct _sock_list sock_group;

sock_group* sock_group_create();
void sock_group_destroy(sock_group*);
void sock_add_to_group(sock_group*, msock*);
int  socket_perform(sock_group* sock);

msock* socket_get(url_info* ui, sock_read_func rf, sock_write_func wf);
void socket_put(msock* sock);


// #ifdef __cplusplus
// }
// #endif

#endif /* _MGET_SOCK_H_ */
