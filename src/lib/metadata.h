#ifndef _METADATA_H_
#define _METADATA_H_

#include "typedefs.h"
#include "fileutils.h"


#define K       (1 << 10)
#define M       (1 << 20)
#define G       (1 << 30) // Max to 4GB.

typedef struct _data_chunk
{
    uint32 cur_pos;
    uint32 end_pos;
} data_chunk;

typedef struct _metadata
{
    uint32     total_size;
    uint8      nr_chunks;
    uint8      flags;
    uint16     url_length;
    char*      url;
    data_chunk body[0];
} metadata;

#define PA_4B(X)       (X % 4 ? 4 * ((X/4) + 1): X)
#define MH_SIZE()      (sizeof(int)*2 + sizeof(char*))
#define MD_SIZE(X)      (MH_SIZE()  + sizeof(data_chunk)*GET_NC(X)+PA_4B(GET_UL(X)))

#define NC_SHIFT 24
#define UL_MASK  ((1 << NC_SHIFT)-1)
#define NC_MASK  (~UL_MASK)
#define GET_UL(X)          (*(((int*)X)+1) & UL_MASK)
#define SET_NC(X, N)       *(((int*)X)+1) = (GET_UL(X) | (N << NC_SHIFT))
#define GET_NC(X)          ((*(((int*)X)+1) >> NC_SHIFT) & 0xFF)
#define SET_UL(X, N)       *(((int*)X)+1) = ((*(((int*)X)+1) & NC_MASK) | N)
#define GET_URL(X)         (((char*)X)+MH_SIZE()+sizeof(data_chunk)*(GET_NC(X)))

typedef struct _metadata_wrapper
{
    metadata* md;
    fh_map*   fm;
    bool from_file;
} metadata_wrapper;

bool metadata_create_from_file(const char* fn, metadata_wrapper* mw);
bool metadata_create_from_url(const char* url,
                              uint32 size,
                              uint32 start_pos,
                              int nc,
                              metadata** md);
void metadata_destroy(metadata_wrapper* mw);
void metadata_display(metadata* md);
#endif /* _METADATA_H_ */
