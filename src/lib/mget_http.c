#include "mget_http.h"
#include "mget_sock.h"
#include "debug.h"
#include "timeutil.h"

uint64 get_remote_file_size_http(url_info* ui)
{
    msock* sk = socket_get(ui->host, NULL, NULL);
    bool ret = socket_perform(sk);
    if (!ret)
    {
        PDEBUG ("Failed ....\n");
    }
    return 0;
}

void process_http_request(url_info* ui, const char* dn, int nc,
                          void (*cb)(metadata* md), bool* stop_flag)
{
    PDEBUG ("enter\n");

    PDEBUG ("cb: %p\n",cb);

    char fn [256] = {'\0'};
    sprintf(fn, "%s/%s.tmd", dn, ui->bname);

    metadata_wrapper mw;
    if (file_existp(fn) && metadata_create_from_file(fn, &mw))
    {
        goto l1;
    }

    remove_file(fn);
    uint64 total_size = get_remote_file_size_http(ui);

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
