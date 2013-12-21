#include "mget_sock.h"
#include <sys/types.h>
#include <sys/socket.h>
#include "typedefs.h"


msock* socket_get(const char* host, sock_read_func rf, sock_write_func wf)
{
    return NULL;
}

void socket_put(msock* sock)
{
}

int socket_perform(msock* sock)
{
    return 0;
}
