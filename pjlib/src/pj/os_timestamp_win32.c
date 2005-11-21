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
#include <pj/errno.h>
#include <windows.h>

#if defined(PJ_TIMESTAMP_USE_RDTSC) && PJ_TIMESTAMP_USE_RDTSC!=0 && \
    defined(PJ_M_I386) && PJ_M_I386 != 0 && \
    defined(_MSC_VER)
/*
 * Use rdtsc to get the OS timestamp.
 */
static LONG CpuMhz;
static pj_int64_t CpuHz;
 
static pj_status_t GetCpuHz(void)
{
    HKEY key;
    LONG rc;
    DWORD size;

    rc = RegOpenKey( HKEY_LOCAL_MACHINE,
		     "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
		     &key);
    if (rc != ERROR_SUCCESS)
	return PJ_RETURN_OS_ERROR(rc);

    size = sizeof(CpuMhz);
    rc = RegQueryValueEx(key, "~MHz", NULL, NULL, (BYTE*)&CpuMhz, &size);
    RegCloseKey(key);

    if (rc != ERROR_SUCCESS) {
	return PJ_RETURN_OS_ERROR(rc);
    }

    CpuHz = CpuMhz;
    CpuHz = CpuHz * 1000000;

    return PJ_SUCCESS;
}

/* __int64 is nicely returned in EDX:EAX */
__declspec(naked) __int64 rdtsc() 
{
    __asm 
    {
	RDTSC
	RET
    }
}

PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    ts->u64 = rdtsc();
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    pj_status_t status;

    if (CpuHz == 0) {
	status = GetCpuHz();
	if (status != PJ_SUCCESS)
	    return status;
    }

    freq->u64 = CpuHz;
    return PJ_SUCCESS;
}

#else
/*
 * Use QueryPerformanceCounter and QueryPerformanceFrequency.
 */
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

#endif	/* PJ_TIMESTAMP_USE_RDTSC */

