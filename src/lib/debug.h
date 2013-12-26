/** debug.h --- utility for debugging.
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

#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef DEBUG

#define PDEBUG(fmt, args...)                                            \
    do {                                                                \
        const char* file = __FILE__;                                    \
        const char* ptr = file;                                         \
        const char* sep = "/";                                          \
        while ((ptr = strstr(file, sep)) != NULL) {                     \
            file = ptr;                                                 \
            file = ++ptr;                                               \
        }                                                               \
                                                                        \
        fprintf(stderr, "TDEBUG: - %s(%d)-%s: ",file, __LINE__,__FUNCTION__); \
        fprintf(stderr, fmt, ##args);                                   \
    } while(0)
#else
#define PDEBUG(fmt, args...)
#endif /* _DEBUG_H_ */

#endif
