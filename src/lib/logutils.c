/** logutils.c --- implementation of log.
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

#include "logutils.h"
#include <stdarg.h>
#include <string.h>

log_level g_log_level = LL_NONVERBOSE;

void mlog(log_level o, const char *fmt, ...)
{
    if (o >= g_log_level)
    {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}

void dump_line(const unsigned char* buf, int w, int l, char* out)
{
#define YYYGET(X)       ( X >= 32 && X <= 126) ? X : '.'
    unsigned int i = 0;
    sprintf(out, "%08x: ", l);
    out += 10;
    for (; i < w; ++i)
    {
        sprintf(out, (i % 8 == 7) ? "%02x  " : "%02x ", *(buf+i));
        out += (i % 8 == 7) ? 4 : 3;
    }

    if (w < 0x10)
    {
        for (i = 0; i < 0x10 - w; ++i)
        {
            sprintf(out, "   ");
            out += 3;
        }
        sprintf(out, "  ");
        out+= 2;
    }
    sprintf (out++, "|");
    for (i = 0; i < w; ++i)  sprintf (out++, "%c", YYYGET(*(buf+i)));

    if (w < 0x10)
        for (i = 0; i < 0x10 - w; ++i) sprintf(out++, " ");
    sprintf (out, "|\n");
#undef YYYGET
}

void dump_buffer(const char* tip, const unsigned char* buf, int max)
{
    int l = max / 0x10 + ((max % 0x10) ? 1 : 0);
    int i = 0;
    int w = l - i > 1 ? 0x10 : max;
    const unsigned char* ptr = buf;
    char out[78];
    mlog(LL_DEBUG, tip);
    for (; i < l; ++i,w = l - i > 1 ? 0x10 : max - 0x10 * i)
    {
        memset(out, 0, 78);
        dump_line(ptr, w, i, out);
        mlog(LL_DEBUG, "\t%s", out);
        ptr += w;
    }
}

