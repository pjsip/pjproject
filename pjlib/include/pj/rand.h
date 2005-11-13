/* $Id$
 *
 */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
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

