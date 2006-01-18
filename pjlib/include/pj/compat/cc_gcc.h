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
#ifndef __PJ_COMPAT_CC_GCC_H__
#define __PJ_COMPAT_CC_GCC_H__

/**
 * @file cc_gcc.h
 * @brief Describes GCC compiler specifics.
 */

#ifndef __GNUC__
#  error "This file is only for gcc!"
#endif

#define PJ_CC_NAME		"gcc"
#define PJ_CC_VER_1		__GNUC__
#define PJ_CC_VER_2		__GNUC_MINOR__
#define PJ_CC_VER_3		__GNUC_PATCHLEVEL__


#define PJ_INLINE_SPECIFIER	static inline
#define PJ_THREAD_FUNC	
#define PJ_NORETURN		
#define PJ_ATTR_NORETURN	__attribute__ ((noreturn))

#define PJ_HAS_INT64		1

typedef long long pj_int64_t;
typedef unsigned long long pj_uint64_t;

#define PJ_INT64_FMT		"L"


#endif	/* __PJ_COMPAT_CC_GCC_H__ */

