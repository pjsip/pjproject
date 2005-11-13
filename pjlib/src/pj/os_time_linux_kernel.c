/* $Id$
 *
 */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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


