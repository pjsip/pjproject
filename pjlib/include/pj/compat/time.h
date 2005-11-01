/* $Header: /pjproject-0.3/pjlib/src/pj/compat/time.h 1     9/17/05 10:36a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/time.h $
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_TIME_H__
#define __PJ_COMPAT_TIME_H__

/**
 * @file time.h
 * @brief Provides ftime() and localtime() etc functions.
 */

#if defined(PJ_HAS_TIME_H) && PJ_HAS_TIME_H != 0
#  include <time.h>
#endif

#if defined(PJ_HAS_SYS_TIMEB_H) && PJ_HAS_SYS_TIMEB_H != 0
#  include <sys/timeb.h>
#endif


#endif	/* __PJ_COMPAT_TIME_H__ */
