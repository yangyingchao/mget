/** fileutils.h --- utility of file operations.
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

#ifndef _FILEUTILS_H_
#define _FILEUTILS_H_

#include "typedefs.h"

typedef struct _file_name
{
    char* dirn;
    char* basen;
} file_name;

/** returns true if final directory is file, or false if final path is directory*/
bool get_full_path(const file_name*, char**);

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
