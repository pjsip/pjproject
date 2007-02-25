/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#ifndef __PJLIB_UTIL_HMAC_SHA1_H__
#define __PJLIB_UTIL_HMAC_SHA1_H__

/**
 * @file hmac_sha1.h
 * @brief HMAC SHA1 Message Authentication
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJLIB_UTIL_HMAC_SHA1 HMAC SHA1 Message Authentication
 * @ingroup PJLIB_UTIL_ENCRYPTION
 * @{
 *
 * This module contains the implementation of HMAC: Keyed-Hashing 
 * for Message Authentication, as described in RFC 2104
 */


/**
 * Calculate HMAC SHA1 digest for the specified input and key.
 *
 * @param input		Pointer to the input stream.
 * @param input_len	Length of input stream in bytes.
 * @param key		Pointer to the authentication key.
 * @param key_len	Length of the authentication key.
 * @param digest	Buffer to be filled with HMAC SHA1 digest.
 */
PJ_DECL(void) pj_hmac_sha1(const pj_uint8_t *input, unsigned input_len, 
			   const pj_uint8_t *key, unsigned key_len, 
			   pj_uint8_t digest[20]);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJLIB_UTIL_HMAC_SHA1_H__ */


