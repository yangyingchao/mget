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


#define SHOW_CHUNK(p)    PDEBUG ("%p, cur_pos: %08X, end_pos: %08X\n",p,p->cur_pos, p->end_pos)
bool chunk_split(uint32 start, uint32 size, int num, data_chunk** dc);

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

bool metadata_create_from_url(const char* url, uint32 size,
                              uint32 start_pos, int nc,
                              metadata** md)
{
    data_chunk* dc = NULL;
    if (!md || !chunk_split(start_pos, size, nc, &dc) || !dc)
    {
        PDEBUG ("return err.\n");
        return false;
    }

    size_t m_size = MH_SIZE() + (sizeof(data_chunk)*nc) + (url ? PA_4B(strlen(url)) : 256);

    metadata* pmd = (metadata*)malloc(m_size);
    if (pmd)
    {
        PDEBUG ("pmd: %p, size: %lu, end: %p\n",
                pmd, m_size, (char*)pmd+m_size);

        *md = pmd;
        memset(pmd, 0, m_size);
        pmd->total_size = size;
        SET_NC(pmd, nc);
        SET_UL(pmd, (url ? strlen(url):0));

        for (int i = 0; i < nc; ++i)
        {
            data_chunk* p = dc+i;
            PDEBUG ("%p, cur_pos: %08X, end_pos: %08X\n",
                    p, p->cur_pos, p->end_pos);
            pmd->body[i] = *p;
            p = &pmd->body[i];
            PDEBUG ("%p, cur_pos: %08X, end_pos: %08X\n",
                    p, p->cur_pos, p->end_pos);
        }
        if (url)
        {

            sprintf(GET_URL(pmd), "%s", url);
            PDEBUG ("url: %p, %s\n",GET_URL(pmd) , GET_URL(pmd));
            pmd->url = GET_URL(pmd);
            PDEBUG ("pmd->url: %p, %s\n",pmd->url , pmd->url);
        }
    }
    return true;
}

void metadata_destroy(metadata_wrapper* mw)
{
    if (!mw)
    {
        return;
    }

    if (mw->from_file) // created from file, just close it.
    {
        PDEBUG ("Closing file: %s\n", mw->fm->fh->fn);

        fhandle_munmap_close(&mw->fm);
        return;
    }

    // Dump metadata to fm, close fm, and free md.
    // TODO: Check size and remap if necessary.
    metadata* nmd = (metadata*)mw->fm->addr;
    metadata* omd = mw->md;
    nmd->total_size = omd->total_size;
    SET_NC(nmd, GET_NC(omd));
    SET_UL(nmd, GET_UL(omd));
    nmd->url = GET_URL(nmd);
    if (omd->url)
    {
        sprintf(nmd->url, "%s", omd->url);
    }

    for (int i = 0; i < GET_NC(mw->md); ++i)
    {
        data_chunk* p = &mw->md->body[i];
        SHOW_CHUNK(p);
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

    PDEBUG ("size: %08X (%.2f)M, nc: %d, url_length: %08X, url: %p -- %s\n",
            md->total_size, (float)md->total_size/(1*M), GET_NC(md), GET_UL(md),
            md->url, md->url);

    for (int i = 0; i < GET_NC(md); ++i)
    {
        data_chunk* cp = &md->body[i];
        PDEBUG ("Chunk: %p, cur_pos: %08X, end_pos: %08X(%.2fM)\n",
                    cp, cp->cur_pos, cp->end_pos, (float)cp->end_pos/(M));

    }
}

bool chunk_split(uint32 start, uint32 size, int num, data_chunk** dc)
{
    if (!size || num <= 0 || !dc)
    {
        return false;
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
    *dc = (data_chunk*)malloc(total_size);
    memset(*dc, 0, total_size);
    data_chunk* dp = *dc;
    for (int i = 0; i < num; ++i)
    {
        dp            = *dc+i;
        dp->cur_pos   = i * cs;
        dp->end_pos = dp->cur_pos + cs;
    }

    return true;
}



