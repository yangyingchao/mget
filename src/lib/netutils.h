#ifndef _NETUTILS_H_
#define _NETUTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "typedefs.h"

typedef enum _url_protocol
{
    UP_HTTP = 0,
    UP_HTTPS,
    UP_FTP,
    UP_INVALID
} url_protocol;

typedef struct _url_info
{
    url_protocol eprotocol;
    uint32       port;
    char*        protocol;
    char*        host;
    char*        sport;
    char*        uri;
    char*        bname;
    char*        furl;                  /* full url.*/
} url_info;

bool parse_url(const char* url, url_info** ui);
void url_info_copy(url_info*,url_info*);
void url_info_display(url_info*);
void url_info_destroy(url_info** ui);

#ifdef __cplusplus
}
#endif

#endif /* _NETUTILS_H_ */
