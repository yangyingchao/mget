/** fileutils.c --- implementation of file utility.
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

#include "logutils.h"
#include "fileutils.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <libgen.h>
#include "data_utlis.h"
#define FM_DEFAULT       (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

fhandle *fhandle_create(const char *fn, FHM mode)
{
    fhandle *fh = ZALLOC1(fhandle);

    if (!fn || !fh) {
        goto err;
    }

    fh->fn = strdup(fn);
    fh->fd =
        open(fn, O_RDWR | ((mode & FHM_CREATE) ? O_CREAT : 0), FM_DEFAULT);
    struct stat st;

    if (!fstat(fh->fd, &st)) {
        fh->size = st.st_size;
        return fh;
    }

  err:
    PDEBUG("err..\n");

    if (fh) {
        fhandle_destroy(&fh);
    }

    return NULL;
}

void fhandle_destroy(fhandle ** fh)
{
    if (fh && *fh) {
        if ((*fh)->fd != -1) {
            close((*fh)->fd);
        }

        PDEBUG("fh: %p, fn: %s\n", *fh, (*fh)->fn);

        if ((*fh)->fn) {
            if (0) {
                remove_file((*fh)->fn);
            }
            free((*fh)->fn);
        }
        free(*fh);
        *fh = NULL;
    }
}


fh_map *fhandle_mmap(fhandle * fh, off_t offset, size_t length)
{
    if (!fh || fh->fd == -1) {
        return NULL;
    }

    size_t size = offset + length;

    if ((size > fh->size) && ftruncate(fh->fd, size)) {
        return NULL;
    }

    fh_map *fm = NULL;
    off_t pa_offset = offset & ~(sysconf(_SC_PAGE_SIZE) - 1);

    size = length + offset - pa_offset;
    void *addr = mmap(NULL, size, PROT_WRITE,
                      MAP_SHARED, fh->fd, pa_offset);

    if (addr) {
        fm = (fh_map *) malloc(sizeof(fh_map));
        if (fm) {
            fm->fh = fh;
            fm->addr = addr;
            fm->length = size;
        }
    }

    if (fm) {
        PDEBUG("file %s mapped to addr: %p, length: %lX(%s)\n",
               fh->fn, fm->addr, size, stringify_size(size));
    } else {
        PDEBUG("FM is null!\n");
    }

    return fm;
}

void fhandle_munmap(fh_map ** fm)
{
    if (fm && *fm) {
        if ((*fm)->addr) {
            munmap((*fm)->addr, (*fm)->length);
        }
        free(*fm);
        *fm = NULL;
    }
}

void fhandle_munmap_close(fh_map ** fm)
{
    if (fm && *fm) {
        if ((*fm)->addr) {
            munmap((*fm)->addr, (*fm)->length);
        }

        fhandle_destroy(&(*fm)->fh);
        free(*fm);
        *fm = NULL;
    }
}


char *get_basename(const char *fname)
{
    if (!fname)
        return NULL;
    char *tmp = strdup(fname);
    char *bname = strdup(basename(tmp));

    free(tmp);
    return bname;
}

bool file_existp(const char *path)
{
    PDEBUG("path: %s\n", path);

    if (!path)
        return false;

    int ret = access(path, F_OK);
    PDEBUG("ret: %d\n", ret);

    if (ret < 0)
        return false;
    struct stat sb;

    if ((ret = stat(path, &sb)) == 0) {
        if (S_ISREG(sb.st_mode))
            return true;
    }
    return false;
}


int dir_exist(const char *path)
{
    if (!path)
        return -1;

    int ret;

    ret = access(path, F_OK);
    if (ret < 0)
        return -1;
    struct stat sb;

    if ((ret = stat(path, &sb)) == 0) {
        if (S_ISDIR(sb.st_mode))
            return 0;
    }
    return -1;
}

void remove_file(const char *fn)
{
    if (fn) {
        unlink(fn);
    }
}

void fhandle_msync(fh_map * fm)
{
    msync(fm->addr, fm->length, MS_SYNC);
}

#define STR_LEN(X) (X ? strlen(X) : 0)

bool get_full_path(const file_name * fn, char **final)
{
    bool ret = false;

    if (!final)
        goto ret;

    if (!fn || (!fn->dirn && !fn->basen)) {
        *final = strdup(".");
        goto ret;
    }

    *final = ZALLOC(char, STR_LEN(fn->dirn) + STR_LEN(fn->basen) + 1);

    if (fn->dirn)
        sprintf(*final, "%s/", fn->dirn);
    if (fn->basen) {
        strcat(*final, fn->basen);
        ret = true;
    }

    PDEBUG("final: %s\n", *final);

  ret:
    return ret;
}

fh_map *fm_create(const char *fn, size_t length)
{
    fhandle *fh = fhandle_create(fn, FHM_CREATE);
    fh_map *fm = fh ? fhandle_mmap(fh, 0, length ? length : 1) : NULL;
    if (fm)
        return fm;

    fhandle_destroy(&fh);
    return NULL;
}

bool fm_remap(fh_map ** fm, size_t nl)
{
    if (!fm)
        return false;
    fhandle *fh = (*fm)->fh;
    fhandle_munmap(fm);
    *fm = fhandle_mmap(fh, 0, nl ? nl : 1);
    return true;
}

char *fm_get_directory(fh_map * fm)
{
    char *dirn = NULL;

    if (fm && fm->fh && fm->fh->fn) {
        char *tmp = strdup(fm->fh->fn);
        dirn = strdup(dirname(tmp));
        free(tmp);
    }

    return dirn;
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
