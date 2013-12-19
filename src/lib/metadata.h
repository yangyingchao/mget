#ifndef _METADATA_H_
#define _METADATA_H_

#include "typedefs.h"
#include "fileutils.h"


#define K       (1 << 10)
#define M       (1 << 20)
#define G       (1 << 30)
#define T       (1 << 40)

typedef struct _data_chunk
{
    uint64 cur_pos;
    uint64 end_pos;
} data_chunk;

//TODO: Add magic characters and version number...
typedef struct _metadata
{
    uint64     total_size;
    uint8      nr_chunks;
    uint8      reserved[5];
    uint16     url_length;
    char*      url;
    data_chunk body[0];
} metadata;

#define PA(X, N)       ((X % N) ? (N * ((X/N) + 1)):X)
#define MH_SIZE()      (sizeof(uint64)*2 + sizeof(char*))
#define MD_SIZE(X)      (MH_SIZE()  + sizeof(data_chunk)*X->nr_chunks+PA(X->url_length,4))

#if 0
#define NC_SHIFT 24
#define UL_MASK  ((1 << NC_SHIFT)-1)
#define NC_MASK  (~UL_MASK)
#define NC_PTR(X) (((char*)X)+sizeof(uint64))
#define SET_NC(X, N)       (*NC_PTR(X) = N)
#define GET_NC(X)          ((*((int*)NC_PTR(X)) >> NC_SHIFT) & 0xFF)

#define UL_PTR(X)       ((uint16*)(((char*)X)+sizeof(uint64)+sizeof(uint8)*6))
#define GET_UL(X)          (*UL_PTR(X))
#define SET_UL(X, N)       (*UL_PTR(X) = N)
#endif // End of #if 0

#define GET_URL(X)         (((char*)X)+MH_SIZE()+sizeof(data_chunk)*(X->nr_chunks))

typedef struct _metadata_wrapper
{
    metadata* md;
    fh_map*   fm;
    bool from_file;
} metadata_wrapper;

bool metadata_create_from_file(const char* fn, metadata_wrapper* mw);
bool metadata_create_from_url(const char* url,
                              uint64 size,
                              uint64 start_pos,
                              int nc,
                              metadata** md);
void metadata_destroy(metadata_wrapper* mw);
void metadata_display(metadata* md);
void associate_wrapper(metadata_wrapper* mw);
#endif /* _METADATA_H_ */

