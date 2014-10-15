/** connection.c --- implementation of connection stuffs.
 *
 * Copyright (C) 2013 Yang,Ying-chao
 *
 * Author: Yang,Ying-chao <yangyingchao@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>

#include "connection.h"
#include "data_utlis.h"
#include "log.h"
#include "mget_config.h"
#include "mget_types.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>        /* For mode constants */
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_GNUTLS
#include "plugin/ssl/ssl.h"
#endif

#define MAX_CONNS_PER_HOST  32

typedef struct addrinfo address;

typedef enum _connection_feature {
    sf_keep_alive = 1,
} connection_feature;

typedef struct _addr_entry {
    int      size;                     /* total size of this entry... */
    address *addr;                     /* addrinfo pointer. */

    int ai_family;                      /* Protocol family for socket.  */
    int ai_socktype;                    /* Socket type.  */
    int ai_protocol;                    /* Protocol for socket.  */

    socklen_t ai_addrlen;               /* Length of socket address.  */
    char      buffer[0];                /* Buffer of sockaddr.  */
} addr_entry;

#define ALLOC_ADDR_ENTRY(X)       \
    (addr_entry*)XALLOC(sizeof(addr_entry) + (X))

#ifdef DEBUG
#define OUT_ADDR(X) fprintf(stderr, "entry->" #X ": %d\n", entry->ai_##X)
#else
#define OUT_ADDR(X)
#endif

static address* addrentry_to_address(addr_entry* entry)
{
    address* addr = NULL;
    if (entry && (addr = ZALLOC1(address)))
    {
        addr->ai_family    = entry->ai_family;
        addr->ai_socktype  = entry->ai_socktype;
        addr->ai_protocol  = entry->ai_protocol;
        addr->ai_addrlen   = entry->ai_addrlen;
        addr->ai_addr = ZALLOC1(struct sockaddr);

        memcpy(addr->ai_addr, entry->buffer, addr->ai_addrlen);
        entry->addr = addr;

        OUT_ADDR(family);
        OUT_ADDR(socktype);
        OUT_ADDR(protocol);
        OUT_BIN(addr->ai_addr, addr->ai_addrlen);
    }
    return addr;
}

static addr_entry* address_to_addrentry(address* addr)
{
    addr_entry* entry = NULL;
    if (addr && (entry = ALLOC_ADDR_ENTRY(addr->ai_addrlen))) {
        entry->size        = sizeof(*entry) + addr->ai_addrlen;
        entry->ai_family   = addr->ai_family;
        entry->ai_socktype = addr->ai_socktype;
        entry->ai_protocol = addr->ai_protocol;
        entry->ai_addrlen  = addr->ai_addrlen;
        memcpy(entry->buffer, addr->ai_addr, addr->ai_addrlen);

        OUT_ADDR(family);
        OUT_ADDR(socktype);
        OUT_ADDR(protocol);
        OUT_BIN(entry->buffer, entry->ai_addrlen);

        // update entry->addr to itself.
        void* ptr = addrentry_to_address(entry);
        PDEBUG ("ptr: %p -- %p\n", entry->addr, ptr);
    }

    return entry;
}

typedef struct _connection_p {
    connection  conn;
    slist_head  lst;
    int         sock;
    int         port;
    char*       host;
    address*    addr;
    void*       priv;
    bool        connected;
    bool        active;
    bool        busy;
} connection_p;

#define CONN2CONNP(X) (connection_p*)(X)
#define LIST2PCONN(X) (connection_p*)((char*)X - offsetof(connection_p, lst))

struct _connection_group {
    int         cnt;                    // count of sockets.
    bool*       cflag;                  // control flag.
    slist_head* lst;                    // list of sockets.
};

void addr_entry_destroy(void *entry)
{
    addr_entry *e = (addr_entry *) entry;

    if (e) {
        free(e);
    }
}

#define SHM_LENGTH       4096

typedef struct _shm_region
{
    int len;
    bool busy; // Race condition: this may be used by multiple instances
    char buf[SHM_LENGTH];
} shm_region;

static uint32 tcp_connection_read(connection * conn, char *buf,
                                  uint32 size, void *priv)
{
    connection_p *pconn = (connection_p *) conn;

    if (pconn && pconn->sock && buf) {
        uint32 rd = (uint32) read(pconn->sock, buf, size);
        if (rd == -1) {
            PDEBUG ("rd: %d, errno: %d\n", rd, errno);

            if (errno == EAGAIN) { // nothing to read, normal if non-blocking
                ;
            }
            else  {
                PDEBUG ("Read connection: %p returns -1.\n", pconn);
            }
        }
        else if (!rd)  {
            /* PDEBUG ("Read connection: %p, sock: %d returns 0, connection closed...\n", */
            /*         pconn, pconn->sock); */
            /* PDEBUG ("Set %p disconnected\n", pconn); */

            /* pconn->connected = false; */
            ;
        }

        return rd;
    }

    return 0;

}

static uint32 tcp_connection_write(connection * conn, char *buf,
                                   uint32 size, void *priv)
{
    connection_p *pconn = (connection_p *) conn;

    if (pconn && pconn->sock && buf) {
        PDEBUG ("begin write, conn: %p, sock: %d ....\n",
                pconn, pconn->sock);
        return (uint32) write(pconn->sock, buf, size);
    }
    return 0;
}

static void tcp_connection_close(connection * conn, char *buf,
                                 uint32 size, void *priv)
{
}

#ifdef HAVE_GNUTLS
static uint32 secure_connection_read(connection * conn, char *buf,
                                     uint32 size, void *priv)
{
    connection_p *pconn = (connection_p *) conn;

    if (pconn && pconn->sock && buf) {
        return secure_socket_read(pconn->sock, buf, size, pconn->priv);
    }
    return 0;
}

static uint32 secure_connection_write(connection * conn, char *buf,
                                      uint32 size, void *priv)
{
    connection_p *pconn = (connection_p *) conn;

    if (pconn && pconn->sock && buf) {
        return secure_socket_write(pconn->sock, buf, size, pconn->priv);
    }
    return 0;
}
#endif


static socklen_t
sockaddr_size (const struct sockaddr *sa)
{
    switch (sa->sa_family)
    {
        case AF_INET:
            return sizeof (struct sockaddr_in);
#ifdef ENABLE_IPV6
        case AF_INET6:
            return sizeof (struct sockaddr_in6);
#endif
        default:
            abort ();
    }
}

static void
sockaddr_set_data (struct sockaddr *sa, ip_address* ip, int port)
{
    switch (ip->family)
    {
        case AF_INET:
        {
            struct sockaddr_in *sin = (struct sockaddr_in *)sa;
            XZERO (*sin);
            sin->sin_family = PF_INET;
            sin->sin_port = htons (21);
            sin->sin_addr = ip->data.d4;
            break;
        }
#ifdef ENABLE_IPV6
        case AF_INET6:
        {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
            xzero (*sin6);
            sin6->sin6_family = AF_INET6;
            sin6->sin6_port = htons (port);
            sin6->sin6_addr = ip->data.d6;
#ifdef HAVE_SOCKADDR_IN6_SCOPE_ID
            sin6->sin6_scope_id = ip->ipv6_scope;
#endif
            break;
        }
#endif /* ENABLE_IPV6 */
        default:
            abort ();
    }
}

static hash_table* g_conn_cache = NULL;
static byte_queue* dq = NULL; // drop queue

typedef struct _connection_cache
{
    int count;
    slist_head* lst;
} ccache;


#define print_address(X)   inet_ntoa ((X)->data.d4)


/* Return true iff the connection to the remote site established
   through SOCK is still open.

   Specifically, this function returns true if SOCK is not ready for
   reading.  This is because, when the connection closes, the socket
   is ready for reading because EOF is about to be delivered.  A side
   effect of this method is that sockets that have pending data are
   considered non-open.  This is actually a good thing for callers of
   this function, where such pending data can only be unwanted
   leftover from a previous request.  */

bool
validate_connection (connection_p* pconn)
{
    fd_set check_set;
    struct timeval to;
    int ret = 0;

    /* Check if we still have a valid (non-EOF) connection.  From Andrew
     * Maholski's code in the Unix Socket FAQ.  */

    FD_ZERO (&check_set);
    FD_SET (pconn->sock, &check_set);

    /* Wait one microsecond */
    to.tv_sec = 0;
    to.tv_usec = 1;

    ret = select (pconn->sock + 1, &check_set, NULL, NULL, &to);
    if ( !ret )
        /* We got a timeout, it means we're still connected. */
        return true;
    else
        /* Read now would not wait, it means we have either pending data
           or EOF/error. */
        return false;
}

connection* connection_get(const url_info* ui)
{
    PDEBUG ("%p -- %p\n", ui, ui->addr);
    if (!ui || (!ui->host && !ui->addr)) {
        PDEBUG ("invalid ui.\n");
        goto ret;
    }

    if (!dq) // init this drop queue.
    {
        dq = bq_init(1024);
    }

    if (!g_conn_cache)
    {
        g_conn_cache = hash_table_create(64, free);
        assert(g_conn_cache);
    }

    connection_p* conn = NULL;
    addr_entry *entry = NULL;
    if (ui->addr)
    {
        conn = ZALLOC1(connection_p);
        conn->sock = socket(AF_INET, SOCK_STREAM, 0);
        if (conn->sock == -1) {
            goto err;
        }

        struct sockaddr_in sa;
        if ((conn->sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket");
            goto err;
        }

        bzero(&sa, sizeof sa);

        sa.sin_family = AF_INET;
        sa.sin_port = htons(ui->port);
        sa.sin_addr = ui->addr->data.d4;
        DEBUGP (("trying to connect to %s port %u\n",
                 print_address (ui->addr), ui->port));
        if (connect(conn->sock, (struct sockaddr *)&sa, sizeof sa) < 0) {
            perror("connect");
            close(conn->sock);
            goto err;
        }
    }
    else
    {
        char* host_key = NULL;
        int ret = asprintf(&host_key, "%s:%u", ui->host, ui->port);
        if (!ret) {
            goto alloc;
        }

        ccache* cache = HASH_ENTRY_GET(ccache, g_conn_cache, host_key);
        FIF(host_key);
        PDEBUG ("cache: %p, count: %d, lst: %p\n", cache,
                cache ? cache->count : 0, cache ? cache->lst:NULL);

        if (cache && cache->count && cache->lst) {
            conn = LIST2PCONN(cache->lst);
            cache->lst = cache->lst->next;
            cache->count--;
            conn->lst.next = NULL;
            if (!validate_connection(conn)) {
                goto connect;
            }
            PDEBUG ( "\nRusing connection: %p\n", conn);
            goto post_connected;
        }

  alloc:
        conn = ZALLOC1(connection_p);
        static hash_table* addr_cache = NULL;
        static shm_region* shm_rptr   = NULL;
        if (!addr_cache) {
#define handle_error(msg)                                           \
            do { perror(msg); goto alloc_addr_cache; } while (0)

            //@todo: should check if shm is supported..
            char key[64] = {'\0'};
            sprintf(key, "/libmget_%s_uid_%d", VERSION_STRING, getuid());
            int fd = shm_open(key, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
            if (fd == -1)
                handle_error("Failed to open shared memory");

            if (ftruncate(fd, sizeof(shm_region)) == -1)
                handle_error("Failed to truncate..");

            /* Map shared memory object */
            shm_rptr = (shm_region*)mmap(NULL, sizeof(shm_region),
                                         PROT_READ | PROT_WRITE, MAP_SHARED,
                                         fd, 0);
            if (shm_rptr == MAP_FAILED)
                handle_error("do mmap");

            addr_cache = hash_table_create_from_buffer(shm_rptr->buf, SHM_LENGTH);
            if (!addr_cache) {
          alloc_addr_cache: ;
                addr_cache = hash_table_create(64, addr_entry_destroy);
                assert(addr_cache);
            }
        }

        entry = GET_HASH_ENTRY(addr_entry, addr_cache, ui->host);
        if (entry) {
            logprintf(LOG_ALWAYS, "Using cached adress ...\n");
            conn->addr = addrentry_to_address(entry);
      connect:
            PDEBUG ("Connecting to: %s:%d\n", ui->host, ui->port);

            conn->sock = socket(conn->addr->ai_family,
                                conn->addr->ai_socktype,
                                conn->addr->ai_protocol);
            if (connect (conn->sock, conn->addr->ai_addr,
                         conn->addr->ai_addrlen) == -1) {
                perror("Failed to connect");
                goto hint;
            }

            PDEBUG ("conn: %p, sock: %d\n", conn, conn->sock);

        } else {
      hint:;
            address hints;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;	//AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = 0;
            hints.ai_protocol = 0;
            logprintf(LOG_ALWAYS, "Resolving host: %s ...\n", ui->host);
            struct addrinfo* infos = NULL;
            int ret = getaddrinfo(ui->host, ui->sport, &hints, &infos);

            PDEBUG ("ret = %d, error: %s\n", ret, strerror(errno));

            if (ret)
                goto err;
            address *rp = NULL;

            for (rp = infos; rp != NULL; rp = rp->ai_next) {
                conn->sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (conn->sock == -1) {
                    continue;
                }
                if (connect(conn->sock, rp->ai_addr, rp->ai_addrlen) != -1) {
                    conn->connected = true;
                    break;
                }
                close(conn->sock);
            }

            if (rp != NULL) {
                entry = address_to_addrentry(rp);
                conn->addr = entry->addr;
                if (!hash_table_update(addr_cache, (char*)ui->host, entry,
                                       entry->size)) {
                    addr_entry_destroy(entry);
                    fprintf(stderr, "Failed to insert cache: %s\n", ui->host);
                }
                else {
                    // Only update cache to shm when it is not used by others.
                    // This is just for optimization, and it does not hurt
                    // much if one or two cache is missing...
                    if (shm_rptr && shm_rptr != MAP_FAILED && !shm_rptr->busy) {
                        shm_rptr->busy = true;
                        shm_rptr->len = dump_hash_table(addr_cache,
                                                        shm_rptr->buf,
                                                        SHM_LENGTH);
                        shm_rptr->busy = false;
                    }
                }
            }
        }
    }

post_connected: ;
    if (conn) {
        if (ui->host && !conn->host)
        {
            conn->host = strdup(ui->host);
        }

        PDEBUG ("sock(%d) %p connected to %s. \n", conn->sock, conn, conn->host);

        conn->connected = true;
        conn->port = ui->port;
        conn->active = true;
        switch (ui->eprotocol) {
            case UP_HTTPS:
            {
#ifdef HAVE_GNUTLS
                ssl_init();
                if ((conn->priv = make_socket_secure(conn->sock)) == NULL) {
                    fprintf(stderr, "Failed to make socket secure\n");
                    exit(1);
                }
                conn->conn.ci.writer = secure_connection_write;
                conn->conn.ci.reader = secure_connection_read;
#else
                fprintf(stderr,
                        "FATAL: HTTPS requires GnuTLS, which is not installed....\n");
                exit(1);
#endif
                break;
            }
            default:
            {
                conn->conn.ci.writer = tcp_connection_write;
                conn->conn.ci.reader = tcp_connection_read;
                break;
            }
        }
        goto ret;
    }

err:
    fprintf(stderr, "Failed to get proper host address for: %s\n",
            ui->host);
    FIF(entry);
    FIF(conn->host);
    FIF(conn);
    conn = NULL;
ret:
    return (connection *) conn;
}

void connection_put(connection* conn)
{
    PDEBUG ("enter: %p\n", conn);

    connection_p* pconn = (connection_p*)conn;
    if (!pconn->host)
    {
        goto clean;
    }

    char* host_key = NULL;
    int ret = asprintf(&host_key, "%s:%d", pconn->host, pconn->port);
    if (!ret)
    {
        goto clean;
    }

    ccache* cache = HASH_ENTRY_GET(ccache, g_conn_cache, host_key);
    if (!cache)
    {
        cache = ZALLOC1(ccache);
        PDEBUG ("allocating new ccahe: %p\n", cache);

        cache->count++;
        cache->lst   = &pconn->lst;

        hash_table_insert(g_conn_cache, host_key, cache, sizeof(void*));
    }
    else if (cache->count >= MAX_CONNS_PER_HOST)
    {
        goto clean;
    }
    else
    {
        cache->count++;
        PDEBUG ("increasing: %d\n", cache->count);
        pconn->lst.next = cache->lst;
        cache->lst = &pconn->lst;
    }

    FIF(host_key);
    PDEBUG ("return with connection put to cache\n");

    return;

    //@todo: convert this conn_p to connection_p_list and cache it.
clean:
    PDEBUG ("cleaning connetion: %p\n", pconn);

    FIF(pconn->host);
    FIF(pconn);
    PDEBUG ("leave with connection cleared...\n");

}

static inline int do_perform_select(connection_group* group)
{
    int    maxfd = 0;
    int    cnt   = group->cnt;

    fd_set rfds;
    FD_ZERO(&rfds);
    fd_set wfds;
    FD_ZERO(&wfds);

    slist_head* p = group->lst;
    PDEBUG("p: %p, next: %p\n", p, p->next);
    while (p) {
        connection_p* conn = LIST2PCONN(p);
        PDEBUG("p: %p, conn: %p, sock: %d, off: %ld, next: %p\n",
               p, conn, conn->sock, offsetof(connection_p, lst), p->next);
        if ((conn->sock != 0) && conn->conn.rf && conn->conn.wf) {
            int ret = fcntl(conn->sock, F_SETFD, O_NONBLOCK);
            if (ret == -1)
            {
                fprintf(stderr, "Failed to unblock socket!\n");
                return -1;
            }
            FD_SET(conn->sock, &wfds);
            if (maxfd < conn->sock) {
                maxfd = conn->sock;
            }
        }
        p = p->next;
    }

    maxfd++;

    int nfds = 0;

    PDEBUG("%p: %d\n", group->cflag, *group->cflag);
    struct timeval tv;

    while (!(*(group->cflag))) {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        nfds = select(maxfd, &rfds, &wfds, NULL, &tv);
        if (nfds == -1) {
            perror("select fail:");
            break;
        }

        /* if (nfds == 0) { */
        /*     fprintf(stderr, "time out ....\n"); */
        /* } */

        slist_head* p = group->lst;

        while (p) {
            connection_p* pconn = LIST2PCONN(p);
            int           ret   = 0;

            if (FD_ISSET(pconn->sock, &wfds)) {
                ret = pconn->conn.wf((connection *) pconn, pconn->conn.priv);
                PDEBUG ("%d byte written\n", ret);

                if (ret > 0) {
                    FD_CLR(pconn->sock, &wfds);
                    FD_SET(pconn->sock, &rfds);
                }
            }
            else if (FD_ISSET(pconn->sock, &rfds)) {
                ret = pconn->conn.rf((connection *) pconn, pconn->conn.priv);
                switch (ret)
                {
                    case COF_CLOSED:
                    {
                        pconn->connected = false;
                    }
                    case COF_FAILED:
                    case COF_FINISHED:
                    {
                        PDEBUG("remove conn: %p socket: %d, ret: %d...\n",
                               pconn, pconn->sock, ret);
                        FD_CLR(pconn->sock, &rfds);
                        /* close(pconn->sock); */
                        cnt--;
                        PDEBUG("remaining sockets: %d\n", cnt);
                        pconn->active = false;
                        pconn->busy   = false;
                        break;
                    }
                    case COF_AGAIN:
                    default:
                    {
                        FD_SET(pconn->sock, &rfds);
                        break;
                    }
                }
            }
            else if (pconn->active) {
                FD_SET(pconn->sock, &rfds);
            }

            p = p->next;
        }

        if (cnt == 0) {
            break;
        } else if (cnt < group->cnt / 2) { // half of connections are free, reschedule.
            static bool rescheduled = false;
            if (!rescheduled) {
                PDEBUG("TODO: implement reschduling...\n");
                rescheduled = true;
            }
        }

        if (*(group->cflag)) {
            fprintf(stderr, "Stop because control_flag set to 1!!!\n");
            break;
        }
    }
    return cnt;
}

int connection_perform(connection_group* group)
{
    PDEBUG("enter, sg: %p\n", group);

    int cnt = group->cnt;
    PDEBUG("total socket: %d\n", cnt);

    if (!cnt)
        return -1;

    cnt = do_perform_select(group);

    return cnt;
}


connection_group *connection_group_create(bool * flag)
{
    connection_group *group = ZALLOC1(connection_group);

    group->cflag = flag;

    return group;
}

void connection_group_destroy(connection_group* group)
{
    slist_head* g = group->lst;

    while (g) {
        slist_head*   ng    = g->next;
        connection_p* pconn = LIST2PCONN(g);
        connection_put((connection*)pconn);
        g = ng;
    }

    FIF(group);
}

void connection_add_to_group(connection_group* group, connection* conn)
{
    group->cnt++;
    connection_p* pconn = CONN2CONNP(conn);
    pconn->lst.next = group->lst;
    group->lst = &pconn->lst;
    PDEBUG("Socket: %p added to group: %p, current count: %d\n",
           conn, group, group->cnt);
}

/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * fill-column: 78
 * End:
 *
 * vim: set noet ts=4 sw=4:
 */
