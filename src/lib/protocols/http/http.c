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
#include "../../connection.h"
#include "../../data_utlis.h"
#include "../../fileutils.h"
#include "../../metadata.h"
#include "../../mget_utils.h"
#include "http.h"
#include <errno.h>
#include <unistd.h>

#define DEFAULT_HTTP_CONNECTIONS 5
#define PAGE                     4096

static const char *HEADER_END        = "\r\n\r\n";
static const char *CAN_SPLIT         = "can_split";
static const char *TRANSFER_ENCODING = "transfer-encoding";
static const char *CHUNKED           = "chunked";

typedef enum _http_transfer_type {
    htt_raw,
    htt_chunked,
    htt_unknown = 255
} htxtype;

typedef struct http_request_context hcontext;
// @todo: move this param into src/lib/protocol when more protocols are added.
typedef struct _connection_operation_param {
    void          *addr;                //base addr;
    data_chunk    *dp;
    url_info      *ui;
    bool           header_finished;
    byte_queue    *bq;
    hash_table    *ht;
    void (*cb) (metadata *, void *);
    metadata      *md;
    dinfo         *info;
    hcontext      *context;
    void          *user_data;
} co_param;

struct http_request_context
{
    bool        can_split;
    bool*       cflag;                  // control flag
    byte_queue* bq;
    htxtype     type;
    dinfo*      info;
    url_info*   ui;
    connection* conn;

    void (*cb) (metadata *, void *);
    void*       user_data;
};

#define CALLBACK(X)                             \
    do                                          \
    {                                           \
        if (X->cb)                              \
            (X->cb)(X->info->md, X->user_data);  \
    } while (0)


static char *generate_request_header(const char *method,
                                     url_info   *uri,
                                     bool        request_partial,
                                     uint64      start_pos,
                                     uint64    end_pos);
static int dissect_header(byte_queue * bq, hash_table ** ht);
static uint64 get_remote_file_size(url_info    *ui,
                                   hash_table **ht,
                                   hcontext* context);
static char* get_suggested_name(const char* input);
static mget_err process_request_single_form(hcontext*);
static mget_err process_request_multi_form(hcontext*);


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

    int rd = 0;
    void *addr = param->addr + dp->cur_pos;
    if (!param->header_finished) {
        bq_enlarge(param->bq, 4096 * 100);
        rd = conn->co.read(conn, param->bq->w,
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
                param->md->hd.current_size+=length;
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

    do {
        rd = conn->co.read(conn, param->addr + dp->cur_pos,
                           dp->end_pos - dp->cur_pos, NULL);
    } while (rd == -1 && errno == EINTR);

    if (rd > 0) {
        dp->cur_pos += rd;
        param->md->hd.current_size+=rd;
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
    char *hd = generate_request_header("GET", cp->ui, true,
                                       cp->dp->cur_pos,
                                       cp->dp->end_pos);
    size_t written = conn->co.write(conn, hd, strlen(hd), NULL);

    PDEBUG("written: %d\n", written);

    free(hd);
    return COF_FINISHED;
}

mget_err process_http_request(dinfo *info, dp_callback cb,
                              bool *stop_flag, void *user_data)
{
    PDEBUG("enter\n");

    hcontext context = {
        .can_split = false,
        .cflag     = stop_flag,
        .type      = htt_raw,
        .bq        = bq_init(PAGE),
        .info      = info,
        .cb        = cb,
        .user_data = user_data,
    };

    if (dinfo_ready(info)) {
        context.can_split = info->md->hd.package_size != 0;
        PDEBUG ("Start directly...\n");
        goto start;
    }

    url_info *ui = info ? info->ui : NULL;
    context.conn = connection_get(info->ui);
    if (!context.conn) {
        fprintf(stderr, "Failed to get socket!\n");
        return ME_CONN_ERR;
    }

    hash_table *ht    = NULL;
    uint64      total = get_remote_file_size(info->ui, &ht, &context);

    if (!ht) {
        mlog(LL_ALWAYS, "Failed to parse http response..\n");
        return ME_RES_ERR;
    }

    if (!total) {
        const char* val = (char*) hash_table_entry_get(ht, TRANSFER_ENCODING);
        if (val) {
            PDEBUG ("Transer-Encoding is: %s\n", val);
            if (!strcmp(val, CHUNKED)) {
                context.can_split = false;
                context.type = htt_chunked;
            }
        }
    }

    // try to get file name from http header..
    char *fn = NULL;
    if (info->md->hd.update_name) {
        char *dis = hash_table_entry_get(ht, "content-disposition");
        PDEBUG("updating name based on disposition: %s\n", dis);
        fn = get_suggested_name(dis);

        if (!fn && (!strcmp(ui->bname, ".") ||!strcmp(ui->bname, "/"))) {
            fn = strdup("index.html"); 
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

    if (!context.can_split) {
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

    PDEBUG ("md: %p, status: %d, nr: %d -- %d\n", md, md->hd.status,
            md->hd.nr_user, md->hd.nr_effective);

    if (md->hd.status == RS_FINISHED) {
        goto ret;
    }

    md->hd.status = RS_STARTED;
    if (cb) {
        (*cb) (md, user_data);
    }

    mget_err err = ME_OK;
    if (context.can_split)
        err = process_request_multi_form(&context);
    else
        err = process_request_single_form(&context);

    PDEBUG ("md: %p, status: %d, nr: %d -- %d\n", md, md->hd.status,
            md->hd.nr_user, md->hd.nr_effective);

    if (err != ME_OK && err < ME_DO_NOT_RETRY &&
        stop_flag && !*stop_flag) {  // errors occurred, restart
        goto restart;
    }

    if (err == ME_OK) {
        md->hd.status = RS_FINISHED;
    } else {
        md->hd.status = RS_PAUSED;
        if (stop_flag && *stop_flag) {
            err = ME_ABORT;
        }
    }

    md->hd.acc_time += get_time_s() - md->hd.last_time;

ret:
    PDEBUG ("md: %p, size: %d - %d, nr: %d -- %d\n", md, md->hd.package_size,
            md->hd.current_size, md->hd.nr_user, md->hd.nr_effective );

    if (md->hd.package_size)
        metadata_display(md);

    if (cb) {
        (*cb) (md, user_data);
    }

    bq_destroy(&context.bq);
    PDEBUG("stopped, ret: %d.\n", ME_OK);
    return ME_OK;
}



static char *generate_request_header(const char *method,
                                     url_info   *uri,
                                     bool        request_partial,
                                     uint64      start_pos,
                                     uint64      end_pos)
{
    static char buffer[PAGE];
    memset(buffer, 0, PAGE);
    if (request_partial)
        sprintf(buffer,
                "%s %s HTTP/1.1\r\n"
                "User-Agent: mget(%s)\r\n"
                "Host: %s\r\n"
                "Accept: *\r\n"
                "Connection: Keep-Alive\r\n"
                "Keep-Alive: timeout=600\r\n"
                "Range: bytes=%" PRIu64 "-%" PRIu64 "\r\n\r\n",
                method, uri->uri,
                VERSION_STRING,
                uri->host,
                start_pos,
                end_pos);
    else
        sprintf(buffer,
                "%s %s HTTP/1.1\r\n"
                "User-Agent: mget(%s)\r\n"
                "Host: %s\r\n"
                "Accept: *\r\n"
                "Connection: Keep-Alive\r\n"
                "Keep-Alive: timeout=600\r\n\r\n",
                method, uri->uri,
                VERSION_STRING,
                uri->host);
    return strdup(buffer);
}

static int dissect_header(byte_queue * bq, hash_table ** ht)
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

// return http status if success, or -1 if failed.
static int get_response_for_header(hcontext* context,
                                   const char* hd,
                                   hash_table** ht)
{
    if (!context || !context->conn) {
        return -1;
    }

    PDEBUG("enter\n");

    byte_queue  *bq   = context->bq;
    connection*  conn = context->conn;

    conn->co.write(conn, hd, strlen(hd), NULL);

    char *eptr = NULL;
    do {
        bq = bq_enlarge(bq, PAGE);
        size_t rd = conn->co.read(conn, bq->w, bq->x - bq->w, NULL);
        if (!rd) {
            PDEBUG("Failed to read from connection(%p),"
                   " connection closed.\n", conn);
            return 0;
        }

        bq->w += rd;
    } while ((eptr = strstr(bq->r, HEADER_END)) == NULL);

    PDEBUG ("header: %p -- %p, %s\n", bq->r, eptr, bq->r);
    int stat = dissect_header(bq, ht);
    PDEBUG("stat: %d, description: %s\n",
           stat, (char *) hash_table_entry_get(*ht, "status"));
    return stat;
}

/* This function accepts an pointer of connection pointer, on return. When 302
 * is detected, it will modify both ui and conn to ensure a valid connection
 * can be initialized. */
uint64 get_remote_file_size(url_info * ui,
                            hash_table **ht,
                            hcontext* context)
{
    if (!context || !context->conn) {
        return 0;
    }

    PDEBUG("enter\n");

    char *hd   = generate_request_header("GET", ui, true, 0, 1);
    int   stat = get_response_for_header(context, hd, ht);
    free(hd);

    int     num = 0;
    char   *ptr = NULL;
    uint64  t   = 0;

    switch (stat) {
        case 206: { // Ok, we can start download now.
            ptr = (char *) hash_table_entry_get(*ht, "content-range");
            if (!ptr) {
                fprintf(stderr, "Content Range not returned: %s!\n",
                        context->bq->p);
                t = 0;
                goto ret;
            }

            PDEBUG("Range:: %s\n", ptr);
            context->can_split = true;
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
                connection_put(context->conn);
                context->conn = connection_get(ui);
                bq_reset(context->bq);
                return get_remote_file_size(ui, ht, context);
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
            mlog(LL_ALWAYS, "Detail Responds: %s\n", context->bq->p);
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

// free by caller.
char* get_suggested_name(const char* dis)
{
    if (STREMPTY(dis))
        return NULL;

    char* fn = ZALLOC(char, strlen(dis));
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
        FIFZ(&fn);
    }
    else {
        mlog(LL_ALWAYS, "Renaming file name to: %s\n", fn);
    }

    return fn;
}

static long get_chunk_size(byte_queue* bq, int* consumed)
{
    char* ptr = strstr((char*)bq->r, "\r\n");
    if (!ptr)
        return -1;
    long  sz  = strtol((char*)bq->r, NULL, 16);
    *consumed = ptr - (char*)bq->r + 2;
    bq->r = (byte*)ptr + 2;
    return sz;
}

static mget_err receive_chunked_data(hcontext* context)
{
    int         fd   = fm_get_fd(context->info->fm_file);
    byte_queue* bq   = context->bq;
    connection* conn = context->conn;
    long chunk_size;
    int consumed = 0;
read_chunk_size:
    chunk_size = get_chunk_size(context->bq, &consumed);
    PDEBUG ("chunk_size: %d\n", chunk_size);
    if (chunk_size == -1)  { // not enough data...
        bq_enlarge(bq, PAGE);
        int rd = conn->co.read(conn, (char*)bq->w, PAGE, NULL);
        PDEBUG ("rd: %d\n", rd);
        if (rd > 0) {
            bq->w += rd;
            goto read_chunk_size;
        }
        else {
            return ME_CONN_ERR;
        }
    }

    PDEBUG ("ChunkSize: %d\n", chunk_size);
    if (chunk_size>0) {
        long pending = chunk_size - consumed;
        pending -= (bq->w  - bq->r);
        do {
            bq_enlarge(bq, PAGE);
            int rd = conn->co.read(conn, (char*)bq->w, pending > PAGE ? PAGE:pending, NULL);
            CALLBACK(context);
            if (rd > 0) {
                bq->w += rd;
                pending -= rd;
            }
            else
                return ME_CONN_ERR;

            PDEBUG ("pending: %d\n", pending);
        } while (pending > 0);

        if (!(safe_write(fd, (char*)bq->r, bq->w - bq->r - 2))) { // exclude \r\n
            mlog(LL_NONVERBOSE,
                 "Failed to write to fd: %d, error: %d -- %s\n",
                 fd, errno, strerror(errno));
            return ME_RES_ERR;
        }
        bq_reset(bq);
        PDEBUG ("read again...\n");
        goto read_chunk_size;
    }

    return ME_OK;
}

static mget_err receive_limited_data(hcontext* context)
{
    uint64      pending = context->info->md->hd.package_size;
    int         fd      = fm_get_fd(context->info->fm_file);
    byte_queue* bq      = context->bq;
    connection* conn    = context->conn;
    size_t      length = bq->w - bq->r;
retry:
    if (length > 0) {
        if (!safe_write(fd, (char*)bq->r, length))
            return ME_RES_ERR;
        bq_reset(bq);
        pending -= length;
    }
    if (pending > 0) {
        int rd = conn->co.read(conn, (char*)bq->w, PAGE, NULL);
        CALLBACK(context);
        if (rd > 0) {
            bq->w += rd;
            length = rd;
            goto retry;
        }
        else
            return ME_CONN_ERR;
    }

    return ME_OK;
}

// receive until connection closed...
static mget_err receive_unlimited_data(hcontext* context)
{
    PDEBUG ("enter..\n");
    PDEBUG ("md: %p, nr: %d--%d\n", context->info->md,
            context->info->md->hd.nr_user,    context->info->md->hd.nr_effective);
    int         fd      = fm_get_fd(context->info->fm_file);
    byte_queue* bq      = context->bq;
    connection* conn    = context->conn;
    size_t      length = bq->w - bq->r;
retry:
    if (length > 0) {
        if (!safe_write(fd, (char*)bq->r, length))
            return ME_RES_ERR;
    }
    bq_reset(bq);
    int rd = conn->co.read(conn, (char*)bq->w, PAGE, NULL);
    CALLBACK(context);
    PDEBUG ("md: %p, nr: %d--%d\n", context->info->md,
            context->info->md->hd.nr_user,    context->info->md->hd.nr_effective);
    PDEBUG ("rd: %d\n", rd);
    if (rd > 0) {
        bq->w += rd;
        length = rd;
        goto retry;
    }
    else
        return rd == 0 ? ME_OK : ME_RES_ERR;
}

mget_err process_request_single_form(hcontext* context)
{
    connection* conn = context->conn;
    if (!conn) {
  retry:
        conn = connection_get(context->info->ui);
        if (!conn)
            return ME_RES_ERR;

        context->conn = conn;
        hash_table* ht = NULL;
        char* header = generate_request_header("GET", context->info->ui,
                                               false, 0, 0);
        int state = get_response_for_header(context, header, &ht);
        free(header);
        if (state == -1)
            return ME_CONN_ERR;
        switch (state) {
            case 200:
            case 206: {
                break;
            }
            case 301:
            case 302:          // Resource moved to other place.
            case 307:{
                char *loc = (char *) hash_table_entry_get(ht, "location");
                printf("Server returns 302, trying new locations: %s...\n",
                       loc);
                url_info *nui = NULL;

                if (loc && parse_url(loc, &nui)) {
                    url_info_copy(context->ui, nui);
                    url_info_destroy(&nui);
                    connection_put(context->conn);
                    context->conn = connection_get(context->ui);
                    bq_reset(context->bq);
                    goto retry;
                }
                break;
            }
            default:{
                if (state >= 400 && state < 511) {
                    mlog(LL_ALWAYS, "Server returns %d for HTTP request\n",
                         state);
                } else if (state == 511) {
                    mlog(LL_ALWAYS, "Network Authentication Required"
                         "(%d)..\n", state);
                } else {
                    mlog(LL_ALWAYS, "Not implemented for status code: %d\n",
                         state);
                }
                exit(1);
            }
        }
    }

    metadata* md = context->info->md;
    PDEBUG ("md: %p, status: %d, nr: %d -- %d\n", md, md->hd.status,
            md->hd.nr_user, md->hd.nr_effective);

    mget_err err = ME_OK;
    bq_enlarge(context->bq, PAGE);
    if (context->type == htt_chunked)
        err = receive_chunked_data(context);
    else if (context->info->md->hd.package_size)
        err = receive_limited_data(context);
    else
        err = receive_unlimited_data(context);

    if (err == ME_OK)
        context->info->md->hd.package_size = get_file_size(context->info->fm_md);
    return err;
}

mget_err process_request_multi_form(hcontext* ctx)
{
    mget_err err = ME_OK;

    connection_group *sg = connection_group_create(cg_all, ctx->cflag);
    if (!sg) {
        fprintf(stderr, "Failed to create sock group.\n");
        return ME_RES_ERR;
    }

    bool need_request = false;
    dinfo*       info = ctx->info;
    metadata*    md   = info->md;
    data_chunk  *dp   = md->ptrs->body;
    url_info*    ui   = ctx->info->ui;

    for (int i = 0; i < md->hd.nr_effective; ++i, ++dp) {
        if (dp->cur_pos >= dp->end_pos) {
            continue;
        }

        need_request = true;
        connection*  conn = connection_get(ui);
        if (!conn) {
            fprintf(stderr, "Failed to create connection!!\n");
            err = ME_RES_ERR; // @todo: clean up resource..
            goto ret;
        }

        co_param *param = ZALLOC1(co_param);

        param->addr      = info->fm_file->addr;
        param->dp        = dp;
        param->ui        = ui;
        param->md        = md;
        param->info      = info;
        param->bq        = bq_init(PAGE);
        param->cb        = ctx->cb;
        param->user_data = ctx->user_data;

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

    connection_group_destroy(sg);
    dinfo_sync(info);

    dp = md->ptrs->body;
    bool finished = true;

    for (int i = 0; i < CHUNK_NUM(md); ++i, ++dp) {
        if (dp->cur_pos < dp->end_pos) {
            finished = false;
            break;
        }
    }

    err =  finished ? ME_OK : ME_GENERIC;
ret:
    return err;
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
