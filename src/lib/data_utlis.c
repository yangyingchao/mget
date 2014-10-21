/** data_utlis.c --- implementation of data structures.
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

#include "log.h"
#include "mget_config.h"
#include "data_utlis.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

mget_slis *mget_slis_append(mget_slis * l, void *data, free_func f)
{
    return NULL;
}

// Hash table operations.

struct _TableEntry {
    char   *key;
    void   *val;
    time_t  ts;                         /* time stamp of last access time.*/
    uint32  val_len;                    /* length of value. */
};

static const int HASH_SIZE = 256;

// debug int ptr.
#define DIP(X, Y)\
    PDEBUG (#X": (%p): %X\n", (Y), *((int*)(Y)));


uint32 StringHashFunction(const char *str)
{
    uint32 hash = 0;
    uint32 i = 0;
    const char *key = str;

    for (; i < strlen(str); ++i) {
        hash += *(key + i);
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash % HASH_SIZE;
}

#define DEL_ENTRY(T, E)                 \
    do                                  \
    {                                   \
        FIF((E)->key);                  \
        if ((E)->val && (T)->deFunctor) \
            (T)->deFunctor(entry->val); \
    } while (0)

void hash_table_destroy(hash_table* table)
{
    if (table) {
        if (table->entries) {
            int i = 0;

            for (; i < table->capacity; ++i) {
                TableEntry *entry = &table->entries[i];

                if (entry->key) {
                    free(entry->key);
                }
                if (entry->val && table->deFunctor) {
                    table->deFunctor(entry->val);
                }
            }
            free(table->entries);
        }
        free(table);
    }
}

hash_table*hash_table_create(uint32 hashSize, DestroyFunction dFunctor)
{
    hash_table*table = malloc(sizeof(hash_table));

    if (table) {
        memset(table, 0, sizeof(hash_table));
        if (hashSize < HASH_SIZE) {
            hashSize = HASH_SIZE;
        }

        table->capacity = hashSize;
        table->entries = ZALLOC(TableEntry, hashSize);
        table->hashFunctor = StringHashFunction;	// use default one.
        table->deFunctor = dFunctor;

        if (table->entries) {
            memset(table->entries, 0, sizeof(TableEntry) * hashSize);
        } else {
            hash_table_destroy(table);
            table = NULL;
        }
    }

    PDEBUG ("return with table: %p\n", table);
    return table;
}

bool hash_table_insert(hash_table* table, char *key, void *val, uint32 len)
{
    bool ret = false;
    if (table && key && val) {
        uint32 i;
        char *d_key = strdup(key);

        // Insert entry into the first open slot starting from index.
        for (i = table->hashFunctor(d_key); i < table->capacity; ++i) {
            TableEntry *entry = &table->entries[i];
            if (entry->key)
            {
                if (!strcmp(key, entry->key))
                {
                    return false;
                }
            }
            else {
                entry->key     = d_key;
                entry->val     = val;
                entry->val_len = len;
                entry->ts      = time(NULL);
                ret            = true;
                table->occupied ++;
                break;
            }
        }
    }

    if (!ret)
    {
        PDEBUG ("Failed to insert: %s -- %s\n", key, (char*)val);
    }

    return ret;
}

#define DTB(X, Y)                                                         \
    PDEBUG (X ", table: %p, capacity: %d, occupied: %d\n",Y,Y->capacity, Y->occupied)

bool hash_table_update(hash_table* table, char *key, void *val, uint32 len)
{
    DTB("enter", table);

    bool ret = false;
    if (table && key && val) {
        uint32 i;
        char        *d_key  = strdup(key);
        TableEntry*  oldest = NULL;
        TableEntry  *entry  = NULL;
        // Insert entry into the first open slot starting from index.
        for (i = table->hashFunctor(d_key); i < table->capacity; ++i) {
            entry = &table->entries[i];
            if (entry->key) {
                if (!strcmp(key, entry->key)) {
                    goto fill_slot;
                }
                else {
                    if (!oldest)
                        oldest = entry;
                    else
                        oldest = oldest->ts < entry->ts ? oldest : entry;
                }
            }
            else {
                table->occupied ++;
          fill_slot:
                entry->key     = d_key;
                entry->val     = val;
                entry->val_len = len;
                entry->ts      = time(NULL);
                ret            = true;
                break;
            }
        }

        if (!ret && oldest) {
            //@todo: consider add a callback for this event.
            DEL_ENTRY(table, oldest);
            ret = true;
            entry = oldest;
            goto fill_slot;
        }

        if (!ret) {
            PDEBUG ("Failed to update hash table for key: %s\n", key);
            FIF(d_key);
        }
    }

    DTB("leave", table);
    return ret;
}

/*! Looks for the given data based on key.

  @return void*
*/
void *hash_table_entry_get(hash_table* table, const char *key)
{
    TableEntry *entry = NULL;
    uint32 index = table->hashFunctor(key);
    int i;

    for (i = index; i < table->capacity; ++i) {
        entry = &table->entries[i];
        if (entry->key == NULL) {
            return NULL;
        }
        if (strcmp(entry->key, key) == 0) {
            break;
        }
    }
    if (entry) {
        PDEBUG("Key: %s - %s, val: %p\n", key, entry->key, entry->val);
        return entry->val;
    }
    return NULL;
}

uint32 dump_hash_table(hash_table* ht, void *buffer, uint32 buffer_size)
{
    if (!buffer || buffer_size <= 3 * sizeof(int))
    {
        return -1;
    }

    PDEBUG ("Dumping to %p\n", buffer);

    char* ptr = buffer;

    // Version Number
    *(int*)ptr = GET_VERSION();
    DIP(version, ptr);

    ptr += sizeof(int);

    // Capacity.
    *(int*)ptr = ht->capacity;
    DIP(capacity, ptr);
    ptr += sizeof(int);

    *(int*)ptr = ht->occupied;
    DIP(occupied, ptr);
    ptr += sizeof(int);

    TableEntry *entry = NULL;
    for (int i = 0; i < ht->capacity; i++) {
        entry = &ht->entries[i];

        if (entry->key && entry->val)
        {
            int key_len = (int)strlen(entry->key);
            //todo: check buffer size.

            *(int*)ptr = key_len;
            ptr += sizeof(int);
            memcpy(ptr, entry->key, key_len);
            ptr += key_len;

            *(int*)ptr = entry->val_len;
            ptr += sizeof(int);
            memcpy(ptr, entry->val, entry->val_len);
            ptr += entry->val_len;
        }
    }

    return ptr - (char*)buffer;
}

hash_table* hash_table_create_from_buffer(void* buffer, uint32 buffer_size)
{
    if (!buffer || buffer_size <= 3 * sizeof(int))
    {
        return NULL;
    }

    // version checking.
    char*       ptr = buffer;
    hash_table* ht  = NULL;

    int v = *(int*)ptr;
    enum log_options lo = LOG_NONE;
    if (VER_TO_MAJOR(v) != VERSION_MAJOR)
        lo = LOG_ALWAYS;
    else if (VER_TO_MINOR(v) != VERSION_MINOR)
        lo = LOG_NONVERBOSE;
    else if (VER_TO_PATCH(v) != VERSION_PATCH)
        lo = LOG_DEBUG;

    if (lo != LOG_NONE) {
        logprintf(lo, "WARNING: version changed!!!\n"
                  " You're reading hash tables of old version!!"
                  " -- %u.%u.%u: %u.%u.%u\n", DIVIDE_VERSION(v),
                  VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    }

    DIP(version, ptr);
    ptr += sizeof(int);

    DIP(capacity, ptr);
    if (*((int*)ptr) == 0 || !(ht = hash_table_create(*((int*)ptr), NULL))) {
        PDEBUG ("ptr: (%p): %d, ht: %p\n", ptr, *((int*)ptr), ht);
        return NULL;
    }

    ptr += sizeof(int);

    DIP(occupied, ptr);
    int i = 0, total = *(int*)ptr;
    ptr += sizeof(int);
    while (i++ < total) {
        assert(ptr - (char*)buffer <= buffer_size);

        int key_len = *(int*)ptr;
        char* key = ZALLOC(char, key_len+1);
        ptr += sizeof(int);

        memcpy(key, ptr, key_len);
        ptr += key_len;

        int val_len = *(int*)ptr;
        void* val = ZALLOC(char, val_len+1);
        ptr += sizeof(int);

        memcpy(val, ptr, val_len);
        ptr += val_len;

        hash_table_insert(ht, key, val, val_len);
    }

ret:
    return ht;
}

uint32 calculate_hash_table_buffer(hash_table* ht)
{
    uint32 size = 3 * sizeof(int);
    TableEntry* entry = NULL;
    for (int i = 0; i < ht->capacity; i++) {
        entry = ht->entries+i;
        if (entry->key && entry->val)
        {
            size += 2 * sizeof(int) + strlen(entry->key) + entry->val_len;
        }
    }

    return size;
}

char *rstrip(char *str)
{
    if (str) {
        char *ptr = str + strlen(str);

        while ((*ptr == ' ' || *ptr == '\t' || *ptr == '\0') && ptr > str) {
            *ptr = '\0';
            ptr--;
        }
    }
    return str;
}


const char *stringify_size(uint64 sz)
{
    static char str_size[64] = { '\0' };
    memset(str_size, 0, 64);

    if (sz < M) {
        sprintf(str_size, "%.02fKB", (double) sz / K);
    } else if (sz < G) {
        sprintf(str_size, "%.02fMB", (double) sz / M);
    } else {
        sprintf(str_size, "%.02fGB", (double) sz / G);
    }

    return str_size;
}

byte_queue* bq_init(size_t size)
{
    byte_queue* bq = ZALLOC1(byte_queue);
    bq->r = bq->w = bq->p = ZALLOC(byte, size);
    bq->x = bq->p + size;
    return  bq;
}

//@todo: align to page??
byte_queue* bq_enlarge(byte_queue* bq, size_t sz)
{
    if (sz > bq->x - bq->w) {
        // simply add sz bytes at the end if necessary.
        size_t nsz = bq->x + sz - bq->p;
        char* ptr = ZALLOC(char, nsz);
        if (ptr) {
            memcpy(ptr, bq->p, bq->w - bq->p);
            bq->r = (byte*)ptr + (bq->r - bq->p);
            bq->w = (byte*)ptr + (bq->w - bq->p);
            FIF(bq->p);
            bq->p = ptr;
            bq->x = ptr + nsz;
        }
    }

    return bq;
}

void bq_destroy(byte_queue** bq)
{
    if (bq && *bq)
    {
        FIF((*bq)->p);
        FIFZ(bq);
    }
}

void lowwer_case(char* p, size_t len)
{
    for (int i = 0; i < len; i++) {
        p[i] = tolower(p[i]);
    }
}


 /* This code is public-domain - it is based on libcrypt
  * placed in the public domain by Wei Dai and other contributors.
  */



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
