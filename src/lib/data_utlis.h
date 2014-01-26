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
#include "macros.h"
#include "mget_utils.h"

typedef void (*free_func) (void *);

typedef struct _mget_slis {
    void *data;
    free_func f;
    struct _mget_slis *next;
} mget_slis;

mget_slis *mget_slist_append(mget_slis * l, void *data, free_func f);
void mget_slist_free(mget_slis * lst);

typedef struct _mget_slist_head {
    struct mget_slist_head *next;
} mget_slist_head;

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
                ptr->next = (mget_slist_head*)ZALLOC1(type);    \
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
} TableEntry;

typedef struct _hash_table {
    int capacity;
    TableEntry *entries;

    void *bufer;		// Serailized buffer.
    HashFunction hashFunctor;
    DestroyFunction deFunctor;
} hash_table;


// Functions.
void hash_table_destroy(hash_table * table);
hash_table *hash_table_create(uint32 size, DestroyFunction dFunctor);
bool hash_table_insert(hash_table * table, char *key, void *val);
void *hash_table_entry_get(hash_table * table, const char *key);

void dump_hash_table(hash_table * ht, void *buffer);

char *rstrip(char *str);

#define GET_HASH_ENTRY(T, H, K)       ((T*)hash_table_entry_get(H, K))

#ifdef __cplusplus
}
#endif

#endif				/* _DATA_UTLIS_H_ */
