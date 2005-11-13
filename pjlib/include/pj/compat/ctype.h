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
/* $Log: /pjproject-0.3/pjlib/include/pj/compat/ctype.h $
 * 
 * 3     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 2     9/22/05 10:31a Bennylp
 * Moving all *.h files to include/.
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_CTYPE_H__
#define __PJ_COMPAT_CTYPE_H__

/**
 * @file ctype.h
 * @brief Provides ctype function family.
 */

#if defined(PJ_HAS_CTYPE_H) && PJ_HAS_CTYPE_H != 0
#  include <ctype.h>
#else
#  define isalnum(c)	    (isalpha(c) || isdigit(c))
#  define isalpha(c)	    (islower(c) || isupper(c))
#  define isascii(c)	    (((unsigned char)(c))<=0x7f)
#  define isdigit(c)	    ((c)>='0' && (c)<='9')
#  define isspace(c)	    ((c)==' '  || (c)=='\t' ||\
			     (c)=='\n' || (c)=='\r' || (c)=='\v')
#  define islower(c)	    ((c)>='a' && (c)<='z')
#  define isupper(c)	    ((c)>='A' && (c)<='Z')
#  define isxdigit(c)	    (isdigit(c) || (tolower(c)>='a'&&tolower(c)<='f'))
#  define tolower(c)	    (((c) >= 'A' && (c) <= 'Z') ? (c)+('a'-'A') : (c))
#  define toupper(c)	    (((c) >= 'a' && (c) <= 'z') ? (c)-('a'-'A') : (c))
#endif

#define isblank(c)	    (c==' ' || c=='\t')


#endif	/* __PJ_COMPAT_CTYPE_H__ */
