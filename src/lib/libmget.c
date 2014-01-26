/** libmget.c --- implementation of libmget.
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

#include "macros.h"
#include <stdio.h>
#include <strings.h>
#include "log.h"
#include "http.h"

typedef void (*request_processor) (url_info * ui, const char *dn, int nc,
				   void (*cb) (metadata * md),
				   bool * stop_flag);

bool start_request(const char *url, const file_name * fn, int nc,
                   dp_callback cb, bool * stop_flag)
{
    dinfo* info = NULL;
    bool   ret  = dinfo_create(url, fn, nc, &info);
    if (!ret)
    {
        fprintf(stderr, "Failed to create download info\n");
        return ret;
    }

    url_info_display(info->ui);
    switch (info->ui->eprotocol) {
        case UP_HTTP:
        case UP_HTTPS:
        {
            int ret = process_http_request(info, cb, stop_flag);

            if (ret == 0) {
                break;
            }
        }
        default:
        {
            break;
        }
    }

    dinfo_destroy(&info);

    return ret;
}

char* get_version()
{}
