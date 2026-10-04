/* 
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJ_ASSERT_H__
#define __PJ_ASSERT_H__

/**
 * @file assert.h
 * @brief Assertion macro pj_assert().
 */

#include <pj/config.h>
#include <pj/compat/assert.h>

/**
 * @defgroup pj_assert Assertion Macro
 * @ingroup PJ_MISC
 * @{
 *
 * Assertion and other helper macros for sanity checking.
 */

/**
 * @hideinitializer
 * Check during debug build that an expression is true. If the expression
 * computes to false during run-time, then the program will stop at the
 * offending statements.
 * For release build, this macro will only log the assertion, while the
 * program continues running.
 *
 * @param expr      The expression to be evaluated.
 */
#if PJ_DEBUG==0

#   ifndef pj_assert
#       include "pj/os.h"
#       include "pj/log.h"
#       define pj_assert(expr) \
            do { \
                if (!(expr)) { \
                    if (pj_thread_is_registered()) \
                        PJ_LOG(1, (__FILE__, "Assert failed: %s", #expr)); \
                } \
            } while (0)
#   endif

#else /* PJ_DEBUG != 0 */

#   ifndef pj_assert
#       define pj_assert(expr)   assert(expr)
#   endif

#endif

/**
 * @hideinitializer
 * For all builds, log the message.
 * Check during debug build that an expression is true. If the expression
 * computes to false during run-time, then the program will stop at the
 * offending statements.
 * For release build, this macro only print message on the log.
 * @param expr	    The expression to be evaluated.
 * @param ...	    File name. The format string for the log message
 *                  ("config.c", " PJ_VERSION: %s", PJ_VERSION)
 */
#ifndef PJ_ASSERT_LOG
#include "pj/log.h"
#define PJ_ASSERT_LOG(expr,...)    \
            do { \
                if (!(expr)) { PJ_LOG(1,(__VA_ARGS__)); assert(expr); } \
            } while (0)

#endif

/**
 * @hideinitializer
 * If the expression yields false, assertion will be triggered
 * and the current function will return with the specified return value.
 */
// #if defined(PJ_ENABLE_EXTRA_CHECK) && PJ_ENABLE_EXTRA_CHECK != 0
#define PJ_ASSERT_RETURN(expr,retval)    \
            do { \
                if (!(expr)) { pj_assert(expr); return retval; } \
            } while (0)
//#else
//#   define PJ_ASSERT_RETURN(expr,retval)    pj_assert(expr)
//#endif

/**
 * @hideinitializer
 * If the expression yields false, assertion will be triggered
 * and @a exec_on_fail will be executed.
 */
//#if defined(PJ_ENABLE_EXTRA_CHECK) && PJ_ENABLE_EXTRA_CHECK != 0
#define PJ_ASSERT_ON_FAIL(expr,exec_on_fail)    \
            { \
                pj_assert(expr); \
                if (!(expr)) exec_on_fail; \
            }
//#else
//#   define PJ_ASSERT_ON_FAIL(expr,exec_on_fail)    pj_assert(expr)
//#endif

/**
 * @def PJ_STATIC_ASSERT(expr, msg)
 * @hideinitializer
 * Check a condition at compile time. If \a expr is false, the build fails
 * with an error that includes \a msg. Unlike pj_assert(), nothing is
 * evaluated at run time and no code is generated.
 *
 * Prefer it over a preprocessor \#if for clarity. It is required when the
 * condition involves something the preprocessor cannot evaluate, such as
 * sizeof().
 *
 * The macro can be used wherever a declaration is allowed (at file scope, or
 * among the declarations of a block) and must be followed by a semicolon.
 * Sample usage:
 * \code
   PJ_STATIC_ASSERT(sizeof(my_hdr) == 4, my_hdr_must_be_4_bytes);
   \endcode
 *
 * @param expr      An integer constant expression. The build fails if it
 *                  evaluates to zero.
 * @param msg       An identifier, NOT a string literal: write it without
 *                  quotes, e.g. buffer_too_small. It names the condition
 *                  being checked and appears in the compiler's error
 *                  message. It must be a valid C identifier because, on
 *                  compilers without C11 _Static_assert, it becomes part of
 *                  a typedef name.
 *
 * @note            On compilers without C11 _Static_assert, at most one
 *                  PJ_STATIC_ASSERT() may appear per source line.
 */
#ifndef PJ_STATIC_ASSERT
#   if defined(__cplusplus) && __cplusplus >= 201103L
#       define PJ_STATIC_ASSERT(expr,msg)   static_assert(expr, #msg)
#   elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#       define PJ_STATIC_ASSERT(expr,msg)   _Static_assert(expr, #msg)
#   else
        /* Fallback: an array whose size is negative when the expression is
         * false. The typedef name carries the message and __LINE__, so
         * several assertions may appear in the same scope -- but no more
         * than one per line.
         */

        /* At block scope the typedef is never referenced, which -Wall
         * reports as -Wunused-local-typedefs.
         */
#       if defined(__GNUC__) || defined(__clang__)
#           define PJ_STATIC_ASSERT_UNUSED_ __attribute__((unused))
#       else
#           define PJ_STATIC_ASSERT_UNUSED_
#       endif

#       define PJ_STATIC_ASSERT_CAT_(a,b)   a ## b
#       define PJ_STATIC_ASSERT_(expr,msg,line) \
            typedef char PJ_STATIC_ASSERT_CAT_(pj_static_assert_##msg##_, \
                                               line)[(expr) ? 1 : -1] \
                                               PJ_STATIC_ASSERT_UNUSED_
#       define PJ_STATIC_ASSERT(expr,msg) \
            PJ_STATIC_ASSERT_(expr, msg, __LINE__)
#   endif
#endif

/** @} */

#endif  /* __PJ_ASSERT_H__ */

