/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
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
#ifndef __PJ_CONFIG_H__
#define __PJ_CONFIG_H__

/**
 * @file config.h
 * @brief PJLIB Main configuration settings.
 */

/********************************************************************
 * Include compiler specific configuration.
 */
#if defined(_MSC_VER)
#  include <pj/compat/cc_msvc.h>
#elif defined(__GNUC__)
#  include <pj/compat/cc_gcc.h>
#else
#  error "Unknown compiler."
#endif


/********************************************************************
 * Include target OS specific configuration.
 */
#if defined(PJ_AUTOCONF)
    /*
     * Autoconf
     */
#   include <pj/compat/os_auto.h>

#elif defined(PJ_WIN32_WINCE) || defined(_WIN32_WCE) || defined(UNDER_CE)
    /*
     * Windows CE
     */
#   undef PJ_WIN32_WINCE
#   define PJ_WIN32_WINCE   1
#   include <pj/compat/os_win32_wince.h>

    /* Also define Win32 */
#   define PJ_WIN32 1

#elif defined(PJ_WIN32) || defined(_WIN32) || defined(__WIN32__) || \
	defined(_WIN64) || defined(WIN32) || defined(__TOS_WIN__)
    /*
     * Win32
     */
#   undef PJ_WIN32
#   define PJ_WIN32 1
#   include <pj/compat/os_win32.h>

#elif defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL!=0
    /*
     * Linux kernel
     */
#  include <pj/compat/os_linux_kernel.h>

#elif defined(PJ_LINUX) || defined(linux) || defined(__linux)
    /*
     * Linux
     */
#   undef PJ_LINUX
#   define PJ_LINUX	    1
#   include <pj/compat/os_linux.h>

#elif defined(PJ_PALMOS) && PJ_PALMOS!=0
    /*
     * Palm
     */
#  include <pj/compat/os_palmos.h>

#elif defined(PJ_SUNOS) || defined(sun) || defined(__sun)
    /*
     * SunOS
     */
#   undef PJ_SUNOS
#   define PJ_SUNOS	    1
#   include <pj/compat/os_sunos.h>

#elif defined(PJ_DARWINOS) || defined(__MACOSX__)
    /*
     * MacOS X
     */
#   undef PJ_DARWINOS
#   define PJ_DARWINOS	    1
#   include <pj/compat/os_darwinos.h>

#elif defined(PJ_RTEMS) && PJ_RTEMS!=0
    /*
     * RTEMS
     */
#  include <pj/compat/os_rtems.h>
#else
#   error "Please specify target os."
#endif


/********************************************************************
 * Target machine specific configuration.
 */
#if defined(PJ_AUTOCONF)
    /*
     * Autoconf configured
     */
#include <pj/compat/m_auto.h>

#elif defined (PJ_M_I386) || defined(_i386_) || defined(i_386_) || \
	defined(_X86_) || defined(x86) || defined(__i386__) || \
	defined(__i386) || defined(_M_IX86) || defined(__I86__)
    /*
     * Generic i386 processor family, little-endian
     */
#   undef PJ_M_I386
#   define PJ_M_I386		1
#   define PJ_M_NAME		"i386"
#   define PJ_HAS_PENTIUM	1
#   define PJ_IS_LITTLE_ENDIAN	1
#   define PJ_IS_BIG_ENDIAN	0


#elif defined (PJ_M_X86_64) || defined(__amd64__) || defined(__amd64) || \
	defined(__x86_64__) || defined(__x86_64)
    /*
     * AMD 64bit processor, little endian
     */
#   undef PJ_M_X86_64
#   define PJ_M_X86_64		1
#   define PJ_M_NAME		"x86_64"
#   define PJ_HAS_PENTIUM	1
#   define PJ_IS_LITTLE_ENDIAN	1
#   define PJ_IS_BIG_ENDIAN	0

#elif defined(PJ_M_IA64) || defined(__ia64__) || defined(_IA64) || \
	defined(__IA64__) || defined( 	_M_IA64)
    /*
     * Intel IA64 processor, little endian
     */
#   undef PJ_M_IA64
#   define PJ_M_IA64		1
#   define PJ_M_NAME		"ia64"
#   define PJ_HAS_PENTIUM	1
#   define PJ_IS_LITTLE_ENDIAN	1
#   define PJ_IS_BIG_ENDIAN	0

#elif defined (PJ_M_M68K) && PJ_M_M68K != 0

    /*
     * Motorola m64k processor, little endian
     */
#   undef PJ_M_M68K
#   define PJ_M_M68K		1
#   define PJ_M_NAME		"m68k"
#   define PJ_HAS_PENTIUM	0
#   define PJ_IS_LITTLE_ENDIAN	1
#   define PJ_IS_BIG_ENDIAN	0


#elif defined (PJ_M_ALPHA) || defined (__alpha__) || defined (__alpha) || \
	defined (_M_ALPHA)
    /*
     * DEC Alpha processor, little endian
     */
#   undef PJ_M_ALPHA
#   define PJ_M_ALPHA		1
#   define PJ_M_NAME		"alpha"
#   define PJ_HAS_PENTIUM	0
#   define PJ_IS_LITTLE_ENDIAN	1
#   define PJ_IS_BIG_ENDIAN	0


#elif defined(PJ_M_MIPS) || defined(__mips__) || defined(__mips) || \
	defined(__MIPS__) || defined(MIPS) || defined(_MIPS_)
    /*
     * MIPS, default to little endian
     */
#   undef PJ_M_MIPS
#   define PJ_M_MIPS		1
#   define PJ_M_NAME		"mips"
#   define PJ_HAS_PENTIUM	0
#   if !defined(PJ_IS_LITTLE_ENDIAN) && !defined(PJ_IS_BIG_ENDIAN)
#   	define PJ_IS_LITTLE_ENDIAN	1
#   	define PJ_IS_BIG_ENDIAN		0
#   endif


#elif defined (PJ_M_SPARC) || defined( 	__sparc__) || defined(__sparc)
    /*
     * Sun Sparc, big endian
     */
#   undef PJ_M_SPARC
#   define PJ_M_SPARC		1
#   define PJ_M_NAME		"sparc"
#   define PJ_HAS_PENTIUM	0
#   define PJ_IS_LITTLE_ENDIAN	0
#   define PJ_IS_BIG_ENDIAN	1

#elif defined (PJ_M_ARMV4) || defined(ARM) || defined(_ARM_) ||  \
	defined(ARMV4) || defined(__arm__)
    /*
     * ARM, default to little endian
     */
#   undef PJ_M_ARMV4
#   define PJ_M_ARMV4		1
#   define PJ_M_NAME		"armv4"
#   define PJ_HAS_PENTIUM	0
#   if !defined(PJ_IS_LITTLE_ENDIAN) && !defined(PJ_IS_BIG_ENDIAN)
#	define PJ_IS_LITTLE_ENDIAN	1
#	define PJ_IS_BIG_ENDIAN		0
#   endif

#elif defined (PJ_M_POWERPC) || defined(__powerpc) || defined(__powerpc__) || \
	defined(__POWERPC__) || defined(__ppc__) || defined(_M_PPC) || \
	defined(_ARCH_PPC)
    /*
     * PowerPC, big endian
     */
#   undef PJ_M_POWERPC
#   define PJ_M_POWERPC		1
#   define PJ_M_NAME		"powerpc"
#   define PJ_HAS_PENTIUM	0
#   define PJ_IS_LITTLE_ENDIAN	0
#   define PJ_IS_BIG_ENDIAN	1

#else
#   error "Please specify target machine."
#endif

/* Include size_t definition. */
#include <pj/compat/size_t.h>

/* Include site/user specific configuration to control PJLIB features.
 * YOU MUST CREATE THIS FILE YOURSELF!!
 */
#include <pj/config_site.h>

/********************************************************************
 * PJLIB Features.
 */

/* Overrides for DOXYGEN */
#ifdef DOXYGEN
#   undef PJ_FUNCTIONS_ARE_INLINED
#   undef PJ_HAS_FLOATING_POINT
#   undef PJ_LOG_MAX_LEVEL
#   undef PJ_LOG_MAX_SIZE
#   undef PJ_LOG_USE_STACK_BUFFER
#   undef PJ_TERM_HAS_COLOR
#   undef PJ_POOL_DEBUG
#   undef PJ_HAS_TCP
#   undef PJ_MAX_HOSTNAME
#   undef PJ_IOQUEUE_MAX_HANDLES
#   undef FD_SETSIZE
#   undef PJ_HAS_SEMAPHORE
#   undef PJ_HAS_EVENT_OBJ
#   undef PJ_ENABLE_EXTRA_CHECK
#   undef PJ_EXCEPTION_USE_WIN32_SEH
#   undef PJ_HAS_ERROR_STRING
#endif

/**
 * @defgroup pj_config Build Configuration
 * @ingroup PJ
 * @{
 *
 * This section contains macros that can set during PJLIB build process
 * to controll various aspects of the library.
 *
 * <b>Note</b>: the values in this page does NOT necessarily reflect to the
 * macro values during the build process.
 */

/**
 * If this macro is set to 1, it will enable some debugging checking
 * in the library.
 *
 * Default: equal to (NOT NDEBUG).
 */
#ifndef PJ_DEBUG
#  ifndef NDEBUG
#    define PJ_DEBUG		    1
#  else
#    define PJ_DEBUG		    0
#  endif
#endif

/**
 * Expand functions in *_i.h header files as inline.
 *
 * Default: 0.
 */
#ifndef PJ_FUNCTIONS_ARE_INLINED
#  define PJ_FUNCTIONS_ARE_INLINED  0
#endif

/**
 * Use floating point computations in the library.
 *
 * Default: 1.
 */
#ifndef PJ_HAS_FLOATING_POINT
#  define PJ_HAS_FLOATING_POINT	    1
#endif

/**
 * Declare maximum logging level/verbosity. Lower number indicates higher
 * importance, with the highest importance has level zero. The least
 * important level is five in this implementation, but this can be extended
 * by supplying the appropriate implementation.
 *
 * The level conventions:
 *  - 0: fatal error
 *  - 1: error
 *  - 2: warning
 *  - 3: info
 *  - 4: debug
 *  - 5: trace
 *  - 6: more detailed trace
 *
 * Default: 4
 */
#ifndef PJ_LOG_MAX_LEVEL
#  define PJ_LOG_MAX_LEVEL   5
#endif

/**
 * Maximum message size that can be sent to output device for each call
 * to PJ_LOG(). If the message size is longer than this value, it will be cut.
 * This may affect the stack usage, depending whether PJ_LOG_USE_STACK_BUFFER
 * flag is set.
 *
 * Default: 2000
 */
#ifndef PJ_LOG_MAX_SIZE
#  define PJ_LOG_MAX_SIZE	    2000
#endif

/**
 * Log buffer.
 * Does the log get the buffer from the stack? (default is yes).
 * If the value is set to NO, then the buffer will be taken from static
 * buffer, which in this case will make the log function non-reentrant.
 *
 * Default: 1
 */
#ifndef PJ_LOG_USE_STACK_BUFFER
#  define PJ_LOG_USE_STACK_BUFFER   1
#endif


/**
 * Colorfull terminal (for logging etc).
 *
 * Default: 1
 */
#ifndef PJ_TERM_HAS_COLOR
#  define PJ_TERM_HAS_COLOR	    1
#endif


/**
 * If pool debugging is used, then each memory allocation from the pool
 * will call malloc(), and pool will release all memory chunks when it
 * is destroyed. This works better when memory verification programs
 * such as Rational Purify is used.
 *
 * Default: 0
 */
#ifndef PJ_POOL_DEBUG
#  define PJ_POOL_DEBUG		    0
#endif


/**
 * Specify this as \a stack_size argument in #pj_thread_create() to specify
 * that thread should use default stack size for the current platform.
 *
 * Default: 8192
 */
#ifndef PJ_THREAD_DEFAULT_STACK_SIZE 
#  define PJ_THREAD_DEFAULT_STACK_SIZE    8192
#endif


/**
 * Do we have alternate pool implementation?
 *
 * Default: 0
 */
#ifndef PJ_HAS_POOL_ALT_API
#   define PJ_HAS_POOL_ALT_API	    PJ_POOL_DEBUG
#endif


/**
 * \def PJ_HAS_TCP
 * Support TCP in the library.
 * Disabling TCP will reduce the footprint slightly (about 6KB).
 *
 * Default: 1
 */
#ifndef PJ_HAS_TCP
#  define PJ_HAS_TCP		    1
#endif

/**
 * Maximum hostname length.
 * Libraries sometimes needs to make copy of an address to stack buffer;
 * the value here affects the stack usage.
 *
 * Default: 128
 */
#ifndef PJ_MAX_HOSTNAME
#  define PJ_MAX_HOSTNAME	    (128)
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
 *
 * Default: if FD_SETSIZE is defined and the value is greather than 256,
 *          then it will be used.  Otherwise 256 (64 for WinCE).
 */
#ifndef PJ_IOQUEUE_MAX_HANDLES
#   if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
#	define PJ_IOQUEUE_MAX_HANDLES	(64)
#   else
#	define PJ_IOQUEUE_MAX_HANDLES	(256)
#   endif
#endif


/**
 * If PJ_IOQUEUE_HAS_SAFE_UNREG macro is defined, then ioqueue will do more
 * things to ensure thread safety of handle unregistration operation by
 * employing reference counter to each handle.
 *
 * In addition, the ioqueue will preallocate memory for the handles, 
 * according to the maximum number of handles that is specified during 
 * ioqueue creation.
 *
 * All applications would normally want this enabled, but you may disable
 * this if:
 *  - there is no dynamic unregistration to all ioqueues.
 *  - there is no threading, or there is no preemptive multitasking.
 *
 * Default: 1
 */
#ifndef PJ_IOQUEUE_HAS_SAFE_UNREG
#   define PJ_IOQUEUE_HAS_SAFE_UNREG	1
#endif


/**
 * When safe unregistration (PJ_IOQUEUE_HAS_SAFE_UNREG) is configured in
 * ioqueue, the PJ_IOQUEUE_KEY_FREE_DELAY macro specifies how long the
 * ioqueue key is kept in closing state before it can be reused.
 *
 * The value is in miliseconds.
 *
 * Default: 500 msec.
 */
#ifndef PJ_IOQUEUE_KEY_FREE_DELAY
#   define PJ_IOQUEUE_KEY_FREE_DELAY	500
#endif


/**
 * Overrides FD_SETSIZE so it is consistent throughout the library.
 * OS specific configuration header (compat/os_*) might have declared
 * FD_SETSIZE, thus we only set if it hasn't been declared.
 *
 * Default: #PJ_IOQUEUE_MAX_HANDLES
 */
#ifndef FD_SETSIZE
#  define FD_SETSIZE		    PJ_IOQUEUE_MAX_HANDLES
#endif

/**
 * Has semaphore functionality?
 *
 * Default: 1
 */
#ifndef PJ_HAS_SEMAPHORE
#  define PJ_HAS_SEMAPHORE	    1
#endif


/**
 * Event object (for synchronization, e.g. in Win32)
 *
 * Default: 1
 */
#ifndef PJ_HAS_EVENT_OBJ
#  define PJ_HAS_EVENT_OBJ	    1
#endif


/**
 * Enable library's extra check.
 * If this macro is enabled, #PJ_ASSERT_RETURN macro will expand to
 * run-time checking. If this macro is disabled, #PJ_ASSERT_RETURN
 * will simply evaluate to #pj_assert().
 *
 * You can disable this macro to reduce size, at the risk of crashes
 * if invalid value (e.g. NULL) is passed to the library.
 *
 * Default: 1
 */
#ifndef PJ_ENABLE_EXTRA_CHECK
#   define PJ_ENABLE_EXTRA_CHECK    1
#endif


/**
 * Enable name registration for exceptions with #pj_exception_id_alloc().
 * If this feature is enabled, then the library will keep track of
 * names associated with each exception ID requested by application via
 * #pj_exception_id_alloc().
 *
 * Disabling this macro will reduce the code and .bss size by a tad bit.
 * See also #PJ_MAX_EXCEPTION_ID.
 *
 * Default: 1
 */
#ifndef PJ_HAS_EXCEPTION_NAMES
#   define PJ_HAS_EXCEPTION_NAMES   1
#endif

/**
 * Maximum number of unique exception IDs that can be requested
 * with #pj_exception_id_alloc(). For each entry, a small record will
 * be allocated in the .bss segment.
 *
 * Default: 16
 */
#ifndef PJ_MAX_EXCEPTION_ID
#   define PJ_MAX_EXCEPTION_ID      16
#endif

/**
 * Should we use Windows Structured Exception Handling (SEH) for the
 * PJLIB exceptions.
 *
 * Default: 0
 */
#ifndef PJ_EXCEPTION_USE_WIN32_SEH
#  define PJ_EXCEPTION_USE_WIN32_SEH 0
#endif

/**
 * Should we attempt to use Pentium's rdtsc for high resolution
 * timestamp.
 *
 * Default: 0
 */
#ifndef PJ_TIMESTAMP_USE_RDTSC
#   define PJ_TIMESTAMP_USE_RDTSC   0
#endif

/**
 * Include error message string in the library (pj_strerror()).
 * This is very much desirable!
 *
 * Default: 1
 */
#ifndef PJ_HAS_ERROR_STRING
#   define PJ_HAS_ERROR_STRING	    1
#endif


/**
 * Include pj_stricmp_alnum() and pj_strnicmp_alnum(), i.e. custom
 * functions to compare alnum strings. On some systems, they're faster
 * then stricmp/strcasecmp, but they can be slower on other systems.
 * When disabled, pjlib will fallback to stricmp/strnicmp.
 * 
 * Default: 0
 */
#ifndef PJ_HAS_STRICMP_ALNUM
#   define PJ_HAS_STRICMP_ALNUM	    0
#endif


/** @} */

/********************************************************************
 * General macros.
 */

/**
 * @def PJ_INLINE(type)
 * @param type The return type of the function.
 * Expand the function as inline.
 */
#define PJ_INLINE(type)	  PJ_INLINE_SPECIFIER type

/**
 * @def PJ_DECL(type)
 * @param type The return type of the function.
 * Declare a function.
 */
/**
 * @def PJ_DECL_NO_RETURN(type)
 * @param type The return type of the function.
 * Declare a function that will not return.
 */
/**
 * @def PJ_BEGIN_DECL
 * Mark beginning of declaration section in a header file.
 */
/**
 * @def PJ_END_DECL
 * Mark end of declaration section in a header file.
 */
#ifdef __cplusplus
#  define PJ_DECL(type)		    type
#  define PJ_DECL_NO_RETURN(type)   type PJ_NORETURN
#  define PJ_IDECL_NO_RETURN(type)  PJ_INLINE(type) PJ_NORETURN
#  define PJ_BEGIN_DECL		    extern "C" {
#  define PJ_END_DECL		    }
#else
#  define PJ_DECL(type)		    extern type
#  define PJ_DECL_NO_RETURN(type)   PJ_NORETURN type
#  define PJ_IDECL_NO_RETURN(type)  PJ_NORETURN PJ_INLINE(type)
#  define PJ_BEGIN_DECL
#  define PJ_END_DECL
#endif

/**
 * @def PJ_DEF(type)
 * @param type The return type of the function.
 * Define a function.
 */
#define PJ_DEF(type)	  type

/**
 * @def PJ_EXPORT_SYMBOL(sym)
 * @param sym The symbol to export.
 * Export the specified symbol in compilation type that requires export
 * (e.g. Linux kernel).
 */
#ifdef __PJ_EXPORT_SYMBOL
#  define PJ_EXPORT_SYMBOL(sym)	    __PJ_EXPORT_SYMBOL(sym)
#else
#  define PJ_EXPORT_SYMBOL(sym)
#endif

/**
 * @def PJ_IDECL(type)
 * @param type  The function's return type.
 * Declare a function that may be expanded as inline.
 */
/**
 * @def PJ_IDEF(type)
 * @param type  The function's return type.
 * Define a function that may be expanded as inline.
 */

#if PJ_FUNCTIONS_ARE_INLINED
#  define PJ_IDECL(type)  PJ_INLINE(type)
#  define PJ_IDEF(type)   PJ_INLINE(type)
#else
#  define PJ_IDECL(type)  PJ_DECL(type)
#  define PJ_IDEF(type)   PJ_DEF(type)
#endif

/**
 * @def PJ_UNUSED_ARG(arg)
 * @param arg   The argument name.
 * PJ_UNUSED_ARG prevents warning about unused argument in a function.
 */
#define PJ_UNUSED_ARG(arg)  (void)arg

/**
 * @def PJ_TODO(id)
 * @param id    Any identifier that will be printed as TODO message.
 * PJ_TODO macro will display TODO message as warning during compilation.
 * Example: PJ_TODO(CLEAN_UP_ERROR);
 */
#ifndef PJ_TODO
#  define PJ_TODO(id)	    TODO___##id:
#endif

/**
 * Function attributes to inform that the function may throw exception.
 *
 * @param x     The exception list, enclosed in parenthesis.
 */
#define __pj_throw__(x)


/********************************************************************
 * Sanity Checks
 */
#ifndef PJ_HAS_HIGH_RES_TIMER
#  error "PJ_HAS_HIGH_RES_TIMER is not defined!"
#endif

#if !defined(PJ_HAS_PENTIUM)
#  error "PJ_HAS_PENTIUM is not defined!"
#endif

#if !defined(PJ_IS_LITTLE_ENDIAN)
#  error "PJ_IS_LITTLE_ENDIAN is not defined!"
#endif

#if !defined(PJ_IS_BIG_ENDIAN)
#  error "PJ_IS_BIG_ENDIAN is not defined!"
#endif

#if !defined(PJ_EMULATE_RWMUTEX)
#  error "PJ_EMULATE_RWMUTEX should be defined in compat/os_xx.h"
#endif

#if !defined(PJ_THREAD_SET_STACK_SIZE)
#  error "PJ_THREAD_SET_STACK_SIZE should be defined in compat/os_xx.h"
#endif

#if !defined(PJ_THREAD_ALLOCATE_STACK)
#  error "PJ_THREAD_ALLOCATE_STACK should be defined in compat/os_xx.h"
#endif

PJ_BEGIN_DECL

/**
 * PJLIB version string.
 */
extern const char *PJ_VERSION;

/**
 * Dump configuration to log with verbosity equal to info(3).
 */
PJ_DECL(void) pj_dump_config(void);

PJ_END_DECL


#endif	/* __PJ_CONFIG_H__ */

