#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <string.h>

#include "mget_http.h"
#include "debug.h"
#include "macros.h"

typedef struct _easy_param
{
    metadata* md;
    int       idx;
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
    static int i = 0;
    easy_param* ppm = (easy_param*) userp;
    data_chunk* dp = &ppm->md->body[ppm->idx];
    PDEBUG ("dp: %p, cur_pos: %.02fM, end_pos: %.02fM\n",
            dp, (float)(dp->cur_pos)/M, (float)(dp->end_pos)/M);
    dp->cur_pos += size*nmemb;
    if ((dp->end_pos - dp->cur_pos) / (4*K) % 2 == 0 || dp->cur_pos == dp->end_pos)
    {
        PDEBUG ("dp: %p, cur_pos: %.02fM, end_pos: %.02fM\n",
                dp, (float)(dp->cur_pos)/M, (float)(dp->end_pos)/M);
        if (g_cb)
        {
            PDEBUG ("notify..\n");

            (*g_cb)(ppm->md);
        }
    }
    return size*nmemb;
}

size_t get_size_from_header(void *buffer,  size_t size, size_t nmemb,
                            void *userp)
{
    const char* ptr = (const char*)buffer;
    if ((strstr(ptr, "Content-Range")) != NULL)
    {
        uint64 s = 0, e = 0;
        sscanf(ptr, "Content-Range: bytes %Lu-%Lu/%Lu", &s, &e,
               (uint64*)userp);
    }
    return size*nmemb;
}

uint64 get_remote_file_size(url_info* ui, CURL* eh)
{
    //@todo: handle 302!!
    uint64 size = 0;
    curl_easy_setopt(eh, CURLOPT_URL, ui->furl);
    curl_easy_setopt(eh, CURLOPT_HEADERFUNCTION, get_size_from_header);
    curl_easy_setopt(eh, CURLOPT_HEADERDATA, &size);

    struct curl_slist* slist = NULL;
    slist = curl_slist_append(slist, "Accept: */*");
    slist = curl_slist_append(slist, "Range: bytes=0-0");
    curl_easy_setopt(eh, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, drop_content);

    curl_easy_perform(eh);
    long stat = 0;
    curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &stat);
    curl_slist_free_all(slist);

    PDEBUG ("remote file size: %llu (%.02fM)\n", size, (float)size/M);
    return size;
}

void process_http_request(url_info* ui, const char* dp, int nc,
                          void (*cb)(metadata* md))
{
    PDEBUG ("enter\n");

    PDEBUG ("cb: %p\n",cb);

    g_cb = cb;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* ehs[nc];
    ehs[0] = curl_easy_init();
    char fn [256] = {'\0'};
    sprintf(fn, "%s/%s.tmd", dp, ui->bname);
    metadata_wrapper mw;
    if (file_existp(fn) && metadata_create_from_file(fn, &mw))
    {
        goto l1;
    }

    remove_file(fn);
    uint64 total_size = get_remote_file_size(ui, ehs[0]);
    if (!metadata_create_from_url(ui->furl, total_size, 0, nc, &mw.md))
    {
        return;
    }

    fhandle* fh = fhandle_create(fn, FHM_CREATE);
    mw.fm = fhandle_mmap(fh, 0, MD_SIZE(mw.md));
    mw.from_file = false;

    metadata_display(mw.md);
    associate_wrapper(&mw);

l1:

    ;
    // Initialize curl_multi
    CURLM* mh = curl_multi_init();

    for (int i = 1; i < mw.md->nr_chunks; ++i)
    {
        ehs[i] = curl_easy_init();
    }

    easy_param* params = ZALLOC(easy_param, mw.md->nr_chunks);
    for (int i = 0; i < mw.md->nr_chunks; ++i)
    {
        CURL* eh = ehs[i];
        data_chunk* dp  = &mw.md->body[i];
        easy_param* ppm = params+i;
        ppm->md = mw.md;
        ppm->idx = i;

        curl_easy_setopt(eh, CURLOPT_URL, ui->furl);
        curl_easy_setopt(eh, CURLOPT_HEADERFUNCTION, drop_content);
        curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, recv_data);
        curl_easy_setopt(eh, CURLOPT_WRITEDATA, ppm);

        struct curl_slist* l = curl_slist_append(NULL, "Accept: */*");
        char rg[64] = {'\0'};
        sprintf(rg, "Range: bytes=%llu-%llu", dp->cur_pos, dp->end_pos);
        l = curl_slist_append(l, rg);
        curl_easy_setopt(eh, CURLOPT_HTTPHEADER, l);

        // Add to multy;
        curl_multi_add_handle(mh, eh);
    }


    int running_hanlders = 0;
    curl_multi_perform(mh, &running_hanlders);

    while (running_hanlders) {
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

    PDEBUG ("stopped.\n");
}
