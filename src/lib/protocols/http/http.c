/** mget_http.c --- implementation of libmget using http, based on rfc2616
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
#include "../../libmget.h"
#include "../../download_info.h"
#include "../../logutils.h"
#include "../../connection.h"
#include "../../data_utlis.h"
#include "../../fileutils.h"
#include "../../metadata.h"
#include "../../mget_utils.h"
#include <errno.h>
#include <unistd.h>

#define DEFAULT_HTTP_CONNECTIONS 5
#define PAGE                     4096

static const char *HEADER_END        = "\r\n\r\n";

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
    void (*cb) (metadata*, void*);
    metadata      *md;
    dinfo         *info;
    hcontext      *context;
    void          *user_data;
} co_param;

struct http_request_context {
    bool        can_split;
    bool*       cflag;                  // control flag
    byte_queue* bq;
    htxtype     type;
    dinfo*      info;

    int         port;
    const char* host;
    const char* uri_host;
    const char* uri;
    const mget_option* opts;

    connection* conn;

    void (*cb) (metadata *, void *);
    void*       user_data;
};

typedef struct _http_url_info
{
    url_info    info;
    const char* uri_host; // used by http request header.
} http_request;

#define CALLBACK(X)                             \
    do                                          \
    {                                           \
        if (X->cb)                              \
            (X->cb)(X->info->md, X->user_data); \
    } while (0)


static int dissect_header(byte_queue*, hash_table**);
static uint64 get_remote_file_size(url_info*, hash_table**, hcontext*);
static char* get_suggested_name(const char*);
static mget_err process_request_single_form(hcontext*);
static mget_err process_request_multi_form(hcontext*);

static char *generate_request_header(const char*, const char*, const char*,
                                     bool, uint64, uint64);
static int get_response_for_header(connection*, byte_queue*, const char*,
                                   hash_table**);
static url_info* add_proxy(url_info*, const struct mget_proxy*);



int http_read_sock(connection* conn, void* priv)
{
    if (!priv)
        return -1;

    co_param    *param = (co_param *) priv;
    data_chunk*  dp    = param->dp;

    if (dp->cur_pos >= dp->end_pos)
        return 0;

    int rd = 0;
    void *addr = param->addr + dp->cur_pos;
    if (!param->header_finished) {
        int stat = get_response_for_header(conn, param->bq, NULL, &param->ht);
        switch (stat) {
            case 206:
            case 200:{
                break;
            }
            case 301:
            case 302:
            case 303:
            case 307:{
                char *loc = (char*) hash_table_entry_get(param->ht, "location");
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
                        stat);
                exit(1);
            }
        }

        size_t length = param->bq->w - param->bq->r;
        PDEBUG("LEN: %ld, bq->w - bq->r: %ld\n", length,
               param->bq->w - param->bq->r);
        if (length) {
            memcpy(addr, param->bq->r, length);
            param->md->hd.current_size+=length;
            dp->cur_pos += length;
        }
        param->header_finished = true;
        bq_destroy(param->bq);
        if (dp->cur_pos == dp->end_pos)
            goto ret;
        else if (dp->cur_pos == dp->end_pos)
            mlog(LL_ALWAYS, "Wrong data: dp: %p : %llX -- %llX\n",
                 dp, dp->cur_pos, dp->end_pos);
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
    char *hd = generate_request_header("GET",
                                       cp->context->uri_host,
                                       cp->context->uri,
                                       true,
                                       cp->dp->cur_pos,
                                       cp->dp->end_pos);
    size_t written = conn->co.write(conn, hd, strlen(hd), NULL);

    PDEBUG("written: %d\n", written);

    free(hd);
    return COF_FINISHED;
}

mget_err process_http_request(dinfo *info, dp_callback cb,
                              bool *stop_flag,
                              mget_option* opts,
                              void *user_data)
{
    PDEBUG("enter\n");

    hcontext context = {
        .can_split = false,
        .cflag     = stop_flag,
        .type      = htt_raw,
        .bq        = bq_init(PAGE),
        .info      = info,
        .cb        = cb,

        .port     = info->ui->port,
        .uri_host = strdup(info->ui->host),
        .uri      = info->ui->uri,
        .opts     = opts,

        .user_data = user_data,
    };

    PDEBUG ("Proxy enabled: %d, host: %s\n", opts->proxy.enabled, opts->proxy.server);
    if (HAS_PROXY(&opts->proxy)) {
        url_info* new = add_proxy(info->ui, &opts->proxy);
        if (new) {
            url_info_destroy(info->ui);
            info->ui     = new;
            context.uri = new->uri;
        }
        PDEBUG ("new: %p\n", new);
    }

    PDEBUG ("host: %p\n", info->ui->host);
    PDEBUG ("host: %s, uri_host: %s, uri: %s\n",
            info->ui->host, context.uri_host, context.uri);

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

    if (total == (uint64)-1) {
        info->md->hd.status = RS_DROP;
        return ME_RES_ERR;
    }
    else if (!total) {
        const char* val = (char*) hash_table_entry_get(ht, "transfer-encoding");
        if (val) {
            PDEBUG ("Transer-Encoding is: %s\n", val);
            if (!strcmp(val, "chunked")) {
                context.can_split = false;
                context.type      = htt_chunked;
            }
        }
    }

    ui = context.info->ui; // ui maybe changed ...
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

    if (info->md->hd.nr_user == 0xff)
        info->md->hd.nr_user = DEFAULT_HTTP_CONNECTIONS;

    if (!context.can_split)
        info->md->hd.nr_user = 1;

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

    if (md->hd.status == RS_FINISHED)
        goto ret;

    md->hd.status = RS_STARTED;
    if (cb)
        (*cb) (md, user_data);

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

    bq_destroy(context.bq);
    PDEBUG("stopped, ret: %d.\n", ME_OK);
    return ME_OK;
}



static char *generate_request_header(const char* method,
                                     const char* uri_host,
                                     const char* uri,
                                     bool        request_partial,
                                     uint64      start_pos,
                                     uint64      end_pos)
{
    PDEBUG ("enter with: %s -- %s \n", uri_host, uri);
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
                method, uri,
                VERSION_STRING,
                uri_host,
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
                method, uri,
                VERSION_STRING,
                uri_host);

    PDEBUG ("hd: %s\n", buffer);
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

    hash_table *pht = hash_table_create(256, free);
    if (!pht)
        abort();

    *ht = pht;

    const char *ptr = strstr(bq->r, "HTTP/");
    int    num      = 0;
    int    n        = 0;
    int    stat     = 0;
    char value[64]  = { '\0' };
    char version[8] = { '\0' };
    size_t ldsize   = 0;
    if (ptr)
        num = sscanf(ptr, "HTTP/%s %[^\r\n]\r\n%n", version, value, &n);

    if (!num || num != 2) {
        fprintf(stderr, "Failed to parse header: %s\n", ptr);
        perror("Failed to parse header");
        return -1;
    }
    //TODO: Check http version if necessary...
    char *key = "version";
    HASH_TABLE_INSERT(pht, key, strdup(version), strlen(version));

    key = "status";
    HASH_TABLE_INSERT(pht, key, strdup(value), strlen(value));
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
            HASH_TABLE_INSERT(pht, k, strdup(v), strlen(v));
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
int get_response_for_header(connection* conn,
                            byte_queue* bq,
                            const char* hd,
                            hash_table** ht)
{
    PDEBUG("enter\n");

    if (!conn)
        return -1;
    if (!bq)
        bq = bq_init(PAGE);

    if (hd) {
        conn->co.write(conn, hd, strlen(hd), NULL);
    }

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


// return size of remote file, or -1 if error occurs, or 0 if size not returned...
uint64 get_remote_file_size(url_info* ui,
                            hash_table** ht,
                            hcontext* context)
{
    if (!context || !context->conn) {
        return 0;
    }

    PDEBUG("enter, uri_host: %s, uri: %s\n", context->uri_host, context->uri);

    char *hd   = generate_request_header("GET", context->uri_host, context->uri, true, 0, 1);
    int   stat = get_response_for_header(context->conn,
                                         context->bq,
                                         hd, ht);
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
        case 302:
        case 303:
        case 307: {
            char *loc = (char *) hash_table_entry_get(*ht, "location");

            printf("Server returns 302, trying new locations: %s...\n",
                   loc);
            url_info *nui = NULL;
            if (loc && parse_url(loc, &nui)) {
                url_info_copy(ui, nui);
                url_info_destroy(nui);
                if (HAS_PROXY(&context->opts->proxy)) {
                    url_info* new = add_proxy(ui, &context->opts->proxy);
                    if (new) {
                        url_info_destroy(ui);
                        ui           = new;
                        context->uri = new->uri;
                    }
                    PDEBUG ("new: %p\n", new);
                }
                else {
                    connection_put(context->conn);
                    context->conn = connection_get(ui);
                }
                bq_reset(context->bq);
                context->info->ui = ui;
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
            t = -1;
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
            mlog(LL_NOTQUIET, "Detail Responds: %s\n", context->bq->p);
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
    bq->r = ptr + 2;
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

        size_t total =  bq->w - bq->r - 2;
        if (!(safe_write(fd, (char*)bq->r, total))) { // exclude \r\n
            mlog(LL_NONVERBOSE,
                 "Failed to write to fd: %d, error: %d -- %s\n",
                 fd, errno, strerror(errno));
            return ME_RES_ERR;
        }
        context->info->md->hd.current_size += total;
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
        length = rd;
        context->info->md->hd.current_size += length;
        CALLBACK(context);
        if (rd > 0) {
            bq->w += rd;
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
    length = rd;
    context->info->md->hd.current_size += length;
    CALLBACK(context);
    PDEBUG ("md: %p, nr: %d--%d\n", context->info->md,
            context->info->md->hd.nr_user,    context->info->md->hd.nr_effective);
    PDEBUG ("rd: %d\n", rd);
    if (rd > 0) {
        bq->w += rd;
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
        char* header = generate_request_header("GET", context->uri_host,
                                               context->uri,
                                               false, 0, 0);
        int state = get_response_for_header(context->conn,
                                            context->bq,
                                            header, &ht);
        free(header);
        if (state == -1)
            return ME_CONN_ERR;
        switch (state) {
            case 200:
            case 206: {
                break;
            }
            case 301:
            case 302:
            case 303:
            case 307:{
                char *loc = (char *) hash_table_entry_get(ht, "location");
                printf("Server returns 302, trying new locations: %s...\n",
                       loc);
                url_info *nui = NULL;

                if (loc && parse_url(loc, &nui)) {
                    url_info_copy(context->info->ui, nui);
                    url_info_destroy(nui);
                    if (HAS_PROXY(&context->opts->proxy)) {
                        url_info* new = add_proxy(context->info->ui, &context->opts->proxy);
                        if (new) {
                            url_info_destroy(context->info->ui);
                            context->info->ui  = new;
                            context->uri = new->uri;
                        }
                        PDEBUG ("new: %p\n", new);
                    }
                    else {
                        connection_put(context->conn);
                        context->conn = connection_get(context->info->ui);
                    }
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
        param->context   = ctx;
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

static url_info* add_proxy(url_info* ui, const struct mget_proxy* proxy)
{
    PDEBUG ("Updating uri with proxy, server: %s...\n", proxy->server);
    url_info* new = ZALLOC1(url_info);
    new->host = strdup(proxy->server); // needs to update this.
    if (proxy->encrypted) {
        new->eprotocol = UP_HTTPS;
        sprintf(new->protocol, "https");
    }
    else {
        new->eprotocol = UP_HTTP;
        sprintf(new->protocol, "http");
    }

    new->port = proxy->port;
    sprintf(new->sport, "%d", proxy->port);

    if (!ui->furl) {
        mlog(LL_ALWAYS, "Failed to get url!");
        url_info_destroy(new);
        return NULL;
    }

    new->furl  = strdup(ui->furl);
    new->uri   = strdup(ui->furl);
    new->bname = strdup(ui->bname);

    return new;
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
