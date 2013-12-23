#include "metadata.h"
#include <stdlib.h>
#include "fileutils.h"
#include <math.h>
#include <string.h>
#include <assert.h>
#include "debug.h"
#include "timeutil.h"
#include "mget_config.h"
#include <stdio.h>

#define MAX_CHUNKS       10
#define MIN_CHUNK_SIZE   (64*K)

#define SHOW_CHUNK(p)    PDEBUG ("%p, cur_pos: %08llX, end_pos: %08llX\n",p,p->cur_pos, p->end_pos)

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

        //TODO: version checks....
        mw->md->body  = (data_chunk*)mw->md->raw_data;
        char* ptr     = GET_URL(mw->md);
        mw->md->url   = ptr;

        ptr          += strlen(mw->md->url) + 1;
        mw->md->mime  = ptr;

        ptr                += strlen(mw->md->mime) + 1;
        mw->md->ht = NULL; //TODO: parse and initialize hash table.
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

bool metadata_create_from_url(const char* url,
                              uint64      size,
                              int         nc,
                              mget_slis*  lst,
                              metadata** md)
{
    data_chunk* dc = NULL;
    if (!md || !chunk_split(0, size, &nc, &dc) || !dc)
    {
        PDEBUG ("return err.\n");
        return false;
    }

    uint16 ebl = PA(strlen(url), 4) + 512; // TODO: calculate real eb_length from lst.
    size_t md_size = MH_SIZE() + (sizeof(data_chunk)*nc) + ebl;

    metadata* pmd = (metadata*)malloc(md_size);
    if (pmd)
    {
        *md = pmd;
        memset(pmd, 0, md_size);
        mh* hd = &pmd->hd;
        sprintf(((char*) &hd->iden), "TMD");
        hd->version = GET_VERSION();
        hd->package_size = size;
        hd->last_time = get_time_s();
        hd->acc_time = 0;
        hd->status = RS_INIT;
        hd->nr_chunks = nc;
        hd->eb_length = ebl;

        pmd->body = (data_chunk*)pmd->raw_data;
        char* ptr = pmd->raw_data + CHUNK_SIZE(pmd);
        pmd->url  = ptr;
        if (url)
        {
            sprintf(pmd->url, "%s", url);
        }

        ptr += strlen(pmd->url) + 1;
        pmd->mime = ptr;
        ptr += strlen(pmd->mime) + 1;
        pmd->ht = NULL; // TODO: Initialize hash table based on ptr.

        for (int i = 0; i < nc; ++i)
        {
            data_chunk* p = dc+i;
            pmd->body[i] = *p;
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

    // Dump metadata to fm
    // TODO: Check size and remap if necessary.
    metadata* nmd   = (metadata*)mw->fm->addr;
    metadata* omd   = mw->md;
    nmd->hd  = omd->hd;
    nmd->url = GET_URL(nmd);
    nmd->body = (data_chunk*)nmd->raw_data;
    if (omd->url)
        sprintf(nmd->url, "%s", omd->url);
    else
        nmd->url = NULL;

    for (uint8 i = 0; i < mw->md->hd.nr_chunks; ++i)
    {
        data_chunk* p = &mw->md->body[i];
        nmd->body[i]  = mw->md->body[i];
    }

    nmd->ht = omd->ht;
    omd->ht = NULL;

    mw->md = nmd;
    mw->from_file  = true;
}

void metadata_destroy(metadata_wrapper* mw)
{
    if (!mw)
    {
        return;
    }

    if (mw->from_file) // created from file, just close it.
    {
        if (mw->md->hd.status == RS_SUCCEEDED)
        {
            mw->fm->fh->auto_remove = true;
        }
        fhandle_munmap_close(&mw->fm);
        return;
    }

    assert(0);
// TODO: Remove this ifdef!
#if 0

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
#endif // End of #if 0
}

void metadata_display(metadata* md)
{
    fprintf(stderr, "Showing metadata: %p\n", md);
    if (!md)
    {
        fprintf(stderr, "Empty metadata!%s\n", "");
        return;
    }

    PDEBUG ("size: %08llX (%.2f)M, nc: %d,url: %p -- %s\n",
            md->hd.package_size, (float)md->hd.package_size/(1*M), md->hd.nr_chunks,
            md->url, md->url, md->url);

    uint64 recv = 0;
    for (uint8 i = 0; i < md->hd.nr_chunks; ++i)
    {
        data_chunk* cp = &md->body[i];
        uint64 chunk_recv = cp->cur_pos - cp->start_pos;
        uint64 chunk_size = cp->end_pos - cp->start_pos;
        recv += chunk_recv;
        fprintf(stderr,
                "Chunk: %p(%s), start: %08llX, cur: %08llX, end: %08llX -- %.02f%%\n",
                cp, stringify_size(chunk_size), cp->start_pos, cp->cur_pos,
                cp->end_pos, (float)(chunk_recv)/chunk_size * 100);

    }
    fprintf(stderr, "%s finished...\n", stringify_size(recv));
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
    if (cs <= MIN_CHUNK_SIZE)
    {
        cs = MIN_CHUNK_SIZE;
    }
    else
    {
        cs = ((uint64)1 << ((int)log2(cs / MIN_CHUNK_SIZE) + 1)) * MIN_CHUNK_SIZE;
    }

    uint32 total_size = *num * sizeof(data_chunk);
    *dc = (data_chunk*)malloc(total_size);
    memset(*dc, 0, total_size);
    data_chunk* dp = *dc;
    for (int i = 0; i < *num; ++i)
    {
        dp               = *dc+i;
        dp->start_pos    = i * cs;
        dp->cur_pos      = i *   cs;
        dp->end_pos      = dp->cur_pos + cs;
        if (dp->end_pos >= size)
        {
            dp->end_pos = size;
            *num = i+1;
            break;
        }
    }
    PDEBUG ("results: chunk_size: %.02fM, nc: %d\n",
            (float)cs/M, *num);

    return true;
}

