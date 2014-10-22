/** logutils.h --- utilities for logs...
 *
 * Copyright (C) 2014 Yang,Ying-chao
 *
 * Author: Yang,Ying-chao <yangyingchao@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 */
#ifndef _LOGUTILS_H_
#define _LOGUTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include "libmget.h"
#include <assert.h>

void mlog(log_level, const char *, ...);
void dump_buffer(const char* tip, const unsigned char* buf, int max);

#if !defined (PDEBUG)
#define PDEBUG(fmt, ...)                                \
    do {                                                \
        char* msg = NULL;                               \
        asprintf(&msg, "mget: - %s(%d)-%s: %s",         \
                 __FILE__, __LINE__,__FUNCTION__, fmt); \
        mlog(LL_DEBUG, msg, ##  __VA_ARGS__);     \
        free(msg);                                      \
    } while(0)
#endif  /*End of if PDEBUG*/

#if !defined(OUT_BIN)
#define OUT_BIN(X, Y)                               \
    do {                                            \
        char* msg = NULL;                           \
        asprintf(&msg,  "mget: - %s(%d)-%s:",       \
                 __FILE__, __LINE__,__FUNCTION__);  \
        dump_buffer(msg, (unsigned char*)X, Y);     \
        free(msg);                                  \
    } while (0)
#endif // OUT_BIN


#ifdef __cplusplus
}
#endif

#endif /* _LOGUTILS_H_ */
