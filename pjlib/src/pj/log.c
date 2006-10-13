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
#include <pj/types.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/os.h>
#include <pj/compat/stdarg.h>

#if PJ_LOG_MAX_LEVEL >= 1

static int log_max_level = PJ_LOG_MAX_LEVEL;
static pj_log_func *log_writer = &pj_log_write;
static unsigned log_decor = PJ_LOG_HAS_TIME | PJ_LOG_HAS_MICRO_SEC |
			    PJ_LOG_HAS_SENDER | PJ_LOG_HAS_NEWLINE;

#if PJ_LOG_USE_STACK_BUFFER==0
static char log_buffer[PJ_LOG_MAX_SIZE];
#endif

PJ_DEF(void) pj_log_set_decor(unsigned decor)
{
    log_decor = decor;
}

PJ_DEF(unsigned) pj_log_get_decor(void)
{
    return log_decor;
}

PJ_DEF(void) pj_log_set_level(int level)
{
    log_max_level = level;
}

PJ_DEF(int) pj_log_get_level(void)
{
    return log_max_level;
}

PJ_DEF(void) pj_log_set_log_func( pj_log_func *func )
{
    log_writer = func;
}

PJ_DEF(pj_log_func*) pj_log_get_log_func(void)
{
    return log_writer;
}

PJ_DEF(void) pj_log( const char *sender, int level, 
		     const char *format, va_list marker)
{
    pj_time_val now;
    pj_parsed_time ptime;
    char *pre;
#if PJ_LOG_USE_STACK_BUFFER
    char log_buffer[PJ_LOG_MAX_SIZE];
#endif
    int len, print_len;

    PJ_CHECK_STACK();

    if (level > log_max_level)
	return;

    /* Get current date/time. */
    pj_gettimeofday(&now);
    pj_time_decode(&now, &ptime);

    pre = log_buffer;
    if (log_decor & PJ_LOG_HAS_DAY_NAME) {
	static const char *wdays[] = { "Sun", "Mon", "Tue", "Wed",
				       "Thu", "Fri", "Sat"};
	pj_ansi_strcpy(pre, wdays[ptime.wday]);
	pre += 3;
    }
    if (log_decor & PJ_LOG_HAS_YEAR) {
	*pre++ = ' ';
	pre += pj_utoa(ptime.year, pre);
    }
    if (log_decor & PJ_LOG_HAS_MONTH) {
	*pre++ = '-';
	pre += pj_utoa_pad(ptime.mon, pre, 2, '0');
    }
    if (log_decor & PJ_LOG_HAS_DAY_OF_MON) {
	*pre++ = ' ';
	pre += pj_utoa_pad(ptime.day, pre, 2, '0');
    }
    if (log_decor & PJ_LOG_HAS_TIME) {
	*pre++ = ' ';
	pre += pj_utoa_pad(ptime.hour, pre, 2, '0');
	*pre++ = ':';
	pre += pj_utoa_pad(ptime.min, pre, 2, '0');
	*pre++ = ':';
	pre += pj_utoa_pad(ptime.sec, pre, 2, '0');
    }
    if (log_decor & PJ_LOG_HAS_MICRO_SEC) {
	*pre++ = '.';
	pre += pj_utoa_pad(ptime.msec, pre, 3, '0');
    }
    if (log_decor & PJ_LOG_HAS_SENDER) {
	enum { SENDER_WIDTH = 14 };
	int sender_len = strlen(sender);
	*pre++ = ' ';
	if (sender_len <= SENDER_WIDTH) {
	    while (sender_len < SENDER_WIDTH)
		*pre++ = ' ', ++sender_len;
	    while (*sender)
		*pre++ = *sender++;
	} else {
	    int i;
	    for (i=0; i<SENDER_WIDTH; ++i)
		*pre++ = *sender++;
	}
    }

    if (log_decor != 0 && log_decor != PJ_LOG_HAS_NEWLINE)
	*pre++ = ' ';

    len = pre - log_buffer;

    /* Print the whole message to the string log_buffer. */
    print_len = pj_ansi_vsnprintf(pre, sizeof(log_buffer)-len, format, 
				  marker);
    if (print_len < 0) {
	level = 1;
	print_len = pj_ansi_snprintf(pre, sizeof(log_buffer)-len, 
				     "<logging error: msg too long>");
    }
    len = len + print_len;
    if (len > 0 && len < sizeof(log_buffer)-2) {
	if (log_decor & PJ_LOG_HAS_CR) {
	    log_buffer[len++] = '\r';
	}
	if (log_decor & PJ_LOG_HAS_NEWLINE) {
	    log_buffer[len++] = '\n';
	}
	log_buffer[len] = '\0';
    } else {
	len = sizeof(log_buffer)-1;
	if (log_decor & PJ_LOG_HAS_CR) {
	    log_buffer[sizeof(log_buffer)-3] = '\r';
	}
	if (log_decor & PJ_LOG_HAS_NEWLINE) {
	    log_buffer[sizeof(log_buffer)-2] = '\n';
	}
	log_buffer[sizeof(log_buffer)-1] = '\0';
    }

    if (log_writer)
	(*log_writer)(level, log_buffer, len);
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

