/** mget_sock.h --- interface of mget_socket, used internally by libmget.
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

#ifndef _MGET_SOCK_H_
#define _MGET_SOCK_H_

// #ifdef __cplusplus
// extern "C" {
// #endif

#include "netutils.h"

typedef struct _msock msock;

typedef int (*sock_read_func)(int, void*);
typedef int (*sock_write_func)(int, void*);

// TODO: Hide sock and function pointers..
struct _msock
{
    int             sock;
    sock_read_func  rf;
    sock_write_func wf;
    void*           priv;
};

typedef struct _sock_group sock_group;

sock_group* sock_group_create(bool* flag);
void sock_group_destroy(sock_group*);
void sock_add_to_group(sock_group*, msock*);
int  socket_perform(sock_group* sock);

msock* socket_get(url_info* ui);
void socket_put(msock* sock);


// #ifdef __cplusplus
// }
// #endif

#endif /* _MGET_SOCK_H_ */
