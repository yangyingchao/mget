#include "data_utlis.h"
#include "debug.h"

mget_slis* mget_slis_append(mget_slis* l, void*data, free_func f)
{
    return NULL;
}



// Hash table operations.

void hash_tableDestroy(hash_table* table)
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

hash_table* hash_tableCreate(uint32 hashSize, HashFunction cFunctor, DestroyFunction dFunctor)
{
    hash_table* table = malloc(sizeof(hash_table));
    if (table)
    {
        memset(table, 0, sizeof(hash_table));

        table->capacity    = hashSize;
        table->entries     = malloc(sizeof(TableEntry) * hashSize);
        table->hashFunctor = cFunctor;
        table->deFunctor   = dFunctor;

        if (table->entries)
        {
            memset(table->entries, 0, sizeof(TableEntry) * hashSize);
        }
        else
        {
            hash_tableDestroy(table);
            table = NULL;
        }
    }
    return table;
}

int InsertEntry(hash_table* table, char* key, void* val)
{
    int ret = 0;
    if (!table || !key || !val )
    {
        return ret;
    }

    PDEBUG ("KEY: %s, val: %p\n", key, val);

    uint32 index = table->hashFunctor(key);
    // Insert entry into the first open slot starting from index.
    uint32 i;
    for (i = index; i < table->capacity; ++i)
    {
        TableEntry* entry = &table->entries[i];
        if (entry->key == NULL)
        {
            ret        = 1;
            entry->key = key;
            entry->val = val;
            break;
        }
    }
    return ret;
}

/*! Looks for the given data based on key.

  @return void*
*/
void* GetEntryFromhash_table(hash_table* table, char* key)
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
        PDEBUG("Key: %s - %s, val: %p\n",
               key, entry->key, entry->val);
    }
    return entry->val;
}

void dump_hash_table(hash_table* ht, void* buffer)
{
}
