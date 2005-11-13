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
#if defined(PJ_WIN32) && PJ_WIN32!=0
#  include <pj/compat/os_win32.h>
#elif defined(PJ_LINUX) && PJ_LINUX!=0
#  include <pj/compat/os_linux.h>
#elif defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL!=0
#  include <pj/compat/os_linux_kernel.h>
#elif defined(PJ_PALMOS) && PJ_PALMOS!=0
#  include <pj/compat/os_palmos.h>
#elif defined(PJ_SUNOS) && PJ_SUNOS!=0
#  include <pj/compat/os_sunos.h>
#else
#  error "Please specify target os."
#endif


/********************************************************************
 * Target machine specific configuration.
 */
#if defined (PJ_M_I386) && PJ_M_I386 != 0
#   include <pj/compat/m_i386.h>
#elif defined (PJ_M_M68K) && PJ_M_M68K != 0
#   include <pj/compat/m_m68k.h>
#elif defined (PJ_M_ALPHA) && PJ_M_ALPHA != 0
#   include <pj/compat/m_alpha.h>
#elif defined (PJ_M_SPARC) && PJ_M_SPARC != 0
#   include <pj/compat/m_sparc.h>
#else
#  error "Please specify target machine."
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
 * Default: 800
 */
#ifndef PJ_LOG_MAX_SIZE
#  define PJ_LOG_MAX_SIZE	    800
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
 * Pool debugging.
 *
 * Default: 0
 */
#ifndef PJ_POOL_DEBUG
#  define PJ_POOL_DEBUG		    0
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
 * Default: 256
 */
#ifndef PJ_IOQUEUE_MAX_HANDLES
#  define PJ_IOQUEUE_MAX_HANDLES    (256)
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
#  define PJ_BEGIN_DECL		    extern "C" {
#  define PJ_END_DECL		    }
#else
#  define PJ_DECL(type)		    extern type
#  define PJ_DECL_NO_RETURN(type)   PJ_NORETURN type
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

