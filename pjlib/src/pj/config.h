/* $Header: /pjproject/pjlib/src/pj/config.h 7     5/28/05 11:00a Bennylp $ */
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

#ifndef __PJ_CONFIG_H__
#define __PJ_CONFIG_H__

extern const char *PJ_VERSION;

#ifndef PJ_HAS_SYS_TYPES_H   
#  define PJ_HAS_SYS_TYPES_H	    1
#endif
#ifndef PJ_HAS_STDDEF_H	
#  define PJ_HAS_STDDEF_H	    1
#endif

#ifndef PJ_HAS_ASSERT_H
#  define PJ_HAS_ASSERT_H	    1
#endif

#ifndef PJ_FUNCTIONS_ARE_INLINED
#  define PJ_FUNCTIONS_ARE_INLINED  0
#endif

#ifndef PJ_POOL_DEBUG
#  define PJ_POOL_DEBUG		    0
#endif

#if PJ_HAS_SYS_TYPES_H
# include <sys/types.h>
#endif

#if PJ_HAS_STDDEF_H
# include <stddef.h>
#endif

#if PJ_HAS_ASSERT_H
# include <assert.h>
#endif

/*
 * Threading.
 */
#ifndef PJ_HAS_THREADS
#  define PJ_HAS_THREADS	    (1)
#endif


/*
 * Win32 specific.
 */
#ifdef WIN32
#  define PJ_WIN32		    1
#  define PJ_WIN32_WINNT	    0x0400
#endif

/*
 * Linux
 */
#ifdef LINUX
#  define PJ_LINUX		    1
#endif

/*
 * Default has high resolution timer (PJ_HAS_HIGH_RES_TIMER)
 * (Otherwise pjtest will fail to compile).
 */
#ifndef PJ_HAS_HIGH_RES_TIMER
#  define PJ_HAS_HIGH_RES_TIMER	    1
#endif

/*
 * Default set to Pentium (PJ_HAS_PENTIUM) to get Pentium rdtsc working.
 */
#if !defined(PJ_HAS_PENTIUM)
#  define PJ_HAS_PENTIUM	    1
#endif

/*
 * Default also set to little endian.
 */
#if !defined(PJ_IS_LITTLE_ENDIAN)
#  define PJ_IS_LITTLE_ENDIAN	1
#  define PJ_IS_BIG_ENDIAN	0
#elif defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN != 0
#  define PJ_IS_BIG_ENDIAN	0
#else
#  define PJ_IS_BIG_ENDIAN	1
#  define PJ_IS_LITTLE_ENDIAN	0
#endif


/*
 * If not specified, TCP is by default disabled.
 * Disabling TCP will reduce the footprint slightly.
 */
#ifndef PJ_HAS_TCP
#  define PJ_HAS_TCP		0
#endif

/*
 * I/O Queue uses IOCP on WinNT, and fallback to select() on others
 */
#if defined(PJ_WIN32) && defined(PJ_WIN32_WINNT) && PJ_WIN32_WINNT >= 0x0400
#  define PJ_IOQUEUE_USE_WIN32_IOCP (0)
#  define PJ_IOQUEUE_USE_SELECT     (1)
#else
#  define PJ_IOQUEUE_USE_SELECT	    (1)
#  define PJ_IOQUEUE_USE_WIN32_IOCP (0)
#endif

/*
 * Max hostname.
 */
#ifndef PJ_MAX_HOSTNAME
#  define PJ_MAX_HOSTNAME	    (80)
#endif

/**
 * Constants for declaring the maximum handles that can be supported by
 * a single IOQ framework. This constant might not be relevant to the 
 * underlying I/O queue impelementation, but still, developers should be 
 * aware of this constant, to make sure that the program will not break when
 * the underlying implementation changes.
 *
 * For implementation based on select(), the value here will be used as the
 * maximum number of socket handles passed to select() (i.e. FD_SETSIZE will 
 * be set to this value).
 */
#ifndef PJ_IOQUEUE_MAX_HANDLES
#  define PJ_IOQUEUE_MAX_HANDLES    (64)
#endif


/*
 * TODO information
 */
#ifndef PJ_TODO
#  define PJ_TODO(id)	    TODO___##id:
#endif


#endif	/* __PJ_CONFIG_H__ */

