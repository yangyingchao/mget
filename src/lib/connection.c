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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "typedefs.h"
#include "data_utlis.h"
#include "debug.h"
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>

typedef struct addrinfo address;


typedef enum _connection_feature
{
    sf_keep_alive = 1,
} connection_feature;


typedef struct _addr_entry
{
    address* addr;  // don't release ti.
    address* infos; // should be freed.
    uint32   feature;
} addr_entry;

typedef struct _connection_p
{
    connection conn;

    int      sock;
    char*    host;
    address* addr;
    bool     connected;
    bool     active;
    /* bool   busy; */
    /* uint32 atime; */
} connection_p;

#define CONN2CONNP(X)       (connection_p*)(X)

typedef struct _connection_list
{
    mget_slist_head* next;
    connection*           conn;
} connection_list;

struct _connection_group
{
    int        cnt;                     // count of sockets.
    bool*      cflag;                   // control flag.
    connection_list* lst;                     // list of sockets.
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


static uint32 tcp_connection_read(connection* conn, char* buf,
                              uint32 size, void *priv)
{
    connection_p* pconn = (connection_p*) conn;
    if (pconn && pconn->sock && buf)
    {
        uint32 rd = (uint32)read(pconn->sock, buf, size);
        if (!rd || rd == -1)
        {
            PDEBUG ("rd: %d\n", rd);
        }
        return rd;
    }

    return 0;

}
static uint32 tcp_connection_write(connection* conn, char* buf,
                               uint32 size, void *priv)
{
    connection_p* pconn = (connection_p*) conn;
    if (pconn && pconn->sock && buf)
    {
        return (uint32)write(pconn->sock, buf, size);
    }
    return 0;
}

static void tcp_connection_close(connection* conn, char* buf,
                             uint32 size, void *priv)
{
}

static hash_table* g_addr_cache = NULL;

connection* connection_get(url_info* ui)
{
    if (!g_addr_cache)
    {
        g_addr_cache = hash_table_create(64, addr_entry_destroy); //TODO: add deallocation..
        assert(g_addr_cache);
    }

    connection_p* conn = ZALLOC1(connection_p);
    if (!ui || !ui->host)
    {
        goto ret;
    }

    addr_entry* addr = GET_HASH_ENTRY(addr_entry, g_addr_cache, ui->host);
    if (addr)
    {
        conn->addr = addr->addr;
        conn->sock = socket(conn->addr->ai_family, conn->addr->ai_socktype,
                             conn->addr->ai_protocol);
        if (connect(conn->sock, conn->addr->ai_addr, conn->addr->ai_addrlen) == -1)
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
        conn->sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (conn->sock == -1)
        {
            continue;
        }
        if (connect(conn->sock, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            conn->connected = true;
            break;
        }
        close(conn->sock);
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
    FIF(conn);
    conn = NULL;
ret:
    conn->active = true;
    conn->conn.ci.writer = tcp_connection_write;
    conn->conn.ci.reader = tcp_connection_read;
    return (connection*)conn;
}

void connection_put(connection* sock)
{
    FIF(CONN2CONNP(sock));
}

int connection_perform(connection_group* group)
{
    PDEBUG ("enter, sg: %p\n", group);

    int cnt = group->cnt;
    PDEBUG ("total socket: %d\n", cnt);

    if (!cnt)
        return -1;

    fd_set rfds;
    FD_ZERO(&rfds);

    fd_set wfds;
    FD_ZERO(&wfds);

    int maxfd = 0;

    //TODO: Add sockets to Epoll
    connection_list* p = group->lst;
    PDEBUG ("p: %p, next: %p\n", p, p->next);

    while (p && p->conn) {
        PDEBUG ("p: %p, next: %p\n", p, p->next);
        connection_p* conn = (connection_p*)p->conn;
        if ((conn->sock != 0) && conn->conn.rf && conn->conn.wf) {
            /* fcntl(conn->sock, F_SETFD, O_NONBLOCK); */
            FD_SET(conn->sock, &wfds);
            if (maxfd < conn->sock)
            {
                maxfd = conn->sock;
            }
        }
        p = (connection_list*)p->next;
    }

    maxfd ++;

    int nfds = 0;
    PDEBUG ("%p: %d\n", group->cflag, *group->cflag);
    struct timeval tv;
    tv.tv_sec  = 1;
    tv.tv_usec = 0;

    while (!(*(group->cflag))) {

        nfds = select(maxfd, &rfds, &wfds, NULL, &tv);

        if (nfds == -1) {
            perror("select fail:");
            break;
        }

        if (nfds == 0)
        {
            fprintf (stderr, "time out ....\n");
        }

        connection_list* p = group->lst;
        while (p && p->conn) {
            connection_p* pconn    = (connection_p*)p->conn;
            int           ret      = 0;

            if (FD_ISSET(pconn->sock, &wfds)) {
                ret = pconn->conn.wf((connection*)pconn, pconn->conn.priv);
                if (ret > 0)
                {
                    FD_CLR(pconn->sock, &wfds);
                    FD_SET(pconn->sock, &rfds);
                }
            }

            else if (FD_ISSET(pconn->sock, &rfds)) {
                ret = pconn->conn.rf((connection*)pconn, pconn->conn.priv);
                if (ret && ret != -1)
                {
                    FD_SET(pconn->sock, &rfds);
                }
                else
                {
                    PDEBUG ("remove socket: %d, ret: %d...\n", pconn->sock, ret);
                    FD_CLR(pconn->sock, &rfds);
                    /* close(pconn->sock); */
                    cnt --;
                    PDEBUG ("remaining sockets: %d\n", cnt);
                    pconn->active = false;
                }
           }

            else if (pconn->active)
            {
                FD_SET(pconn->sock, &rfds);
            }

            p = (connection_list*)p->next;
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


connection_group* connection_group_create(bool* flag)
{
    connection_group* group = ZALLOC1(connection_group);
    group->cflag = flag;

    INIT_LIST(group->lst, connection_list);

    return group;
}

void connection_group_destroy(connection_group* group)
{
    connection_list* g = group->lst;
    while (g) {
        connection_list* ng = (connection_list*)g->next;
        FIF(CONN2CONNP(g->conn));
        FIF(g);
        g=ng;
    }

    FIF(group);
}

void connection_add_to_group(connection_group* group, connection* sock)
{
    group->cnt ++;
    connection_list* tail = group->lst;
    if (tail->conn)
    {
        SEEK_LIST_TAIL(group->lst, tail, connection_list);
    }
    assert(tail->conn == NULL);
    tail->conn = sock;
    PDEBUG ("Socket: %p added to group: %p, current count: %d\n",
            sock, group, group->cnt);
}
