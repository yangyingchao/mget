#ifndef _FILEUTILS_H_
#define _FILEUTILS_H_

#include "typedefs.h"

typedef struct _fhandle
{
    char*  fn;                          /*!< File name*/
    int    fd;                          /*!< File descriptor. */
    size_t size;                        /*!< Size of this file. */
    bool   auto_remove;
} fhandle;

typedef enum _FHM
{
    FHM_DEFAULT = 0,
    FHM_CREATE  = 1
} FHM;

fhandle* fhandle_create(const char* fn, FHM mode);
void     fhandle_destroy(fhandle** fh);

typedef struct _fh_map
{
    fhandle* fh;
    void*    addr;
    size_t   length;
} fh_map;

fh_map* fhandle_mmap(fhandle* fh, off_t offset, size_t length);
void fhandle_munmap(fh_map** fm);
void fhandle_msync(fh_map* fm);
void fhandle_munmap_close(fh_map** fm);
char* get_basename(const char* fname);
bool file_existp(const char* fn);
void remove_file(const char* fn);
#endif /* _FILEUTILS_H_ */
