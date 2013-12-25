#ifndef _CMGET_HTTP_H_
#define _CMGET_HTTP_H_

#include "netutils.h"
#include "metadata.h"
void process_http_request_c(url_info* ui, const char* dp, int nc,
                            void (*cb)(metadata* md), bool* stop_flag);

#endif /* _CMGET_HTTP_H_ */
