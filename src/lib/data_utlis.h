#ifndef _DATA_UTLIS_H_
#define _DATA_UTLIS_H_

// #ifdef __cplusplus
// extern "C" {
// #endif

#include "typedefs.h"

typedef void (*free_func)   (void*);

typedef struct _mget_slis
{
    void*     data;
    free_func f;
    struct _mget_slis* next;
}mget_slis;

mget_slis* mget_slist_append(mget_slis* l, void*data, free_func f);
void       mget_slist_free(mget_slis* lst);

typedef struct _kv_pair
{
    char* k;
    char* v;
} kvp;


// Hash Tables.

typedef void (*DestroyFunction)(void* data);
typedef uint32  (*HashFunction)(const char* key);

typedef struct _TableEntry
{
    char* key;
    void* val;
} TableEntry;

typedef struct _hash_table
{
    int         capacity;
    TableEntry* entries;

    void*           bufer;              // Serailized buffer.
    HashFunction    hashFunctor;
    DestroyFunction deFunctor;
} hash_table;


// Functions.
void hash_tableDestroy(hash_table* table);
hash_table* hash_tableCreate(uint32 size, DestroyFunction dFunctor);
bool InsertEntry(hash_table* table, char* key, void* val);
void* GetEntryFromhash_table(hash_table* table, char* key);

void dump_hash_table(hash_table* ht, void* buffer);

char* rstrip(char* str);
// #ifdef __cplusplus
// }
// #endif

#endif /* _DATA_UTLIS_H_ */
