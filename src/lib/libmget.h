#ifndef _LIBMGET_H_
#define _LIBMGET_H_

#include "metadata.h"
#include "netutils.h"

typedef void (*download_progress_callback)(metadata* md);


bool start_request(const char* url, const file_name* fn, int nc,
                   download_progress_callback cb,
                   bool* stop_flag);

#endif /* _LIBMGET_H_ */
