/* $Header: /pjproject-0.3/pjlib/include/pj/compat/high_precision.h 3     10/29/05 11:51a Bennylp $ */
#ifndef __PJ_COMPAT_HIGH_PRECISION_H__
#define __PJ_COMPAT_HIGH_PRECISION_H__


#if defined(PJ_HAS_FLOATING_POINT) && PJ_HAS_FLOATING_POINT != 0
    /*
     * The first choice for high precision math is to use double.
     */
#   include <math.h>
    typedef double pj_highprec_t;

#   define PJ_HIGHPREC_VALUE_IS_ZERO(a)     (a==0)
#   define pj_highprec_mod(a,b)             (a=fmod(a,b))

#elif defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL != 0

#   include <asm/div64.h>
    
    typedef pj_int64_t pj_highprec_t;

#   define pj_highprec_div(a1,a2)   do_div(a1,a2)
#   define pj_highprec_mod(a1,a2)   (a1=do_mod(a1, a2))

    PJ_INLINE(pj_int64_t) do_mod( pj_int64_t a1, pj_int64_t a2)
    {
	return do_div(a1,a2);
    }
    
    
#elif defined(PJ_HAS_INT64) && PJ_HAS_INT64 != 0
    /*
     * Next choice is to use 64-bit arithmatics.
     */
    typedef pj_int64_t pj_highprec_t;

#else
    /*
     * Last, fallback to 32-bit arithmetics.
     */
    typedef pj_int32_t pj_highprec_t;

#endif

/**
 * @def pj_highprec_mul
 * pj_highprec_mul(a1, a2) - High Precision Multiplication
 * Multiply a1 and a2, and store the result in a1.
 */
#ifndef pj_highprec_mul
#   define pj_highprec_mul(a1,a2)   (a1 = a1 * a2)
#endif

/**
 * @def pj_highprec_div
 * pj_highprec_div(a1, a2) - High Precision Division
 * Divide a2 from a1, and store the result in a1.
 */
#ifndef pj_highprec_div
#   define pj_highprec_div(a1,a2)   (a1 = a1 / a2)
#endif

/**
 * @def pj_highprec_mod
 * pj_highprec_mod(a1, a2) - High Precision Modulus
 * Get the modulus a2 from a1, and store the result in a1.
 */
#ifndef pj_highprec_mod
#   define pj_highprec_mod(a1,a2)   (a1 = a1 % a2)
#endif


/**
 * @def PJ_HIGHPREC_VALUE_IS_ZERO(a)
 * Test if the specified high precision value is zero.
 */
#ifndef PJ_HIGHPREC_VALUE_IS_ZERO
#   define PJ_HIGHPREC_VALUE_IS_ZERO(a)     (a==0)
#endif


#endif	/* __PJ_COMPAT_HIGH_PRECISION_H__ */

