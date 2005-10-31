/* $Header: /pjproject-0.3/pjlib/src/pj/compat/assert.h 3     9/22/05 10:31a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/compat/assert.h $
 * 
 * 3     9/22/05 10:31a Bennylp
 * Moving all *.h files to include/.
 * 
 * 2     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 * 
 */
#ifndef __PJ_COMPAT_ASSERT_H__
#define __PJ_COMPAT_ASSERT_H__

/**
 * @file assert.h
 * @brief Provides assert() macro.
 */

#if defined(PJ_HAS_ASSERT_H) && PJ_HAS_ASSERT_H != 0
#  include <assert.h>

#elif defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL != 0
#  define assert(expr) do { \
			if (!(expr)) \
			  printk("!!ASSERTION FAILED: [%s:%d] \"" #expr "\"\n",\
				 __FILE__, __LINE__); \
		       } while (0)

#else
#  warning "assert() is not implemented"
#  define assert(expr)
#endif

#endif	/* __PJ_COMPAT_ASSERT_H__ */

