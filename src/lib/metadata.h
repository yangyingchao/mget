/** metadta_private.h --- Internally used by libmget.
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

#ifndef _METADTA_PRIVATE_H_
#define _METADTA_PRIVATE_H_

#include "mget_metadata.h"
#include "data_utlis.h"
#include "fileutils.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PA(X, N)       ((X % N) ? (N * ((X/N) + 1)):X)
#define MH_SIZE()      48
#define MD_SIZE(X)     (MH_SIZE()+sizeof(void*)*4+sizeof(data_chunk)*(X->hd.nr_user)+PA(X->hd.eb_length,4))
#define CHUNK_NUM(X)       (X->hd.nr_effective)
#define CHUNK_SIZE(X)      (sizeof(data_chunk)*(X->hd.nr_user))
#define GET_URL(X)         (((char*)X->raw_data)+CHUNK_SIZE(X))
#define GET_FN(X)         (GET_URL(X)+strlen(GET_URL(X))+1)

typedef struct _metadata_wrapper {
    metadata *md;
    fh_map *fm;
    bool from_file;
} metadata_wrapper;

bool chunk_split(uint64 start, uint64 size, int *num, data_chunk ** dc);
bool metadata_create_from_file(const char *fn, metadata** md, fh_map** fm_md);
bool metadata_create_from_url(const char *url,
                              const char *fn,
                              uint64      size,
                              int         nc,
                              mget_slis*  lst,
                              metadata** md);
void metadata_destroy(metadata_wrapper * mw);
void metadata_display(metadata * md);

metadata* metadata_create_empty();

#ifdef __cplusplus
}
#endif

#endif /* _METADTA_PRIVATE_H_ */
