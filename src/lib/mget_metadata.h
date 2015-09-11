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
    uint64 start_pos;
    uint64 end_pos;
    uint64 cur_pos;
} __attribute__((aligned (32))) data_chunk;

#define is_chunk_finished(p)       (p->cur_pos >= p->end_pos)

typedef enum _request_status {
    RS_INIT = 0,
    RS_STARTED,
    RS_PAUSED,
    RS_FINISHED,
    RS_DROP, // Error that can't be recovered, should drop any temporary data.
    RS_FAILED,
} request_status;

typedef struct metadata_head {
    uint32 iden;          // \0xFC"TMD"                                     /*  0  4 */
    uint32 version;       // Major, Minor, Patch, NULL                      /*  4  4 */
    uint64 package_size;  // size of package;                               /*  8  8 */
    uint64 chunk_size;    // size of single chunk                           /* 16  8 */
    uint64 finished_size; // current finished size                          /* 24  8 */
    uint32 recent_ts;     // last time started.                             /* 32  4 */
    uint32 acc_time;      // accumulated time in this downloading.          /* 36  4 */
    uint8 status;         // status.                                        /* 40  1 */
    uint8 nr_user;        // number of chunks set by user.                  /* 41  1 */
    uint8 nr_effective;   // number of chunks that are effective.           /* 42  1 */
    uint8 acon;           // active connections.                            /* 43  1 */
    uint16 ebl;           // length of extra body: url_len+mime_len+others  /* 44  2 */
    uint8 update_name;    // flag to indicate file name should be updated.  /* 46  1 */
    uint8 reserved[17];   // reserved ...                                   /* 47 17 */
	/* --- cacheline 1 boundary (64 bytes) --- */
	/* size: 64, cachelines: 1, members: 14 */
} mh;                     // up to 64 bytes

typedef struct _hash_table hash_table;

// Byte/     0       |       1       |       2       |       3       |
// ***/              |               |               |               |
// **|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|
// **+-------------------------------+-------------------------------+
// **|                       DATA_CHUNK  START                       |
// **+---------------------------------------------------------------+
// **|                       ..................                      |
// **+---------------------------------------------------------------+
// **|                       DATA_CHUNK  END                         |
// **+---------------------------------------------------------------+
// **|                       SH_TABLE_BUFFER                         |
// **+---------------------------------------------------------------+

typedef struct _metadata_ptrs {
    data_chunk *body;                   // pointer to data_chunk
    char       *ht_buffer;              // points to buffer of serialized hash_tables.
    hash_table *ht;                     // points to hash table.
    char       *url;                    // pointer to url
    char       *fn;                     // saved file name.
    char       *user;
    char       *passwd;
    char       *mime;                   // pointer to mime type
} mp;


typedef struct _metadata {
    mh hd;          // header, up to 64 bytes.
    mp *ptrs;       // All pointers that can be calculated.
    char raw_data[0];   // start to body of raw_data.
} metadata;

#ifdef __cplusplus
}
#endif
#endif              /* _METADATA_H_ */
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
