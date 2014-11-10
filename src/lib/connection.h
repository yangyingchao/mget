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

/* Predefined return values for read_func/write_func.
 *
 * COF stands for Connection Operation Failure
 *
 */

#define COF_FINISHED   -1               // Connection Operation Finished
#define COF_EXIT       -2               // Connection Operation Finished
#define COF_FAILED     -3               // Connection Operation Failed
#define COF_INVALID    -4               // Connection Operation Failed
#define COF_AGAIN      -5               // No data, try again.
#define COF_ABORT      -6               // Connection Operation Finished
#define COF_CLOSED     0                // Connection was closed.
#define COF_SUCCESS(X) ((X)>0)

/* Connection operation functions, it returns number of bytes it operated, or
 * COF_XXX to indicate detailed errors.
 */

typedef struct _connection_operations {
    int32(*read) (connection*, char*, uint32, void*);
    int32(*write) (connection*, char*, uint32, void*);
    void (*close) (connection*, void*);
} connection_operations;


// TODO: Hide sock, use transport_implementation above...
struct _connection {
	connection_operations co; // read only, returned by connection
                              // implementation, recv_data & write_data
                              // functions should use co.read()/co.write() to
                              // read from or write data into connection.
    int (*recv_data) (connection*, void*);
    int (*write_data) (connection*, void*);
    bool (*connection_reschedule_func) (connection*, void*);
	void *priv;
};

typedef struct _connection_group connection_group;

connection_group *connection_group_create(bool* flag);
void connection_group_destroy(connection_group*);
void connection_add_to_group(connection_group*, connection*);

/*! Processing multiple connectsion.

  @param group group of connections.

  @return 0 if finished successfully, or -1 if failed for some reason, or
          return remained connections if no error occur but processing
          stopped by user.
*/
int connection_perform(connection_group* group);

connection *connection_get(const url_info* ui);
void connection_put(connection* sock);

/** Set global bandwith limit, unit: bps.
 */
void set_global_bandwidth(int);


typedef enum _wait_type
{
    WT_NONE  = 0,
    WT_READ  = 1,
    WT_WRITE = 1 << 1,
} wait_type;

/**
 * @name timed_wait - Waits a period and see if socket is readable/writable..
 * @param sock - socket to be checked
 * @param type - Type of wait.
 * @param delay - Number of delay in seconds, set to -1 means infinite.
 * @return true if ok.
 */
bool timed_wait(int sock, int type, int delay);

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
