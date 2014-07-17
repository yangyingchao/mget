/** mget_http.c --- implementation of libmget using http
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
#include "http.h"
#include "connection.h"
#include "log.h"
#include <unistd.h>
#include <errno.h>
#include "data_utlis.h"
#include "metadata.h"
#include "mget_utils.h"

#define DEFAULT_HTTP_CONNECTIONS 5
#define BUF_SIZE                 4096
#define SIZE                     1024

static const char* HEADER_END = "\r\n\r\n";


static inline char *generate_request_header(const char* method, url_info* uri,
                                            uint64 start_pos, uint64 end_pos)
{
    static char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);

    sprintf(buffer,
            "%s %s HTTP/1.1\r\nHost: %s\r\nAccept: *\r\nRange: bytes=%llu-%llu\r\n\r\n",
            method, uri->uri, uri->host, start_pos, end_pos);

    return strdup(buffer);
}

//@todo: should make sure header is complete before dissecting header!
int dissect_header(byte_queue* bq, hash_table ** ht)
{
    if (!ht || !bq || !bq->r) {
        return -1;
    }

    const char *buffer = bq->r;
    size_t length = bq->w - bq->r;
    char *fptr = strstr(buffer, HEADER_END);
    if (!fptr)  {
        fprintf(stderr,
                "Should only dissect header when header is complete\n");
        abort();
    }

#ifdef DEBUG
    size_t h_len = fptr - (char*)bq->r + 4;
    char* bf = ZALLOC(char, h_len);
    PDEBUG ("buffer: %s\n", strncpy(bf, bq->r, h_len-1));
    FIF(bf);
#endif

    hash_table *pht = hash_table_create(256, free);
    if (!pht) {
        abort();
    }

    *ht = pht;

    const char *ptr    = buffer;
    int         n      = 0;
    int         num    = 0;
    int         stat   = 0;
    char value[64]     = { '\0' };
    size_t      ldsize = 0;

    num = sscanf(ptr, "HTTP/1.1 %[^\r\n]\r\n%n", value, &n);
    if (!num) {
        fprintf(stderr, "Failed to parse header: %s\n", ptr);
        perror("Failed to parse header");
        return -1;
    }

    char *key = "status";

    hash_table_insert(pht, key, strdup(value), strlen(value));
    ptr += n;

    num = sscanf(value, "%d", &stat);
    assert(num);

    char k[256] = { '\0' };
    char v[256] = { '\0' };
    while ((ptr < buffer + length) && fptr && ptr < fptr) {
        memset(k, 0, 256);
        memset(v, 0, 256);
        if (sscanf((const char *) ptr, "%[^:]: %[^\r\n]\r\n%n", k, v, &n)) {
            lowwer_case(k, strlen(k));
            hash_table_insert(pht, k, strdup(v), strlen(v));
            ldsize += n;
            ptr += n;
        }
    }

    bq->r = fptr + 4;

    return stat;
}

/* This function accepts an pointer of connection pointer, on return. When 302
 * is detected, it will modify both ui and conn to ensure a valid connection
 * can be initialized. */
uint64 get_remote_file_size_http(url_info* ui, connection** conn)
{
    if (!conn ||!*conn || !ui) {
        return 0;
    }

    PDEBUG ("enter\n");

    char *hd = generate_request_header("GET", ui, 0, 0);

    (*conn)->ci.writer((*conn), hd, strlen(hd), NULL);
    free(hd);

    PDEBUG ("reading...\n");

    // we are in blocking mode for now.
    byte_queue* bq = bq_init(SIZE);
    char* eptr = NULL;
    int i = 1;
    do {
        bq         = bq_resize(bq, SIZE);
        size_t rd  = (*conn)->ci.reader((*conn), bq->w, bq->x - bq->w, NULL);
        bq->w     += rd;
    } while ((eptr = strstr(bq->r, HEADER_END)) == NULL);

    hash_table *ht   = NULL;
    int         stat = dissect_header(bq, &ht);

    PDEBUG("stat: %d, description: %s\n",
           stat, (char *) hash_table_entry_get(ht, "status"));

    int    num = 0;
    char*  ptr = NULL;
    uint64 t   = 0;

    switch (stat) {
        case 206:			// Ok, we can start download now.
        {
            ptr = (char *) hash_table_entry_get(ht, "content-range");
            if (!ptr) {
                fprintf(stderr, "Content Range not returned: %s!\n", bq->p);
                t = 0;
                goto ret;
            }

            PDEBUG("Range:: %s\n", ptr);

            uint64 s, e;
            num = sscanf(ptr, "bytes %llu-%llu/%llu",
                         &s, &e, &t);
            break;
        }
        case 302:			// Resource moved to other place.
        {
            char *loc = (char *) hash_table_entry_get(ht, "location");

            printf("Server returns 302, trying new locations: %s...\n",
                   loc);
            url_info *nui = NULL;

            if (loc && parse_url(loc, &nui)) {
                url_info_copy(ui, nui);
                url_info_destroy(&nui);
                connection_put(*conn);
                *conn = connection_get(ui);
                return get_remote_file_size_http(ui, conn);
            }
            fprintf(stderr,
                    "Failed to get new location for status code: 302\n");
            break;
        }
        case 200:
        {
            ptr = (char *) hash_table_entry_get(ht, "Content-Length");
            if (!ptr) {
                fprintf(stderr, "Content Length not returned!\n");
                t = 0;
                goto ret;
            }

            PDEBUG("Content-Length: %s\n", ptr);

            num = sscanf(ptr, "%llu", &t);

            break;
        }
        default:
        {
            fprintf(stderr, "Not implemented for status code: %d\n", stat);
            fprintf(stderr, "Response Header\n%s\n", bq->p);
            goto ret;
        }
    }

    if (!num) {
        fprintf(stderr, "Failed to parse string: %s\n", ptr);
    } else {
        // Check http headers and update connection_features....
    }

ret:
    bq_destroy(&bq);
    return t;
}

typedef struct _connection_operation_param {
    void *addr;			//base addr;
    data_chunk *dp;
    url_info *ui;
    bool header_finished;
    byte_queue* bq;
    hash_table *ht;
    void (*cb) (metadata*, void*);
    metadata *md;
    void* user_data;
} co_param;

int http_read_sock(connection * conn, void *priv)
{
    if (!priv) {
        return -1;
    }
    //todo: Ensure we read all data stored in this connection.
    co_param   *param = (co_param *) priv;
    data_chunk *dp    = (data_chunk *) param->dp;

    if (dp->cur_pos >= dp->end_pos)  {
        return 0;
    }

    void *addr = param->addr + dp->cur_pos;
    if (!param->header_finished) {
        size_t rd = conn->ci.reader(conn, param->bq->w,
                                    param->bq->x - param->bq->w, NULL);

        if (rd) {
            param->bq->w += rd;
            char *ptr = strstr(param->bq->r, "\r\n\r\n");
            if (ptr == NULL) {
                // Header not complete, store and return positive value.
                return 1;
            }

            size_t ds = (byte*)ptr - param->bq->r + 4;
            int    r  = dissect_header(param->bq, &param->ht);

            if (r != 206 && r != 200) { /* Some server returns 200?? */
                fprintf(stderr, "status code is %d!\n", r);
                exit(1);
            }

            ptr += 4;		// adjust to the tail of "\r\n\r\n"

            if ((byte*)ptr != param->bq->r)
            {
                fprintf(stderr,
                        "Pointer is not at the beginning of http body!!\n");
                abort();
            }

            param->header_finished = true;

            size_t length = param->bq->w - param->bq->r;
            PDEBUG ("LEN: %ld, bq->w - bq->r: %ld\n", length,
                    param->bq->w - param->bq->r);
            if (length) {
                memcpy(addr, param->bq->r, length);
                dp->cur_pos += length;
            }
            bq_destroy(&param->bq);
        }
    }

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

int http_write_sock(connection * conn, void *priv)
{
    PDEBUG("enter\n");
    if (!priv) {
        return -1;
    }

    co_param *cp = (co_param *) priv;
    char     *hd = generate_request_header("GET", cp->ui, cp->dp->cur_pos,
                                           cp->dp->end_pos);
    size_t written = conn->ci.writer(conn, hd, strlen(hd), NULL);

    free(hd);
    return written;
}

mget_err process_http_request(dinfo* info, dp_callback cb, bool * stop_flag,
                              void* user_data)
{
    PDEBUG("enter\n");

    connection *conn = NULL;
    url_info*   ui   = info ? info->ui : NULL;
    if (dinfo_ready(info))
        goto start;

    conn = connection_get(info->ui);
    if (!conn) {
        fprintf(stderr, "Failed to get socket!\n");
        return ME_CONN_ERR;
    }
    PDEBUG ("conn : %p\n", conn);


    uint64 total_size = get_remote_file_size_http(info->ui, &conn);

    if (!total_size) {
        fprintf(stderr, "Can't get remote file size: %s\n", ui->furl);
        return ME_RES_ERR;
    }

    PDEBUG("total_size: %llu\n", total_size);

    /*
      If it goes here, means metadata is not ready, get nc (number of
      connections) from download_info, and recreate metadata.
    */

    int nc = info->md ? info->md->hd.nr_user : DEFAULT_HTTP_CONNECTIONS;
    if (!dinfo_update_metadata(total_size, info)) {
        fprintf(stderr, "Failed to create metadata from url: %s\n", ui->furl);
        return ME_ABORT;
    }

    PDEBUG("metadata created from url: %s\n", ui->furl);

start: ;
    metadata* md = info->md;
    metadata_display(md);

restart:
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
        return ME_GENERIC;
    }

    bool need_request = false;
    data_chunk *dp = md->ptrs->body;

    for (int i = 0; i < md->hd.nr_effective; ++i, ++dp) {
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

        co_param *param = ZALLOC1(co_param);

        param->addr      = info->fm_file->addr;
        param->dp        = dp;
        param->ui        = ui;
        param->md        = md;
        param->bq        = bq_init(SIZE);
        param->cb        = cb;
        param->user_data = user_data;

        conn->rf = http_read_sock;
        conn->wf = http_write_sock;
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

    dp = md->ptrs->body;
    bool finished = true;

    for (int i = 0; i < CHUNK_NUM(md); ++i, ++dp) {
        if (dp->cur_pos < dp->end_pos) {
            finished = false;
            break;
        }
    }

    if (!finished && stop_flag && !*stop_flag) // errors occurred, restart
    {
        goto restart;
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
