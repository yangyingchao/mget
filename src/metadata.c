#include "metadata.h"
#include <stdlib.h>
#include "fileutils.h"
#include <math.h>
#include <string.h>
#include <assert.h>
#include "debug.h"

#define MAX_CHUNKS       10


// Byte/     0       |       1       |       2       |       3       |
// ***/              |               |               |               |
// **|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|
// **+---------------------------------------------------------------+
// **|                        Total Size                             |
// **+---------------+-----------------------------------------------+
// **| Num of Chunks |          RESERVED                             |
// **+---------------+-----------------------------------------------+
// **|                     CHUNK1 START                              |
// **+---------------------------------------------------------------+
// **|                     ......                                    |
// **+---------------------------------------------------------------+
// **|                     .....                                     |
// **+---------------------------------------------------------------+
// **|                     CHUNK1 END                                |
// **+---------------------------------------------------------------+
// **|                     CHUNK2 START                              |
// **+---------------------------------------------------------------+
// **|                     ......                                    |
// **+---------------------------------------------------------------+
// **|                     .....                                     |
// **+---------------------------------------------------------------+
// **|                     CHUNK2 END                                |
// **+---------------------------------------------------------------+
// **|                     ...........                               |
// **+---------------------------------------------------------------+


metadata* metadata_create_from_file(const char* fn)
{
    fhandle*  fh = NULL;
    fh_map*   fm = NULL;
    metadata* md = NULL;
    if ((fh = fhandle_create(fn, FHM_DEFAULT)) &&
        (fm = fhandle_mmap(fh, 0, fh->size)))
    {
        md = (metadata*)fm->addr;

        data_chunk* dp  = md->body;
        uint8 nc = md->head.nr_chunks;
        md->head.mp = (void*)fm;
        md->head.from_file = true;
        return md;
    }

    if (fm)
    {
        fhandle_munmap(&fm);
    }
    if (fh)
    {
        fhandle_destroy(&fh);
    }

    return NULL;
}

void metadata_destroy(metadata** md)
{
    if (!md || !*md)
    {
        return;
    }

    metadata_head* hd = &(*md)->head;
    fh_map* fm = (fh_map*)hd->mp;
    if (hd->from_file) // created from file, just close it.
    {
        fhandle_munmap_close(&fm);
        return;
    }

    // Dump metadata to fm, close fm, and free md.
    metadata* nmd = (metadata*)fm->addr;
    nmd->head = *hd; // copy header;
    data_chunk* dp = NULL;
    for (uint8 i = 0; i < hd->nr_chunks; ++i)
    {
        *(nmd->body + i) = *((*md)->body + i);
    }

    fhandle_munmap_close(&fm);
    free(*md);
    *md = NULL;
}

void metadata_display(metadata* md)
{
    PDEBUG ("Showing metadata: %p\n", md);
    if (!md)
    {
        PDEBUG ("Empty metadata!%s\n", "");
        return;
    }

    metadata_head* hd = &md->head;
    PDEBUG ("size: %08X (%.2f)M, nc: %d, from_file: %s, fn: %s\n",
            hd->total_size, (float)hd->total_size/(1*M), hd->nr_chunks,
            hd->from_file ? "true" : "false",
            (hd->mp &&  ((fh_map*)hd->mp)->fh->fn) ? ((fh_map*)hd->mp)->fh->fn : "nil");


    for (uint8 i = 0; i < hd->nr_chunks; ++i)
    {
        data_chunk* cp = &md->body[i];
        PDEBUG ("Chunk: %p, cur_pos: %08X, end_pos: %08X\n",
                    cp, cp->cur_pos, cp->end_pos, (float)cp->end_pos/(M));

    }
}

data_chunk* chunk_split(uint32 start, uint32 size, int num)
{
    if (!size || num <= 0)
    {
        return NULL;
    }

    num = num > 10 ? 10 : num; // max to 10 chunks.
    uint32 cs = size / num;
    if (cs < 1*M)
    {
        cs = 1*M;
    }
    else
    {
        cs = (1 << ((int)log2(cs / (1*M)) + 1)) * M;
    }

    uint32 total_size = num * sizeof(data_chunk);
    data_chunk* ds = (data_chunk*)malloc(total_size);
    memset(ds, 0, total_size);
    data_chunk* dp = ds;
    for (int i = 0; i < num; ++i)
    {
        dp            = ds+i;
        dp->cur_pos   = i * cs;
        dp->end_pos = dp->cur_pos + cs;
    }

    return ds;
}
