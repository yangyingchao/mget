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
#include "lib/libmget.h"
#include "lib/mget_utils.h"
#include <signal.h>

#define MAX_NC       40

#define handle_error(msg)                               \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

bool control_byte = false;

static const int MAX_RETRY_TIMES = 3;

void sigterm_handler2(int sig, siginfo_t * si, void *param)
{
    control_byte = true;
    fprintf(stderr, "Saving temporary data...\n");
}

#define PROG_REST() idx = ts = last_recv = 0
static int idx = 0;
static uint32 ts = 0;
static uint64 last_recv = 0;

void show_progress(metadata * md, void *user_data)
{
    if (md->hd.status == RS_FINISHED) {
        char *date = current_time_str();

        printf("%s - %s saved in %s [%.02fKB/s] ...\n",
               date, md->ptrs->fn,
               stringify_time(md->hd.acc_time),
               (double) (md->hd.package_size) / K / md->hd.acc_time);
        free(date);
        return;
    }

    int threshhold = 78 * md->hd.acon / md->hd.nr_effective;
    if (idx < threshhold) {
        if (!ts) {
            ts = get_time_ms();
            if (!last_recv) {
                // If last_recv is zero, try to load it from metadata.
                data_chunk *dp = md->ptrs->body;
                for (int i = 0; i < md->hd.nr_effective; ++i) {
                    last_recv += dp->cur_pos - dp->start_pos;
                    dp++;
                }
            }

            fprintf(stderr, ".");
        } else if ((get_time_ms() - ts) > 1000 / threshhold * idx) {
            fprintf(stderr, ".");
            idx++;
        }
    } else {
        data_chunk *dp = md->ptrs->body;
        uint64 total = md->hd.package_size;
        uint64 recv = 0;

        for (int i = 0; i < md->hd.nr_effective; ++i) {
            recv += dp->cur_pos - dp->start_pos;
            dp++;
        }

        uint64 diff_size = recv - last_recv;
        uint64 remain = total - recv;
        uint32 c_time = get_time_ms();
        uint64 bps =
            (uint64) ((double) (diff_size) * 1000 / (c_time - ts)) + 1;

        fprintf(stderr,
                "] %.02f, SPD: %s/s, ETA: %s\r",
                (double) recv / total * 100,
                stringify_size(bps), stringify_time((total - recv) / bps));
        fprintf(stderr, "\n");

        idx = 0;
        last_recv = recv;
        ts = c_time;
    }
}

void print_help()
{
    static const char *help[] = {
        "\nOptions:\n",
        "\t-v:  show version of mget.\n",
        "\t-j:  max connections (should be smaller than 40).\n",
        "\t-d:  set folder to store downloaded data.\n",
        "\t-o:  set file name to store downloaded data."
            "If not provided, mget will name it.\n",
        "\t-r:  resume a previous download using stored metadata.\n",
        "\t-u:  set user name.\n",
        "\t-p:  set user password.\n",
        "\t-s:  show metadata of unfinished task.\n",
        "\t-l:  set log level(0-9) of mget."
            " The smaller value means less verbose.\n",
        "\t-H:  set host cache type, can be one of 'B', 'D' or 'U':\n",
        "\t     'B': Bypass, don't use cached addresses.\n",
        "\t     'D': Default, use host cache to reduce time cost for "
            "resolving host names.\n",
        "\t     'U': Update, get address from DNS server instead of "
            "from cache, but update cache after name resolved.\n",
        "\t-L:  limit bandwidth.\n",
        "\t-h:  show this help.\n",
        "\n",
        NULL
    };

    printf("Mget %s, non-interactive network retriever "
           "with multiple connections\n", VERSION_STRING);

    const char **ptr = help;
    while (*ptr != NULL) {
        printf("%s", *ptr);
        ptr++;
    }
}

int main(int argc, char *argv[])
{

    if (argc == 1) {
        print_help();
        return 1;
    }

    bool view_only = false;
    int opt = 0;

    file_name fn;
    char *target = NULL;
    bool resume = false;
    char *user = NULL;
    char *passwd = NULL;
    mget_option opts;
    memset(&opts, 0, sizeof(mget_option));
    opts.max_connections = -1;
    opts.ll = LL_NONVERBOSE;

    memset(&fn, 0, sizeof(file_name));

    while ((opt = getopt(argc, argv, "hH:j:d:o:r:svu:p:l:L:")) != -1) {
        switch (opt) {
        case 'h':
            {
                print_help();
                exit(0);
            }
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
                opts.max_connections = atoi(optarg);
                if (opts.max_connections > MAX_NC) {
                    printf("Max connections: "
                           "specified: %d, allowed: %d, "
                           "set it to max...\n", opts.max_connections,
                           MAX_NC);
                    opts.max_connections = MAX_NC;
                }

                break;
            }
        case 'H':
            {
                switch (*optarg) {
                case 'B':
                    {
                        opts.hct = HC_BYPASS;
                        break;
                    }
                case 'U':
                    {
                        opts.hct = HC_UPDATE;
                        break;
                    }
                default:
                    {
                        opts.hct = HC_DEFAULT;
                    }
                }
                break;
            }
        case 'u':
            {
                opts.user = strdup(optarg);
                break;
            }
        case 'p':
            {
                opts.passwd = strdup(optarg);
                break;
            }
        case 'l':
            {
                int dl = ((int) LL_ALWAYS) - atoi(optarg);      // debug level
                if (dl < 0)
                    dl = LL_INVLID;
                opts.ll = ((log_level) dl);
                break;
            }
        case 'L':
            {
                opts.limit = integer_size(optarg);
                break;
            }
        case 'o':
            {
                fn.basen = strdup(optarg);
                break;
            }
        case 'r':              // resume downloading
            {
                fn.basen = strdup(optarg);
                resume = true;
                break;
            }
        case 'v':
            {
                printf("mget version: %s\n", VERSION_STRING);
                return 0;
                break;
            }
        default:
            {
                fprintf(stderr, "Wrong usage..\n");
                print_help();
                exit(-1);
            }
        }
    }

    target = optind <= argc ? argv[optind] : NULL;

    if (!resume && !target) {
        print_help();
        exit(1);
    }

    if (view_only) {
        if (file_existp(target)) {
            metadata_inspect(target, &opts);
        } else {
            printf("File: %s not exists!\n", target);
        }
    } else {
        if (!resume) {
            printf("downloading file: %s, saving to %s/%s\n", target,
                   fn.dirn ? fn.dirn : (fn.basen ? "" : "."),
                   fn.basen ? fn.basen : "");
        } else {
            printf("Resume download for file: %s\n", fn.basen);
        }

        struct sigaction act;

        act.sa_sigaction = sigterm_handler2;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        int ret = sigaction(SIGINT, &act, NULL);

        for (int i = optind; i < argc; i++) {
            int retry_time = 0;
            mget_err result = ME_OK;
            PROG_REST();
            control_byte = false;

            target = argv[i];
            while (retry_time++ < MAX_RETRY_TIMES && !control_byte) {
                result = start_request(target, &fn, &opts, show_progress,
                                       &control_byte, NULL);
                if (result == ME_OK || result > ME_DO_NOT_RETRY) {
                    goto next;
                }
            }

            printf("Failed to download from: %s, retried time:  %d.\n",
                   target, retry_time);
          next:;
        }
    }

    return 0;
}

/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * fill-column: 78
 * End:
 *
 * vim: set noet ts=4 sw=4:
 */