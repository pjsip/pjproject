/* 
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJ_RAND_H__
#define __PJ_RAND_H__

/**
 * @file rand.h
 * @brief Random Number Generator.
 */

#include <pj/types.h>

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
 * @param seed      Seed value.
 */
PJ_DECL(void) pj_srand(unsigned int seed);


/**
 * Generate random integer with 32bit randomness.
 *
 * @return a random integer.
 */
PJ_DECL(int) pj_rand(void);


/**
 * Fill a buffer from the operating system's cryptographically secure
 * random number generator, for key material and other values that must
 * be unpredictable. Unlike pj_rand(), this never falls back to a weaker
 * generator: when the OS has no secure generator the function returns
 * PJ_ENOTSUP, and when the generator fails it returns the error and
 * leaves the buffer unspecified.
 *
 * The function does not use the SSL library, so it is available in
 * builds without SSL. The SSL library's generator is available separately
 * as pj_ssl_rand_bytes(); the application chooses which one to use.
 *
 * The OS generators used are:
 *  - Windows: BCryptGenRandom() with the system preferred RNG when
 *    targeting Windows 7 or later, otherwise RtlGenRandom().
 *  - Linux, Android (API 28 and later), FreeBSD (12 and later) and other
 *    systems with getrandom(): getrandom(), or /dev/urandom when the
 *    kernel does not provide the system call or a seccomp filter denies
 *    it.
 *  - macOS, iOS, the other BSDs and older Android: arc4random_buf().
 *  - Other POSIX systems, including Linux with a C library that lacks
 *    getrandom(): /dev/urandom.
 *
 * A missing /dev/urandom means PJ_ENOTSUP only on systems other than
 * Linux that lack getrandom(); elsewhere the kernel has a generator, so
 * it is reported as an error.
 *
 * @param buf       Buffer to fill. May be NULL when \a len is zero.
 * @param len       Number of bytes to fill.
 *
 * @return          PJ_SUCCESS, PJ_ENOTSUP when there is no OS secure
 *                  generator, or the generator's error.
 */
PJ_DECL(pj_status_t) pj_csrand_bytes(void *buf, pj_size_t len);


/** @} */


PJ_END_DECL


#endif  /* __PJ_RAND_H__ */

