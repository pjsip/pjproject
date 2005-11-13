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

