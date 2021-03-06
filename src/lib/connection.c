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

#include <stdio.h>

#include "logutils.h"
#include "connection.h"
#include "data_utlis.h"
#include "mget_config.h"
#include "mget_types.h"
#include "fileutils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef SSL_SUPPORT
#include "plugin/ssl/ssl.h"
#endif

typedef struct addrinfo address;

typedef enum _connection_feature {
    sf_keep_alive = 1,
} connection_feature;

typedef struct _addr_entry {
    int size;                   /* total size of this entry... */
    address *addr;              /* addrinfo pointer. */

    int ai_family;              /* Protocol family for socket.  */
    int ai_socktype;            /* Socket type.  */
    int ai_protocol;            /* Protocol for socket.  */

    socklen_t ai_addrlen;       /* Length of socket address.  */
    char buffer[0];             /* Buffer of sockaddr.  */
} addr_entry;

typedef enum _expected_operation
{
    eow = 1,
    eor = 2,
    eo_all =  eow | eor
} expected_operation;


typedef struct _connection_p {
    connection conn;
    connection_operations rco;  // real operators..
    slist_head lst;
    int sock;
    int port;
    char *host;
    address *addr;
    void *priv;
    bool connected;
    bool active;
    bool busy;
    expected_operation expt;
    int last_access;            // last connected..
} connection_p;

struct _connection_group {
    int cnt;                    // count of sockets.
    bool *cflag;                // control flag.
    uint32 type;                // refer to cgtype
    slist_head *lst;            // list of sockets.
};

typedef struct _connection_cache {
    int count;
    slist_head *lst;
} ccache;




#define ALLOC_ADDR_ENTRY(X)                         \
    (addr_entry*)XALLOC(sizeof(addr_entry) + (X))

#ifdef DEBUG
#define OUT_ADDR(X) fprintf(stderr, "entry->" #X ": %d\n", entry->ai_##X)
#else
#define OUT_ADDR(X)
#endif


#define print_address(X)   inet_ntoa ((X)->data.d4)

#define CONN2CONNP(X) (connection_p*)(X)
#define LIST2PCONN(X) (connection_p*)((char*)X - offsetof(connection_p, lst))


#define TIME_OUT      5

#define MAX_CONNS_PER_HOST  32
host_cache_type g_hct = HC_DEFAULT;
static hash_table *g_conn_cache = NULL;
static byte_queue *dq = NULL;   // drop queue
static hash_table *addr_cache = NULL;

static int bw_limit = -1;       // band-width limit.


/* Address entry related. */
static address *addrentry_to_address(addr_entry * entry);
static addr_entry *address_to_addrentry(address * addr);
static void addr_entry_destroy(void *entry);

/* Generic socket operations */

static int connection_save_to_fd(connection *conn, int out);

/* tcp socket operations */
static int tcp_connection_read(connection * conn, char *buf,
                               uint32 size, void *priv);
static int tcp_connection_write(connection * conn, const char *buf,
                                uint32 size, void *priv);

// TODO: Remove this ifdef!
#if 0
static void tcp_connection_close(connection * conn, char *buf,
                                 uint32 size, void *priv);
#endif // End of #if 0
static bool validate_connection(connection_p * pconn);

#ifdef SSL_SUPPORT
static int secure_connection_read(connection * conn, char *buf,
                                  uint32 size, void *priv);
static int secure_connection_write(connection * conn, const char *buf,
                                   uint32 size, void *priv);
#endif

static int mget_connection_read(connection * conn, char *buf,
                                uint32 size, void *priv);
static int mget_connection_write(connection * conn, const char *buf,
                                 uint32 size, void *priv);
static void limit_bandwidth(connection_p * conn, int size);

static int do_perform_select(connection_group * group);

static int connect_to(int ai_family, int ai_socktype,
                      int ai_protocol,
                      const struct sockaddr *addr,
                      socklen_t addrlen, int timeout);

static bool try_connect(int sock, const struct sockaddr *addr,
                        socklen_t addrlen, int timeout);

static char *get_host_key(const char *host, int port);

static int create_nonblocking_socket();


static void connection_destroy(void* conn)
{
    PDEBUG("cleaning connetion: %p\n", conn);
    connection_p *pconn = (connection_p *) conn;
    if (pconn->rco.close)
        (*pconn->rco.close)(conn, pconn->priv);

    FIF(pconn->host);
    if (pconn->addr)
        FIF(pconn->addr->ai_addr);
    FIF(pconn->addr);
    FIF(pconn);
}

static void ccache_destroy(void* ptr)
{
    if (!ptr)
        return;

    ccache* cache = (ccache*)ptr;
    slist_head* p = cache->lst;
    while (p) {
        connection_p* pc = LIST2PCONN(p);
        p = p->next;
        connection_destroy(pc);
    }

    FIF(cache);
}

connection *connection_get(const url_info* ui, bool async)
{
    PDEBUG ("Getting connection for  %s\n", url_info_stringify(ui));
    connection_p *conn = NULL;
    addr_entry *entry = NULL;

    if (!ui || (!ui->host && !ui->addr)) {
        PDEBUG("invalid ui.\n");
        goto ret;
    }

    if (!dq) {                  // init this drop queue.
        dq = bq_init(1024);
    }

    if (!g_conn_cache) {
        g_conn_cache = hash_table_create(64, ccache_destroy);
        assert(g_conn_cache);
    }

    if (ui->addr) {
        conn = ZALLOC1(connection_p);
        struct sockaddr_in sa;
        bzero(&sa, sizeof sa);
        sa.sin_family = AF_INET;
        sa.sin_port = htons(ui->port);
        sa.sin_addr = ui->addr->data.d4;
        PDEBUG("trying to connect to %s port %u\n",
               print_address(ui->addr), ui->port);
        conn->sock = connect_to(AF_INET, SOCK_STREAM, 0,
                                (struct sockaddr *) &sa, sizeof sa,
                                async ? 0 : TIME_OUT);
        if (conn->sock == -1) {
            perror("connect");
            close(conn->sock);
            goto err;
        }
    } else {
        char *host_key = get_host_key(ui->host, ui->port);
        if (!host_key) {
            goto alloc;
        }

        ccache *cache = HASH_ENTRY_GET(ccache, g_conn_cache, host_key);
        FIF(host_key);
        PDEBUG("cache: %p, count: %d, lst: %p\n", cache,
               cache ? cache->count : 0, cache ? cache->lst : NULL);

        if (cache && cache->count && cache->lst) {
            conn = LIST2PCONN(cache->lst);
            cache->lst = cache->lst->next;
            cache->count--;
            conn->lst.next = NULL;
            if (!validate_connection(conn)) {
                goto connect;
            }
            PDEBUG("\nRusing connection: %p\n", conn);
            goto post_connected;
        }

  alloc:
        conn = ZALLOC1(connection_p);
        static shm_region *shm_rptr = NULL;
        if (!addr_cache) {
            if (g_hct == HC_BYPASS)
                goto alloc_addr_cache;

            char key[64] = { '\0' };
            sprintf(key, "/libmget_%s_uid_%d", VERSION_STRING, getuid());
            shm_rptr = shm_region_open(key);
            if (shm_rptr)
                addr_cache = hash_table_create_from_buffer(shm_rptr->buf, SHM_LENGTH);
            if (!addr_cache) {
          alloc_addr_cache:;
                addr_cache = hash_table_create(64, addr_entry_destroy);
                assert(addr_cache);
            }
        }

        if (g_hct == HC_DEFAULT) {
            char *key = get_host_key(ui->host, ui->port);
            entry = GET_HASH_ENTRY(addr_entry, addr_cache, key);
            FIF(key);
        }

        if (entry) {
            mlog(QUIET, "Using cached address...\n");
            conn->addr = addrentry_to_address(entry);
      connect:
            PDEBUG("Connecting to: %s:%d\n", ui->host, ui->port);

            conn->sock = connect_to(conn->addr->ai_family,
                                    conn->addr->ai_socktype,
                                    conn->addr->ai_protocol,
                                    conn->addr->ai_addr,
                                    conn->addr->ai_addrlen,
                                    async ? 0 : TIME_OUT);
            if (conn->sock == -1) {
                perror("Failed to connect");
                goto hint;
            }
        } else {
            mlog(QUIET, "Can't find cached address..\n");
      hint:;
            address hints;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;  //AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = 0;
            hints.ai_protocol = 0;
            mlog(ALWAYS, "Resolving host: %s ...\n", ui->host);
            struct addrinfo *infos = NULL;
            int ret = getaddrinfo(ui->host, ui->sport, &hints, &infos);

            PDEBUG(": ret = %d, error: %s\n", ret, strerror(errno));

            if (ret)
                goto err;
            address *rp = NULL;

            for (rp = infos; rp != NULL; rp = rp->ai_next) {
                PDEBUG("Connecting to %s:%u\n", ui->host, ui->port);
                conn->sock = connect_to(rp->ai_family, rp->ai_socktype,
                                        rp->ai_protocol,
                                        rp->ai_addr, rp->ai_addrlen,
                                        TIME_OUT);
                if (conn->sock != -1) {
                    PDEBUG("Connected ...\n");

                    conn->connected = true;
                    break;
                }
                conn->sock = -1;
                PDEBUG("rp: %p\n", rp->ai_next);
            }

            if (rp != NULL) {
                char *key = get_host_key(ui->host, ui->port);
                entry = address_to_addrentry(rp);
                conn->addr = entry->addr;
                if (!hash_table_update(addr_cache, key, entry,
                                       entry->size)) {
                    addr_entry_destroy(entry);
                    fprintf(stderr, "Failed to insert cache: %s\n",
                            ui->host);
                } else {
                    // Only update cache to shm when it is not used by others.
                    // This is just for optimization, and it does not hurt
                    // much if one or two cache is missing...
                    if (shm_rptr && !shm_rptr->busy && g_hct != HC_BYPASS) {
                        shm_rptr->busy = true;
                        shm_rptr->len = dump_hash_table(addr_cache,
                                                        shm_rptr->buf,
                                                        SHM_LENGTH);
                        shm_rptr->busy = false;
                    }
                }
                FIF(key);
            } else {
                goto err;
            }
        }
    }

post_connected:;
    if (conn) {
        if (ui->host && !conn->host) {
            conn->host = strdup(ui->host);
        }

        PDEBUG("sock(%d) %p connected to %s. \n", conn->sock, conn,
               conn->host);

        conn->connected   = true;
        conn->port        = ui->port;
        conn->active      = true;
        conn->last_access = get_time_s();
        switch (ui->eprotocol) {
            case HTTPS: {
                connection_make_secure(&conn->conn);
                break;
            }
            default: {
                conn->rco.write = tcp_connection_write;
                conn->rco.read  = tcp_connection_read;
                break;
            }
        }
        conn->rco.save_to_fd = connection_save_to_fd;
        conn->conn.co.write  = mget_connection_write;
        conn->conn.co.read   = mget_connection_read;

        PDEBUG ("C: %p, P: %p, W: %p, R: %p\n",
                conn,
                &conn->rco,conn->rco.write, conn->rco.read);
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
    PDEBUG("return connection: %p\n", conn);
    if (conn)
        conn->expt = eo_all;

    return (connection *) conn;
}

void connection_put(connection * conn)
{
    if (!conn)
        return;

    connection_p *pconn = (connection_p *) conn;
    if (!pconn->host) {
        goto clean;
    }
    pconn->lst.next = NULL;

    char *host_key = NULL;
    int ret = asprintf(&host_key, "%s:%d", pconn->host, pconn->port);
    if (!ret) {
        goto clean;
    }

    ccache *cache = HASH_ENTRY_GET(ccache, g_conn_cache, host_key);
    if (!cache) {
        cache = ZALLOC1(ccache);
        PDEBUG("allocating new ccahe: %p\n", cache);

        cache->count++;
        cache->lst = &pconn->lst;

        HASH_TABLE_INSERT(g_conn_cache, host_key, cache, sizeof(void *));
    } else if (cache->count >= MAX_CONNS_PER_HOST) {
        goto clean;
    } else {
        cache->count++;
        PDEBUG("increasing: %d\n", cache->count);
        pconn->lst.next = cache->lst;
        cache->lst = &pconn->lst;
    }

    FIF(host_key);
    PDEBUG("return with connection put to cache\n");

    return;

    //@todo: convert this conn_p to connection_p_list and cache it.
clean:
    connection_destroy(conn);
    PDEBUG("leave with connection cleared...\n");
}


int connection_perform(connection_group * group)
{
    PDEBUG("enter, sg: %p\n", group);
    if (!group || !group->cnt)
        return -1;

    return do_perform_select(group);
}


connection_group *connection_group_create(uint32 type, bool * flag)
{
    connection_group *group = ZALLOC1(connection_group);

    group->cflag = flag;
    group->type = type;

    return group;
}

void connection_group_destroy(connection_group * group)
{
    slist_head *g = group->lst;

    while (g) {
        slist_head *ng = g->next;
        connection_p *pconn = LIST2PCONN(g);
        connection_put((connection *) pconn);
        g = ng;
    }

    FIF(group);
}

void connection_add_to_group(connection_group * group, connection * conn)
{
    PDEBUG("enter with group: (%p), conn: (%p)\n", group, conn);
    if (conn && group) {
        group->cnt++;
        connection_p *pconn = CONN2CONNP(conn);
        PDEBUG("pconn->lst: %p\n", pconn->lst);

        pconn->lst.next = group->lst;
        group->lst = &pconn->lst;
        PDEBUG("Socket: %p added to group: %p, current count: %d\n",
               conn, group, group->cnt);
    }
}

bool timed_wait(int sock, int type, int delay)
{
    fd_set r, w, err;

    FD_ZERO(&r);
    FD_ZERO(&w);
    FD_ZERO(&err);
    FD_SET(sock, &err);

    if (type & WT_READ)
        FD_SET(sock, &r);

    if (type & WT_WRITE)
        FD_SET(sock, &w);

    struct timeval tv;
    tv.tv_sec = delay;
    tv.tv_usec = 0;

    int ret = select(sock + 1, &r, &w, &err, delay == -1 ? NULL : &tv);
    if (ret < 0) {
        mlog(ALWAYS, "select fail: %s\n", strerror(errno));
        return false;
    } else if (!ret) {
        mlog(ALWAYS, "Connection timed out...\n");
        return false;
    }

    return true;
}

void set_global_bandwidth(int limit)
{
    if (limit > 1024) {
        bw_limit = limit;
    }
}


// local functions
address *addrentry_to_address(addr_entry * entry)
{
    address *addr = NULL;
    if (entry && (addr = ZALLOC1(address))) {
        addr->ai_family = entry->ai_family;
        addr->ai_socktype = entry->ai_socktype;
        addr->ai_protocol = entry->ai_protocol;
        addr->ai_addrlen = entry->ai_addrlen;
        addr->ai_addr = ZALLOC1(struct sockaddr);

        memcpy(addr->ai_addr, entry->buffer, addr->ai_addrlen);
        entry->addr = addr;
    }
    return addr;
}

addr_entry *address_to_addrentry(address * addr)
{
    addr_entry *entry = NULL;
    if (addr && (entry = ALLOC_ADDR_ENTRY(addr->ai_addrlen))) {
        entry->size = sizeof(*entry) + addr->ai_addrlen;
        entry->ai_family = addr->ai_family;
        entry->ai_socktype = addr->ai_socktype;
        entry->ai_protocol = addr->ai_protocol;
        entry->ai_addrlen = addr->ai_addrlen;
        memcpy(entry->buffer, addr->ai_addr, addr->ai_addrlen);

        OUT_ADDR(family);
        OUT_ADDR(socktype);
        OUT_ADDR(protocol);
        OUT_BIN(entry->buffer, entry->ai_addrlen);

        // update entry->addr to itself.
        void *ptr = addrentry_to_address(entry);
        PDEBUG("ptr: %p -- %p\n", entry->addr, ptr);
    }

    return entry;
}

void addr_entry_destroy(void *entry)
{
    addr_entry *e = (addr_entry *) entry;

    if (e) {
        free(e);
    }
}

int connection_save_to_fd(connection *conn, int out)
{
    if (!conn)
        return COF_INVALID;

    char buf[4096];
    int s = conn->co.read(conn, buf, 4096, NULL);
    if (s > 0) {
        s = (int)write(out, buf, s);
    }

    return s;
}

int tcp_connection_read(connection * conn, char *buf,
                        uint32 size, void *priv)
{
    int rd = COF_INVALID;
    connection_p* pconn = (connection_p*) conn;
    if (pconn && pconn->sock && buf) {

        // double check to ensure there are something to read.
        if (!timed_wait(pconn->sock, WT_READ, -1)) {
            mlog(ALWAYS, "Nothing to read: (%d):%s\n",
                 errno, strerror(errno));
            return 0;
        }

        rd = read(pconn->sock, buf, size);
        if (rd == -1) {
            PDEBUG("rd: %d, sock: %d, errno: (%d) - %s\n",
                   rd, pconn->sock, errno, strerror(errno));
            if (errno == EAGAIN) {      // nothing to read, normal if non-blocking
                rd = COF_AGAIN;
            } else {
                mlog(ALWAYS, "Read connection:"
                     " %p returns -1, (%d): %s.\n",
                     pconn, errno, strerror(errno));
                rd = COF_FAILED;
            }
        } else if (!rd) {
            mlog(QUIET, "Read connection: "
                 "%p, sock: %d returns 0, connection closed...\n",
                 pconn, pconn->sock);
            pconn->connected = false;
            rd = COF_CLOSED;
        }
    }

    return rd;
}

int tcp_connection_write(connection * conn, const char *buf,
                         uint32 size, void *priv)
{
    connection_p *pconn = (connection_p *) conn;

    if (pconn && pconn->sock && buf) {
        PDEBUG("begin write, conn: %p, sock: %d ....\n",
               pconn, pconn->sock);
        int wd = (int)write(pconn->sock, buf, size);
        PDEBUG("%d bytes written\n", wd);
        if (wd < 0) {
            PDEBUG("failed to write to sock: %d, (%d):%s\n",
                   pconn->sock, errno, strerror(errno));
        }


        return wd;
    }
    return 0;
}

// TODO: Remove this ifdef!
#if 0
void tcp_connection_close(connection * conn, char *buf,
                          uint32 size, void *priv)
{
}
#endif // End of #if 0

#ifdef SSL_SUPPORT
int secure_connection_read(connection * conn, char *buf,
                           uint32 size, void *priv)
{
    connection_p *pconn = (connection_p *) conn;

    if (pconn && pconn->sock && buf) {
        return secure_socket_read(pconn->sock, buf, size, pconn->priv);
    }
    return 0;
}

int secure_connection_write(connection * conn, const char *buf,
                            uint32 size, void *priv)
{
    connection_p *pconn = (connection_p *) conn;

    if (pconn && pconn->sock && buf) {
        return secure_socket_write(pconn->sock, (char*)buf, size, pconn->priv);
    }
    return 0;
}

bool secure_connection_has_more(connection * conn, void *priv)
{
    connection_p *pconn = (connection_p *) conn;
    if (pconn && priv) {
        return secure_socket_has_more(0, priv);
    }
    return false;
}

void secure_connection_close(connection * conn, void *priv)
{
    connection_p *pconn = (connection_p *) conn;
    if (pconn && priv) {
        ssl_destroy(priv);
    }
}
#endif

/* Return true iff the connection to the remote site established
   through SOCK is still open.

   Specifically, this function returns true if SOCK is not ready for
   reading.  This is because, when the connection closes, the socket
   is ready for reading because EOF is about to be delivered.  A side
   effect of this method is that sockets that have pending data are
   considered non-open.  This is actually a good thing for callers of
   this function, where such pending data can only be unwanted
   leftover from a previous request.  */

bool validate_connection(connection_p * pconn)
{
    if (!pconn->connected || pconn->sock == -1)
        return false;

    fd_set check_set;
    struct timeval to;
    int ret = 0;

    /* Check if we still have a valid (non-EOF) connection.  From Andrew
     * Maholski's code in the Unix Socket FAQ.  */

    FD_ZERO(&check_set);
    FD_SET(pconn->sock, &check_set);

    /* Wait one microsecond */
    to.tv_sec = 0;
    to.tv_usec = 1;

    ret = select(pconn->sock + 1, &check_set, NULL, NULL, &to);
    if (!ret)
        /* We got a timeout, it means we're still connected. */
        return true;
    else
        /* Read now would not wait, it means we have either pending data
           or EOF/error. */
        return false;
}

#define close_connection(x) do {                \
        close(x->sock);                         \
        x->sock = -1;                           \
        x->active = false;                      \
        x->connected = false;                   \
    } while (0)

int do_perform_select(connection_group* group)
{
    if (!(group->type & cg_all)) {
        mlog(ALWAYS, "Connection timed out...\n");
        return 0;
    }

    int maxfd = 0;
    int cnt = group->cnt;

    fd_set rfds;
    FD_ZERO(&rfds);
    fd_set wfds;
    FD_ZERO(&wfds);
    fd_set efds;
    FD_ZERO(&efds);

    slist_head *p;
    SLIST_FOREACH(p, group->lst) {
        connection_p *pconn = LIST2PCONN(p);
        // make sure no pending data left.
        if (pconn->rco.has_more) {
            while (pconn->rco.has_more(&pconn->conn, pconn->priv)) {
                 pconn->conn.recv_data((connection *) pconn, pconn->conn.priv);
            }
        }

        if (pconn->sock) {
            if ((group->type & cg_read) && pconn->conn.recv_data)
                FD_SET(pconn->sock, &rfds);

            if ((group->type & cg_write) && pconn->conn.write_data)
                FD_SET(pconn->sock, &wfds);

            FD_SET(pconn->sock, &efds);
            maxfd = maxfd > pconn->sock ? maxfd : pconn->sock;
        }
    }

    maxfd++;

    int nfds = 0;
    struct timeval tv;
    while (!(*(group->cflag))) {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        nfds = select(maxfd, &rfds, &wfds, &efds, &tv);
        if (nfds == -1) {
            fprintf(stderr, "Failed to select: %s\n", strerror(errno));
            break;
        }
        if (nfds == 0) {
            PDEBUG("timed out...\n");
            int cts = get_time_s();
            SLIST_FOREACH(p, group->lst) {
                connection_p *pconn = LIST2PCONN(p);
                if (pconn->active) {
                    if (cts - pconn->last_access > TIME_OUT) {
                        close_connection(pconn);
                        cnt--;
                        PDEBUG("cnt: %d\n", cnt);

                    } else {
                        FD_SET(pconn->sock, &rfds);
                    }
                }
            }
        } else {
            SLIST_FOREACH(p, group->lst) {
                connection_p *pconn = LIST2PCONN(p);
                int ret = 0;

                if (!pconn->active) {
                    continue;
                }
                if (FD_ISSET(pconn->sock, &wfds)) {
                    ret = pconn->conn.write_data((connection *) pconn,
                                                 pconn->conn.priv);
                    if (ret == COF_FINISHED) {
                        pconn->expt ^= eow;
                        FD_CLR(pconn->sock, &wfds);
                        FD_SET(pconn->sock, &rfds);
                        FD_SET(pconn->sock, &efds);
                    } else if (ret == COF_MORE_DATA) {
                        FD_SET(pconn->sock, &wfds);
                        FD_SET(pconn->sock, &rfds);
                        FD_SET(pconn->sock, &efds);
                    } else {
                        //@todo: handle this?
                        mlog(ALWAYS, "Unknown value: %d\n", ret);
                    }

                    pconn->last_access = get_time_s();
                } else if (FD_ISSET(pconn->sock, &rfds)) {
                    ret = pconn->conn.recv_data((connection *) pconn,
                                                pconn->conn.priv);
                    pconn->last_access = get_time_s();
                    switch (ret) {
                        case COF_CLOSED:
                        case COF_FAILED: {
                            close_connection(pconn);
                        }
                        case COF_FINISHED:{ //TODO:
                            PDEBUG("remove conn: %p socket: %d, ret: %d...\n",
                                   pconn, pconn->sock, ret);
                            /* close(pconn->sock); */
                            cnt--;
                            PDEBUG("remaining sockets: %d\n", cnt);
                            pconn->active = false;
                            pconn->connected = false;
                            pconn->expt ^= eor;
                            break;
                        }
                        case COF_ABORT:
                        {
                            exit(1);
                            break;
                        }
                        case COF_AGAIN:
                        default:
                        {
                            FD_SET(pconn->sock, &rfds);
                            break;
                        }
                    }
                } else if (FD_ISSET(pconn->sock, &efds)) {
                    PDEBUG ("failed: pconn: %p\n", pconn);
                    close_connection(pconn);
                    cnt--;
                } else if (pconn->active) {
                    if (pconn->expt & eor)
                        FD_SET(pconn->sock, &rfds);
                    if (pconn->expt & eow)
                        FD_SET(pconn->sock, &wfds);
                    FD_SET(pconn->sock, &efds);
                }
            }
        }

        if (cnt == 0) {
            break;
        } else if (cnt < group->cnt / 2) {// half of connections are free, reschedule.
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

int connect_to(int ai_family, int ai_socktype,
               int ai_protocol, const struct sockaddr *addr,
               socklen_t addrlen, int timeout)
{
    int sock = create_nonblocking_socket();
    if (sock == -1) {
        mlog(ALWAYS, "Failed to create socket - %s ...\n",
             strerror(errno));
        goto ret;
    }

    if (!try_connect(sock, addr, addrlen, timeout)) {
        close(sock);
        sock = -1;
    }

ret:
    return sock;
}

bool try_connect(int sockfd, const struct sockaddr *addr,
                 socklen_t addrlen, int timeout)
{
    if (connect(sockfd, addr, addrlen) == -1) {
        // failed to connect if sock is non-blocking or error is not EINTR.
        if (errno != EINPROGRESS) {
            PDEBUG("connection failed, (%d) - %s\n", errno,
                   strerror(errno));
            return false;
        }
        PDEBUG("Connecting ... (%d) - %s\n", errno, strerror(errno));
    }
    if (timeout)
        return timed_wait(sockfd, WT_WRITE, timeout);
    else
        return true;
}

char *get_host_key(const char *host, int port)
{
    char *key = NULL;
    masprintf(&key, "%s:%d", host, port);
    if (!key) {
        mlog(ALWAYS, "Failed to create host key -- %s: %d\n",
             host, port);
        abort();
    }

    return key;
}

int create_nonblocking_socket()
{
#if defined(USE_FCNTL)
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock != -1 && fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
        mlog(ALWAYS,
             "Failed to make socket (%d) non-blocking: %s ...\n", sock,
             strerror(errno));
        close(sock);
    }
#else
    int sock = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
#endif
    return sock;
}

/** Wrapper of tcp_connection_read/secure_connection_read. */
int mget_connection_read(connection * conn, char *buf,
                         uint32 size, void *priv)
{
    connection_p *pconn = (connection_p *) conn;
    int ret = 0;

    if (pconn && pconn->sock && buf) {
        ret = pconn->rco.read(conn, buf, size, priv);
    }

    limit_bandwidth(pconn, ret);

    return ret;
}

/** wrapper of tcp_connection_write/secure_connection_write. */
int mget_connection_write(connection * conn, const char *buf,
                          uint32 size, void *priv)
{
    // do nothing but invoke real writer..
    connection_p *pconn = (connection_p *) conn;
    if (pconn && pconn->sock && buf) {
        PDEBUG ("C: %p, P: %p, W: %p, R: %p\n",
                conn, &pconn->rco, pconn->rco.write, pconn->rco.read);
        return pconn->rco.write(conn, buf, size, priv);
    }
    return 0;
}

void limit_bandwidth(connection_p* conn, int size)
{
    // TODO: Remove this ifdef!
#if 0

    if (bw_limit == -1)         // not enabled.
       return;

    //Don't enable this for ftp which using multi-threads...
    if (conn->port == IPPORT_FTP) {
        return;
    }

    static uint32 ts = 0;
    if (!ts)
        ts = get_time_ms();

    static int chunk = 0;
    uint32 cts = get_time_ms();
    int delta = chunk - bw_limit;
    if (delta < 0) {
        if (cts - ts > 1000) {
            chunk = 0;
            ts = cts;
        } else
            chunk += size;
        return;
    }

    int slp = (int) (((float) chunk) / bw_limit * 1000) + ts - cts;
    PDEBUG("d: %d, limit: %d, ts: %u, c_ts: %u, sleep for: %d mseconds\n",
           delta, bw_limit, ts, cts, slp);
    if (slp > 0) {
        usleep(slp * 1000);
    }

    chunk = 0;
    ts = cts;
#endif                          // End of #if 0
}

void connection_make_secure(connection* conn)
{
    if (!conn)
        return;

#ifdef SSL_SUPPORT
    connection_p* pconn = (connection_p*)conn;
    ssl_init();
    if (pconn->priv) {
        ssl_destroy(pconn->priv);
    }
    if ((pconn->priv = make_socket_secure(pconn->sock)) == NULL) {
        fprintf(stderr, "Failed to make socket secure\n");
        abort();
    }
    pconn->rco.write    = secure_connection_write;
    pconn->rco.read     = secure_connection_read;
    pconn->rco.has_more = secure_connection_has_more;
    pconn->rco.close = secure_connection_close;
    PDEBUG ("C: %p, P: %p, W: %p, R: %p\n",
            conn, &pconn->rco, pconn->rco.write, pconn->rco.read);
#else
    fprintf(stderr,
            "FATAL: HTTPS requires GnuTLS, which is not installed....\n");
    abort();
#endif
}

void connection_cleanup()
{
    PDEBUG("connn: %p\n", g_conn_cache);
    hash_table_destroy(g_conn_cache);
    PDEBUG("addr_cache: %p\n", addr_cache);
    hash_table_destroy(addr_cache);
    bq_destroy(dq);
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
