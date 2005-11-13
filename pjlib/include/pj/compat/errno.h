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
#ifndef __PJ_COMPAT_ERRNO_H__
#define __PJ_COMPAT_ERRNO_H__

#if defined(PJ_WIN32) && PJ_WIN32 != 0

    typedef unsigned long pj_os_err_type;
#   define pj_get_native_os_error()	    GetLastError()
#   define pj_get_native_netos_error()	    WSAGetLastError()

#elif (defined(PJ_LINUX) && PJ_LINUX != 0) || \
      (defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL != 0) || \
      (defined(PJ_SUNOS) && PJ_SUNOS != 0)

    typedef int pj_os_err_type;
#   define pj_get_native_os_error()	    (errno)
#   define pj_get_native_netos_error()	    (errno)

#else

#   error "Please define pj_os_err_type for this platform here!"

#endif


#endif	/* __PJ_COMPAT_ERRNO_H__ */

