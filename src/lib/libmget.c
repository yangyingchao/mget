
/** libmget.c --- implementation of libmget.
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

#include "libmget.h"
#include "macros.h"
#include <stdio.h>
#include <strings.h>
#include "debug.h"
#include "http.h"

typedef void (*request_processor) (url_info * ui, const char *dn, int nc,
				   void (*cb) (metadata * md),
				   bool * stop_flag);

bool start_request(const char *url, const file_name * fn, int nc,
		   download_progress_callback cb, bool * stop_flag)
{
    if (!url || *url == '\0' || !fn) {
	PDEBUG("Invalid args...\n");
	return false;		//@todo: add more checks...
    }

    url_info *ui = NULL;

    if (!parse_url(url, &ui)) {
	printf("Failed to parse URL: %s\n", url);
	return false;
    }

    char *fpath = NULL;

    if (!get_full_path(fn, &fpath)) {
	char *tmp = ZALLOC(char, strlen(fpath) + strlen(ui->bname) + 2);

	sprintf(tmp, "%s/%s", fpath, ui->bname);
	FIF(fpath);
	fpath = tmp;
    }

    PDEBUG("ui: %p\n", ui);
    url_info_display(ui);
    if (!ui) {
	fprintf(stderr, "Failed to parse URL: %s\n", url);
	return false;
    }
    switch (ui->eprotocol) {
    case UP_HTTP:
    case UP_HTTPS:
	{
	    int ret = process_http_request(ui, fpath, nc, cb, stop_flag);

	    if (ret == 0) {
		break;
	    }
	}
    default:
	{
	    break;
	}
    }

    url_info_destroy(&ui);
    return true;
}
