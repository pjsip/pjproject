/* $Header: /pjproject-0.3/pjlib/src/pj/compat/stdarg.h 1     9/17/05 10:36a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/stdarg.h $
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_STDARG_H__
#define __PJ_COMPAT_STDARG_H__

/**
 * @file stdarg.h
 * @brief Provides stdarg functionality.
 */

#if defined(PJ_HAS_STDARG_H) && PJ_HAS_STDARG_H != 0
#  include <stdarg.h>
#endif

#endif	/* __PJ_COMPAT_STDARG_H__ */
