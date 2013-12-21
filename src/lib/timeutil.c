#include "timeutil.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
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

#define MINUTE     (60)
#define HOUR       (MINUTE*60)
#define DAY       (HOUR*24)

char* stringify_time(uint64 ts)
{
    static char str_time[64] = {'\0'};
    memset(str_time, 0, 64);

    if (ts < MINUTE)
    {
        sprintf(str_time, "%llu seconds", ts);
    }
    else if (ts < HOUR)
    {
        sprintf(str_time, "%.01f minutes", (double)ts/MINUTE);
    }
    else if (ts < DAY)
    {
        sprintf(str_time, "%.01f hours", (double)ts/HOUR);
    }
    else
    {
        sprintf(str_time, "%.01f days", (double)ts/DAY);
    }
    return str_time;
}
