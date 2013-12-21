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
        char* k = NULL;
        char* v = NULL;
        if (sscanf((const char*)buffer, "%m[^:] : %m[^$] ", &k, &v))
        {
            if (!InsertEntry(ht, rstrip(k), rstrip(v)))
            {
                PDEBUG ("Failed to add kvp: %s, %s\n", k, v);
                FIF(k); FIF(v);
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

    hash_table* ht = hash_tableCreate(256, free);

    //@todo: handle 302!!
    uint64 size = 0;
    curl_easy_setopt(eh0, CURLOPT_URL, ui->furl);
    curl_easy_setopt(eh0, CURLOPT_HEADERFUNCTION, recv_header);
    curl_easy_setopt(eh0, CURLOPT_HEADERDATA, ht);

    struct curl_slist* slist = NULL;
    slist = curl_slist_append(slist, "Accept: */*");
    slist = curl_slist_append(slist, "Range: bytes=0-0");
    curl_easy_setopt(eh0, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(eh0, CURLOPT_WRITEFUNCTION, drop_content);
    CURLcode ret = curl_easy_perform(eh0);
    PDEBUG ("ret = %d\n",ret);


    long stat = 0;
    curl_easy_getinfo(eh0, CURLINFO_RESPONSE_CODE, &stat);

    PDEBUG ("Status Code: %ld\n", stat);

    char* val = (char*)GetEntryFromhash_table(ht, "Content-Range");
    if (val)
    {
        uint64 s = 0, e = 0;
        int n = sscanf(val, "bytes %Lu-%Lu/%Lu", &s, &e, &size);
    }

    PDEBUG ("remote file size: %llu (%.02fM)\n", size, (float)size/M);
    curl_slist_free_all(slist);
    return size;
}

void process_http_request_c(url_info* ui, const char* dn, int nc,
                            void (*cb)(metadata* md), bool* stop_flag)
{
    PDEBUG ("enter\n");

    PDEBUG ("cb: %p\n",cb);

    g_cb = cb;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    char fn [256] = {'\0'};
    sprintf(fn, "%s/%s.tmd", dn, ui->bname);
    metadata_wrapper mw;
    if (file_existp(fn) && metadata_create_from_file(fn, &mw))
    {
        goto l1;
    }

    remove_file(fn);
    uint64 total_size = get_remote_file_size(ui);

    mget_slis* lst = NULL; //TODO: fill this lst.
    if (!metadata_create_from_url(ui->furl, total_size, nc, lst, &mw.md))
    {
        return;
    }

    fhandle* fh  = fhandle_create(fn, FHM_CREATE);
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

    memset(fn, 0, 256);
    sprintf(fn, "%s/%s", dn, ui->bname);
    PDEBUG ("fn: %s, bname: %s\n", fn, ui->bname);

    fhandle* fh2 = fhandle_create(fn, FHM_CREATE);
    fh_map*  fm2 = fhandle_mmap(fh2, 0, mw.md->hd.package_size);
    if (!fh2 || !fm2)
    {
        PDEBUG ("Failed to create mapping!\n");
        // TODO: cleanup...
        return;
    }

    easy_param* params = ZALLOC(easy_param, mw.md->hd.nr_chunks);

    struct curl_slist* flist = NULL; // used to record allocated resourcs...

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
    int running_hanlders = 0;
    curl_multi_perform(mh, &running_hanlders);
    PDEBUG ("perform returns: %d\n",running_hanlders);

    while (running_hanlders && stop_flag && !*stop_flag) {
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
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_ZERO(&efds);
            curl_multi_fdset(mh, &rfds, &wfds, &efds, &maxfd);
            curl_multi_perform(mh, &running_hanlders);
            if (nr == 0)
            {
                PDEBUG ("timeout, running handles: %d\n", running_hanlders);
            }
        }
        else
        {
            PDEBUG ("failed to select...\n");
            break;
        }
    }
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

    sync();
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
}
