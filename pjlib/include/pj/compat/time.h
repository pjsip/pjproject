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
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/time.h $
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_TIME_H__
#define __PJ_COMPAT_TIME_H__

/**
 * @file time.h
 * @brief Provides ftime() and localtime() etc functions.
 */

#if defined(PJ_HAS_TIME_H) && PJ_HAS_TIME_H != 0
#  include <time.h>
#endif

#if defined(PJ_HAS_SYS_TIMEB_H) && PJ_HAS_SYS_TIMEB_H != 0
#  include <sys/timeb.h>
#endif


#endif	/* __PJ_COMPAT_TIME_H__ */
