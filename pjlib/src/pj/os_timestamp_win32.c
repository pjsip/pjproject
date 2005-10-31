/* $Header: /pjproject-0.3/pjlib/src/pj/os_timestamp_win32.c 2     10/14/05 12:26a Bennylp $ */
/* 
 * $Log: /pjproject-0.3/pjlib/src/pj/os_timestamp_win32.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     9/18/05 8:15p Bennylp
 * Created.
 * 
 */
#include <pj/os.h>
#include <pj/errno.h>
#include <windows.h>

PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    LARGE_INTEGER val;

    if (!QueryPerformanceCounter(&val))
	return PJ_RETURN_OS_ERROR(GetLastError());

    ts->u64 = val.QuadPart;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    LARGE_INTEGER val;

    if (!QueryPerformanceFrequency(&val))
	return PJ_RETURN_OS_ERROR(GetLastError());

    freq->u64 = val.QuadPart;
    return PJ_SUCCESS;
}

