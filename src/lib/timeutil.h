
/** timeutil.h --- utility related to time.
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

#ifndef _TIMEUTIL_H_
#define _TIMEUTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "typedefs.h"

    uint32 get_time_ms();
    uint32 get_time_s();

    char *stringify_time(uint64 ts);

    char *current_time_str();

#ifdef __cplusplus
}
#endif
#endif				/* _TIMEUTIL_H_ */
