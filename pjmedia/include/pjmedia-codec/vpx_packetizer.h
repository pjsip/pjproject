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
#ifndef __PJMEDIA_VPX_PACKETIZER_H__
#define __PJMEDIA_VPX_PACKETIZER_H__

/**
 * @file vpx_packetizer.h
 * @brief Packetizes VPX bitstream into RTP payload and vice versa.
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * Opaque declaration for VPX packetizer.
 */
typedef struct pjmedia_vpx_packetizer pjmedia_vpx_packetizer;


/**
 * VPX packetizer setting.
 */
typedef struct pjmedia_vpx_packetizer_cfg
{
    /**
     * VPX format id.
     * Default: PJMEDIA_FORMAT_VP8
     */
    pj_uint32_t	fmt_id;

    /**
     * MTU size.
     * Default: PJMEDIA_MAX_VID_PAYLOAD_SIZE
     */
    unsigned mtu;
}
pjmedia_vpx_packetizer_cfg;

/**
 * Use this function to initialize VPX packetizer config.
 *
 * @param cfg	The VPX packetizer config to be initialized.
 */
PJ_DECL(void) pjmedia_vpx_packetizer_cfg_default(
					    pjmedia_vpx_packetizer_cfg *cfg);


/**
 * Create VPX packetizer.
 *
 * @param pool		The memory pool.
 * @param cfg		Packetizer settings, if NULL, default setting
 *			will be used.
 * @param p_pktz	Pointer to receive the packetizer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vpx_packetizer_create(
				    pj_pool_t *pool,
				    const pjmedia_vpx_packetizer_cfg *cfg,
				    pjmedia_vpx_packetizer **p_pktz);


/**
 * Generate an RTP payload from a VPX picture bitstream. Note that this
 * function will apply in-place processing, so the bitstream may be modified
 * during the packetization.
 *
 * @param pktz		The packetizer.
 * @param bits_len	The length of the bitstream.
 * @param bits_pos	The bitstream offset to be packetized.
 * @param is_keyframe	The frame is keyframe.
 * @param payload	The output payload.
 * @param payload_len	The output payload length, on input it represents max
 *                      payload length.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vpx_packetize(const pjmedia_vpx_packetizer *pktz,
					   pj_size_t bits_len,
                                           unsigned *bits_pos,
                                           pj_bool_t is_keyframe,
                                           pj_uint8_t **payload,
                                           pj_size_t *payload_len);


/**
 * Append an RTP payload to an VPX picture bitstream. Note that in case of
 * noticing packet lost, application should keep calling this function with
 * payload pointer set to NULL, as the packetizer need to update its internal
 * state.
 *
 * @param pktz		    The packetizer.
 * @param payload	    The payload to be unpacketized.
 * @param payload_len	    The payload length.
 * @param payload_desc_len  The payload description length.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vpx_unpacketize(pjmedia_vpx_packetizer *pktz,
					     const pj_uint8_t *payload,
                                             pj_size_t payload_len,
					     unsigned  *payload_desc_len);

PJ_END_DECL

#endif	/* __PJMEDIA_VPX_PACKETIZER_H__ */
