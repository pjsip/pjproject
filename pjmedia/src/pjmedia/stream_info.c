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

static const pj_str_t ID_IN = { "IN", 2 };
static const pj_str_t ID_IP4 = { "IP4", 3};
static const pj_str_t ID_IP6 = { "IP6", 3};
//static const pj_str_t ID_SDP_NAME = { "pjmedia", 7 };
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
    const pj_str_t STR_INACTIVE = { "inactive", 8 };
    const pj_str_t STR_SENDONLY = { "sendonly", 8 };
    const pj_str_t STR_RECVONLY = { "recvonly", 8 };

    pjmedia_codec_mgr *mgr;
    const pjmedia_sdp_attr *attr;
    const pjmedia_sdp_media *local_m;
    const pjmedia_sdp_media *rem_m;
    const pjmedia_sdp_conn *local_conn;
    const pjmedia_sdp_conn *rem_conn;
    int rem_af, local_af;
    unsigned i;
    pj_status_t status;


    /* Validate arguments: */
    PJ_ASSERT_RETURN(pool && si && local && remote, PJ_EINVAL);
    PJ_ASSERT_RETURN(stream_idx < local->media_count, PJ_EINVAL);
    PJ_ASSERT_RETURN(stream_idx < remote->media_count, PJ_EINVAL);

    /* Keep SDP shortcuts */
    local_m = local->media[stream_idx];
    rem_m = remote->media[stream_idx];

    local_conn = local_m->conn ? local_m->conn : local->conn;
    if (local_conn == NULL)
        return PJMEDIA_SDP_EMISSINGCONN;

    rem_conn = rem_m->conn ? rem_m->conn : remote->conn;
    if (rem_conn == NULL)
        return PJMEDIA_SDP_EMISSINGCONN;

    /* Media type must be audio */
    if (pjmedia_get_type(&local_m->desc.media) != PJMEDIA_TYPE_AUDIO)
        return PJMEDIA_EINVALIMEDIATYPE;

    /* Get codec manager. */
    mgr = pjmedia_endpt_get_codec_mgr(endpt);

    /* Reset: */

    pj_bzero(si, sizeof(*si));

#if PJMEDIA_HAS_RTCP_XR && PJMEDIA_STREAM_ENABLE_XR
    /* Set default RTCP XR enabled/disabled */
    si->rtcp_xr_enabled = PJ_TRUE;
#endif

    /* Media type: */
    si->type = PJMEDIA_TYPE_AUDIO;

    /* Transport protocol */

    /* At this point, transport type must be compatible,
     * the transport instance will do more validation later.
     */
    status = pjmedia_sdp_transport_cmp(&rem_m->desc.transport,
                                       &local_m->desc.transport);
    if (status != PJ_SUCCESS)
        return PJMEDIA_SDPNEG_EINVANSTP;

    /* Get the transport protocol */
    si->proto = pjmedia_sdp_transport_get_proto(&local_m->desc.transport);

    /* Just return success if stream is not RTP/AVP compatible */
    if (!PJMEDIA_TP_PROTO_HAS_FLAG(si->proto, PJMEDIA_TP_PROTO_RTP_AVP))
        return PJ_SUCCESS;

    /* Check address family in remote SDP */
    rem_af = pj_AF_UNSPEC();
    if (pj_stricmp(&rem_conn->net_type, &ID_IN)==0) {
        if (pj_stricmp(&rem_conn->addr_type, &ID_IP4)==0) {
            rem_af = pj_AF_INET();
        } else if (pj_stricmp(&rem_conn->addr_type, &ID_IP6)==0) {
            rem_af = pj_AF_INET6();
        }
    }

    if (rem_af==pj_AF_UNSPEC()) {
        /* Unsupported address family */
        return PJ_EAFNOTSUP;
    }

    /* Set remote address: */
    status = pj_sockaddr_init(rem_af, &si->rem_addr, &rem_conn->addr,
                              rem_m->desc.port);
    if (status == PJ_ERESOLVE && rem_af == pj_AF_INET()) {
        /* Handle special case in NAT64 scenario where for some reason, server
         * puts IPv6 (literal or FQDN) in SDP answer while indicating "IP4"
         * in its address type, let's retry resolving using AF_INET6.
         */
        status = pj_sockaddr_init(pj_AF_INET6(), &si->rem_addr,
                                  &rem_conn->addr, rem_m->desc.port);
    }
    if (status != PJ_SUCCESS) {
        /* Invalid IP address. */
        return PJMEDIA_EINVALIDIP;
    }

    /* Check address family of local info */
    local_af = pj_AF_UNSPEC();
    if (pj_stricmp(&local_conn->net_type, &ID_IN)==0) {
        if (pj_stricmp(&local_conn->addr_type, &ID_IP4)==0) {
            local_af = pj_AF_INET();
        } else if (pj_stricmp(&local_conn->addr_type, &ID_IP6)==0) {
            local_af = pj_AF_INET6();
        }
    }

    if (local_af==pj_AF_UNSPEC()) {
        /* Unsupported address family */
        return PJ_SUCCESS;
    }

    /* Set remote address: */
    status = pj_sockaddr_init(local_af, &si->local_addr, &local_conn->addr,
                              local_m->desc.port);
    if (status != PJ_SUCCESS) {
        /* Invalid IP address. */
        return PJMEDIA_EINVALIDIP;
    }

    /* Local and remote address family must match, except when ICE is used
     * by both sides (see also ticket #1952).
     */
    if (local_af != rem_af) {
        const pj_str_t STR_ICE_CAND = { "candidate", 9 };
        if (pjmedia_sdp_media_find_attr(rem_m, &STR_ICE_CAND, NULL)==NULL ||
            pjmedia_sdp_media_find_attr(local_m, &STR_ICE_CAND, NULL)==NULL)
        {
            return PJ_EAFNOTSUP;
        }
    }

    /* Media direction: */

    if (local_m->desc.port == 0 ||
        pj_sockaddr_has_addr(&si->local_addr)==PJ_FALSE ||
        pj_sockaddr_has_addr(&si->rem_addr)==PJ_FALSE ||
        pjmedia_sdp_media_find_attr(local_m, &STR_INACTIVE, NULL)!=NULL)
    {
        /* Inactive stream. */

        si->dir = PJMEDIA_DIR_NONE;

    } else if (pjmedia_sdp_media_find_attr(local_m, &STR_SENDONLY, NULL)!=NULL) {

        /* Send only stream. */

        si->dir = PJMEDIA_DIR_ENCODING;

    } else if (pjmedia_sdp_media_find_attr(local_m, &STR_RECVONLY, NULL)!=NULL) {

        /* Recv only stream. */

        si->dir = PJMEDIA_DIR_DECODING;

    } else {

        /* Send and receive stream. */

        si->dir = PJMEDIA_DIR_ENCODING_DECODING;

    }

    /* No need to do anything else if stream is rejected */
    if (local_m->desc.port == 0) {
        return PJ_SUCCESS;
    }

    /* Check if "rtcp-mux" is present in the SDP. */
    attr = pjmedia_sdp_attr_find2(rem_m->attr_count, rem_m->attr,
                                  "rtcp-mux", NULL);
    if (attr)
        si->rtcp_mux = PJ_TRUE;

    /* If "rtcp" attribute is present in the SDP, set the RTCP address
     * from that attribute. Otherwise, calculate from RTP address.
     */
    attr = pjmedia_sdp_attr_find2(rem_m->attr_count, rem_m->attr,
                                  "rtcp", NULL);
    if (attr) {
        pjmedia_sdp_rtcp_attr rtcp;
        status = pjmedia_sdp_attr_get_rtcp(attr, &rtcp);
        if (status == PJ_SUCCESS) {
            if (rtcp.addr.slen) {
                status = pj_sockaddr_init(rem_af, &si->rem_rtcp, &rtcp.addr,
                                          (pj_uint16_t)rtcp.port);
                if (status != PJ_SUCCESS)
                    return PJMEDIA_EINVALIDIP;
            } else {
                pj_sockaddr_init(rem_af, &si->rem_rtcp, NULL,
                                 (pj_uint16_t)rtcp.port);
                pj_memcpy(pj_sockaddr_get_addr(&si->rem_rtcp),
                          pj_sockaddr_get_addr(&si->rem_addr),
                          pj_sockaddr_get_addr_len(&si->rem_addr));
            }
        }
    }

    if (!pj_sockaddr_has_addr(&si->rem_rtcp)) {
        int rtcp_port;

        pj_memcpy(&si->rem_rtcp, &si->rem_addr, sizeof(pj_sockaddr));
        rtcp_port = pj_sockaddr_get_port(&si->rem_addr) + 1;
        pj_sockaddr_set_port(&si->rem_rtcp, (pj_uint16_t)rtcp_port);
    }

    /* Check if "ssrc" attribute is present in the SDP. */
    for (i = 0; i < rem_m->attr_count; i++) {
        if (pj_strcmp2(&rem_m->attr[i]->name, "ssrc") == 0) {
            pjmedia_sdp_ssrc_attr ssrc;

            status = pjmedia_sdp_attr_get_ssrc(
                        (const pjmedia_sdp_attr *)rem_m->attr[i], &ssrc);
            if (status == PJ_SUCCESS) {
                si->has_rem_ssrc = PJ_TRUE;
                si->rem_ssrc = ssrc.ssrc;
                if (ssrc.cname.slen > 0) {
                    pj_strdup(pool, &si->rem_cname, &ssrc.cname);
                    break;
                }
            }
        }
    }

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
    if (status != PJ_SUCCESS)
        return status;

    /* Leave SSRC to random. */
    si->ssrc = pj_rand();

    /* Set default jitter buffer parameter. */
    si->jb_init = si->jb_max = si->jb_min_pre = si->jb_max_pre = -1;
    si->jb_discard_algo = PJMEDIA_JB_DISCARD_PROGRESSIVE;

    /* Get local RTCP-FB info */
    status = pjmedia_rtcp_fb_decode_sdp2(pool, endpt, NULL, local, stream_idx,
                                         si->rx_pt, &si->loc_rtcp_fb);
    if (status != PJ_SUCCESS)
        return status;

    /* Get remote RTCP-FB info */
    status = pjmedia_rtcp_fb_decode_sdp2(pool, endpt, NULL, remote, stream_idx,
                                         si->tx_pt, &si->rem_rtcp_fb);
    if (status != PJ_SUCCESS)
        return status;

    return status;
}

