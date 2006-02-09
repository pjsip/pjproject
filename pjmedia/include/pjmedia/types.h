/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#ifndef __PJMEDIA_TYPES_H__
#define __PJMEDIA_TYPES_H__

#include <pj/types.h>
#include <pj/sock.h>


/** 
 * Top most media type. 
 */
enum pjmedia_type
{
    /** No type. */
    PJMEDIA_TYPE_NONE = 0,

    /** The media is audio */
    PJMEDIA_TYPE_AUDIO = 1,

    /** The media is video. */
    PJMEDIA_TYPE_VIDEO = 2,

    /** Unknown media type, in this case the name will be specified in 
     *  encoding_name.
     */
    PJMEDIA_TYPE_UNKNOWN = 3,

};


/** 
 * Media direction. 
 */
enum pjmedia_dir
{
    /** None */
    PJMEDIA_DIR_NONE = 0,

    /** Encoding (outgoing to network) stream */
    PJMEDIA_DIR_ENCODING = 1,

    /** Decoding (incoming from network) stream. */
    PJMEDIA_DIR_DECODING = 2,

    /** Incoming and outgoing stream. */
    PJMEDIA_DIR_ENCODING_DECODING = 3,

};


/**
 * Top level media type.
 */
typedef enum pjmedia_type pjmedia_type;

/**
 * Media direction.
 */
typedef enum pjmedia_dir pjmedia_dir;

/**
 * Codec info.
 */
typedef struct pjmedia_codec_info pjmedia_codec_info;

/**
 * Codec initialization parameter.
 */
typedef struct pjmedia_codec_param pjmedia_codec_param;

/** 
 * Types of media frames. 
 */
typedef enum pjmedia_frame_type pjmedia_frame_type;

/** 
 * This structure describes a media frame. 
 */
typedef struct pjmedia_frame pjmedia_frame;

/**
 * Codec instance.
 */
typedef struct pjmedia_codec pjmedia_codec;

/**
 * Codec factory.
 */
typedef struct pjmedia_codec_factory pjmedia_codec_factory;

/**
 * Codec operation.
 */
typedef struct pjmedia_codec_op pjmedia_codec_op;

/**
 * Codec factory operation.
 */
typedef struct pjmedia_codec_factory_op pjmedia_codec_factory_op;

/**
 * Codec manager.
 */
typedef struct pjmedia_codec_mgr pjmedia_codec_mgr;

/** 
 * Opague declaration of media endpoint. 
 */
typedef struct pjmedia_endpt pjmedia_endpt;


/** 
 * Media socket info. 
 */
typedef struct pjmedia_sock_info
{

    pj_sock_t	    rtp_sock;
    pj_sockaddr_in  rtp_addr_name;
    pj_sock_t	    rtcp_sock;
    pj_sockaddr_in  rtcp_addr_name;

} pjmedia_sock_info;


/**
 * Typedef for media stream information.
 */
typedef struct pjmedia_stream_info pjmedia_stream_info;

/**
 * Typedef for media stream statistic.
 */
typedef struct pjmedia_stream_stat pjmedia_stream_stat;

/**
 * Typedef for media stream.
 */
typedef struct pjmedia_stream pjmedia_stream;

/**
 * Individual channel statistic.
 */
typedef struct pjmedia_channel_stat pjmedia_channel_stat;

/** 
 * Opaque declaration of media session. 
 */
typedef struct pjmedia_session pjmedia_session;

/**
 * Forward declaration for SDP attribute (sdp.h)
 */
typedef struct pjmedia_sdp_attr pjmedia_sdp_attr;

/**
 * Forward declaration for SDP rtpmap attribute (sdp.h)
 */
typedef struct pjmedia_sdp_rtpmap pjmedia_sdp_rtpmap;

/**
 * Forward declaration for SDP fmtp attribute (sdp.h)
 */
typedef struct pjmedia_sdp_fmtp pjmedia_sdp_fmtp;

/**
 * Forward declaration for SDP connection info (sdp.h)
 */
typedef struct pjmedia_sdp_conn pjmedia_sdp_conn;

/**
 * Forward declaration for SDP media line (sdp.h)
 */
typedef struct pjmedia_sdp_media pjmedia_sdp_media;

/**
 * Forward declaration for SDP session (sdp.h)
 */
typedef struct pjmedia_sdp_session pjmedia_sdp_session;

/**
 * Forward declaration for SDP negotiator state (sdp_neg.h).
 */
typedef enum pjmedia_sdp_neg_state pjmedia_sdp_neg_state;

/**
 * Forward declaration for SDP negotiator (sdp_neg.h).
 */
typedef struct pjmedia_sdp_neg pjmedia_sdp_neg;


#endif	/* __PJMEDIA_TYPES_H__ */

