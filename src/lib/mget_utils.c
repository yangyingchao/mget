/** timeutil.c --- implementation of time utility.
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

#include "mget_utils.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

uint32 get_time_ms()
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) == 1) {
        return 0;
    }

    return (uint32) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

uint32 get_time_s()
{
    return (uint32) time(NULL);
}

#define MINUTE     (60)
#define HOUR       (MINUTE*60)
#define DAY       (HOUR*24)

char *stringify_time(uint64 ts)
{
    static char str_time[64] = { '\0' };
    memset(str_time, 0, 64);

    if (ts < MINUTE) {
        sprintf(str_time, "%llu seconds", ts);
    } else if (ts < HOUR) {
        sprintf(str_time, "%.01f minutes", (double) ts / MINUTE);
    } else if (ts < DAY) {
        sprintf(str_time, "%.01f hours", (double) ts / HOUR);
    } else {
        sprintf(str_time, "%.01f days", (double) ts / DAY);
    }
    return str_time;
}


char *datetime_str(time_t t)
{
    static char output[32];
    struct tm *tm = localtime(&t);

    if (!tm)
        abort();
    if (!strftime(output, sizeof(output), "%Y-%m-%d %H:%M:%S", tm))
        abort();
    return strdup(output);
}

char *current_time_str()
{
    return datetime_str(time(NULL));
}


int integer_size(const char* size)
{
    return 1024*4;
}

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
