/* 
 * Copyright (C) 2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/stream_common.h>
#include <pjmedia/errno.h>
#include <pj/log.h>
#include <pj/rand.h>

#define THIS_FILE       "stream_common.c"

#define LOGERR_(expr)                   PJ_PERROR(4,expr);

/*  Number of send error before repeat the report. */
#define SEND_ERR_COUNT_TO_REPORT        50

static const pj_str_t ID_IN = { "IN", 2 };
static const pj_str_t ID_IP4 = { "IP4", 3};
static const pj_str_t ID_IP6 = { "IP6", 3};

/*
 * Create stream info from SDP media line.
 */
PJ_DEF(pj_status_t) pjmedia_stream_info_common_from_sdp(
                                           pjmedia_stream_info_common *si,
                                           pj_pool_t *pool,
                                           pjmedia_endpt *endpt,
                                           const pjmedia_sdp_session *local,
                                           const pjmedia_sdp_session *remote,
                                           unsigned stream_idx)
{
    const pj_str_t STR_INACTIVE = { "inactive", 8 };
    const pj_str_t STR_SENDONLY = { "sendonly", 8 };
    const pj_str_t STR_RECVONLY = { "recvonly", 8 };

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

    /* Reset: */
    pj_bzero(si, sizeof(*si));

#if PJMEDIA_HAS_RTCP_XR && PJMEDIA_STREAM_ENABLE_XR
    /* Set default RTCP XR enabled/disabled */
    si->rtcp_xr_enabled = PJ_TRUE;
#endif

    /* Media type: */
    si->type = pjmedia_get_type(&local_m->desc.media);

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

    /* Leave SSRC to random. */
    si->ssrc = pj_rand();

    /* Set default jitter buffer parameter. */
    si->jb_init = si->jb_max = si->jb_min_pre = si->jb_max_pre = -1;
    si->jb_discard_algo = PJMEDIA_JB_DISCARD_PROGRESSIVE;

    if (pjmedia_get_type(&local_m->desc.media) == PJMEDIA_TYPE_AUDIO ||
        pjmedia_get_type(&local_m->desc.media) == PJMEDIA_TYPE_VIDEO)
    {
        /* Get local RTCP-FB info */
        if (pjmedia_get_type(&local_m->desc.media) == PJMEDIA_TYPE_AUDIO ||
        pjmedia_get_type(&local_m->desc.media) == PJMEDIA_TYPE_VIDEO)
        status = pjmedia_rtcp_fb_decode_sdp2(pool, endpt, NULL, local,
                                             stream_idx, si->rx_pt,
                                             &si->loc_rtcp_fb);
        if (status != PJ_SUCCESS)
            return status;

        /* Get remote RTCP-FB info */
        status = pjmedia_rtcp_fb_decode_sdp2(pool, endpt, NULL, remote,
                                             stream_idx, si->tx_pt,
                                             &si->rem_rtcp_fb);
        if (status != PJ_SUCCESS)
            return status;
    }

    return status;
}

#if defined(PJMEDIA_STREAM_ENABLE_KA) && PJMEDIA_STREAM_ENABLE_KA!=0

PJ_DEF(void)
pjmedia_stream_ka_config_default(pjmedia_stream_ka_config *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    cfg->start_count = PJMEDIA_STREAM_START_KA_CNT;
    cfg->start_interval = PJMEDIA_STREAM_START_KA_INTERVAL_MSEC;
    cfg->ka_interval = PJMEDIA_STREAM_KA_INTERVAL;
}

#endif

/*
 * Parse fmtp for specified format/payload type.
 */
PJ_DEF(pj_status_t) pjmedia_stream_info_parse_fmtp( pj_pool_t *pool,
                                                    const pjmedia_sdp_media *m,
                                                    unsigned pt,
                                                    pjmedia_codec_fmtp *fmtp)
{
    const pjmedia_sdp_attr *attr;
    pjmedia_sdp_fmtp sdp_fmtp;
    char fmt_buf[8];
    pj_str_t fmt;
    pj_status_t status;

    pj_assert(m && fmtp);

    pj_bzero(fmtp, sizeof(pjmedia_codec_fmtp));

    /* Get "fmtp" attribute for the format */
    pj_ansi_snprintf(fmt_buf, sizeof(fmt_buf), "%d", pt);
    fmt = pj_str(fmt_buf);
    attr = pjmedia_sdp_media_find_attr2(m, "fmtp", &fmt);
    if (attr == NULL)
        return PJ_SUCCESS;

    /* Parse "fmtp" attribute */
    status = pjmedia_sdp_attr_get_fmtp(attr, &sdp_fmtp);
    if (status != PJ_SUCCESS)
        return status;

    return pjmedia_stream_info_parse_fmtp_data(pool, &sdp_fmtp.fmt_param, fmtp);
}

PJ_DECL(pj_status_t) pjmedia_stream_info_parse_fmtp_data(pj_pool_t *pool,
                                                         const pj_str_t *str,
                                                         pjmedia_codec_fmtp *fmtp)
{
    /* Prepare parsing */
    char *p = str->ptr;
    char *p_end = p + str->slen;

    /* Parse */
    while (p < p_end) {
        char *token, *start, *end;

        if (fmtp->cnt >= PJMEDIA_CODEC_MAX_FMTP_CNT) {
            PJ_LOG(4,(THIS_FILE,
                      "Warning: fmtp parameter count exceeds "
                      "PJMEDIA_CODEC_MAX_FMTP_CNT"));
            return PJ_SUCCESS;
        }

        /* Skip whitespaces */
        while (p < p_end && (*p == ' ' || *p == '\t')) ++p;
        if (p == p_end)
            break;

        /* Get token */
        start = p;
        while (p < p_end && *p != ';') ++p;
        end = p - 1;

        /* Right trim */
        while (end >= start && (*end == ' '  || *end == '\t' || 
                                *end == '\r' || *end == '\n' ))
            --end;

        /* Forward a char after trimming */
        ++end;

        /* Store token */
        if (end > start) {
            char *p2 = start;

            if (pool) {
                token = (char*)pj_pool_alloc(pool, end - start);
                pj_memcpy(token, start, end - start);
            } else {
                token = start;
            }

            /* Check if it contains '=' */
            while (p2 < end && *p2 != '=') ++p2;

            if (p2 < end) {
                char *p3;

                pj_assert (*p2 == '=');

                /* Trim whitespace before '=' */
                p3 = p2 - 1;
                while (p3 >= start && (*p3 == ' ' || *p3 == '\t')) --p3;

                /* '=' found, get param name */
                pj_strset(&fmtp->param[fmtp->cnt].name, token, p3 - start + 1);

                /* Trim whitespace after '=' */
                p3 = p2 + 1;
                while (p3 < end && (*p3 == ' ' || *p3 == '\t')) ++p3;

                /* Advance token to first char after '=' */
                token = token + (p3 - start);
                start = p3;
            }

            /* Got param value */
            pj_strset(&fmtp->param[fmtp->cnt++].val, token, end - start);
        }

        /* Next */
        ++p;
    }

    return PJ_SUCCESS;
}

/*
 * Get stream statistics.
 */
PJ_DEF(pj_status_t)
pjmedia_stream_common_get_stat( const pjmedia_stream_common *c_strm,
                                pjmedia_rtcp_stat *stat)
{
    PJ_ASSERT_RETURN(c_strm && stat, PJ_EINVAL);

    pj_memcpy(stat, &c_strm->rtcp.stat, sizeof(pjmedia_rtcp_stat));
    return PJ_SUCCESS;
}

/*
 * Reset the stream statistics in the middle of a stream session.
 */
PJ_DEF(pj_status_t)
pjmedia_stream_common_reset_stat(pjmedia_stream_common *c_strm)
{
    PJ_ASSERT_RETURN(c_strm, PJ_EINVAL);

    pjmedia_rtcp_init_stat(&c_strm->rtcp.stat);

    return PJ_SUCCESS;
}

/*
 * Send RTCP SDES.
 */
PJ_DEF(pj_status_t)
pjmedia_stream_common_send_rtcp_sdes( pjmedia_stream_common *stream )
{
    PJ_ASSERT_RETURN(stream, PJ_EINVAL);

    return pjmedia_stream_send_rtcp(stream, PJ_TRUE, PJ_FALSE, PJ_FALSE,
                                    PJ_FALSE, PJ_FALSE, PJ_FALSE);
}

/*
 * Send RTCP BYE.
 */
PJ_DEF(pj_status_t)
pjmedia_stream_common_send_rtcp_bye( pjmedia_stream_common *c_strm )
{
    PJ_ASSERT_RETURN(c_strm, PJ_EINVAL);

    if (c_strm->enc && c_strm->transport) {
        return pjmedia_stream_send_rtcp(c_strm, PJ_TRUE, PJ_TRUE, PJ_FALSE,
                                        PJ_FALSE, PJ_FALSE, PJ_FALSE);
    }

    return PJ_SUCCESS;
}

/**
 * Get RTP session information from stream.
 */
PJ_DEF(pj_status_t)
pjmedia_stream_common_get_rtp_session_info(pjmedia_stream_common *c_strm,
                                pjmedia_stream_rtp_sess_info *session_info)
{
    session_info->rx_rtp = &c_strm->dec->rtp;
    session_info->tx_rtp = &c_strm->enc->rtp;
    session_info->rtcp = &c_strm->rtcp;
    return PJ_SUCCESS;
}

static pj_status_t build_rtcp_fb(pjmedia_stream_common *c_strm, void *buf,
                                 pj_size_t *length)
{
    pj_status_t status;

    /* Generic NACK */
    if (c_strm->send_rtcp_fb_nack && c_strm->rtcp_fb_nack.pid >= 0)
    {
        status = pjmedia_rtcp_fb_build_nack(&c_strm->rtcp, buf, length, 1,
                                            &c_strm->rtcp_fb_nack);
        if (status != PJ_SUCCESS)
            return status;

        /* Reset Packet ID */
        c_strm->rtcp_fb_nack.pid = -1;
    }

    return PJ_SUCCESS;
}

pj_status_t pjmedia_stream_send_rtcp(pjmedia_stream_common *c_strm,
                                     pj_bool_t with_sdes,
                                     pj_bool_t with_bye,
                                     pj_bool_t with_xr,
                                     pj_bool_t with_fb,
                                     pj_bool_t with_fb_nack,
                                     pj_bool_t with_fb_pli)
{
    void *sr_rr_pkt;
    pj_uint8_t *pkt;
    int len, max_len;
    pj_status_t status;

    /* We need to prevent data race since there is only a single instance
     * of rtcp packet buffer. And to avoid deadlock with media transport,
     * we use the transport's group lock.
     */
    if (c_strm->transport->grp_lock)
        pj_grp_lock_acquire(c_strm->transport->grp_lock);

    /* Build RTCP RR/SR packet */
    pjmedia_rtcp_build_rtcp(&c_strm->rtcp, &sr_rr_pkt, &len);

#if !defined(PJMEDIA_HAS_RTCP_XR) || (PJMEDIA_HAS_RTCP_XR == 0)
    with_xr = PJ_FALSE;
#endif

    if (with_sdes || with_bye || with_xr || with_fb || with_fb_nack ||
        with_fb_pli)
    {
        pkt = (pj_uint8_t*) c_strm->out_rtcp_pkt;
        pj_memcpy(pkt, sr_rr_pkt, len);
        max_len = c_strm->out_rtcp_pkt_size;
    } else {
        pkt = (pj_uint8_t*)sr_rr_pkt;
        max_len = len;
    }

    /* Build RTCP SDES packet, forced if also send RTCP-FB */
    with_sdes = with_sdes || with_fb_pli || with_fb_nack;

    /* Build RTCP SDES packet */
    if (with_sdes) {
        pjmedia_rtcp_sdes sdes;
        pj_size_t sdes_len;

        pj_bzero(&sdes, sizeof(sdes));
        sdes.cname = c_strm->cname;
        sdes_len = max_len - len;
        status = pjmedia_rtcp_build_rtcp_sdes(&c_strm->rtcp, pkt+len,
                                              &sdes_len, &sdes);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(c_strm->port.info.name.ptr, status,
                                     "Error generating RTCP SDES"));
        } else {
            len += (int)sdes_len;
        }
    }

    if (with_fb) {
        pj_size_t fb_len = max_len - len;
        status = build_rtcp_fb(c_strm, pkt+len, &fb_len);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(c_strm->port.info.name.ptr, status,
                                     "Error generating RTCP FB"));
        } else {
            len += (int)fb_len;
        }
    }

    /* Build RTCP XR packet */
#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
    if (with_xr) {
        int i;
        pjmedia_jb_state jb_state;
        void *xr_pkt;
        int xr_len;

        /* Update RTCP XR with current JB states */
        pjmedia_jbuf_get_state(c_strm->jb, &jb_state);

        i = jb_state.avg_delay;
        status = pjmedia_rtcp_xr_update_info(&c_strm->rtcp.xr_session,
                                             PJMEDIA_RTCP_XR_INFO_JB_NOM, i);
        pj_assert(status == PJ_SUCCESS);

        i = jb_state.max_delay;
        status = pjmedia_rtcp_xr_update_info(&c_strm->rtcp.xr_session,
                                             PJMEDIA_RTCP_XR_INFO_JB_MAX, i);
        pj_assert(status == PJ_SUCCESS);

        pjmedia_rtcp_build_rtcp_xr(&c_strm->rtcp.xr_session, 0,
                                   &xr_pkt, &xr_len);

        if (xr_len + len <= max_len) {
            pj_memcpy(pkt+len, xr_pkt, xr_len);
            len += xr_len;

            /* Send the RTCP XR to third-party destination if specified */
            if (c_strm->rtcp_xr_dest_len) {
                pjmedia_transport_send_rtcp2(c_strm->transport,
                                             &c_strm->rtcp_xr_dest,
                                             c_strm->rtcp_xr_dest_len,
                                             xr_pkt, xr_len);
            }

        } else {
            PJ_PERROR(4,(c_strm->port.info.name.ptr, PJ_ETOOBIG,
                         "Error generating RTCP-XR"));
        }
    }
#endif

    /* Build RTCP BYE packet */
    if (with_bye) {
        pj_size_t bye_len;

        bye_len = max_len - len;
        status = pjmedia_rtcp_build_rtcp_bye(&c_strm->rtcp, pkt+len,
                                             &bye_len, NULL);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(c_strm->port.info.name.ptr, status,
                                     "Error generating RTCP BYE"));
        } else {
            len += (int)bye_len;
        }
    }

    /* Build RTCP-FB generic NACK packet */
    if (with_fb_nack && c_strm->rtcp_fb_nack.pid >= 0) {
        pj_size_t fb_len = max_len - len;
        status = pjmedia_rtcp_fb_build_nack(&c_strm->rtcp, pkt+len, &fb_len,
                                            1, &c_strm->rtcp_fb_nack);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(c_strm->port.info.name.ptr, status,
                                     "Error generating RTCP-FB NACK"));
        } else {
            len += (int)fb_len;
        }
    }

    /* Build RTCP-FB PLI packet */
    if (with_fb_pli) {
        pj_size_t fb_len = max_len - len;
        status = pjmedia_rtcp_fb_build_pli(&c_strm->rtcp, pkt+len, &fb_len);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(c_strm->port.info.name.ptr, status,
                                     "Error generating RTCP-FB PLI"));
        } else {
            len += (int)fb_len;
            PJ_LOG(5,(c_strm->name.ptr, "Sending RTCP-FB PLI packet"));
        }
    }

    /* Send! */
    status = pjmedia_transport_send_rtcp(c_strm->transport, pkt, len);
    if (status != PJ_SUCCESS) {
        if (c_strm->rtcp_tx_err_cnt++ == 0) {
            LOGERR_((c_strm->port.info.name.ptr, status,
                     "Error sending RTCP"));
        }
        if (c_strm->rtcp_tx_err_cnt > SEND_ERR_COUNT_TO_REPORT) {
            c_strm->rtcp_tx_err_cnt = 0;
        }
    }

    if (c_strm->transport->grp_lock)
        pj_grp_lock_release(c_strm->transport->grp_lock);

    return status;
}
