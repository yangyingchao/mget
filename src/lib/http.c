/** mget_http.c --- implementation of libmget using raw socket
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
#include "debug.h"
#include "timeutil.h"
#include <unistd.h>
#include <errno.h>

static char* generate_request_header(const char* method, url_info* uri,
                                     uint64 start_pos, uint64 end_pos)
{
    assert(uri->uri);

    char buffer[4096];
    memset(buffer, 0, 2096);

    sprintf(buffer,
            "%s %s HTTP/1.1\r\nHost: %s\r\nAccept: *\r\nRange: bytes=%llu-%llu\r\n\r\n",
            method, uri->uri, uri->host, start_pos, end_pos);

    return strdup(buffer);
}

int dissect_header(const char* buffer, size_t length, hash_table** ht)
{
    if (!ht || !buffer)
    {
        return -1;
    }

    hash_table* pht = hash_table_create(256, free);
    if (!pht)
    {
        abort();
    }

    *ht = pht;

    const char* ptr = buffer;
    int         n      = 0;
    int         num    = 0;
    int         stat   = 0;
    char value[64]     = {'\0'};
    size_t      ldsize = 0;



    num = sscanf(ptr, "HTTP/1.1 %[^\r\n]\r\n%n", value, &n);
    if (!num)
    {
        perror("Failed to parse header");
        return -1;
    }

    char* key = strdup("Status");

    hash_table_insert(pht, key, strdup(value));
    ptr    += n;

    num = sscanf(value, "%d", &stat);
    assert(num);

    char k[256] = {'\0'};
    char v[256] = {'\0'};
    while (ptr < buffer + length) {
        memset(k, 0, 256);
        memset(v, 0, 256);
        if (sscanf((const char*)ptr, "%[^:]: %[^\r\n]\r\n%n", k, v, &n))
        {
            hash_table_insert(pht, strdup(k), strdup(v));
            ldsize += n;
            ptr += n;
        }
    }

    return stat;
}

uint64 get_remote_file_size_http(url_info* ui)
{
    connection* conn = connection_get(ui);
    if (!conn)
    {
        fprintf(stderr, "Failed to get socket!\n");
        return 0;
    }

    char* hd = generate_request_header("GET", ui, 0, 0);
    conn->ci.writer(conn, hd, strlen(hd), NULL);
    free(hd);

    char buffer[4096];
    memset(buffer, 0, 4096);
    size_t      rd    = conn->ci.reader(conn, buffer, 4096, NULL);
    hash_table* ht    = NULL;
    int         stat  = dissect_header(buffer, rd, &ht);

    PDEBUG ("stat: %d, description: %s\n",
            stat, (char*)hash_table_entry_get(ht, "Status"));

    switch (stat)
    {
        case 206: // Ok, we can start download now.
        {
            break;
        }
        case 302: // Resource moved to other place.
        {
            char* loc = (char*)hash_table_entry_get(ht, "Location");
            printf("Server returns 302, trying new locations: %s...\n", loc);
            url_info* nui = NULL;
            if (loc && parse_url(loc, &nui))
            {
                url_info_copy(ui, nui);
                url_info_destroy(&nui);
                return get_remote_file_size_http(ui);
            }
            fprintf(stderr, "Failed to get new location for status code: 302\n");
            break;
        }
        default:
        {
            fprintf(stderr, "Not implemented for status code: %d\n", stat);
            fprintf(stderr, "Response Header\n%s\n", buffer);
            return 0;
            break;
        }
    }

    char* ptr = (char*)hash_table_entry_get(ht, "Content-Range");
    if (!ptr)
    {
        fprintf(stderr, "Content Range not returned!\n");
        return 0;
    }

    PDEBUG ("Range:: %s\n", ptr);

    uint64 s, e, t;
    int num = sscanf(ptr, "bytes %llu-%llu/%llu",
                     &s, &e, &t);
    if (!num)
    {
        fprintf(stderr, "Failed to parse string: %s\n", ptr);
    }
    else
    {
        // Check http headers and update connection_features....
    }

    return t;
}

typedef struct _connection_operation_param
{
    void*       addr;                   //base addr;
    data_chunk* dp;
    url_info*   ui;
    bool        header_finished;
    char*       hd;
    hash_table* ht;
    void (*cb)(metadata* md);
    metadata*   md;
} so_param;

int http_read_sock(connection* conn, void* priv)
{
    if (!priv)
    {
        return -1;
    }

    so_param*   param = (so_param*) priv;
    data_chunk* dp    = (data_chunk*)param->dp;
    void*       addr  = param->addr + dp->cur_pos;

    if (!param->header_finished)
    {
        char buf[4096] = {'\0'};
        memset(buf, 0, 4096);
        size_t rd = conn->ci.reader(conn, buf, 4095, NULL);
        if (rd)
        {
            char* ptr = strstr(buf, "\r\n\r\n");
            if (ptr == NULL)
            {
                // Header not complete, store and return positive value.
                param->hd = strdup(buf);
                return 1;
            }

            size_t ds = ptr - buf + 4;
            int r = dissect_header(buf, ds, &param->ht);
            if (r != 206)
            {
                fprintf(stderr, "status code is %d!\n", r);
                exit(1);
            }

            ptr+= 4; // adjust to the tail of "\r\n\r\n"

            param->header_finished = true;

            char* mm = ZALLOC(char, ds+1);
            strncpy(mm, buf, ds);
            free(mm);

            size_t length = rd - ds;
            if (length)
            {
                memcpy(addr, ptr, length);
                dp->cur_pos += length;
            }
        }
    }

    int rd;
    do
        rd = conn->ci.reader(conn, param->addr+dp->cur_pos,
                             dp->end_pos - dp->cur_pos, NULL);
    while (rd == -1 && errno == EINTR);
    if (rd == -1)
    {
        fprintf(stderr, "read returns -1\n");
        assert(0);
    }

    if (rd > 0)
    {
        dp->cur_pos += rd;
        if (param->cb)
        {
            (*(param->cb))(param->md);
        }
    }
    else
    {
        PDEBUG ("Read returns 0: showing chunk: \n");

        PDEBUG ("retuned zero: dp: %p : %llX -- %llX\n",
                dp, dp->cur_pos, dp->end_pos);
    }

    if (dp->cur_pos >= dp->end_pos)
    {
        PDEBUG ("Finished chunk: %p\n", dp);
        rd = 0; // Mark as completed.
    }
    else if (!rd)
    {
        PDEBUG ("retuned zero: dp: %p : %llX -- %llX\n",
                dp, dp->cur_pos, dp->end_pos);
    }

    return rd;
}

int http_write_sock(connection* conn, void* priv)
{
    PDEBUG ("enter\n");
    if (!priv)
    {
        return -1;
    }

    so_param*   cp = (so_param*)priv;

    char*       hd = generate_request_header("GET", cp->ui, cp->dp->cur_pos,
                                             cp->dp->end_pos);
    size_t written = conn->ci.writer(conn, hd, strlen(hd), NULL);
    free(hd);
    return written;
}

int process_http_request(url_info* ui, const char* fn, int nc,
                         void (*cb)(metadata* md), bool* stop_flag)
{
    PDEBUG ("enter\n");

    char tfn [256] = {'\0'};
    sprintf(tfn, "%s.tmd", fn);

    metadata_wrapper mw;
    if (file_existp(tfn) && metadata_create_from_file(tfn, &mw))
    {
        goto l1;
    }

    remove_file(tfn);
    uint64 total_size = get_remote_file_size_http(ui);
    if (!total_size)
    {
        fprintf(stderr, "Can't get remote file size: %s\n", ui->furl);
        return -1;
    }

    PDEBUG ("total_size: %llu\n", total_size);


    mget_slis* lst = NULL; //TODO: fill this lst.
    if (!metadata_create_from_url(ui->furl, fn, total_size, nc, lst, &mw.md))
    {
        return -1;
    }

    fhandle* fh  = fhandle_create(tfn, FHM_CREATE);
    mw.fm        = fhandle_mmap(fh, 0, MD_SIZE(mw.md));
    mw.from_file = false;
    memset(mw.fm->addr, 0, MD_SIZE(mw.md));

    associate_wrapper(&mw);

l1:
    metadata_display(mw.md);
    fhandle_msync(mw.fm);
    if (mw.md->hd.status == RS_SUCCEEDED)
    {
        goto ret;
    }

    mw.md->hd.status = RS_STARTED;
    if (cb)
    {
        (*cb)(mw.md);
    }

    fhandle* fh2 = fhandle_create(fn, FHM_CREATE);
    fh_map*  fm2 = fhandle_mmap(fh2, 0, mw.md->hd.package_size);
    if (!fh2 || !fm2)
    {
        PDEBUG ("Failed to create mapping!\n");
        // TODO: cleanup...
        return -1;
    }

    connection_group* sg = connection_group_create(stop_flag);
    if (!sg)
    {
        fprintf(stderr, "Failed to craete sock group.\n");
        return -1;
    }

    bool need_request = false;
    for (int i = 0; i < mw.md->hd.nr_chunks; ++i)
    {
        data_chunk* dp  = &mw.md->body[i];
        if (dp->cur_pos >= dp->end_pos)
        {
            continue;
        }

        need_request = true;

        connection* conn = connection_get(ui);
        if (conn)
        {
            so_param* param = ZALLOC1(so_param);
            param->addr = fm2->addr;
            param->dp = mw.md->body + i;
            param->ui = ui;
            param->md = mw.md;
            param->cb = cb;

            conn->rf = http_read_sock;
            conn->wf = http_write_sock;
            conn->priv = param;

            connection_add_to_group(sg, conn);
        }
    }

    if (!need_request)
    {
        mw.md->hd.status = RS_SUCCEEDED;
        goto ret;
    }

    PDEBUG ("Performing...\n");
    int ret = connection_perform(sg);

    PDEBUG ("ret = %d\n", ret);

    fhandle_munmap_close(&fm2);

    data_chunk* dp = mw.md->body;
    bool finished = true;
    for (int i = 0; i < CHUNK_NUM(mw.md); ++i)
    {
        if (dp->cur_pos < dp->end_pos)
        {
            finished = false;
            break;
        }
        dp++;
    }

    if (finished)
    {
        mw.md->hd.status = RS_SUCCEEDED;
    }
    else
    {
        mw.md->hd.status = RS_STOPPED;
    }

    mw.md->hd.acc_time += get_time_s() - mw.md->hd.last_time;

ret:

    metadata_display(mw.md);
    if (cb)
    {
        (*cb)(mw.md);
    }
    if (mw.md->hd.status == RS_SUCCEEDED)
    {
        remove_file(mw.fm->fh->fn);
    }

    metadata_destroy(&mw);
    PDEBUG ("stopped.\n");
    return 0;
}
