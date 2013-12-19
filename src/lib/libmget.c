#include "libmget.h"
#include "macros.h"
#include <stdio.h>
#include <strings.h>
#include "debug.h"
#include "mget_http.h"

const char* ups[] = {
    "http", "https", "ftp", NULL};

#define DEFAULT_HTTP_PORT         80
#define DEFAULT_HTTPS_PORT        443
#define HTTP_PROTOCOL_IDENTIFIER  "http"
#define HTTPS_PROTOCOL_IDENTIFIER "https"

bool parse_url(const char* url, url_info** ui)
{
    PDEBUG ("url: %s, ui: %p\n", url, ui);

    bool bret = false;
    if (!url)
        goto ret;

    int length = strlen(url);
    if (length == 0)
        goto ret;

    url_info* up = ZALLOC(url_info, (sizeof(url_info)));
    if (!up)
        goto ret;

    up->furl = strdup(url);

    char*     protocol = NULL;
    int tmpPort = -1, length1 = -1, length2 = -1;
    int num     = sscanf(url, "%m[^://]://%m[^:/]%n:%d%n",
                         &protocol, &up->host, &length1, &tmpPort, &length2);
    PDEBUG ("num: %d\n", num);

    if (num <= 0)
    {
        goto free;
    }

    up->protocol = UP_HTTP;
    if (tmpPort == -1)
    {
        up->port = DEFAULT_HTTP_PORT;
        if (strlen(protocol) > 0)
        {
            if (strcasecmp(protocol, HTTP_PROTOCOL_IDENTIFIER) == 0)
            {
                up->port = DEFAULT_HTTP_PORT;
            }
            else if (strcasecmp(protocol, HTTPS_PROTOCOL_IDENTIFIER) == 0)
            {
                up->port = DEFAULT_HTTPS_PORT;
                up->protocol = UP_HTTPS;
            }
        }

        url += length1;
    }
    else
    {
        up->port  = tmpPort;
        url      += length2;
    }

    up->uri   = strdup(url);
    up->bname = get_basename(url);
    *ui       = up;

    PDEBUG ("up: %p\n", up);
    goto ret;

free:
    FIF(protocol);
    PDEBUG ("up: %p\n", up);
ret:
    return true;
}

void url_info_destroy(url_info** ui)
{
    if (ui && *ui)
    {
        url_info* up = *ui;
        FIF(up->host);
        FIF(up->uri);
        FIF(up->bname);
        FIF(up->furl);
        FIFZ(ui);
    }
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
            ui->protocol, ups[ui->protocol], ui->port, ui->host, ui->uri,
            ui->furl, ui->bname);
}



bool start_request(const char* url, const char* dp, int nc,
                   download_progress_callback cb)
{
    if (!url || *url == '\0' || !dp || *dp == '\0')
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

    PDEBUG ("ui: %p\n", ui);
    url_info_display(ui);

    switch (ui->protocol)
    {
        case UP_HTTP:
        case UP_HTTPS:
        {
            process_http_request(ui, dp, nc, cb);
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
