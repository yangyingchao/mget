
/** netutils.c --- implementation of net related utility.
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

#include "netutils.h"
#include <stdio.h>
#include "mget_macros.h"
#include "log.h"
#include "fileutils.h"

#define DEFAULT_HTTP_PORT         80
#define DEFAULT_HTTPS_PORT        443
#define DEFAULT_FTP_PORT          21
#define PROTOCOL_IDENTIFIER_HTTP  "http"
#define PROTOCOL_IDENTIFIER_HTTPS "https"
#define PROTOCOL_IDENTIFIER_FTP   "ftp"


void url_info_destroy(url_info ** ui)
{
    if (ui && *ui) {
        url_info *up = *ui;

        FIF(up->furl);
    }
}

bool parse_url(const char *url, url_info ** ui)
{
    PDEBUG("url: %s, ui: %p\n", url, ui);

    bool bret = false;

    if (!url)
        goto ret;

    size_t length = strlen(url);

    if (length == 0)
        goto ret;

    url_info *up = ZALLOC1(url_info);

    if (!up)
        goto ret;

    up->furl = strdup(url);

    int length1 = -1, length2 = -1;
    int num = sscanf(url, "%[^://]://%[^:/]%n:%lu%n",
                     up->protocol, up->host, &length1, &up->port,
                     &length2);
    if (num <= 0) {
        goto free;
    }

    if (!up->port) {
        url += length1;
    } else {
        url += length2;
    }

    up->uri = strdup(url);
    up->bname = get_basename(url);
    up->eprotocol = UP_INVALID;

    if (strlen(up->protocol) > 0) {
        if (strcasecmp(up->protocol, PROTOCOL_IDENTIFIER_HTTP) == 0) {
            up->eprotocol = UP_HTTP;
            if (!up->port) {
                up->port = DEFAULT_HTTP_PORT;
            }
        } else if (strcasecmp(up->protocol, PROTOCOL_IDENTIFIER_HTTPS) ==
                   0) {
            up->eprotocol = UP_HTTPS;
            if (!up->port) {
                up->port = DEFAULT_HTTPS_PORT;
            }
        } else if (strcasecmp(up->protocol, PROTOCOL_IDENTIFIER_FTP) == 0) {
            up->eprotocol = UP_FTP;
            if (!up->port) {
                up->port = DEFAULT_FTP_PORT;
            }
        }
    }

    sprintf(up->sport, "%lu", up->port);
    *ui = up;
    bret = true;
    goto ret;

free:
    if (up) {
        url_info_destroy(&up);
    }
ret:
    return bret;
}


void url_info_display(url_info * ui)
{
    printf("ui: %p, url: %s\n", ui, ui ? ui->furl : NULL);
    if (!ui) {
        printf("empty url_info...\n");
        return;
    }
    printf ("Protocol: %02X (%s), port: %lu, host: %s, uri: %s,\nurl: %s,filename: %s\n",
            ui->eprotocol, ui->protocol, ui->port, ui->host, ui->uri,
            ui->furl, ui->bname);
}


void url_info_copy(url_info * u1, url_info * u2)
{
    if (u1 && u2) {
        FIF(u1->uri);
        FIF(u1->bname);
        FIF(u1->furl);

        sprintf(u1->protocol, "%s", u2->protocol);
        sprintf(u1->host, "%s", u2->host);
        sprintf(u1->sport, "%s", u2->sport);
        u1->uri = strdup(u2->uri);
        u1->bname = strdup(u2->bname);
        u1->furl = strdup(u2->furl);
    }
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
