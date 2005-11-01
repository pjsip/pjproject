/* $Header: /pjproject-0.3/pjlib/src/pj/compat/vsprintf.h 1     9/17/05 10:36a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/vsprintf.h $
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_VSPRINTF_H__
#define __PJ_COMPAT_VSPRINTF_H__

/**
 * @file vsprintf.h
 * @brief Provides vsprintf and vsnprintf function.
 */

#if defined(PJ_HAS_STDIO_H) && PJ_HAS_STDIO_H != 0
#  include <stdio.h>
#endif

#if defined(_MSC_VER)
#  define vsnprintf	_vsnprintf	
#endif

#define pj_vsnprintf	vsnprintf

#endif	/* __PJ_COMPAT_VSPRINTF_H__ */
