/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/log.h,v 1.1 2005/12/02 20:02:29 nn Exp $ */
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

#ifndef __PJ_LOG_H__
#define __PJ_LOG_H__

/**
 * @file log.h
 * @brief Logging Utility.
 */

#include <pj/types.h>
#include <stdio.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_MISC Miscelaneous
 * @ingroup PJ
 */

/**
 * @defgroup PJ_LOG Logging Utility
 * @ingroup PJ_MISC
 * @{
 */

/**
 * Declare maximum logging level/verbosity. Lower number indicates higher
 * importance, with the highest importance has level zero. The least
 * important level is five in this implementation, but this can be extended
 * by supplying the appropriate implementation.
 *
 * The level conventions:
 *  - 0: fatal error
 *  - 1: error
 *  - 2: warning
 *  - 3: info
 *  - 4: debug
 *  - 5: trace
 *  - 6: more detailed trace
 */
#ifndef PJ_LOG_MAX_LEVEL
#  define PJ_LOG_MAX_LEVEL   4
#endif

/**
 * Logging macro.
 * @param level The logging verbosity level. Lower number indicates higher
 *              importance, with level zero indicates fatal error.
 * @param obj_format_argf Enclosed 'printf' like arguments, with the first 
 *	        argument is the format string and the following arguments are 
 *              the arguments suitable for the format string.
 *
 * Sample:
 * \verbatim
   PJ_LOG(2, (__FILE__, "current value is %d", value));
   \endverbatim
 */
#define PJ_LOG(level,obj_format_argf)	pj_log_wrapper_##level(obj_format_argf)

/**
 * Logging function.
 *
 * @param level	    Log level.
 * @param data	    Log message.
 * @param len	    Message length.
 */
typedef void pj_log_func(int level, const char *data, int len);

/**
 * Default logging function which writes to stdout.
 *
 * @param level	    Log level.
 * @param buffer    Log message.
 * @param len	    Message length.
 */
PJ_DECL(void) pj_log_to_stdout(int level, const char *buffer, int len);


#if PJ_LOG_MAX_LEVEL >= 1

/**
 * Set log output.
 * @param file the output file.
 */
PJ_DECL(void) pj_log_set_log_func( pj_log_func *func );

/**
 * Set maximum log level.
 * @param level the maximum level.
 */
PJ_DECL(void) pj_log_set_level(int level);

#else	/* #if PJ_LOG_MAX_LEVEL >= 1 */

#  define pj_log_set_log_func(x)
#  define pj_log_set_level(x)

#endif	/* #if PJ_LOG_MAX_LEVEL >= 1 */

/** 
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/*
 * Log functions implementation prototypes.
 */

#if PJ_LOG_MAX_LEVEL >= 1
    #define pj_log_wrapper_1(arg)	pj_log_1 arg
    PJ_DECL(void) pj_log_1(const char *, const char *format, ...);
#else
    #define pj_log_wrapper_1(arg)
#endif

#if PJ_LOG_MAX_LEVEL >= 2
    #define pj_log_wrapper_2(arg)	pj_log_2 arg
    PJ_DECL(void) pj_log_2(const char *, const char *format, ...);
#else
    #define pj_log_wrapper_2(arg)
#endif

#if PJ_LOG_MAX_LEVEL >= 3
    #define pj_log_wrapper_3(arg)	pj_log_3 arg
    PJ_DECL(void) pj_log_3(const char *, const char *format, ...);
#else
    #define pj_log_wrapper_3(arg)
#endif

#if PJ_LOG_MAX_LEVEL >= 4
    #define pj_log_wrapper_4(arg)	pj_log_4 arg
    PJ_DECL(void) pj_log_4(const char *, const char *format, ...);
#else
    #define pj_log_wrapper_4(arg)
#endif

#if PJ_LOG_MAX_LEVEL >= 5
    #define pj_log_wrapper_5(arg)	pj_log_5 arg
    PJ_DECL(void) pj_log_5(const char *, const char *format, ...);
#else
    #define pj_log_wrapper_5(arg)
#endif

#if PJ_LOG_MAX_LEVEL >= 6
    #define pj_log_wrapper_6(arg)	pj_log_6 arg
    PJ_DECL(void) pj_log_6(const char *, const char *format, ...);
#else
    #define pj_log_wrapper_6(arg)
#endif


PJ_END_DECL

#endif

