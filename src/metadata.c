#include "metadata.h"
#include <stdlib.h>

// Byte/     0       |       1       |       2       |       3       |
// ***/              |               |               |               |
// **|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|0 1 2 3 4 5 6 7|
// **+---------------------------------------------------------------+
// **|                        Total Size                             |
// **+---------------+-----------------------------------------------+
// **| Num of Chunks |          RESERVED                             |
// **+---------------+-----------------------------------------------+
// **|                     CHUNK1 START                              |
// **+---------------------------------------------------------------+
// **|                     ......                                    |
// **+---------------------------------------------------------------+
// **|                     .....                                     |
// **+---------------------------------------------------------------+
// **|                     CHUNK1 END                                |
// **+---------------------------------------------------------------+
// **|                     CHUNK2 START                              |
// **+---------------------------------------------------------------+
// **|                     ......                                    |
// **+---------------------------------------------------------------+
// **|                     .....                                     |
// **+---------------------------------------------------------------+
// **|                     CHUNK2 END                                |
// **+---------------------------------------------------------------+
// **|                     ...........                               |
// **+---------------------------------------------------------------+


metadata* metadata_read_from_file(const char* fn)
{
    return NULL;
}

int metadata_save_to_file(const char* fn)
{
    return 0;
}

void metadata_destroy(metadata* data)
{
}

