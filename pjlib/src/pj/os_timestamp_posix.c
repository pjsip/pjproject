/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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

#if defined(PJ_HAS_UNISTD_H) && PJ_HAS_UNISTD_H != 0
#   include <unistd.h>

#   if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && \
       defined(_POSIX_MONOTONIC_CLOCK)
#       define USE_POSIX_TIMERS 1
#   endif

#endif

#if defined(PJ_HAS_PENTIUM) && PJ_HAS_PENTIUM!=0 && \
    defined(PJ_TIMESTAMP_USE_RDTSC) && PJ_TIMESTAMP_USE_RDTSC!=0 && \
    defined(PJ_M_I386) && PJ_M_I386!=0 && \
    defined(PJ_LINUX) && PJ_LINUX!=0
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
        freq->u64 = 1;  /* return 1 to prevent division by zero in apps. */
        return -1;
    } 

    freq->u64 = machine_speed.u64;
    return 0;
}

#elif defined(PJ_DARWINOS) && PJ_DARWINOS != 0

/* SYSTEM_CLOCK will stop when the device is in deep sleep, so we use
 * KERN_BOOTTIME instead. 
 * See ticket #2140 for more details.
 */
#define USE_KERN_BOOTTIME 1

#if USE_KERN_BOOTTIME
#   include <sys/sysctl.h>
#else
#   include <mach/mach.h>
#   include <mach/clock.h>
#   include <errno.h>
#endif

#ifndef NSEC_PER_SEC
#       define NSEC_PER_SEC     1000000000
#endif

#if USE_KERN_BOOTTIME
static int64_t get_boottime()
{
    struct timeval boottime;
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    size_t size = sizeof(boottime);
    int rc;

    rc = sysctl(mib, 2, &boottime, &size, NULL, 0);
    if (rc != 0)
      return 0;

    return (int64_t)boottime.tv_sec * 1000000 + (int64_t)boottime.tv_usec;
}
#endif

PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
#if USE_KERN_BOOTTIME
    int64_t before_now, after_now;
    struct timeval now;

    after_now = get_boottime();
    do {
        before_now = after_now;
        gettimeofday(&now, NULL);
        after_now = get_boottime();
    } while (after_now != before_now);

    ts->u64 = (int64_t)now.tv_sec * 1000000 + (int64_t)now.tv_usec;
    ts->u64 -= before_now;
    ts->u64 *= 1000;
#else
    mach_timespec_t tp;
    int ret;
    clock_serv_t serv;

    ret = host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &serv);
    if (ret != KERN_SUCCESS) {
        return PJ_RETURN_OS_ERROR(EINVAL);
    }

    ret = clock_get_time(serv, &tp);
    if (ret != KERN_SUCCESS) {
        return PJ_RETURN_OS_ERROR(EINVAL);
    }

    ts->u64 = tp.tv_sec;
    ts->u64 *= NSEC_PER_SEC;
    ts->u64 += tp.tv_nsec;
#endif

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = NSEC_PER_SEC;

    return PJ_SUCCESS;
}

#elif defined(__ANDROID__)

#include <errno.h>
#include <time.h>

#if defined(PJ_HAS_ANDROID_ALARM_H) && PJ_HAS_ANDROID_ALARM_H != 0
#  include <linux/android_alarm.h>
#  include <fcntl.h>
#endif

#define NSEC_PER_SEC    1000000000

#if defined(ANDROID_ALARM_GET_TIME)
static int s_alarm_fd = -1;

void close_alarm_fd()
{
    if (s_alarm_fd != -1)
        close(s_alarm_fd);
    s_alarm_fd = -1;
}
#endif

PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    struct timespec tp;
    int err = -1;

#if defined(ANDROID_ALARM_GET_TIME)
    if (s_alarm_fd == -1) {
        int fd = open("/dev/alarm", O_RDONLY);
        if (fd >= 0) {
            s_alarm_fd = fd;
            pj_atexit(&close_alarm_fd);
        }
    }
    
    if (s_alarm_fd != -1) {
        err = ioctl(s_alarm_fd,
              ANDROID_ALARM_GET_TIME(ANDROID_ALARM_ELAPSED_REALTIME), &tp);
    }
#elif defined(CLOCK_BOOTTIME)
    err = clock_gettime(CLOCK_BOOTTIME, &tp);
#endif
    
    if (err != 0) {
        /* Fallback to CLOCK_MONOTONIC if /dev/alarm is not found, or
         * getting ANDROID_ALARM_ELAPSED_REALTIME fails, or 
         * CLOCK_BOOTTIME fails.
         */
        err = clock_gettime(CLOCK_MONOTONIC, &tp);
    }

    if (err != 0) {
        return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
    }

    ts->u64 = tp.tv_sec;
    ts->u64 *= NSEC_PER_SEC;
    ts->u64 += tp.tv_nsec;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = NSEC_PER_SEC;

    return PJ_SUCCESS;
}

#elif defined(USE_POSIX_TIMERS) && USE_POSIX_TIMERS != 0
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define NSEC_PER_SEC    1000000000

PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    struct timespec tp;

    if (clock_gettime(CLOCK_MONOTONIC, &tp) != 0) {
        return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
    }

    ts->u64 = tp.tv_sec;
    ts->u64 *= NSEC_PER_SEC;
    ts->u64 += tp.tv_nsec;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = NSEC_PER_SEC;

    return PJ_SUCCESS;
}

#else
#include <sys/time.h>
#include <errno.h>

#define USEC_PER_SEC    1000000

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
