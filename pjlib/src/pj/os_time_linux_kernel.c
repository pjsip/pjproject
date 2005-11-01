/* $Id$
 *
 */
/* $Log: /pjproject-0.3/pjlib/src/pj/os_time_linux_kernel.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     9/22/05 10:39a Bennylp
 * Created.
 *
 */
#include <pj/os.h>
#include <linux/time.h>

///////////////////////////////////////////////////////////////////////////////

PJ_DEF(pj_status_t) pj_gettimeofday(pj_time_val *tv)
{
    struct timeval tval;
  
    do_gettimeofday(&tval);
    tv->sec = tval.tv_sec;
    tv->msec = tval.tv_usec / 1000;

    return 0;
}

PJ_DEF(pj_status_t) pj_time_decode(const pj_time_val *tv, pj_parsed_time *pt)
{
    pt->year = 2005;
    pt->mon = 8;
    pt->day = 20;
    pt->hour = 16;
    pt->min = 30;
    pt->sec = 30;
    pt->wday = 3;
    pt->yday = 200;
    pt->msec = 777;

    return -1;
}

/**
 * Encode parsed time to time value.
 */
PJ_DEF(pj_status_t) pj_time_encode(const pj_parsed_time *pt, pj_time_val *tv);

/**
 * Convert local time to GMT.
 */
PJ_DEF(pj_status_t) pj_time_local_to_gmt(pj_time_val *tv);

/**
 * Convert GMT to local time.
 */
PJ_DEF(pj_status_t) pj_time_gmt_to_local(pj_time_val *tv);


