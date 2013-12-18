#ifndef _METADATA_H_
#define _METADATA_H_

#include "typedefs.h"

typedef struct _data_chunk
{
    uint32 cur_pos;
    uint32 end_pos;
} data_chunk;

typedef struct _metadata_head
{
    uint32 total_size;
    uint8  nr_chunks;
    uint8  from_file;
    uint8  reserved[2];
    void*  mp;
} metadata_head;

typedef struct _metadata
{
    metadata_head head;
    data_chunk    body[0];
} metadata;

/**
 * @name metadata_read_from_file - Reads metadata from specified file.
 * @param fn - name of file to be read from.
 * @return pointer of metadata.
 */
metadata* metadata_create_from_file(const char* fn);

/**
 * @name metadata_destroy - destroy specified metadata.
 * @param data -  metadata to be destroyed.
 * @return void
 */
void metadata_destroy(metadata** md);

void metadata_display(metadata* md);

#define K       (1 << 10)
#define M       (1 << 20)
#define G       (1 << 30) // Max to 4GB.


data_chunk* chunk_split(uint32 start, uint32 size, int num);
void chunk_destroy(data_chunk* dc);

#endif /* _METADATA_H_ */
