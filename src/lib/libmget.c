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
    url_info *ui    = NULL;
    char     *fpath = NULL;
    metadata_wrapper  mw;

retry:
    memset(&mw, 0, sizeof(mw));

    if (url && !!parse_url(url, &ui)) {
        fprintf(stderr, "Failed to parse given url: %s\n", url);
        return false;
    }

    if (!get_full_path(fn, &fpath)) {
        if (!ui || !ui->bname) {
            fprintf(stderr, "Failed parse url(%s) and output directory\n",
                    url);
            return false;
        }

        char *tmp = ZALLOC(char, strlen(fpath) + strlen(ui->bname) + 2);
        sprintf(tmp, "%s/%s", fpath, ui->bname);
        FIF(fpath);
        fpath = tmp;
    }

    if (!fpath) {
        fprintf(stderr, "Failed to guess local file name!\n");
        return false;
    }

    char tfn[256] = { '\0' };
    sprintf(tfn, "%s.tmd", fpath);
    if (file_existp(tfn) && metadata_create_from_file(tfn, &mw)) {
        PDEBUG("metadta created from file: %s\n", tfn);

        // Destroy url info and recreate using url stored in mw.
        url_info_destroy(&ui);
        if (!parse_url(mw.md->url, &ui)) {
            fprintf(stderr, "Failed to parse stored url: %s.\n", mw.md->url);
            if (url) {
                fprintf(stderr, "Removing old metadata and retring...\n");
                url_info_destroy(&ui);
                goto retry;
            }
            fprintf(stderr, "Abort: no url provided...\n");
            return false;
        }
    }
    else {
        fhandle *fh = fhandle_create(tfn, FHM_CREATE);

        mw.fm = fhandle_mmap(fh, 0, MD_SIZE(mw.md));
        mw.from_file = false;
        memset(mw.fm->addr, 0, MD_SIZE(mw.md));
    }

    url_info_display(ui);
    if (!ui) {
        fprintf(stderr, "Failed to parse URL: %s\n", url);
        return false;
    }
    switch (ui->eprotocol) {
        case UP_HTTP:
        case UP_HTTPS:
        {
            int ret = process_http_request(ui, &mw, nc, cb, stop_flag);

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
