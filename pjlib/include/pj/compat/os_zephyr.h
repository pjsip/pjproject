/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJ_COMPAT_OS_ZEPHYR_H__
#define __PJ_COMPAT_OS_ZEPHYR_H__

/**
 * @file os_zephyr.h
 * @brief Describes Zephyr RTOS specifics.
 *
 * PJLIB runs on Zephyr through its POSIX layer (CONFIG_POSIX_API), with the
 * unix backends: os_core_unix.c, sock_bsd.c, ioqueue_select.c and friends.
 * Zephyr force-includes its Kconfig output into every compilation unit, so
 * the facts that depend on the application's configuration (threads, TCP,
 * IPv6, TLS) are taken from the CONFIG_ macros here rather than probed.
 * Each of those can still be overridden from the build with -D.
 */

#define PJ_OS_NAME                  "zephyr"

#define PJ_HAS_ARPA_INET_H          1
#define PJ_HAS_ASSERT_H             1
#define PJ_HAS_CTYPE_H              1
#define PJ_HAS_ERRNO_H              1
#define PJ_HAS_EXECINFO_H           0
#define PJ_HAS_FCNTL_H              1
#define PJ_HAS_INTTYPES_H           1
#define PJ_HAS_LIMITS_H             1
#define PJ_HAS_LINUX_SOCKET_H       0
#define PJ_HAS_MALLOC_H             0
#define PJ_HAS_NETDB_H              1
#define PJ_HAS_NETINET_IN_SYSTM_H   0
#define PJ_HAS_NETINET_IN_H         1
#define PJ_HAS_NETINET_IP_H         0
#define PJ_HAS_NETINET_TCP_H        1
#define PJ_HAS_NET_IF_H             1
#define PJ_HAS_IFADDRS_H            0
#define PJ_HAS_SETJMP_H             1
#define PJ_HAS_STDARG_H             1
#define PJ_HAS_STDDEF_H             1
#define PJ_HAS_STDIO_H              1
#define PJ_HAS_STDINT_H             1
#define PJ_HAS_STDLIB_H             1
#define PJ_HAS_STRING_H             1
#define PJ_HAS_STRINGS_H            1
#define PJ_HAS_SYS_IOCTL_H          1
#define PJ_HAS_SYS_SELECT_H         1
#define PJ_HAS_SYS_SOCKET_H         1
#define PJ_HAS_SYS_FILIO_H          0
#define PJ_HAS_SYS_SOCKIO_H         0
#define PJ_HAS_SYS_STAT_H           1
#define PJ_HAS_SYS_TIME_H           1
#define PJ_HAS_SYS_TIMEB_H          0
#define PJ_HAS_SYS_TYPES_H          1
#define PJ_HAS_SYS_UTSNAME_H        1
#define PJ_HAS_TIME_H               1
#define PJ_HAS_UNISTD_H             1
#define PJ_HAS_SEMAPHORE_H          1

#define PJ_HAS_MSWSOCK_H            0
#define PJ_HAS_WINSOCK_H            0
#define PJ_HAS_WINSOCK2_H           0
#define PJ_HAS_WS2TCPIP_H           0
#define PJ_HAS_PTHREAD_NP_H         0

#define PJ_HAS_LOCALTIME_R          1

/* On native_sim the image is compiled against picolibc's headers but
 * setjmp() and longjmp() come from the host's C library, whose jmp_buf is
 * larger. Oversize pj_jmp_buf so setjmp() cannot overrun the stack.
 */
#if defined(CONFIG_NATIVE_BUILD)
#   define PJ_JMP_BUF_LONGS         64
#endif

/* Zephyr's socket layer has the POSIX.1-2008 set: inet_pton()/inet_ntop()
 * and getaddrinfo(), but not the obsolete inet_aton(), inet_addr() and
 * gethostbyname().
 */
#define PJ_SOCK_HAS_INET_ATON       0
#define PJ_SOCK_HAS_INET_ADDR       0
#define PJ_SOCK_HAS_INET_PTON       1
#define PJ_SOCK_HAS_INET_NTOP       1
#define PJ_SOCK_HAS_GETADDRINFO     1
#define PJ_SOCK_HAS_GETHOSTBYNAME   0
#ifndef PJ_SOCK_HAS_SOCKETPAIR
#   if defined(CONFIG_NET_SOCKETPAIR)
#       define PJ_SOCK_HAS_SOCKETPAIR   1
#   else
#       define PJ_SOCK_HAS_SOCKETPAIR   0
#   endif
#endif

/* Non-blocking mode is set with fcntl(); Zephyr sockets have no FIONBIO. */
#define PJ_SOCK_NONBLOCK_USE_FCNTL  1

/* Zephyr's sockaddr_in has no sin_zero padding, so it is 8 bytes. */
#define PJ_SOCKADDR_IN_SIN_ZERO_LEN 0
#define PJ_SOCKADDR_HAS_LEN         0
#define PJ_HAS_SOCKLEN_T            1
#define PJ_SELECT_NEEDS_NFDS        1
#define PJ_HAS_ERRNO_VAR            1
#define PJ_HAS_SO_ERROR             1
#define PJ_BLOCKING_ERROR_VAL       EAGAIN
#define PJ_BLOCKING_CONNECT_ERROR_VAL   EINPROGRESS

/* Threads follow the pthread option. Note that Zephyr's thread-specific
 * storage works only on threads created with pthread_create(), so in a
 * threaded build the application must call PJLIB from such a thread, not
 * from Zephyr's main thread.
 */
#ifndef PJ_HAS_THREADS
#   if defined(CONFIG_POSIX_THREADS)
#       define PJ_HAS_THREADS       1
#   else
#       define PJ_HAS_THREADS       0
#   endif
#endif

#ifndef PJ_HAS_SCHED_YIELD
#   if defined(CONFIG_POSIX_PRIORITY_SCHEDULING)
#       define PJ_HAS_SCHED_YIELD   1
#   else
#       define PJ_HAS_SCHED_YIELD   0
#   endif
#endif

#ifndef PJ_HAS_TCP
#   if defined(CONFIG_NET_TCP)
#       define PJ_HAS_TCP           1
#   else
#       define PJ_HAS_TCP           0
#   endif
#endif

/* Zephyr's sockaddr_in6 has no flow label and an 8-bit scope id, so its
 * layout differs from pj_sockaddr_in6, which follows BSD. Until pjlib
 * converts between the two, IPv6 stays off whatever CONFIG_NET_IPV6 says.
 */
#ifndef PJ_HAS_IPV6
#   define PJ_HAS_IPV6              0
#endif
#define PJ_SOCK_HAS_IPV6_V6ONLY     PJ_HAS_IPV6

/* SIP over TLS uses Zephyr's Mbed TLS, when the pjproject module says so. */
#ifndef PJ_HAS_SSL_SOCK
#   if defined(CONFIG_PJPROJECT_TLS)
#       define PJ_HAS_SSL_SOCK      1
#       define PJ_SSL_SOCK_IMP      PJ_SSL_SOCK_IMP_MBEDTLS
#   else
#       define PJ_HAS_SSL_SOCK      0
#   endif
#endif

#define PJ_HAS_HIGH_RES_TIMER       1
#define PJ_HAS_MALLOC               1
#ifndef PJ_OS_HAS_CHECK_STACK
#   define PJ_OS_HAS_CHECK_STACK    0
#endif
#define PJ_NATIVE_STRING_IS_UNICODE 0

#define PJ_ATOMIC_VALUE_TYPE        long

/* pthread_rwlock is optional in Zephyr; emulating it costs little. */
#define PJ_EMULATE_RWMUTEX          1

/* Zephyr cannot grow a thread's stack, so pj_thread_create() passes the
 * requested size on, and leaves the allocation to Zephyr.
 */
#define PJ_THREAD_SET_STACK_SIZE    1
#define PJ_THREAD_ALLOCATE_STACK    0

/* The atomic queue is C++, used by Android backends only. */
#define PJ_HAS_ATOMIC_QUEUE         0

#endif  /* __PJ_COMPAT_OS_ZEPHYR_H__ */
