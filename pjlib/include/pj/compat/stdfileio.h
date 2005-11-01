/* $Id$
 *
 */
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/stdfileio.h $
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_STDFILEIO_H__
#define __PJ_COMPAT_STDFILEIO_H__

/**
 * @file stdfileio.h
 * @brief Compatibility for ANSI file I/O like fputs, fflush, etc.
 */

#if defined(PJ_HAS_STDIO_H) && PJ_HAS_STDIO_H != 0
#  include <stdio.h>
#endif

#endif	/* __PJ_COMPAT_STDFILEIO_H__ */
