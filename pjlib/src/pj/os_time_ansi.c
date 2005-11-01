/* $Header: /pjproject-0.3/pjlib/src/pj/os_time_ansi.c 2     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/os_time_ansi.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 *
 */
#include <pj/os.h>
#include <pj/compat/time.h>

///////////////////////////////////////////////////////////////////////////////

PJ_DEF(pj_status_t) pj_gettimeofday(pj_time_val *tv)
{
    struct timeb tb;

    PJ_CHECK_STACK();

    ftime(&tb);
    tv->sec = tb.time;
    tv->msec = tb.millitm;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_time_decode(const pj_time_val *tv, pj_parsed_time *pt)
{
    struct tm *local_time;

    PJ_CHECK_STACK();

    local_time = localtime((time_t*)&tv->sec);

    pt->year = local_time->tm_year+1900;
    pt->mon = local_time->tm_mon;
    pt->day = local_time->tm_mday;
    pt->hour = local_time->tm_hour;
    pt->min = local_time->tm_min;
    pt->sec = local_time->tm_sec;
    pt->wday = local_time->tm_wday;
    pt->yday = local_time->tm_yday;
    pt->msec = tv->msec;

    return PJ_SUCCESS;
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


