#include "mget_sock.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "typedefs.h"
#include "data_utlis.h"
#include "debug.h"
#include <unistd.h>

typedef struct _msock_list
{
    mget_slist_head* next;

    msock* sock;
    bool   busy;
    uint32 atime;
} msock_list;

typedef struct addrinfo address;

typedef struct _addr_entry
{
    address* addr;  // don't release ti.
    address* infos; // should be freed.
} addr_entry;

typedef struct _msock_p
{
    msock sk;

    char*  host;
    address* addr;
    bool connected;
    /* bool   busy; */
    /* uint32 atime; */
} msock_p;

#define SK2SKP(X)       (msock_p*)(X)

void addr_entry_destroy(void* entry)
{
    addr_entry* e = (addr_entry*) entry;
    if (e)
    {
        FIF(e->infos);
        free(e);
    }
}


static hash_table* g_addr_cache = NULL;

msock* socket_get(url_info* ui, sock_read_func rf, sock_write_func wf)
{
    if (!g_addr_cache)
    {
        g_addr_cache = hash_table_create(64, addr_entry_destroy); //TODO: add deallocation..
        assert(g_addr_cache);
    }

    msock_p* sk = ZALLOC1(msock_p);
    if (!ui || !ui->host)
    {
        goto ret;
    }

    addr_entry* addr = GET_HASH_ENTRY(addr_entry, g_addr_cache, ui->host);
    if (addr)
    {
        sk->addr = addr->addr;
        sk->sk.sock = socket(sk->addr->ai_family, sk->addr->ai_socktype,
                             sk->addr->ai_protocol);
        goto ret;
    }

    addr = ZALLOC1(addr_entry);
    address hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = 0;
    hints.ai_protocol = 0;
    int ret = getaddrinfo(ui->host, ui->sport, &hints, &addr->infos);
    if (ret)
        goto err;
    int sfd = -1;
    address* rp = NULL;
    for (rp = addr->infos; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
        {
            continue;
        }
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            sk->connected = true;
            break;
        }
        close(sfd);
    }

    if (rp != NULL)
    {
        addr->addr = rp;
        if (!hash_table_insert(g_addr_cache, ui->host, addr))
        {
            addr_entry_destroy(addr);
            fprintf(stderr, "Failed to insert cache: %s\n", ui->host);
        }
        goto ret;
    }

err:
    fprintf(stderr, "Failed to get proper host address for: %s\n",
            ui->host);
    FIF(addr);
    FIF(sk);
    sk = NULL;
ret:
    return (msock*)sk;
}

void socket_put(msock* sock)
{
}

int socket_perform(msock* sock)
{
    return 0;
}
