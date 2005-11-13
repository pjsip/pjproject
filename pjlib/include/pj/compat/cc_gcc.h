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
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/cc_gcc.h $
 * 
 * 2     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#ifndef __PJ_COMPAT_CC_GCC_H__
#define __PJ_COMPAT_CC_GCC_H__

/**
 * @file cc_gcc.h
 * @brief Describes GCC compiler specifics.
 */

#ifndef __GNUC__
#  error "This file is only for gcc!"
#endif

#define PJ_INLINE_SPECIFIER	static inline
#define PJ_THREAD_FUNC	
#define PJ_NORETURN		
#define PJ_ATTR_NORETURN	__attribute__ ((noreturn))

#define PJ_HAS_INT64		1

typedef long long pj_int64_t;
typedef unsigned long long pj_uint64_t;


#endif	/* __PJ_COMPAT_CC_GCC_H__ */

