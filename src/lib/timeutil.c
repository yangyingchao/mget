#include "timeutil.h"
#include <time.h>
#include <sys/time.h>

uint32 get_time_ms()
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 1)
    {
        return 0;
    }

    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

uint32 get_time_s()
{
    return time(NULL);
}

