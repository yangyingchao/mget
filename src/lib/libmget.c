#include "libmget.h"
#include "macros.h"
#include <stdio.h>
#include <strings.h>
#include "debug.h"
#include "cmget_http.h"
#ifdef TRY_MSOCK
#include "mget_http.h"
#endif

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

