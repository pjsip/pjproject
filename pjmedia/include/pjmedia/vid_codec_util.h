/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_VID_CODEC_UTIL_H__
#define __PJMEDIA_VID_CODEC_UTIL_H__


/**
 * @file vid_codec_util.h
 * @brief Video codec utilities.
 */

#include <pjmedia/vid_codec.h>


PJ_BEGIN_DECL


/**
 * Definition of H.263 parameters.
 */
typedef struct pjmedia_vid_codec_h263_fmtp
{
    unsigned mpi_cnt;		    /**< # of parsed MPI param		    */
    struct mpi {
	pjmedia_rect_size   size;   /**< Picture size/resolution	    */
	unsigned	    val;    /**< MPI value			    */
    } mpi[8];			    /**< Minimum Picture Interval parameter */

} pjmedia_vid_codec_h263_fmtp;


/**
 * Parse SDP fmtp of H.263.
 *
 * @param fmtp		The H.263 SDP fmtp to be parsed.
 * @param h263_fmtp	The parsing result.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_codec_h263_parse_fmtp(
				const pjmedia_codec_fmtp *fmtp,
				pjmedia_vid_codec_h263_fmtp *h263_fmtp);


/**
 * Parse, negotiate, and apply the encoding and decoding SDP fmtp of H.263
 * in the specified codec parameter.
 *
 * @param param		The codec parameter.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_codec_h263_apply_fmtp(
				pjmedia_vid_codec_param *param);


/**
 * Definition of H.264 parameters.
 */
typedef struct pjmedia_vid_codec_h264_fmtp
{
    /* profile-level-id */
    pj_uint8_t	    profile_idc;    /**< Profile ID			    */
    pj_uint8_t	    profile_iop;    /**< Profile constraints bits	    */
    pj_uint8_t	    level;	    /**< Level				    */

    /* packetization-mode */
    pj_uint8_t	    packetization_mode;	/**< Packetization mode		    */

    /* max-mbps, max-fs, max-cpb, max-dpb, and max-br */
    unsigned	    max_mbps;	    /**< Max macroblock processing rate	    */
    unsigned	    max_fs;	    /**< Max frame size (in macroblocks)    */
    unsigned	    max_cpb;	    /**< Max coded picture buffer size	    */
    unsigned	    max_dpb;	    /**< Max decoded picture buffer size    */
    unsigned	    max_br;	    /**< Max video bit rate		    */

    /* sprop-parameter-sets, in NAL units */
    pj_size_t	    sprop_param_sets_len;   /**< Parameter set length	    */
    pj_uint8_t	    sprop_param_sets[256];  /**< Parameter set (SPS & PPS),
						 in NAL unit bitstream	    */

} pjmedia_vid_codec_h264_fmtp;


/**
 * Parse SDP fmtp of H.264.
 *
 * @param fmtp		The H.264 SDP fmtp to be parsed.
 * @param h264_fmtp	The parsing result.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_vid_codec_parse_h264_fmtp(
				const pjmedia_codec_fmtp *fmtp,
				pjmedia_vid_codec_h264_fmtp *h264_fmtp);




PJ_END_DECL


#endif	/* __PJMEDIA_VID_CODEC_UTIL_H__ */
