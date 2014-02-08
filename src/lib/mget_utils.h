/** mget_utils.h --- utilities provided by mget.
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

#ifndef _MGET_UTILS_H_
#define _MGET_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "mget_types.h"

#define K       (1 << 10)
#define M       (1 << 20)
#define G       (1 << 30)
#define T       (1 << 40)

// user should copy this tring after it returns!
const char *stringify_size(uint64 sz);
bool file_existp(const char *fn);

uint32 get_time_ms();
uint32 get_time_s();
char *stringify_time(uint64 ts);
char *current_time_str();

#ifdef __cplusplus
}
#endif

#endif /* _MGET_UTILS_H_ */

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
