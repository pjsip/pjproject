/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include <pjsip/sip_auth_aka.h>
#include <pjsip/sip_errno.h>
#include <pjlib-util/base64.h>
#include <pjlib-util/md5.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#include "../../third_party/milenage/milenage.h"

/*
 * Create MD5-AKA1 digest response.
 */
PJ_DEF(pj_status_t) pjsip_auth_create_akav1( pj_pool_t *pool,
					     const pjsip_digest_challenge*chal,
					     const pjsip_cred_info *cred,
					     const pj_str_t *method,
					     pjsip_digest_credential *auth)
{
    pj_str_t nonce_bin;
    pj_uint8_t *chal_rand, *chal_autn, *chal_mac;
    pj_uint8_t res[PJSIP_AKA_RESLEN];
    pj_uint8_t ck[PJSIP_AKA_CKLEN];
    pj_uint8_t ik[PJSIP_AKA_IKLEN];
    pj_uint8_t ak[PJSIP_AKA_AKLEN];
    pj_uint8_t sqn[PJSIP_AKA_AUTNLEN];
    pj_uint8_t xmac[PJSIP_AKA_MACLEN];
    pjsip_cred_info aka_cred;
    int i;
    pj_status_t status;

    /* Check the algorithm is supported. */
    if (pj_stricmp2(&chal->algorithm, "md5") == 0) {
	pjsip_auth_create_digest(&auth->response, &auth->nonce, &auth->nc,
				 &auth->cnonce, &auth->qop, &auth->uri,
				 &auth->realm, cred, method);
	return PJ_SUCCESS;

    } else if (pj_stricmp2(&chal->algorithm, "AKAv1-MD5") != 0) {
	/* Unsupported algorithm */
	return PJSIP_EINVALIDALGORITHM;
    }

    /* Decode nonce */
    nonce_bin.slen = PJ_BASE64_TO_BASE256_LEN(chal->nonce.slen);
    nonce_bin.ptr = pj_pool_alloc(pool, nonce_bin.slen + 1);
    status = pj_base64_decode(&chal->nonce, (pj_uint8_t*)nonce_bin.ptr,
			      &nonce_bin.slen);
    if (status != PJ_SUCCESS)
	return PJSIP_EAUTHINNONCE;

    if (nonce_bin.slen < PJSIP_AKA_RANDLEN + PJSIP_AKA_AUTNLEN + PJSIP_AKA_MACLEN)
	return PJSIP_EAUTHINNONCE;

    /* Get RAND, AUTN, and MAC */
    chal_rand = (pj_uint8_t*) (nonce_bin.ptr + 0);
    chal_autn = (pj_uint8_t*) (nonce_bin.ptr + PJSIP_AKA_RANDLEN);
    chal_mac =  (pj_uint8_t*) (nonce_bin.ptr + PJSIP_AKA_RANDLEN + PJSIP_AKA_AUTNLEN);

    /* Verify credential */
    PJ_ASSERT_RETURN(cred->ext.aka.k.slen == PJSIP_AKA_KLEN, PJSIP_EAUTHINAKACRED);
    PJ_ASSERT_RETURN(cred->ext.aka.op.slen == PJSIP_AKA_OPLEN, PJSIP_EAUTHINAKACRED);

    /* Given key K and random challenge RAND, compute response RES,
     * confidentiality key CK, integrity key IK and anonymity key AK.
     */
    f2345((pj_uint8_t*)cred->ext.aka.k.ptr, 
	  chal_rand, 
	  res, ck, ik, ak, 
          (pj_uint8_t*)cred->ext.aka.op.ptr);

    /* Compute sequence number SQN */
    for (i=0; i<PJSIP_AKA_AUTNLEN; ++i)
	sqn[i] = (pj_uint8_t) (chal_autn[i] ^ ak[i]);

    /* Compute XMAC */
    f1((pj_uint8_t*)cred->ext.aka.k.ptr, chal_rand, sqn,
       (pj_uint8_t*)cred->ext.aka.amf.ptr, xmac, 
       (pj_uint8_t*)cred->ext.aka.op.ptr);

    /* Verify MAC in the challenge */
    if (pj_memcmp(chal_mac, xmac, PJSIP_AKA_MACLEN) != 0) {
	return PJSIP_EAUTHINNONCE;
    }

    /* Build a temporary credential info to create MD5 digest, using
     * "res" as the password. 
     */
    pj_memcpy(&aka_cred, cred, sizeof(aka_cred));
    aka_cred.data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    aka_cred.data.ptr = (char*)res;
    aka_cred.data.slen = PJSIP_AKA_RESLEN;

    /* Create a response */
    pjsip_auth_create_digest(&auth->response, &chal->nonce, 
			     &auth->nc, &auth->cnonce, &auth->qop, &auth->uri,
			     &chal->realm, &aka_cred, method);

    /* Done */
    return PJ_SUCCESS;
}

