/** ftp.c --- implementation download from ftp server.
 *
 * Copyright (C) 2014 Yang,Ying-chao
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

#include "log.h"
#include "ftp.h"
#include "connection.h"
#include <unistd.h>
#include <errno.h>
#include "data_utlis.h"
#include "metadata.h"
#include "mget_utils.h"
#include "wftp.h"
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>

#define DEFAULT_FTP_CONNECTIONS   3

typedef struct _connection_operation_param_ftp {
    void *addr;			//base addr;
    data_chunk *dp;
    url_info *ui;
    hash_table *ht;
    void (*cb) (metadata*, void*);
    metadata *md;
    int idx;
    dinfo* info;
    int fds[2];
    void* user_data;
} co_param_ftp;

/* This function accepts an pointer of connection pointer, on return. When 302
 * is detected, it will modify both ui and conn to ensure a valid connection
 * can be initialized. */
uerr_t get_remote_file_size_ftp(dinfo* info, ftp_connection* conn,
                                bool* can_split,
                                uint64* psize)
{
    if (!conn || !conn->conn || !info || !info->ui) {
        return 0;
    }


    /* Login to the server: */

    /* First: Establish the control ftp_connection, already done.  */

    /* Second: Login with proper USER/PASS sequence.  */
    mlog (LL_VERBOSE, "Logging in as %s ... \n", info->md->ptrs->user);

    uerr_t err = ftp_login (conn, info->md->ptrs->user, info->md->ptrs->passwd);

    /* FTPRERR, FTPSRVERR, WRITEFAILED, FTPLOGREFUSED, FTPLOGINC */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
        case FTPSRVERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("Error in server greeting.\n"));
            return err;
        case WRITEFAILED:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET,
                     _("Write failed, closing control ftp_connection.\n"));
            return err;
        case FTPLOGREFUSED:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("The server refuses login.\n"));
            return FTPLOGREFUSED;
        case FTPLOGINC:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("Login incorrect.\n"));
            return FTPLOGINC;
        case FTPOK:
            /* if (!opt.server_response) */
            /*     logputs (LOG_VERBOSE, _("Logged in!\n")); */
            break;
        default:
            abort ();
    }

    /* Third: Get the system type */
    logprintf (LOG_VERBOSE, "==> SYST ... ");
    err = ftp_syst (conn, &conn->rs);
    /* FTPRERR */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return err;
        case FTPSRVERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET,
                     _("Server error, can't determine system type.\n"));
            break;
        case FTPOK:
            /* Everything is OK.  */
            break;
        default:
            abort ();
    }


    /* Fourth: Find the initial ftp directory */

    logprintf (LOG_VERBOSE, "==> PWD ... ");
    err = ftp_pwd (conn, &conn->id);
    /* FTPRERR */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return err;
        case FTPSRVERR :
            /* PWD unsupported -- assume "/". */
            xfree (conn->id);
            conn->id = xstrdup ("/");
            break;
        case FTPOK:
            /* Everything is OK.  */
            break;
        default:
            abort ();
    }


    /* Fifth: Set the FTP type.  */
    char type_char = ftp_process_type (NULL);
    logprintf (LOG_VERBOSE, "==> TYPE %c ... ", type_char);
    err = ftp_type (conn, type_char);
    /* FTPRERR, WRITEFAILED, FTPUNKNOWNTYPE */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return err;
        case WRITEFAILED:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET,
                     _("Write failed, closing control ftp_connection.\n"));
            return err;
        case FTPUNKNOWNTYPE:
            logputs (LOG_VERBOSE, "\n");
            logprintf (LOG_NOTQUIET,
                       _("Unknown type `%c', closing control ftp_connection.\n"),
                       type_char);
            return err;
        case FTPOK:
            /* Everything is OK.  */
            break;
        default:
            abort ();
    }


    err = ftp_size (conn, info->ui->uri, (wgint*)psize);
    /* FTPRERR */
    switch (err)
    {
        case FTPRERR:
        case FTPSRVERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return err;
        case FTPOK:
            /* got_expected_bytes = true; */
            /* Everything is OK.  */
            break;
        default:
            abort ();
    }

    PDEBUG ("Total size: %s\n", stringify_size(*psize));

    err = ftp_rest (conn, 16); // Just a test to see if it supports this.

    /* FTPRERR, WRITEFAILED, FTPRESTFAIL */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return err;
        case WRITEFAILED:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET,
                     _("Write failed, closing control ftp_connection.\n"));
            return err;
        case FTPRESTFAIL:
            logputs (LOG_VERBOSE, _("\nREST failed, starting from scratch.\n"));
            break;
        case FTPOK:
            *can_split = true;
            break;
        default:
            abort ();
    }

ret:
    return 0;
}


static inline uerr_t get_data_connection(dinfo* info,
                                         co_param_ftp* param,
                                         ftp_connection** pconn)
{
    ftp_connection* conn = ZALLOC1(ftp_connection);
    /* Login to the server: */
    conn->conn = connection_get(info->ui);
    if (!conn->conn)
    {
        return FTPRERR;
    }

    uerr_t err = ftp_login (conn, info->md->ptrs->user, info->md->ptrs->passwd);
    /* FTPRERR, FTPSRVERR, WRITEFAILED, FTPLOGREFUSED, FTPLOGINC */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
        case FTPSRVERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("Error in server greeting.\n"));
            return err;
        case WRITEFAILED:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET,
                     _("Write failed, closing control ftp_connection.\n"));
            return err;
        case FTPLOGREFUSED:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("The server refuses login.\n"));
            return FTPLOGREFUSED;
        case FTPLOGINC:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("Login incorrect.\n"));
            return FTPLOGINC;
        case FTPOK:
            /* if (!opt.server_response) */
            /*     logputs (LOG_VERBOSE, _("Logged in!\n")); */
            break;
        default:
            abort ();
    }

    /* Third: Get the system type */
    err = ftp_syst (conn, &conn->rs);
    /* FTPRERR */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return err;
        case FTPSRVERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET,
                     _("Server error, can't determine system type.\n"));
            break;
        case FTPOK:
            /* Everything is OK.  */
            break;
        default:
            abort ();
    }

    /* Fifth: Set the FTP type.  */
    char type_char = ftp_process_type (NULL);
    logprintf (LOG_VERBOSE, "==> TYPE %c ... ", type_char);
    err = ftp_type (conn, type_char);
    /* FTPRERR, WRITEFAILED, FTPUNKNOWNTYPE */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return err;
        case WRITEFAILED:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET,
                     _("Write failed, closing control ftp_connection.\n"));
            return err;
        case FTPUNKNOWNTYPE:
            logputs (LOG_VERBOSE, "\n");
            logprintf (LOG_NOTQUIET,
                       _("Unknown type `%c', closing control ftp_connection.\n"),
                       type_char);
            return err;
        case FTPOK:
            /* Everything is OK.  */
            break;
        default:
            abort ();
    }

    uint64 size = 0;
    err = ftp_size (conn, info->ui->uri, (wgint*)&size);
    /* FTPRERR */
    switch (err)
    {
        case FTPRERR:
        case FTPSRVERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return err;
        case FTPOK:
            /* got_expected_bytes = true; */
            /* Everything is OK.  */
            break;
        default:
            abort ();
    }

    if (size != info->md->hd.package_size) {
        abort();
    }

    uint64 offset = param->idx * param->md->hd.chunk_size +
                    param->dp->cur_pos - param->dp->start_pos;
    err = ftp_rest (conn, offset);

    /* FTPRERR, WRITEFAILED, FTPRESTFAIL */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return err;
        case WRITEFAILED:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET,
                     _("Write failed, closing control ftp_connection.\n"));
            return err;
        case FTPRESTFAIL:
            logputs (LOG_VERBOSE, _("\nREST failed, starting from scratch.\n"));
            break;
        case FTPOK:
            break;
        default:
            abort ();
    }

    //TODO: Add support to PORT mode.
    ip_address passive_addr;
    int        passive_port;
    err = ftp_pasv (conn, &passive_addr, &passive_port);
    /* FTPRERR, WRITEFAILED, FTPNOPASV, FTPINVPASV */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return err;
        case WRITEFAILED:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET,
                     _("Write failed, closing control ftp_connection.\n"));
            return err;
        case FTPNOPASV:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("Cannot initiate PASV transfer.\n"));
            break;
        case FTPINVPASV:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("Cannot parse PASV response.\n"));
            break;
        case FTPOK:
            break;
        default:
            abort ();
    }   /* switch (err) */

    url_info ui;
    memset(&ui, 0, sizeof(url_info));
    ui.eprotocol = UP_FTP;
    ui.addr = &passive_addr;
    ui.port = passive_port;

    PDEBUG ("A0: %p\n", ui.addr);

    conn->data_conn = connection_get(&ui);
    if (!conn->data_conn)
    {
        PDEBUG ("Failed to get connection!\n");

        return FTPRERR;
    }


    *pconn = conn;
    return FTPOK;
}

void* ftp_download_thread(void* arg)
{
    co_param_ftp* param = (co_param_ftp*)arg;
    if (!arg)
    {
        pthread_exit(0);
        return NULL;
    }

    (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    (void)pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    ftp_connection* conn = NULL;
    uerr_t err = get_data_connection(param->info, param, &conn);
    data_chunk *dp    = (data_chunk *) param->dp;
    if (!conn || err != FTPOK)
    {
        fprintf (stderr, "OOPS: %p, err: %d\n", conn, (int)err);
        goto ret;
    }

    err = ftp_retr (conn, param->info->ui->uri);
    /* FTPRERR, WRITEFAILED, FTPNSFOD */
    switch (err)
    {
        case FTPRERR:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET, _("\
Error in server response, closing control ftp_connection.\n"));
            return NULL;
        case WRITEFAILED:
            logputs (LOG_VERBOSE, "\n");
            logputs (LOG_NOTQUIET,
                     _("Write failed, closing control ftp_connection.\n"));
            return NULL;
        case FTPNSFOD:
            logputs (LOG_VERBOSE, "\n");
            logprintf (LOG_NOTQUIET, _("No such file.\n\n"));
        case FTPOK:
            break;
        default:
            abort ();
    }

    int i = 0;
    PDEBUG ("chunk_info: base: %p, cur: %08llX, start: %08llX, end_pos: %08llX\n",
            param->addr, dp->cur_pos, dp->start_pos, dp->end_pos);

    while (dp->cur_pos < dp->end_pos) {
        void *addr = param->addr + dp->cur_pos;
        int rd = conn->data_conn->co.read(conn->data_conn,
                                          param->addr + dp->cur_pos,
                                          dp->end_pos - dp->cur_pos, NULL);
        if (rd > 0) {
            dp->cur_pos += rd;
            write(param->fds[1], "R", 1);
        } else if (rd == 0) {
            PDEBUG("Read returns 0: showing chunk: "
                   "retuned zero: dp: %p : %llX -- %llX\n",
                   dp, dp->cur_pos, dp->end_pos);
        } else {
            PDEBUG("read returns %d\n", rd);
            if (errno != EAGAIN) {
                fprintf(stderr, "read returns %d: %s\n", rd, strerror(errno));
                rd = COF_AGAIN;
            }
        }

        if (dp->cur_pos >= dp->end_pos) {
            PDEBUG("Finished chunk: %p\n", dp);
            rd = COF_FINISHED;
            write(param->fds[1], "E", 1);
            break;
        } else if (!rd) {
            rd = COF_CLOSED;
            write(param->fds[1], "E", 1);
            PDEBUG("retuned zero: dp: %p : %llX -- %llX\n",
                   dp, dp->cur_pos, dp->end_pos);
            break;
        }
    }

    PDEBUG ("chunk_info: base: %p, cur: %08llX, start: %08llX, end_pos: %08llX\n",
            param->addr, dp->cur_pos, dp->start_pos, dp->end_pos);

ret:
    return NULL;
}

#define MAX(X,Y)       X > Y ? X : Y

mget_err process_ftp_request(dinfo* info,
                             dp_callback cb,
                             bool* cflag,
                             void* user_data)
{
    PDEBUG("enter\n");

    url_info*   ui   = info ? info->ui : NULL;
    if (dinfo_ready(info))
        goto start;

    PDEBUG ("C: %p -- %p\n",info->md->ptrs->user, info->md->ptrs->passwd);

    if (!info->md->ptrs->user)
    {
        info->md->ptrs->user = strdup("ftp");
    }
    if (!info->md->ptrs->passwd)
    {
        info->md->ptrs->passwd = strdup("ftp");
    }

    ftp_connection* fconn = ZALLOC1(ftp_connection);
    fconn->conn = connection_get(info->ui);
    if (!fconn->conn) {
        fprintf(stderr, "Failed to get socket!\n");
        return ME_CONN_ERR;
    }

    bool can_split = true;
    uint64 total_size = 0;

    uerr_t err = get_remote_file_size_ftp(info, fconn, &can_split, &total_size);

    if (!total_size) {
        fprintf(stderr, "Can't get remote file size: %s\n", ui->furl);
        return ME_RES_ERR;
    }

    if (!can_split)
    {
        fprintf(stderr, "Can't split.....\n");
    }

    PDEBUG("total_size: %llu\n", total_size);

    /*
      If it goes here, means metadata is not ready, get nc (number of
      connections) from download_info, and recreate metadata.
    */

    if (info->md->hd.nr_user == 0xff)
    {
        info->md->hd.nr_user = DEFAULT_FTP_CONNECTIONS;
    }

    if (!dinfo_update_metadata(info, total_size, NULL)) {
        fprintf(stderr, "Failed to create metadata from url: %s\n", ui->furl);
        return ME_ABORT;
    }

    PDEBUG("metadata created from url: %s\n", ui->furl);

start: ;
    metadata* md = info->md;
    metadata_display(md);

    dinfo_sync(info);

    if (md->hd.status == RS_FINISHED) {
        goto ret;
    }

    md->hd.status = RS_STARTED;
    if (cb) {
        (*cb) (md, user_data);
    }

    bool             need_request = false;
    ftp_connection*  conn   = NULL;
    ftp_connection*  conns  = ZALLOC(ftp_connection, md->hd.nr_effective);
    pthread_t*       tids   = ZALLOC(pthread_t, md->hd.nr_effective);
    uerr_t           uerr   = FTPOK;
    data_chunk      *dp     = md->ptrs->body;
    int              maxfd  = 0;
    co_param_ftp*    params = ZALLOC(co_param_ftp, md->hd.nr_effective);
    co_param_ftp    *param  = params;
    int thread_number = 0;

    fd_set rfds;
    FD_ZERO(&rfds);

    // ftp protocol asks us to interact multiple times before opening data
    // connections, start different threads for them.
    for (int i = 0; i < md->hd.nr_effective; ++i, ++dp, ++param) {
        PDEBUG ("i = %d\n", i);

        if (dp->cur_pos >= dp->end_pos) {
            continue;
        }

        thread_number ++;
        need_request = true;
        conn = conns++;
        param->addr      = info->fm_file->addr;
        param->idx       = i;
        param->dp        = dp;
        param->ui        = ui;
        param->md        = md;
        param->cb        = cb;
        param->user_data = user_data;
        param->info = info;
        if (pipe(param->fds) == -1)
        {
            fprintf(stderr, "Failed to create pipe: %s\n", strerror(errno));
            return ME_GENERIC;
        }

        fcntl(param->fds[0], F_SETFD, O_NONBLOCK);
        maxfd = MAX(maxfd, param->fds[0]);
        PDEBUG ("max: %d, params->fds[0] = %d\n", maxfd, param->fds[0]);

        FD_SET(param->fds[0], &rfds);
        int tr = pthread_create(tids+i, NULL, ftp_download_thread, param);
        if (tr < 0)
        {
            perror("Failed to create threads!");
            exit(1);
        }
    }

    maxfd++;

    PDEBUG ("cflag: %p -- %d, thread_number: %d, maxfd: %d\n",
            cflag, *cflag, thread_number, maxfd);

    if (!thread_number)
        goto check_status;

    int active_threads = thread_number;
    struct timeval tv;
    while (!*cflag && active_threads) {
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int nfds = select(maxfd, &rfds, NULL, NULL, &tv);
        if (nfds == -1) {
            perror("select fail:");
            break;
        }
        for (int i = 0; i < md->hd.nr_effective; i++) {
            param = params + i;
            int fd = param->fds[0];
            if (!nfds)
            {
                if (param->cb)
                {
                    (*(param->cb)) (param->md, param->user_data);
                }
                FD_SET(fd, &rfds);
            }

            else if (FD_ISSET(fd, &rfds)) {
                static char buff[64];
                memset(buff, 0, 64);
                int rd = read(fd, buff, 64);
                if (rd == -1)
                {
                    PDEBUG ("rd : %d, %d: %s\n",
                            rd, errno, strerror(errno));
                    ;
                }
                else if (!rd)
                {
                    ;
                }
                else
                {
                    for (int j = 0; j < fd; j++) {
                        switch (buff[j])
                        {
                            case 'R':
                            {
                                (*(param->cb)) (param->md, param->user_data);
                                if (param->cb) {
                                    (*(param->cb)) (param->md, param->user_data);
                                }
                                FD_SET(fd, &rfds);
                                break;
                            }
                            case 'E':
                            {
                                FD_CLR(fd, &rfds);
                                active_threads --;
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                }
            }
        }

  loop:
        if (*cflag) {
            fprintf(stderr, "Stop because control_flag set to 1!!!\n");
            break;
        }
    }

    for (int i = 0; i < thread_number; i++) {
        (void)pthread_cancel(tids[i]);
    }

    for (int i = 0; i < thread_number; i++) {
        int tr = pthread_join(tids[i], NULL);
        if (tr < 0)
        {
            fprintf(stderr, "failed to join thread\n");
        }
    }

    if (!need_request) {
        md->hd.status = RS_FINISHED;
        goto ret;
    }

    dinfo_sync(info);
    sleep(1);

check_status:
    dp = md->ptrs->body;
    bool finished = true;

    for (int i = 0; i < CHUNK_NUM(md); ++i, ++dp) {
        if (dp->cur_pos < dp->end_pos) {
            finished = false;
            break;
        }
    }

    if (finished) {
        md->hd.status = RS_FINISHED;
    } else {
        md->hd.status = RS_PAUSED;
    }

    md->hd.acc_time += get_time_s() - md->hd.last_time;

ret:
    metadata_display(md);
    if (cb) {
        (*cb) (md, user_data);
    }

    PDEBUG("stopped.\n");
    return ME_OK;
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
