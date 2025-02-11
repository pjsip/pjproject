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
#include <pjmedia/stream.h>
#include <pjmedia/sdp_neg.h>
#include <pjmedia/stream_common.h>
#include <pj/ctype.h>
#include <pj/rand.h>

static const pj_str_t ID_RTPMAP = { "rtpmap", 6 };
static const pj_str_t ID_TELEPHONE_EVENT = { "telephone-event", 15 };

static void get_opus_channels_and_clock_rate(const pjmedia_codec_fmtp *enc_fmtp,
                                             const pjmedia_codec_fmtp *dec_fmtp,
                                             unsigned *channel_cnt,
                                             unsigned *clock_rate)
{
    unsigned i;
    unsigned enc_channel_cnt = 0, local_channel_cnt = 0;
    unsigned enc_clock_rate = 0, local_clock_rate = 0;

    for (i = 0; i < dec_fmtp->cnt; ++i) {
        if (!pj_stricmp2(&dec_fmtp->param[i].name, "sprop-maxcapturerate")) {
            local_clock_rate = (unsigned)pj_strtoul(&dec_fmtp->param[i].val);
        } else if (!pj_stricmp2(&dec_fmtp->param[i].name, "sprop-stereo")) {
            local_channel_cnt = (unsigned)pj_strtoul(&dec_fmtp->param[i].val);
            local_channel_cnt = (local_channel_cnt > 0) ? 2 : 1;
        }
    }
    if (!local_clock_rate) local_clock_rate = *clock_rate;
    if (!local_channel_cnt) local_channel_cnt = *channel_cnt;

    for (i = 0; i < enc_fmtp->cnt; ++i) {
        if (!pj_stricmp2(&enc_fmtp->param[i].name, "maxplaybackrate")) {
            enc_clock_rate = (unsigned)pj_strtoul(&enc_fmtp->param[i].val);
        } else if (!pj_stricmp2(&enc_fmtp->param[i].name, "stereo")) {
            enc_channel_cnt = (unsigned)pj_strtoul(&enc_fmtp->param[i].val);
            enc_channel_cnt = (enc_channel_cnt > 0) ? 2 : 1;
        }
    }
    /* The default is a standard mono session with 48000 Hz clock rate
     * (RFC 7587, section 7)
     */
    if (!enc_clock_rate) enc_clock_rate = 48000;
    if (!enc_channel_cnt) enc_channel_cnt = 1;

    *clock_rate = (enc_clock_rate < local_clock_rate) ? enc_clock_rate :
                  local_clock_rate;

    *channel_cnt = (enc_channel_cnt < local_channel_cnt) ? enc_channel_cnt :
                   local_channel_cnt;
}

/*
 * Internal function for collecting codec info and param from the SDP media.
 */
static pj_status_t get_audio_codec_info_param(pjmedia_stream_info *si,
                                              pj_pool_t *pool,
                                              pjmedia_codec_mgr *mgr,
                                              const pjmedia_sdp_media *local_m,
                                              const pjmedia_sdp_media *rem_m)
{
    const pjmedia_sdp_attr *attr;
    pjmedia_sdp_rtpmap *rtpmap;
    unsigned i, fmti, pt = 0;
    unsigned rx_ev_clock_rate = 0;
    pj_status_t status;

    /* Find the first codec which is not telephone-event */
    for ( fmti = 0; fmti < local_m->desc.fmt_count; ++fmti ) {
        pjmedia_sdp_rtpmap r;

        if ( !pj_isdigit(*local_m->desc.fmt[fmti].ptr) )
            return PJMEDIA_EINVALIDPT;
        pt = pj_strtoul(&local_m->desc.fmt[fmti]);

        if (pt < 96) {
            /* This is known static PT. Skip rtpmap checking because it is
             * optional. */
            break;
        }

        attr = pjmedia_sdp_media_find_attr(local_m, &ID_RTPMAP,
                                           &local_m->desc.fmt[fmti]);
        if (attr == NULL)
            continue;

        status = pjmedia_sdp_attr_get_rtpmap(attr, &r);
        if (status != PJ_SUCCESS)
            continue;

        if (pj_strcmp(&r.enc_name, &ID_TELEPHONE_EVENT) != 0)
            break;
    }
    if ( fmti >= local_m->desc.fmt_count )
        return PJMEDIA_EINVALIDPT;

    /* Get payload type for receiving direction */
    si->rx_pt = pt;

    /* Get codec info.
     * For static payload types, get the info from codec manager.
     * For dynamic payload types, MUST get the rtpmap.
     */
    if (pt < 96) {
        pj_bool_t has_rtpmap;

        rtpmap = NULL;
        has_rtpmap = PJ_TRUE;

        attr = pjmedia_sdp_media_find_attr(local_m, &ID_RTPMAP,
                                           &local_m->desc.fmt[fmti]);
        if (attr == NULL) {
            has_rtpmap = PJ_FALSE;
        }
        if (attr != NULL) {
            status = pjmedia_sdp_attr_to_rtpmap(pool, attr, &rtpmap);
            if (status != PJ_SUCCESS)
                has_rtpmap = PJ_FALSE;
        }

        /* Build codec format info: */
        if (has_rtpmap) {
            si->fmt.type = si->type;
            si->fmt.pt = pj_strtoul(&local_m->desc.fmt[fmti]);
            pj_strdup(pool, &si->fmt.encoding_name, &rtpmap->enc_name);
            si->fmt.clock_rate = rtpmap->clock_rate;

#if defined(PJMEDIA_HANDLE_G722_MPEG_BUG) && (PJMEDIA_HANDLE_G722_MPEG_BUG != 0)
            /* The session info should have the actual clock rate, because
             * this info is used for calculationg buffer size, etc in stream
             */
            if (si->fmt.pt == PJMEDIA_RTP_PT_G722)
                si->fmt.clock_rate = 16000;
#endif

            /* For audio codecs, rtpmap parameters denotes the number of
             * channels.
             */
            if (si->type == PJMEDIA_TYPE_AUDIO && rtpmap->param.slen) {
                si->fmt.channel_cnt = (unsigned) pj_strtoul(&rtpmap->param);
            } else {
                si->fmt.channel_cnt = 1;
            }

        } else {
            const pjmedia_codec_info *p_info;

            status = pjmedia_codec_mgr_get_codec_info( mgr, pt, &p_info);
            if (status != PJ_SUCCESS)
                return status;

            pj_memcpy(&si->fmt, p_info, sizeof(pjmedia_codec_info));
        }

        /* For static payload type, pt's are symetric */
        si->tx_pt = pt;

    } else {
        pjmedia_codec_id codec_id;
        pj_str_t codec_id_st;
        const pjmedia_codec_info *p_info;

        attr = pjmedia_sdp_media_find_attr(local_m, &ID_RTPMAP,
                                           &local_m->desc.fmt[fmti]);
        if (attr == NULL)
            return PJMEDIA_EMISSINGRTPMAP;

        status = pjmedia_sdp_attr_to_rtpmap(pool, attr, &rtpmap);
        if (status != PJ_SUCCESS)
            return status;

        /* Build codec format info: */

        si->fmt.type = si->type;
        si->fmt.pt = pj_strtoul(&local_m->desc.fmt[fmti]);
        si->fmt.encoding_name = rtpmap->enc_name;
        si->fmt.clock_rate = rtpmap->clock_rate;

        /* For audio codecs, rtpmap parameters denotes the number of
         * channels.
         */
        if (si->type == PJMEDIA_TYPE_AUDIO && rtpmap->param.slen) {
            si->fmt.channel_cnt = (unsigned) pj_strtoul(&rtpmap->param);
        } else {
            si->fmt.channel_cnt = 1;
        }

        /* Normalize the codec info from codec manager. Note that the
         * payload type will be resetted to its default (it might have
         * been rewritten by the SDP negotiator to match to the remote
         * offer), this is intentional as currently some components may
         * prefer (or even require) the default PT in codec info.
         */
        pjmedia_codec_info_to_id(&si->fmt, codec_id, sizeof(codec_id));

        i = 1;
        codec_id_st = pj_str(codec_id);
        status = pjmedia_codec_mgr_find_codecs_by_id(mgr, &codec_id_st,
                                                     &i, &p_info, NULL);
        if (status != PJ_SUCCESS)
            return status;

        pj_memcpy(&si->fmt, p_info, sizeof(pjmedia_codec_info));

        /* Determine payload type for outgoing channel, by finding
         * dynamic payload type in remote SDP that matches the answer.
         */
        si->tx_pt = 0xFFFF;
        for (i=0; i<rem_m->desc.fmt_count; ++i) {
            if (pjmedia_sdp_neg_fmt_match(pool,
                                          (pjmedia_sdp_media*)local_m, fmti,
                                          (pjmedia_sdp_media*)rem_m, i, 0) ==
                PJ_SUCCESS)
            {
                /* Found matched codec. */
                si->tx_pt = pj_strtoul(&rem_m->desc.fmt[i]);
                break;
            }
        }

        if (si->tx_pt == 0xFFFF)
            return PJMEDIA_EMISSINGRTPMAP;
    }


    /* Now that we have codec info, get the codec param. */
    si->param = PJ_POOL_ALLOC_T(pool, pjmedia_codec_param);
    status = pjmedia_codec_mgr_get_default_param(mgr, &si->fmt,
                                                 si->param);

    /* Get remote fmtp for our encoder. */
    pjmedia_stream_info_parse_fmtp(pool, rem_m, si->tx_pt,
                                   &si->param->setting.enc_fmtp);

    /* Get local fmtp for our decoder. */
    pjmedia_stream_info_parse_fmtp(pool, local_m, si->rx_pt,
                                   &si->param->setting.dec_fmtp);

    if (!pj_stricmp2(&si->fmt.encoding_name, "opus")) {
        get_opus_channels_and_clock_rate(&si->param->setting.enc_fmtp,
                                         &si->param->setting.dec_fmtp,
                                         &si->fmt.channel_cnt,
                                         &si->fmt.clock_rate);
    }


    /* Get the remote ptime for our encoder. */
    attr = pjmedia_sdp_attr_find2(rem_m->attr_count, rem_m->attr,
                                  "ptime", NULL);
    if (attr) {
        pj_str_t tmp_val = attr->value;
        unsigned frm_per_pkt;

        pj_strltrim(&tmp_val);

        /* Round up ptime when the specified is not multiple of frm_ptime */
        frm_per_pkt = (pj_strtoul(&tmp_val) +
                      si->param->info.frm_ptime/2) /
                      si->param->info.frm_ptime;
        if (frm_per_pkt != 0) {
            si->param->setting.frm_per_pkt = (pj_uint8_t)frm_per_pkt;
        }
    }

    /* Get remote maxptime for our encoder. */
    attr = pjmedia_sdp_attr_find2(rem_m->attr_count, rem_m->attr,
                                  "maxptime", NULL);
    if (attr) {
        pj_str_t tmp_val = attr->value;

        pj_strltrim(&tmp_val);
        si->tx_maxptime = pj_strtoul(&tmp_val);
    }

    /* When direction is NONE (it means SDP negotiation has failed) we don't
     * need to return a failure here, as returning failure will cause
     * the whole SDP to be rejected. See ticket #:
     *  http://
     *
     * Thanks Alain Totouom
     */
    if (status != PJ_SUCCESS && si->dir != PJMEDIA_DIR_NONE)
        return status;


    /* Get incomming payload type for telephone-events */
    si->rx_event_pt = -1;
    for (i=0; i<local_m->attr_count; ++i) {
        pjmedia_sdp_rtpmap r;

        attr = local_m->attr[i];
        if (pj_strcmp(&attr->name, &ID_RTPMAP) != 0)
            continue;
        if (pjmedia_sdp_attr_get_rtpmap(attr, &r) != PJ_SUCCESS)
            continue;
        if (pj_strcmp(&r.enc_name, &ID_TELEPHONE_EVENT) == 0) {
            si->rx_event_pt = pj_strtoul(&r.pt);
            rx_ev_clock_rate = r.clock_rate;
            break;
        }
    }

    /* Get outgoing payload type for telephone-events */
    si->tx_event_pt = -1;
    for (i=0; i<rem_m->attr_count; ++i) {
        pjmedia_sdp_rtpmap r;

        attr = rem_m->attr[i];
        if (pj_strcmp(&attr->name, &ID_RTPMAP) != 0)
            continue;
        if (pjmedia_sdp_attr_get_rtpmap(attr, &r) != PJ_SUCCESS)
            continue;
        if (pj_strcmp(&r.enc_name, &ID_TELEPHONE_EVENT) == 0) {
            /* Check if the clock rate matches local event's clock rate. */
            if (r.clock_rate == rx_ev_clock_rate) {
                si->tx_event_pt = pj_strtoul(&r.pt);
                break;
            } else if (si->tx_event_pt == -1) {
                si->tx_event_pt = pj_strtoul(&r.pt);
            }
        }
    }

    return PJ_SUCCESS;
}



/*
 * Create stream info from SDP media line.
 */
PJ_DEF(pj_status_t) pjmedia_stream_info_from_sdp(
                                           pjmedia_stream_info *si,
                                           pj_pool_t *pool,
                                           pjmedia_endpt *endpt,
                                           const pjmedia_sdp_session *local,
                                           const pjmedia_sdp_session *remote,
                                           unsigned stream_idx)
{
    pjmedia_stream_info_common *csi = (pjmedia_stream_info_common *)si;
    pjmedia_codec_mgr *mgr;
    const pjmedia_sdp_media *local_m;
    const pjmedia_sdp_media *rem_m;
    pj_status_t status;

    status = pjmedia_stream_info_common_from_sdp(csi, pool, endpt, local,
                                                 remote, stream_idx);
    if (status != PJ_SUCCESS)
        return status;

    /* Keep SDP shortcuts */
    local_m = local->media[stream_idx];
    rem_m = remote->media[stream_idx];

    /* Media type must be audio */
    if (pjmedia_get_type(&local_m->desc.media) != PJMEDIA_TYPE_AUDIO)
        return PJMEDIA_EINVALIMEDIATYPE;

    /* Get codec manager. */
    mgr = pjmedia_endpt_get_codec_mgr(endpt);

    /* Get the payload number for receive channel. */
    /*
       Previously we used to rely on fmt[0] being the selected codec,
       but some UA sends telephone-event as fmt[0] and this would
       cause assert failure below.

       Thanks Chris Hamilton <chamilton .at. cs.dal.ca> for this patch.

    // And codec must be numeric!
    if (!pj_isdigit(*local_m->desc.fmt[0].ptr) ||
        !pj_isdigit(*rem_m->desc.fmt[0].ptr))
    {
        return PJMEDIA_EINVALIDPT;
    }

    pt = pj_strtoul(&local_m->desc.fmt[0]);
    pj_assert(PJMEDIA_RTP_PT_TELEPHONE_EVENTS==0 ||
              pt != PJMEDIA_RTP_PT_TELEPHONE_EVENTS);
    */

    /* Get codec info and param */
    status = get_audio_codec_info_param(si, pool, mgr, local_m, rem_m);

    return status;
}

