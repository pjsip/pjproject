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
#include "pjsua_app.h"

#define THIS_FILE       "pjsua_app.c"

//#define STEREO_DEMO
//#define TRANSPORT_ADAPTER_SAMPLE
//#define HAVE_MULTIPART_TEST

/* Ringtones                US         UK  */
#define RINGBACK_FREQ1      440     /* 400 */
#define RINGBACK_FREQ2      480     /* 450 */
#define RINGBACK_ON         2000    /* 400 */
#define RINGBACK_OFF        4000    /* 200 */
#define RINGBACK_CNT        1       /* 2   */
#define RINGBACK_INTERVAL   4000    /* 2000 */

#define RING_FREQ1          800
#define RING_FREQ2          640
#define RING_ON             200
#define RING_OFF            100
#define RING_CNT            3
#define RING_INTERVAL       3000

#define current_acc     pjsua_acc_get_default()

#ifdef STEREO_DEMO
static void stereo_demo();
#endif

#ifdef USE_GUI
pj_bool_t showNotification(pjsua_call_id call_id);
pj_bool_t reportCallState(pjsua_call_id call_id);
#endif

static void ringback_start(pjsua_call_id call_id);
static void ring_start(pjsua_call_id call_id);
static void ring_stop(pjsua_call_id call_id);
static pj_status_t app_init(void);
static pj_status_t app_destroy(void);

static pjsua_app_cfg_t app_cfg;
pj_str_t                    uri_arg;
pj_bool_t                   app_running = PJ_FALSE;

/*****************************************************************************
 * Configuration manipulation
 */

/*****************************************************************************
 * Callback 
 */
static void ringback_start(pjsua_call_id call_id)
{
    if (app_config.no_tones)
        return;

    if (app_config.call_data[call_id].ringback_on)
        return;

    app_config.call_data[call_id].ringback_on = PJ_TRUE;

    if (++app_config.ringback_cnt==1 && 
        app_config.ringback_slot!=PJSUA_INVALID_ID) 
    {
        pjsua_conf_connect(app_config.ringback_slot, 0);
    }
}

static void ring_stop(pjsua_call_id call_id)
{
    if (app_config.no_tones)
        return;

    if (app_config.call_data[call_id].ringback_on) {
        app_config.call_data[call_id].ringback_on = PJ_FALSE;

        pj_assert(app_config.ringback_cnt>0);
        if (--app_config.ringback_cnt == 0 && 
            app_config.ringback_slot!=PJSUA_INVALID_ID) 
        {
            pjsua_conf_disconnect(app_config.ringback_slot, 0);
            pjmedia_tonegen_rewind(app_config.ringback_port);
        }
    }

    if (app_config.call_data[call_id].ring_on) {
        app_config.call_data[call_id].ring_on = PJ_FALSE;

        pj_assert(app_config.ring_cnt>0);
        if (--app_config.ring_cnt == 0 && 
            app_config.ring_slot!=PJSUA_INVALID_ID) 
        {
            pjsua_conf_disconnect(app_config.ring_slot, 0);
            pjmedia_tonegen_rewind(app_config.ring_port);
        }
    }
}

static void ring_start(pjsua_call_id call_id)
{
    if (app_config.no_tones)
        return;

    if (app_config.call_data[call_id].ring_on)
        return;

    app_config.call_data[call_id].ring_on = PJ_TRUE;

    if (++app_config.ring_cnt==1 && 
        app_config.ring_slot!=PJSUA_INVALID_ID) 
    {
        pjsua_conf_connect(app_config.ring_slot, 0);
    }
}

/* Callback from timer when the maximum call duration has been
 * exceeded.
 */
static void call_timeout_callback(pj_timer_heap_t *timer_heap,
                                  struct pj_timer_entry *entry)
{
    pjsua_call_id call_id = entry->id;
    pjsua_msg_data msg_data_;
    pjsip_generic_string_hdr warn;
    pj_str_t hname = pj_str("Warning");
    pj_str_t hvalue = pj_str("399 pjsua \"Call duration exceeded\"");

    PJ_UNUSED_ARG(timer_heap);

    if (call_id == PJSUA_INVALID_ID) {
        PJ_LOG(1,(THIS_FILE, "Invalid call ID in timer callback"));
        return;
    }
    
    /* Add warning header */
    pjsua_msg_data_init(&msg_data_);
    pjsip_generic_string_hdr_init2(&warn, &hname, &hvalue);
    pj_list_push_back(&msg_data_.hdr_list, &warn);

    /* Call duration has been exceeded; disconnect the call */
    PJ_LOG(3,(THIS_FILE, "Duration (%d seconds) has been exceeded "
                         "for call %d, disconnecting the call",
                         app_config.duration, call_id));
    entry->id = PJSUA_INVALID_ID;
    pjsua_call_hangup(call_id, 200, NULL, &msg_data_);
}

/*
 * Handler when invite state has changed.
 */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info call_info;

    PJ_UNUSED_ARG(e);

#ifdef USE_GUI
    reportCallState(call_id);
#endif

    pjsua_call_get_info(call_id, &call_info);

    if (call_info.state == PJSIP_INV_STATE_DISCONNECTED) {

        /* Stop all ringback for this call */
        ring_stop(call_id);

        /* Cancel duration timer, if any */
        if (app_config.call_data[call_id].timer.id != PJSUA_INVALID_ID) {
            app_call_data *cd = &app_config.call_data[call_id];
            pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();

            cd->timer.id = PJSUA_INVALID_ID;
            pjsip_endpt_cancel_timer(endpt, &cd->timer);
        }

        /* Rewind play file when hangup automatically, 
         * since file is not looped
         */
        if (app_config.auto_play_hangup)
            pjsua_player_set_pos(app_config.wav_id, 0);


        PJ_LOG(3,(THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%.*s)]", 
                  call_id,
                  call_info.last_status,
                  (int)call_info.last_status_text.slen,
                  call_info.last_status_text.ptr));

        if (call_id == current_call) {
            find_next_call();
        }

        /* Dump media state upon disconnected.
         * Now pjsua_media_channel_deinit() automatically log the call dump.
         */
        if (0) {
            PJ_LOG(5,(THIS_FILE, 
                      "Call %d disconnected, dumping media stats..", 
                      call_id));
            log_call_dump(call_id);
        }

    } else {

        if (app_config.duration != PJSUA_APP_NO_LIMIT_DURATION && 
            call_info.state == PJSIP_INV_STATE_CONFIRMED) 
        {
            /* Schedule timer to hangup call after the specified duration */
            app_call_data *cd = &app_config.call_data[call_id];
            pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
            pj_time_val delay;

            cd->timer.id = call_id;
            delay.sec = app_config.duration;
            delay.msec = 0;
            pjsip_endpt_schedule_timer(endpt, &cd->timer, &delay);
        }

        if (call_info.state == PJSIP_INV_STATE_EARLY) {
            int code;
            pj_str_t reason;
            pjsip_msg *msg;

            /* This can only occur because of TX or RX message */
            pj_assert(e->type == PJSIP_EVENT_TSX_STATE);

            if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {
                msg = e->body.tsx_state.src.rdata->msg_info.msg;
            } else {
                msg = e->body.tsx_state.src.tdata->msg;
            }

            code = msg->line.status.code;
            reason = msg->line.status.reason;

            /* Start ringback for 180 for UAC unless there's SDP in 180 */
            if (call_info.role==PJSIP_ROLE_UAC && code==180 && 
                msg->body == NULL && 
                call_info.media_status==PJSUA_CALL_MEDIA_NONE) 
            {
                ringback_start(call_id);
            }

            PJ_LOG(3,(THIS_FILE, "Call %d state changed to %.*s (%d %.*s)", 
                      call_id, (int)call_info.state_text.slen, 
                      call_info.state_text.ptr, code, 
                      (int)reason.slen, reason.ptr));
        } else {
            PJ_LOG(3,(THIS_FILE, "Call %d state changed to %.*s", 
                      call_id,
                      (int)call_info.state_text.slen,
                      call_info.state_text.ptr));
        }

        if (current_call==PJSUA_INVALID_ID)
            current_call = call_id;

    }
}

/*
 * Handler when audio stream is destroyed.
 */
static void on_stream_destroyed(pjsua_call_id call_id,
                                pjmedia_stream *strm,
                                unsigned stream_idx)
{
    PJ_UNUSED_ARG(strm);

    /* Now pjsua_media_channel_deinit() automatically log the call dump. */
    if (0) {
        PJ_LOG(5,(THIS_FILE, 
                  "Call %d stream %d destroyed, dumping media stats..", 
                  call_id, stream_idx));
        log_call_dump(call_id);
    }
}

/**
 * Handler when there is incoming call.
 */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
                             pjsip_rx_data *rdata)
{
    pjsua_call_info call_info;

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    pjsua_call_get_info(call_id, &call_info);

    if (current_call==PJSUA_INVALID_ID)
        current_call = call_id;

#ifdef USE_GUI
    showNotification(call_id);
#endif

    /* Start ringback */
    if (call_info.rem_aud_cnt)
        ring_start(call_id);
    
    if (app_config.auto_answer > 0) {
        pjsua_call_setting opt;

        pjsua_call_setting_default(&opt);
        opt.aud_cnt = app_config.aud_cnt;
        opt.vid_cnt = app_config.vid.vid_cnt;
        opt.txt_cnt = app_config.txt_cnt;

        pjsua_call_answer2(call_id, &opt, app_config.auto_answer, NULL,
                           NULL);
    }
    
    if (app_config.auto_answer < 200) {
        char notif_st[80] = {0};

#if PJSUA_HAS_VIDEO
        if (call_info.rem_offerer && call_info.rem_vid_cnt) {
            snprintf(notif_st, sizeof(notif_st), 
                     "To %s the video, type \"vid %s\" first, "
                     "before answering the call!\n",
                     (app_config.vid.vid_cnt? "reject":"accept"),
                     (app_config.vid.vid_cnt? "disable":"enable"));
        }
#endif

        PJ_LOG(3,(THIS_FILE,
                  "Incoming call for account %d!\n"
                  "Media count: %d audio & %d video & %d text\n"
                  "%s"
                  "From: %.*s\n"
                  "To: %.*s\n"
                  "Press %s to answer or %s to reject call",
                  acc_id,
                  call_info.rem_aud_cnt,
                  call_info.rem_vid_cnt,
                  call_info.rem_txt_cnt,
                  notif_st,
                  (int)call_info.remote_info.slen,
                  call_info.remote_info.ptr,
                  (int)call_info.local_info.slen,
                  call_info.local_info.ptr,
                  (app_config.use_cli?"ca a":"a"),
                  (app_config.use_cli?"g":"h")));
    }
}

/* General processing for media state. "mi" is the media index */
static void on_call_generic_media_state(pjsua_call_info *ci, unsigned mi,
                                        pj_bool_t *has_error)
{
    const char *status_name[] = {
        "None",
        "Active",
        "Local hold",
        "Remote hold",
        "Error"
    };

    PJ_UNUSED_ARG(has_error);

    pj_assert(ci->media[mi].status <= PJ_ARRAY_SIZE(status_name));
    pj_assert(PJSUA_CALL_MEDIA_ERROR == 4);

    PJ_LOG(4,(THIS_FILE, "Call %d media %d [type=%s], status is %s",
              ci->id, mi, pjmedia_type_name(ci->media[mi].type),
              status_name[ci->media[mi].status]));
}

/* Process audio media state. "mi" is the media index. */
static void on_call_audio_state(pjsua_call_info *ci, unsigned mi,
                                pj_bool_t *has_error)
{
    PJ_UNUSED_ARG(has_error);

    /* Stop ringback */
    ring_stop(ci->id);

    /* Connect ports appropriately when media status is ACTIVE or REMOTE HOLD,
     * otherwise we should NOT connect the ports.
     */
    if (ci->media[mi].status == PJSUA_CALL_MEDIA_ACTIVE ||
        ci->media[mi].status == PJSUA_CALL_MEDIA_REMOTE_HOLD)
    {
        pj_bool_t connect_sound = PJ_TRUE;
        pj_bool_t disconnect_mic = PJ_FALSE;
        pjsua_conf_port_id call_conf_slot;

        call_conf_slot = ci->media[mi].stream.aud.conf_slot;

        /* Make sure conf slot is valid (e.g: media dir is not "inactive") */
        if (call_conf_slot == PJSUA_INVALID_ID)
            return;

        /* Loopback sound, if desired */
        if (app_config.auto_loop) {
            pjsua_conf_connect(call_conf_slot, call_conf_slot);
            connect_sound = PJ_FALSE;
        }

        /* Automatically record conversation, if desired */
        if (app_config.auto_rec && app_config.rec_port != PJSUA_INVALID_ID) {
            pjsua_conf_connect(call_conf_slot, app_config.rec_port);
        }

        /* Stream a file, if desired */
        if ((app_config.auto_play || app_config.auto_play_hangup) && 
            app_config.wav_port != PJSUA_INVALID_ID)
        {
            pjsua_conf_connect(app_config.wav_port, call_conf_slot);
            connect_sound = PJ_FALSE;
        }

        /* Stream AVI, if desired */
        if (app_config.avi_auto_play &&
            app_config.avi_def_idx != PJSUA_INVALID_ID &&
            app_config.avi[app_config.avi_def_idx].slot != PJSUA_INVALID_ID)
        {
            pjsua_conf_connect(app_config.avi[app_config.avi_def_idx].slot,
                               call_conf_slot);
            disconnect_mic = PJ_TRUE;
        }

        /* Put call in conference with other calls, if desired */
        if (app_config.auto_conf) {
            pjsua_call_id call_ids[PJSUA_MAX_CALLS];
            unsigned call_cnt=PJ_ARRAY_SIZE(call_ids);
            unsigned i;

            /* Get all calls, and establish media connection between
             * this call and other calls.
             */
            pjsua_enum_calls(call_ids, &call_cnt);

            for (i=0; i<call_cnt; ++i) {
                if (call_ids[i] == ci->id)
                    continue;
                
                if (!pjsua_call_has_media(call_ids[i]))
                    continue;

                pjsua_conf_connect(call_conf_slot,
                                   pjsua_call_get_conf_port(call_ids[i]));
                pjsua_conf_connect(pjsua_call_get_conf_port(call_ids[i]),
                                   call_conf_slot);

                /* Automatically record conversation, if desired */
                if (app_config.auto_rec && app_config.rec_port !=
                                           PJSUA_INVALID_ID)
                {
                    pjsua_conf_connect(pjsua_call_get_conf_port(call_ids[i]), 
                                       app_config.rec_port);
                }

            }

            /* Also connect call to local sound device */
            connect_sound = PJ_TRUE;
        }

        /* Otherwise connect to sound device */
        if (connect_sound) {
            pjsua_conf_connect(call_conf_slot, 0);
            if (!disconnect_mic)
                pjsua_conf_connect(0, call_conf_slot);

            /* Automatically record conversation, if desired */
            if (app_config.auto_rec && app_config.rec_port != PJSUA_INVALID_ID)
            {
                pjsua_conf_connect(call_conf_slot, app_config.rec_port);
                pjsua_conf_connect(0, app_config.rec_port);
            }
        }
    }
}

/* Process video media state. "mi" is the media index. */
static void on_call_video_state(pjsua_call_info *ci, unsigned mi,
                                pj_bool_t *has_error)
{
    if (ci->media_status != PJSUA_CALL_MEDIA_ACTIVE)
        return;

    arrange_window(ci->media[mi].stream.vid.win_in);

    PJ_UNUSED_ARG(has_error);
}

/*
 * Callback on media state changed event.
 * The action may connect the call to sound device, to file, or
 * to loop the call.
 */
static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info call_info;
    unsigned mi;
    pj_bool_t has_error = PJ_FALSE;

    pjsua_call_get_info(call_id, &call_info);

    for (mi=0; mi<call_info.media_cnt; ++mi) {
        on_call_generic_media_state(&call_info, mi, &has_error);

        switch (call_info.media[mi].type) {
        case PJMEDIA_TYPE_AUDIO:
            on_call_audio_state(&call_info, mi, &has_error);
            break;
        case PJMEDIA_TYPE_VIDEO:
            on_call_video_state(&call_info, mi, &has_error);
            break;
        default:
            /* Make gcc happy about enum not handled by switch/case */
            break;
        }
    }

    if (has_error) {
        pj_str_t reason = pj_str("Media failed");
        pjsua_call_hangup(call_id, 500, &reason, NULL);
    }

#if PJSUA_HAS_VIDEO
    /* Check if remote has just tried to enable video */
    if (call_info.rem_offerer && call_info.rem_vid_cnt)
    {
        int vid_idx;

        /* Check if there is active video */
        vid_idx = pjsua_call_get_vid_stream_idx(call_id);
        if (vid_idx == -1 || call_info.media[vid_idx].dir == PJMEDIA_DIR_NONE) {
            PJ_LOG(3,(THIS_FILE,
                      "Just rejected incoming video offer on call %d, "
                      "use \"vid call enable %d\" or \"vid call add\" to "
                      "enable video!", call_id, vid_idx));
        }
    }
#endif
}

/*
 * DTMF callback.
 */
/*
static void call_on_dtmf_callback(pjsua_call_id call_id, int dtmf)
{
    PJ_LOG(3,(THIS_FILE, "Incoming DTMF on call %d: %c", call_id, dtmf));
}
*/

static void call_on_dtmf_callback2(pjsua_call_id call_id, 
                                   const pjsua_dtmf_info *info)
{    
    char duration[16];
    char method[16];

    duration[0] = '\0';

    switch (info->method) {
    case PJSUA_DTMF_METHOD_RFC2833:
        pj_ansi_snprintf(method, sizeof(method), "RFC2833");
        break;
    case PJSUA_DTMF_METHOD_SIP_INFO:
        pj_ansi_snprintf(method, sizeof(method), "SIP INFO");
        pj_ansi_snprintf(duration, sizeof(duration), ":duration(%d)", 
                         info->duration);
        break;
    };    
    PJ_LOG(3,(THIS_FILE, "Incoming DTMF on call %d: %c%s, using %s method", 
           call_id, info->digit, duration, method));
}

/* Incoming text stream callback. */
static void call_on_rx_text(pjsua_call_id call_id,
                            const pjsua_txt_stream_data *data)
{
    if (data->text.slen == 0) {
        PJ_LOG(4, (THIS_FILE, "Received empty T140 block with seq %d",
                              data->seq));
    } else {
        PJ_LOG(3, (THIS_FILE, "Incoming text on call %d, seq %d: %.*s "
                              "(%d bytes)", call_id, data->seq,
                              (int)data->text.slen, data->text.ptr,
                              (int)data->text.slen));
    }
}

/*
 * Redirection handler.
 */
static pjsip_redirect_op call_on_redirected(pjsua_call_id call_id, 
                                            const pjsip_uri *target,
                                            const pjsip_event *e)
{
    PJ_UNUSED_ARG(e);

    if (app_config.redir_op == PJSIP_REDIRECT_PENDING) {
        char uristr[PJSIP_MAX_URL_SIZE];
        int len;

        len = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, target, uristr, 
                              sizeof(uristr));
        if (len < 1) {
            pj_ansi_strxcpy(uristr, "--URI too long--", sizeof(uristr));
        }

        PJ_LOG(3,(THIS_FILE, "Call %d is being redirected to %.*s. "
                  "Press 'Ra' to accept+replace To header, 'RA' to accept, "
                  "'Rr' to reject, or 'Rd' to disconnect.",
                  call_id, len, uristr));
    }

    return app_config.redir_op;
}

/*
 * Handler registration status has changed.
 */
static void on_reg_state(pjsua_acc_id acc_id)
{
    PJ_UNUSED_ARG(acc_id);

    // Log already written.
}

/*
 * Handler for incoming presence subscription request
 */
static void on_incoming_subscribe(pjsua_acc_id acc_id,
                                  pjsua_srv_pres *srv_pres,
                                  pjsua_buddy_id buddy_id,
                                  const pj_str_t *from,
                                  pjsip_rx_data *rdata,
                                  pjsip_status_code *code,
                                  pj_str_t *reason,
                                  pjsua_msg_data *msg_data_)
{
    /* Just accept the request (the default behavior) */
    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(srv_pres);
    PJ_UNUSED_ARG(buddy_id);
    PJ_UNUSED_ARG(from);
    PJ_UNUSED_ARG(rdata);
    PJ_UNUSED_ARG(code);
    PJ_UNUSED_ARG(reason);
    PJ_UNUSED_ARG(msg_data_);
}


/*
 * Handler on buddy state changed.
 */
static void on_buddy_state(pjsua_buddy_id buddy_id)
{
    pjsua_buddy_info info;
    pjsua_buddy_get_info(buddy_id, &info);

    PJ_LOG(3,(THIS_FILE, "%.*s status is %.*s, subscription state is %s "
                         "(last termination reason code=%d %.*s)",
              (int)info.uri.slen,
              info.uri.ptr,
              (int)info.status_text.slen,
              info.status_text.ptr,
              info.sub_state_name,
              info.sub_term_code,
              (int)info.sub_term_reason.slen,
              info.sub_term_reason.ptr));
}


/*
 * Handler on buddy dialog event state changed.
 */
static void on_buddy_dlg_event_state(pjsua_buddy_id buddy_id)
{
    pjsua_buddy_dlg_event_info info;
    pjsua_buddy_get_dlg_event_info(buddy_id, &info);

    PJ_LOG(3,(THIS_FILE, "%.*s dialog-info-state: %.*s, "
              "dialog-info-entity: %.*s, dialog-id: %.*s, "
              "dialog-call-id: %.*s, dialog-direction: %.*s, "
              "dialog-state: %.*s, dialog-duration: %.*s, "
              "local-identity: %.*s, local-target-uri: %.*s, "
              "remote-identity: %.*s, remote-target-uri: %.*s, "
              "dialog-local-tag: %.*s, dialog-remote-tag: %.*s, "
              "subscription state: %s (last termination reason code=%d %.*s)",
              (int)info.uri.slen, info.uri.ptr,
              (int)info.dialog_info_state.slen, info.dialog_info_state.ptr,
              (int)info.dialog_info_entity.slen, info.dialog_info_entity.ptr,
              (int)info.dialog_id.slen, info.dialog_id.ptr,
              (int)info.dialog_call_id.slen, info.dialog_call_id.ptr,
              (int)info.dialog_direction.slen, info.dialog_direction.ptr,
              (int)info.dialog_state.slen, info.dialog_state.ptr,
              (int)info.dialog_duration.slen, info.dialog_duration.ptr,
              (int)info.local_identity.slen, info.local_identity.ptr,
              (int)info.local_target_uri.slen, info.local_target_uri.ptr,
              (int)info.remote_identity.slen, info.remote_identity.ptr,
              (int)info.remote_target_uri.slen, info.remote_target_uri.ptr,
              (int)info.dialog_local_tag.slen, info.dialog_local_tag.ptr,
              (int)info.dialog_remote_tag.slen, info.dialog_remote_tag.ptr,
              info.sub_state_name, info.sub_term_code,
              (int)info.sub_term_reason.slen, info.sub_term_reason.ptr));
}

/*
 * Subscription state has changed.
 */
static void on_buddy_evsub_state(pjsua_buddy_id buddy_id,
                                 pjsip_evsub *sub,
                                 pjsip_event *event)
{
    char event_info[80];

    PJ_UNUSED_ARG(sub);

    event_info[0] = '\0';

    if (event->type == PJSIP_EVENT_TSX_STATE &&
            event->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
    {
        pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
        snprintf(event_info, sizeof(event_info),
                 " (RX %s)",
                 pjsip_rx_data_get_info(rdata));
    }

    PJ_LOG(4,(THIS_FILE,
              "Buddy %d: subscription state: %s (event: %s%s)",
              buddy_id, pjsip_evsub_get_state_name(sub),
              pjsip_event_str(event->type),
              event_info));

}

static void on_buddy_evsub_dlg_event_state(pjsua_buddy_id buddy_id,
                                     pjsip_evsub *sub,
                                     pjsip_event *event)
{
    char event_info[80];

    PJ_UNUSED_ARG(sub);

    event_info[0] = '\0';

    if (event->type == PJSIP_EVENT_TSX_STATE &&
        event->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
    {
        pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
        snprintf(event_info, sizeof(event_info),
                 " (RX %s)",
                 pjsip_rx_data_get_info(rdata));
    }

    PJ_LOG(4,(THIS_FILE,
              "Buddy %d: dialog event subscription state: %s (event: %s%s)",
              buddy_id, pjsip_evsub_get_state_name(sub),
              pjsip_event_str(event->type), event_info));
}


/**
 * Incoming IM message (i.e. MESSAGE request)!
 */
static void on_pager(pjsua_call_id call_id, const pj_str_t *from,
                     const pj_str_t *to, const pj_str_t *contact,
                     const pj_str_t *mime_type, const pj_str_t *text)
{
    /* Note: call index may be -1 */
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(to);
    PJ_UNUSED_ARG(contact);
    PJ_UNUSED_ARG(mime_type);

    PJ_LOG(3,(THIS_FILE,"MESSAGE from %.*s: %.*s (%.*s)",
              (int)from->slen, from->ptr,
              (int)text->slen, text->ptr,
              (int)mime_type->slen, mime_type->ptr));
}


/**
 * Received typing indication
 */
static void on_typing(pjsua_call_id call_id, const pj_str_t *from,
                      const pj_str_t *to, const pj_str_t *contact,
                      pj_bool_t is_typing)
{
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(to);
    PJ_UNUSED_ARG(contact);

    PJ_LOG(3,(THIS_FILE, "IM indication: %.*s %s",
              (int)from->slen, from->ptr,
              (is_typing?"is typing..":"has stopped typing")));
}


/**
 * Call transfer request status.
 */
static void on_call_transfer_status(pjsua_call_id call_id,
                                    int status_code,
                                    const pj_str_t *status_text,
                                    pj_bool_t final,
                                    pj_bool_t *p_cont)
{
    PJ_LOG(3,(THIS_FILE, "Call %d: transfer status=%d (%.*s) %s",
              call_id, status_code,
              (int)status_text->slen, status_text->ptr,
              (final ? "[final]" : "")));

    if (status_code/100 == 2) {
        PJ_LOG(3,(THIS_FILE, 
                  "Call %d: call transferred successfully, disconnecting call",
                  call_id));
        pjsua_call_hangup(call_id, PJSIP_SC_GONE, NULL, NULL);
        *p_cont = PJ_FALSE;
    }
}


/*
 * Notification that call is being replaced.
 */
static void on_call_replaced(pjsua_call_id old_call_id,
                             pjsua_call_id new_call_id)
{
    pjsua_call_info old_ci, new_ci;

    pjsua_call_get_info(old_call_id, &old_ci);
    pjsua_call_get_info(new_call_id, &new_ci);

    PJ_LOG(3,(THIS_FILE, "Call %d with %.*s is being replaced by "
                         "call %d with %.*s",
                         old_call_id, 
                         (int)old_ci.remote_info.slen, old_ci.remote_info.ptr,
                         new_call_id,
                         (int)new_ci.remote_info.slen, new_ci.remote_info.ptr));
}


/*
 * NAT type detection callback.
 */
static void on_nat_detect(const pj_stun_nat_detect_result *res)
{
    if (res->status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "NAT detection failed", res->status);
    } else {
        PJ_LOG(3, (THIS_FILE, "NAT detected as %s", res->nat_type_name));
    }
}


/*
 * MWI indication
 */
static void on_mwi_info(pjsua_acc_id acc_id, pjsua_mwi_info *mwi_info)
{
    pj_str_t body;
    
    PJ_LOG(3,(THIS_FILE, "Received MWI for acc %d:", acc_id));

    if (mwi_info->rdata->msg_info.ctype) {
        const pjsip_ctype_hdr *ctype = mwi_info->rdata->msg_info.ctype;

        PJ_LOG(3,(THIS_FILE, " Content-Type: %.*s/%.*s",
                  (int)ctype->media.type.slen,
                  ctype->media.type.ptr,
                  (int)ctype->media.subtype.slen,
                  ctype->media.subtype.ptr));
    }

    if (!mwi_info->rdata->msg_info.msg->body) {
        PJ_LOG(3,(THIS_FILE, "  no message body"));
        return;
    }

    body.ptr = (char *)mwi_info->rdata->msg_info.msg->body->data;
    body.slen = mwi_info->rdata->msg_info.msg->body->len;

    PJ_LOG(3,(THIS_FILE, " Body:\n%.*s", (int)body.slen, body.ptr));
}


/*
 * Transport status notification
 */
static void on_transport_state(pjsip_transport *tp, 
                               pjsip_transport_state state,
                               const pjsip_transport_state_info *info)
{
    char host_port[128];

    pj_addr_str_print(&tp->remote_name.host, 
                      tp->remote_name.port, host_port, sizeof(host_port), 1);
    switch (state) {
    case PJSIP_TP_STATE_CONNECTED:
        {
            PJ_LOG(3,(THIS_FILE, "SIP %s transport is connected to %s",
                     tp->type_name, host_port));
        }
        break;

    case PJSIP_TP_STATE_DISCONNECTED:
        {
            char buf[100];
            int len;

            len = pj_ansi_snprintf(buf, sizeof(buf), "SIP %s transport is "
                      "disconnected from %s", tp->type_name, host_port);
            PJ_CHECK_TRUNC_STR(len, buf, sizeof(buf));
            pjsua_perror(THIS_FILE, buf, info->status);
        }
        break;

    default:
        break;
    }

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0

    if (!pj_ansi_stricmp(tp->type_name, "tls") && info->ext_info &&
        (state == PJSIP_TP_STATE_CONNECTED || 
         ((pjsip_tls_state_info*)info->ext_info)->
                                 ssl_sock_info->verify_status != PJ_SUCCESS))
    {
        pjsip_tls_state_info *tls_info = (pjsip_tls_state_info*)info->ext_info;
        pj_ssl_sock_info *ssl_sock_info = tls_info->ssl_sock_info;
        char buf[2048];
        const char *verif_msgs[32];
        unsigned verif_msg_cnt;

        /* Dump server TLS cipher */
        PJ_LOG(4,(THIS_FILE, "TLS cipher used: 0x%06X/%s",
                  ssl_sock_info->cipher,
                  pj_ssl_cipher_name(ssl_sock_info->cipher) ));

        /* Dump server TLS certificate */
        pj_ssl_cert_info_dump(ssl_sock_info->remote_cert_info, "  ",
                              buf, sizeof(buf));
        PJ_LOG(4,(THIS_FILE, "TLS cert info of %s:\n%s", host_port, buf));

        /* Dump server TLS certificate verification result */
        verif_msg_cnt = PJ_ARRAY_SIZE(verif_msgs);
        pj_ssl_cert_get_verify_status_strings(ssl_sock_info->verify_status,
                                              verif_msgs, &verif_msg_cnt);
        PJ_LOG(3,(THIS_FILE, "TLS cert verification result of %s : %s",
                             host_port,
                             (verif_msg_cnt == 1? verif_msgs[0]:"")));
        if (verif_msg_cnt > 1) {
            unsigned i;
            for (i = 0; i < verif_msg_cnt; ++i)
                PJ_LOG(3,(THIS_FILE, "- %s", verif_msgs[i]));
        }

        if (ssl_sock_info->verify_status &&
            !app_config.udp_cfg.tls_setting.verify_server) 
        {
            PJ_LOG(3,(THIS_FILE, "PJSUA is configured to ignore TLS cert "
                                 "verification errors"));
        }
    }

#endif

}

/*
 * Notification on ICE error.
 */
static void on_ice_transport_error(int index, pj_ice_strans_op op,
                                   pj_status_t status, void *param)
{
    PJ_UNUSED_ARG(op);
    PJ_UNUSED_ARG(param);
    PJ_PERROR(1,(THIS_FILE, status,
                 "ICE keep alive failure for transport %d", index));
}

/*
 * Notification on sound device operation.
 */
static pj_status_t on_snd_dev_operation(int operation)
{
    int cap_dev, play_dev;

    pjsua_get_snd_dev(&cap_dev, &play_dev);
    PJ_LOG(3,(THIS_FILE, "Turning sound device %d %d %s", cap_dev, play_dev,
              (operation? "ON":"OFF")));
    return PJ_SUCCESS;
}

static char *get_media_dir(pjmedia_dir dir) {
    switch (dir) {
    case PJMEDIA_DIR_ENCODING:
        return "TX";
    case PJMEDIA_DIR_DECODING:
        return "RX";
    case PJMEDIA_DIR_ENCODING+PJMEDIA_DIR_DECODING:
        return "TX+RX";
    default:
        return "unknown dir";
    }    
}

/* Callback on media events */
static void on_call_media_event(pjsua_call_id call_id,
                                unsigned med_idx,
                                pjmedia_event *event)
{
    char event_name[5];

    PJ_LOG(5,(THIS_FILE, "Event %s",
              pjmedia_fourcc_name(event->type, event_name)));

    if (event->type == PJMEDIA_EVENT_MEDIA_TP_ERR) {
        pjmedia_event_media_tp_err_data *err_data;

        err_data = &event->data.med_tp_err;
        PJ_PERROR(3, (THIS_FILE, err_data->status, 
                  "Media transport error event (%s %s %s)",
                  (err_data->type==PJMEDIA_TYPE_AUDIO)?"Audio":"Video",
                  (err_data->is_rtp)?"RTP":"RTCP",
                  get_media_dir(err_data->dir)));
    }
#if PJSUA_HAS_VIDEO
    else if (event->type == PJMEDIA_EVENT_FMT_CHANGED) {
        /* Adjust renderer window size to original video size */
        pjsua_call_info ci;

        pjsua_call_get_info(call_id, &ci);

        if ((ci.media[med_idx].type == PJMEDIA_TYPE_VIDEO) &&
            (ci.media[med_idx].dir & PJMEDIA_DIR_DECODING))
        {
            pjsua_vid_win_id wid;
            pjmedia_rect_size size;
            pjsua_vid_win_info win_info;

            wid = ci.media[med_idx].stream.vid.win_in;
            pjsua_vid_win_get_info(wid, &win_info);

            size = event->data.fmt_changed.new_fmt.det.vid.size;
            if (size.w != win_info.size.w || size.h != win_info.size.h) {
                pjsua_vid_win_set_size(wid, &size);

                /* Re-arrange video windows */
                arrange_window(PJSUA_INVALID_ID);
            }
        }
    }
#else
    PJ_UNUSED_ARG(call_id);
    PJ_UNUSED_ARG(med_idx);    
#endif
}

#ifdef TRANSPORT_ADAPTER_SAMPLE
/*
 * This callback is called when media transport needs to be created.
 */
static pjmedia_transport* on_create_media_transport(pjsua_call_id call_id,
                                                    unsigned media_idx,
                                                    pjmedia_transport *base_tp,
                                                    unsigned flags)
{
    pjmedia_transport *adapter;
    pj_status_t status;

    /* Create the adapter */
    status = pjmedia_tp_adapter_create(pjsua_get_pjmedia_endpt(),
                                       NULL, base_tp,
                                       (flags & PJSUA_MED_TP_CLOSE_MEMBER),
                                       &adapter);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(1,(THIS_FILE, status, "Error creating adapter"));
        return NULL;
    }

    PJ_LOG(3,(THIS_FILE, "Media transport is created for call %d media %d",
              call_id, media_idx));

    return adapter;
}
#endif

/* Playfile done notification, set timer to hangup calls */
void on_playfile_done(pjmedia_port *port, void *usr_data)
{
    pj_time_val delay;

    PJ_UNUSED_ARG(port);
    PJ_UNUSED_ARG(usr_data);

    /* Just rewind WAV when it is played outside of call */
    if (pjsua_call_get_count() == 0) {
        pjsua_player_set_pos(app_config.wav_id, 0);
    }

    /* Timer is already active */
    if (app_config.auto_hangup_timer.id == 1)
        return;

    app_config.auto_hangup_timer.id = 1;
    delay.sec = 0;
    delay.msec = 200; /* Give 200 ms before hangup */
    pjsip_endpt_schedule_timer(pjsua_get_pjsip_endpt(), 
                               &app_config.auto_hangup_timer, 
                               &delay);
}

/* IP change progress callback. */
void on_ip_change_progress(pjsua_ip_change_op op,
                           pj_status_t status,
                           const pjsua_ip_change_op_info *info)
{
    char info_str[128];
    pjsua_acc_info acc_info;
    pjsua_transport_info tp_info;

    if (status == PJ_SUCCESS) {
        switch (op) {
        case PJSUA_IP_CHANGE_OP_SHUTDOWN_TP:
            pj_ansi_snprintf(info_str, sizeof(info_str),
                             "TCP/TLS transports shutdown");
            break;

        case PJSUA_IP_CHANGE_OP_RESTART_LIS:
            pjsua_transport_get_info(info->lis_restart.transport_id, &tp_info);
            pj_ansi_snprintf(info_str, sizeof(info_str),
                             "restart transport %.*s",
                             (int)tp_info.info.slen, tp_info.info.ptr);
            break;
        case PJSUA_IP_CHANGE_OP_ACC_SHUTDOWN_TP:
            pjsua_acc_get_info(info->acc_shutdown_tp.acc_id, &acc_info);

            pj_ansi_snprintf(info_str, sizeof(info_str),
                             "transport shutdown for account %.*s",
                             (int)acc_info.acc_uri.slen,
                             acc_info.acc_uri.ptr);
            break;
        case PJSUA_IP_CHANGE_OP_ACC_UPDATE_CONTACT:
            pjsua_acc_get_info(info->acc_shutdown_tp.acc_id, &acc_info);
            if (info->acc_update_contact.code) {
                pj_ansi_snprintf(info_str, sizeof(info_str),
                                 "update contact for account %.*s, code[%d]",
                                 (int)acc_info.acc_uri.slen,
                                 acc_info.acc_uri.ptr,
                                 info->acc_update_contact.code);
            } else {
                pj_ansi_snprintf(info_str, sizeof(info_str),
                                 "update contact for account %.*s",
                                 (int)acc_info.acc_uri.slen,
                                 acc_info.acc_uri.ptr);
            }
            break;
        case PJSUA_IP_CHANGE_OP_ACC_HANGUP_CALLS:
            pjsua_acc_get_info(info->acc_shutdown_tp.acc_id, &acc_info);
            pj_ansi_snprintf(info_str, sizeof(info_str),
                             "hangup call for account %.*s, call_id[%d]",
                             (int)acc_info.acc_uri.slen, acc_info.acc_uri.ptr,
                             info->acc_hangup_calls.call_id);
            break;
        case PJSUA_IP_CHANGE_OP_ACC_REINVITE_CALLS:
            pjsua_acc_get_info(info->acc_shutdown_tp.acc_id, &acc_info);
            pj_ansi_snprintf(info_str, sizeof(info_str),
                             "reinvite call for account %.*s, call_id[%d]",
                             (int)acc_info.acc_uri.slen, acc_info.acc_uri.ptr,
                             info->acc_reinvite_calls.call_id);
            break;
        case PJSUA_IP_CHANGE_OP_COMPLETED:
            pj_ansi_snprintf(info_str, sizeof(info_str),
                             "done");
            break;
        default:
            pj_ansi_snprintf(info_str, sizeof(info_str),
                             "unknown-op");
            break;
        }
        PJ_LOG(3,(THIS_FILE, "IP change progress report : %s", info_str));

    } else {
        PJ_PERROR(3,(THIS_FILE, status, "IP change progress fail"));
    }
}

/* Auto hangup timer callback */
static void hangup_timeout_callback(pj_timer_heap_t *timer_heap,
                                    struct pj_timer_entry *entry)
{
    PJ_UNUSED_ARG(timer_heap);
    PJ_UNUSED_ARG(entry);

    app_config.auto_hangup_timer.id = 0;
    pjsua_call_hangup_all();
}

/*
 * A simple registrar, invoked by default_mod_on_rx_request()
 */
static void simple_registrar(pjsip_rx_data *rdata)
{
    pjsip_tx_data *tdata;
    const pjsip_expires_hdr *exp;
    const pjsip_hdr *h;
    pjsip_generic_string_hdr *srv;
    pj_status_t status;

    status = pjsip_endpt_create_response(pjsua_get_pjsip_endpt(),
                                         rdata, 200, NULL, &tdata);
    if (status != PJ_SUCCESS)
    return;

    exp = (pjsip_expires_hdr *)pjsip_msg_find_hdr(rdata->msg_info.msg, 
                                                  PJSIP_H_EXPIRES, NULL);

    h = rdata->msg_info.msg->hdr.next;
    while (h != &rdata->msg_info.msg->hdr) {
        if (h->type == PJSIP_H_CONTACT) {
            const pjsip_contact_hdr *c = (const pjsip_contact_hdr*)h;
            unsigned e = c->expires;

            if (e != PJSIP_EXPIRES_NOT_SPECIFIED) {
                if (exp)
                    e = exp->ivalue;
                else
                    e = 3600;
            }

            if (e > 0) {
                pjsip_contact_hdr *nc = (pjsip_contact_hdr *)pjsip_hdr_clone(
                                                                tdata->pool, h);
                nc->expires = e;
                pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)nc);
            }
        }
        h = h->next;
    }

    srv = pjsip_generic_string_hdr_create(tdata->pool, NULL, NULL);
    srv->name = pj_str("Server");
    srv->hvalue = pj_str("pjsua simple registrar");
    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)srv);

    status = pjsip_endpt_send_response2(pjsua_get_pjsip_endpt(),
                       rdata, tdata, NULL, NULL);
        if (status != PJ_SUCCESS) {
            pjsip_tx_data_dec_ref(tdata);
        }

}

/*****************************************************************************
 * A simple module to handle otherwise unhandled request. We will register
 * this with the lowest priority.
 */

/* Notification on incoming request */
static pj_bool_t default_mod_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_tx_data *tdata;
    pjsip_status_code status_code;
    pj_status_t status;

    /* Don't respond to ACK! */
    if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method,
                         &pjsip_ack_method) == 0)
        return PJ_TRUE;

    /* Simple registrar */
    if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method,
                         &pjsip_register_method) == 0)
    {
        simple_registrar(rdata);
        return PJ_TRUE;
    }

    /* Create basic response. */
    if (pjsip_method_cmp(&rdata->msg_info.msg->line.req.method, 
                         &pjsip_notify_method) == 0)
    {
        /* Unsolicited NOTIFY's, send with Bad Request */
        status_code = PJSIP_SC_BAD_REQUEST;
    } else {
        /* Probably unknown method */
        status_code = PJSIP_SC_METHOD_NOT_ALLOWED;
    }
    status = pjsip_endpt_create_response(pjsua_get_pjsip_endpt(), 
                                         rdata, status_code, 
                                         NULL, &tdata);
    if (status != PJ_SUCCESS) {
        pjsua_perror(THIS_FILE, "Unable to create response", status);
        return PJ_TRUE;
    }

    /* Add Allow if we're responding with 405 */
    if (status_code == PJSIP_SC_METHOD_NOT_ALLOWED) {
        const pjsip_hdr *cap_hdr;
        cap_hdr = pjsip_endpt_get_capability(pjsua_get_pjsip_endpt(), 
                                             PJSIP_H_ALLOW, NULL);
        if (cap_hdr) {
            pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr *)pjsip_hdr_clone(
                                                         tdata->pool, cap_hdr));
        }
    }

    /* Add User-Agent header */
    {
        pj_str_t user_agent;
        char tmp[80];
        const pj_str_t USER_AGENT = { "User-Agent", 10};
        pjsip_hdr *h;

        pj_ansi_snprintf(tmp, sizeof(tmp), "PJSUA v%s/%s", 
                         pj_get_version(), PJ_OS_NAME);
        pj_strdup2_with_null(tdata->pool, &user_agent, tmp);

        h = (pjsip_hdr*) pjsip_generic_string_hdr_create(tdata->pool,
                                                         &USER_AGENT,
                                                         &user_agent);
        pjsip_msg_add_hdr(tdata->msg, h);
    }

    status = pjsip_endpt_send_response2(pjsua_get_pjsip_endpt(), rdata, tdata, 
                               NULL, NULL);
            if (status != PJ_SUCCESS) pjsip_tx_data_dec_ref(tdata);

    return PJ_TRUE;
}

/* The module instance. */
static pjsip_module mod_default_handler = 
{
    NULL, NULL,                         /* prev, next.          */
    { "mod-default-handler", 19 },      /* Name.                */
    -1,                                 /* Id                   */
    PJSIP_MOD_PRIORITY_APPLICATION+99,  /* Priority             */
    NULL,                               /* load()               */
    NULL,                               /* start()              */
    NULL,                               /* stop()               */
    NULL,                               /* unload()             */
    &default_mod_on_rx_request,         /* on_rx_request()      */
    NULL,                               /* on_rx_response()     */
    NULL,                               /* on_tx_request.       */
    NULL,                               /* on_tx_response()     */
    NULL,                               /* on_tsx_state()       */

};

/** CLI callback **/

/* Called on CLI (re)started, e.g: initial start, after iOS bg */
void cli_on_started(pj_status_t status)
{
    /* Notify app */
    if (app_cfg.on_started) {
        if (status == PJ_SUCCESS) {
            char info[128];
            cli_get_info(info, sizeof(info));
            if (app_cfg.on_started) {
                (*app_cfg.on_started)(status, info);            
            } 
        } else {
            if (app_cfg.on_started) {
                (*app_cfg.on_started)(status, NULL);
            }           
        }
    }
}

/* Called on CLI quit */
void cli_on_stopped(pj_bool_t restart, int argc, char* argv[])
{
    /* Notify app */
    if (app_cfg.on_stopped)
        (*app_cfg.on_stopped)(restart, argc, argv);
}


/* Called on pjsua legacy quit */
void legacy_on_stopped(pj_bool_t restart)
{
    /* Notify app */
    if (app_cfg.on_stopped)
        (*app_cfg.on_stopped)(restart, 1, NULL);
}


static void app_cleanup(pjsip_endpoint *endpt)
{
    PJ_UNUSED_ARG(endpt);
    pj_pool_safe_release(&app_config.pool);
}


/*****************************************************************************
 * Public API
 */

int stdout_refresh_proc(void *arg)
{
    extern char *stdout_refresh_text;

    PJ_UNUSED_ARG(arg);

    /* Set thread to lowest priority so that it doesn't clobber
     * stdout output
     */
    pj_thread_set_prio(pj_thread_this(), 
                       pj_thread_get_prio_min(pj_thread_this()));

    while (!stdout_refresh_quit) {
        pj_thread_sleep(stdout_refresh * 1000);
        puts(stdout_refresh_text);
        fflush(stdout);
    }

    return 0;
}


static pj_status_t app_init(void)
{
    pjsua_transport_id transport_id = -1;
    pjsua_transport_config tcp_cfg;
    unsigned i;
    pj_pool_t *tmp_pool;
    pj_status_t status;

    /** Create pjsua **/
    status = pjsua_create();
    if (status != PJ_SUCCESS)
        return status;

    /* Create pool for application */
    app_config.pool = pjsua_pool_create("pjsua-app", 1000, 1000);
    tmp_pool = pjsua_pool_create("tmp-pjsua", 1000, 1000);

    /* Queue pool release at PJLIB exit */
    pjsip_endpt_atexit(pjsua_get_pjsip_endpt(), &app_cleanup);

    /* Init CLI & its FE settings */
    if (!app_running) {
        pj_cli_cfg_default(&app_config.cli_cfg.cfg);
        pj_cli_telnet_cfg_default(&app_config.cli_cfg.telnet_cfg);
        pj_cli_console_cfg_default(&app_config.cli_cfg.console_cfg);
        app_config.cli_cfg.telnet_cfg.on_started = cli_on_started;
    }

    /** Parse args **/
    status = load_config(app_cfg.argc, app_cfg.argv, &uri_arg);
    if (status != PJ_SUCCESS) {
        pj_pool_release(tmp_pool);
        return status;
    }

    /* Initialize application callbacks */
    app_config.cfg.cb.on_call_state = &on_call_state;
    app_config.cfg.cb.on_stream_destroyed = &on_stream_destroyed;
    app_config.cfg.cb.on_call_media_state = &on_call_media_state;
    app_config.cfg.cb.on_incoming_call = &on_incoming_call;
    app_config.cfg.cb.on_dtmf_digit2 = &call_on_dtmf_callback2;
    app_config.cfg.cb.on_call_rx_text = &call_on_rx_text;
    app_config.cfg.cb.on_call_redirected = &call_on_redirected;
    app_config.cfg.cb.on_reg_state = &on_reg_state;
    app_config.cfg.cb.on_incoming_subscribe = &on_incoming_subscribe;
    app_config.cfg.cb.on_buddy_state = &on_buddy_state;
    app_config.cfg.cb.on_buddy_dlg_event_state = &on_buddy_dlg_event_state;
    app_config.cfg.cb.on_buddy_evsub_state = &on_buddy_evsub_state;
    app_config.cfg.cb.on_buddy_evsub_dlg_event_state = 
        &on_buddy_evsub_dlg_event_state;
    app_config.cfg.cb.on_pager = &on_pager;
    app_config.cfg.cb.on_typing = &on_typing;
    app_config.cfg.cb.on_call_transfer_status = &on_call_transfer_status;
    app_config.cfg.cb.on_call_replaced = &on_call_replaced;
    app_config.cfg.cb.on_nat_detect = &on_nat_detect;
    app_config.cfg.cb.on_mwi_info = &on_mwi_info;
    app_config.cfg.cb.on_transport_state = &on_transport_state;
    app_config.cfg.cb.on_ice_transport_error = &on_ice_transport_error;
    app_config.cfg.cb.on_snd_dev_operation = &on_snd_dev_operation;
    app_config.cfg.cb.on_call_media_event = &on_call_media_event;
    app_config.cfg.cb.on_ip_change_progress = &on_ip_change_progress;
#ifdef TRANSPORT_ADAPTER_SAMPLE
    app_config.cfg.cb.on_create_media_transport = &on_create_media_transport;
#endif

    /* Set sound device latency */
    if (app_config.capture_lat > 0)
        app_config.media_cfg.snd_rec_latency = app_config.capture_lat;
    if (app_config.playback_lat)
        app_config.media_cfg.snd_play_latency = app_config.playback_lat;

    if (app_cfg.on_config_init)
        (*app_cfg.on_config_init)(&app_config);

    /* Initialize pjsua */
    status = pjsua_init(&app_config.cfg, &app_config.log_cfg,
                        &app_config.media_cfg);
    if (status != PJ_SUCCESS) {
        pj_pool_release(tmp_pool);
        return status;
    }

    /* Initialize our module to handle otherwise unhandled request */
    status = pjsip_endpt_register_module(pjsua_get_pjsip_endpt(),
                                         &mod_default_handler);
    if (status != PJ_SUCCESS)
        return status;

#ifdef STEREO_DEMO
    stereo_demo();
#endif

    /* Initialize calls data */
    for (i=0; i<PJ_ARRAY_SIZE(app_config.call_data); ++i) {
        app_config.call_data[i].timer.id = PJSUA_INVALID_ID;
        app_config.call_data[i].timer.cb = &call_timeout_callback;
    }

    /* Optionally registers WAV file */
    for (i=0; i<app_config.wav_count; ++i) {
        pjsua_player_id wav_id;
        unsigned play_options = 0;

        if (app_config.auto_play_hangup)
            play_options |= PJMEDIA_FILE_NO_LOOP;

        status = pjsua_player_create(&app_config.wav_files[i], play_options, 
                                     &wav_id);
        if (status != PJ_SUCCESS)
            goto on_error;

        if (app_config.wav_id == PJSUA_INVALID_ID) {
            app_config.wav_id = wav_id;
            app_config.wav_port = pjsua_player_get_conf_port(app_config.wav_id);
            if (app_config.auto_play_hangup) {
                pjmedia_port *port;

                pjsua_player_get_port(app_config.wav_id, &port);
                status = pjmedia_wav_player_set_eof_cb2(port, NULL, 
                                                        &on_playfile_done);
                if (status != PJ_SUCCESS)
                    goto on_error;

                pj_timer_entry_init(&app_config.auto_hangup_timer, 0, NULL, 
                                    &hangup_timeout_callback);
            }
        }
    }

    /* Optionally registers tone players */
    for (i=0; i<app_config.tone_count; ++i) {
        pjmedia_port *tport;
        char name[80];
        pj_str_t label;
        pj_status_t status2;

        pj_ansi_snprintf(name, sizeof(name), "tone-%d,%d",
                         app_config.tones[i].freq1, 
                         app_config.tones[i].freq2);
        label = pj_str(name);
        status2 = pjmedia_tonegen_create2(app_config.pool, &label,
                                          8000, 1, 160, 16, 
                                          PJMEDIA_TONEGEN_LOOP,  &tport);
        if (status2 != PJ_SUCCESS) {
            pjsua_perror(THIS_FILE, "Unable to create tone generator", status);
            goto on_error;
        }

        status2 = pjsua_conf_add_port(app_config.pool, tport,
                                     &app_config.tone_slots[i]);
        pj_assert(status2 == PJ_SUCCESS);

        status2 = pjmedia_tonegen_play(tport, 1, &app_config.tones[i], 0);
        pj_assert(status2 == PJ_SUCCESS);
    }

    /* Optionally create recorder file, if any. */
    if (app_config.rec_file.slen) {
        status = pjsua_recorder_create(&app_config.rec_file, 0, NULL, 0, 0,
                                       &app_config.rec_id);
        if (status != PJ_SUCCESS)
            goto on_error;

        app_config.rec_port = pjsua_recorder_get_conf_port(app_config.rec_id);
    }

    pj_memcpy(&tcp_cfg, &app_config.udp_cfg, sizeof(tcp_cfg));

    /* Create ringback tones */
    if (app_config.no_tones == PJ_FALSE) {
        unsigned samples_per_frame;
        pjmedia_tone_desc tone[RING_CNT+RINGBACK_CNT];
        pj_str_t name;

        samples_per_frame = app_config.media_cfg.audio_frame_ptime * 
                            app_config.media_cfg.clock_rate *
                            app_config.media_cfg.channel_count / 1000;

        /* Ringback tone (call is ringing) */
        name = pj_str("ringback");
        status = pjmedia_tonegen_create2(app_config.pool, &name, 
                                         app_config.media_cfg.clock_rate,
                                         app_config.media_cfg.channel_count, 
                                         samples_per_frame,
                                         16, PJMEDIA_TONEGEN_LOOP, 
                                         &app_config.ringback_port);
        if (status != PJ_SUCCESS)
            goto on_error;

        pj_bzero(&tone, sizeof(tone));
        for (i=0; i<RINGBACK_CNT; ++i) {
            tone[i].freq1 = RINGBACK_FREQ1;
            tone[i].freq2 = RINGBACK_FREQ2;
            tone[i].on_msec = RINGBACK_ON;
            tone[i].off_msec = RINGBACK_OFF;
        }
        tone[RINGBACK_CNT-1].off_msec = RINGBACK_INTERVAL;

        pjmedia_tonegen_play(app_config.ringback_port, RINGBACK_CNT, tone,
                             PJMEDIA_TONEGEN_LOOP);


        status = pjsua_conf_add_port(app_config.pool, app_config.ringback_port,
                                     &app_config.ringback_slot);
        if (status != PJ_SUCCESS)
            goto on_error;

        /* Ring (to alert incoming call) */
        name = pj_str("ring");
        status = pjmedia_tonegen_create2(app_config.pool, &name, 
                                         app_config.media_cfg.clock_rate,
                                         app_config.media_cfg.channel_count, 
                                         samples_per_frame,
                                         16, PJMEDIA_TONEGEN_LOOP, 
                                         &app_config.ring_port);
        if (status != PJ_SUCCESS)
            goto on_error;

        for (i=0; i<RING_CNT; ++i) {
            tone[i].freq1 = RING_FREQ1;
            tone[i].freq2 = RING_FREQ2;
            tone[i].on_msec = RING_ON;
            tone[i].off_msec = RING_OFF;
        }
        tone[RING_CNT-1].off_msec = RING_INTERVAL;

        pjmedia_tonegen_play(app_config.ring_port, RING_CNT, 
                             tone, PJMEDIA_TONEGEN_LOOP);

        status = pjsua_conf_add_port(app_config.pool, app_config.ring_port,
                                     &app_config.ring_slot);
        if (status != PJ_SUCCESS)
            goto on_error;

    }

    /* Create AVI player virtual devices */
    if (app_config.avi_cnt) {
#if PJMEDIA_HAS_VIDEO && PJMEDIA_VIDEO_DEV_HAS_AVI
        pjmedia_vid_dev_factory *avi_factory;

        status = pjmedia_avi_dev_create_factory(pjsua_get_pool_factory(),
                                                app_config.avi_cnt,
                                                &avi_factory);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(1,(THIS_FILE, status, "Error creating AVI factory"));
            goto on_error;
        }

        for (i=0; i<app_config.avi_cnt; ++i) {
            pjmedia_avi_dev_param avdp;
            pjmedia_vid_dev_index avid;
            unsigned strm_idx, strm_cnt;

            app_config.avi[i].dev_id = PJMEDIA_VID_INVALID_DEV;
            app_config.avi[i].slot = PJSUA_INVALID_ID;

            pjmedia_avi_dev_param_default(&avdp);
            avdp.path = app_config.avi[i].path;

            status =  pjmedia_avi_dev_alloc(avi_factory, &avdp, &avid);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(1,(THIS_FILE, status,
                             "Error creating AVI player for %.*s",
                             (int)avdp.path.slen, avdp.path.ptr));
                goto on_error;
            }

            PJ_LOG(4,(THIS_FILE, "AVI player %.*s created, dev_id=%d",
                      (int)avdp.title.slen, avdp.title.ptr, avid));

            app_config.avi[i].dev_id = avid;
            if (app_config.avi_def_idx == PJSUA_INVALID_ID)
                app_config.avi_def_idx = i;

            strm_cnt = pjmedia_avi_streams_get_num_streams(avdp.avi_streams);
            for (strm_idx=0; strm_idx<strm_cnt; ++strm_idx) {
                pjmedia_port *aud;
                pjmedia_format *fmt;
                pjsua_conf_port_id slot;
                char fmt_name[5];

                aud = pjmedia_avi_streams_get_stream(avdp.avi_streams,
                                                     strm_idx);
                fmt = &aud->info.fmt;

                pjmedia_fourcc_name(fmt->id, fmt_name);

                if (fmt->id == PJMEDIA_FORMAT_PCM) {
                    status = pjsua_conf_add_port(app_config.pool, aud,
                                                 &slot);
                    if (status == PJ_SUCCESS) {
                        PJ_LOG(4,(THIS_FILE,
                                  "AVI %.*s: audio added to slot %d",
                                  (int)avdp.title.slen, avdp.title.ptr,
                                  slot));
                        app_config.avi[i].slot = slot;
                    }
                } else {
                    PJ_LOG(4,(THIS_FILE,
                              "AVI %.*s: audio ignored, format=%s",
                              (int)avdp.title.slen, avdp.title.ptr,
                              fmt_name));
                }
            }
        }
#else
        for (i=0; i<app_config.avi_cnt; ++i) {
            app_config.avi[i].dev_id = PJMEDIA_VID_INVALID_DEV;
            app_config.avi[i].slot = PJSUA_INVALID_ID;
        }

        PJ_LOG(2,(THIS_FILE,
                  "Warning: --play-avi is ignored because AVI is disabled"));
#endif  /* PJMEDIA_VIDEO_DEV_HAS_AVI */
    }

    /* Add UDP transport unless it's disabled. */
    if (!app_config.no_udp) {
        pjsua_acc_id aid;
        pjsip_transport_type_e type = PJSIP_TRANSPORT_UDP;

        status = pjsua_transport_create(type,
                                        &app_config.udp_cfg,
                                        &transport_id);
        if (status != PJ_SUCCESS)
            goto on_error;

        /* Add local account */
        pjsua_acc_add_local(transport_id, PJ_TRUE, &aid);

        /* Adjust local account config based on pjsua app config */
        {
            pjsua_acc_config acc_cfg;
            pjsua_acc_get_config(aid, tmp_pool, &acc_cfg);

            app_config_init_video(&acc_cfg);
            acc_cfg.txt_red_level = app_config.txt_red_level;
            acc_cfg.rtp_cfg = app_config.rtp_cfg;
            pjsua_acc_modify(aid, &acc_cfg);
        }

        //pjsua_acc_set_transport(aid, transport_id);
        pjsua_acc_set_online_status(current_acc, PJ_TRUE);

        if (app_config.udp_cfg.port == 0) {
            pjsua_transport_info ti;
            pj_sockaddr_in *a;

            pjsua_transport_get_info(transport_id, &ti);
            a = (pj_sockaddr_in*)&ti.local_addr;

            tcp_cfg.port = pj_ntohs(a->sin_port);
        }
    }

    /* Add UDP IPv6 transport unless it's disabled. */
    if (!app_config.no_udp && app_config.ipv6) {
        pjsua_acc_id aid;
        pjsip_transport_type_e type = PJSIP_TRANSPORT_UDP6;
        pjsua_transport_config udp_cfg;

        udp_cfg = app_config.udp_cfg;
        if (udp_cfg.port == 0)
            udp_cfg.port = 5060;
        else
            udp_cfg.port += 10;
        status = pjsua_transport_create(type,
                                        &udp_cfg,
                                        &transport_id);
        if (status != PJ_SUCCESS)
            goto on_error;

        /* Add local account */
        pjsua_acc_add_local(transport_id, PJ_TRUE, &aid);

        /* Adjust local account config based on pjsua app config */
        {
            pjsua_acc_config acc_cfg;
            pjsua_acc_get_config(aid, tmp_pool, &acc_cfg);

            app_config_init_video(&acc_cfg);
            acc_cfg.txt_red_level = app_config.txt_red_level;
            acc_cfg.rtp_cfg = app_config.rtp_cfg;
            // acc_cfg.ipv6_media_use = PJSUA_IPV6_ENABLED;
            pjsua_acc_modify(aid, &acc_cfg);
        }

        //pjsua_acc_set_transport(aid, transport_id);
        pjsua_acc_set_online_status(current_acc, PJ_TRUE);

        if (app_config.udp_cfg.port == 0) {
            pjsua_transport_info ti;

            pjsua_transport_get_info(transport_id, &ti);
            tcp_cfg.port = pj_sockaddr_get_port(&ti.local_addr);
        }
    }

    /* Add TCP transport unless it's disabled */
    if (!app_config.no_tcp) {
        pjsua_acc_id aid;

        status = pjsua_transport_create(PJSIP_TRANSPORT_TCP,
                                        &tcp_cfg, 
                                        &transport_id);
        if (status != PJ_SUCCESS)
            goto on_error;

        /* Add local account */
        pjsua_acc_add_local(transport_id, PJ_TRUE, &aid);

        /* Adjust local account config based on pjsua app config */
        {
            pjsua_acc_config acc_cfg;
            pjsua_acc_get_config(aid, tmp_pool, &acc_cfg);

            app_config_init_video(&acc_cfg);
            acc_cfg.txt_red_level = app_config.txt_red_level;
            acc_cfg.rtp_cfg = app_config.rtp_cfg;
            pjsua_acc_modify(aid, &acc_cfg);
        }

        pjsua_acc_set_online_status(current_acc, PJ_TRUE);

    }

    /* Add TCP IPv6 transport unless it's disabled. */
    if (!app_config.no_tcp && app_config.ipv6) {
        pjsua_acc_id aid;
        pjsip_transport_type_e type = PJSIP_TRANSPORT_TCP6;

        tcp_cfg.port += 10;

        status = pjsua_transport_create(type,
                                        &tcp_cfg,
                                        &transport_id);
        if (status != PJ_SUCCESS)
            goto on_error;

        /* Add local account */
        pjsua_acc_add_local(transport_id, PJ_TRUE, &aid);

        /* Adjust local account config based on pjsua app config */
        {
            pjsua_acc_config acc_cfg;
            pjsua_acc_get_config(aid, tmp_pool, &acc_cfg);

            app_config_init_video(&acc_cfg);
            acc_cfg.txt_red_level = app_config.txt_red_level;
            acc_cfg.rtp_cfg = app_config.rtp_cfg;
            // acc_cfg.ipv6_media_use = PJSUA_IPV6_ENABLED;
            pjsua_acc_modify(aid, &acc_cfg);
        }

        //pjsua_acc_set_transport(aid, transport_id);
        pjsua_acc_set_online_status(current_acc, PJ_TRUE);
    }


#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0
    /* Add TLS transport when application wants one */
    if (app_config.use_tls) {

        pjsua_acc_id acc_id;

        /* Copy the QoS settings */
        tcp_cfg.tls_setting.qos_type = tcp_cfg.qos_type;
        pj_memcpy(&tcp_cfg.tls_setting.qos_params, &tcp_cfg.qos_params, 
                  sizeof(tcp_cfg.qos_params));

        /* Set TLS port as TCP port+1 */
        tcp_cfg.port++;
        status = pjsua_transport_create(PJSIP_TRANSPORT_TLS,
                                        &tcp_cfg, 
                                        &transport_id);
        tcp_cfg.port--;
        if (status != PJ_SUCCESS)
            goto on_error;
        
        /* Add local account */
        pjsua_acc_add_local(transport_id, PJ_FALSE, &acc_id);

        /* Adjust local account config based on pjsua app config */
        {
            pjsua_acc_config acc_cfg;
            pjsua_acc_get_config(acc_id, tmp_pool, &acc_cfg);

            app_config_init_video(&acc_cfg);
            acc_cfg.txt_red_level = app_config.txt_red_level;
            acc_cfg.rtp_cfg = app_config.rtp_cfg;
            pjsua_acc_modify(acc_id, &acc_cfg);
        }

        pjsua_acc_set_online_status(acc_id, PJ_TRUE);
    }

    /* Add TLS IPv6 transport unless it's disabled. */
    if (app_config.use_tls && app_config.ipv6) {
        pjsua_acc_id aid;
        pjsip_transport_type_e type = PJSIP_TRANSPORT_TLS6;

        tcp_cfg.port += 10;

        status = pjsua_transport_create(type,
                                        &tcp_cfg,
                                        &transport_id);
        if (status != PJ_SUCCESS)
            goto on_error;

        /* Add local account */
        pjsua_acc_add_local(transport_id, PJ_TRUE, &aid);

        /* Adjust local account config based on pjsua app config */
        {
            pjsua_acc_config acc_cfg;
            pjsua_acc_get_config(aid, tmp_pool, &acc_cfg);

            app_config_init_video(&acc_cfg);
            acc_cfg.txt_red_level = app_config.txt_red_level;
            acc_cfg.rtp_cfg = app_config.rtp_cfg;
            // acc_cfg.ipv6_media_use = PJSUA_IPV6_ENABLED;
            pjsua_acc_modify(aid, &acc_cfg);
        }

        //pjsua_acc_set_transport(aid, transport_id);
        pjsua_acc_set_online_status(current_acc, PJ_TRUE);
    }

#endif

    if (transport_id == -1) {
        PJ_LOG(1,(THIS_FILE, "Error: no transport is configured"));
        status = -1;
        goto on_error;
    }


    /* Add accounts */
    for (i=0; i<app_config.acc_cnt; ++i) {
        app_config.acc_cfg[i].rtp_cfg = app_config.rtp_cfg;
        app_config.acc_cfg[i].reg_retry_interval = 300;
        app_config.acc_cfg[i].reg_first_retry_interval = 60;

        app_config_init_video(&app_config.acc_cfg[i]);
        app_config.acc_cfg[i].txt_red_level = app_config.txt_red_level;

        status = pjsua_acc_add(&app_config.acc_cfg[i], PJ_TRUE, NULL);
        if (status != PJ_SUCCESS)
            goto on_error;
        pjsua_acc_set_online_status(current_acc, PJ_TRUE);
    }

    /* Add buddies */
    for (i=0; i<app_config.buddy_cnt; ++i) {
        status = pjsua_buddy_add(&app_config.buddy_cfg[i], NULL);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(1,(THIS_FILE, status, "Error adding buddy"));
            goto on_error;
        }
    }

    /* Optionally disable some codec */
    for (i=0; i<app_config.codec_dis_cnt; ++i) {
        pjsua_codec_set_priority(&app_config.codec_dis[i],
                                 PJMEDIA_CODEC_PRIO_DISABLED);
#if PJSUA_HAS_VIDEO
        pjsua_vid_codec_set_priority(&app_config.codec_dis[i],
                                     PJMEDIA_CODEC_PRIO_DISABLED);
#endif
    }

    /* Optionally set codec orders */
    for (i=0; i<app_config.codec_cnt; ++i) {
        pjsua_codec_set_priority(&app_config.codec_arg[i],
                                 (pj_uint8_t)(PJMEDIA_CODEC_PRIO_NORMAL+i+9));
#if PJSUA_HAS_VIDEO
        pjsua_vid_codec_set_priority(&app_config.codec_arg[i],
                                   (pj_uint8_t)(PJMEDIA_CODEC_PRIO_NORMAL+i+9));
#endif
    }

    /* Use null sound device? */
#ifndef STEREO_DEMO
    if (app_config.null_audio) {
        status = pjsua_set_null_snd_dev();
        if (status != PJ_SUCCESS)
            goto on_error;
    }
#endif

    if (app_config.capture_dev  != PJSUA_INVALID_ID ||
        app_config.playback_dev != PJSUA_INVALID_ID) 
    {
        status = pjsua_set_snd_dev(app_config.capture_dev, 
                                   app_config.playback_dev);
        if (status != PJ_SUCCESS)
            goto on_error;
    }

    /* Init call setting */
    pjsua_call_setting_default(&call_opt);
    call_opt.aud_cnt = app_config.aud_cnt;
    call_opt.vid_cnt = app_config.vid.vid_cnt;
    call_opt.txt_cnt = app_config.txt_cnt;
    if (app_config.enable_loam) {
        call_opt.flag |= PJSUA_CALL_NO_SDP_OFFER;
    }

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0
    /* Wipe out TLS key settings in transport configs */
    pjsip_tls_setting_wipe_keys(&app_config.udp_cfg.tls_setting);
#endif

    pj_pool_release(tmp_pool);
    return PJ_SUCCESS;

on_error:

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0
    /* Wipe out TLS key settings in transport configs */
    pjsip_tls_setting_wipe_keys(&app_config.udp_cfg.tls_setting);
#endif

    pj_pool_release(tmp_pool);
    app_destroy();
    return status;
}

pj_status_t pjsua_app_init(const pjsua_app_cfg_t *cfg)
{
    pj_status_t status;
    pj_memcpy(&app_cfg, cfg, sizeof(app_cfg));

    status = app_init();
    if (status != PJ_SUCCESS)
        return status;

    /* Init CLI if configured */    
    if (app_config.use_cli) {
        status = cli_init();
    } 
    return status;
}

pj_status_t pjsua_app_run(pj_bool_t wait_telnet_cli)
{
    pj_thread_t *stdout_refresh_thread = NULL;
    pj_status_t status;

    /* Start console refresh thread */
    if (stdout_refresh > 0) {
        status = pj_thread_create(app_config.pool, "stdout", 
                                  &stdout_refresh_proc,
                                  NULL, 0, 0, &stdout_refresh_thread);
        PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);
    }

    status = pjsua_start();
    if (status != PJ_SUCCESS)
        goto on_return;

    if (app_config.use_cli && (app_config.cli_cfg.cli_fe & CLI_FE_TELNET)) {
        char info[128];
        cli_get_info(info, sizeof(info));
        if (app_cfg.on_started) {
            (*app_cfg.on_started)(status, info);
        }
    } else {
        if (app_cfg.on_started) {
            (*app_cfg.on_started)(status, "Ready");
        }    
    }

    /* If user specifies URI to call, then call the URI */
    if (uri_arg.slen) {
        pjsua_call_setting_default(&call_opt);
        call_opt.aud_cnt = app_config.aud_cnt;
        call_opt.vid_cnt = app_config.vid.vid_cnt;
        call_opt.txt_cnt = app_config.txt_cnt;

        pjsua_call_make_call(current_acc, &uri_arg, &call_opt, NULL, 
                             NULL, NULL);
    }   

    app_running = PJ_TRUE;

    if (app_config.use_cli)
        cli_main(wait_telnet_cli);      
    else
        legacy_main();

    status = PJ_SUCCESS;

on_return:
    if (stdout_refresh_thread) {
        stdout_refresh_quit = PJ_TRUE;
        pj_thread_join(stdout_refresh_thread);
        pj_thread_destroy(stdout_refresh_thread);
        stdout_refresh_quit = PJ_FALSE;
    }
    return status;
}

static pj_status_t app_destroy(void)
{
    pj_status_t status = PJ_SUCCESS;
    unsigned i;
    pj_bool_t use_cli = PJ_FALSE;
    int cli_fe = 0;
    pj_uint16_t cli_telnet_port = 0;

#ifdef STEREO_DEMO
    if (app_config.snd) {
        pjmedia_snd_port_destroy(app_config.snd);
        app_config.snd = NULL;
    }
    if (app_config.sc_ch1) {
        pjsua_conf_remove_port(app_config.sc_ch1_slot);
        app_config.sc_ch1_slot = PJSUA_INVALID_ID;
        pjmedia_port_destroy(app_config.sc_ch1);
        app_config.sc_ch1 = NULL;
    }
    if (app_config.sc) {
        pjmedia_port_destroy(app_config.sc);
        app_config.sc = NULL;
    }
#endif

    /* Close avi devs and ports */
    for (i=0; i<app_config.avi_cnt; ++i) {
        if (app_config.avi[i].slot != PJSUA_INVALID_ID) {
            pjsua_conf_remove_port(app_config.avi[i].slot);
            app_config.avi[i].slot = PJSUA_INVALID_ID;
        }
#if PJMEDIA_HAS_VIDEO && PJMEDIA_VIDEO_DEV_HAS_AVI
        if (app_config.avi[i].dev_id != PJMEDIA_VID_INVALID_DEV) {
            pjmedia_avi_dev_free(app_config.avi[i].dev_id);
            app_config.avi[i].dev_id = PJMEDIA_VID_INVALID_DEV;
        }
#endif
    }

    /* Close ringback port */
    if (app_config.ringback_port && 
        app_config.ringback_slot != PJSUA_INVALID_ID) 
    {
        pjsua_conf_remove_port(app_config.ringback_slot);
        app_config.ringback_slot = PJSUA_INVALID_ID;
        pjmedia_port_destroy(app_config.ringback_port);
        app_config.ringback_port = NULL;
    }

    /* Close ring port */
    if (app_config.ring_port && app_config.ring_slot != PJSUA_INVALID_ID) {
        pjsua_conf_remove_port(app_config.ring_slot);
        app_config.ring_slot = PJSUA_INVALID_ID;
        pjmedia_port_destroy(app_config.ring_port);
        app_config.ring_port = NULL;
    }

    /* Close wav player */
    if (app_config.wav_id != PJSUA_INVALID_ID) {
        pjsua_player_destroy(app_config.wav_id);
        app_config.wav_id = PJSUA_INVALID_ID;
        app_config.wav_port = PJSUA_INVALID_ID;
    }

    /* Close wav recorder */
    if (app_config.rec_id != PJSUA_INVALID_ID) {
        pjsua_recorder_destroy(app_config.rec_id);
        app_config.rec_id = PJSUA_INVALID_ID;
        app_config.rec_port = PJSUA_INVALID_ID;
    }

    /* Close tone generators */
    for (i=0; i<app_config.tone_count; ++i) {
        if (app_config.tone_slots[i] != PJSUA_INVALID_ID) {
            pjsua_conf_remove_port(app_config.tone_slots[i]);
            app_config.tone_slots[i] = PJSUA_INVALID_ID;
        }
    }

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0
    /* Wipe out TLS key settings in transport configs */
    pjsip_tls_setting_wipe_keys(&app_config.udp_cfg.tls_setting);
#endif

    /* The pool release has been scheduled via pjsip_endpt_atexit().
     *
     * We can only release the pool after audio & video conference destroy.
     * Note that pjsua_conf_remove_port()/pjsua_vid_conf_remove_port()
     * is asynchronous, so when sound device is not active, PJMEDIA ports
     * have not been removed from the conference (and destroyed) yet
     * until the audio & video conferences are destroyed (in pjsua_destroy()).
     */
    //pj_pool_safe_release(&app_config.pool);

    status = pjsua_destroy();

    if (app_config.use_cli) {
        use_cli = app_config.use_cli;
        cli_fe = app_config.cli_cfg.cli_fe;
        cli_telnet_port = app_config.cli_cfg.telnet_cfg.port;   
    }

    /* Reset config */
    pj_bzero(&app_config, sizeof(app_config));
    app_config.wav_id = PJSUA_INVALID_ID;
    app_config.rec_id = PJSUA_INVALID_ID;

    if (use_cli) {    
        app_config.use_cli = use_cli;
        app_config.cli_cfg.cli_fe = cli_fe;
        app_config.cli_cfg.telnet_cfg.port = cli_telnet_port;
    }

    return status;
}

pj_status_t pjsua_app_destroy(void)
{
    pj_status_t status;

    status = app_destroy();

    if (app_config.use_cli) {   
        cli_destroy();
    }
    
    return status;
}

/** ======================= **/

#ifdef STEREO_DEMO
/*
 * In this stereo demo, we open the sound device in stereo mode and
 * arrange the attachment to the PJSUA-LIB conference bridge as such
 * so that channel0/left channel of the sound device corresponds to
 * slot 0 in the bridge, and channel1/right channel of the sound
 * device corresponds to slot 1 in the bridge. Then user can independently
 * feed different media to/from the speakers/microphones channels, by
 * connecting them to slot 0 or 1 respectively.
 *
 * Here's how the connection looks like:
 *
   +-----------+ stereo +-----------------+ 2x mono +-----------+
   | AUDIO DEV |<------>| SPLITCOMB   left|<------->|#0  BRIDGE |
   +-----------+        |            right|<------->|#1         |
                        +-----------------+         +-----------+
 */
static void stereo_demo()
{
    pjmedia_port *conf;
    pj_status_t status;

    /* Disable existing sound device */
    conf = pjsua_set_no_snd_dev();

    /* Create stereo-mono splitter/combiner */
    status = pjmedia_splitcomb_create(app_config.pool, 
                                PJMEDIA_PIA_SRATE(&conf->info) /* clock rate */,
                                2           /* stereo */,
                                2 * PJMEDIA_PIA_SPF(&conf->info),
                                PJMEDIA_PIA_BITS(&conf->info),
                                0           /* options */,
                                &app_config.sc);
    pj_assert(status == PJ_SUCCESS);

    /* Connect channel0 (left channel?) to conference port slot0 */
    status = pjmedia_splitcomb_set_channel(app_config.sc, 0 /* ch0 */, 
                                           0 /*options*/,
                                           conf);
    pj_assert(status == PJ_SUCCESS);

    /* Create reverse channel for channel1 (right channel?)... */
    status = pjmedia_splitcomb_create_rev_channel(app_config.pool,
                                                  app_config.sc,
                                                  1  /* ch1 */,
                                                  0  /* options */,
                                                  &app_config.sc_ch1);
    pj_assert(status == PJ_SUCCESS);

    /* .. and register it to conference bridge (it would be slot1
     * if there's no other devices connected to the bridge)
     */
    status = pjsua_conf_add_port(app_config.pool, app_config.sc_ch1, 
                                 &app_config.sc_ch1_slot);
    pj_assert(status == PJ_SUCCESS);
    
    /* Create sound device */
    status = pjmedia_snd_port_create(app_config.pool, -1, -1, 
                                     PJMEDIA_PIA_SRATE(&conf->info),
                                     2      /* stereo */,
                                     2 * PJMEDIA_PIA_SPF(&conf->info),
                                     PJMEDIA_PIA_BITS(&conf->info),
                                     0, &app_config.snd);

    pj_assert(status == PJ_SUCCESS);


    /* Connect the splitter to the sound device */
    status = pjmedia_snd_port_connect(app_config.snd, app_config.sc);
    pj_assert(status == PJ_SUCCESS);
}
#endif

/*****************************************************************************
 * Media port
 */

/** Media port identification */
typedef int pjsua_mport_id;

/**
 * Size of internal media port record buffer (in ms)
 */
#ifndef PJSUA_MPORT_RECORD_BUFFER_SIZE
#   define PJSUA_MPORT_RECORD_BUFFER_SIZE	1250
#endif


/**
 * Size of internal media port replay buffer (in ms)
 */
#ifndef PJSUA_MPORT_REPLAY_BUFFER_SIZE
#   define PJSUA_MPORT_REPLAY_BUFFER_SIZE	1250
#endif

/*****************************************************************************
* 
* 
* 
*
 * Low-level media/audio port/channel API (similar to memory players and recorders).
 */

/**
 * Create/allocate a media port, and automatically add this port to
 * the conference bridge so that its connectivity can be controlled
 * and play, record and/or conferencing operations can be initiated
 * as necessary.
 *
 * @param dir					Media port's direction.
 * @param enable_vad			Set to true to enable VAD features during record.
 * @param record_buffer_size	Desired size of internal record buffer (in ms).
 *								If 0, the default value in mport_record_buffer_size
 *								will be used instead.
 * @param record_data_threshold	Threshold, in ms, that should be used to signal
 *								the record event so that the application can collect
 *								the recorded data. The record event will signaled
 *								whilst the available/free space in the record buffer
 *								is below this threshold. If 0, the default value of
 *								250 will be used;
 * @param play_buffer_size		Desired size of internal play buffer (in ms).
 *								If 0, the default value in mport_replay_buffer_size
 *								will be used instead.
 * @param play_data_threshold	Threshold, in ms, that should be used to signal
 *								the play event so that the application can supply
 *								additional data. The play event will signaled
 *								whilst the available/unplayed data in the play buffer
 *								is below this threshold. If 0, the default value of
 *								250 will be used;
 * @param p_id					Pointer to receive media port ID.
 *
 * @return						PJ_SUCCESS on success, or the appropriate error
 *								code.
 */
PJ_DECL(pj_status_t) pjsua_mport_alloc(pjmedia_dir dir,
									   pj_bool_t enable_vad,
									   pj_size_t record_buffer_size,
									   pj_size_t record_data_threshold,
									   pj_size_t play_buffer_size,
									   pj_size_t play_data_threshold,
									   pjsua_mport_id *p_id);


/**
 * Destroy/free media port, remove it from the bridge, and free
 * associated resources.
 *
 * @param id		The media port ID.
 *
 * @return			PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_free(pjsua_mport_id id);

/**
 * Get conference port ID associated with media port.
 *
 * @param id		The media port ID.
 *
 * @return			Conference port ID associated with this media port.
 */
PJ_DECL(pjsua_conf_port_id) pjsua_mport_get_conf_port(pjsua_mport_id id);

/**
 * Get the underlying pjmedia_port for the media port.
 *
 * @param id		The media port ID.
 * @param p_port	The media port associated with the player.
 *
 * @return			PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsua_mport_get_port(pjsua_mport_id id,
										  pjmedia_port **p_port);

/**
 * Get the media port's direction.
 *
 * @param id		The media port ID.
 *
 * @return			Direction.
 */
PJ_DECL(pjmedia_dir) pjsua_mport_get_dir(pjsua_mport_id id);

/**
 * Get media port's replay event object. The returned event object is
 * valid only until pjsua_mport_free() is invoked and must therefore
 * not be accessed after that point.
 *
 * @param id		The media port ID.
 *
 * @return			Event object that is signaled when the status of the
 *					replay operation changes indicating that the
 *					application should invoke pjsua_mport_play_status().
 */
PJ_DECL(pj_event_t *) pjsua_mport_get_play_event(pjsua_mport_id id);

/**
 * Get media port's record event object. The returned event object is
 * valid only until pjsua_mport_free() is invoked and must therefore
 * not be accessed after that point.
 *
 * @param id		The media port ID.
 *
 * @return			Event object that is signaled when the status of the
 *					record operation changes indicating that the
 *					application should invoke pjsua_mport_record_status().
 */
PJ_DECL(pj_event_t *) pjsua_mport_get_record_event(pjsua_mport_id id);

/**
 * Get media port's recognition event object. The returned event object
 * is valid only until pjsua_mport_free() is invoked and must therefore
 * not be accessed after that point.
 *
 * @param id		The media port ID.
 *
 * @return			Event object that is signaled when a recognition
 *					event is available for collection via
 *					pjsua_mport_get_recognised().
 */
PJ_DECL(pj_event_t *) pjsua_mport_get_recognition_event(pjsua_mport_id id);

/**
 * Initiate a replay operation. If successful, the replay will continue
 * until pjsua_mport_play_status() returns with the 'completed' flag set
 * after all the supplied data is played. Additional data can be periodically
 * supplied by invoking pjsua_mport_play_put_data() once the play event is
 * signaled.
 *
 * @param id		The media port ID.
 * @param fmt		Format/ecoding details for the replay data.
 * @param data		Optional address of initial block of data that should be
 *					replayed.
 * @param size		Number of bytes/octets of replay data provided in the
 *					initial block.
 * @param count		Address of count that will receive the number of bytes
 *					of replay data that were copied from the supplied initial
 *					data block to the internal replay buffer.
 *
 * @return			PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_play_start(pjsua_mport_id id,
											const pjmedia_format *fmt,
											const void *data,
											pj_size_t size,
											pj_size_t *count);

/**
 * Status information about the play operation.
 */
typedef struct pjsua_mport_play_info
{
	/**
	 * Number of samples actually played thus far.
	 */
	pj_uint64_t	samples_played;

	/**
	 * Flag which is set to true if the replay has completed.
	 */
	pj_bool_t	completed;

	/**
	 * Flag which is set to true if an underrun has occurred
	 * since the last invokation of pjsua_mport_play_status().
	 */
	pj_bool_t	underrun;

	/**
	 * Space, in samples, available in the internal buffer
	 * for new replay data.
	 */
	pj_uint32_t		free_buffer_size;

} pjs_mport_play_info;

/**
 * Get status info about the current play operation.
 *
 * @param id		The media port ID.
 * @param info		The status.
 *
 * @return			PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_play_status(pjsua_mport_id id,
											 pjs_mport_play_info *info);

/**
 * Supply the next chunk of data to be replayed. If no data is supplied,
 * then this taken as an idication that no more data is available and
 * that the replay should complete when the already buffered data is
 * replayed.
 *
 * @param id		The media port ID.
 * @param data		Address of next block of data that should be replayed.
 * @param size		Number of bytes/octets of replay data provided in the
 *					data block.
 * @param count		Address of count that will receive the number of bytes
 *					of replay data that were copied from the supplied data
 *					block to the internal replay buffer.
 *
 * @return			PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_play_put_data(pjsua_mport_id id,
	const void *data,
	pj_size_t size,
	pj_size_t *count);

/**
 * Stop/abort the current play operation.
 *
 * @param id		The file player ID.
 * @param discard	Flag which should be set to discard any unplayed data
 *					in the internal replay buffer (i.e., stop immediately).
 *					If not set, the replay operation will continue until
 *					all the already internally buffered data is played.
 *
 * @return			PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_play_stop(pjsua_mport_id id, pj_bool_t discard);


/**
 * Initiate a record operation. If successful, the record will continue
 * until pjsua_mport_record_status() returns with the 'completed' flag
 * set after a completion condition has been met. The recorded data can be
 * retreived periodically (when the record event is signaled) by invoking
 * pjsua_mport_record_get_data().
 *
 * @param id				The media port ID.
 * @param fmt				Format/ecoding details for the recorded data.
 * @param rec_output		Flag which should be set to PJ_TRUE if the
 *							port's output (player or transmit direction)
 *							should be recorded. Otherwise, the port's input
 *							will be recorded.
 * @param max_duration		Maximum duration, in ms, for the recording. 0
 *							if no limit.
 * @param max_samples		Maximum number of samples to record. 0 if no
 *							limit.
 * @param max_silence		Max period of silence, in ms, before the
 *							recording is terminated. 0 if no limit.
 *@param eliminate_silence	The maximum duration, in ms, of silence to
 *							record. Silences longer than this are truncated
 *							to this length. 0 disables silence elimination.
 *
 * @return					PJ_SUCCESS on success, or the appropriate error
 *							code.
 */
PJ_DECL(pj_status_t) pjsua_mport_record_start(pjsua_mport_id id,
	const pjmedia_format *fmt,
	pj_bool_t rec_output,
	pj_size_t max_duration,
	pj_size_t max_samples,
	pj_size_t max_silence,
	pj_size_t eliminate_silence);

/**
* Status information about the record operation.
*/
typedef enum pjsua_record_end_reason
{
	PJSUA_REC_ER_NONE,
	PJSUA_REC_ER_MAX_SAMPLES,
	PJSUA_REC_ER_MAX_DURATION,
	PJSUA_REC_ER_MAX_SILENCE,
	PJSUA_REC_ER_STOP
} pjs_record_end_reason;
typedef struct pjsua_mport_record_info
{
	/**
	 * Total number of samples actually recorded thus far.
	 */
	pj_uint64_t	samples_recorded;

	/**
	 * Flag which is set to true if the record has completed.
	 */
	pj_bool_t	completed;

	/**
	 * Identifies the reason the recording was completed.
	 */
	pjs_record_end_reason end_reason;

	/**
	 * Flag which is set to true if an overrun has occurred
	 * since the last invokation of pjsua_mport_record_status().
	 */
	pj_bool_t	overrun;

	/**
	 * Number of samples available for immediate retreival from
	 * the internal record buffer.
	 */
	pj_uint32_t		samples_available;

} pjs_mport_record_info;

/**
 * Get the status of the current record operation.
 *
 * @param id		The media port ID.
 * @param info		The info.
 *
 * @return			PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_record_status(pjsua_mport_id id,
	pjs_mport_record_info *info);

/**
 * Get the next chunk of data that was recorded.
 *
 * @param id		The media port ID.
 * @param data		Address of buffer that should receive the data.
 * @param size		Size of data buffer in bytes.
 * @param count		Address of count that will receive the number of bytes
 *					of record data that were copied to the supplied buffer.
 *
 * @return			PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_record_get_data(pjsua_mport_id id,
	void *data,
	pj_size_t size,
	pj_size_t *count);

/**
 * Stop/abort the current record operation.
 *
 * @param id		The media port ID.
 * @param discard	Flag which should be set to discard any uncollected
 *					data in the internal record buffer.
 *					The media port will continue to be 'reserved' for
 *					recording purposes until pjsua_mport_record_status()
 *					returns with the 'completed' flag set.
 *
 * @return			PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_record_stop(pjsua_mport_id id, pj_bool_t discard);


/**
 * Initiate a conference operation. If successful, the port's output will
 * contain the mixed result of the inputs of all the conference's
 * participants. A play operation can not be active at the same time as
 * a conference on the same port/channel. The conference will continue
 * until #pjsua_mport_conf_stop() is invoked. Participants can be added
 * and removed using #pjsua_mport_conf_add() and
 * #pjsua_mport_conf_remove() respectively.
 *
 * @param id		The media port ID.
 *
 * @return			PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_conf_start(pjsua_mport_id id);

/**
 * Add new participant to the conference on the media port.
 *
 * @param id		The media port ID.
 * @param pid		The media port ID of the participant to add.
 *
 * @return			PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_conf_add(pjsua_mport_id id, pjsua_mport_id pid);

/**
 * Remove participant from the conference on the media port.
 *
 * @param id		The media port ID.
 * @param pid		The media port ID of the participant to remove.
 *
 * @return			PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_conf_remove(pjsua_mport_id id, pjsua_mport_id pid);

/**
 * Stop conference operation.
 *
 * @param id		The media port ID.
 *
 * @return			PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_conf_stop(pjsua_mport_id id);


/**
* Recognition event types
*/
typedef enum pjsua_recognition_type
{
	PJSUA_RCG_NONE = 0
	,PJSUA_RCG_DTMF_RFC2833 = 1
	,PJSUA_RCG_DTMF_TONE = 2
	,PJSUA_RCG_DTMF = (PJSUA_RCG_DTMF_RFC2833 | PJSUA_RCG_DTMF_TONE)
	//,PJSUA_RCG_GRUNT = 4
	//,PJSUA_RCG_CALL_PROGRESS_TONE = 8
	//,PJSUA_RCG_TONE = 16
	//,PJSUA_RCG_LIVE_SPEAKER = 32
} pjs_recognition_type;

typedef struct pjsua_listen_for_parms {
	/**
	 * Bitmask of the detector types that should be activated or
	 * PJSUA_RCG_NONE to disable all detectors.
	 *
	 * PJSUA_RCG_DTMF_RFC2833 does not represent a detector that can
	 * be activated on the audio stream, however, it allows the
	 * application's #on_dtmf_digit() callback handler to use the
	 * same consistent event delivery mechanism for RFC2833 received
	 * DTMF digits as those received directly as inband tones from
	 * peers that do not support RFC2833. RFC2833 digits can be
	 * queued by the application by invoking
	 * #pjsua_mport_add_rfc2833_dtmf_digit(). If desired,
	 * PJSUA_RCG_DTMF_TONE can be added to detect inband tones. If
	 * both RFC2833 and tone based DTMF tone detectors are activated
	 * (i.e., PJSUA_RCG_DTMF), then the first event queued of either
	 * type will automatically deactivate the other detector to
	 * prevent the potential of digit duplication.
	 *
	 * PJSUA_RCG_GRUNT activates a grunt detector that generates an
	 * event whenever the silence state of the audio stream changes
	 * based on an adaptive noise threshold algorithm.
	 */
	unsigned	types;

	//int		grunt_latency;
	//double	min_noise_level;
	//double	grunt_threshold;
} pjs_listen_for_parms;

/**
 * Start or stop one or more of a series of detectors that can be
 * activated on a media port. Once a detector is activated, detection
 * events can be retreived by invoking #pjsua_mport_get_recognised()
 * after the recognition event, which can be retreived by invoking
 * #pjsua_mport_get_recognition_event(), is signaled.
 *
 * @param id		The media port ID.
 * @param params	Details of detectors to be activated or NULL to
 *					disable all detectors.
 *
 * @return			PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsua_mport_listen_for(pjsua_mport_id id, pjs_listen_for_parms* params);

typedef struct pjsua_recognition_info {
	pjs_recognition_type	type;
	unsigned				timestamp;
	unsigned				param0;
	unsigned				param1;
} pjs_recognition_info;

/**
 * Get the next available detection event.
 *
 * @param id		The media port ID.
 * @param info		Details of detected event.
 *
 * @return			PJ_SUCCESS on success, or the appropriate error code.
 */

PJ_DECL(pj_status_t) pjsua_mport_get_recognised(pjsua_mport_id id, pjs_recognition_info* info);

/**
 * Discard any recognition events currently queued in the internal buffer.
 *
 * @param id		The media port ID.
 *
 * @return			PJ_SUCCESS on success, or the appropriate error code.
 */

PJ_DECL(pj_status_t) pjsua_mport_discard_recognised(pjsua_mport_id id);

/**
 * Add a RFC2833 DTMF digit to the internal recognition event queue.
 *
 * @param id		The media port ID.
 * @param digit		The actual digit.
 * @param timestamp	The RTP timestamp associated with the digit.
 * @param duration	The duration, in ms, of the digit.
 *
 * @return			PJ_SUCCESS on success PJ_EIGNORED if the port is
 *					not currently listening for RFC2833 digits, or
 *					the appropriate error code.
 */

PJ_DECL(pj_status_t) pjsua_mport_add_rfc2833_dtmf_digit(pjsua_mport_id id, char digit, unsigned timestamp, unsigned duration);


/**
 * Media port data.
 */
typedef enum pjsua_record_status
{
	PJSUA_REC_IDLE,
	PJSUA_REC_RUNNING,
	PJSUA_REC_STOPPING
} pjsua_record_status;
typedef struct pjsua_mport_record_data
{
	pjsua_record_status				status;
	pjmedia_format_id				fmt_id;
	pj_size_t						max_samples;
	pj_size_t						max_duration;
	pj_size_t						max_silence;
	pj_size_t						eliminate_silence;
	pj_uint64_t						samples_seen;
	pj_uint64_t						samples_recorded;
	pj_timestamp					vad_timestamp;
	pjmedia_silence_det				*vad;
	pjmedia_circ_buf				*buffer;
	pj_event_t						*event;
	pj_size_t						buffer_size;
	pj_size_t						threshold;
	pj_bool_t						signaled;
	pj_bool_t						overrun;
	pj_bool_t						is_silence;
	pj_bool_t						rec_output;
	pjs_record_end_reason			er;
} pjsua_mport_record_data;
typedef struct pjsua_mport_recognition_data
{
	pj_event_t						*event;
	pj_uint32_t						event_cnt;
	pjs_listen_for_parms			params;
	pjs_recognition_info			events[32];
	pj_bool_t						signaled;
	pj_bool_t						overrun;
} pjsua_mport_recognition_data;
typedef enum pjsua_replay_status
{
	PJSUA_REP_IDLE,
	PJSUA_REP_RUNNING,
	PJSUA_REP_STOPPING,
	PJSUA_REP_CONFERENCING
} pjsua_replay_status;
typedef struct pjsua_mport_replay_data
{
	pjsua_replay_status				status;
	pjmedia_format_id				fmt_id;
	pj_uint64_t						samples_played;
	pj_timestamp					timestamp;
	pjmedia_circ_buf				*buffer;
	pj_event_t						*event;
	pj_size_t						buffer_size;
	pj_size_t						threshold;
	pj_bool_t						signaled;
	pj_bool_t						underrun;
} pjsua_mport_replay_data;
typedef struct pjsua_mport_data
{
	pjmedia_port					base;
	pj_pool_t						*pool;
	pjsua_conf_port_id				slot;
	pj_uint32_t						participant_cnt;
	pjsua_mport_id					*participants;
	pj_uint32_t						listener_cnt;
	pjsua_mport_id					*listeners;
	pj_uint32_t						mix_cnt;
	int								mix_adj;
	int								last_mix_adj;
	pj_int32_t						*mix_buf;
	pjsua_mport_record_data			record_data;
	pjsua_mport_replay_data			play_data;
	pjsua_mport_recognition_data	recogntion_data;
} pjsua_mport_data;

typedef struct pjsua_conf_setting
{
    unsigned    channel_count;
    unsigned    samples_per_frame;
    unsigned    bits_per_sample;
} pjsua_conf_setting;

// TODO: implement your own lock
#define PJSUA_LOCK()
#define PJSUA_UNLOCK()

struct {
    pjsua_media_config   media_cfg; /**< Media config.                  */
    pjsua_conf_setting   mconf_cfg; /**< Additionan conf. bridge. param */
    pjmedia_conf        *mconf;     /**< Conference bridge.             */

    /* Media ports/channels */
    unsigned			mport_cnt;	/**< Number of media channels.	*/
	pjsua_mport_id		mport_id;	/**< Id of last media port that was allocated */
	pjsua_mport_data	*mport;		/**< Array of media channels.*/
} pjsua_var;

#define SIGNATURE	PJMEDIA_SIG_CLASS_PORT_AUD('A','P')

#define NORMAL_LEVEL		128

 /* These are settings to control the adaptivity of changes in the
 * signal level of the ports, so that sudden change in signal level
 * in the port does not cause misaligned signal (which causes noise).
 */
#define ATTACK_A			(pjsua_var.media_cfg.clock_rate / pjsua_var.mconf_cfg.samples_per_frame)
#define ATTACK_B			1
#define DECAY_A				0
#define DECAY_B				1

#define SIMPLE_AGC(last, target) \
	if (target >= last) \
		target = (ATTACK_A*(last+1)+ATTACK_B*target)/(ATTACK_A+ATTACK_B); \
	else \
		target = (DECAY_A*last+DECAY_B*target)/(DECAY_A+DECAY_B)

#define MAX_LEVEL			(32767)
#define MIN_LEVEL			(-32768)

#define IS_OVERFLOW(s)		(((s) > MAX_LEVEL) || ((s) < MIN_LEVEL))


static void mport_rec_frame(pjsua_mport_data *data, pjmedia_frame *frame)
{
	pj_size_t size, avl, count;
	pj_bool_t is_silence = PJ_FALSE;
	pj_size_t silence_samples = 0;
	pj_int16_t *buf = (pj_int16_t *)frame->buf;

	if (data->record_data.status != PJSUA_REC_RUNNING)
		return;

	if (frame->type == PJMEDIA_FRAME_TYPE_NONE)
	{
		count = size = pjsua_var.mconf_cfg.samples_per_frame;
		buf = (pj_int16_t *)pj_pool_alloc(data->pool, sizeof(*buf) * size);
		pj_bzero(buf, sizeof(*buf) * size);
	}
	else
	{
		count = size = (frame->size >> 1);
	}

	pj_assert(data->record_data.buffer != NULL);

	if (data->record_data.max_samples)
	{
		pj_assert(data->record_data.max_samples > data->record_data.samples_recorded);
		avl = (pj_size_t)((pj_uint64_t)data->record_data.max_samples - data->record_data.samples_recorded);
		if (count > avl)
			count = avl;
	}
	if (data->record_data.max_duration)
	{
		pj_assert(data->record_data.max_duration > data->record_data.samples_seen);
		avl = data->record_data.max_duration - (pj_size_t)data->record_data.samples_seen;
		if (count > avl)
			count = avl;
	}
	if ((data->record_data.max_silence || data->record_data.eliminate_silence) && data->record_data.vad)
	{
		is_silence = pjmedia_silence_det_detect(data->record_data.vad, (const pj_int16_t *)buf, size, NULL);
		if (is_silence)
		{
			if (!data->record_data.is_silence)
			{
				data->record_data.is_silence = PJ_TRUE;
				data->record_data.vad_timestamp = frame->timestamp;
			}
			else
			{
				pj_assert(frame->timestamp.u64 > data->record_data.vad_timestamp.u64);
			}
			silence_samples = (pj_size_t)(frame->timestamp.u64 - data->record_data.vad_timestamp.u64);
			if (data->record_data.eliminate_silence)
			{
				if ((silence_samples + count) >= data->record_data.eliminate_silence)
				{
					count = 0;
				}
				else
				{
					avl = (data->record_data.eliminate_silence - silence_samples);
					if (count > avl)
						count = avl;
				}
			}
			if (data->record_data.max_silence)
			{
				pj_assert(data->record_data.max_silence > silence_samples);
				avl = silence_samples - data->record_data.max_silence;
				if (count > avl)
					count = avl;
			}
		}
		else if (data->record_data.is_silence)
		{
			data->record_data.is_silence = PJ_FALSE;
			data->record_data.vad_timestamp = frame->timestamp;
		}
	}

	pj_assert(data->record_data.buffer->capacity >= data->record_data.buffer->len);
	avl = (data->record_data.buffer->capacity - data->record_data.buffer->len);
	if (count > avl)
	{
		if (!data->record_data.overrun)
		{
			PJ_LOG(3, (THIS_FILE, "Record buffer overrun for %s: avl=%lu, size=%lu", data->base.info.name.ptr, avl, size));
			data->record_data.overrun = PJ_TRUE;
		}
		if ((data->base.info.fmt.type == PJMEDIA_TYPE_AUDIO) &&
			(data->base.info.fmt.detail_type == PJMEDIA_FORMAT_DETAIL_AUDIO) &&
			(data->base.info.fmt.det.aud.channel_count > 1))
		{
			// Take account of block size and ensure that only complete blocks are buffered
			count = avl % data->base.info.fmt.det.aud.channel_count;
			if (!count)
				count = avl;
			else
				count = avl - count;
		}
		else
		{
			count = avl;
		}
	}
	if (count)
	{
		pjmedia_circ_buf_write(data->record_data.buffer, buf, (unsigned int)count);
		data->record_data.samples_recorded += count;
		avl -= count;
	}
	data->record_data.samples_seen += size;

	do {
		if (data->record_data.max_samples && (data->record_data.samples_recorded >= data->record_data.max_samples))
		{
			data->record_data.status = PJSUA_REC_STOPPING;
			data->record_data.er = PJSUA_REC_ER_MAX_SAMPLES;
			break;
		}
		if (data->record_data.max_duration && (data->record_data.samples_seen >= data->record_data.max_duration))
		{
			data->record_data.status = PJSUA_REC_STOPPING;
			data->record_data.er = PJSUA_REC_ER_MAX_DURATION;
			break;
		}
		if (data->record_data.max_silence && ((silence_samples + size) >= data->record_data.max_silence))
		{
			data->record_data.status = PJSUA_REC_STOPPING;
			data->record_data.er = PJSUA_REC_ER_MAX_SILENCE;
			break;
		}
	} while (0);

	// Notify the application to collect the buffered data when the available space
	// falls below the data threshold or if we've encountered a termination
	// condition
	if (!data->record_data.signaled &&
		((avl < data->record_data.threshold) || (data->record_data.status == PJSUA_REC_STOPPING)))
	{
		data->record_data.signaled = PJ_TRUE;
		pj_event_set(data->record_data.event);
	}

	if (data->record_data.status == PJSUA_REC_STOPPING)
	{
		if (data->record_data.rec_output)
			pjmedia_conf_configure_port(pjsua_var.mconf, data->slot, PJMEDIA_PORT_NO_CHANGE, PJMEDIA_PORT_ENABLE);
		else
			pjmedia_conf_configure_port(pjsua_var.mconf, data->slot, PJMEDIA_PORT_ENABLE, PJMEDIA_PORT_NO_CHANGE);
	}
}

static pj_status_t mport_put_frame(pjmedia_port *this_port, pjmedia_frame *frame)
{
	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVALIDOP);

	pjsua_mport_data *data = (pjsua_mport_data*) this_port;
	if ((data->record_data.status == PJSUA_REC_RUNNING) &&
		!data->record_data.rec_output)
	{
		pj_enter_critical_section();
		mport_rec_frame(data, frame);
		pj_leave_critical_section();
	}

	if ((frame->type == PJMEDIA_FRAME_TYPE_AUDIO) &&
		(frame->size > 0U) &&
		(data->listener_cnt > 0U))
	{
		const pj_size_t samples = frame->size >> 1;
		pj_assert(samples == pjsua_var.mconf_cfg.samples_per_frame);
		pj_enter_critical_section();
		pj_assert(data->listeners != NULL);
		for (register pj_uint32_t i = 0; i < data->listener_cnt; ++i)
		{
			const pjsua_mport_id id = data->listeners[i];
			pjsua_mport_data *listener;
			pj_int32_t *mix_buf;
			pj_int16_t *buf;
			pj_size_t j;
			pj_assert((id >= 0) && (id < (pjsua_mport_id)pjsua_var.media_cfg.max_media_ports));
			listener = &pjsua_var.mport[id];
			mix_buf = listener->mix_buf;
			buf = (pj_int16_t *)frame->buf;
			if (listener->mix_cnt++ > 0)
			{
				for (j = 0; j < samples; ++j)
				{
					*mix_buf += *buf++;
					if (IS_OVERFLOW(*mix_buf))
					{
						int tmp_adj = (MAX_LEVEL << 7) / *mix_buf;
						if (tmp_adj < 0)
							tmp_adj = -tmp_adj;
						if (tmp_adj < listener->mix_adj)
							listener->mix_adj = tmp_adj;
					}
					mix_buf++;
				}
			}
			else
			{
				for (j = 0; j < samples; ++j)
				{
					*mix_buf++ = *buf++;
				}
			}
		}
		pj_leave_critical_section();
	}

	return PJ_SUCCESS;
}

static pj_status_t mport_get_frame(pjmedia_port *this_port, pjmedia_frame *frame)
{
	pjsua_mport_data *data;
	pj_size_t i, size, avl, count;

	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVALIDOP);

	data = (pjsua_mport_data*) this_port;

	size = pjsua_var.mconf_cfg.samples_per_frame;

	frame->timestamp.u64 = data->play_data.timestamp.u64;
	data->play_data.timestamp.u64 += size;

	pj_enter_critical_section();

	if (data->play_data.status <= PJSUA_REP_IDLE)
	{
		frame->size = 0;
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
	}
	else if (data->play_data.status == PJSUA_REP_CONFERENCING)
	{
		if (data->mix_cnt)
		{
			pj_int16_t *buf = (pj_int16_t *)frame->buf;
			SIMPLE_AGC(data->last_mix_adj, data->mix_adj);
			data->last_mix_adj = data->mix_adj;
			pj_assert(data->mix_buf != NULL);
			if (data->mix_adj != NORMAL_LEVEL)
			{
				for (i = 0; i < size; ++i)
				{
					pj_int32_t s = data->mix_buf[i];
					s = (s * data->mix_adj) >> 7;
					if (s > MAX_LEVEL) s = MAX_LEVEL;
					else if (s < MIN_LEVEL) s = MIN_LEVEL;
					*buf++ = (pj_int16_t)s;
				}
				data->mix_adj = NORMAL_LEVEL;
			}
			else
			{
				for (i = 0; i < size; ++i)
				{
					*buf++ = (pj_int16_t)data->mix_buf[i];
				}
			}
			data->mix_cnt = 0U;
			frame->size = size << 1;
			frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
		}
		else
		{
			frame->size = 0;
			frame->type = PJMEDIA_FRAME_TYPE_NONE;
		}
	}
	else
	{
		pj_assert(data->play_data.buffer != NULL);
		avl = data->play_data.buffer->len;
		count = size;
		if (count > avl)
		{
			if (!data->play_data.underrun && (avl || data->play_data.samples_played) && (data->play_data.status == PJSUA_REP_RUNNING))
			{
				PJ_LOG(3, (THIS_FILE, "Replay buffer underrun for %s: avl=%lu, size=%lu", this_port->info.name.ptr, avl, size));
				data->play_data.underrun = PJ_TRUE;
			}
			count = avl;
		}
		if (count)
		{
			pjmedia_circ_buf_read(data->play_data.buffer, (pj_int16_t*)frame->buf, (unsigned int)count);
			avl -= count;
			data->play_data.samples_played += count;
		}

		// Pad with zeroes if necessary
		if (count < size)
			pj_bzero((pj_int16_t*)(frame->buf) + count, (size - count) << 1);

		// Notify the application when the remaining data falls below the
		// threshold or if the replay has completed
		if (!data->play_data.signaled &&
			(((data->play_data.status == PJSUA_REP_STOPPING) && !avl) ||
			((data->play_data.status == PJSUA_REP_RUNNING) && (avl < data->play_data.threshold))))
		{
			data->play_data.signaled = PJ_TRUE;
			pj_event_set(data->play_data.event);
		}

		frame->size = size << 1;
		frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	}

	if ((data->record_data.status == PJSUA_REC_RUNNING) &&
		data->record_data.rec_output)
		mport_rec_frame(data, frame);

	pj_leave_critical_section();

	return PJ_SUCCESS;
}


static pj_status_t mport_on_destroy(pjmedia_port *this_port)
{
	pjsua_mport_data *data;

	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVALIDOP);

	data = (pjsua_mport_data*) this_port;

	pj_enter_critical_section();

	if (data->record_data.buffer)
		pjmedia_circ_buf_reset(data->record_data.buffer);
	if (data->record_data.status == PJSUA_REC_RUNNING)
		data->record_data.status = PJSUA_REC_STOPPING;
	if ((data->record_data.status != PJSUA_REC_IDLE) && !data->record_data.signaled && data->record_data.event)
	{
		data->record_data.signaled = PJ_TRUE;
		pj_event_set(data->record_data.event);
	}

	if (data->play_data.buffer)
		pjmedia_circ_buf_reset(data->play_data.buffer);
	if (data->play_data.status == PJSUA_REP_RUNNING)
		data->play_data.status = PJSUA_REP_STOPPING;
	if ((data->play_data.status == PJSUA_REP_STOPPING) && !data->play_data.signaled && data->play_data.event)
	{
		data->play_data.signaled = PJ_TRUE;
		pj_event_set(data->play_data.event);
	}

	pj_leave_critical_section();

	if (data->play_data.status == PJSUA_REP_CONFERENCING)
		pjsua_mport_conf_stop((pjsua_mport_id)(data - pjsua_var.mport));

	return PJ_SUCCESS;
}

static pj_status_t init_mport(pjsua_mport_id id)
{
	pj_status_t status;
	char buf[PJ_MAX_OBJ_NAME];
	pj_str_t name;
	register pjsua_mport_data *p = &pjsua_var.mport[id];
	//pj_bzero(p, sizeof(*p));
	p->slot = PJSUA_INVALID_ID;
	pj_ansi_snprintf(buf, sizeof(buf), "mp%d", id);
	buf[sizeof(buf)-1] = '\0';
	pj_strdup2_with_null(app_config.pool, &name, buf);
	status = pjmedia_port_info_init(&p->base.info, &name, SIGNATURE, pjsua_var.media_cfg.clock_rate,
		pjsua_var.mconf_cfg.channel_count, pjsua_var.mconf_cfg.bits_per_sample, pjsua_var.mconf_cfg.samples_per_frame);
	if (status == PJ_SUCCESS)
	{
		p->base.port_data.ldata = id;
		p->base.put_frame = &mport_put_frame;
		p->base.get_frame = &mport_get_frame;
		p->base.on_destroy = &mport_on_destroy;
	}
	return status;
}

static void deinit_mport(pjsua_mport_id id)
{
	pjsua_mport_free(id);
	register pjsua_mport_data *p = &pjsua_var.mport[id];
	pjmedia_port_destroy(&p->base);
	p->base.info.signature = 0;
}

// TODO: call this when destroying library
static void destroy_mport()
{
	if (pjsua_var.mport != NULL)
	{
		for (unsigned i = 0U; i < pjsua_var.media_cfg.max_media_ports; ++i)
			deinit_mport(i);
		pjsua_var.mport = NULL;
	}
	pjsua_var.mport_cnt = 0U;
}

//#################

PJ_DEF(pj_status_t) pjsua_mport_alloc(pjmedia_dir dir,
	pj_bool_t enable_vad, pj_size_t record_buffer_size,
	pj_size_t record_data_threshold, pj_size_t play_buffer_size,
	pj_size_t play_data_threshold, pjsua_mport_id *p_id)
{
	char name[PJ_MAX_OBJ_NAME];

	if ((dir != PJMEDIA_DIR_CAPTURE) && (dir != PJMEDIA_DIR_PLAYBACK) &&
		(dir != PJMEDIA_DIR_CAPTURE_PLAYBACK))
		return PJ_EINVAL;

	PJSUA_LOCK();

	if (pjsua_var.mport_cnt >= pjsua_var.media_cfg.max_media_ports)
	{
		PJSUA_UNLOCK();
		return PJ_ETOOMANY;
	}

	unsigned int id;
	for (id = 0; id<pjsua_var.media_cfg.max_media_ports; ++id)
	{
		register unsigned int tmp = id + pjsua_var.mport_id + 1;
		if (tmp >= pjsua_var.media_cfg.max_media_ports)
			tmp -= pjsua_var.media_cfg.max_media_ports;
		if (pjsua_var.mport[tmp].slot == PJSUA_INVALID_ID)
		{
			pjsua_var.mport_id = id = tmp;
			break;
		}
	}
	if (id == pjsua_var.media_cfg.max_media_ports)
	{
		/* This is unexpected */
		pj_assert(0);
		PJSUA_UNLOCK();
		return PJ_EBUG;
	}

	pj_log_push_indent();

	pj_status_t status = PJ_SUCCESS;
	register pjsua_mport_data *const p = &pjsua_var.mport[id];
	p->pool = pjsua_pool_create(p->base.info.name.ptr, 1000, 1000);
	if (p->pool == NULL)
	{
		status = PJ_ENOMEM;
		goto on_error;
	}

	name[sizeof(name) - 1] = '\0';

	p->listener_cnt = p->participant_cnt = 0U;
	p->listeners = p->participants = NULL;
	p->mix_buf = NULL;
	p->last_mix_adj = p->mix_adj = NORMAL_LEVEL;

	p->record_data.vad = NULL;
	p->record_data.buffer_size = 0U;
	p->record_data.buffer = NULL;
	p->record_data.event = NULL;
	p->record_data.signaled = PJ_FALSE;
	p->record_data.status = PJSUA_REC_IDLE;
	if (dir & PJMEDIA_DIR_CAPTURE)
	{
		if (enable_vad)
		{
			status = pjmedia_silence_det_create(p->pool, pjsua_var.media_cfg.clock_rate, pjsua_var.mconf_cfg.samples_per_frame, &p->record_data.vad);
			if (status != PJ_SUCCESS)
				goto on_error;
			status = pjmedia_silence_det_set_name(p->record_data.vad, p->base.info.name.ptr);
		}
		if (!record_buffer_size)
		{
			record_buffer_size = pjsua_var.media_cfg.mport_record_buffer_size;
			if (!record_buffer_size)
				record_buffer_size = 1250;
		}
		if (record_buffer_size < 40)
			record_buffer_size = 40;
		else if (record_buffer_size > 5000)
			record_buffer_size = 5000;
		record_buffer_size = ((record_buffer_size + (pjsua_var.media_cfg.audio_frame_ptime - 1)) / pjsua_var.media_cfg.audio_frame_ptime);
		p->record_data.buffer_size = record_buffer_size * pjsua_var.mconf_cfg.samples_per_frame;
		pj_ansi_snprintf(name, sizeof(name)-1, "mp_rec%d", id);
		status = pj_event_create(p->pool, name, PJ_TRUE, PJ_FALSE, &p->record_data.event);
		if (status != PJ_SUCCESS)
			goto on_error;
		if (!record_data_threshold)
			record_data_threshold = 250;
		record_data_threshold = ((record_data_threshold + (pjsua_var.media_cfg.audio_frame_ptime - 1)) / pjsua_var.media_cfg.audio_frame_ptime);
		if (record_data_threshold > record_buffer_size)
			record_data_threshold = record_buffer_size;
		p->record_data.threshold = record_data_threshold * pjsua_var.mconf_cfg.samples_per_frame;
		pj_ansi_snprintf(name, sizeof(name)-1, "mp_rgn%d", id);
		status = pj_event_create(p->pool, name, PJ_TRUE, PJ_FALSE, &p->recogntion_data.event);
		if (status != PJ_SUCCESS)
			goto on_error;
	}

	p->play_data.buffer_size = 0U;
	p->play_data.timestamp.u64 = 0;
	p->play_data.buffer = NULL;
	p->play_data.event = NULL;
	p->play_data.signaled = PJ_FALSE;
	p->play_data.status = PJSUA_REP_IDLE;
	if (dir & PJMEDIA_DIR_PLAYBACK)
	{
		if (!play_buffer_size)
		{
			play_buffer_size = pjsua_var.media_cfg.mport_replay_buffer_size;
			if (!play_buffer_size)
				play_buffer_size = 1250;
		}
		if (play_buffer_size < 40)
			play_buffer_size = 40;
		else if (play_buffer_size > 5000)
			play_buffer_size = 5000;
		play_buffer_size = ((play_buffer_size + (pjsua_var.media_cfg.audio_frame_ptime - 1)) / pjsua_var.media_cfg.audio_frame_ptime);
		p->play_data.buffer_size = play_buffer_size * pjsua_var.mconf_cfg.samples_per_frame;
		pj_ansi_snprintf(name, sizeof(name)-1, "mp_pla%d", id);
		status = pj_event_create(p->pool, name, PJ_TRUE, PJ_FALSE, &p->play_data.event);
		if (status != PJ_SUCCESS)
			goto on_error;
		if (!play_data_threshold)
			play_data_threshold = 250;
		play_data_threshold = ((play_data_threshold + (pjsua_var.media_cfg.audio_frame_ptime - 1)) / pjsua_var.media_cfg.audio_frame_ptime);
		if (play_data_threshold > play_buffer_size)
			play_data_threshold = play_buffer_size;
		p->play_data.threshold = play_data_threshold * pjsua_var.mconf_cfg.samples_per_frame;
	}

	status = pjsua_conf_add_port(p->pool, &p->base, &p->slot);
	if (status != PJ_SUCCESS)
	{
		pjsua_perror(THIS_FILE, "Unable to add media port to conference bridge", status);
		goto on_error;
	}

	p->base.info.dir = dir;

	++pjsua_var.mport_cnt;

	PJSUA_UNLOCK();

	PJ_LOG(4,(THIS_FILE, "Media port %d allocated: slot=%d", id, p->slot));

	if (p_id)
		*p_id = id;

	pj_log_pop_indent();
	return PJ_SUCCESS;

on_error:
	p->listeners = p->participants = NULL;
	p->mix_buf = NULL;
	if (p->recogntion_data.event != NULL)
	{
		pj_event_destroy(p->recogntion_data.event);
		p->recogntion_data.event = NULL;
	}
	if (p->record_data.event != NULL)
	{
		pj_event_destroy(p->record_data.event);
		p->record_data.event = NULL;
	}
	p->record_data.vad = NULL;
	p->record_data.buffer = NULL;
	if (p->play_data.event != NULL)
	{
		pj_event_destroy(p->play_data.event);
		p->play_data.event = NULL;
	}
	p->play_data.buffer = NULL;
	if (p->pool != NULL)
	{
		pj_pool_release(p->pool);
		p->pool = NULL;
	}
	p->base.info.dir = PJMEDIA_DIR_NONE;
	p->slot = PJSUA_INVALID_ID;
	PJSUA_UNLOCK();
	pj_log_pop_indent();
	return status;
}

PJ_DEF(pj_status_t) pjsua_mport_free(pjsua_mport_id id)
{
	if ((id < 0) || ((unsigned int)id >= pjsua_var.media_cfg.max_media_ports))
	{
		pj_assert(0);
		return PJ_EINVAL;
	}

	PJSUA_LOCK();

	pjsua_mport_data *p = &pjsua_var.mport[id];

	if (!p->pool)
	{
		PJSUA_UNLOCK();
		return PJ_SUCCESS;
	}

	pjsua_mport_conf_stop(id);
	while (p->listener_cnt > 0)
		pjsua_mport_conf_remove(p->listeners[p->listener_cnt - 1], id);

	pj_enter_critical_section();

	p->listeners = p->participants = NULL;
	p->mix_buf = NULL;

	if (p->recogntion_data.event_cnt > 0U)
	{
		if (p->recogntion_data.signaled)
		{
			p->recogntion_data.signaled = PJ_FALSE;
			pj_event_reset(p->recogntion_data.event);
		}
		p->recogntion_data.event_cnt = 0U;
	}
	if (p->record_data.buffer)
	{
		pjmedia_circ_buf_reset(p->record_data.buffer);
		p->record_data.buffer = NULL;
	}
	if (p->record_data.status != PJSUA_REC_IDLE)
	{
		if ((p->record_data.event != NULL) &&
			!p->record_data.signaled)
		{
			p->record_data.signaled = PJ_TRUE;
			pj_event_set(p->record_data.event);
		}
		p->record_data.status = PJSUA_REC_IDLE;
	}
	p->record_data.vad = NULL;

	if (p->play_data.buffer)
	{
		pjmedia_circ_buf_reset(p->play_data.buffer);
		p->play_data.buffer = NULL;
	}
	if (p->play_data.status != PJSUA_REP_IDLE)
	{
		if ((p->play_data.status != PJSUA_REP_CONFERENCING) &&
			(p->play_data.event != NULL) &&
			!p->play_data.signaled)
		{
			p->play_data.signaled = PJ_TRUE;
			pj_event_set(p->play_data.event);
		}
		p->play_data.status = PJSUA_REP_IDLE;
	}

	pj_leave_critical_section();

	if (p->slot != PJSUA_INVALID_ID)
	{
		pjsua_conf_remove_port(p->slot);
		p->slot = PJSUA_INVALID_ID;
	}

	if (p->recogntion_data.event != NULL)
	{
		pj_event_destroy(p->recogntion_data.event);
		p->recogntion_data.event = NULL;
		p->recogntion_data.signaled = PJ_FALSE;
	}

	if (p->record_data.event != NULL)
	{
		pj_event_destroy(p->record_data.event);
		p->record_data.event = NULL;
		p->record_data.signaled = PJ_FALSE;
	}

	if (p->play_data.event != NULL)
	{
		pj_event_destroy(p->play_data.event);
		p->play_data.event = NULL;
		p->play_data.signaled = PJ_FALSE;
	}

	if (p->pool != NULL)
	{
		pj_pool_release(p->pool);
		p->pool = NULL;
	}

	p->base.info.dir = PJMEDIA_DIR_NONE;

	pj_assert(pjsua_var.mport_cnt > 0);
	--pjsua_var.mport_cnt;

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Media port %d freed", id));

	return PJ_SUCCESS;
}

PJ_DEF(pjsua_conf_port_id) pjsua_mport_get_conf_port(pjsua_mport_id id)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,PJSUA_INVALID_ID);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJSUA_INVALID_ID);

	return pjsua_var.mport[id].slot;
}

PJ_DEF(pj_status_t) pjsua_mport_get_port( pjsua_mport_id id, pjmedia_port **p_port)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(p_port != NULL, PJ_EINVAL);

	*p_port = &pjsua_var.mport[id].base;

	return PJ_SUCCESS;
}

PJ_DEF(pjmedia_dir) pjsua_mport_get_dir(pjsua_mport_id id)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJMEDIA_DIR_NONE);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJMEDIA_DIR_NONE);

	return pjsua_var.mport[id].base.info.dir;
}

PJ_DEF(pj_event_t *) pjsua_mport_get_play_event(pjsua_mport_id id)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,NULL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, NULL);

	return pjsua_var.mport[id].play_data.event;
}

PJ_DEF(pj_event_t *) pjsua_mport_get_record_event(pjsua_mport_id id)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,NULL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, NULL);

	return pjsua_var.mport[id].record_data.event;
}

PJ_DEF(pj_event_t *) pjsua_mport_get_recognition_event(pjsua_mport_id id)
{
	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,NULL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, NULL);

	return pjsua_var.mport[id].recogntion_data.event;
}

static void add_replay_data(pjmedia_format_id fmt_id,
	pjmedia_circ_buf *buffer,
	const void *data,
	pj_size_t size,
	pj_size_t *count)
{
	pj_int16_t *reg1, *reg2;
	unsigned reg1cnt, reg2cnt;
	pj_size_t avl = buffer->capacity - buffer->len;
	if (avl)
	{
		if (fmt_id == PJMEDIA_FORMAT_PCM)
		{
			size = size >> 1;
			if (size > avl)
				size = avl;
			pjmedia_circ_buf_write(buffer, (pj_int16_t*)data, (unsigned int)size);
			size = size << 1;
		}
		else
		{
			if (size > avl)
				size = avl;
			pjmedia_circ_buf_get_write_regions(buffer, &reg1, &reg1cnt, &reg2, &reg2cnt);
			if (reg1cnt >= size)
			{
				switch (fmt_id)
				{
				case PJMEDIA_FORMAT_PCMA:
					pjmedia_alaw_decode(reg1, data, size);
					break;
				case PJMEDIA_FORMAT_PCMU:
					pjmedia_ulaw_decode(reg1, data, size);
					break;
				default:
					pj_assert(0);
					break;
				}
			}
			else
			{
				switch (fmt_id)
				{
				case PJMEDIA_FORMAT_PCMA:
					pjmedia_alaw_decode(reg1, data, reg1cnt);
					pjmedia_alaw_decode(reg2, (const pj_uint8_t*)data + reg1cnt, size - reg1cnt);
					break;
				case PJMEDIA_FORMAT_PCMU:
					pjmedia_ulaw_decode(reg1, data, reg1cnt);
					pjmedia_ulaw_decode(reg2, (const pj_uint8_t*)data + reg1cnt, size - reg1cnt);
					break;
				default:
					pj_assert(0);
					break;
				}
			}
			pjmedia_circ_buf_adv_write_ptr(buffer, (unsigned int)size);
		}
		*count = size;
	}
}

PJ_DEF(pj_status_t) pjsua_mport_play_start(
	pjsua_mport_id id,
	const pjmedia_format *fmt,
	const void *data,
	pj_size_t size,
	pj_size_t *count)
{
	pjsua_mport_replay_data *p;

	PJ_ASSERT_RETURN(count != NULL, PJ_EINVAL);

	*count = 0;

	PJ_ASSERT_RETURN(id>=0&&(unsigned int)id<pjsua_var.media_cfg.max_media_ports,PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(fmt != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	p = &pjsua_var.mport[id].play_data;
	if (!(pjsua_var.mport[id].base.info.dir & PJMEDIA_DIR_PLAYBACK))
	{
		PJSUA_UNLOCK();
		return PJ_EINVALIDOP;
	}
	if (p->status != PJSUA_REP_IDLE)
	{
		PJSUA_UNLOCK();
		return PJ_EBUSY;
	}
	if (((fmt->id != PJMEDIA_FORMAT_PCM) && (fmt->id != PJMEDIA_FORMAT_PCMA) && (fmt->id != PJMEDIA_FORMAT_PCMU)) ||
		(fmt->det.aud.clock_rate != pjsua_var.media_cfg.clock_rate) ||
		(fmt->det.aud.channel_count != pjsua_var.media_cfg.channel_count))
	{
		PJSUA_UNLOCK();
		return PJ_ENOTSUP;
	}
	if (!p->buffer)
	{
		pj_status_t status = pjmedia_circ_buf_create(pjsua_var.mport[id].pool, (unsigned int)p->buffer_size, &p->buffer);
		if (status != PJ_SUCCESS)
		{
			PJSUA_UNLOCK();
			return status;
		}
	}

	pjmedia_circ_buf_reset(p->buffer);
	if ((data != NULL) && size)
		add_replay_data(fmt->id, p->buffer, data, size, count);
	p->samples_played = 0;
	p->underrun = PJ_FALSE;
	if (p->buffer->len < p->threshold)
	{
		pj_event_set(p->event);
		p->signaled = PJ_TRUE;
	}
	else
	{
		pj_event_reset(p->event);
		p->signaled = PJ_FALSE;
	}
	p->fmt_id = fmt->id;
	p->status = PJSUA_REP_RUNNING;

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Started replay on media port %d", id));

	pjmedia_conf_configure_port(pjsua_var.mconf, pjsua_var.mport[id].slot, PJMEDIA_PORT_NO_CHANGE, PJMEDIA_PORT_ENABLE_ALWAYS);

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_play_status(pjsua_mport_id id,
	pjs_mport_play_info *info)
{
	pjsua_mport_replay_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(info != NULL, PJ_EINVAL);

	pj_bzero(info, sizeof(*info));

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].play_data;

	if ((p->status != PJSUA_REP_RUNNING) && (p->status != PJSUA_REP_STOPPING))
	{
		pj_leave_critical_section();
		return PJ_ENOTFOUND;
	}

	info->samples_played = p->samples_played;
	info->underrun = p->underrun;
	if (info->underrun)
		p->underrun = PJ_FALSE;
	if (p->status == PJSUA_REP_STOPPING)
	{
		if (!p->buffer->len)
		{
			if (p->signaled)
			{
				pj_event_reset(p->event);
				p->signaled = PJ_FALSE;
			}
			info->completed = PJ_TRUE;
			p->status = PJSUA_REP_IDLE;

			PJ_LOG(4, (THIS_FILE, "Replay completed on media port %d - %llu samples were played",
				id, p->samples_played));
		}
	}
	else
	{
		info->free_buffer_size = p->buffer->capacity - p->buffer->len;
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_play_put_data(pjsua_mport_id id,
	const void *data,
	pj_size_t size,
	pj_size_t *count)
{
	pjsua_mport_replay_data *p;

	PJ_ASSERT_RETURN(count != NULL, PJ_EINVAL);

	*count = 0;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].play_data;

	if (p->status != PJSUA_REP_RUNNING)
	{
		pj_status_t status = (p->status == PJSUA_REP_STOPPING) ? PJ_EEOF : PJ_ENOTFOUND;
		pj_leave_critical_section();
		return status;
	}

	if ((data != NULL) && size)
	{
		add_replay_data(p->fmt_id, p->buffer, data, size, count);
		if (p->buffer->len >= p->threshold)
		{
			if (p->signaled)
			{
				pj_event_reset(p->event);
				p->signaled = PJ_FALSE;
			}
		}
		else
		{
			if (!p->signaled)
			{
				pj_event_set(p->event);
				p->signaled = PJ_TRUE;
			}
		}
	}
	else
	{
		if (p->buffer->len)
		{
			if (p->signaled)
			{
				pj_event_reset(p->event);
				p->signaled = PJ_FALSE;
			}
		}
		else
		{
			if (!p->signaled)
			{
				pj_event_set(p->event);
				p->signaled = PJ_TRUE;
			}
		}
		p->status = PJSUA_REP_STOPPING;
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_play_stop(pjsua_mport_id id, pj_bool_t discard)
{
	pjsua_mport_replay_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].play_data;

	if ((p->status != PJSUA_REP_RUNNING) && (p->status != PJSUA_REP_STOPPING))
	{
		pj_leave_critical_section();
		return PJ_ENOTFOUND;
	}

	if (discard)
		p->buffer->len = 0;
	if (p->buffer->len)
	{
		if (p->signaled)
		{
			pj_event_reset(p->event);
			p->signaled = PJ_FALSE;
		}
	}
	else
	{
		if (!p->signaled)
		{
			pj_event_set(p->event);
			p->signaled = PJ_TRUE;
		}
	}
	if (p->status == PJSUA_REP_RUNNING)
		p->status = PJSUA_REP_STOPPING;

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_record_start(pjsua_mport_id id,
	const pjmedia_format *fmt,
	pj_bool_t rec_output,
	pj_size_t max_duration,
	pj_size_t max_samples,
	pj_size_t max_silence,
	pj_size_t eliminate_silence)
{
	pjsua_mport_record_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(fmt != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	p = &pjsua_var.mport[id].record_data;
	if (!(pjsua_var.mport[id].base.info.dir & PJMEDIA_DIR_CAPTURE))
	{
		PJSUA_UNLOCK();
		return PJ_EINVALIDOP;
	}
	if (rec_output && !(pjsua_var.mport[id].base.info.dir & PJMEDIA_DIR_PLAYBACK))
	{
		PJSUA_UNLOCK();
		return PJ_EINVALIDOP;
	}
	if (p->status != PJSUA_REC_IDLE)
	{
		PJSUA_UNLOCK();
		return PJ_EBUSY;
	}
	if (((fmt->id != PJMEDIA_FORMAT_PCM) && (fmt->id != PJMEDIA_FORMAT_PCMA) && (fmt->id != PJMEDIA_FORMAT_PCMU)) ||
		(fmt->det.aud.clock_rate != pjsua_var.media_cfg.clock_rate) ||
		(fmt->det.aud.channel_count != pjsua_var.media_cfg.channel_count))
	{
		PJSUA_UNLOCK();
		return PJ_ENOTSUP;
	}
	if (!p->buffer)
	{
		pj_status_t status = pjmedia_circ_buf_create(pjsua_var.mport[id].pool, (unsigned int)p->buffer_size, &p->buffer);
		if (status != PJ_SUCCESS)
		{
			PJSUA_UNLOCK();
			return status;
		}
	}

	// Convert all the time based arguments to the corresponding sample counts
	// and ensure that they are truncated to complete blocks if necessary
	if (max_duration)
	{
		max_duration = max_duration * fmt->det.aud.clock_rate / 1000;
		max_duration -= (max_duration % fmt->det.aud.channel_count);
	}
	if (max_silence)
	{
		max_silence = max_silence * fmt->det.aud.clock_rate / 1000;
		max_silence -= (max_silence % fmt->det.aud.channel_count);
	}
	if (eliminate_silence)
	{
		eliminate_silence = eliminate_silence * fmt->det.aud.clock_rate / 1000;
		eliminate_silence -= (eliminate_silence % fmt->det.aud.channel_count);
	}

	if (p->signaled)
	{
		pj_event_reset(p->event);
		p->signaled = PJ_FALSE;
	}
	pjmedia_circ_buf_reset(p->buffer);
	p->samples_seen = 0;
	p->samples_recorded = 0;
	p->vad_timestamp.u64 = 0;
	p->is_silence = PJ_FALSE;
	p->overrun = PJ_FALSE;
	p->er = PJSUA_REC_ER_NONE;
	p->fmt_id = fmt->id;
	p->rec_output = rec_output;
	p->max_samples = max_samples;
	p->max_duration = max_duration;
	p->max_silence = max_silence;
	p->eliminate_silence = eliminate_silence;
	p->status = PJSUA_REC_RUNNING;

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Started recording on media port %d; event=%p", id, p->event));

	if (rec_output)
		pjmedia_conf_configure_port(pjsua_var.mconf, pjsua_var.mport[id].slot, PJMEDIA_PORT_NO_CHANGE, PJMEDIA_PORT_ENABLE_ALWAYS);
	else
		pjmedia_conf_configure_port(pjsua_var.mconf, pjsua_var.mport[id].slot, PJMEDIA_PORT_ENABLE_ALWAYS, PJMEDIA_PORT_NO_CHANGE);

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_record_status(pjsua_mport_id id,
	pjs_mport_record_info *info)
{
	pjsua_mport_record_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(info != NULL, PJ_EINVAL);

	pj_bzero(info, sizeof(*info));

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].record_data;

	if ((p->status != PJSUA_REP_RUNNING) && (p->status != PJSUA_REP_STOPPING))
	{
		pj_leave_critical_section();
		return PJ_ENOTFOUND;
	}

	info->samples_recorded = p->samples_recorded;
	info->overrun = p->overrun;
	if (info->overrun)
		p->overrun = PJ_FALSE;
	info->samples_available = p->buffer->len;
	info->end_reason = p->er;
	if ((p->status == PJSUA_REC_STOPPING) && !info->samples_available)
	{
		if (p->signaled)
		{
			pj_event_reset(p->event);
			p->signaled = PJ_FALSE;
		}
		info->completed = PJ_TRUE;
		p->status = PJSUA_REC_IDLE;

		PJ_LOG(4, (THIS_FILE, "Recording completed on media port %d - %llu samples were recorded",
			id, p->samples_recorded));
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

static void get_record_data(pjmedia_format_id fmt_id,
	pjmedia_circ_buf *buffer,
	void *data,
	pj_size_t size,	/* Size of the data buffer in bytes - not samples! */
	pj_size_t *count)
{
	pj_int16_t *reg1, *reg2;
	unsigned reg1cnt, reg2cnt;
	if (buffer->len)
	{
		if (fmt_id == PJMEDIA_FORMAT_PCM)
		{
			if (size == 1U)
			{
				pj_int16_t buf;
				pjmedia_circ_buf_read(buffer, &buf, 1U);
				memcpy(data, &buf, 1U);
			}
			else
			{
				unsigned int samples = (unsigned int)(size >> 1);
				if (samples > buffer->len)
				{
					samples = buffer->len;
					size = samples << 1;
				}
				pjmedia_circ_buf_read(buffer, (pj_int16_t*)data, samples);
			}
		}
		else
		{
			if (size > buffer->len)
				size = buffer->len;
			pjmedia_circ_buf_get_read_regions(buffer, &reg1, &reg1cnt, &reg2, &reg2cnt);
			if (reg1cnt >= size)
			{
				switch (fmt_id)
				{
				case PJMEDIA_FORMAT_PCMA:
					pjmedia_alaw_encode((pj_uint8_t*)data, reg1, size);
					break;
				case PJMEDIA_FORMAT_PCMU:
					pjmedia_ulaw_encode((pj_uint8_t*)data, reg1, size);
					break;
				default:
					pj_assert(0);
					break;
				}
			}
			else
			{
				switch (fmt_id)
				{
				case PJMEDIA_FORMAT_PCMA:
					pjmedia_alaw_encode((pj_uint8_t*)data, reg1, reg1cnt);
					pjmedia_alaw_encode((pj_uint8_t*)data + reg1cnt, reg2, size - reg1cnt);
					break;
				case PJMEDIA_FORMAT_PCMU:
					pjmedia_ulaw_encode((pj_uint8_t*)data, reg1, reg1cnt);
					pjmedia_ulaw_encode((pj_uint8_t*)data + reg1cnt, reg2, size - reg1cnt);
					break;
				default:
					pj_assert(0);
					break;
				}
			}
			pjmedia_circ_buf_adv_read_ptr(buffer, (unsigned int)size);
		}
		*count = size;
	}
}

PJ_DEF(pj_status_t) pjsua_mport_record_get_data(pjsua_mport_id id,
	void *buffer,
	pj_size_t size,
	pj_size_t *count)
{
	pjsua_mport_record_data *p;

	PJ_ASSERT_RETURN(count != NULL, PJ_EINVAL);

	*count = 0;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(buffer && size, PJ_EINVAL);

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].record_data;

	if ((p->status != PJSUA_REC_RUNNING) && (p->status != PJSUA_REC_STOPPING))
	{
		pj_leave_critical_section();
		return PJ_ENOTFOUND;
	}

	get_record_data(p->fmt_id, p->buffer, buffer, size, count);
	if ((p->status == PJSUA_REC_RUNNING) &&
		((p->buffer->capacity - p->buffer->len) >= p->threshold))
	{
		if (p->signaled)
		{
			pj_event_reset(p->event);
			p->signaled = PJ_FALSE;
		}
	}
	else
	{
		if (!p->signaled)
		{
			pj_event_set(p->event);
			p->signaled = PJ_TRUE;
		}
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_record_stop(pjsua_mport_id id, pj_bool_t discard)
{
	pjsua_mport_record_data *p;
	int rec_output = -1;
	pj_event_t* signaled_event = NULL;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	PJ_ASSERT_RETURN(pjsua_var.mport[id].pool != NULL, PJ_EINVAL);

	PJ_LOG(4, (THIS_FILE, "Stopping recording on media port %d%s...", id, discard ? " and discarding buffered data" : ""));

	pj_enter_critical_section();

	p = &pjsua_var.mport[id].record_data;

	if ((p->status != PJSUA_REC_RUNNING) && (p->status != PJSUA_REC_STOPPING))
	{
		pj_leave_critical_section();
		return PJ_ENOTFOUND;
	}

	if (discard)
		p->buffer->len = 0;
	if (p->status == PJSUA_REC_RUNNING)
	{
		p->er = PJSUA_REC_ER_STOP;
		p->status = PJSUA_REC_STOPPING;
		rec_output = p->rec_output;
	}
	if (!p->signaled)
	{
		signaled_event = p->event;
		pj_event_set(p->event);
		p->signaled = PJ_TRUE;
	}

	pj_leave_critical_section();

	if (rec_output >= 0)
	{
		if (rec_output)
			pjmedia_conf_configure_port(pjsua_var.mconf, pjsua_var.mport[id].slot, PJMEDIA_PORT_NO_CHANGE, PJMEDIA_PORT_ENABLE);
		else
			pjmedia_conf_configure_port(pjsua_var.mconf, pjsua_var.mport[id].slot, PJMEDIA_PORT_ENABLE, PJMEDIA_PORT_NO_CHANGE);
	}

	PJ_LOG(4, (THIS_FILE, "rec_output=%d, event=%p", rec_output, signaled_event));

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_conf_start(pjsua_mport_id id)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	if (!(p->base.info.dir & PJMEDIA_DIR_PLAYBACK))
	{
		PJSUA_UNLOCK();
		return PJ_EINVALIDOP;
	}
	if (p->play_data.status != PJSUA_REP_IDLE)
	{
		PJSUA_UNLOCK();
		return PJ_EBUSY;
	}
	if (!p->participants)
	{
		p->participants = (pjsua_mport_id*)pj_pool_zalloc(p->pool, sizeof(pjsua_mport_id) * PJSUA_MAX_CONF_PORTS);
		if (!p->participants)
		{
			PJSUA_UNLOCK();
			return PJ_ENOMEM;
		}
		for (register int i = 0; i < PJSUA_MAX_CONF_PORTS; ++i)
			p->participants[i] = PJSUA_INVALID_ID;
	}
	if (!p->mix_buf)
	{
		p->mix_buf = (pj_int32_t*)pj_pool_zalloc(p->pool, pjsua_var.mconf_cfg.samples_per_frame * sizeof(p->mix_buf[0]));
		if (!p->mix_buf)
		{
			PJSUA_UNLOCK();
			return PJ_ENOMEM;
		}
		p->last_mix_adj = NORMAL_LEVEL;
	}

	p->mix_cnt = 0U;
	p->mix_adj = NORMAL_LEVEL;
	p->play_data.status = PJSUA_REP_CONFERENCING;

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Started conference on media port %d", id));

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_conf_add(pjsua_mport_id id, pjsua_mport_id pid)
{
	register pjsua_mport_data *p, *pp;
	register int i;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(pid >= 0 && (unsigned int)pid<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	pp = &pjsua_var.mport[pid];
	PJ_ASSERT_RETURN(pp->pool != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	if (p->play_data.status != PJSUA_REP_CONFERENCING)
	{
		PJSUA_UNLOCK();
		return PJ_ENOTFOUND;
	}

	for (i = (int)p->participant_cnt - 1; i >= 0; --i)
	{
		if (p->participants[i] == pid)
		{
			PJSUA_UNLOCK();
			return PJ_SUCCESS;
		}
	}

	if (p->participant_cnt >= PJSUA_MAX_CONF_PORTS)
	{
			PJSUA_UNLOCK();
			return PJ_ETOOMANY;
	}

	if (!pp->listeners)
	{
		pp->listeners = (pjsua_mport_id*)pj_pool_zalloc(pp->pool, sizeof(pjsua_mport_id) * pjsua_var.media_cfg.max_media_ports);
		if (!pp->listeners)
		{
			PJSUA_UNLOCK();
			return PJ_ENOMEM;
		}
		for (i = 0; i < (int)pjsua_var.media_cfg.max_media_ports; ++i)
			pp->listeners[i] = PJSUA_INVALID_ID;
	}

	pj_enter_critical_section();

	p->participants[p->participant_cnt++] = pid;

	for (i = (int)pp->listener_cnt - 1; i >= 0; --i)
	{
		if (pp->listeners[i] == id)
			break;
	}
	if (i < 0)
	{
		pj_assert(pp->listener_cnt < pjsua_var.media_cfg.max_media_ports);
		pp->listeners[pp->listener_cnt++] = id;
	}

	pj_leave_critical_section();

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Added media port %d to conference on media port %d", pid, id));

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_conf_remove(pjsua_mport_id id, pjsua_mport_id pid)
{
	register pjsua_mport_data *p, *pp;
	register int i, j;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(pid >= 0 && (unsigned int)pid<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	pp = &pjsua_var.mport[pid];
	PJ_ASSERT_RETURN(pp->pool != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	if (p->play_data.status != PJSUA_REP_CONFERENCING)
	{
		PJSUA_UNLOCK();
		return PJ_ENOTFOUND;
	}

	pj_enter_critical_section();

	for (i = (int)(p->participant_cnt) - 1; i >= 0; --i)
	{
		if (p->participants[i] != pid)
			continue;
		for (j = (int)(pp->listener_cnt) - 1; j >= 0; --j)
		{
			if (pp->listeners[j] == id)
			{
				pp->listener_cnt--;
				if (j == (int)pp->listener_cnt)
				{
					pp->listeners[j] = PJSUA_INVALID_ID;
				}
				else
				{
					pp->listeners[j] = pp->listeners[pp->listener_cnt];
					pp->listeners[pp->listener_cnt] = PJSUA_INVALID_ID;
				}
				break;
			}
		}
		p->participant_cnt--;
		if (i == (int)p->participant_cnt)
		{
			p->participants[i] = PJSUA_INVALID_ID;
		}
		else
		{
			p->participants[i] = p->participants[p->participant_cnt];
			p->participants[p->participant_cnt] = PJSUA_INVALID_ID;
		}
		break;
	}

	pj_leave_critical_section();

	PJSUA_UNLOCK();

	if (i < 0)
		return PJ_ENOTFOUND;

	PJ_LOG(4, (THIS_FILE, "Removed media port %d from conference on media port %d", pid, id));

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_conf_stop(pjsua_mport_id id)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	PJSUA_LOCK();

	if (p->play_data.status != PJSUA_REP_CONFERENCING)
	{
		PJSUA_UNLOCK();
		return PJ_ENOTFOUND;
	}

	pj_enter_critical_section();

	for (register int i = (int)(p->participant_cnt) - 1; i >= 0; --i)
	{
		register pjsua_mport_data *pp = &pjsua_var.mport[p->participants[i]];
		for (register int j = (int)(pp->listener_cnt) - 1; j >= 0; --j)
		{
			if (pp->listeners[j] == id)
			{
				pp->listener_cnt--;
				if (j == (int)pp->listener_cnt)
				{
					pp->listeners[j] = PJSUA_INVALID_ID;
				}
				else
				{
					pp->listeners[j] = pp->listeners[pp->listener_cnt];
					pp->listeners[pp->listener_cnt] = PJSUA_INVALID_ID;
				}
				break;
			}
		}
		p->participants[i] = PJSUA_INVALID_ID;
		p->participant_cnt--;
	}

	p->play_data.status = PJSUA_REP_IDLE;

	pj_leave_critical_section();

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Stopped conference on media port %d", id));

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_listen_for(pjsua_mport_id id, pjs_listen_for_parms* params)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	if (params && (params->types == PJSUA_RCG_NONE))
		params = NULL;

	PJSUA_LOCK();

	if (!(p->base.info.dir & PJMEDIA_DIR_CAPTURE))
	{
		PJSUA_UNLOCK();
		return PJ_EINVALIDOP;
	}

	pj_enter_critical_section();

	if (params)
		p->recogntion_data.params = *params;
	else
		p->recogntion_data.params.types = PJSUA_RCG_NONE;

	pj_leave_critical_section();

	PJSUA_UNLOCK();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_get_recognised(pjsua_mport_id id, pjs_recognition_info* info)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(info != NULL, PJ_EINVAL);
	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	pj_enter_critical_section();

	if (p->recogntion_data.event_cnt > 0U)
	{
		*info = p->recogntion_data.events[0U];
		pj_array_erase(p->recogntion_data.events, sizeof(p->recogntion_data.events[0]), p->recogntion_data.event_cnt, 0U);
		if (!--p->recogntion_data.event_cnt)
		{
			if (p->recogntion_data.signaled)
			{
				pj_event_reset(p->recogntion_data.event);
				p->recogntion_data.signaled = PJ_FALSE;
			}
		}
	}
	else
	{
		info->type = PJSUA_RCG_NONE;
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsua_mport_discard_recognised(pjsua_mport_id id)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	pj_enter_critical_section();

	if (p->recogntion_data.event_cnt > 0U)
	{
		p->recogntion_data.event_cnt = 0U;
		if (p->recogntion_data.signaled)
		{
			pj_event_reset(p->recogntion_data.event);
			p->recogntion_data.signaled = PJ_FALSE;
		}
	}

	pj_leave_critical_section();

	return PJ_SUCCESS;
}

static pj_status_t pjsua_mport_add_recognition_event(pjsua_mport_id id, const pjs_recognition_info* ri)
{
	register pjsua_mport_data *p;

	PJ_ASSERT_RETURN(ri != NULL && ri->type != PJSUA_RCG_NONE, PJ_EINVAL);
	PJ_ASSERT_RETURN(id >= 0 && (unsigned int)id<pjsua_var.media_cfg.max_media_ports, PJ_EINVAL);
	p = &pjsua_var.mport[id];
	PJ_ASSERT_RETURN(p->pool != NULL, PJ_EINVAL);

	register pj_status_t pje = PJ_SUCCESS;

	pj_enter_critical_section();

	if (!(ri->type & p->recogntion_data.params.types))
	{
		pje = PJ_EIGNORED;
	}
	else
	{
		if ((ri->type & PJSUA_RCG_DTMF) &&
			((p->recogntion_data.params.types & PJSUA_RCG_DTMF) == PJSUA_RCG_DTMF))
			p->recogntion_data.params.types &=
				~(!(ri->type & PJSUA_RCG_DTMF_RFC2833) ? PJSUA_RCG_DTMF_RFC2833 : PJSUA_RCG_DTMF_TONE);
		if (p->recogntion_data.event_cnt >= PJ_ARRAY_SIZE(p->recogntion_data.events))
		{
			pje = PJ_ETOOMANY;
		}
		else
		{
			p->recogntion_data.events[p->recogntion_data.event_cnt++] = *ri;
			if (!p->recogntion_data.signaled)
			{
				pj_event_set(p->recogntion_data.event);
				p->recogntion_data.signaled = PJ_TRUE;
			}
		}
	}

	pj_leave_critical_section();

	return pje;
}

PJ_DEF(pj_status_t) pjsua_mport_add_rfc2833_dtmf_digit(pjsua_mport_id id, char digit, unsigned timestamp, unsigned duration)
{
	pjs_recognition_info ri;
	ri.type = PJSUA_RCG_DTMF_RFC2833;
	ri.timestamp = timestamp;
	ri.param0 = (unsigned)(pj_uint8_t)digit;
	ri.param1 = duration;
	return pjsua_mport_add_recognition_event(id, &ri);
}
