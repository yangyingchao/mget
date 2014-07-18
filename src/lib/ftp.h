/** ftp.h --- interface of libmget to download data from ftp server.
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

#ifndef _FTP_H_
#define _FTP_H_

#include "libmget.h"
#include "download_info.h"

/**
 * @name process_ftp_request - Begin process ftp request.
 * @param ui -  url information, should not be NULL.
 * @param info -  information about metadata and file.
 * @param cb - Callback function to notify progress.
 * @param stop_flag - A flag used by http handler to check if need to stop.
 * @return mget_err
 */
mget_err process_ftp_request(dinfo* info,
                             dp_callback cb,
                             bool* stop_flag,
                             void* user_data);

#endif /* _FTP_H_ */
