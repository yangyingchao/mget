#ifndef _METADATA_H_
#define _METADATA_H_

#include "typedefs.h"
#include "fileutils.h"
#include "data_utlis.h"

#define K       (1 << 10)
#define M       (1 << 20)
#define G       (1 << 30)
#define T       (1 << 40)

typedef struct _data_chunk
{
    uint64 cur_pos;
    uint64 end_pos;
} data_chunk;

typedef enum _request_status
{
    RS_INIT = 0,
    RS_STARTED,
    RS_PAUSED,
    RS_SUCCEEDED,
    RS_FAILED,
} request_status;

typedef struct metadata_head
{
    uint32 iden;                        // TMD0
    uint32 version;                     // Major, Minor, Patch, NULL
    uint64 package_size;                // size of package;
    uint64 last_time;                   // last time used.
    uint32 acc_time;                    // accumulated time in this downloading.

    uint8  status;                      // status.
    uint8  nr_chunks;                   // number of chunks.
    uint16 eb_length;                   // length of extra body: url_len+mime_len+others

    uint8  reserved[16];                // reserved ...
} mh;                                   // up to 48 bytes

// Byte/     0       |       1       |       2       |       3       |
// ***/              |               |               |               |
// **|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|
// **+-------------------------------+-------------------------------+
// **|               DATA_CHUNK  START                               |
// **+---------------------------------------------------------------+
// **|               ..................           .                  |
// **+---------------------------------------------------------------+
// **|               DATA_CHUNK  END                                 |
// **+-------------------------------+-------------------------------+
// **|            URL    MIME   OTHERS   .....                       |
// **+-------------------------------+-------------------------------+

typedef struct _metadata
{
    mh          hd;                     // header, up to 48 bytes.
    data_chunk* body;                   // pointer to data_chunk
    char*       url;                    // pointer to url
    char*       mime;                   // pointer to mime type
    hash_table* ht;                     // hash table of extra body.
    char        raw_data[0];            // start to body of raw_data.
} metadata;

#define PA(X, N)       ((X % N) ? (N * ((X/N) + 1)):X)
#define MH_SIZE()      48
#define MD_SIZE(X)     (MH_SIZE()+sizeof(void*)*4+sizeof(data_chunk)*(X->hd.nr_chunks)+PA(X->hd.eb_length,4))
#define CHUN_NUM(X)       (X->hd.nr_chunks)
#define CHUNK_SIZE(X)      (sizeof(data_chunk)*(X->hd.nr_chunks))
#define GET_URL(X)         (((char*)X->raw_data)+CHUNK_SIZE(X))

typedef struct _metadata_wrapper
{
    metadata* md;
    fh_map*   fm;
    bool from_file;
} metadata_wrapper;

bool metadata_create_from_file(const char* fn, metadata_wrapper* mw);
bool metadata_create_from_url(const char* url,
                              uint64      size,
                              int         nc,
                              mget_slis*  lst,
                              metadata** md);
void metadata_destroy(metadata_wrapper* mw);
void metadata_display(metadata* md);
void associate_wrapper(metadata_wrapper* mw);
#endif /* _METADATA_H_ */
