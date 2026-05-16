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
#  error The PJSUA_MEDIA_HAS_PJMEDIA should be declared as zero
#endif


#define THIS_FILE               "alt_pjsua_txt.c"
#define UNIMPLEMENTED(func)     PJ_LOG(2,(THIS_FILE, "*** Call to unimplemented function %s ***", #func));

/*****************************************************************************
 * API
 */

/* Our callback to receive incoming RTP packets */
static void txt_rtp_cb(void *user_data, void *pkt, pj_ssize_t size)
{
    pjsua_call_media *call_med = (pjsua_call_media*) user_data;

    /* TODO: Do something with the packet */
    PJ_LOG(4,(THIS_FILE, "RX %d bytes text RTP packet", (int)size));
}

/* Our callback to receive RTCP packets */
static void txt_rtcp_cb(void *user_data, void *pkt, pj_ssize_t size)
{
    pjsua_call_media *call_med = (pjsua_call_media*) user_data;

    /* TODO: Do something with the packet here */
    PJ_LOG(4,(THIS_FILE, "RX %d bytes text RTCP packet", (int)size));
}

/* A demo function to send dummy "RTP" packets periodically. You would not
 * need to have this function in the real app!
 */
static void timer_to_send_txt_rtp(void *user_data)
{
    pjsua_call_media *call_med = (pjsua_call_media*) user_data;
    const char *pkt = "Not RTP packet";

    if (!call_med->call || !call_med->call->inv || !call_med->tp) {
        /* Call has been disconnected. There is race condition here as
         * this cb may be called sometime after call has been disconnected */
        return;
    }

    pjmedia_transport_send_rtp(call_med->tp, pkt, strlen(pkt));

    pjsua_schedule_timer2(&timer_to_send_txt_rtp, call_med, 2000);
}

static void timer_to_send_txt_rtcp(void *user_data)
{
    pjsua_call_media *call_med = (pjsua_call_media*) user_data;
    const char *pkt = "Not RTCP packet";

    if (!call_med->call || !call_med->call->inv || !call_med->tp) {
        /* Call has been disconnected. There is race condition here as
         * this cb may be called sometime after call has been disconnected */
        return;
    }

    pjmedia_transport_send_rtcp(call_med->tp, pkt, strlen(pkt));

    pjsua_schedule_timer2(&timer_to_send_txt_rtcp, call_med, 5000);
}

/* Stop the text stream of a call. */
void pjsua_txt_stop_stream(pjsua_call_media *call_med)
{
    /* Detach our RTP/RTCP callbacks from transport */
    if (call_med->tp) {
        pjmedia_transport_detach(call_med->tp, call_med);
    }

    /* TODO: destroy your text stream here */
}

/*
 * This function is called whenever SDP negotiation has completed
 * successfully. Here you'd want to start your text stream
 * based on the info in the SDPs.
 */
pj_status_t pjsua_txt_channel_update(pjsua_call_media *call_med,
                                     pj_pool_t *tmp_pool,
                                     pjmedia_txt_stream_info *si,
                                     const pjmedia_sdp_session *local_sdp,
                                     const pjmedia_sdp_session *remote_sdp)
{
    pj_status_t status = PJ_SUCCESS;

    PJ_LOG(4,(THIS_FILE,"Alt text channel update.."));
    pj_log_push_indent();

    /* Check if no media is active */
    if (si->dir != PJMEDIA_DIR_NONE) {
        /* Attach our RTP and RTCP callbacks to the media transport */
        status = pjmedia_transport_attach(call_med->tp, call_med,
                                          &si->rem_addr, &si->rem_rtcp,
                                          pj_sockaddr_get_len(&si->rem_addr),
                                          &txt_rtp_cb, &txt_rtcp_cb);

        /* For a demonstration, let's use a timer to send "RTP" packet
         * periodically.
         */
        pjsua_schedule_timer2(&timer_to_send_txt_rtp, call_med, 0);
        pjsua_schedule_timer2(&timer_to_send_txt_rtcp, call_med, 2500);

        /* TODO:
         *   - Create and start your media stream based on the parameters
         *     in si
         */
    }

on_return:
    pj_log_pop_indent();
    return status;
}

/*****************************************************************************
 *
 * Call API which MAY need to be re-implemented if different backend is used.
 */

/* Send text to remote. */
PJ_DEF(pj_status_t)
pjsua_call_send_text(pjsua_call_id call_id,
                     const pjsua_call_send_text_param *param)
{
    UNIMPLEMENTED(pjsua_call_send_text)
    return PJ_ENOTSUP;
}
