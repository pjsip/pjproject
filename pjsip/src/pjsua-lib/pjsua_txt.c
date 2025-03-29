/*
 * Copyright (C) 2024-2025 Teluu Inc. (http://www.teluu.com)
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

#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>

#if defined(PJSUA_MEDIA_HAS_PJMEDIA) && PJSUA_MEDIA_HAS_PJMEDIA != 0

#define THIS_FILE       "pjsua_txt.c"


/* Send text to remote. */
PJ_DEF(pj_status_t)
pjsua_call_send_text(pjsua_call_id call_id,
                     const pjsua_call_send_text_param *param)
{
    pjsua_call *call;
    pjsua_call_media *call_med;
    pjsip_dialog *dlg = NULL;
    int med_idx;
    pj_status_t status;

    PJ_ASSERT_RETURN(call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls &&
                     param, PJ_EINVAL);

    pj_log_push_indent();

    status = acquire_call("pjsua_call_send_text()", call_id, &call, &dlg);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Verify and normalize media index */
    if ((med_idx = param->med_idx) == -1) {
        unsigned i;

        for (i = 0; i < call->med_cnt; ++i) {
            if (call->media[i].type == PJMEDIA_TYPE_TEXT &&
                (call->media[i].dir & PJMEDIA_DIR_ENCODING))
            {
                med_idx = i;
                break;
            }
        }

        if (med_idx == -1) {
            status = PJ_ENOTFOUND;
            goto on_return;
        }
    }

    PJ_ASSERT_ON_FAIL(med_idx >= 0 && med_idx < (int)call->med_cnt,
                      {status = PJ_EINVAL; goto on_return;});

    /* Verify if the stream is transmitting text */
    call_med = &call->media[med_idx];
    if (call_med->type != PJMEDIA_TYPE_TEXT ||
        (call_med->dir & PJMEDIA_DIR_ENCODING) == 0 ||
        !call_med->strm.t.stream)
    {
        status = PJ_EINVALIDOP;
        goto on_return;
    }

    status = pjmedia_txt_stream_send_text(call_med->strm.t.stream,
                                          &param->text);

on_return:
    if (dlg) pjsip_dlg_dec_lock(dlg);
    pj_log_pop_indent();
    return status;
}

/*
 * Incoming text callback from the stream.
 */
static void rx_text_cb(pjmedia_txt_stream *strm, void *user_data,
                       const pjmedia_txt_stream_data *data)
{
    pjsua_call_id call_id;
    pjsua_txt_stream_data txt;

    PJ_UNUSED_ARG(strm);

    call_id = (pjsua_call_id)(pj_ssize_t)user_data;
    if (pjsua_var.calls[call_id].hanging_up)
        return;

    pj_log_push_indent();

    if (pjsua_var.ua_cfg.cb.on_call_rx_text) {
        txt.seq = data->seq;
        txt.ts = data->ts;
        txt.text = data->text;
        (*pjsua_var.ua_cfg.cb.on_call_rx_text)(call_id, &txt);
    }

    pj_log_pop_indent();
}

/* Internal function to stop text stream */
void pjsua_txt_stop_stream(pjsua_call_media *call_med)
{
    pjmedia_txt_stream *strm = call_med->strm.t.stream;
    pjmedia_stream_common *c_strm = (pjmedia_stream_common *)strm;
    pjmedia_rtcp_stat stat;
    pjmedia_txt_stream_info prev_txt_si;

    pj_assert(call_med->type == PJMEDIA_TYPE_TEXT);

    if (!strm)
        return;

    PJ_LOG(4,(THIS_FILE, "Stopping text stream.."));
    pj_log_push_indent();

    pjmedia_txt_stream_get_info(strm, &prev_txt_si);
    call_med->prev_local_addr = prev_txt_si.local_addr;
    call_med->prev_rem_addr = prev_txt_si.rem_addr;

    pjmedia_stream_common_send_rtcp_bye(c_strm);

    if (pjmedia_stream_common_get_stat(c_strm, &stat) == PJ_SUCCESS)
    {
        /* Save RTP timestamp & sequence, so when media session is
         * restarted, those values will be restored as the initial
         * RTP timestamp & sequence of the new media session. So in
         * the same call session, RTP timestamp and sequence are
         * guaranteed to be contigue.
         */
        call_med->rtp_tx_seq_ts_set = 1 | (1 << 1);
        call_med->rtp_tx_seq = stat.rtp_tx_last_seq;
        call_med->rtp_tx_ts = stat.rtp_tx_last_ts;
    }

    pjmedia_txt_stream_destroy(strm);
    call_med->strm.t.stream = NULL;

    pj_log_pop_indent();
}

pj_status_t pjsua_txt_channel_update(pjsua_call_media *call_med,
                                     pj_pool_t *tmp_pool,
                                     pjmedia_txt_stream_info *si,
                                     const pjmedia_sdp_session *local_sdp,
                                     const pjmedia_sdp_session *remote_sdp)
{
    pjsua_call *call = call_med->call;
    unsigned strm_idx = call_med->idx;
    pj_status_t status = PJ_SUCCESS;

    PJ_UNUSED_ARG(tmp_pool);
    PJ_UNUSED_ARG(local_sdp);
    PJ_UNUSED_ARG(remote_sdp);

    PJ_LOG(4,(THIS_FILE,"Text channel update for index %d for call %d...",
		                call_med->idx, call_med->call->index));
    pj_log_push_indent();

    si->rtcp_sdes_bye_disabled = pjsua_var.media_cfg.no_rtcp_sdes_bye;

    /* Check if no media is active */
    if (local_sdp->media[strm_idx]->desc.port != 0) {

        /* Optionally, application may modify other stream settings here
         * (such as jitter buffer parameters, codec ptime, etc.)
         */
        si->jb_init = pjsua_var.media_cfg.jb_init;
        si->jb_min_pre = pjsua_var.media_cfg.jb_min_pre;
        si->jb_max_pre = pjsua_var.media_cfg.jb_max_pre;
        si->jb_max = pjsua_var.media_cfg.jb_max;
        si->jb_discard_algo = pjsua_var.media_cfg.jb_discard_algo;

        /* Set SSRC and CNAME */
        si->ssrc = call_med->ssrc;
        si->cname = call->cname;

        /* Set RTP timestamp & sequence, normally these value are intialized
         * automatically when stream session created, but for some cases (e.g:
         * call reinvite, call update) timestamp and sequence need to be kept
         * contigue.
         */
        si->rtp_ts = call_med->rtp_tx_ts;
        si->rtp_seq = call_med->rtp_tx_seq;
        si->rtp_seq_ts_set = call_med->rtp_tx_seq_ts_set;

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0
        /* Enable/disable stream keep-alive and NAT hole punch. */
        si->use_ka = pjsua_var.acc[call->acc_id].cfg.use_stream_ka;

        si->ka_cfg = pjsua_var.acc[call->acc_id].cfg.stream_ka_cfg;
#endif

        if (!call->hanging_up && pjsua_var.ua_cfg.cb.on_stream_precreate) {
            pjsua_on_stream_precreate_param prm;
            prm.stream_idx = strm_idx;
            prm.stream_info.type = PJMEDIA_TYPE_TEXT;
            prm.stream_info.info.txt = *si;
            (*pjsua_var.ua_cfg.cb.on_stream_precreate)(call->index, &prm);

            /* Copy back only the fields which are allowed to be changed. */
            si->jb_init = prm.stream_info.info.aud.jb_init;
            si->jb_min_pre = prm.stream_info.info.aud.jb_min_pre;
            si->jb_max_pre = prm.stream_info.info.aud.jb_max_pre;
            si->jb_max = prm.stream_info.info.aud.jb_max;
            si->jb_discard_algo = prm.stream_info.info.aud.jb_discard_algo;
#if defined(PJMEDIA_STREAM_ENABLE_KA) && (PJMEDIA_STREAM_ENABLE_KA != 0)
            si->use_ka = prm.stream_info.info.aud.use_ka;
#endif
            si->rtcp_sdes_bye_disabled =
                prm.stream_info.info.aud.rtcp_sdes_bye_disabled;
        }

        /* Create session based on session info. */
        status = pjmedia_txt_stream_create(pjsua_var.med_endpt, NULL, si,
                                           call_med->tp, NULL,
                                           &call_med->strm.t.stream);
        if (status != PJ_SUCCESS) {
            goto on_return;
        }

        /* Start stream. */
        status = pjmedia_txt_stream_start(call_med->strm.t.stream);
        if (status != PJ_SUCCESS) {
            goto on_return;
        }

        if (call_med->prev_state == PJSUA_CALL_MEDIA_NONE) {
            pjmedia_stream_common_send_rtcp_sdes(
                (pjmedia_stream_common *)call_med->strm.t.stream);
        }

        /* Set incoming text callback. */
        if (!call->hanging_up && pjsua_var.ua_cfg.cb.on_call_rx_text) {
            pjmedia_txt_stream_set_rx_callback(
                call_med->strm.t.stream, &rx_text_cb,
                (void *)(pj_ssize_t)(call->index), 0);
        }
    }

on_return:
    pj_log_pop_indent();
    if (status != PJ_SUCCESS)
        pjsua_perror(THIS_FILE, "pjsua_txt_channel_update failed", status);
    return status;
}

#endif /* PJSUA_MEDIA_HAS_PJMEDIA */
