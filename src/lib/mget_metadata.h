/** metadata.h --- metadata of file, recording data of data.
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

#ifndef _METADATA_H_
#define _METADATA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "mget_types.h"

typedef struct _data_chunk {
    char *base_addr;
    uint64 start_pos;
    uint64 end_pos;
    uint64 cur_pos;
} data_chunk;

#define is_chunk_finished(p)       (p->cur_pos >= p->end_pos)

typedef enum _request_status {
    RS_INIT = 0,
    RS_STARTED,
    RS_PAUSED,
    RS_FINISHED,
    RS_FAILED,
} request_status;

typedef struct metadata_head {
    uint32 iden;        // TMD0                                          -- 04
    uint32 version;     // Major, Minor, Patch, NULL                     -- 08
    uint64 package_size;    // size of package;                          -- 16
    uint64 last_time;       // last time used.                           -- 24
    uint32 acc_time;        // accumulated time in this downloading.     -- 28

    uint8 status;       // status.                                       -- 29
    uint8 nr_user;      // number of chunks set by user.                 -- 30
    uint8 nr_effective; // number of chunks that are effective.          -- 31
    uint8 acon;         // active connections.                           -- 32

    uint16 eb_length;   // length of extra body: url_len+mime_len+others -- 34

    uint8 reserved[14]; // reserved ...                                  -- 48
} mh;				// up to 48 bytes

// Byte/     0       |       1       |       2       |       3       |
// ***/              |               |               |               |
// **|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|
// **+-------------------------------+-------------------------------+
// **|               DATA_CHUNK  START                               |
// **+---------------------------------------------------------------+
// **|               ..................           .                  |
// **+---------------------------------------------------------------+
// **|               DATA_CHUNK  END                                 |
// **+-------------------------------+-------------------------------+
// **|            URL  FN  MIME   OTHERS   .....                     |
// **+-------------------------------+-------------------------------+

typedef struct _metadata {
    mh hd;			// header, up to 48 bytes.
    data_chunk *body;		// pointer to data_chunk
    char *url;			// pointer to url
    char *fn;			// saved file name.
    char *mime;			// pointer to mime type
    char raw_data[0];		// start to body of raw_data.
} metadata;

#ifdef __cplusplus
}
#endif
#endif				/* _METADATA_H_ */

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
