#ifndef _METADATA_H_
#define _METADATA_H_

#include "typedefs.h"

typedef struct _data_chunk
{
    uint32 chunk_start;
    uint32 chunk_end;
    uint32 cur_pos;
} data_chunk;

typedef struct _data_chunk_list
{
    data_chunk*              chunk;
    struct _data_chunk_list* next;
} data_chunk_list;

typedef struct _metadata_head
{
    uint32 total_size;
    uint8  nr_chunks;
    uint8  reserved[3];
} metadata_head;

typedef struct _metadata
{
    metadata_head   head;
    data_chunk_list body;
} metadata;

/**
 * @name metadata_read_from_file - Reads metadata from specified file.
 * @param fn - name of file to be read from.
 * @return pointer of metadata.
 */
metadata* metadata_read_from_file(const char* fn);

/**
 * @name metadata_save_to_file - save metadata to specified file.
 * @param fn - name of file to be written.
 * @return 0 if succeeded.
 */
int metadata_save_to_file(const char* fn);

/**
 * @name metadata_destroy - destroy specified metadata.
 * @param data -  metadata to be destroyed.
 * @return void
 */
void metadata_destroy(metadata* data);

#endif /* _METADATA_H_ */
