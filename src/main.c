#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "fileutils.h"
#include "metadata.h"
#include "debug.h"

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[])
{
    PDEBUG ("Test begins, PID: %d\n", getpid());

    uint32      start = 0;
    uint32      size  = 533 * M;
    const char* url   = "http://www.baidu.com";
    metadata_wrapper mw;
    data_chunk* cp    = NULL;

    bool b_re = metadata_create_from_url(url, 433*M, 0, 9, &mw.md);
    if (!b_re)
    {
        handle_error("Failed to create metadata from url\n");
    }

    for (int i = 0; i < 9; ++i)
    {
        cp = &mw.md->body[i];
        PDEBUG ("chunk: %p, cur_pos: %08X, end: %08X (%.02fM)\n",
                cp, cp->cur_pos, cp->end_pos, (float)cp->end_pos/(M));
    }

    metadata_display(mw.md);

    const char* tmp = "/tmp/test.txt";
    fhandle* fh = fhandle_create(tmp, FHM_CREATE);
    PDEBUG ("after handle created.\n");
    metadata_display(mw.md);
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
    return 0;
}
