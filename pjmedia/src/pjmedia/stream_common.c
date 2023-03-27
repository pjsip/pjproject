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
#include <pj/log.h>

#define THIS_FILE       "stream_common.c"

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
    char *p, *p_end, fmt_buf[8];
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

    /* Prepare parsing */
    p = sdp_fmtp.fmt_param.ptr;
    p_end = p + sdp_fmtp.fmt_param.slen;

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

