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
/* $Log: /pjproject-0.3/pjlib/include/pj/compat/sprintf.h $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_SPRINTF_H__
#define __PJ_COMPAT_SPRINTF_H__

/**
 * @file sprintf.h
 * @brief Provides sprintf() and snprintf() functions.
 */

#if defined(PJ_HAS_STDIO_H) && PJ_HAS_STDIO_H != 0
#  include <stdio.h>
#endif

#if defined(_MSC_VER)
#  define snprintf	_snprintf
#endif

#define pj_sprintf      sprintf
#define pj_snprintf	snprintf

#endif	/* __PJ_COMPAT_SPRINTF_H__ */
