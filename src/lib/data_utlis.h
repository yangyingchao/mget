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

#include "mget_types.h"
#include "mget_macros.h"
#include "mget_utils.h"
#include "mget_metadata.h"

typedef void (*free_func) (void *);

typedef struct _mget_slis {
    void *data;
    free_func f;
    struct _mget_slis *next;
} mget_slis;

mget_slis *mget_slist_append(mget_slis * l, void *data, free_func f);
void mget_slist_free(mget_slis * lst);

typedef struct _slist_head {
    struct _slist_head *next;
} slist_head;

#define INIT_LIST(instance, type) do {                                  \
        if (instance == NULL) {                                         \
            instance = (type *)malloc(sizeof(type));                    \
            if (instance == NULL) {                                     \
                fprintf(stderr, "ERROR: failed to alloc memory.\n");    \
                return NULL;                                              \
            }                                                           \
            memset(instance, 0, sizeof(type));                          \
        }                                                               \
    } while (0);

// Seek to list tail and create new empty.
#define SEEK_LIST_TAIL(lst, ptr, type)                      \
    do                                                      \
    {                                                       \
        ptr = lst;                                          \
        while (ptr)                                         \
        {                                                   \
            if (ptr->next == NULL)                          \
            {                                               \
                ptr->next = (slist_head*)ZALLOC1(type);    \
                ptr = (type*)ptr->next;                         \
                break;                                      \
            }                                               \
            else                                            \
            {                                               \
                ptr = (type*)ptr->next;                     \
            }                                               \
        }                                                   \
    } while (0)                                             \


typedef struct _kv_pair {
    char *k;
    char *v;
} kvp;


// Hash Tables.

typedef void (*DestroyFunction) (void *data);
typedef uint32(*HashFunction) (const char *key);

typedef struct _TableEntry {
    char *key;
    void *val;
    uint32 val_len; // length of value.
} TableEntry;

struct _hash_table {
    int capacity;
    int occupied;
    TableEntry *entries;

    HashFunction hashFunctor;
    DestroyFunction deFunctor;
};


// Functions.
void hash_table_destroy(hash_table* table);
hash_table*hash_table_create(uint32 size, DestroyFunction dFunctor);
bool hash_table_insert(hash_table* table, char *key, void *val, uint32 val_len);
void *hash_table_entry_get(hash_table* table, const char *key);

#define HASH_ENTRY_GET(T, H, K) (T*)hash_table_entry_get((H),(K))

/**
 * @name dump_hash_table - Dump hash table to buffer.
 * @param ht -  ht to be dumpped.
 * @param buffer - buffer
 * @param buffer_size - buffer size 
 * @return uint32: size used, or -1 if not enough space.
 */
uint32 dump_hash_table(hash_table* ht, void *buffer, uint32 buffer_size);

/**
 * @name hash_table_create_from_buffer - Creates a hash table from buffer.
 * @param buffer - buffer where serialized content is stored.
 * @param buffer_size - Number of buffer size
 * @return hash_table*
 */
hash_table* hash_table_create_from_buffer(void* buffer, uint32 buffer_size);

/**
 * @name calculate_hash_table_size - calculate buffer size to hold hash table
 * @param ht -  hash table.
 * @return uint32
 */
uint32 calculate_hash_table_buffer(hash_table* ht);

char *rstrip(char *str);

#define GET_HASH_ENTRY(T, H, K)       ((T*)hash_table_entry_get(H, K))


typedef struct _byte_queue
{
    byte* p; // start of buffer
    byte* r; // read ptr
    byte* w; // write ptr
    byte* x; // end of buffer
} byte_queue;

byte_queue* bq_init(size_t size);

// it will ensure there are at least sz bytes left for writting...
byte_queue* bq_enlarge(byte_queue* bq, size_t sz);
void bq_destroy(byte_queue**);

void lowwer_case(char* p, size_t len);
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
