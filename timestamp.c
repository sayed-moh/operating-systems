#include <stdio.h>
#include <time.h>
#include "timestamp.h"

void get_time_stamp(char *buf)
{
    static time_t timer;
    static struct tm *timeinfo;
    
    timer = time(NULL);
    timeinfo = localtime(&timer);
    strftime(buf, 26, "%Y-%m-%d %H:%M:%S", timeinfo);

    buf[26] = '0';

}