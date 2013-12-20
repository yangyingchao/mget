#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "libmget.h"
#include "timeutil.h"

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

void show_progress(metadata* md)
{
    static int idx = 0;
    static uint32 ts = 0;
    static uint64 last_recv = 0;

    if (idx++ < 78)
        putchar('.');
    else
    {
        data_chunk* dp = md->body;
        uint64 total = md->total_size;
        uint64 miss  = 0;
        for (int i = 0; i < md->nr_chunks; ++i)
        {
            miss += dp->end_pos - dp->cur_pos;
            dp++;
        }
        uint64 recv = total - miss;
        printf("Progress: total: %llu, recv: %llu, %.02f percent, %.02fKB/s\n",
               total, recv, (float)recv/total * 100,
               (float)(recv-last_recv)/K/(get_time_s() -ts));
        idx       = 0;
        last_recv = recv;
        ts = get_time_s();
    }
}

int main(int argc, char *argv[])
{
// TODO: Remove this ifdef!
#if 0

if (argc != 3)
    {
        printf("Usage: %s size, nc.\n", argv[0]);
        return 1;
    }

    uint32      start = 0;
    uint64      size  = (uint64)atoi(argv[1]) * M;
    int nc = atoi(argv[2]);
    PDEBUG ("Test begins, size: %llX (%.02fM)\n", size, (float)size/M);
    const char* url   = "http://www.baidu.com";
    metadata_wrapper mw;
    data_chunk* cp    = NULL;

    bool b_re = metadata_create_from_url(url, size, 0, nc, &mw.md);
    if (!b_re)
    {
        handle_error("Failed to create metadata from url\n");
    }

    for (int i = 0; i < mw.md->nr_chunks; ++i)
    {
        cp = &mw.md->body[i];
        PDEBUG ("chunk: %p, cur_pos: %08llX, end: %08llX (%.02fM)\n",
                cp, cp->cur_pos, cp->end_pos, (float)cp->end_pos/(M));
    }

    metadata_display(mw.md);

    const char* tmp = "/tmp/test.txt";
    fhandle* fh = fhandle_create(tmp, FHM_CREATE);
    mw.fm = fhandle_mmap(fh, 0, MD_SIZE(mw.md));
    mw.from_file = false;

    PDEBUG ("Destroying mw: %p, md: %p\n", &mw, mw.md);

    metadata_destroy(&mw);

    printf("Now md: %p, recreating....\n", mw.md);
    b_re = metadata_create_from_file(tmp, &mw);
    if (b_re)
    {
        metadata_display(mw.md);
        metadata_destroy(&mw);
    }
    else
    {
        PDEBUG ("Faield to create mw from file: %s\n", tmp);
    }


    PDEBUG ("Test Staring request!\n");
#endif // End of #if 0
    /* const char* url = "http://mirrors.163.com/gentoo/distfiles/xterm-297.tgz"; */
    const char* url = "http://mirrors.163.com/gentoo/distfiles/curl-7.33.0.tar.bz2";
    start_request(url, "/tmp/", 9, show_progress);
    return 0;
}
