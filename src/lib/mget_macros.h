
/** mget_macros.h --- mget_macros...
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

#ifndef _MACROS_H_
#define _MACROS_H_

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>

#define ZALLOC(T, N)       (T*)calloc(N, sizeof(T))
#define ZALLOC1(T) ZALLOC(T, 1)
#define FIF(X)  if((X)) free((X))
#define FIFZ(X) if(*X) {free(*(X)), *(X) = NULL;}

#define XZERO(X)       memset(&(X), '\0', sizeof(X));

#define COUNTOF(array) (sizeof (array) / sizeof ((array)[0]))
#define XREALLOC(X,Y)       realloc((X), (Y))

#define xfree(X) FIF(X)
#define xrealloc(X, Y)       XREALLOC(X, Y)
#define XALLOC(N)       ZALLOC(char, N)


#endif				/* _MACROS_H_ */

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
