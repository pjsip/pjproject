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
/* $Log: /pjproject-0.3/pjlib/include/pj/compat/cc_msvc.h $
 * 
 * 3     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 2     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
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

#  pragma warning(disable: 4127)	// conditional expression is constant
#  pragma warning(disable: 4611)	// not wise to mix setjmp with C++
#  pragma warning(disable: 4514)	// unreferenced inline function has been removed
#  ifdef __cplusplus
#    define PJ_INLINE_SPECIFIER	inline
#  else
#    define PJ_INLINE_SPECIFIER	static __inline
#  endif
#  define PJ_THREAD_FUNC	
#  define PJ_NORETURN		__declspec(noreturn)
#  define PJ_ATTR_NORETURN	

#  define PJ_HAS_INT64	1
typedef __int64 pj_int64_t;
typedef unsigned __int64 pj_uint64_t;

#endif	/* __PJ_COMPAT_CC_MSVC_H__ */
