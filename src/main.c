#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "libmget.h"
#include "timeutil.h"
#include <signal.h>

#define MAX_NC       40

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

bool control_byte = false;

void sigterm_handler(int signum)
{
    control_byte = true;
    fprintf(stderr, "Saving temporary data...\n");
}

void show_progress(metadata* md)
{
    static int idx = 0;
    static uint32 ts = 0;
    static uint64 last_recv = 0;

    if (md->hd.status == RS_SUCCEEDED)
    {
        printf("Download finished, total cost: %s....\n",
               stringify_time(md->hd.acc_time));
        return;
    }

    int threshhold = 78 * md->hd.acon / md->hd.nr_chunks;

    if (idx++ < threshhold)
    {
        if (!ts)
        {
            ts = get_time_ms();
        }
        fprintf(stderr, ".");
    }
    else
    {
        data_chunk* dp = md->body;
        uint64 total = md->hd.package_size;
        uint64 recv  = 0;
        for (int i = 0; i < md->hd.nr_chunks; ++i)
        {
            recv += dp->cur_pos - dp->start_pos;
            dp++;
        }

        uint64 diff_size = recv-last_recv;
        char* s2 = strdup(stringify_size(diff_size));
        uint32 c_time = get_time_ms();
        fprintf(stderr, "Progress: received %s in %.02f seconds, %.02f percent, %.02fKB/s\n",
                s2, (double)(c_time-ts)/1000, (double)recv/total * 100,
                (double)(diff_size)*1000/K/(c_time -ts));
        idx       = 0;
        last_recv = recv;
        ts = c_time;
        free(s2);
    }
}

void usage(int argc, char *argv[])
{
    printf("Usage:\n\tTo download url and store to directory:"
           "\t%s -d directory url..\n", argv[0]);
    printf("or:\n\tTo show metadata of file:\n\t"
           "%s -s file..\n", argv[0]);
}
int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        usage(argc, argv);
        return 1;
    }


    bool view_only = false;
    int  opt       = 0;
    int  nc        = 5;                 // default number of connections.

    file_name fn;
    memset(&fn, 0, sizeof(file_name));

    while ((opt = getopt(argc, argv, "j:d:o:s")) != -1) {
        switch (opt)
        {
            case 'd':
            {
                fn.dirn = strdup(optarg);
                printf ("dirn: %s\n", fn.dirn);
                break;
            }
            case 's':
            {
                view_only = true;
                break;
            }
            case 'j':
            {
                nc = atoi(optarg);
                if (nc > MAX_NC)
                {
                    printf ("Max connections: "
                            "specified: %d, allowed: %d, "
                            "set it to max...\n",
                            nc, MAX_NC);
                    nc = MAX_NC;
                }

                break;
            }
            case 'o':
            {
                fn.basen = strdup(optarg);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    char* target = optind <= argc ? argv[optind] : NULL;
    if (!target)
    {
        usage(argc, argv);
        exit(1);
    }

    if (view_only)
    {
        if (file_existp(target))
        {
            printf ("showing tmd file: %s\n", target);

            metadata_wrapper mw;
            metadata_create_from_file(target, &mw);
            metadata_display(mw.md);
            metadata_destroy(&mw);
        }
        else
        {
            printf("File: %s not exists!\n", target);
        }
    }
    else
    {
        printf ("downloading file: %s, saving to %s/%s\n", target,
                fn.dirn ? fn.dirn : (fn.basen ? "":"."), fn.basen ? fn.basen:"");
        struct sigaction act;
        act.sa_handler   = sigterm_handler;
        act.sa_sigaction = NULL;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        int ret = sigaction(SIGINT, &act, NULL);
        PDEBUG ("ret = %d\n", ret);

        signal(SIGINT, sigterm_handler);
        start_request(target, &fn, nc, show_progress, &control_byte);
    }

    return 0;
}
