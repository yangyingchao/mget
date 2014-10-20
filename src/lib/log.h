
/* Declarations for log.c.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
   2007, 2008, 2009, 2010, 2011 Free Software Foundation, Inc.

   This file is part of GNU Wget.

   GNU Wget is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   GNU Wget is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Wget.  If not, see <http://www.gnu.org/licenses/>.

   Additional permission under GNU GPL version 3 section 7

   If you modify this program, or any covered work, by linking or
   combining it with the OpenSSL project's OpenSSL library (or a
   modified version of that library), containing parts covered by the
   terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
   grants you additional permission to convey the resulting work.
   Corresponding Source for a non-source form of such a combination
   shall include the source code for the parts of OpenSSL used as well
   as that of the covered work.  */

#ifndef LOG_H
#define LOG_H

/* The log file to which Wget writes to after HUP.  */
#define DEFAULT_LOGFILE "mget-log"

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "libmget.h"

void log_set_warc_log_fp(FILE *);

void logprintf(enum log_options, const char *, ...);
void debug_logprintf(const char *, ...);
void logputs(enum log_options, const char *);
void logflush(void);
void log_set_flush(bool);
bool log_set_save_context(bool);

void log_init(const char *, bool);
void log_close(void);
void log_cleanup(void);
void log_request_redirect_output(const char *);

void dump_buffer(const char* tip, const unsigned char* buf, int max);

const char *escnonprint(const char *);
const char *escnonprint_uri(const char *);

#if !defined (PDEBUG)
#define PDEBUG(fmt, ...)                                \
    do {                                                \
        char* msg = NULL;                               \
        asprintf(&msg, "mget: - %s(%d)-%s: %s",         \
                 __FILE__, __LINE__,__FUNCTION__, fmt); \
        logprintf(LOG_DEBUG, msg, ##  __VA_ARGS__);     \
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

#define DEBUGP(X)       PDEBUG X;


#define _(X)       (X)

#endif				/* LOG_H */


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
