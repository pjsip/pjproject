/* $Id$
 *
 */
/* $Log: /pjproject-0.3/pjlib/include/pj/compat/sprintf.h $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_SPRINTF_H__
#define __PJ_COMPAT_SPRINTF_H__

/**
 * @file sprintf.h
 * @brief Provides sprintf() and snprintf() functions.
 */

#if defined(PJ_HAS_STDIO_H) && PJ_HAS_STDIO_H != 0
#  include <stdio.h>
#endif

#if defined(_MSC_VER)
#  define snprintf	_snprintf
#endif

#define pj_sprintf      sprintf
#define pj_snprintf	snprintf

#endif	/* __PJ_COMPAT_SPRINTF_H__ */
