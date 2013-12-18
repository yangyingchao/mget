#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "fileutils.h"
#include "metadata.h"

int main(int argc, char *argv[])
{
    const char* tmp = "/tmp/test.txt";
    fhandle* fh = fhandle_create(tmp, FHM_CREATE);
    printf ("Hello, this is mget (PID: %d)!\n", getpid());

    uint32 start = 0;
    uint32 size = 533 * M;

    data_chunk* dc = chunk_split(start, size, 9);
    data_chunk* cp = dc;
    for (uint8 i = 0; i < 9; ++i)
    {
        cp = dc + i;
        printf ("chunk: %p, cur_pos: %08X, end: %08X (%.02fM)\n",
                cp, cp->cur_pos, cp->end_pos, (float)cp->end_pos/(M));

    }

    metadata* md = malloc(sizeof(metadata_head) + 9 * sizeof(data_chunk));
    memset(md, 0, sizeof(metadata_head) + 9 * sizeof(data_chunk));

    md->head.total_size = size;
    md->head.nr_chunks = 9;
    md->head.from_file = false;
    md->head.mp = fhandle_mmap(fh, 0, sizeof(metadata_head) + 9 * sizeof(data_chunk));

    printf("Copying data chunks to metadata...\n");
    for (uint8 i = 0; i < 9; ++i)
    {
        cp = &md->body[i];
        *cp = *(dc +i);
        cp->cur_pos += 0x1111;
    }

    metadata_display(md);
    metadata_destroy(&md);

    printf("Now md: %p, recreating....\n", md);
    md = metadata_create_from_file(tmp);
    metadata_display(md);
    metadata_destroy(&md);
    return 0;
}
