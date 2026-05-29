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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pjmedia.h>
#include <pjlib.h>

#include <pjmedia/sdp.h>
#include <pjmedia/sdp_neg.h>

#define kMinInputLength 10
#define kMaxInputLength 5120

pj_pool_factory *mem;

/* Hardcoded local capability SDP used in negotiation paths */
static const char LOCAL_CAP_SDP[] =
    "v=0\r\n"
    "o=- 1234 1 IN IP4 127.0.0.1\r\n"
    "s=pjmedia\r\n"
    "c=IN IP4 127.0.0.1\r\n"
    "t=0 0\r\n"
    "m=audio 4000 RTP/AVP 0 8 9 101\r\n"
    "a=rtpmap:0 PCMU/8000\r\n"
    "a=rtpmap:8 PCMA/8000\r\n"
    "a=rtpmap:9 G722/8000\r\n"
    "a=rtpmap:101 telephone-event/8000\r\n"
    "a=fmtp:101 0-15\r\n"
    "m=video 4002 RTP/AVP 96 97\r\n"
    "a=rtpmap:96 H264/90000\r\n"
    "a=rtpmap:97 VP8/90000\r\n";

/* Fuzz sdp negotiation code */
static void sdp_neg_fuzz(const char *data, size_t size)
{
    pj_pool_t *pool;
    pjmedia_sdp_session *local_cap, *remote_sdp, *local_sdp2;
    pjmedia_sdp_neg *neg;
    char *lbuf, *rbuf, *lbuf2;
    size_t lcap_len = sizeof(LOCAL_CAP_SDP) - 1;

    pool = pj_pool_create(mem, "sdp_neg_fuzz", 8000, 4000, NULL);

    /* Answerer logic */
    lbuf = (char *)pj_pool_alloc(pool, lcap_len + 1);
    pj_memcpy(lbuf, LOCAL_CAP_SDP, lcap_len + 1);
    rbuf = (char *)pj_pool_alloc(pool, size + 1);
    pj_memcpy(rbuf, data, size);
    rbuf[size] = '\0';

    if (pjmedia_sdp_parse(pool, lbuf, lcap_len, &local_cap) == PJ_SUCCESS &&
        pjmedia_sdp_parse(pool, rbuf, size, &remote_sdp) == PJ_SUCCESS) {

        if (pjmedia_sdp_neg_create_w_remote_offer(pool, local_cap, remote_sdp,
                                                   &neg) == PJ_SUCCESS) {
            pjmedia_sdp_neg_set_prefer_remote_codec_order(neg, PJ_FALSE);
            pjmedia_sdp_neg_set_answer_multiple_codecs(neg, PJ_FALSE);

            if (pjmedia_sdp_neg_negotiate(pool, neg, 0) == PJ_SUCCESS) {
                const pjmedia_sdp_session *active_l, *active_r;
                pjmedia_sdp_neg_get_active_local(neg, &active_l);
                pjmedia_sdp_neg_get_active_remote(neg, &active_r);
                pjmedia_sdp_neg_was_answer_remote(neg);

                lbuf2 = (char *)pj_pool_alloc(pool, lcap_len + 1);
                pj_memcpy(lbuf2, LOCAL_CAP_SDP, lcap_len + 1);
                if (pjmedia_sdp_parse(pool, lbuf2, lcap_len, &local_sdp2) == PJ_SUCCESS) {
                    if (pjmedia_sdp_neg_modify_local_offer2(pool, neg, 0,
                                                             local_sdp2) == PJ_SUCCESS)
                        pjmedia_sdp_neg_cancel_offer(neg);
                }
            }
        }
    }

    /* Offerer logic */
    lbuf = (char *)pj_pool_alloc(pool, lcap_len + 1);
    pj_memcpy(lbuf, LOCAL_CAP_SDP, lcap_len + 1);
    rbuf = (char *)pj_pool_alloc(pool, size + 1);
    pj_memcpy(rbuf, data, size);
    rbuf[size] = '\0';

    if (pjmedia_sdp_parse(pool, lbuf, lcap_len, &local_cap) == PJ_SUCCESS) {
        if (pjmedia_sdp_neg_create_w_local_offer(pool, local_cap, &neg) == PJ_SUCCESS) {
            if (pjmedia_sdp_parse(pool, rbuf, size, &remote_sdp) == PJ_SUCCESS) {
                if (pjmedia_sdp_neg_set_remote_answer(pool, neg, remote_sdp) == PJ_SUCCESS)
                    pjmedia_sdp_neg_negotiate(pool, neg, 0);
            }
        }
    }

    /* Remote Offerer logic */
    rbuf = (char *)pj_pool_alloc(pool, size + 1);
    pj_memcpy(rbuf, data, size);
    rbuf[size] = '\0';
    lbuf = (char *)pj_pool_alloc(pool, lcap_len + 1);
    pj_memcpy(lbuf, LOCAL_CAP_SDP, lcap_len + 1);

    if (pjmedia_sdp_parse(pool, rbuf, size, &remote_sdp) == PJ_SUCCESS) {
        if (pjmedia_sdp_neg_create_w_remote_offer(pool, NULL, remote_sdp,
                                                   &neg) == PJ_SUCCESS) {
            if (pjmedia_sdp_parse(pool, lbuf, lcap_len, &local_cap) == PJ_SUCCESS) {
                if (pjmedia_sdp_neg_set_local_answer(pool, neg, local_cap) == PJ_SUCCESS)
                    pjmedia_sdp_neg_negotiate(pool, neg, 0);
            }
        }
    }

    pj_pool_release(pool);
}

int sdp_parser(char *DataFx,size_t Size){

    int ret = 0;
	pj_pool_t *pool;
    pjmedia_sdp_session *sdp;
    pj_status_t status;

    pool = pj_pool_create(mem, "sdp_neg_test", 4000, 4000, NULL);

    status = pjmedia_sdp_parse(pool, DataFx, Size,&sdp);

    if (status != PJ_SUCCESS){
        ret = 1;
        goto end;
    }

    status = pjmedia_sdp_validate(sdp);

end:
    pj_pool_release(pool);

    return ret;
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{

    if (Size < kMinInputLength || Size > kMaxInputLength){
        return 1;
    }

    /* Add Extra byte */
    char *DataFx;
    DataFx = (char *)calloc((Size+1), sizeof(char));
    memcpy((void *)DataFx, (void *)Data, Size);

    int ret = 0;
    pj_caching_pool caching_pool;
    pj_pool_t *pool;

    pj_init();
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    pool = pj_pool_create(&caching_pool.factory, "test", 1000, 512, NULL);

    pj_log_set_level(0);

    mem = &caching_pool.factory;

    pjmedia_event_mgr_create(pool, 0, NULL);

    /* Fuzz sdp parsing */
    ret = sdp_parser(DataFx, Size);

    /* Fuzz sdp negotiation */
    sdp_neg_fuzz(DataFx, Size);

    /* Free object */
    pjmedia_event_mgr_destroy(pjmedia_event_mgr_instance());
    pj_pool_release(pool);
    pj_caching_pool_destroy(&caching_pool);

    free(DataFx);
    return ret;
}
