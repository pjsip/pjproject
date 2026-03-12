/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjmedia/transport_srtp.h>
#include <pjmedia/transport_loop.h>
#include <pjmedia/sdp.h>

#define kMinInputLength 100
#define kMaxInputLength 5120

static pj_pool_factory *mem;

static void fuzz_sdes(const uint8_t *data, size_t size)
{
    pjmedia_endpt *endpt = NULL;
    pj_pool_t *pool = NULL, *sdp_pool = NULL;
    pj_status_t status;
    pjmedia_transport *loop_tp = NULL, *srtp_tp = NULL;
    pjmedia_transport *loop_tp2 = NULL, *srtp_tp2 = NULL;
    pjmedia_srtp_setting srtp_opt;
    pjmedia_sdp_session *local_sdp = NULL, *remote_sdp = NULL;
    char *sdp_str = NULL;

    status = pjmedia_endpt_create(mem, NULL, 1, &endpt);
    if (status != PJ_SUCCESS)
        return;

    status = pjmedia_srtp_init_lib(endpt);
    if (status != PJ_SUCCESS)
        goto cleanup;

    pool = pj_pool_create(mem, "sdes", 2000, 1000, NULL);
    if (!pool)
        goto cleanup;

    sdp_pool = pj_pool_create(mem, "sdp", 2000, 1000, NULL);
    if (!sdp_pool)
        goto cleanup;

    status = pjmedia_transport_loop_create(endpt, &loop_tp);
    if (status != PJ_SUCCESS)
        goto cleanup;

    pjmedia_srtp_setting_default(&srtp_opt);
    srtp_opt.use = PJMEDIA_SRTP_OPTIONAL;

    status = pjmedia_transport_srtp_create(endpt, loop_tp, &srtp_opt, &srtp_tp);
    if (status != PJ_SUCCESS)
        goto cleanup;

    sdp_str = (char*)pj_pool_alloc(pool, size + 1);
    if (!sdp_str)
        goto cleanup;

    pj_memcpy(sdp_str, data, size);
    sdp_str[size] = '\0';

    if (size < 3 || sdp_str[0] != 'v' || sdp_str[1] != '=')
        goto cleanup;

    status = pjmedia_sdp_parse(sdp_pool, sdp_str, size, &remote_sdp);
    if (status != PJ_SUCCESS)
        goto cleanup;

    status = pjmedia_sdp_validate(remote_sdp);
    if (status != PJ_SUCCESS)
        goto cleanup;

    /* Create minimal local SDP */
    local_sdp = PJ_POOL_ZALLOC_T(sdp_pool, pjmedia_sdp_session);
    local_sdp->origin.user = pj_str("pjsip");
    local_sdp->origin.version = local_sdp->origin.id = 1;
    local_sdp->origin.net_type = pj_str("IN");
    local_sdp->origin.addr_type = pj_str("IP4");
    local_sdp->origin.addr = pj_str("127.0.0.1");
    local_sdp->name = pj_str("pjmedia");
    local_sdp->time.start = 0;
    local_sdp->time.stop = 0;

    if (remote_sdp->media_count > 0) {
        pjmedia_sdp_media *m;

        m = PJ_POOL_ZALLOC_T(sdp_pool, pjmedia_sdp_media);
        m->desc.media = pj_str("audio");
        m->desc.port = 4000;
        m->desc.port_count = 1;
        m->desc.transport = pj_str("RTP/AVP");
        m->desc.fmt_count = 1;
        m->desc.fmt[0] = pj_str("0");

        local_sdp->media[local_sdp->media_count++] = m;

        status = pjmedia_transport_media_create(srtp_tp, sdp_pool, 0,
                                                remote_sdp, 0);
        if (status == PJ_SUCCESS)
            pjmedia_transport_encode_sdp(srtp_tp, sdp_pool,
                                         local_sdp, remote_sdp, 0);

        status = pjmedia_transport_loop_create(endpt, &loop_tp2);
        if (status != PJ_SUCCESS)
            goto cleanup;

        status = pjmedia_transport_srtp_create(endpt, loop_tp2, &srtp_opt,
                                               &srtp_tp2);
        if (status != PJ_SUCCESS)
            goto cleanup;

        status = pjmedia_transport_media_create(srtp_tp2, sdp_pool, 0,
                                                NULL, 0);
        if (status == PJ_SUCCESS) {
            pjmedia_transport_encode_sdp(srtp_tp2, sdp_pool,
                                         local_sdp, NULL, 0);
            pjmedia_transport_media_start(srtp_tp2, sdp_pool,
                                          local_sdp, remote_sdp, 0);
        }
    }

cleanup:
    if (srtp_tp2)
        pjmedia_transport_close(srtp_tp2);
    else if (loop_tp2)
        pjmedia_transport_close(loop_tp2);
    if (srtp_tp)
        pjmedia_transport_close(srtp_tp);
    else if (loop_tp)
        pjmedia_transport_close(loop_tp);
    if (sdp_pool)
        pj_pool_release(sdp_pool);
    if (pool)
        pj_pool_release(pool);
    if (endpt)
        pjmedia_endpt_destroy(endpt);
}

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    pj_caching_pool caching_pool;
    pj_status_t status;

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    status = pj_init();
    if (status != PJ_SUCCESS)
        return 0;

    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    pj_log_set_level(0);

    mem = &caching_pool.factory;

    /* Fuzz SRTP SDES */
    fuzz_sdes(Data, Size);

    pj_caching_pool_destroy(&caching_pool);

    return 0;
}
