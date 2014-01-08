/** ssl.h --- secure socket.
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

#ifndef _SSL_H_
#define _SSL_H_

// #ifdef __cplusplus
// extern "C" {
// #endif

#include "typedefs.h"

bool ssl_init();

void*  make_socket_secure(int sk);
uint32 secure_socket_read(int sk, char* buf,
                          uint32 size, void *priv);

uint32 secure_socket_write(int sk, char* buf,
                           uint32 size, void *priv);

// #ifdef __cplusplus
// }
// #endif

#endif /* _SSL_H_ */
