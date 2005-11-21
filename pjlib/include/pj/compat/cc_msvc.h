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
#ifndef __PJ_COMPAT_CC_MSVC_H__
#define __PJ_COMPAT_CC_MSVC_H__

/**
 * @file cc_msvc.h
 * @brief Describes Microsoft Visual C compiler specifics.
 */

#ifndef _MSC_VER
#  error "This header file is only for Visual C compiler!"
#endif

#pragma warning(disable: 4127) // conditional expression is constant
#pragma warning(disable: 4611) // not wise to mix setjmp with C++
#pragma warning(disable: 4514) // unref. inline function has been removed
#ifdef NDEBUG
#  pragma warning(disable: 4702) // unreachable code
#  pragma warning(disable: 4710) // function is not inlined.
#  pragma warning(disable: 4711) // function selected for auto inline expansion
#endif

#ifdef __cplusplus
#  define PJ_INLINE_SPECIFIER	inline
#else
#  define PJ_INLINE_SPECIFIER	static __inline
#endif

#define PJ_THREAD_FUNC	
#define PJ_NORETURN		__declspec(noreturn)
#define PJ_ATTR_NORETURN	

#define PJ_HAS_INT64	1

typedef __int64 pj_int64_t;
typedef unsigned __int64 pj_uint64_t;

#endif	/* __PJ_COMPAT_CC_MSVC_H__ */
