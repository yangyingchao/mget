#ifndef _MGET_SOCK_H_
#define _MGET_SOCK_H_

// #ifdef __cplusplus
// extern "C" {
// #endif

#include "netutils.h"

typedef struct _msock msock;

typedef int (*sock_read_func)(int, void*);
typedef int (*sock_write_func)(int, void*);

// TODO: Hide sock and function pointers..
struct _msock
{
    int             sock;
    sock_read_func  rf;
    sock_write_func wf;
    void*           priv;
};

typedef struct _sock_group sock_group;

sock_group* sock_group_create(bool* flag);
void sock_group_destroy(sock_group*);
void sock_add_to_group(sock_group*, msock*);
int  socket_perform(sock_group* sock);

msock* socket_get(url_info* ui);
void socket_put(msock* sock);


// #ifdef __cplusplus
// }
// #endif

#endif /* _MGET_SOCK_H_ */
