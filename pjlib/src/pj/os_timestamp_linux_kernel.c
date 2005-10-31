/* $Header: /pjproject-0.3/pjlib/src/pj/os_timestamp_linux_kernel.c 2     10/29/05 11:51a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/os_timestamp_linux_kernel.c $
 * 
 * 2     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 1     9/22/05 10:39a Bennylp
 * Created.
 * 
 */
#include <pj/os.h>
#include <linux/time.h>

#if 0
PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    ts->u32.hi = 0;
    ts->u32.lo = jiffies;
    return 0;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = HZ;
    return 0;
}
#elif 0
PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    struct timespec tv;
    
    tv = CURRENT_TIME;

    ts->u64 = tv.tv_sec;
    ts->u64 *= NSEC_PER_SEC;
    ts->u64 += tv.tv_nsec;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = NSEC_PER_SEC;
    return 0;
}
#else
PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    struct timeval tv;
    
    do_gettimeofday(&tv);

    ts->u64 = tv.tv_sec;
    ts->u64 *= USEC_PER_SEC;
    ts->u64 += tv.tv_usec;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = USEC_PER_SEC;
    return 0;
}

#endif

