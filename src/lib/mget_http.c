#include "mget_http.h"
#include "mget_sock.h"
#include "debug.h"
#include "timeutil.h"
#include <unistd.h>

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

int dissect_header(const char* buffer, size_t length, size_t* dsize, hash_table** ht)
{
    if (!ht || !buffer)
    {
        return -1;
    }

    hash_table* pht = hash_table_create(256, free);
    assert(pht);
    *ht = pht;

    const char* ptr = buffer;
    int   num   = 0;
    int   stat  = 0;
    char* value = NULL;
    int   n     = 0;
    size_t ldsize = 0;

    if (!*dsize) // dsize is zero, means no header is parsed yet.
    {
        num = sscanf(ptr, "HTTP/1.1 %m[^\r\n]\r\n%n", &value, &n);
        char* key = strdup("Status");
        hash_table_insert(pht, key, value);
        ldsize += n;
        ptr    += n;

        num = sscanf(value, "%d", &stat);
        assert(num);
    }

    while (ptr < buffer + length) {
        char* k = NULL;
        char* v = NULL;
        if (sscanf((const char*)ptr, "%m[^:]: %m[^\r\n]\r\n%n", &k, &v, &n))
        {
            hash_table_insert(pht, k, v);
            ldsize += n;
            ptr += n;
        }
    }

    if (dsize)
    {
        *dsize = ldsize;
    }

    return stat;
}

uint64 get_remote_file_size_http(url_info* ui)
{
    msock* sk = socket_get(ui, NULL, NULL);
    if (!sk)
    {
        fprintf(stderr, "Failed to get socket!\n");
        return 0;
    }

    char* hd = generate_request_header("GET", ui, 0, 0);
    write(sk->sock, hd, strlen(hd));
    free(hd);

    char buffer[4096];
    memset(buffer, 0, 4096);
    size_t rd = read(sk->sock, buffer, 4096);
    printf("bytes: %lu, content: %s", rd, buffer);

    hash_table* ht    = NULL;
    size_t      dsize = 0;
    int         stat  = dissect_header(buffer, rd, &dsize, &ht);

    PDEBUG ("stat: %d, description: %s\n",
            stat, (char*)hash_table_entry_get(ht, "Status"));

    if (stat != 206)
    {
        fprintf(stderr, "Not implemented for status code: %d\n", stat);
        return 0;
    }

    char* ptr = (char*)hash_table_entry_get(ht, "Content-Range");
    if (!ptr)
    {
        fprintf(stderr, "Content Range not returned!\n");
        return 0;
    }

    uint64 s, e, t;
    int num = sscanf(ptr, "bytes %Lu-%Lu/%Lu",
                     &s, &e, &t);
    if (!num)
    {
        fprintf(stderr, "Failed to parse string: %s\n", ptr);
    }
    else
    {
        // Check http headers and update sock_features....
    }

    return t;
}

void process_http_request(url_info* ui, const char* dn, int nc,
                          void (*cb)(metadata* md), bool* stop_flag)
{
    PDEBUG ("enter\n");

    char fn [256] = {'\0'};
    sprintf(fn, "%s/%s.tmd", dn, ui->bname);

    metadata_wrapper mw;
    if (file_existp(fn) && metadata_create_from_file(fn, &mw))
    {
        goto l1;
    }

    remove_file(fn);
    uint64 total_size = get_remote_file_size_http(ui);
    PDEBUG ("total_size: %llu\n", total_size);


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

    /* easy_param* params = ZALLOC(easy_param, mw.md->hd.nr_chunks); */

    bool need_request = false;
    for (int i = 0; i < mw.md->hd.nr_chunks; ++i)
    {
        data_chunk* dp  = &mw.md->body[i];
        if (dp->cur_pos >= dp->end_pos)
        {
            continue;
        }

        need_request = true;

        // ....
    }

    if (!need_request)
    {
        mw.md->hd.status = RS_SUCCEEDED;
        goto ret;
    }

    PDEBUG ("Performing...\n");

    while (stop_flag && !*stop_flag) {
        //...
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
