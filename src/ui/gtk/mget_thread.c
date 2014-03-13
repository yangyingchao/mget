/** mget_thread.c --- implementation of mget_thread.
 *
 * Copyright (C) 2014 Yang,Ying-chao
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

#include "mget_thread.h"
#include <stdlib.h>

void update_progress(metadata* md)
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
        uint64 remain    = total - recv;
        uint32 c_time    = get_time_ms();
        uint64 bps       = (uint64)((double) (diff_size) * 1000 / (c_time - ts));

        fprintf(stderr,
                "] %.02f percent finished, speed: %s/s, eta: %s\r",
                (double) recv / total * 100,
                stringify_size(bps),
                stringify_time((total-recv)/bps));
        fprintf(stderr, "\n");

        idx       = 0;
        last_recv = recv;
        ts        = c_time;
    }
}

/*
  it will:
  1. create thread.
  2. in the call back, it will emit "update-progress" signal.
 */
pthread_t start_request_thread(gmget_request* request)
{
    return 0;
}
