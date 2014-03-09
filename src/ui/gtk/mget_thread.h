#ifndef _MGET_THREAD_H_
#define _MGET_THREAD_H_

#include <gtk/gtk.h>

#include <libmget/libmget.h>

typedef struct _mget_request
{
    const char* url;
    file_name   fn;
    bool        flag;
} mget_request;

typedef struct _gmet_request
{
    GtkTreeIter  iter;
    mget_request request;
} gmet_request;


#endif /* _MGET_THREAD_H_ */
