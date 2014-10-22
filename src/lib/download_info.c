/** dinfo.c --- Implementation of download info.
 *
 * Copyright (C) 2014 Yang,Ying-chao
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

#include "logutils.h"
#include "download_info.h"
#include "mget_macros.h"
#include "metadata.h"
#include "mget_config.h"
#include "mget_utils.h"

// This magic number is calculated by: (year+month+day)%256, where
// year/month/day is birthday of my son, just for fun!
#define MAGIC_NUMBER     0xFC

void dinfo_destroy(dinfo** info)
{
    if (!info || !(*info))
        return;

    bool remove_package  = (*info)->md->hd.package_size == 0;
    bool remove_metadata = remove_package || \
                           (*info)->md->hd.status == RS_FINISHED;

    if (remove_metadata)
        remove_file((*info)->fm_md->fh->fn);

    if (remove_package && *info && (*info)->fm_file)
        remove_file((*info)->fm_file->fh->fn);

    url_info_destroy(&(*info)->ui);
    fhandle_munmap_close(&(*info)->fm_md);
    fhandle_munmap_close(&(*info)->fm_file);
    FIFZ(info);
}

bool dinfo_create(const char *url, const file_name * fn,
                  mget_option* opt, dinfo** info)
{
    url_info* ui           = NULL;
    char*     fpath        = NULL;
    dinfo*    dInfo        = NULL;
    bool      ret          = false;
    bool      update_fn    = false;
    bool      md_from_file = false;

    dInfo = ZALLOC1(dinfo);
    if (!dInfo)  {
        goto out;
    }

    if (url && !parse_url(url, &ui)) {
        fprintf(stderr,
                "Failed to parse given url: %s, try loading local metadata\n",
                url);
    }

    if (!get_full_path(fn, &fpath)) {
        if (!ui || !ui->bname) {
            fprintf(stderr, "Failed parse url(%s) and output directory\n", url);
            goto free;
        }

        char *tmp = ZALLOC(char, strlen(fpath) + strlen(ui->bname) + 2);
        sprintf(tmp, "%s/%s", fpath, ui->bname);
        PDEBUG ("tmp: %s -- %s\n", fpath, ui->bname);

        FIF(fpath);
        fpath = tmp;
        update_fn = true;
    }

    if (!fpath) {
        fprintf(stderr, "Failed to guess local file name!\n");
        goto free;
    }

    char* tfn = ZALLOC(char, strlen(fpath) + 5);
    sprintf(tfn, "%s.tmd", fpath);

    if (file_existp(tfn) &&
        metadata_create_from_file(tfn, &dInfo->md, &dInfo->fm_md)) {
        PDEBUG("DInfo created from file: %s\n", tfn);

        // Destroy url info and recreate using url stored in mw.
        url_info_destroy(&ui);
        if (!parse_url(dInfo->md->ptrs->url, &ui)) {
            fprintf(stderr, "Failed to parse stored url: %s.\n", dInfo->md->ptrs->url);
            if (url) {
                fprintf(stderr, "Removing old metadata and retring...\n");
                url_info_destroy(&ui);

                if (url && !parse_url(url, &ui)) {
                    fprintf(stderr, "Failed to parse given url: %s\n", url);
                    goto free;
                }
            }
            else  {
                fprintf(stderr, "Abort: no url provided...\n");
                return false;
            }
        }

        // update fpath
        char* dirn  = fm_get_directory(dInfo->fm_md);
        FIF(fpath);
        if (dirn) {
            fpath = ZALLOC(char, strlen(dirn) + strlen(dInfo->md->ptrs->fn) + 2);
            sprintf(fpath, "%s/%s", dirn, dInfo->md->ptrs->fn);
        }
        else
        {
            fpath = strdup(dInfo->md->ptrs->fn);
        }

        md_from_file = true;
    }
    else {
        if (!url) {
            fprintf(stderr, "Failed to get TMD file and url is empty!\n");
            goto free;
        }

        // TODO: remove this magic number...
        //       space reserved by this magic number should be filled by extra
        //       information: auth, proxy, ....

        PDEBUG ("creating empty metadata using: %s -- %s --%s --%s\n",
                url, fpath, opt->user, opt->passwd);

        uint16 ebl = 1024;
        size_t md_size = CALC_MD_SIZE(opt->max_connections, ebl);

        dInfo->fm_md = fm_create(tfn, md_size);
        dInfo->md    = (metadata*)dInfo->fm_md->addr;
        if (!dInfo->md)
        {
            fprintf(stderr, "Failed to create metadata!\n");
            return false;
        }

        // fill this pmd.
        metadata* pmd = dInfo->md;
        memset(pmd, 0, md_size);
        mh *hd = &pmd->hd;
        hash_table* ht = hash_table_create(128, free);
        mp* ptrs = ZALLOC1(mp);

        unsigned char* ptr = (unsigned char*)&hd->iden;
        *ptr = MAGIC_NUMBER;
        ptr++;
        sprintf(ptr, "TMD");

        hd->version      = GET_VERSION();
        hd->package_size = 0;
        hd->last_time    = get_time_s();
        hd->acc_time     = 0;
        hd->status       = RS_INIT;
        hd->nr_user      = opt->max_connections;
        hd->nr_effective = 0;
        hd->ebl          = ebl;
        hd->update_name  = update_fn;
        hd->acon         = opt->max_connections;

        pmd->ptrs       = ptrs;
        ptrs->body      = (data_chunk*)pmd->raw_data;
        ptrs->ht        = ht;
        ptrs->ht_buffer = (char*)(pmd->raw_data) +
                          sizeof(data_chunk) * pmd->hd.nr_user;

        if (url) {
            ptrs->url = strdup(url);
            hash_table_insert(ptrs->ht, K_URL, ptrs->url,
                              strlen(url));
        }

        if (fpath) {
            char *tmp = get_basename(fpath);
            ptrs->fn = strdup(tmp);
            free(tmp);
            hash_table_insert(ptrs->ht, K_FN, ptrs->fn,
                              strlen(ptrs->fn));
       }

        if (opt->user)
        {
            pmd->ptrs->user = strdup(opt->user);
            hash_table_insert(ptrs->ht, K_USR, ptrs->user,
                              strlen(ptrs->fn));
        }

        if (opt->passwd)
        {
            pmd->ptrs->passwd = strdup(opt->passwd);
            hash_table_insert(ptrs->ht, K_PASSWD, ptrs->passwd,
                              strlen(ptrs->fn));
        }
    }

    // create fm for downloaded file.
    dInfo->ui = ui;
    if (md_from_file) {
        dInfo->fm_file = fm_create(fpath, dInfo->md->hd.package_size);
        PDEBUG ("dInfo: %p, md: %p, fm_md: %p, fm_file: %p\n",
                dInfo, dInfo->md,  dInfo->fm_md, dInfo->fm_file);
        if (!dInfo->fm_file)
            goto free;
    }

    if (dInfo && dInfo->md && dInfo->fm_md)  {
        *info = dInfo;
        ret   = true;
        goto out;
    }

free:
    FIF(dInfo);
    FIF(fpath);

out:
    return ret;
}


bool dinfo_ready(dinfo* info)
{
    return (info && info->ui && info->md && info->fm_md && info->fm_file && \
            info->md->hd.package_size && info->md->hd.status);
}

extern bool chunk_split(uint64 start, uint64 size, int *num,
                        uint64* cs, data_chunk ** dc);

bool dinfo_update_metadata(dinfo* info, uint64 size, const char* fn)
{
    PDEBUG ("enter\n");

    if (!info || !info->md)
        return false;

    PDEBUG ("enter 2\n");

    metadata*   md = info->md;
    mh*         hd = &md->hd;
    data_chunk* dc = NULL;
    int nc         = hd->nr_user;
    uint64 cs = 0;
    if (!md || !chunk_split(0, size, &nc, &cs, &dc) || !dc) {
        PDEBUG("return err.\n");
        return false;
    }

    hd->package_size = size;
    hd->chunk_size    = cs;
    hd->nr_effective = nc;
    hd->acon         = nc;

    data_chunk* np = md->ptrs->body;
    for (int i = 0; i < nc; ++i, ++np) {
        data_chunk *p = dc + i;
        *np = *p;
    }

    hash_table* ht = md->ptrs->ht;
    assert(ht != NULL);

#define DINFO_UPDATE_HASH(K, V)                 \
    PDEBUG("showing: %s - %s: \n", (K), ((V)));              \
    if (V)                                                 \
        hash_table_update(ht, (K), (V), strlen(V))

    DINFO_UPDATE_HASH(K_URL, md->ptrs->url);
    DINFO_UPDATE_HASH(K_USR, md->ptrs->user);
    DINFO_UPDATE_HASH(K_PASSWD, md->ptrs->passwd);

    if (fn && hd->update_name)
    {
        md->ptrs->fn = strdup(fn);
        DINFO_UPDATE_HASH(K_FN, md->ptrs->fn);
        hd->update_name = FALSE;
    }

#undef DINFO_UPDATE_HASH

    // reset ht_buffer...
    char* ptr = (char*)(md->raw_data) +
                sizeof(data_chunk) * hd->nr_effective;
    uint32 n_ebl = md->ptrs->ht_buffer + hd->ebl - ptr;

    md->ptrs->ht_buffer = ptr;
    hd->ebl = dump_hash_table(ht, md->ptrs->ht_buffer, n_ebl);
    fm_remap(&info->fm_md, CALC_MD_SIZE(hd->nr_effective, hd->ebl));
    PDEBUG ("chunk: %p -- %p, ht_buffer: %p\n",
            md->raw_data, md->ptrs->body, md->ptrs->ht_buffer);

    // now update fm_file.
    if (!info->fm_file) {
        char* fpath = NULL;
        char* dirn  = fm_get_directory(info->fm_md);
        if (dirn) {
            fpath = ZALLOC(char, strlen(dirn) + strlen(md->ptrs->fn) + 2);
            sprintf(fpath, "%s/%s", dirn, md->ptrs->fn);
        }
        else
        {
            fpath = strdup(md->ptrs->fn);
        }

        PDEBUG ("Creating file mapping: %s\n", fpath);
        info->fm_file = fm_create(fpath, info->md->hd.package_size);
        FIF(fpath);
    }
    else
        fm_remap(&info->fm_file, size);

    return true;
}


void dinfo_sync(dinfo* info)
{
    if (info)
    {
        fhandle_msync(info->fm_md);
        fhandle_msync(info->fm_file);
    }
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
