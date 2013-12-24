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
#include <sys/epoll.h>
#include <fcntl.h>

typedef struct addrinfo address;


typedef enum _sock_feature
{
    sf_keep_alive = 1,
} sock_feature;


typedef struct _addr_entry
{
    address* addr;  // don't release ti.
    address* infos; // should be freed.
    uint32   feature;
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

typedef struct _sock_list
{
    mget_slist_head* next;
    union  {
        msock* sk;
        int cnt;
    } un;
} sock_list;


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
    hints.ai_family   = AF_INET;//AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = 0;
    hints.ai_protocol = 0;
    int ret = getaddrinfo(ui->host, ui->sport, &hints, &addr->infos);
    if (ret)
        goto err;
    address* rp = NULL;
    for (rp = addr->infos; rp != NULL; rp = rp->ai_next)
    {
        sk->sk.sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sk->sk.sock == -1)
        {
            continue;
        }
        if (connect(sk->sk.sock, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            sk->connected = true;
            break;
        }
        close(sk->sk.sock);
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
    FIF(SK2SKP(sock));
}

int socket_perform(sock_group* sock)
{
    int cnt = sock->un.cnt;
    if (!cnt)
        return -1;

    int epl = epoll_create(cnt);
    if (epl == -1)
    {
        perror("epool_create");
        exit(EXIT_FAILURE);
    }

    //TODO: Add sockets to Epoll
    sock_list* p = sock;
    while (p && p->un.sk) {
        msock_p* sk = (msock_p*)p->un.sk;
        if (sk->sk.sock != 0){
            fcntl(sk->sk.sock, F_SETFD, O_NONBLOCK);
        }
        p = (sock_group*)p->next;
    }
    return 0;
}


sock_group* sock_group_create()
{
    sock_group* group = NULL;
    INIT_LIST(group, sock_group);
    group->un.cnt = 0;
    return group;
}

void sock_group_destroy(sock_group* group)
{
    sock_list* g = group;
    while (g) {
        sock_list* ng = (sock_list*)g->next;
        FIF(SK2SKP(g->un.sk));
        FIF(g);
        g=ng;
    }
}

void sock_add_to_group(sock_group* group, msock* sock)
{
    sock_group* tail = group;
    SEEK_LIST_TAIL(group, tail, sock_group);
    tail->un.sk = sock;
    group->un.cnt ++;
}
