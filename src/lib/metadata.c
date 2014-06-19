/** metadata.c --- implementation of metadata.
 *
 * Copyright (C) 2013 Yang,Ying-chao
 *
 * Author: Yang,Ying-chao <yangyingchao@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "metadata.h"
#include <stdlib.h>
#include "fileutils.h"
#include <math.h>
#include <string.h>
#include <assert.h>
#include "log.h"
#include "mget_utils.h"
#include <stdio.h>
#include "mget_config.h"

#define MIN_CHUNK_SIZE   (64*K)

#define SHOW_CHUNK(p)    PDEBUG ("%p, cur_pos: %08llX, end_pos: %08llX\n",p,p->cur_pos, p->end_pos)


bool metadata_create_from_file(const char *fn, metadata** md, fh_map** fm_md)
{
    PDEBUG ("enter with fn: %s\n", fn);

    bool     ret = false;
    fhandle *fh  = NULL;
    fh_map  *fm  = NULL;

    if (!fn || !md ||!fm_md)
        goto ret;

    if ((fh = fhandle_create(fn, FHM_DEFAULT)) &&
        (fm = fhandle_mmap(fh, 0, fh->size))) {

        *fm_md = fm;
        metadata* pmd = (metadata *) fm->addr;
        *md = pmd;

        mp* ptrs = ZALLOC1(mp);
        pmd->ptrs = ptrs;

        //TODO: version checks....
        ptrs->body = (data_chunk *) pmd->raw_data;
        ptrs->ht_buffer = (char*)(pmd->raw_data) +
                          sizeof(data_chunk) * pmd->hd.nr_user;

        ptrs->ht = hash_table_create_from_buffer(pmd->ptrs->ht_buffer, pmd->hd.eb_length);
        if (!ptrs->ht)
        {
            fprintf(stderr, "Failed to create hash table from buffer.\n");
        }

        ptrs->url = (char*)hash_table_entry_get(pmd->ptrs->ht, K_URL);
        ptrs->fn = (char*)hash_table_entry_get(pmd->ptrs->ht, K_FN);
        ptrs->user = (char*)hash_table_entry_get(pmd->ptrs->ht, K_USR);
        ptrs->passwd = (char*)hash_table_entry_get(pmd->ptrs->ht, K_PASSWD);

        return true;
    }

    if (fm) {
        fhandle_munmap(&fm);
    }
    if (fh) {
        fhandle_destroy(&fh);
    }

ret:
    return ret;
}

bool  metadata_create_from_url(const char* url,
                              const char* fn,
                              uint64 size,
                              int nc,
                              mget_slis* lst,
                              metadata** md)
{
    data_chunk *dc = NULL;
    uint64 cs = 0;
    if (!md || !chunk_split(0, size, &nc, &cs, &dc) || !dc) {
        PDEBUG("return err.\n");
        return false;
    }

    uint16 ebl = 1024;	// TODO: calculate real eb_length from lst.
    size_t md_size = MH_SIZE() + (sizeof(data_chunk) * nc) + ebl;

    metadata* pmd = (metadata *) malloc(md_size);
    mp* ptrs = ZALLOC1(mp);

    if (!pmd || !ptrs) {
        FIF(pmd);
        FIF(ptrs);
        return false;
    }

    *md = pmd;
    memset(pmd, 0, md_size);
    pmd->ptrs = ptrs;
    mh* hd    = &pmd->hd;

    sprintf(((char *) &hd->iden), "TMD");
    hd->version      = GET_VERSION();
    hd->package_size = size;
    hd->chunk_size   = cs;
    hd->last_time    = get_time_s();
    hd->acc_time     = 0;
    hd->status       = RS_INIT;
    hd->nr_user      = nc;
    hd->nr_effective = 0;
    hd->eb_length    = ebl;
    hd->acon         = nc;

    ptrs->ht = hash_table_create(128, free);
    ptrs->body = (data_chunk *) pmd->raw_data;
    if (url) {
        ptrs->url = strdup(url);
        hash_table_insert(ptrs->ht, strdup(K_URL), ptrs->url, strlen(url));
    }

    if (fn) {
        char *tmp = get_basename(fn);
        ptrs->fn = strdup(tmp);
        free(tmp);
        hash_table_insert(ptrs->ht, strdup(K_FN), ptrs->fn, strlen(ptrs->fn));
    }

    /* ptr += strlen(pmd->mime) + 1; */
    /* pmd->ptrs->ht = NULL;		// TODO: Initialize hash table based on ptr. */

    for (int i = 0; i < nc; ++i) {
        data_chunk *p = dc + i;
        ptrs->body[i] = *p;
    }

    PDEBUG ("A\n");

    metadata_display(*md);
    PDEBUG ("A2\n");
    return true;
}

void metadata_destroy(metadata_wrapper * mw)
{
    if (!mw) {
        return;
    }

    if (mw->from_file)		// created from file, just close it.
    {
        if (mw->md->hd.status == RS_FINISHED) {
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
    metadata *nmd = (metadata *) mw->fm->addr;
    metadata *omd = mw->md;

    nmd->total_size = omd->total_size;
    nmd->nr_chunks = omd->nr_chunks;
    nmd->url_length = omd->url_length;
    nmd->url = GET_URL(nmd);
    if (omd->url) {
        sprintf(nmd->url, "%s", omd->url);
    }

    for (uint8 i = 0; i < mw->md->nr_chunks; ++i) {
        data_chunk *p = &mw->md->body[i];

        nmd->body[i] = mw->md->body[i];
    }

    fhandle_munmap_close(&mw->fm);
    free(mw->md);
    mw->md = NULL;
#endif				// End of #if 0
}

void metadata_display(metadata * md)
{
    fprintf(stderr, "\nShowing metadata: %p\n", md);
    if (!md) {
        fprintf(stderr, "Empty metadata!%s\n", "");
        return;
    }

    fprintf(stderr, "size: %08llX (%.2f)M, nc: %d,url: %s, user: %p, passwd: %p\n",
            md->hd.package_size, (float) md->hd.package_size / (1 * M),
            md->hd.nr_effective, md->ptrs->url,
            md->ptrs->user, md->ptrs->passwd);

    fprintf(stderr, "ptrs: raw_data: %p, chunk: %p, hash_buffer: %p\n",
            md->raw_data, md->ptrs->body, md->ptrs->ht_buffer);
    uint64 recv = 0;

    for (uint8 i = 0; i < md->hd.nr_effective; ++i) {
        data_chunk* cp = (data_chunk*)md->raw_data;
        uint64 chunk_recv = cp->cur_pos - cp->start_pos;
        uint64 chunk_size = cp->end_pos - cp->start_pos;
        char *cs = strdup(stringify_size(chunk_size));
        char *es = strdup(stringify_size(cp->end_pos));

        recv += chunk_recv;
        fprintf(stderr,
                "Chunk: %p -- (%s), start: %08llX, cur: %08llX, end: %08llX (%s) -- %.02f%%\n",
                cp, cs, cp->start_pos, cp->cur_pos,
                cp->end_pos, es, (float) (chunk_recv) / chunk_size * 100);
        free(cs);
        free(es);
    }
    fprintf(stderr, "%s finished...\n\n", stringify_size(recv));
}

bool chunk_split(uint64 start, uint64 size, int *num,
                 uint64* chunk_size, data_chunk ** dc)
{
    if (!size || !dc || !num) {
        return false;
    }

    if (*num <= 0) {
        *num = 1;
    }

    uint64 cs = size / *num;

    if (cs <= MIN_CHUNK_SIZE) {
        cs = MIN_CHUNK_SIZE;
    } else {
        cs = ((uint64) 1 << ((int) log2(cs / MIN_CHUNK_SIZE) + 1)) *
             MIN_CHUNK_SIZE;
    }

    *chunk_size = cs;
    uint32 total_size = *num * sizeof(data_chunk);

    *dc = (data_chunk *) malloc(total_size);
    memset(*dc, 0, total_size);
    data_chunk *dp = *dc;

    for (int i = 0; i < *num; ++i) {
        dp = *dc + i;
        dp->start_pos = i * cs;
        dp->cur_pos = i * cs;
        dp->end_pos = dp->cur_pos + cs;
        if (dp->end_pos >= size) {
            dp->end_pos = size;
            *num = i + 1;
            break;
        }
    }
    PDEBUG("results: chunk_size: %.02fM, nc: %d\n", (float) cs / M, *num);

    return true;
}

metadata* metadata_create_empty()
{
    return NULL;
}

void metadata_inspect(const char*path)
{
    metadata* md = NULL;
    fh_map*   fm = NULL;
    if (!metadata_create_from_file(path, &md, &fm))
    {
        printf ("Failed to create metadata from file: %s\n", path);
        return;
    }

    metadata_display(md);
}

/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * fill-column: 78
 * End:
 *
 * vim: set noet ts=4 sw=4:
 */
