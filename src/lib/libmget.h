#ifndef _LIBMGET_H_
#define _LIBMGET_H_

#include "metadata.h"

typedef void (*download_progress_callback)(metadata* md);


bool start_request(const char* url, const char* dp, int nc,
                   download_progress_callback cb);

#endif /* _LIBMGET_H_ */
