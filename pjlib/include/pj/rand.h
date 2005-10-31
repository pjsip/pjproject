/* $Header: /pjproject-0.3/pjlib/include/pj/rand.h 3     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/include/pj/rand.h $
 * 
 * 3     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 2     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 * 1     9/15/05 8:40p Bennylp
 * Created.
 */
#ifndef __PJ_RAND_H__
#define __PJ_RAND_H__

/**
 * @file rand.h
 * @brief Random Number Generator.
 */

#include <pj/config.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJ_RAND Random Number Generator
 * @ingroup PJ_MISC
 * @{
 * This module contains functions for generating random numbers.
 * This abstraction is needed not only because not all platforms have
 * \a rand() and \a srand(), but also on some platforms \a rand()
 * only has 16-bit randomness, which is not good enough.
 */

/**
 * Put in seed to random number generator.
 *
 * @param seed	    Seed value.
 */
PJ_DECL(void) pj_srand(unsigned int seed);


/**
 * Generate random integer with 32bit randomness.
 *
 * @return a random integer.
 */
PJ_DECL(int) pj_rand(void);


/** @} */


PJ_END_DECL


#endif	/* __PJ_RAND_H__ */

