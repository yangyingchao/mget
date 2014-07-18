/** mget_thread.h --- interface for threading.
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

#ifndef _MGET_THREAD_H_
#define _MGET_THREAD_H_

#include <gtk/gtk.h>
#include <libmget/libmget.h>
#include <pthread.h>
#include "gm_window.h"

typedef struct _mget_request
{
    const char* url;
    file_name   fn;
    mget_option opts;
    bool        flag;
} mget_request;

typedef struct _gmget_statistics
{
    int            idx;
    uint32         ts;
    uint64         last_recv;
    request_status last_status;
} gmget_statistics;


typedef struct _gmget_request
{
    mget_request request;
    gmget_statistics sts;
    GtkTreeIter  iter;
    struct _GmWindow* window;
} gmget_request;

/**
 * @name start_request_thread - start process request in another thread.
 * @param request -  request to be processed.
 * @return pthread id
 */
pthread_t* start_request_thread(gmget_request* request);

#endif /* _MGET_THREAD_H_ */
