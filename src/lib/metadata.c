#include "metadata.h"
#include <stdlib.h>
#include "fileutils.h"
#include <math.h>
#include <string.h>
#include <assert.h>
#include "debug.h"

#define MAX_CHUNKS       10
#define MAX_CHUNK_SIZE   (512*K)

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


#define SHOW_CHUNK(p)    PDEBUG ("%p, cur_pos: %08llX, end_pos: %08llX\n",p,p->cur_pos, p->end_pos)
bool chunk_split(uint64 start, uint64 size, int *num, data_chunk** dc);

bool metadata_create_from_file(const char* fn, metadata_wrapper* mw)
{
    fhandle*  fh = NULL;
    fh_map*   fm = NULL;
    if (mw && (fh = fhandle_create(fn, FHM_DEFAULT)) &&
        (fm = fhandle_mmap(fh, 0, fh->size)))
    {
        mw->fm = fm;
        mw->from_file = true;
        mw->md = (metadata*)fm->addr;
        mw->md->url = GET_URL(mw->md);
        return true;
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

bool metadata_create_from_url(const char* url, uint64 size,
                              uint64 start_pos, int nc,
                              metadata** md)
{
    data_chunk* dc = NULL;
    if (!md || !chunk_split(start_pos, size, &nc, &dc) || !dc)
    {
        PDEBUG ("return err.\n");
        return false;
    }

    size_t m_size = MH_SIZE() + (sizeof(data_chunk)*nc) + (url ? PA(strlen(url), 4) : 256);

    metadata* pmd = (metadata*)malloc(m_size);
    if (pmd)
    {
        *md = pmd;
        memset(pmd, 0, m_size);
        pmd->total_size = size;
        pmd->nr_chunks = nc;
        pmd->url_length = url ? strlen(url):0;

        for (int i = 0; i < nc; ++i)
        {
            data_chunk* p = dc+i;
            pmd->body[i] = *p;
        }
        if (url)
        {
            sprintf(GET_URL(pmd), "%s", url);
            pmd->url = GET_URL(pmd);
        }
    }
    return true;
}

void associate_wrapper(metadata_wrapper* mw)
{
    if (!mw || mw->from_file)
    {
        return;
    }

    // Dump metadata to fm, close fm, and free md.
    // TODO: Check size and remap if necessary.
    metadata* nmd = (metadata*)mw->fm->addr;
    metadata* omd = mw->md;
    nmd->total_size = omd->total_size;
    nmd->nr_chunks = omd->nr_chunks;
    nmd->url_length = omd->url_length;
    nmd->url = GET_URL(nmd);
    if (omd->url)
    {
        sprintf(nmd->url, "%s", omd->url);
    }

    for (uint8 i = 0; i < mw->md->nr_chunks; ++i)
    {
        data_chunk* p = &mw->md->body[i];
        nmd->body[i] = mw->md->body[i];
    }
}

void metadata_destroy(metadata_wrapper* mw)
{
    if (!mw)
    {
        return;
    }

    if (mw->from_file) // created from file, just close it.
    {
        fhandle_munmap_close(&mw->fm);
        return;
    }

    // Dump metadata to fm, close fm, and free md.
    // TODO: Check size and remap if necessary.
    metadata* nmd = (metadata*)mw->fm->addr;
    metadata* omd = mw->md;
    nmd->total_size = omd->total_size;
    nmd->nr_chunks = omd->nr_chunks;
    nmd->url_length = omd->url_length;
    nmd->url = GET_URL(nmd);
    if (omd->url)
    {
        sprintf(nmd->url, "%s", omd->url);
    }

    for (uint8 i = 0; i < mw->md->nr_chunks; ++i)
    {
        data_chunk* p = &mw->md->body[i];
        nmd->body[i] = mw->md->body[i];
    }

    fhandle_munmap_close(&mw->fm);
    free(mw->md);
    mw->md = NULL;
}

void metadata_display(metadata* md)
{
    PDEBUG ("Showing metadata: %p\n", md);
    if (!md)
    {
        PDEBUG ("Empty metadata!%s\n", "");
        return;
    }

    PDEBUG ("size: %08llX (%.2f)M, nc: %d, url_length: %08X, url: %p -- %s\n",
            md->total_size, (float)md->total_size/(1*M), md->nr_chunks,
            md->url_length, md->url, md->url, md->url);

    for (uint8 i = 0; i < md->nr_chunks; ++i)
    {
        data_chunk* cp = &md->body[i];
        PDEBUG ("Chunk: %p, cur_pos: %08llX, end_pos: %08llX(%.2fM)\n",
                    cp, cp->cur_pos, cp->end_pos, (float)cp->end_pos/(M));

    }
}

bool chunk_split(uint64 start, uint64 size, int *num, data_chunk** dc)
{
    if (!size || !dc || !num)
    {
        return false;
    }

    if (*num <= 0)
    {
        *num = 1;
    }
    else
    {
        *num = *num > 10 ? 10 : *num; // max to 10 chunks.
    }

    uint64 cs = size / *num;
    if (cs <= MAX_CHUNK_SIZE)
    {
        cs = MAX_CHUNK_SIZE;
    }
    else
    {
        cs = ((uint64)1 << ((int)log2(cs / (1*M)) + 1)) * M;
    }

    uint32 total_size = *num * sizeof(data_chunk);
    *dc = (data_chunk*)malloc(total_size);
    memset(*dc, 0, total_size);
    data_chunk* dp = *dc;
    for (int i = 0; i < *num; ++i)
    {
        dp            = *dc+i;
        dp->cur_pos   = i * cs;
        dp->end_pos = dp->cur_pos + cs;
        if (dp->end_pos >= size)
        {
            dp->end_pos = size;
            *num = i+1;
            break;
        }
    }

    return true;
}



