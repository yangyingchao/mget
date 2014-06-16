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

#include "ftp.h"
#include "connection.h"
#include "log.h"
#include <unistd.h>
#include <errno.h>
#include "data_utlis.h"
#include "metadata.h"
#include "mget_utils.h"
#include "wftp.h"

#define DEFAULT_FTP_CONNECTIONS       8

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
    logprintf (LOG_VERBOSE, "Logging in as %s ... ", info->md->user);

    uerr_t err = ftp_login (conn, info->md->user, info->md->passwd);

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


ret:
    return 0;
}

typedef struct _connection_operation_param_ftp {
    void *addr;			//base addr;
    data_chunk *dp;
    url_info *ui;
    hash_table *ht;
    void (*cb) (metadata*, void*);
    metadata *md;
    void* user_data;
} co_param_ftp;

int ftp_read_sock(connection * conn, void *priv)
{
    if (!priv) {
        return -1;
    }

    //todo: Ensure we read all data stored in this connection.
    co_param_ftp   *param = (co_param_ftp *) priv;
    data_chunk *dp    = (data_chunk *) param->dp;

    if (dp->cur_pos >= dp->end_pos)  {
        return 0;
    }

    void *addr = param->addr + dp->cur_pos;
    int rd;
    do {
        rd = conn->ci.reader(conn, param->addr + dp->cur_pos,
                             dp->end_pos - dp->cur_pos, NULL);
    } while (rd == -1 && errno == EINTR);
    if (rd > 0) {
        dp->cur_pos += rd;
        if (param->cb) {
            (*(param->cb)) (param->md, param->user_data);
        }
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
    } else if (!rd) {
        rd = COF_CLOSED;
        PDEBUG("retuned zero: dp: %p : %llX -- %llX\n",
               dp, dp->cur_pos, dp->end_pos);
    }

    return rd;
}

int ftp_write_sock(connection * conn, void *priv)
{
    PDEBUG("enter\n");
    if (!priv) {
        return -1;
    }
// TODO: Remove this ifdef!
#if 0
    co_param_ftp *cp = (co_param_ftp *) priv;
    char     *hd = generate_request_header("GET", cp->ui, cp->dp->cur_pos,
                                           cp->dp->end_pos);
    size_t written = conn->ci.writer(conn, hd, strlen(hd), NULL);

    free(hd);
    return written;
#endif // End of #if 0

    return 0;
}

int process_ftp_request(dinfo* info,
                        dp_callback cb,
                        bool* stop_flag,
                        void* user_data)
{
    PDEBUG("enter\n");

    url_info*   ui   = info ? info->ui : NULL;
    if (dinfo_ready(info))
        goto start;

    // XXX: set default username and password
    if (!info->md->user)
    {
        info->md->user = "anonymous";
    }
    if (!info->md->passwd)
    {
        info->md->passwd = "anonymous";
    }

    ftp_connection* fconn = ZALLOC1(ftp_connection);
    fconn->conn = connection_get(info->ui);
    if (!fconn->conn) {
        fprintf(stderr, "Failed to get socket!\n");
        return -1;
    }

    bool can_split = true;
    uint64 total_size = 0;

    uerr_t err = get_remote_file_size_ftp(info, fconn, &can_split, &total_size);

    if (!total_size) {
        fprintf(stderr, "Can't get remote file size: %s\n", ui->furl);
        return -1;
    }

    PDEBUG("total_size: %llu\n", total_size);

    /*
      If it goes here, means metadata is not ready, get nc (number of
      connections) from download_info, and recreate metadata.
    */

    int nc = info->md ? info->md->hd.nr_user : DEFAULT_FTP_CONNECTIONS;
    if (!dinfo_update_metadata(total_size, info)) {
        fprintf(stderr, "Failed to create metadata from url: %s\n", ui->furl);
        return -1;
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

    connection_group *sg = connection_group_create(stop_flag);

    if (!sg) {
        fprintf(stderr, "Failed to craete sock group.\n");
        return -1;
    }

    bool need_request = false;
    connection* conn = NULL;

    for (int i = 0; i < md->hd.nr_effective; ++i) {
        data_chunk *dp = &md->body[i];

        if (dp->cur_pos >= dp->end_pos) {
            continue;
        }

        need_request = true;

        if (!conn) {
            conn = connection_get(ui);
            if (!conn) {
                fprintf(stderr, "Failed to create connection!!\n");
                goto ret;
            }
        }

        co_param_ftp *param = ZALLOC1(co_param_ftp);

        param->addr      = info->fm_file->addr;
        param->dp        = md->body + i;
        param->ui        = ui;
        param->md        = md;
        param->cb        = cb;
        param->user_data = user_data;

        conn->rf = ftp_read_sock;
        conn->wf = ftp_write_sock;
        conn->priv = param;

        connection_add_to_group(sg, conn);
        conn = NULL;
    }

    if (!need_request) {
        md->hd.status = RS_FINISHED;
        goto ret;
    }

    PDEBUG("Performing...\n");
    int ret = connection_perform(sg);
    PDEBUG("ret = %d\n", ret);

    dinfo_sync(info);

    data_chunk *dp = md->body;
    bool finished = true;

    for (int i = 0; i < CHUNK_NUM(md); ++i) {
        if (dp->cur_pos < dp->end_pos) {
            finished = false;
            break;
        }
        dp++;
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
    return 0;
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
