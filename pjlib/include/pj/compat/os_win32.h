/* $Header: /pjproject-0.3/pjlib/include/pj/compat/os_win32.h 6     10/29/05 11:51a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/include/pj/compat/os_win32.h $
 * 
 * 6     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 5     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 4     9/22/05 10:31a Bennylp
 * Moving all *.h files to include/.
 * 
 * 3     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 2     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#ifndef __PJ_COMPAT_OS_WIN32_H__
#define __PJ_COMPAT_OS_WIN32_H__

/**
 * @file os_win32.h
 * @brief Describes Win32 operating system family specifics.
 */

#define WIN32_LEAN_AND_MEAN
#define PJ_WIN32_WINNT		    0x0400
#define _WIN32_WINNT		    PJ_WIN32_WINNT

#define PJ_HAS_ARPA_INET_H	    0
#define PJ_HAS_ASSERT_H		    1
#define PJ_HAS_CTYPE_H		    1
#define PJ_HAS_ERRNO_H		    0   /* Must be zero, otherwise errno_test() fails. */
#define PJ_HAS_LINUX_SOCKET_H	    0
#define PJ_HAS_MALLOC_H		    1
#define PJ_HAS_NETDB_H		    0
#define PJ_HAS_NETINET_IN_H	    0
#define PJ_HAS_SETJMP_H		    1
#define PJ_HAS_STDARG_H		    1
#define PJ_HAS_STDDEF_H		    1
#define PJ_HAS_STDIO_H		    1
#define PJ_HAS_STDLIB_H		    1
#define PJ_HAS_STRING_H		    1
#define PJ_HAS_SYS_IOCTL_H	    0
#define PJ_HAS_SYS_SELECT_H	    0
#define PJ_HAS_SYS_SOCKET_H	    0
#define PJ_HAS_SYS_TIMEB_H	    1
#define PJ_HAS_SYS_TYPES_H	    1
#define PJ_HAS_TIME_H		    1
#define PJ_HAS_UNISTD_H		    0

#define PJ_HAS_MSWSOCK_H	    1
#define PJ_HAS_WINSOCK_H	    0
#define PJ_HAS_WINSOCK2_H	    1

#define PJ_SOCK_HAS_INET_ATON	    0

/* Default threading is enabled, unless it's overridden. */
#ifndef PJ_HAS_THREADS
#  define PJ_HAS_THREADS	    (1)
#endif

#define PJ_HAS_HIGH_RES_TIMER	    1
#define PJ_HAS_MALLOC               1
#define PJ_OS_HAS_CHECK_STACK	    1

#define PJ_ATOMIC_VALUE_TYPE	    long

#endif	/* __PJ_COMPAT_OS_WIN32_H__ */
