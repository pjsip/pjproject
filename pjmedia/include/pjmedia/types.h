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
#ifndef __PJMEDIA_TYPES_H__
#define __PJMEDIA_TYPES_H__

/**
 * @file pjmedia/types.h Basic Types
 * @brief Basic PJMEDIA types.
 */

#include <pjmedia/config.h>
#include <pj/sock.h>	    /* pjmedia_sock_info	*/
#include <pj/string.h>	    /* pj_memcpy(), pj_memset() */

/**
 * @defgroup PJMEDIA_PORT Media Ports Framework
 * @brief Extensible framework for media terminations
 */


/**
 * @defgroup PJMEDIA_FRAME_OP Audio Manipulation Algorithms
 * @brief Algorithms to manipulate audio frames
 */

/**
 * @defgroup PJMEDIA_TYPES Basic Types
 * @ingroup PJMEDIA_BASE
 * @brief Basic PJMEDIA types and operations.
 * @{
 */

/**
 * Top most media type.
 */
typedef enum pjmedia_type
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

    /** The media is application. */
    PJMEDIA_TYPE_APPLICATION = 4

} pjmedia_type;


/**
 * Media transport protocol.
 */
typedef enum pjmedia_tp_proto
{
    /** No transport type */
    PJMEDIA_TP_PROTO_NONE = 0,

    /** RTP using A/V profile */
    PJMEDIA_TP_PROTO_RTP_AVP,

    /** Secure RTP */
    PJMEDIA_TP_PROTO_RTP_SAVP,

    /** Unknown */
    PJMEDIA_TP_PROTO_UNKNOWN

} pjmedia_tp_proto;


/**
 * Media direction.
 */
typedef enum pjmedia_dir
{
    /** None */
    PJMEDIA_DIR_NONE = 0,

    /** Encoding (outgoing to network) stream */
    PJMEDIA_DIR_ENCODING = 1,

    /** Decoding (incoming from network) stream. */
    PJMEDIA_DIR_DECODING = 2,

    /** Incoming and outgoing stream. */
    PJMEDIA_DIR_ENCODING_DECODING = 3

} pjmedia_dir;



/* Alternate names for media direction: */

/**
 * Direction is capturing audio frames.
 */
#define PJMEDIA_DIR_CAPTURE	PJMEDIA_DIR_ENCODING

/**
 * Direction is playback of audio frames.
 */
#define PJMEDIA_DIR_PLAYBACK	PJMEDIA_DIR_DECODING

/**
 * Direction is both capture and playback.
 */
#define PJMEDIA_DIR_CAPTURE_PLAYBACK	PJMEDIA_DIR_ENCODING_DECODING


/**
 * Create 32bit port signature from ASCII characters.
 */
#define PJMEDIA_PORT_SIGNATURE(a,b,c,d)	    \
	    (a<<24 | b<<16 | c<<8 | d)


/**
 * Opaque declaration of media endpoint.
 */
typedef struct pjmedia_endpt pjmedia_endpt;


/*
 * Forward declaration for stream (needed by transport).
 */
typedef struct pjmedia_stream pjmedia_stream;


/**
 * Media socket info is used to describe the underlying sockets
 * to be used as media transport.
 */
typedef struct pjmedia_sock_info
{
    /** The RTP socket handle */
    pj_sock_t	    rtp_sock;

    /** Address to be advertised as the local address for the RTP
     *  socket, which does not need to be equal as the bound
     *  address (for example, this address can be the address resolved
     *  with STUN).
     */
    pj_sockaddr	    rtp_addr_name;

    /** The RTCP socket handle. */
    pj_sock_t	    rtcp_sock;

    /** Address to be advertised as the local address for the RTCP
     *  socket, which does not need to be equal as the bound
     *  address (for example, this address can be the address resolved
     *  with STUN).
     */
    pj_sockaddr	    rtcp_addr_name;

} pjmedia_sock_info;


/**
 * Macro for packing format.
 */
#define PJMEDIA_FORMAT_PACK(C1, C2, C3, C4) ( C4<<24 | C3<<16 | C2<<8 | C1 )

/**
 * This enumeration describes format ID. 
 */
typedef enum pjmedia_format_id
{
    /**
     * 16bit linear
     */
    PJMEDIA_FORMAT_L16	    = 0,
    
    /**
     * Alias for PJMEDIA_FORMAT_L16
     */
    PJMEDIA_FORMAT_PCM	    = PJMEDIA_FORMAT_L16,

    /**
     * G.711 ALAW
     */
    PJMEDIA_FORMAT_PCMA	    = PJMEDIA_FORMAT_PACK('A', 'L', 'A', 'W'),

    /**
     * Alias for PJMEDIA_FORMAT_PCMA
     */
    PJMEDIA_FORMAT_ALAW	    = PJMEDIA_FORMAT_PCMA,

    /**
     * G.711 ULAW
     */
    PJMEDIA_FORMAT_PCMU	    = PJMEDIA_FORMAT_PACK('u', 'L', 'A', 'W'),

    /**
     * Aliaw for PJMEDIA_FORMAT_PCMU
     */
    PJMEDIA_FORMAT_ULAW	    = PJMEDIA_FORMAT_PCMU,

    /**
     * AMR narrowband
     */
    PJMEDIA_FORMAT_AMR	    = PJMEDIA_FORMAT_PACK(' ', 'A', 'M', 'R'),

    /**
     * ITU G.729
     */
    PJMEDIA_FORMAT_G729	    = PJMEDIA_FORMAT_PACK('G', '7', '2', '9'),

    /**
     * Internet Low Bit-Rate Codec (ILBC)
     */
    PJMEDIA_FORMAT_ILBC	    = PJMEDIA_FORMAT_PACK('I', 'L', 'B', 'C')

} pjmedia_format_id;


/**
 * Media format information.
 */
typedef struct pjmedia_format
{
    /** Format ID */
    pjmedia_format_id	id;

    /** Bitrate. */
    pj_uint32_t		bitrate;

    /** Flag to indicate whether VAD is enabled */
    pj_bool_t		vad;

} pjmedia_format;



/**
 * This is a general purpose function set PCM samples to zero.
 * Since this function is needed by many parts of the library,
 * by putting this functionality in one place, it enables some.
 * clever people to optimize this function.
 *
 * @param samples	The 16bit PCM samples.
 * @param count		Number of samples.
 */
PJ_INLINE(void) pjmedia_zero_samples(pj_int16_t *samples, unsigned count)
{
#if 1
    pj_bzero(samples, (count<<1));
#elif 0
    unsigned i;
    for (i=0; i<count; ++i) samples[i] = 0;
#else
    unsigned i;
    count >>= 1;
    for (i=0; i<count; ++i) ((pj_int32_t*)samples)[i] = (pj_int32_t)0;
#endif
}


/**
 * This is a general purpose function to copy samples from/to buffers with
 * equal size. Since this function is needed by many parts of the library,
 * by putting this functionality in one place, it enables some.
 * clever people to optimize this function.
 */
PJ_INLINE(void) pjmedia_copy_samples(pj_int16_t *dst, const pj_int16_t *src,
				     unsigned count)
{
#if 1
    pj_memcpy(dst, src, (count<<1));
#elif 0
    unsigned i;
    for (i=0; i<count; ++i) dst[i] = src[i];
#else
    unsigned i;
    count >>= 1;
    for (i=0; i<count; ++i)
	((pj_int32_t*)dst)[i] = ((pj_int32_t*)src)[i];
#endif
}


/**
 * This is a general purpose function to copy samples from/to buffers with
 * equal size. Since this function is needed by many parts of the library,
 * by putting this functionality in one place, it enables some.
 * clever people to optimize this function.
 */
PJ_INLINE(void) pjmedia_move_samples(pj_int16_t *dst, const pj_int16_t *src,
				     unsigned count)
{
#if 1
    pj_memmove(dst, src, (count<<1));
#elif 0
    unsigned i;
    for (i=0; i<count; ++i) dst[i] = src[i];
#else
    unsigned i;
    count >>= 1;
    for (i=0; i<count; ++i)
	((pj_int32_t*)dst)[i] = ((pj_int32_t*)src)[i];
#endif
}

/**
 * @}
 */


#endif	/* __PJMEDIA_TYPES_H__ */

