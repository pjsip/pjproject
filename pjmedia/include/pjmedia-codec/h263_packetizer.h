/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_H263_PACKETIZER_H__
#define __PJMEDIA_H263_PACKETIZER_H__


/**
 * @file h263_packetizer.h
 * @brief Packetizes H.263 bitstream into RTP payload.
 */

#include <pj/errno.h>

PJ_BEGIN_DECL

/**
 * Find synchronization point (PSC, slice, GSBC, EOS, EOSBS) in H.263 
 * bitstream, in reversed manner.
 */
PJ_INLINE(pj_uint8_t*) pjmedia_h263_find_sync_point_rev(pj_uint8_t *data,
                                                        pj_size_t data_len)
{
    pj_uint8_t *p = data+data_len-1;

    while (p > data && *p && *(p+1))
        --p;

    if (p == data)
        return (data + data_len);
        
    return p;
}

/**
 * Generate an RTP payload from H.263 frame bitstream, in-place processing.
 */
PJ_INLINE(pj_status_t) pjmedia_h263_packetize(pj_uint8_t *buf,
                                              pj_size_t buf_len,
                                              unsigned *pos,
                                              int max_payload_len,
                                              const pj_uint8_t **payload,
                                              pj_size_t *payload_len)
{
    pj_uint8_t *p, *p_end;

    p = buf + *pos;
    p_end = buf + buf_len;

    /* Put two octets payload header */
    if ((p_end-p > 2) && *p==0 && *(p+1)==0) {
        /* The bitstream starts with synchronization point, just override
         * the two zero octets (sync point mark) for payload header.
         */
        *p = 0x04;
    } else {
        /* Not started in synchronization point, we will use two octets
         * preceeding the bitstream for payload header!
         */

	if (*pos < 2) {
	    /* Invalid H263 bitstream, it's not started with PSC */
	    return PJ_EINVAL;
	}

	p -= 2;
        *p = 0;
    }
    *(p+1) = 0;

    /* When bitstream truncation needed because of payload length/MTU 
     * limitation, try to use sync point for the payload boundary.
     */
    if (p_end-p > max_payload_len) {
        p_end = pjmedia_h263_find_sync_point_rev(p+2, max_payload_len-2);
    }

    *payload = p;
    *payload_len = p_end-p;
    *pos = p_end - buf;

    return PJ_SUCCESS;
}

/**
 * Append RTP payload to a H.263 picture bitstream.
 */
PJ_INLINE(pj_status_t) pjmedia_h263_unpacketize(const pj_uint8_t *payload,
                                                pj_size_t   payload_len,
                                                pj_uint8_t *bits,
                                                pj_size_t  *bits_len)
{
    pj_uint8_t P, V, PLEN;
    const pj_uint8_t *p=payload;
    pj_size_t max_len = *bits_len;

    P = *p & 0x04;
    V = *p & 0x02;
    PLEN = ((*p & 0x01) << 5) + ((*(p+1) & 0xF8)>>3);

    /* Get bitstream pointer */
    p += 2;
    if (V)
        p += 1; /* Skip VRC data */
    if (PLEN)
        p += PLEN; /* Skip extra picture header data */

    /* Get bitstream length */
    payload_len -= (p-payload);

    *bits_len = payload_len + (P?2:0);
    PJ_ASSERT_RETURN(max_len >= *bits_len, PJ_ETOOSMALL);

    /* Add two zero octets when payload flagged with sync point */
    if (P) {
        *bits++ = 0;
        *bits++ = 0;
    }

    /* Add the bitstream */
    pj_memcpy(bits, p, payload_len);

    return PJ_SUCCESS;
}


PJ_END_DECL


#endif	/* __PJMEDIA_H263_PACKETIZER_H__ */
