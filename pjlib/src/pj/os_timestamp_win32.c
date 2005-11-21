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

