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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if defined(PJ_HAS_PENTIUM) && PJ_HAS_PENTIUM!=0
static int machine_speed_mhz;
static pj_timestamp machine_speed;

static __inline__ unsigned long long int rdtsc()
{
    unsigned long long int x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

/* Determine machine's CPU MHz to get the counter's frequency.
 */
static int get_machine_speed_mhz()
{
    FILE *strm;
    char buf[512];
    int len;
    char *pos, *end;
	
    PJ_CHECK_STACK();
	
    /* Open /proc/cpuinfo and read the file */
    strm = fopen("/proc/cpuinfo", "r");
    if (!strm)
        return -1;
    len = fread(buf, 1, sizeof(buf), strm);
    fclose(strm);
    if (len < 1) {
        return -1;
    }
    buf[len] = '\0';

    /* Locate the MHz digit. */
    pos = strstr(buf, "cpu MHz");
    if (!pos)
        return -1;
    pos = strchr(pos, ':');
    if (!pos)
        return -1;
    end = (pos += 2);
    while (isdigit(*end)) ++end;
    *end = '\0';

    /* Return the Mhz part, and give it a +1. */
    return atoi(pos)+1;
}

PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    if (machine_speed_mhz == 0) {
	machine_speed_mhz = get_machine_speed_mhz();
	if (machine_speed_mhz > 0) {
	    machine_speed.u64 = machine_speed_mhz * 1000000.0;
	}
    }
    
    if (machine_speed_mhz == -1) {
	ts->u64 = 0;
	return -1;
    } 
    ts->u64 = rdtsc();
    return 0;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    if (machine_speed_mhz == 0) {
	machine_speed_mhz = get_machine_speed_mhz();
	if (machine_speed_mhz > 0) {
	    machine_speed.u64 = machine_speed_mhz * 1000000.0;
	}
    }
    
    if (machine_speed_mhz == -1) {
	freq->u64 = 1;	/* return 1 to prevent division by zero in apps. */
	return -1;
    } 

    freq->u64 = machine_speed.u64;
    return 0;
}

#else
#include <sys/time.h>
#include <errno.h>

#define USEC_PER_SEC	1000000

PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
	return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
    }

    ts->u64 = tv.tv_sec;
    ts->u64 *= USEC_PER_SEC;
    ts->u64 += tv.tv_usec;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = USEC_PER_SEC;

    return PJ_SUCCESS;
}

#endif

