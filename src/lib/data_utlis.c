#include "data_utlis.h"
#include <stdlib.h>
#include "debug.h"
#include <string.h>

mget_slis* mget_slis_append(mget_slis* l, void*data, free_func f)
{
    return NULL;
}



// Hash table operations.

static const int   HASH_SIZE    = 256;

uint32 StringHashFunction(const char* str)
{
    uint32 hash = 0;
    uint32 i    = 0;
    const char*  key  = str;

    for (; i < strlen(str); ++i)
    {
        hash += *(key + i);
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash % HASH_SIZE;
}

void hash_table_destroy(hash_table* table)
{
    if (table)
    {
        if (table->entries)
        {
            int i = 0;
            for (; i < table->capacity; ++i)
            {
                TableEntry* entry = &table->entries[i];
                if (entry->key)
                {
                    free(entry->key);
                }
                if (entry->val && table->deFunctor)
                {
                    table->deFunctor(entry->val);
                }
            }
            free(table->entries);
        }
        free(table);
    }
}

hash_table* hash_table_create(uint32 hashSize, DestroyFunction dFunctor)
{
    hash_table* table = malloc(sizeof(hash_table));
    if (table)
    {
        memset(table, 0, sizeof(hash_table));
        if (hashSize < HASH_SIZE)
        {
            hashSize = HASH_SIZE;
        }

        table->capacity    = hashSize;
        table->entries     = ZALLOC(TableEntry, hashSize);
        table->hashFunctor = StringHashFunction; // use default one.
        table->deFunctor   = dFunctor;

        if (table->entries)
        {
            memset(table->entries, 0, sizeof(TableEntry) * hashSize);
        }
        else
        {
            hash_table_destroy(table);
            table = NULL;
        }
    }
    return table;
}

bool hash_table_insert(hash_table* table, char* key, void* val)
{
    if (!table || !key || !val )
    {
        return false;
    }

    // Insert entry into the first open slot starting from index.
    uint32 i;
    for (i = table->hashFunctor(key); i < table->capacity; ++i)
    {
        TableEntry* entry = &table->entries[i];
        if (entry->key == NULL)
        {
            entry->key = key;
            entry->val = val;
            return true;
        }
    }
    return false;
}

/*! Looks for the given data based on key.

  @return void*
*/
void* hash_table_entry_get(hash_table* table, const char* key)
{
    TableEntry* entry = NULL;
    uint32 index = table->hashFunctor(key);
    int i;
    for (i = index; i < table->capacity; ++i)
    {
        entry = &table->entries[i];
        if (entry->key == NULL)
        {
            return NULL;
        }
        if (strcmp(entry->key, key) == 0)
        {
            break;
        }
    }
    if (entry)
    {
        /* PDEBUG("Key: %s - %s, val: %p\n", key, entry->key, entry->val); */
        return entry->val;
    }
    return NULL;
}

void dump_hash_table(hash_table* ht, void* buffer)
{
}


char* rstrip(char* str)
{
    if (str)
    {
        char* ptr = str + strlen(str);
        while ((*ptr == ' ' || *ptr == '\t' || *ptr == '\0') && ptr>str)
        {
            *ptr = '\0';
            ptr--;
        }
    }
    return str;
}


const char* stringify_size(uint64 sz)
{
    static char str_size[64] = {'\0'};
    memset(str_size, 0, 64);

    if (sz < M)
    {
        sprintf(str_size, "%.02fKB", (double)sz/K);
    }
    else if (sz < G)
    {
        sprintf(str_size, "%.02fMB", (double)sz/M);
    }
    else
    {
        sprintf(str_size, "%.02fGB", (double)sz/G);
    }

    return str_size;
}


