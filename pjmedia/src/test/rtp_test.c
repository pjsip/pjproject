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
#include "test.h"
#include <pjmedia/errno.h>
#include <pjmedia/rtp.h>

#define THIS_FILE   "rtp_test.c"

/*
 * A packet shorter than the RTP header must be rejected before any of it
 * is read. Each packet is placed at the end of the array, so a read past
 * it is out of bounds, and its bytes don't carry a valid RTP version.
 */
int rtp_test(void)
{
    pjmedia_rtp_session ses;
    const pjmedia_rtp_hdr *hdr;
    pjmedia_rtp_dec_hdr dec_hdr;
    const void *rtphdr, *payload;
    unsigned payloadlen;
    pj_uint8_t pkt[sizeof(pjmedia_rtp_hdr)];
    int hdrlen, len;
    pj_status_t status;

    PJ_LOG(3, (THIS_FILE, "  RTP decode short packet test"));

    pjmedia_rtp_session_init(&ses, 4, 0x12345678);
    hdrlen = 0;
    status = pjmedia_rtp_encode_rtp(&ses, 4, 0, 160, 160, &rtphdr, &hdrlen);
    if (status != PJ_SUCCESS || hdrlen != (int)sizeof(pkt))
        return -10;

    /* A packet with only the header is valid */
    pj_memcpy(pkt, rtphdr, hdrlen);
    status = pjmedia_rtp_decode_rtp2(&ses, pkt, hdrlen, &hdr, &dec_hdr,
                                     &payload, &payloadlen);
    if (status != PJ_SUCCESS || payloadlen != 0)
        return -20;

    pj_bzero(pkt, sizeof(pkt));
    for (len = 0; len < hdrlen; ++len) {
        status = pjmedia_rtp_decode_rtp2(&ses, pkt + sizeof(pkt) - len, len,
                                         &hdr, &dec_hdr, &payload,
                                         &payloadlen);
        if (status != PJMEDIA_RTP_EINLEN) {
            PJ_LOG(3, (THIS_FILE, "    error: %d-byte packet: status %d",
                       len, status));
            return -30;
        }
    }

    return 0;
}
