#include "libmget.h"
#include "macros.h"
#include <stdio.h>
#include <strings.h>
#include "debug.h"

typedef enum _url_protocol
{
    UP_HTTP = 0,
    UP_HTTPS,
    UP_FTP,
    UP_INVALID
} url_protocol;

const char* ups[] = {
    "http", "https", "ftp", NULL};

typedef struct _url_info
{
    url_protocol protocol;
    uint32       port;
    char*        host;
    char*        uri;
    char*        bname;
    char*        furl; /* full url.*/
} url_info;

bool parse_url(const char* url, url_info** ui);
void url_info_destroy(url_info** ui);
void url_info_display(url_info* ui);

bool start_request(const char* url, const char* dp, int nc,
                   download_progress_callback* cb)
{
    if (!url || *url == '\0' || !dp || *dp == '\0')
    {
        return false; //@todo: add more checks...
    }

    url_info* ui = NULL;
    if (!parse_url(url, &ui))
    {
        return false;
    }

    url_info_display(ui);

    url_info_destroy(&ui);
    return true;
}

#define DEFAULT_HTTP_PORT         80
#define DEFAULT_HTTPS_PORT        443
#define HTTP_PROTOCOL_IDENTIFIER  "http"
#define HTTPS_PROTOCOL_IDENTIFIER "https"

bool parse_url(const char* url, url_info** ui)
{
    bool bret = false;
    if (!url)
        goto ret;

    int length = strlen(url);
    if (length == 0)
        goto ret;

    url_info* up       = ZALLOC(url_info, (sizeof(url_info)));
    char*     protocol = NULL;
    if (!up || !protocol)
        goto ret;

    *ui      = up;
    up->furl = strdup(url);

    int tmpPort = -1, length1 = -1, length2 = -1;
    int num     = sscanf(url, "%m[^://]://%m[^:/]%n:%d%n",
                         &protocol, &up->host, &length1, &tmpPort, &length2);
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
    goto ret;

free:
    FIF(protocol);
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
    PDEBUG ("Protocol: %02X (%s), port: %d, host: %s, uri: %s, filename: %s\n",
            ui->protocol, ups[ui->protocol], ui->port, ui->host, ui->uri, ui->bname);
}
