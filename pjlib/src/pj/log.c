/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/log.c,v 1.1 2005/12/02 20:02:29 nn Exp $ */
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
#include <pj/types.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/os.h>
#include <stdio.h>
#include <stdarg.h>

#if PJ_LOG_MAX_LEVEL >= 1

static int log_max_level = PJ_LOG_MAX_LEVEL;
static pj_log_func *log_writer = &pj_log_to_stdout;

PJ_DEF(void) pj_log_set_level(int level)
{
    log_max_level = level;
}

static void pj_log(const char *obj, int level, const char *format, va_list marker)
{
    pj_time_val now;
    pj_parsed_time ptime;
    char buffer[1500];
    int len;

    if (level > log_max_level)
	return;

    /* Get current date/time. */
    pj_gettimeofday(&now);
    pj_time_decode(&now, &ptime);

    /* Print the whole message to the string buffer. */
    len = snprintf(buffer, sizeof(buffer), "%02d:%02d:%03d %-12.12s ", 
		   ptime.min, ptime.sec, ptime.msec, obj);
    len = len + vsnprintf(buffer+len, sizeof(buffer)-len, format, marker);
    if (len > 0 && len < sizeof(buffer)-1) {
	buffer[len++] = '\n';
	buffer[len++] = '\0';
    } else {
	len = sizeof(buffer)-1;
	buffer[sizeof(buffer)-2] = '\n';
	buffer[sizeof(buffer)-1] = '\0';
    }

    if (log_writer)
	(*log_writer)(level, buffer, len);
}

PJ_DEF(void) pj_log_set_log_func( pj_log_func *func )
{
    log_writer = func;
}

PJ_DEF(void) pj_log_0(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 0, format, arg);
    va_end(arg);
}

PJ_DEF(void) pj_log_1(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 1, format, arg);
    va_end(arg);
}
#endif	/* PJ_LOG_MAX_LEVEL >= 1 */

#if PJ_LOG_MAX_LEVEL >= 2
PJ_DEF(void) pj_log_2(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 2, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 3
PJ_DEF(void) pj_log_3(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 3, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 4
PJ_DEF(void) pj_log_4(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 4, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 5
PJ_DEF(void) pj_log_5(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 5, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 6
PJ_DEF(void) pj_log_6(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 6, format, arg);
    va_end(arg);
}
#endif

