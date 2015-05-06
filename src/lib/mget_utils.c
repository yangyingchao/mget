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
#include "mget_macros.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <stdlib.h>

int get_time_ms()
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) == 1) {
        return 0;
    }

    return (int) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int get_time_s()
{
    return (int) time(NULL);
}

#define MINUTE     (60)
#define HOUR       (MINUTE*60)
#define DAY       (HOUR*24)

char *stringify_time(uint64 ts)
{
    static char str_time[64] = { '\0' };
    memset(str_time, 0, 64);

    if (ts < MINUTE) {
        sprintf(str_time, "%" PRIu64 " seconds", ts);
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


int integer_size(const char *size)
{
    return 1024 * 512;
}


struct _progress {
    size_t   capacity;
    size_t   offset;
    char  pbuf[256];
    char* buf;
    char* marker;
};

static char MARKERS[] = {'-', '\\', '|', '/'};

progress* progress_create(int capacity, char* head, char* tail)
{
    progress* p     = ZALLOC1(progress);
    size_t    total = capacity + 3;  // 1: ' ', 2: MARK, 3: ' '
    p->capacity     = capacity;

    if (head) {
        p->offset = strlen(head) + 1; // including trailing '\0'
        total += p->offset;
    }

    if (tail)
        total += strlen(tail);

    total += 1;  // for trailing '\0'

    p->buf    = ZALLOC(char, total);
    p->marker = p->buf + total - 3;
    memset(p->buf, ' ', total);
    *p->marker = MARKERS[0];
    int i      = 0;
    size_t offset = capacity+p->offset;

    p->buf[0] = '\r';
    if (head) {
        while (*(head + i)) {
            p->buf[i+1] = *(head + i);
            ++i;
        }
    }

    if (tail) {
        i = 0;
        while (*(tail + i)) {
            p->buf[offset+i] = *(tail + i);
            ++i;
        }
    }

    p->buf[total-1] = '\0';
    return p;
}

void progress_report(progress* p, int pos, bool details,
                     double per, double spd, double eta)
{
    if (!p || pos >= p->capacity)
        return;

    p->buf[p->offset+pos] = '.';
    *p->marker = MARKERS[pos%4];
    fputs(p->buf, stdout);

    if (details) {
        memset(p->pbuf, 0, 256);
        sprintf(p->pbuf, " %.02f%%, SPD: %s/s, ETA: %s\r",
                per, stringify_size(spd), stringify_time(eta));
        fputs(p->pbuf, stdout);
        memset(p->buf+p->offset, ' ', p->capacity);
    }
    fflush(stdout);
    return;
}
void progress_destroy(progress* p)
{
    if (!p)
        return;
    FIF(p->buf);
    FIF(p);
}


char* format_string(const char* fmt, ...)
{
    char* result = NULL;
    va_list ap;
    va_start(ap, fmt);
    vasprintf(&result, fmt, ap);
    va_end(ap);
    return result;
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
