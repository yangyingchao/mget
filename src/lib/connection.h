/** connection.h --- interface of connectionet, used internally by libmget.
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

#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "netutils.h"

typedef struct _connection connection;

typedef uint32(*read_func) (connection *, char *, uint32, void *);
typedef uint32(*write_func) (connection *, char *, uint32, void *);
typedef void (*close_func) (connection *, void *);

typedef struct _connection_impl {
	read_func reader;
	write_func writer;
} cipl;


typedef int (*connection_read_func) (connection *, void *);
typedef int (*connection_write_func) (connection *, void *);
typedef bool (*connection_reschedule_func) (connection *, void *);

#define COF_FAILED       -1 // Connection Operation Failed
#define COF_AGAIN        -2 // No data, try again.
#define COF_FINISHED     -3 // Connection Operation Finished
#define COF_CLOSED        0 // Connection was closed.
#define COF_SUCCESS(X)       ((X)>0)


// TODO: Hide sock, use transport_implementation above...
struct _connection {
	cipl ci;		// returned by connection impl, should not be modified.
	connection_read_func rf;
	connection_write_func wf;
    connection_reschedule_func rsf;
	void *priv;
};

typedef struct _connection_group connection_group;

connection_group *connection_group_create(bool * flag);
void connection_group_destroy(connection_group *);
void connection_add_to_group(connection_group *, connection *);

/*! Processing multiple connectsion.

  @param group group of connections.

  @return 0 if finished successfully, or -1 if failed for some reason, or
          return remained connections if no error occur but processing
          stopped by user.
*/
int connection_perform(connection_group* group);

connection *connection_get(const url_info * ui);
void connection_put(connection * sock);

#ifdef __cplusplus
}
#endif

#endif				/* _MGET_SOCK_H_ */

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
