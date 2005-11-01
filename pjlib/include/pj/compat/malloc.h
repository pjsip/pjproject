/* $Header: /pjproject-0.3/pjlib/src/pj/compat/malloc.h 2     9/17/05 10:37a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/malloc.h $
 * 
 * 2     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 * 1     9/16/05 10:02p Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_MALLOC_H__
#define __PJ_COMPAT_MALLOC_H__

/**
 * @file malloc.h
 * @brief Provides malloc() and free() functions.
 */

#if defined(PJ_HAS_MALLOC_H) && PJ_HAS_MALLOC_H != 0
#  include <malloc.h>
#elif defined(PJ_HAS_STDLIB_H) && PJ_HAS_STDLIB_H != 0
#  include <stdlib.h>
#endif

#endif	/* __PJ_COMPAT_MALLOC_H__ */
