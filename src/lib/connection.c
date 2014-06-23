/** mget_sock.c --- implementation of mget_sock
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

#include "connection.h"
#include "data_utlis.h"
#include "log.h"
#include "mget_config.h"
#include "mget_types.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#ifdef HAVE_GNUTLS
#include "ssl.h"
#endif


typedef struct addrinfo address;

typedef enum _connection_feature {
    sf_keep_alive = 1,
} connection_feature;


typedef struct _addr_entry {
    address *addr;                      // don't release ti.
    address *infos;                     // should be freed.
    uint32   feature;
} addr_entry;

typedef struct _connection_p {
    connection conn;

    int      sock;
    char    *host;
    address *addr;
    void    *priv;
    bool     connected;
    bool     active;
    bool     closed;
    bool   busy;
    /* uint32 atime; */
} connection_p;

#define CONN2CONNP(X)       (connection_p*)(X)

typedef struct _connection_list {
    mget_slist_head *next;
    connection *conn;
} connection_list;

struct _connection_group {
    int cnt;			// count of sockets.
    bool *cflag;		// control flag.
    connection_list *lst;	// list of sockets.
};

void addr_entry_destroy(void *entry)
{
    addr_entry *e = (addr_entry *) entry;

    if (e) {
        FIF(e->infos);
        free(e);
    }
}


static uint32 tcp_connection_read(connection * conn, char *buf,
                                  uint32 size, void *priv)
{
    connection_p *pconn = (connection_p *) conn;

    if (pconn && pconn->sock && buf) {
        uint32 rd = (uint32) read(pconn->sock, buf, size);

        if (rd == -1) {
            if (errno == EAGAIN) { // nothing to read, normal if non-blocking
                ;
            }
            else  {
                PDEBUG ("Read connection: %p returns -1.\n", pconn);
            }
        }
        else if (!rd)  {
            PDEBUG ("Read connection: %p returns 0, connection closed...\n",
                    pconn);
            pconn->closed = true;
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

static hash_table* g_addr_cache = NULL;

#define print_address(X)   inet_ntoa ((X)->data.d4)

connection* connection_get(const url_info* ui)
{
    if (!g_addr_cache) {
        g_addr_cache = hash_table_create(64, addr_entry_destroy);	//TODO: add deallocation..
        assert(g_addr_cache);
    }

    connection_p* conn = ZALLOC1(connection_p);

    if (!ui || (!ui->host && !ui->addr)) {
        PDEBUG ("invalid ui.\n");
        goto ret;
    }

    PDEBUG ("%p -- %p\n", ui, ui->addr);

    addr_entry *addr = NULL;
    if (ui->addr)
    {
        conn->sock = socket(AF_INET, SOCK_STREAM, 0);
        if (conn->sock == -1) {
            goto err;
        }
        PDEBUG ("AA0\n");


        struct sockaddr_in sa;
        if ((conn->sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket");
            goto err;
        }

        bzero(&sa, sizeof sa);

        sa.sin_family = AF_INET;
        sa.sin_port = htons(ui->port);
        sa.sin_addr = ui->addr->data.d4;
        DEBUGP (("AA:trying to connect to %s port %lu\n",
                 print_address (ui->addr), ui->port));
        if (connect(conn->sock, (struct sockaddr *)&sa, sizeof sa) < 0) {
            perror("connect");
            close(conn->sock);
            goto err;
        }
    }
    else
    {
        addr = GET_HASH_ENTRY(addr_entry, g_addr_cache, ui->host);

        if (addr) {
            conn->addr = addr->addr;
            conn->sock = socket(conn->addr->ai_family, conn->addr->ai_socktype,
                                conn->addr->ai_protocol);
            if (connect (conn->sock, conn->addr->ai_addr,
                         conn->addr->ai_addrlen) == -1) {
                perror("Failed to connect");
                goto err;
            }
        } else {
            addr = ZALLOC1(addr_entry);
            address hints;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;	//AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = 0;
            hints.ai_protocol = 0;
            logprintf(LOG_ALWAYS, "Resolving host: %s ...\n", ui->host);
            int ret = getaddrinfo(ui->host, ui->sport, &hints, &addr->infos);

            if (ret)
                goto err;
            address *rp = NULL;

            for (rp = addr->infos; rp != NULL; rp = rp->ai_next) {
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
                addr->addr = rp;
                if (!hash_table_insert(g_addr_cache, (char*)ui->host, addr,
                                       sizeof(*addr))) {
                    addr_entry_destroy(addr);
                    fprintf(stderr, "Failed to insert cache: %s\n", ui->host);
                }

            }
        }
    }

    if (conn) {
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
    FIF(addr);
    FIF(conn);
    conn = NULL;
ret:
    return (connection *) conn;
}

void connection_put(connection * sock)
{
    FIF(CONN2CONNP(sock));
}

static inline int do_perform_select(connection_group* group)
{
    int    maxfd = 0;
    int    cnt   = group->cnt;

    fd_set rfds;
    FD_ZERO(&rfds);
    fd_set wfds;
    FD_ZERO(&wfds);

    connection_list *p = group->lst;
    PDEBUG("p: %p, next: %p\n", p, p->next);
    while (p && p->conn) {
        PDEBUG("p: %p, next: %p\n", p, p->next);
        connection_p *conn = (connection_p *) p->conn;

        if ((conn->sock != 0) && conn->conn.rf && conn->conn.wf) {
            fcntl(conn->sock, F_SETFD, O_NONBLOCK);
            FD_SET(conn->sock, &wfds);
            if (maxfd < conn->sock) {
                maxfd = conn->sock;
            }
        }
        p = (connection_list *) p->next;
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

        if (nfds == 0) {
            /* fprintf(stderr, "time out ....\n"); */
        }

        connection_list *p = group->lst;

        while (p && p->conn) {
            connection_p *pconn = (connection_p *) p->conn;
            int ret = 0;

            if (FD_ISSET(pconn->sock, &wfds)) {
                ret = pconn->conn.wf((connection *) pconn, pconn->conn.priv);
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
                        pconn->closed = true;
                    }
                    case COF_FAILED:
                    case COF_FINISHED:
                    {
                        PDEBUG("remove socket: %d, ret: %d...\n",
                               pconn->sock, ret);
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

            p = (connection_list *) p->next;
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

    INIT_LIST(group->lst, connection_list);

    return group;
}

void connection_group_destroy(connection_group * group)
{
    connection_list *g = group->lst;

    while (g) {
        connection_list *ng = (connection_list *) g->next;

        FIF(CONN2CONNP(g->conn));
        FIF(g);
        g = ng;
    }

    FIF(group);
}

void connection_add_to_group(connection_group * group, connection * sock)
{
    group->cnt++;
    connection_list *tail = group->lst;

    if (tail->conn) {
        SEEK_LIST_TAIL(group->lst, tail, connection_list);
    }
    assert(tail->conn == NULL);
    tail->conn = sock;
    PDEBUG("Socket: %p added to group: %p, current count: %d\n",
           sock, group, group->cnt);
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
