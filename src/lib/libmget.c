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
#include "libmget.h"
#include "logutils.h"
#include "download_info.h"
#include "data_utlis.h"
#include "protocols.h"
#include "connection.h"
#include <stdio.h>
#include <strings.h>

extern log_level g_log_level;
extern host_cache_type g_hct;

typedef mget_err(*protocol_handler) (dinfo *, dp_callback, bool *, void *);
static hash_table *g_handlers = NULL;

mget_err start_request(const char *url, const file_name * fn,
                       mget_option * opt, dp_callback cb, bool * stop_flag,
                       void *user_data)
{
    dinfo *info = NULL;
    mget_err ret = ME_OK;
    if (!dinfo_create(url, fn, opt, &info)) {
        fprintf(stderr, "Failed to create download info\n");
        ret = ME_RES_ERR;
        return ret;
    }

    if (!g_handlers && !(g_handlers = collect_handlers())) {
        fprintf(stderr, "Failed to scan protocol handlers\n");
        return ME_RES_ERR;
    }

    protocol_handler handler =
        (protocol_handler) hash_table_entry_get(g_handlers,
                                                info->ui->protocol);
    if (!handler) {
        fprintf(stderr, "Protocol: %s is not supported...\n",
                info->ui->protocol);
        return ME_NOT_SUPPORT;
    }

    mlog(LL_ALWAYS, "Using handler: %p for %s\n", handler,
         info->ui->protocol);

    g_log_level = opt->ll;
    g_hct = opt->hct;
    if (opt->limit > 0)
        set_global_bandwidth(opt->limit);

    ret = handler(info, cb, stop_flag, user_data);

    dinfo_destroy(info);

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
