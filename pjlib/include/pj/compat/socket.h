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
/* $Log: /pjproject-0.3/pjlib/include/pj/compat/socket.h $
 * 
 * 5     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 4     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 3     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 2     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#ifndef __PJ_COMPAT_SOCKET_H__
#define __PJ_COMPAT_SOCKET_H__

/**
 * @file socket.h
 * @brief Provides all socket related functions,data types, error codes, etc.
 */

#if defined(PJ_HAS_WINSOCK_H) && PJ_HAS_WINSOCK_H != 0
#  include <winsock.h>
#endif

#if defined(PJ_HAS_WINSOCK2_H) && PJ_HAS_WINSOCK2_H != 0
#  include <winsock2.h>
#endif

#if defined(PJ_HAS_SYS_TYPES_H) && PJ_HAS_SYS_TYPES_H != 0
#  include <sys/types.h>
#endif

#if defined(PJ_HAS_SYS_SOCKET_H) && PJ_HAS_SYS_SOCKET_H != 0
#  include <sys/socket.h>
#endif

#if defined(PJ_HAS_LINUX_SOCKET_H) && PJ_HAS_LINUX_SOCKET_H != 0
#  include <linux/socket.h>
#endif

#if defined(PJ_HAS_SYS_SELECT_H) && PJ_HAS_SYS_SELECT_H != 0
#  include <sys/select.h>
#endif

#if defined(PJ_HAS_NETINET_IN_H) && PJ_HAS_NETINET_IN_H != 0
#  include <netinet/in.h>
#endif

#if defined(PJ_HAS_ARPA_INET_H) && PJ_HAS_ARPA_INET_H != 0
#  include <arpa/inet.h>
#endif

#if defined(PJ_HAS_SYS_IOCTL_H) && PJ_HAS_SYS_IOCTL_H != 0
#  include <sys/ioctl.h>	/* FBIONBIO */
#endif

#if defined(PJ_HAS_ERRNO_H) && PJ_HAS_ERRNO_H != 0
#  include <errno.h>
#endif

#if defined(PJ_HAS_NETDB_H) && PJ_HAS_NETDB_H != 0
#  include <netdb.h>
#endif

#if defined(PJ_HAS_UNISTD_H) && PJ_HAS_UNISTD_H != 0
#  include <unistd.h>
#endif


/*
 * Define common errors.
 */
#ifdef PJ_WIN32
#  define OSERR_EWOULDBLOCK    WSAEWOULDBLOCK
#  define OSERR_EINPROGRESS    WSAEINPROGRESS
#else
#  define OSERR_EWOULDBLOCK    EWOULDBLOCK
#  define OSERR_EINPROGRESS    EINPROGRESS
#endif


/*
 * And undefine this..
 */
#undef s_addr

/*
 * Linux kernel specifics
 */
#ifdef PJ_LINUX_KERNEL
#   include <linux/net.h>
#   include <asm/ioctls.h>		/* FIONBIO	*/
#   include <linux/syscalls.h>	/* sys_select() */
#   include <asm/uaccess.h>	/* set/get_fs()	*/

    typedef int socklen_t;
#   define getsockopt  sys_getsockopt

    /*
     * Wrapper for select() in Linux kernel.
     */
    PJ_INLINE(int) select(int n, fd_set *inp, fd_set *outp, fd_set *exp,
		          struct timeval *tvp)
    {
        int count;
        mm_segment_t oldfs = get_fs();
        set_fs(KERNEL_DS);
        count = sys_select(n, inp, outp, exp, tvp);
        set_fs(oldfs);
        return count;
    }
#endif	/* PJ_LINUX_KERNEL */


/*
 * Windows specific
 */
#ifdef PJ_WIN32
    typedef int socklen_t;;
#endif


#endif	/* __PJ_COMPAT_SOCKET_H__ */

