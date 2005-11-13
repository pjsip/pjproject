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
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/string.h $
 * 
 * 3     9/22/05 10:31a Bennylp
 * Moving all *.h files to include/.
 * 
 * 2     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
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
#  define strncasecmp	strnicmp
#  define snprintf	_snprintf
#else
#  define stricmp	strcasecmp
#  define strnicmp	strncasecmp
#endif


#define pj_native_strcmp        strcmp
#define pj_native_strlen        strlen
#define pj_native_strcpy        strcpy
#define pj_native_strstr        strstr
#define pj_native_strchr        strchr
#define pj_native_strcasecmp    strcasecmp
#define pj_native_strncasecmp   strncasecmp


#endif	/* __PJ_COMPAT_STRING_H__ */
