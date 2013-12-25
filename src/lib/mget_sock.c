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
    msock*           sk;
} sock_list;

struct _sock_group
{
    int        cnt;                     // count of sockets.
    bool*      cflag;                   // control flag.
    sock_list* lst;                     // list of sockets.
};

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

msock* socket_get(url_info* ui)
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
        if (connect(sk->sk.sock, sk->addr->ai_addr, sk->addr->ai_addrlen) == -1)
        {
            perror("Failed to connect");
        }

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

int socket_perform(sock_group* group)
{
    PDEBUG ("enter, sg: %p\n", group);

    int cnt = group->cnt;
    PDEBUG ("total socket: %d\n", cnt);

    if (!cnt)
        return -1;

    int epl = epoll_create(cnt);
    if (epl == -1)
    {
        perror("epool_create");
        exit(EXIT_FAILURE);
    }

    struct epoll_event* events = ZALLOC(struct epoll_event, cnt);

    //TODO: Add sockets to Epoll
    sock_list* p = group->lst;
    PDEBUG ("p: %p, next: %p\n", p, p->next);

    while (p && p->sk) {
        PDEBUG ("p: %p, next: %p\n", p, p->next);
        msock_p* sk = (msock_p*)p->sk;
        if ((sk->sk.sock != 0) && sk->sk.rf && sk->sk.wf) {
            /* fcntl(sk->sk.sock, F_SETFD, O_NONBLOCK); */
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLOUT;
            ev.data.ptr = sk;
            if (epoll_ctl(epl, EPOLL_CTL_ADD, sk->sk.sock, &ev) == -1) {
                perror("epoll_ctl: listen_sock");
                exit(EXIT_FAILURE);
            }
        }
        p = (sock_list*)p->next;
    }

    int nfds = 0;

    PDEBUG ("%p: %d\n", group->cflag, *group->cflag);
    while (!(*(group->cflag))) {
        nfds = epoll_wait(epl, events, cnt, 1000); // set timeout to 1 second.

        if (nfds == -1) {
            perror("epoll_pwait");
            break;
        }

        if (nfds == 0)
        {
            fprintf (stderr, "time out ....\n");
            continue;
        }

        for (int i = 0; i < nfds; ++i) {
            msock_p* psk = (msock_p*)events[i].data.ptr;
            assert(psk);

            int ret = 0;
            bool need_mod = false;
            if (events[i].events & EPOLLOUT) // Ready to send..
            {
                ret = psk->sk.wf(psk->sk.sock, psk->sk.priv);
                need_mod = true;
            }

            else if (events[i].events & EPOLLIN ) // Ready to read..
            {
                ret = psk->sk.rf(psk->sk.sock, psk->sk.priv);
            }

            if (ret <= 0)
            {
                PDEBUG ("remove socket...\n");
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLOUT;
                ev.data.ptr = psk;
                if (epoll_ctl(epl, EPOLL_CTL_DEL, psk->sk.sock,
                              &ev) == -1) {
                    perror("epoll_ctl: conn_sock");
                    exit(EXIT_FAILURE);
                }
                /* close(psk->sk.sock); */
                cnt --;
                PDEBUG ("remaining sockets: %d\n", cnt);

            }
            else if (need_mod)
            {
                struct epoll_event ev;
                ev.events = EPOLLIN;
                ev.data.ptr = psk;
                epoll_ctl(epl, EPOLL_CTL_MOD, psk->sk.sock, &ev);
            }
        }

        if (cnt == 0)
        {
            break;
        }

        if (*(group->cflag))
        {
            fprintf(stderr, "Stop because control_flag set to 1!!!\n");
            break;
        }
    }

    return 0;
}


sock_group* sock_group_create(bool* flag)
{
    sock_group* group = ZALLOC1(sock_group);
    group->cflag = flag;

    INIT_LIST(group->lst, sock_list);

    return group;
}

void sock_group_destroy(sock_group* group)
{
    sock_list* g = group->lst;
    while (g) {
        sock_list* ng = (sock_list*)g->next;
        FIF(SK2SKP(g->sk));
        FIF(g);
        g=ng;
    }

    FIF(group);
}

void sock_add_to_group(sock_group* group, msock* sock)
{
    group->cnt ++;
    sock_list* tail = group->lst;
    if (tail->sk)
    {
        SEEK_LIST_TAIL(group->lst, tail, sock_list);
    }
    assert(tail->sk == NULL);
    tail->sk = sock;
    PDEBUG ("Socket: %p added to group: %p, current count: %d\n",
            sock, group, group->cnt);
}
