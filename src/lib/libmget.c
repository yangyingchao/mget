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

#include "mget_macros.h"
#include <stdio.h>
#include <strings.h>
#include "log.h"
#include "http.h"
#include "ftp.h"

mget_err start_request(const char *url, const file_name* fn, mget_option* opt,
                       dp_callback cb, bool* stop_flag, void* user_data)
{
    dinfo* info = NULL;
    bool   ret  = dinfo_create(url, fn, opt, &info);
    if (!ret)
    {
        fprintf(stderr, "Failed to create download info\n");
        return ret;
    }

    switch (info->ui->eprotocol) {
        case UP_HTTP:
        case UP_HTTPS:
        {
            ret = process_http_request(info, cb, stop_flag, user_data);
            break;
        }
        case UP_FTP:
        {
            ret = process_ftp_request(info, cb, stop_flag, user_data);
            break;
        }
        default:
        {
            break;
        }
    }

    dinfo_destroy(&info);

    return ret;
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
