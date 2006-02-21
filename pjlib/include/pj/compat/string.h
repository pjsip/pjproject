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
#ifndef __PJ_COMPAT_STRING_H__
#define __PJ_COMPAT_STRING_H__

/**
 * @file string.h
 * @brief Provides string manipulation functions found in ANSI string.h.
 */


#if defined(PJ_HAS_STRING_H) && PJ_HAS_STRING_H != 0
#  include <string.h>
#else

    PJ_DECL(int) strcasecmp(const char *s1, const char *s2);
    PJ_DECL(int) strncasecmp(const char *s1, const char *s2, int len);

#endif

#if defined(_MSC_VER)
#  define strcasecmp	stricmp
#  if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
#   define strncasecmp	_strnicmp
#  else
#   define strncasecmp	strnicmp
#  endif
#  define snprintf	_snprintf
#  define snwprintf	_snwprintf
#  define wcsicmp	_wcsicmp
#  define wcsnicmp	_wcsnicmp
#else
#  define stricmp	strcasecmp
#  define strnicmp	strncasecmp

#  if defined(PJ_NATIVE_STRING_IS_UNICODE) && PJ_NATIVE_STRING_IS_UNICODE!=0
#     error "Implement Unicode string functions"
#  endif
#endif


#define pj_ansi_strcmp		strcmp
#define pj_ansi_strncmp		strncmp
#define pj_ansi_strlen		strlen
#define pj_ansi_strcpy		strcpy
#define pj_ansi_strcat		strcat
#define pj_ansi_strstr		strstr
#define pj_ansi_strchr		strchr
#define pj_ansi_strcasecmp	strcasecmp
#define pj_ansi_stricmp		strcasecmp
#define pj_ansi_strncasecmp	strncasecmp
#define pj_ansi_strnicmp	strncasecmp
#define pj_ansi_sprintf		sprintf
#define pj_ansi_snprintf	snprintf

#define pj_unicode_strcmp	wcscmp
#define pj_unicode_strncmp	wcsncmp
#define pj_unicode_strlen	wcslen
#define pj_unicode_strcpy	wcscpy
#define pj_unicode_strcat	wcscat
#define pj_unicode_strstr	wcsstr
#define pj_unicode_strchr	wcschr
#define pj_unicode_strcasecmp	wcsicmp
#define pj_unicode_stricmp	wcsicmp
#define pj_unicode_strncasecmp	wcsnicmp
#define pj_unicode_strnicmp	wcsnicmp
#define pj_unicode_sprintf	swprintf
#define pj_unicode_snprintf	snwprintf


#if defined(PJ_NATIVE_STRING_IS_UNICODE) && PJ_NATIVE_STRING_IS_UNICODE!=0
#   define pj_native_strcmp	    pj_unicode_strcmp
#   define pj_native_strncmp	    pj_unicode_strncmp
#   define pj_native_strlen	    pj_unicode_strlen
#   define pj_native_strcpy	    pj_unicode_strcpy
#   define pj_native_strcat	    pj_unicode_strcat
#   define pj_native_strstr	    pj_unicode_strstr
#   define pj_native_strchr	    pj_unicode_strchr
#   define pj_native_strcasecmp	    pj_unicode_strcasecmp
#   define pj_native_stricmp	    pj_unicode_stricmp
#   define pj_native_strncasecmp    pj_unicode_strncasecmp
#   define pj_native_strnicmp	    pj_unicode_strnicmp
#   define pj_native_sprintf	    pj_unicode_sprintf
#   define pj_native_snprintf	    pj_unicode_snprintf
#else
#   define pj_native_strcmp	    pj_ansi_strcmp
#   define pj_native_strncmp	    pj_ansi_strncmp
#   define pj_native_strlen	    pj_ansi_strlen
#   define pj_native_strcpy	    pj_ansi_strcpy
#   define pj_native_strcat	    pj_ansi_strcat
#   define pj_native_strstr	    pj_ansi_strstr
#   define pj_native_strchr	    pj_ansi_strchr
#   define pj_native_strcasecmp	    pj_ansi_strcasecmp
#   define pj_native_stricmp	    pj_ansi_stricmp
#   define pj_native_strncasecmp    pj_ansi_strncasecmp
#   define pj_native_strnicmp	    pj_ansi_strnicmp
#   define pj_native_sprintf	    pj_ansi_sprintf
#   define pj_native_snprintf	    pj_ansi_snprintf
#endif


#endif	/* __PJ_COMPAT_STRING_H__ */
