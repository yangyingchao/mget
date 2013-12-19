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
    url_protocol protocol;
    uint32       port;
    char*        host;
    char*        uri;
    char*        bname;
    char*        furl; /* full url.*/
} url_info;

#ifdef __cplusplus
}
#endif

#endif /* _NETUTILS_H_ */
