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

#include <pjmedia/config.h>
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
 * @see pjmedia_type
 */
typedef enum pjmedia_type pjmedia_type;



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
 * @see pjmedia_dir
 */
typedef enum pjmedia_dir pjmedia_dir;


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
 * Opague declaration of media endpoint. 
 */
typedef struct pjmedia_endpt pjmedia_endpt;


/** 
 * Media socket info. 
 */
typedef struct pjmedia_sock_info
{

    pj_sock_t	    rtp_sock;	    /**< Socket for RTP.		    */
    pj_sockaddr_in  rtp_addr_name;  /**< Local RTP address to be advertised.*/
    pj_sock_t	    rtcp_sock;	    /**< Socket for RTCP.		    */
    pj_sockaddr_in  rtcp_addr_name; /**< Local RTCP addr to be advertised.  */

} pjmedia_sock_info;


/**
 * This is a general purpose function set PCM samples to zero.
 * Since this function is needed by many parts of the library, it is important
 * that the library should select the best performance for this.
 *
 * @param samples	The 16bit PCM samples.
 * @param count		Number of samples.
 */
PJ_INLINE(void) pjmedia_zero_samples(pj_int16_t *samples, unsigned count)
{
    unsigned i;
    for (i=0; i<count; ++i)
	samples[i] = 0;
}


#endif	/* __PJMEDIA_TYPES_H__ */

