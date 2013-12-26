#include "libmget.h"
#include "macros.h"
#include <stdio.h>
#include <strings.h>
#include "debug.h"
#include "cmget_http.h"
#ifdef TRY_MSOCK
#include "mget_http.h"
#endif
#define DEFAULT_HTTP_PORT         80
#define DEFAULT_HTTPS_PORT        443
#define DEFAULT_FTP_PORT          21
#define PROTOCOL_IDENTIFIER_HTTP  "http"
#define PROTOCOL_IDENTIFIER_HTTPS "https"
#define PROTOCOL_IDENTIFIER_FTP   "ftp"

void url_info_destroy(url_info** ui)
{
    if (ui && *ui)
    {
        url_info* up = *ui;
        FIF(up->protocol);
        FIF(up->host);
        FIF(up->uri);
        FIF(up->bname);
        FIF(up->furl);
        FIFZ(ui);
    }
}

bool parse_url(const char* url, url_info** ui)
{
    PDEBUG ("url: %s, ui: %p\n", url, ui);

    bool bret = false;
    if (!url)
        goto ret;

    int length = strlen(url);
    if (length == 0)
        goto ret;

    url_info* up = ZALLOC1(url_info);
    if (!up)
        goto ret;

    up->furl = strdup(url);

    int length1 = -1, length2 = -1;
    int num     = sscanf(url, "%m[^://]://%m[^:/]%n:%d%n",
                         &up->protocol, &up->host, &length1, &up->port, &length2);
    if (num <= 0)
    {
        goto free;
    }

    if (!up->port)
    {
        url += length1;
    }
    else
    {
        url += length2;
    }

    up->uri       = strdup(url);
    up->bname     = get_basename(url);
    up->eprotocol = UP_INVALID;

    if (strlen(up->protocol) > 0)
    {
        if (strcasecmp(up->protocol, PROTOCOL_IDENTIFIER_HTTP) == 0)
        {
            up->eprotocol = UP_HTTP;
            if (!up->port)
            {
                up->port = DEFAULT_HTTP_PORT;
            }
        }
        else if (strcasecmp(up->protocol, PROTOCOL_IDENTIFIER_HTTPS) == 0)
        {
            up->eprotocol = UP_HTTPS;
            if (!up->port)
            {
                up->port = DEFAULT_HTTPS_PORT;
            }
        }
        else if (strcasecmp(up->protocol, PROTOCOL_IDENTIFIER_FTP) == 0)
        {
            up->eprotocol = UP_FTP;
            if (!up->port)
            {
                up->port = DEFAULT_FTP_PORT;
            }
        }
    }

    char tmp[64] = {'\0'};
    sprintf(tmp, "%d", up->port);
    up->sport = strdup(tmp);
    *ui       = up;
    goto ret;

free:
    if (up)
    {
        url_info_destroy(&up);
    }
ret:
    return true;
}


void url_info_display(url_info* ui)
{
    PDEBUG ("ui: %p, url: %s\n", ui, ui ? ui->furl : NULL);
    if (!ui)
    {
        PDEBUG ("empty url_info...\n");
        return;
    }
    PDEBUG ("Protocol: %02X (%s), port: %d, host: %s, uri: %s,\nurl: %s,filename: %s\n",
            ui->eprotocol, ui->protocol, ui->port, ui->host, ui->uri,
            ui->furl, ui->bname);
}

typedef void (*request_processor)(url_info* ui, const char* dn, int nc,
                                  void (*cb)(metadata* md), bool* stop_flag);

bool start_request(const char* url, const file_name* fn, int nc,
                   download_progress_callback cb, bool* stop_flag)
{
    if (!url || *url == '\0' || !fn)
    {
        PDEBUG ("Invalid args...\n");
        return false; //@todo: add more checks...
    }

    url_info* ui = NULL;
    if (!parse_url(url, &ui))
    {
        PDEBUG ("Failed to parse URL: %s\n", url);
        return false;
    }

    char* fpath = NULL;
    if (!get_full_path(fn, &fpath))
    {
        char* tmp = ZALLOC(char, strlen(fpath) + strlen(ui->bname) + 2);
        sprintf(tmp, "%s/%s", fpath, ui->bname);
        FIF(fpath);
        fpath = tmp;
    }

    PDEBUG ("ui: %p\n", ui);
    url_info_display(ui);
    if (!ui)
    {
        fprintf(stderr, "Failed to parse URL: %s\n", url);
        return false;
    }
    switch (ui->eprotocol)
    {
        case UP_HTTP:
        {
            // If TRY_MSOCK defined, try use raw socket
            // if it is not defined or raw socket failed, use libcurl.
#ifdef TRY_MSOCK
            int ret = process_http_request(ui, fpath, nc, cb, stop_flag);
            if (ret == 0)
            {
                break;
            }
#endif
        }
        case UP_HTTPS:
        {
            process_http_request_c(ui, fpath, nc, cb, stop_flag);
            break;
        }

        default:
        {
            break;
        }
    }

    url_info_destroy(&ui);
    return true;
}
