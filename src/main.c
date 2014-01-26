/** main.c --- source file of tmget, a sample app of libmget.
 *
 * Copyright (C) 2013 Yang,Ying-chao
 *
 * Author: Yang,Ying-chao <yangyingchao@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "libmget.h"
#include "timeutil.h"
#include <signal.h>

#ifdef DEBUG
#define PDEBUG(fmt, args...)                                            \
    do {                                                                \
        char msg[256] = {'\0'};                                         \
        const char* file = __FILE__;                                    \
        const char* ptr = file;                                         \
        const char* sep = "/";                                          \
        while ((ptr = strstr(file, sep)) != NULL) {                     \
            file = ptr;                                                 \
            file = ++ptr;                                               \
        }                                                               \
                                                                        \
        sprintf(msg, "TDEBUG: - %s(%d)-%s\t: ",                         \
                file, __LINE__,__FUNCTION__);                           \
        fprintf(stderr, strcat(msg, fmt ), ##args);                     \
        } while(0)
#endif  /*End of if PDEBUG*/

#define MAX_NC       40

#define handle_error(msg)                               \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

bool control_byte = false;

void sigterm_handler(int signum)
{
    control_byte = true;
    fprintf(stderr, "Saving temporary data...\n");
}

void show_progress(metadata* md)
{
    static int    idx       = 0;
    static uint32 ts        = 0;
    static uint64 last_recv = 0;

    if (md->hd.status == RS_SUCCEEDED) {
        char *date = current_time_str();

        printf("%s - %s saved in %s [%.02fKB/s] ...\n",
               date, md->fn,
               stringify_time(md->hd.acc_time),
               (double) (md->hd.package_size) / K / md->hd.acc_time);
        free(date);
        return;
    }

    int threshhold = 78 * md->hd.acon / md->hd.nr_effective;

    if (idx < threshhold) {
        if (!ts) {
            ts = get_time_ms();
            fprintf(stderr, ".");
        } else if ((get_time_ms() - ts) > 1000 / threshhold * idx) {
            fprintf(stderr, ".");
            idx++;
        }
    } else {
        data_chunk *dp    = md->body;
        uint64      total = md->hd.package_size;
        uint64      recv  = 0;

        for (int i = 0; i < md->hd.nr_effective; ++i) {
            recv += dp->cur_pos - dp->start_pos;
            dp++;
        }

        uint64 diff_size = recv - last_recv;
        char *s2 = strdup(stringify_size(diff_size));
        uint32 c_time    = get_time_ms();

        fprintf(stderr,
                "Progress: received %s in %.02f seconds, %.02f percent, %s/s\n",
                s2, (double) (c_time - ts) / 1000,
                (double) recv / total * 100,
                stringify_size((size_t)((double) (diff_size) * 1000 / (c_time - ts))));
        idx       = 0;
        last_recv = recv;
        ts        = c_time;
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

    if (argc == 1) {
        usage(argc, argv);
        return 1;
    }

    bool view_only = false;
    int  opt       = 0;
    int  nc        = 5;                 // default number of connections.

    file_name fn;
    char *target = NULL;
    bool resume = false;

    memset(&fn, 0, sizeof(file_name));

    while ((opt = getopt(argc, argv, "j:d:o:r:s")) != -1) {
        switch (opt) {
            case 'd':
            {
                fn.dirn = strdup(optarg);
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
                if (nc > MAX_NC) {
                    printf("Max connections: "
                           "specified: %d, allowed: %d, "
                           "set it to max...\n", nc, MAX_NC);
                    nc = MAX_NC;
                }

                break;
            }
            case 'o':
            {
                fn.basen = strdup(optarg);
                break;
            }
            case 'r': // resume downloading
            {
                fn.basen = strdup(optarg);
                resume = true;
                break;
            }
            default:
            {
                break;
            }
        }
    }

    target = optind <= argc ? argv[optind] : NULL;

    if (!resume && !target) {
        usage(argc, argv);
        exit(1);
    }

    if (view_only) {
        if (file_existp(target)) {
            printf("showing tmd file: %s, TBD...\n", target);
            // TODO: Remove this ifdef!
#if 0

            metadata_wrapper mw;

            metadata_create_from_file(target, &mw);
            metadata_display(mw.md);
            metadata_destroy(&mw);
#endif // End of #if 0

        } else {
            printf("File: %s not exists!\n", target);
        }
    } else {
        if (!resume)  {
            printf("downloading file: %s, saving to %s/%s\n", target,
                   fn.dirn ? fn.dirn : (fn.basen ? "" : "."),
                   fn.basen ? fn.basen : "");
        }
        else  {
            printf ("Resume download for file: %s\n", fn.basen);
        }

        struct sigaction act;

        act.sa_handler   = sigterm_handler;
        act.sa_sigaction = NULL;
        sigemptyset(&act.sa_mask);
        act.sa_flags     = 0;
        int ret = sigaction(SIGINT, &act, NULL);

        PDEBUG("ret = %d\n", ret);

        signal(SIGINT, sigterm_handler);
        start_request(target, &fn, nc, show_progress, &control_byte);
    }

    return 0;
}
