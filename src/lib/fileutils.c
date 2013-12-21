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
#include "debug.h"
#include <libgen.h>
#include "data_utlis.h"
#define FM_RWUSR       (S_IRUSR|S_IWUSR)


fhandle* fhandle_create(const char* fn, FHM mode)
{
    fhandle* fh = (fhandle*)malloc(sizeof(fhandle));
    if (!fn || !fh)
    {
        return NULL;
    }

    memset(fh, 0, sizeof(fh));
    fh->fn = strdup(fn);
    fh->fd = open(fn, O_RDWR | ((mode & FHM_CREATE) ? O_CREAT : 0), FM_RWUSR);
    struct stat st;
    if (!fstat(fh->fd, &st))
    {
        fh->size = st.st_size;
        return fh;
    }

err:
    PDEBUG ("err..\n");

    if (fh)
    {
        fhandle_destroy(&fh);
    }

    return NULL;
}

void fhandle_destroy(fhandle** fh)
{
    if (fh && *fh)
    {
        if ((*fh)->fd != -1)
        {
            close((*fh)->fd);
        }

        if ((*fh)->fn)
        {
            if ((*fh)->auto_remove)
            {
                PDEBUG ("Autoremove: %s\n", (*fh)->fn);

                remove_file((*fh)->fn);
            }
            free((*fh)->fn);
        }
        free(*fh);
        *fh = NULL;
    }
}


fh_map* fhandle_mmap(fhandle* fh, off_t offset, size_t length)
{
    if (!fh || fh->fd == -1 || !length)
    {
        return NULL;
    }

    size_t size = offset + length;
    if ((size > fh->size) && ftruncate(fh->fd, size))
    {
        return NULL;
    }

    fh_map* fm = NULL;
    off_t pa_offset = offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
    size = length + offset - pa_offset;
    void* addr = mmap(NULL, size, PROT_WRITE,
                      MAP_SHARED, fh->fd, pa_offset);

    if (addr)
    {
        fm = (fh_map*)malloc(sizeof(fh_map));
        if (fm)
        {
            fm->fh     = fh;
            fm->addr   = addr;
            fm->length = size;
        }
    }

    PDEBUG ("file %s mapped to addr: %p, length: %lX(%s)\n",
            fh->fn, fm->addr, size, stringify_size(size));

    return fm;
}

void fhandle_munmap(fh_map** fm)
{
    if (fm && *fm)
    {
        if ((*fm)->addr)
        {
            munmap((*fm)->addr, (*fm)->length);
        }
        free(*fm);
        *fm = NULL;
    }
}

void fhandle_munmap_close(fh_map** fm)
{
    if (fm && *fm)
    {
        if ((*fm)->addr)
        {
            munmap((*fm)->addr, (*fm)->length);
        }

        fhandle_destroy(&(*fm)->fh);
        free(*fm);
        *fm = NULL;
    }
}


char* get_basename(const char* fname)
{
    if (!fname)
        return NULL;
    char* tmp = strdup(fname);
    char* bname = strdup(basename(tmp));
    free(tmp);
    return bname;
}

bool file_existp(const char* path)
{
    if (!path)
        return false;

    int ret;
    ret = access(path, F_OK);
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

void remove_file(const char* fn)
{
    if (fn)
    {
        unlink(fn);
    }
}

void fhandle_msync(fh_map* fm)
{
    msync(fm->addr, fm->length, MS_SYNC);
}
