
/** libmget.h --- interface of libmget.
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

#ifndef _LIBMGET_H_
#define _LIBMGET_H_

#include "mget_metadata.h"
#include "mget_utils.h"
#include "mget_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _file_name {
    char *dirn;
    char *basen;
} file_name;

// dp stands for download_progress
typedef void (*dp_callback) (metadata * md, void* user_data);

/**
 * @name start_request - start processing request.
 * @param url - Character url to be retrieved.
 * @param fn -  structer to specify where to save download data.
 * @param nc - Number of connections.
 * @param cb -  callback to report downloading progress.
 * @param stop_flag - Flag to control when to stop.
 * @return bool
 */
bool start_request(const char *url, const file_name* fn, int nc,
                   dp_callback cb, bool * stop_flag, void* user_data);

#ifdef __cplusplus
}
#endif

#endif

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
