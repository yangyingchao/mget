#ifndef _MGET_HTTP_H_
#define _MGET_HTTP_H_

#include "netutils.h"
#include "metadata.h"
void process_http_request(url_info* ui, const char* dp, int nc,
                          void (*cb)(metadata* md));

#endif /* _MGET_HTTP_H_ */
