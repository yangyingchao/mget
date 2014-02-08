/** dinfo.h --- information of downloading.
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

#ifndef _DOWNLOAD_INFO_H_
#define _DOWNLOAD_INFO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "mget_metadata.h"
#include "netutils.h"
#include "fileutils.h"

typedef struct _dinfo
{
    url_info* ui;
    metadata* md;
    fh_map*   fm_md;
    fh_map*   fm_file;
} dinfo;

bool dinfo_create(const char *url, const file_name* fn, int nc, dinfo** info);
void dinfo_destroy(dinfo** info);
bool dinfo_ready(dinfo* info);

bool dinfo_update_metadata(uint64 size, dinfo* info);

void dinfo_sync(dinfo* info);

#ifdef __cplusplus
}
#endif

#endif /* _DOWNLOAD_INFO_H_ */

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
