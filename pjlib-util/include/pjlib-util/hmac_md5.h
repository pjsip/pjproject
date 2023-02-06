/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJLIB_UTIL_HMAC_MD5_H__
#define __PJLIB_UTIL_HMAC_MD5_H__

/**
 * @file hmac_md5.h
 * @brief HMAC MD5 Message Authentication
 */

/**
 * @defgroup PJLIB_UTIL_ENCRYPTION Encryption Algorithms
 */

#include <pj/types.h>
#include <pjlib-util/md5.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJLIB_UTIL_HMAC_MD5 HMAC MD5 Message Authentication
 * @ingroup PJLIB_UTIL_ENCRYPTION
 * @{
 *
 * This module contains the implementation of HMAC: Keyed-Hashing 
 * for Message Authentication, as described in RFC 2104
 */

/**
 * The HMAC-MD5 context used in the incremental HMAC calculation.
 */
typedef struct pj_hmac_md5_context
{
    pj_md5_context  context;    /**< MD5 context            */
    pj_uint8_t      k_opad[64]; /**< opad xor-ed with key   */
} pj_hmac_md5_context;


/**
 * Calculate HMAC MD5 digest for the specified input and key.
 *
 * @param input         Pointer to the input stream.
 * @param input_len     Length of input stream in bytes.
 * @param key           Pointer to the authentication key.
 * @param key_len       Length of the authentication key.
 * @param digest        Buffer to be filled with HMAC MD5 digest.
 */
PJ_DECL(void) pj_hmac_md5(const pj_uint8_t *input, unsigned input_len, 
                          const pj_uint8_t *key, unsigned key_len, 
                          pj_uint8_t digest[16]);


/**
 * Initiate HMAC-MD5 context for incremental hashing.
 *
 * @param hctx          HMAC-MD5 context.
 * @param key           Pointer to the authentication key.
 * @param key_len       Length of the authentication key.
 */
PJ_DECL(void) pj_hmac_md5_init(pj_hmac_md5_context *hctx, 
                               const pj_uint8_t *key, unsigned key_len);

/**
 * Append string to the message.
 *
 * @param hctx          HMAC-MD5 context.
 * @param input         Pointer to the input stream.
 * @param input_len     Length of input stream in bytes.
 */
PJ_DECL(void) pj_hmac_md5_update(pj_hmac_md5_context *hctx,
                                 const pj_uint8_t *input, 
                                 unsigned input_len);

/**
 * Finish the message and return the digest. 
 *
 * @param hctx          HMAC-MD5 context.
 * @param digest        Buffer to be filled with HMAC MD5 digest.
 */
PJ_DECL(void) pj_hmac_md5_final(pj_hmac_md5_context *hctx,
                                pj_uint8_t digest[16]);

/**
 * @}
 */

PJ_END_DECL


#endif  /* __PJLIB_UTIL_HMAC_MD5_H__ */


