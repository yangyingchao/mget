#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "libmget.h"

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[])
{
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

    url = "https://launchpadlibrarian.net/149149704/python-mode.el-6.1.2.tar.gz";
    start_request(url, "/tmp/", 9, NULL);

    return 0;
}
