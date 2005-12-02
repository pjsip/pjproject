/* $Header: /pjproject/pjlib/src/pj/compat.h 6     8/24/05 10:27a Bennylp $ */
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

#ifndef __PJ_COMPAT_H__
#define __PJ_COMPAT_H__

#include <pj/config.h>

/*
 * Compiler specific macros
 */
#if defined(_MSC_VER)
  /*
   * MSVC Compiler specific
   */
#  pragma warning(disable: 4127)	// conditional expression is constant
#  pragma warning(disable: 4611)	// not wise to mix setjmp with C++
#  pragma warning(disable: 4514)	// unreferenced inline function has been removed
#  ifdef __cplusplus
#    define PJ_INLINE_SPECIFIER	inline
#  else
#    define PJ_INLINE_SPECIFIER	static __inline
#  endif
#  define PJ_THREAD_FUNC	__stdcall
#  define PJ_NORETURN		__declspec(noreturn)
#  define PJ_ATTR_NORETURN	

#  define PJ_HAS_INT64	1
typedef __int64 pj_int64_t;
typedef unsigned __int64 pj_uint64_t;

#elif defined(__GNUC__)

#  define PJ_INLINE_SPECIFIER	static inline
#  define PJ_THREAD_FUNC	
#  define PJ_NORETURN		
#  define PJ_ATTR_NORETURN	__attribute__ ((noreturn))

#  define PJ_HAS_INT64	0

#else
#  error Unknown compiler.
#endif
/* End of compiler specific macros */


#define PJ_INLINE(type)	  PJ_INLINE_SPECIFIER type

#ifdef __cplusplus
#  define PJ_DECL(type)		    type
#  define PJ_DECL_NO_RETURN(type)   type PJ_NORETURN
#  define PJ_BEGIN_DECL		    extern "C" {
#  define PJ_END_DECL		    }
#else
#  define PJ_DECL(type)		    type
#  define PJ_DECL_NO_RETURN(type)   PJ_NORETURN type
#  define PJ_BEGIN_DECL
#  define PJ_END_DECL
#endif

#define PJ_DEF(type)	  type
#define PJ_DECL_DATA


#if PJ_FUNCTIONS_ARE_INLINED
#  define PJ_IDECL(type)  PJ_INLINE(type)
#  define PJ_IDEF(type)   PJ_INLINE(type)
#else
#  define PJ_IDECL(type)  PJ_DECL(type)
#  define PJ_IDEF(type)   PJ_DEF(type)
#endif


#define PJ_UNUSED_ARG(arg)  (void)arg;

PJ_BEGIN_DECL

/**
 * Dump configuration to log with verbosity equal to info(3).
 */
PJ_DECL(void) pj_dump_config(void);

PJ_END_DECL

#endif /* __PJ_COMPAT_H__ */

