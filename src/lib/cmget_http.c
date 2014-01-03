/** cmget_http.c --- implementation of libmget using curl.
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

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <string.h>
#include <unistd.h>

#include "cmget_http.h"
#include "debug.h"
#include "macros.h"
#include "timeutil.h"

typedef struct _easy_param
{
    metadata* md;
    int       idx;
    void*     base_addr;                // addr of real file.
} easy_param;

typedef void (*hcb) (metadata*);

hcb g_cb = NULL;

size_t drop_content(void *buffer,  size_t size, size_t nmemb,
                    void *userp)
{
    return size*nmemb;
}

size_t recv_data(void *buffer,  size_t size, size_t nmemb,
                 void *userp)
{
    easy_param* ppm = (easy_param*) userp;
    data_chunk* dp = &ppm->md->body[ppm->idx];
    if (dp->cur_pos < dp->end_pos)
    {
        memcpy(ppm->base_addr + dp->cur_pos, buffer, size*nmemb);
        dp->cur_pos += size*nmemb;
    }

    if (g_cb)
    {
        (*g_cb)(ppm->md);
    }

    return size*nmemb;
}

size_t recv_header(void *buffer,  size_t size, size_t nmemb,
                   void *userp)
{
    hash_table* ht = (hash_table*)userp;

    if (ht)
    {
        char k[256] = {'\0'};
        char v[256] = {'\0'};
        if (sscanf((const char*)buffer, "%[^:]: %[^$] ", k, v))
        {
            if (!hash_table_insert(ht, strdup(rstrip(k)), strdup(rstrip(v))))
            {
                PDEBUG ("Failed to add kvp: %s, %s\n", k, v);
            }
        }
    }
    return size*nmemb;
}

uint64 get_remote_file_size(url_info* ui)
{
    static CURL* eh0 = NULL;
    if (!eh0)
    {
        eh0 = curl_easy_init();
    }
    PDEBUG ("eh0: %p\n", eh0);

    hash_table* ht = hash_table_create(256, free);

    curl_easy_setopt(eh0, CURLOPT_URL, ui->furl);
    curl_easy_setopt(eh0, CURLOPT_HEADERFUNCTION, recv_header);
    curl_easy_setopt(eh0, CURLOPT_HEADERDATA, ht);

    curl_easy_setopt(eh0, CURLOPT_WRITEFUNCTION, drop_content);
    curl_easy_setopt(eh0, CURLOPT_NOBODY, 1);
    CURLcode ret = curl_easy_perform(eh0);
    PDEBUG ("ret = %d\n",ret);


    long stat = 0;
    curl_easy_getinfo(eh0, CURLINFO_RESPONSE_CODE, &stat);

    PDEBUG ("Status Code: %ld\n", stat);
    //@todo: handle 302!!

    switch (stat)
    {
        case 200:
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
                return get_remote_file_size(ui);
            }
            fprintf(stderr, "Failed to get new location for status code: 302\n");
            break;
        }
        default:
        {
            fprintf(stderr, "Not implemented for status code: %ld\n", stat);
            return 0;
            break;
        }
    }

    char* val = (char*)hash_table_entry_get(ht, "Content-Length");
    if (val)
    {
        uint64 size = atoll(val);
        PDEBUG ("remote file size: %llu %s\n", size, stringify_size(size));
        return size;
    }

    return 0;
}

void process_http_request_c(url_info* ui, const char* fn, int nc,
                            void (*cb)(metadata* md), bool* stop_flag)
{
    PDEBUG ("enter\n");

    PDEBUG ("cb: %p\n",cb);

    g_cb = cb;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    char tfn [256] = {'\0'};
    sprintf(tfn, "%s.tmd", fn);
    metadata_wrapper mw;
    if (file_existp(tfn) && metadata_create_from_file(tfn, &mw))
    {
        goto l1;
    }

    remove_file(tfn);
    uint64 total_size = get_remote_file_size(ui);

    mget_slis* lst = NULL; //TODO: fill this lst.
    if (!metadata_create_from_url(ui->furl, fn, total_size, nc, lst, &mw.md))
    {
        return;
    }

    fhandle* fh  = fhandle_create(tfn, FHM_CREATE);
    mw.fm        = fhandle_mmap(fh, 0, MD_SIZE(mw.md));
    mw.from_file = false;
    memset(mw.fm->addr, 0, MD_SIZE(mw.md));

    associate_wrapper(&mw);

l1:;
    metadata_display(mw.md);
    if (mw.md->hd.status == RS_SUCCEEDED)
    {
        goto ret;
    }

    mw.md->hd.status = RS_STARTED;
    if (cb)
    {
        (*cb)(mw.md);
    }

    PDEBUG ("Intializing multi-handler...\n");
    // Initialize curl_multi
    CURLM* mh = curl_multi_init();
    if (!mh)
    {
        PDEBUG ("Failed to create mh.\n");
        exit(1);
    }

    fhandle* fh2 = fhandle_create(fn, FHM_CREATE);
    fh_map*  fm2 = fhandle_mmap(fh2, 0, mw.md->hd.package_size);
    if (!fh2 || !fm2)
    {
        PDEBUG ("Failed to create mapping!\n");
        // TODO: cleanup...
        return;
    }

    easy_param* params = ZALLOC(easy_param, mw.md->hd.nr_chunks);

    CURL**  ehs = ZALLOC(CURL*, mw.md->hd.nr_chunks);
    bool need_request = false;
    for (int i = 0; i < mw.md->hd.nr_chunks; ++i)
    {
        CURL* eh = ehs[i]  = curl_easy_init();

        data_chunk* dp  = &mw.md->body[i];
        if (dp->cur_pos >= dp->end_pos)
        {
            continue;
        }

        need_request = true;

        easy_param* ppm = params+i;
        ppm->md         = mw.md;
        ppm->idx        = i;
        ppm->base_addr  = fm2->addr;

        curl_easy_setopt(eh, CURLOPT_URL, ui->furl);
        curl_easy_setopt(eh, CURLOPT_HEADERFUNCTION, drop_content);
        curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, recv_data);
        curl_easy_setopt(eh, CURLOPT_WRITEDATA, ppm);

        struct curl_slist* l = NULL;
        char rg[64]          = {'\0'};
        sprintf(rg, "Range: bytes=%llu-%llu", dp->cur_pos, dp->end_pos);
        l = curl_slist_append(NULL, "Accept: */*");
        l = curl_slist_append(l, rg);
        curl_easy_setopt(eh, CURLOPT_HTTPHEADER, l);

        // Add to multy;
        curl_multi_add_handle(mh, eh);
    }

    if (!need_request)
    {
        mw.md->hd.status = RS_SUCCEEDED;
        goto ret;
    }

    PDEBUG ("Performing...\n");
    mw.md->hd.last_time = get_time_s();
    int* running_hanlders = &mw.md->hd.acon;
    curl_multi_perform(mh, running_hanlders);
    PDEBUG ("perform returns: %d\n", *running_hanlders);

    bool reschedule = true;
    while (*running_hanlders && stop_flag && !*stop_flag) {
        struct timeval timeout;
        fd_set rfds;
        fd_set wfds;
        fd_set efds;
        int    maxfd = -99;

        timeout.tv_sec = 0;
        timeout.tv_usec = 2000000L; /* 100 ms */

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        curl_multi_fdset(mh, &rfds, &wfds, &efds, &maxfd);

        int nr = select(maxfd, &rfds, &wfds, &efds, &timeout);
        if (nr >= 0)
        {
            if (false && *running_hanlders < mw.md->hd.nr_chunks && reschedule)
            {
                uint64 remained_size = 0;
                int    max_eh        = -1;
                int free_dps = 0;
                int eh_mask[mw.md->hd.nr_chunks];
                memset(eh_mask, 0, mw.md->hd.nr_chunks);
                data_chunk* max_dp = NULL;
                for (int i = 0; i < mw.md->hd.nr_chunks; ++i)
                {
                    data_chunk* dp = mw.md->body + i;
                    if (is_chunk_finished(dp))
                    {
                        curl_multi_remove_handle(mh, ehs[i]);
                        eh_mask[i] = 1;
                        free_dps++;
                    }
                    else
                    {
                        uint64 r_size =  (dp->end_pos - dp->cur_pos);
                        if (r_size > remained_size)
                        {
                            max_eh = i;
                            remained_size = r_size;
                            max_dp = dp;
                        }
                    }
                }

                if (remained_size > M && max_eh != -1)
                {
                    PDEBUG ("Rescheduling....\n");

                    curl_multi_remove_handle(mh, ehs[max_eh]);
                    eh_mask[max_eh] = 1;
                    free_dps ++;

                    data_chunk* ndc = NULL;
                    if (chunk_split(max_dp->cur_pos,
                                    max_dp->end_pos - max_dp->cur_pos,
                                    &free_dps, &ndc))
                    {
                        int lastIdx = 0;
                        for (int i = 0; i < free_dps; ++i)
                        {
                            for (int j=lastIdx; j < mw.md->hd.nr_chunks; ++j)
                            {
                                if (eh_mask[j])
                                {
                                    *(mw.md->body+j) = ndc[i];
                                    curl_multi_add_handle(mh, ehs[j]);
                                    lastIdx = j++;
                                    break;
                                }
                            }
                        }
                    }
                }
                else
                {
                    reschedule = false;
                }
            }

            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_ZERO(&efds);
            curl_multi_fdset(mh, &rfds, &wfds, &efds, &maxfd);
            curl_multi_perform(mh, running_hanlders);

            static int cnt = 0;
            if (nr == 0)
            {
                if (++cnt > 10)
                {
                    PDEBUG ("timeout, running handles: %d\n", *running_hanlders);
                    cnt = 0;
                }
            }
            else
                cnt = 0;
        }
        else
        {
            PDEBUG ("failed to select...\n");
            break;
        }
    }
    fhandle_msync(fm2);
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
    fhandle_msync(mw.fm);
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
}
