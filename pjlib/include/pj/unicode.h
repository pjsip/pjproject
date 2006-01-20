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
#ifndef __PJ_UNICODE_H__
#define __PJ_UNICODE_H__

#include <pj/types.h>

/**
 * @file unicode.h
 * @brief Provides Unicode conversion for Unicode OSes
 */

/**
 * Convert ANSI strings to Unicode strings.
 *
 * @param str		    The ANSI string to be converted.
 * @param len		    The length of the input string.
 * @param wbuf		    Buffer to hold the Unicode string output.
 * @param wbuf_count	    Buffer size, in number of elements (not bytes).
 *
 * @return		    The Unicode string, NULL terminated.
 */
PJ_DECL(wchar_t*) pj_ansi_to_unicode(const char *str, pj_size_t len,
				     wchar_t *wbuf, pj_size_t wbuf_count);


/**
 * Convert Unicode string to ANSI string.
 *
 * @param wstr		    The Unicode string to be converted.
 * @param len		    The length of the input string.
 * @param buf		    Buffer to hold the ANSI string output.
 * @param buf_size	    Size of the output buffer.
 *
 * @return		    The ANSI string, NULL terminated.
 */
PJ_DECL(char*) pj_unicode_to_ansi(const wchar_t *wstr, pj_size_t len,
				  char *buf, pj_size_t buf_size);


#if defined(PJ_NATIVE_STRING_IS_UNICODE) && PJ_NATIVE_STRING_IS_UNICODE!=0

#   define PJ_DECL_UNICODE_TEMP_BUF(buf,size)    wchar_t buf[size]
#   define PJ_STRING_TO_NATIVE(s,buf)		 pj_ansi_to_unicode( \
						    s, strlen(s), \
						    buf, PJ_ARRAY_SIZE(buf))
#   define PJ_TEXT(s)				 _TEXT(s)

#else

#   define PJ_DECL_UNICODE_TEMP_BUF(var,size)
#   define PJ_STRING_TO_NATIVE(s, buf)		(s)
#   define PJ_TEXT(s)				(s)

#endif



#endif	/* __PJ_UNICODE_H__ */
