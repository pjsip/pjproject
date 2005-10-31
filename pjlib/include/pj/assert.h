/* $Header: /pjproject-0.3/pjlib/include/pj/assert.h 4     10/14/05 12:25a Bennylp $ */
#ifndef __PJ_ASSERT_H__
#define __PJ_ASSERT_H__

/**
 * @file assert.h
 * @brief Assertion macro pj_assert().
 */

#include <pj/config.h>
#include <pj/compat/assert.h>

/**
 * @defgroup pj_assert Assertion Macro
 * @ingroup PJ_MISC
 * @{
 *
 * Assertion and other helper macros for sanity checking.
 */

/**
 * @hideinitializer
 * Check during debug build that an expression is true. If the expression
 * computes to false during run-time, then the program will stop at the
 * offending statements.
 * For release build, this macro will not do anything.
 *
 * @param expr	    The expression to be evaluated.
 */
#define pj_assert(expr)   assert(expr)


/**
 * @hideinitializer
 * If #PJ_ENABLE_EXTRA_CHECK is declared and non-zero, then 
 * #PJ_ASSERT_RETURN macro will evaluate the expression in @a expr during
 * run-time. If the expression yields false, assertion will be triggered
 * and the current function will return with the specified return value.
 *
 * If #PJ_ENABLE_EXTRA_CHECK is not declared or is zero, then no run-time
 * checking will be performed. The macro simply evaluates to pj_assert(expr).
 */
#if defined(PJ_ENABLE_EXTRA_CHECK) && PJ_ENABLE_EXTRA_CHECK != 0
#   define PJ_ASSERT_RETURN(expr,retval)    \
	    do { \
		pj_assert(expr); \
		if (!(expr)) return retval; \
	    } while (0)
#else
#   define PJ_ASSERT_RETURN(expr,retval)    pj_assert(expr)
#endif

/** @} */

#endif	/* __PJ_ASSERT_H__ */

