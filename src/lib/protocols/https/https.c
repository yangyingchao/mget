/** https.c --- implementation of libmget using http
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
#include "libmget.h"
#include "download_info.h"

extern mget_err process_http_request(dinfo * info,
                                     dp_callback cb,
                                     bool* stop_flag,
                                     mget_option* opt,
                                     void*user_data);

// does nothing but forward this to http...
mget_err process_https_request(dinfo * info,
                               dp_callback cb,
                               bool* stop_flag,
                               mget_option* opt,
                               void* user_data)
{
    return process_http_request(info, cb, stop_flag, opt, user_data);
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
