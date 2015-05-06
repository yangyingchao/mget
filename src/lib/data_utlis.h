/** data_utlis.h --- generic data structures
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

#ifndef _DATA_UTLIS_H_
#define _DATA_UTLIS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "mget_types.h"
#include "mget_macros.h"
#include "mget_utils.h"
#include "mget_metadata.h"

typedef void (*free_func) (void *);

typedef struct _slist_head {
	struct _slist_head *next;
} slist_head;

#define SLIST_FOREACH(var, head)                        \
    for ((var) = (head); (var);  (var) = (var)->next)   \

// Hash Tables.
typedef void (*DestroyFunction) (void *data);
typedef uint32(*HashFunction) (const char *key);

typedef struct _TableEntry TableEntry;

struct _hash_table {
	uint32 capacity;
	uint32 occupied;
	TableEntry *entries;

	HashFunction hashFunctor;
	DestroyFunction deFunctor;
};


// Functions.
void hash_table_destroy(hash_table * table);
hash_table *hash_table_create(uint32 size, DestroyFunction dFunctor);
bool hash_table_insert(hash_table * table, const char *key, void *val,
                       uint32 val_len);
bool hash_table_update(hash_table * table, char *key, void *val,
                       uint32 val_len);
void *hash_table_entry_get(hash_table * table, const char *key);

#define HASH_ENTRY_GET(T, H, K) (T*)hash_table_entry_get((H),(K))
#define HASH_TABLE_INSERT(T, K, V, L) \
    hash_table_insert((T), (K), (V), (uint32)(L))

/**
 * @name dump_hash_table - Dump hash table to buffer.
 * @param ht -  ht to be dumpped.
 * @param buffer - buffer
 * @param buffer_size - buffer size 
 * @return uint32: size used, or -1 if not enough space.
 */
size_t dump_hash_table(hash_table * ht, void *buffer,
                       size_t buffer_size);

/**
 * @name hash_table_create_from_buffer - Creates a hash table from buffer.
 * @param buffer - buffer where serialized content is stored.
 * @param buffer_size - Number of buffer size
 * @return hash_table*
 */
hash_table *hash_table_create_from_buffer(void *buffer,
                                          uint32 buffer_size);

char *rstrip(char *str);

#define GET_HASH_ENTRY(T, H, K)       ((T*)hash_table_entry_get(H, K))

typedef struct _byte_queue {
	char* p;		// start of buffer
	char* r;		// read ptr
	char* w;		// write ptr
	char* x;		// end of buffer
} byte_queue;

byte_queue *bq_init(size_t size);

// it will ensure there are at least sz bytes left for writting...
byte_queue *bq_enlarge(byte_queue*, size_t);
void bq_destroy(byte_queue*);
void bq_reset(byte_queue*);

void lowwer_case(char *p, size_t len);

#ifdef __cplusplus
}
#endif
#endif				/* _DATA_UTLIS_H_ */
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
