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
#include <pjmedia/vid_stream.h>
#include <pjmedia/sdp_neg.h>
#include <pjmedia/stream_common.h>
#include <pj/ctype.h>
#include <pj/rand.h>

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

static const pj_str_t ID_RTPMAP = { "rtpmap", 6 };

/*
 * Internal function for collecting codec info and param from the SDP media.
 */
static pj_status_t get_video_codec_info_param(pjmedia_vid_stream_info *si,
                                              pj_pool_t *pool,
                                              pjmedia_vid_codec_mgr *mgr,
                                              const pjmedia_sdp_media *local_m,
                                              const pjmedia_sdp_media *rem_m)
{
    unsigned pt = 0;
    const pjmedia_vid_codec_info *p_info;
    pj_status_t status;

    pt = pj_strtoul(&local_m->desc.fmt[0]);

    /* Get payload type for receiving direction */
    si->rx_pt = pt;

    /* Get codec info and payload type for transmitting direction. */
    if (pt < 96) {
        /* For static payload types, get the codec info from codec manager. */
        status = pjmedia_vid_codec_mgr_get_codec_info(mgr, pt, &p_info);
        if (status != PJ_SUCCESS)
            return status;

        si->codec_info = *p_info;

        /* Get payload type for transmitting direction.
         * For static payload type, pt's are symetric.
         */
        si->tx_pt = pt;
    } else {
        const pjmedia_sdp_attr *attr;
        pjmedia_sdp_rtpmap *rtpmap;
        pjmedia_codec_id codec_id;
        pj_str_t codec_id_st;
        unsigned i;

        /* Determine payload type for outgoing channel, by finding
         * dynamic payload type in remote SDP that matches the answer.
         */
        si->tx_pt = 0xFFFF;
        for (i=0; i<rem_m->desc.fmt_count; ++i) {
            if (pjmedia_sdp_neg_fmt_match(NULL,
                                          (pjmedia_sdp_media*)local_m, 0,
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

        /* For dynamic payload types, get codec name from the rtpmap */
        attr = pjmedia_sdp_media_find_attr(local_m, &ID_RTPMAP,
                                           &local_m->desc.fmt[0]);
        if (attr == NULL)
            return PJMEDIA_EMISSINGRTPMAP;

        status = pjmedia_sdp_attr_to_rtpmap(pool, attr, &rtpmap);
        if (status != PJ_SUCCESS)
            return status;

        /* Then get the codec info from the codec manager */
        pj_ansi_snprintf(codec_id, sizeof(codec_id), "%.*s/",
                         (int)rtpmap->enc_name.slen, rtpmap->enc_name.ptr);
        codec_id_st = pj_str(codec_id);
        i = 1;
        status = pjmedia_vid_codec_mgr_find_codecs_by_id(mgr, &codec_id_st,
                                                         &i, &p_info, NULL);
        if (status != PJ_SUCCESS)
            return status;

        si->codec_info = *p_info;
    }


    /* Request for codec with the correct packing for streaming */
    si->codec_info.packings = PJMEDIA_VID_PACKING_PACKETS;

    /* Now that we have codec info, get the codec param. */
    si->codec_param = PJ_POOL_ALLOC_T(pool, pjmedia_vid_codec_param);
    status = pjmedia_vid_codec_mgr_get_default_param(mgr,
                                                     &si->codec_info,
                                                     si->codec_param);

    /* Adjust encoding bitrate, if higher than remote preference. The remote
     * bitrate preference is read from SDP "b=TIAS" line in media level.
     */
    if ((si->dir & PJMEDIA_DIR_ENCODING) && rem_m->bandw_count) {
        unsigned i, bandw = 0;

        for (i = 0; i < rem_m->bandw_count; ++i) {
            const pj_str_t STR_BANDW_MODIFIER_TIAS = { "TIAS", 4 };
            if (!pj_stricmp(&rem_m->bandw[i]->modifier,
                &STR_BANDW_MODIFIER_TIAS))
            {
                bandw = rem_m->bandw[i]->value;
                break;
            }
        }

        if (bandw) {
            pjmedia_video_format_detail *enc_vfd;
            enc_vfd = pjmedia_format_get_video_format_detail(
                                        &si->codec_param->enc_fmt, PJ_TRUE);
            if (!enc_vfd->avg_bps || enc_vfd->avg_bps > bandw)
                enc_vfd->avg_bps = bandw * 3 / 4;
            if (!enc_vfd->max_bps || enc_vfd->max_bps > bandw)
                enc_vfd->max_bps = bandw;
        }
    }

    /* Get remote fmtp for our encoder. */
    pjmedia_stream_info_parse_fmtp(pool, rem_m, si->tx_pt,
                                   &si->codec_param->enc_fmtp);

    /* Get local fmtp for our decoder. */
    pjmedia_stream_info_parse_fmtp(pool, local_m, si->rx_pt,
                                   &si->codec_param->dec_fmtp);

    /* When direction is NONE (it means SDP negotiation has failed) we don't
     * need to return a failure here, as returning failure will cause
     * the whole SDP to be rejected. See ticket #:
     *  http://
     *
     * Thanks Alain Totouom
     */
    if (status != PJ_SUCCESS && si->dir != PJMEDIA_DIR_NONE)
        return status;

    return PJ_SUCCESS;
}



/*
 * Create stream info from SDP media line.
 */
PJ_DEF(pj_status_t) pjmedia_vid_stream_info_from_sdp(
                                           pjmedia_vid_stream_info *si,
                                           pj_pool_t *pool,
                                           pjmedia_endpt *endpt,
                                           const pjmedia_sdp_session *local,
                                           const pjmedia_sdp_session *remote,
                                           unsigned stream_idx)
{
    pjmedia_stream_info_common *csi = (pjmedia_stream_info_common *)si;
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
    if (pjmedia_get_type(&local_m->desc.media) != PJMEDIA_TYPE_VIDEO)
        return PJMEDIA_EINVALIMEDIATYPE;

    /* Get codec info and param */
    status = get_video_codec_info_param(si, pool, NULL, local_m, rem_m);

    return status;
}

#endif /* PJMEDIA_HAS_VIDEO */
