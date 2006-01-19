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
#ifndef __PJ_COMPAT_UNICODE_H__
#define __PJ_COMPAT_UNICODE_H__

#include <pj/types.h>

/**
 * @file unicode.h
 * @brief Provides Unicode conversion for Unicode OSes
 */

#if defined(PJ_NATIVE_STRING_IS_UNICODE) && PJ_NATIVE_STRING_IS_UNICODE!=0

#   define PJ_DECL_UNICODE_TEMP_BUF(var,size)    wchar_t var[size]
#   define PJ_NATIVE_STRING(s,buf) pj_ansi_to_unicode(s,buf,PJ_ARRAY_SIZE(buf))

    PJ_DECL(wchar_t*) pj_ansi_to_unicode(const char *s, wchar_t *buf,
					 pj_size_t buf_count);

#else	/* PJ_NATIVE_STRING_IS_UNICODE */

#   define PJ_DECL_UNICODE_TEMP_BUF(var,size)
#   define PJ_NATIVE_STRING(s, buf)		s

#endif	/* PJ_NATIVE_STRING_IS_UNICODE */


#endif	/* __PJ_COMPAT_UNICODE_H__ */
