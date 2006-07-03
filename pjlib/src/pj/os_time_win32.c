/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/os.h>
#include <pj/string.h>
#include <windows.h>

///////////////////////////////////////////////////////////////////////////////

#define SECS_TO_FT_MULT 10000000

static LARGE_INTEGER base_time;

// Find 1st Jan 1970 as a FILETIME 
static void get_base_time(void)
{
    SYSTEMTIME st;
    FILETIME ft;

    memset(&st,0,sizeof(st));
    st.wYear=1970;
    st.wMonth=1;
    st.wDay=1;
    SystemTimeToFileTime(&st, &ft);
    
    base_time.LowPart = ft.dwLowDateTime;
    base_time.HighPart = ft.dwHighDateTime;
    base_time.QuadPart /= SECS_TO_FT_MULT;
}

PJ_DEF(pj_status_t) pj_gettimeofday(pj_time_val *tv)
{
    SYSTEMTIME st;
    FILETIME ft;
    LARGE_INTEGER li;

    if (base_time.QuadPart == 0)
	get_base_time();

    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &ft);

    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    li.QuadPart /= SECS_TO_FT_MULT;
    li.QuadPart -= base_time.QuadPart;

    tv->sec = li.LowPart;
    tv->msec = st.wMilliseconds;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_time_decode(const pj_time_val *tv, pj_parsed_time *pt)
{
    LARGE_INTEGER li;
    FILETIME ft;
    SYSTEMTIME st;

    li.QuadPart = tv->sec;
    li.QuadPart += base_time.QuadPart;
    li.QuadPart *= SECS_TO_FT_MULT;

    ft.dwLowDateTime = li.LowPart;
    ft.dwHighDateTime = li.HighPart;
    FileTimeToSystemTime(&ft, &st);

    pt->year = st.wYear;
    pt->mon = st.wMonth-1;
    pt->day = st.wDay;
    pt->wday = st.wDayOfWeek;

    pt->hour = st.wHour;
    pt->min = st.wMinute;
    pt->sec = st.wSecond;
    pt->msec = tv->msec;

    return PJ_SUCCESS;
}

/**
 * Encode parsed time to time value.
 */
PJ_DEF(pj_status_t) pj_time_encode(const pj_parsed_time *pt, pj_time_val *tv)
{
    SYSTEMTIME st;
    FILETIME ft;
    LARGE_INTEGER li;

    pj_bzero(&st, sizeof(st));
    st.wYear = (pj_uint16_t) pt->year;
    st.wMonth = (pj_uint16_t) (pt->mon + 1);
    st.wDay = (pj_uint16_t) pt->day;
    st.wHour = (pj_uint16_t) pt->hour;
    st.wMinute = (pj_uint16_t) pt->min;
    st.wSecond = (pj_uint16_t) pt->sec;
    st.wMilliseconds = (pj_uint16_t) pt->msec;
    
    SystemTimeToFileTime(&st, &ft);

    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    li.QuadPart /= SECS_TO_FT_MULT;
    li.QuadPart -= base_time.QuadPart;

    tv->sec = li.LowPart;
    tv->msec = st.wMilliseconds;

    return PJ_SUCCESS;
}

/**
 * Convert local time to GMT.
 */
PJ_DEF(pj_status_t) pj_time_local_to_gmt(pj_time_val *tv);

/**
 * Convert GMT to local time.
 */
PJ_DEF(pj_status_t) pj_time_gmt_to_local(pj_time_val *tv);


