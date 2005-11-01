/* $Header: /pjproject-0.3/pjlib/src/pj/compat/os_palmos.h 3     9/21/05 1:39p Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/os_palmos.h $
 * 
 * 3     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 2     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#ifndef __PJ_COMPAT_OS_PALMOS_H__
#define __PJ_COMPAT_OS_PALMOS_H__

/**
 * @file os_palmos.h
 * @brief Describes PalmOS operating system specifics.
 */

#define PJ_HAS_ARPA_INET_H	    0
#define PJ_HAS_ASSERT_H		    1
#define PJ_HAS_CTYPE_H		    1
#define PJ_HAS_ERRNO_H		    0
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
#define PJ_HAS_SYS_TIMEB_H	    0
#define PJ_HAS_SYS_TYPES_H	    1
#define PJ_HAS_TIME_H		    1
#define PJ_HAS_UNISTD_H		    0

#define PJ_HAS_MSWSOCK_H	    0
#define PJ_HAS_WINSOCK_H	    0
#define PJ_HAS_WINSOCK2_H	    0

#define PJ_SOCK_HAS_INET_ATON	    0

/* Default threading is enabled, unless it's overridden. */
#ifndef PJ_HAS_THREADS
#  define PJ_HAS_THREADS	    (1)
#endif

#define PJ_HAS_HIGH_RES_TIMER	    1
#define PJ_OS_HAS_CHECK_STACK	    0

#endif	/* __PJ_COMPAT_OS_PALMOS_H__ */
