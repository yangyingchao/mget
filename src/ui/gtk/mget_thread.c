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

#define _BSD_SOURCE
#include <stdio.h>
#include <string.h>
#include "mget_thread.h"
#include <libmget/mget_macros.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#define BASE_NAME(X)       basename(X)


static inline task_status convert_task_status(request_status s)
{
    switch (s)
    {
        case RS_INIT:
        {
            return TS_CREATED;
        }
        case RS_PAUSED:
        {
            return TS_PAUSED;
        }
        case RS_STARTED:
        {
            return TS_STARTED;
        }
        case RS_FAILED:
        {
            return TS_FAILED;
        }
        case RS_FINISHED:
        {
            return TS_FINISHED;
            break;
        }
        default:
        {
            abort();
            break;
        }
    }
}

void update_progress(metadata* md, void* user_data)
{
    gmget_request* req = (gmget_request*)user_data;

    if (!md || !req) {
        return;
    }

    static char fname[1024];
    memset(fname, 0, 1024);
    sprintf(fname, "%s", md->fn);
    char* bname = basename(fname);

    if (req->sts.last_status != md->hd.status) {
        req->sts.last_status = md->hd.status;

        char* msg = NULL;

        switch (req->sts.last_status) {
            case RS_INIT: {
                asprintf(&msg, "Download task created...");
                break;
            }
            case RS_STARTED: {
                asprintf(&msg, "Download task started...");
                break;
            }
            case RS_PAUSED: {
                asprintf(&msg, "Download task pasued...");
                break;
            }
            case RS_FINISHED: {
                asprintf(&msg, "Download task finished...");
                break;
            }
            case RS_FAILED: {
                asprintf(&msg, "Download task failed...");
                break;
            }
            default:{
                asprintf(&msg, "Unkown status: %d...", req->sts.last_status);
                break;
            }
        }

        g_signal_emit_by_name(req->window,
                              "status-changed",
                              bname,
                              req->sts.last_status,
                              msg,
                              user_data);

        //XXX: mem leak here!
        /* free(msg); */
    }



    int threshhold = 78 * md->hd.acon / md->hd.nr_effective;
    if (req->sts.idx < threshhold && md->hd.status == RS_STARTED) {
        if (!req->sts.ts) {
            req->sts.ts = get_time_ms();
        }
        else if ((get_time_ms() - req->sts.ts) > 1000 / threshhold * req->sts.idx) {
            req->sts.idx++;
        }
    }
    else {
        data_chunk *dp    = md->body;
        uint64      total = md->hd.package_size;
        uint64      recv  = 0;

        for (int i = 0; i < md->hd.nr_effective; ++i) {
            recv += dp->cur_pos - dp->start_pos;
            dp++;
        }

        uint64 diff_size = recv - req->sts.last_recv;
        uint64 remain    = total - recv;
        uint32 c_time    = get_time_ms();
        uint32 time_cost = c_time - req->sts.ts;
        if (!time_cost)    {
            time_cost = 1;
        }

        uint64 bps       = (uint64)((double) (diff_size) * 1000 / time_cost);

        char* sz = strdup(stringify_size(total));
        g_signal_emit_by_name(req->window, "update-progress",
                              bname,
                              sz,
                              (double) recv / total * 100,
                              bps ? stringify_size(bps) : "--",
                              bps ? stringify_time((total-recv)/bps) : "--",
                              user_data);

        /* fprintf(stderr, */
        /*         "] %.02f percent finished, speed: %s/s, eta: %s\r", */
        /*         (double) recv / total * 100, */
        /*         stringify_size(bps), */
        /*         stringify_time((total-recv)/bps)); */
        /* fprintf(stderr, "\n"); */
        /* free(sz); */ // XXX: leak
        req->sts.idx       = 0;
        req->sts.last_recv = recv;
        req->sts.ts        = c_time;
    }
}

static void* worker_thread(void* param)
{
    gmget_request* request = (gmget_request*) param;
    fprintf(stderr, "url: %s\n",request->request.url);
    int r = start_request(request->request.url,
                          &request->request.fn,
                          request->request.nc,
                          update_progress,
                          &request->request.flag,
                          param);
    printf ("r = %d\n", r);
    return NULL;
}

/*
  it will:
  1. create thread.
  2. in the call back, it will emit "update-progress" signal.
 */
pthread_t* start_request_thread(gmget_request* request)
{
    pthread_t* tid = ZALLOC1(pthread_t);

    int ret = pthread_create(tid, NULL, worker_thread, request);
    if (ret)
    {
        return tid;
    }

    free(tid);
    return NULL;
}
