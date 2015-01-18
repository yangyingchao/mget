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
#include "../../logutils.h"
#include "http.h"
#include "../../connection.h"
#include <unistd.h>
#include <errno.h>
#include "../../data_utlis.h"
#include "../../metadata.h"
#include "../../mget_utils.h"

#define DEFAULT_HTTP_CONNECTIONS 5
#define PAGE                     4096

static const char *HEADER_END = "\r\n\r\n";
static const char *CAN_SPLIT = "can_split";

// @todo: move this param into src/lib/protocol when more protocols are added.
typedef struct _connection_operation_param {
    void *addr;                 //base addr;
    data_chunk *dp;
    url_info *ui;
    bool header_finished;
    byte_queue *bq;
    hash_table *ht;
    void (*cb) (metadata *, void *);
    metadata *md;
    dinfo *info;
    void *user_data;
} co_param;



static inline char *generate_request_header(const char *method,
                                            url_info * uri,
                                            uint64 start_pos,
                                            uint64 end_pos);
static inline int dissect_header(byte_queue * bq, hash_table ** ht);
static inline uint64 get_remote_file_size_http(url_info * ui,
                                               byte_queue * bq,
                                               connection ** conn,
                                               hash_table ** ht);



int http_read_sock(connection * conn, void *priv)
{
    if (!priv) {
        return -1;
    }
    //todo: Ensure we read all data stored in this connection.
    co_param *param = (co_param *) priv;
    data_chunk *dp = (data_chunk *) param->dp;

    if (dp->cur_pos >= dp->end_pos) {
        return 0;
    }

    void *addr = param->addr + dp->cur_pos;
    if (!param->header_finished) {
        bq_enlarge(param->bq, 4096 * 100);
        int rd = conn->co.read(conn, param->bq->w,
                               param->bq->x - param->bq->w, NULL);

        if (rd > 0) {
            param->bq->w += rd;
            char *ptr = strstr(param->bq->r, "\r\n\r\n");
            if (ptr == NULL) {
                // Header not complete, store and return positive value.
                return 1;
            }

            int r = dissect_header(param->bq, &param->ht);
            switch (r) {
            case 206:
            case 200:{
                    break;
                }
            case 301:
            case 302:          // Resource moved to other place.
            case 307:{
                    char *loc = (char *) hash_table_entry_get(param->ht,
                                                              "location");
                    if (dinfo_update_url(param->info, loc)) {
                        mlog(LL_ALWAYS, "url updated to :%s\n", loc);
                        return COF_CLOSED;
                    } else {
                        printf("Server returns 302, but failed to"
                               " update metadata, please delete all files "
                               " and try again...\n"
                               "New locations: %s...\n", loc);
                        exit(1);
                    }
                    break;
                }
            default:{
                    fprintf(stderr, "Error occurred, status code is %d!\n",
                            r);
                    exit(1);
                }
            }

            ptr += 4;           // adjust to the tail of "\r\n\r\n"

            if ((byte *) ptr != param->bq->r) {
                fprintf(stderr,
                        "Pointer is not at the beginning of http body!!\n");
                abort();
            }

            param->header_finished = true;

            size_t length = param->bq->w - param->bq->r;
            PDEBUG("LEN: %ld, bq->w - bq->r: %ld\n", length,
                   param->bq->w - param->bq->r);
            if (length) {
                memcpy(addr, param->bq->r, length);
                dp->cur_pos += length;
            }
            PDEBUG("Showing chunk: "
                   "dp: %p : %llX -- %llX\n",
                   dp, dp->cur_pos, dp->end_pos);

            bq_destroy(&param->bq);
            if (dp->cur_pos == dp->end_pos)
                goto ret;
            else if (dp->cur_pos == dp->end_pos)
                mlog(LL_ALWAYS, "Wrong data: dp: %p : %llX -- %llX\n",
                     dp, dp->cur_pos, dp->end_pos);

        } else if (rd == 0) {
            PDEBUG("Read returns 0: showing chunk: "
                   "retuned zero: dp: %p : %llX -- %llX\n",
                   dp, dp->cur_pos, dp->end_pos);
            return rd;
        } else {
            mlog(LL_NONVERBOSE, "Read returns -1: showing chunk: "
                 "retuned zero: dp: %p : %llX -- %llX\n",
                 dp, dp->cur_pos, dp->end_pos);
            if (errno != EAGAIN) {
                mlog(LL_ALWAYS, "read returns %d: %s\n", rd,
                     strerror(errno));
                rd = COF_ABORT;
                exit(1);
                metadata_display(param->md);
                return rd;
            }
        }
    }

    PDEBUG("here...\n");

    int rd = 0;
    do {
        rd = conn->co.read(conn, param->addr + dp->cur_pos,
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
            mlog(LL_ALWAYS, "read returns %d: %s\n", rd, strerror(errno));
            rd = COF_ABORT;
            metadata_display(param->md);
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

  ret:
    return rd;
}

int http_write_sock(connection * conn, void *priv)
{
    PDEBUG("enter\n");
    if (!priv) {
        PDEBUG("no priv..\n");
        return -1;
    }

    co_param *cp = (co_param *) priv;
    char *hd = generate_request_header("GET", cp->ui, cp->dp->cur_pos,
                                       cp->dp->end_pos);
    size_t written = conn->co.write(conn, hd, strlen(hd), NULL);

    PDEBUG("written: %d\n", written);

    free(hd);
    return COF_FINISHED;
}

mget_err process_http_request(dinfo * info, dp_callback cb,
                              bool * stop_flag, void *user_data)
{
    PDEBUG("enter\n");

    connection *conn = NULL;
    url_info *ui = info ? info->ui : NULL;
    if (dinfo_ready(info))
        goto start;

    conn = connection_get(info->ui);
    if (!conn) {
        fprintf(stderr, "Failed to get socket!\n");
        return ME_CONN_ERR;
    }
    PDEBUG("conn : %p\n", conn);

    hash_table *ht = NULL;
    byte_queue *bq = bq_init(PAGE);
    uint64 total = get_remote_file_size_http(info->ui, bq, &conn,
                                             &ht);

    if (!total) {
        fprintf(stderr, "Can't get remote file size: %s\n", ui->furl);
        fprintf(stderr, "It won't help without knowning remote file size,"
                " Please use wget instead...\");
        return ME_RES_ERR;
    }

    bq_destroy(&bq);

    // try to get file name from http header..
    char *fn = NULL;
    if (info->md->hd.update_name) {
        char *dis = hash_table_entry_get(ht, "content-disposition");
        PDEBUG("updating name based on disposition: %s\n", dis);

        if (dis) {
            fn = ZALLOC(char, strlen(dis));
            char *tmp = ZALLOC(char, strlen(dis));
            (void) sscanf(dis, "%*[^;];filename=%s", tmp);

            // some server may add whitespace between ";" and "filename"..
            if (!*tmp)
                (void) sscanf(dis, "%*[^;];%*[ ]filename=%s", tmp);

            // file name is encoded...
            int idx = 0;
            char *ptr = tmp;
            char *end = ptr + strlen(ptr);
            while (ptr < end) {
                if (*ptr == '%') {
                    int X = 0;
                    int n = 0;
                    sscanf(ptr, "%%%2X%n", &X, &n);
                    fn[idx++] = X;
                    ptr += n;
                } else if (*ptr == '"')
                    ptr++;
                else {
                    fn[idx++] = *ptr;
                    ptr++;
                }
            }
            FIF(tmp);

            if (fn && !(*fn)) {
                mlog(LL_NOTQUIET, "Sadly, we can't parse filename: %s\n",
                     dis);
                FIFZ(&fn)
            } else {
                mlog(LL_ALWAYS, "Renaming file name to: %s\n", fn);
            }
        }
    }

    PDEBUG("total: %" PRIu64 ", fileName: %s\n", total, fn);

    /*
       If it goes here, means metadata is not ready, get nc (number of
       connections) from download_info, and recreate metadata.
     */

    if (info->md->hd.nr_user == 0xff) {
        info->md->hd.nr_user = DEFAULT_HTTP_CONNECTIONS;
    }

    if (!hash_table_entry_get(ht, CAN_SPLIT)) {
        info->md->hd.nr_user = 1;
    }

    if (!dinfo_update_metadata(info, total, fn)) {
        fprintf(stderr, "Failed to create metadata from url: %s\n",
                ui->furl);
        return ME_ABORT;
    }

    PDEBUG("metadata created from url: %s\n", ui->furl);

  start:;
    metadata *md = info->md;
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

    connection_group *sg = connection_group_create(cg_all, stop_flag);

    if (!sg) {
        fprintf(stderr, "Failed to create sock group.\n");
        return ME_GENERIC;
    }

    bool need_request = false;
    data_chunk *dp = md->ptrs->body;
    conn = NULL;                // leak...

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
        param->info      = info;
        param->bq        = bq_init(PAGE);
        param->cb        = cb;
        param->user_data = user_data;

        conn->recv_data  = http_read_sock;
        conn->write_data = http_write_sock;
        conn->priv       = param;

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

    connection_group_destroy(sg);
    if (!finished && stop_flag && !*stop_flag) {        // errors occurred, restart
        goto restart;
    }

    if (finished) {
        md->hd.status = RS_FINISHED;
    } else {
        md->hd.status = RS_PAUSED;
        if (stop_flag && *stop_flag) {
            ret = ME_ABORT;
        }
    }

    md->hd.acc_time += get_time_s() - md->hd.last_time;

  ret:
    metadata_display(md);
    if (cb) {
        (*cb) (md, user_data);
    }

    PDEBUG("stopped, ret: %d.\n", ME_OK);
    return ME_OK;
}


static inline char *generate_request_header(const char *method,
                                            url_info * uri,
                                            uint64 start_pos,
                                            uint64 end_pos)
{
    static char buffer[PAGE];
    memset(buffer, 0, PAGE);

    sprintf(buffer,
            "%s %s HTTP/1.1\r\nHost: %s\r\n"
            "Accept: *\r\n"
            "Connection: Keep-Alive\r\n"
            "Keep-Alive: timeout=600\r\n"
            "Range: bytes=%" PRIu64 "-%" PRIu64 "\r\n\r\n",
            method, uri->uri, uri->host, start_pos, end_pos);

    return strdup(buffer);
}

static inline int dissect_header(byte_queue * bq, hash_table ** ht)
{
    if (!ht || !bq || !bq->r) {
        return -1;
    }

    char *fptr = strstr(bq->r, HEADER_END);
    if (!fptr) {
        fprintf(stderr,
                "Should only dissect header when header is complete\n");
        abort();
    }
#ifdef DEBUG
    size_t h_len = fptr - (char *) bq->r + 4;
    char *bf = ZALLOC(char, h_len);
    PDEBUG("buffer: %s\n", strncpy(bf, bq->r, h_len - 1));
    FIF(bf);
#endif

    hash_table *pht = hash_table_create(256, free);
    if (!pht) {
        abort();
    }

    *ht = pht;

    const char *ptr = strstr(bq->r, "HTTP/");
    int num = 0;
    int n = 0;
    int stat = 0;
    char value[64] = { '\0' };
    char version[8] = { '\0' };
    size_t ldsize = 0;
    if (ptr)
        num = sscanf(ptr, "HTTP/%s %[^\r\n]\r\n%n", version, value, &n);

    if (!num || num != 2) {
        fprintf(stderr, "Failed to parse header: %s\n", ptr);
        perror("Failed to parse header");
        return -1;
    }
    //TODO: Check http version if necessary...
    char *key = "version";
    hash_table_insert(pht, key, strdup(version), strlen(version));

    key = "status";
    hash_table_insert(pht, key, strdup(value), strlen(value));
    ptr += n;

    num = sscanf(value, "%d", &stat);
    assert(num);

    // It is the worst case to allocate such a large memory region ...
    size_t length = (char *) bq->w - ptr;;
    char *k = ZALLOC(char, length);
    char *v = ZALLOC(char, length);
    while ((ptr < (char *) bq->r + length) && fptr && ptr < fptr) {
        memset(k, 0, length);
        memset(v, 0, length);
        if (sscanf((const char *) ptr, "%[^ 	:]: %[^\r\n]\r\n%n",
                   k, v, &n)) {
            lowwer_case(k, strlen(k));
            hash_table_insert(pht, k, strdup(v), strlen(v));
            ldsize += n;
            ptr += n;
        }
    }

    bq->r = fptr + 4;           // seek to \r\n\r\n..

    FIF(k);
    FIF(v);
    return stat;
}

/* This function accepts an pointer of connection pointer, on return. When 302
 * is detected, it will modify both ui and conn to ensure a valid connection
 * can be initialized. */
static inline uint64 get_remote_file_size_http(url_info * ui,
                                               byte_queue * bq,
                                               connection ** conn,
                                               hash_table ** ht)
{
    if (!conn || !*conn || !ui) {
        return 0;
    }

    PDEBUG("enter\n");

    char *hd = generate_request_header("GET", ui, 0, 0);

    (*conn)->co.write((*conn), hd, strlen(hd), NULL);
    free(hd);

    char *eptr = NULL;
    int i = 1;
    do {
        bq = bq_enlarge(bq, PAGE);
        size_t rd = (*conn)->co.read((*conn), bq->w, bq->x - bq->w, NULL);
        if (!rd) {
            PDEBUG("Failed to read from connection(%p),"
                   " connection closed.\n", *conn);
            return 0;
        }

        bq->w += rd;
    } while ((eptr = strstr(bq->r, HEADER_END)) == NULL);

    int stat = dissect_header(bq, ht);

    PDEBUG("stat: %d, description: %s\n",
           stat, (char *) hash_table_entry_get(*ht, "status"));

    int num = 0;
    char *ptr = NULL;
    uint64 t = 0;

    switch (stat) {
        case 206: { // Ok, we can start download now.
            ptr = (char *) hash_table_entry_get(*ht, "content-range");
            if (!ptr) {
                fprintf(stderr, "Content Range not returned: %s!\n",
                        bq->p);
                t = 0;
                goto ret;
            }

            PDEBUG("Range:: %s\n", ptr);
            hash_table_insert(*ht, CAN_SPLIT, strdup("true"), 4);
            uint64 s, e;
            num = sscanf(ptr, "bytes %" PRIu64 "-%" PRIu64 "/%" PRIu64,
                         &s, &e, &t);
            break;
        }
    case 301:
    case 302:                  // Resource moved to other place.
    case 307: {
            char *loc = (char *) hash_table_entry_get(*ht, "location");

            printf("Server returns 302, trying new locations: %s...\n",
                   loc);
            url_info *nui = NULL;

            if (loc && parse_url(loc, &nui)) {
                url_info_copy(ui, nui);
                url_info_destroy(&nui);
                connection_put(*conn);
                *conn = connection_get(ui);
                bq_reset(bq);
                //TODO: reset ht?
                return get_remote_file_size_http(ui, bq, conn, ht);
            }
            fprintf(stderr,
                    "Failed to get new location for status code: 302\n");
            break;
        }
    case 200: {
            ptr = (char *) hash_table_entry_get(*ht, "content-length");
            if (!ptr) {
                mlog(LL_ALWAYS, "Content Length not returned!\n");
                t = 0;
                goto show_rsp;
            }

            mlog(LL_NONVERBOSE,
                 "Not sure if server supports Content-Range,"
                 " Will not use multi-connections..\n");
            PDEBUG("Content-Length: %s\n", ptr);

            num = sscanf(ptr, "%" PRIu64, &t);
            //@todo: this kind of server don't support multi connection.
            // handle this!
            break;
        }
    default: {
            if (stat >= 400 && stat < 511) {
                mlog(LL_ALWAYS, "Server returns %d for HTTP request\n",
                     stat);
            } else if (stat == 511) {
                mlog(LL_ALWAYS, "Network Authentication Required"
                     "(%d)..\n", stat);
            } else {
                mlog(LL_ALWAYS, "Not implemented for status code: %d\n",
                     stat);
            }
          show_rsp:
            mlog(LL_ALWAYS, "Detail Responds: %s\n", bq->p);
            goto ret;
        }
    }

    if (!num) {
        fprintf(stderr, "Failed to parse string: %s\n", ptr);
    } else {
        // Check http headers and update connection_features....
    }

  ret:
    return t;
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
