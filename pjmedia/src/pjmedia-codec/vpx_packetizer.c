/* 
 * Copyright (C) 2020 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia-codec/vpx_packetizer.h>
#include <pjmedia/errno.h>
#include <pjmedia/types.h>
#include <pjmedia/vid_codec_util.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>


#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

#define THIS_FILE		"vpx_packetizer.c"

/* VPX packetizer definition */
struct pjmedia_vpx_packetizer
{
    /* Current settings */
    pjmedia_vpx_packetizer_cfg cfg;
};

/*
 * Initialize VPX packetizer.
 */
PJ_DEF(void) pjmedia_vpx_packetizer_cfg_default(pjmedia_vpx_packetizer_cfg *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    cfg->fmt_id = PJMEDIA_FORMAT_VP8;
    cfg->mtu =PJMEDIA_MAX_VID_PAYLOAD_SIZE;
}

/*
 * Create vpx packetizer.
 */
PJ_DEF(pj_status_t) pjmedia_vpx_packetizer_create(
				pj_pool_t *pool,
				const pjmedia_vpx_packetizer_cfg *cfg,
				pjmedia_vpx_packetizer **p)
{
    pjmedia_vpx_packetizer *p_;

    PJ_ASSERT_RETURN(pool && p, PJ_EINVAL);

    if (cfg && cfg->fmt_id != PJMEDIA_FORMAT_VP8 &&
	cfg->fmt_id != PJMEDIA_FORMAT_VP9)
    {
	return PJ_ENOTSUP;
    }

    p_ = PJ_POOL_ZALLOC_T(pool, pjmedia_vpx_packetizer);
    if (cfg) {
	pj_memcpy(&p_->cfg, cfg, sizeof(*cfg));
    } else {
	pjmedia_vpx_packetizer_cfg_default(&p_->cfg);
    }
    *p = p_;

    return PJ_SUCCESS;
}

/*
 * Generate an RTP payload from H.264 frame bitstream, in-place processing.
 */
PJ_DEF(pj_status_t) pjmedia_vpx_packetize(const pjmedia_vpx_packetizer *pktz,
					  pj_size_t bits_len,
                                          unsigned *bits_pos,
                                          pj_bool_t is_keyframe,
                                          pj_uint8_t **payload,
                                          pj_size_t *payload_len)
{
    unsigned payload_desc_size = 1;
    unsigned max_size = pktz->cfg.mtu - payload_desc_size;
    unsigned remaining_size = (unsigned)bits_len - *bits_pos;
    unsigned out_size = (unsigned)*payload_len;
    pj_uint8_t *bits = *payload;

    *payload_len = PJ_MIN(remaining_size, max_size);
    if (*payload_len + payload_desc_size > out_size)
	return PJMEDIA_CODEC_EFRMTOOSHORT;

    /* Set payload header */
    bits[0] = 0;
    if (pktz->cfg.fmt_id == PJMEDIA_FORMAT_VP8) {
	/* Set N: Non-reference frame */
        if (!is_keyframe) bits[0] |= 0x20;
        /* Set S: Start of VP8 partition. */
        if (*bits_pos == 0) bits[0] |= 0x10;
    } else if (pktz->cfg.fmt_id == PJMEDIA_FORMAT_VP9) {
	/* Set P: Inter-picture predicted frame */
        if (!is_keyframe) bits[0] |= 0x40;
        /* Set B: Start of a frame */
        if (*bits_pos == 0) bits[0] |= 0x8;
        /* Set E: End of a frame */
        if (*bits_pos + *payload_len == bits_len) {
            bits[0] |= 0x4;
	}
    }
    return PJ_SUCCESS;
}


/*
 * Append RTP payload to a VPX picture bitstream
 */
PJ_DEF(pj_status_t) pjmedia_vpx_unpacketize(pjmedia_vpx_packetizer *pktz,
					    const pj_uint8_t *payload,
                                            pj_size_t payload_len,
					    unsigned  *payload_desc_len)
{
    unsigned desc_len = 1;
    pj_uint8_t *p = (pj_uint8_t *)payload;

#define INC_DESC_LEN() {if (++desc_len >= payload_len) return PJ_ETOOSMALL;}

    if (payload_len <= desc_len) return PJ_ETOOSMALL;

    if (pktz->cfg.fmt_id == PJMEDIA_FORMAT_VP8) {
        /*  0 1 2 3 4 5 6 7
         * +-+-+-+-+-+-+-+-+
         * |X|R|N|S|R| PID | (REQUIRED)
         */
	/* X: Extended control bits present. */
	if (p[0] & 0x80) {
	    INC_DESC_LEN();
	    /* |I|L|T|K| RSV   | */
	    /* I: PictureID present. */
	    if (p[1] & 0x80) {
	    	INC_DESC_LEN();
	    	/* If M bit is set, the PID field MUST contain 15 bits. */
	    	if (p[2] & 0x80) INC_DESC_LEN();
	    }
	    /* L: TL0PICIDX present. */
	    if (p[1] & 0x40) INC_DESC_LEN();
	    /* T: TID present or K: KEYIDX present. */
	    if ((p[1] & 0x20) || (p[1] & 0x10)) INC_DESC_LEN();
	}

    } else if (pktz->cfg.fmt_id == PJMEDIA_FORMAT_VP9) {
        /*  0 1 2 3 4 5 6 7
         * +-+-+-+-+-+-+-+-+
         * |I|P|L|F|B|E|V|-| (REQUIRED)
         */
        /* I: Picture ID (PID) present. */
	if (p[0] & 0x80) {
	    INC_DESC_LEN();
	    /* If M bit is set, the PID field MUST contain 15 bits. */
	    if (p[1] & 0x80) INC_DESC_LEN();
	}
	/* L: Layer indices present. */
	if (p[0] & 0x20) {
	    INC_DESC_LEN();
	    if (!(p[0] & 0x10)) INC_DESC_LEN();
	}
	/* F: Flexible mode.
	 * I must also be set to 1, and if P is set, there's up to 3
	 * reference index.
	 */
	if ((p[0] & 0x10) && (p[0] & 0x80) && (p[0] & 0x40)) {
	    unsigned char *q = p + desc_len;

	    INC_DESC_LEN();
	    if (*q & 0x1) {
	    	q++;
	    	INC_DESC_LEN();
	    	if (*q & 0x1) {
	    	    q++;
	    	    INC_DESC_LEN();
	    	}
	    }
	}
	/* V: Scalability structure (SS) data present. */
	if (p[0] & 0x2) {
	    unsigned char *q = p + desc_len;
	    unsigned N_S = (*q >> 5) + 1;

	    INC_DESC_LEN();
	    /* Y: Each spatial layer's frame resolution present. */
	    if (*q & 0x10) desc_len += N_S * 4;

	    /* G: PG description present flag. */
	    if (*q & 0x8) {
	    	unsigned j;
	    	unsigned N_G = *(p + desc_len);

	    	INC_DESC_LEN();
	    	for (j = 0; j< N_G; j++) {
	    	    unsigned R;

	    	    q = p + desc_len;
	    	    INC_DESC_LEN();
	    	    R = (*q & 0x0F) >> 2;
	    	    desc_len += R;
	    	    if (desc_len >= payload_len)
	    	    	return PJ_ETOOSMALL;
	    	}
	    }
	}
    }
#undef INC_DESC_LEN

    *payload_desc_len = desc_len;
    return PJ_SUCCESS;
}


#endif /* PJMEDIA_HAS_VIDEO */
