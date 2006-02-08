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
#ifndef __PJMEDIA_STREAM_H__
#define __PJMEDIA_STREAM_H__


/**
 * @file stream.h
 * @brief Stream of media.
 */

#include <pjmedia/sound.h>
#include <pjmedia/codec.h>
#include <pjmedia/endpoint.h>
#include <pj/sock.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJMED_SES Media session
 * @ingroup PJMEDIA
 * @{
 */

/**
 * Opaque declaration for media channel.
 * Media channel is unidirectional flow of media from sender to
 * receiver.
 */
typedef struct pjmedia_channel pjmedia_channel;

/** 
 * This structure describes media stream information. Each media stream
 * corresponds to one "m=" line in SDP session descriptor, and it has
 * its own RTP/RTCP socket pair.
 */
struct pjmedia_stream_info
{
    pjmedia_type	type;	    /**< Media type (audio, video)	    */
    pjmedia_dir		dir;	    /**< Media direction.		    */
    pjmedia_sock_info	sock_info;  /**< Media transport (RTP/RTCP sockets) */
    pj_sockaddr_in	rem_addr;   /**< Remote RTP address		    */
    pjmedia_codec_info	fmt;	    /**< Codec format info.		    */
    pj_uint32_t		ssrc;	    /**< RTP SSRC.			    */
    int			jb_min;	    /**< Jitter buffer min delay.	    */
    int			jb_max;	    /**< Jitter buffer max delay.	    */
    int			jb_maxcnt;  /**< Jitter buffer max delay.	    */
};


/**
 * Individual channel statistic.
 */
struct pjmedia_channel_stat
{
    pj_uint32_t pkt;	    /**< Total number of packets.		    */
    pj_uint32_t bytes;	    /**< Total number of bytes, including RTP hdr.  */
    pj_uint32_t lost;	    /**< Total number of packet lost		    */
};

/**
 * Stream statistic.
 */
struct pjmedia_stream_stat
{
    pjmedia_channel_stat    enc;    /**< Encoder statistics.		    */
    pjmedia_channel_stat    dec;    /**< Decoder statistics.		    */
};


/**
 * Create a media stream based on the specified stream parameter.
 * All channels in the stream initially will be inactive.
 *
 * @param endpt		Media endpoint.
 * @param pool		Pool to allocate memory for the stream. A large
 *			number of memory may be needed because jitter
 *			buffer needs to preallocate some storage.
 * @param info		Stream information.
 * @param p_stream	Pointer to receive the media stream.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_create(pjmedia_endpt *endpt,
					   pj_pool_t *pool,
					   const pjmedia_stream_info *info,
					   pjmedia_stream **p_stream);

/**
 * Destroy the media stream.
 *
 * @param stream	The media stream.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_destroy(pjmedia_stream *stream);

/**
 * Start the media stream. This will start the appropriate channels
 * in the media stream, depending on the media direction that was set
 * when the stream was created.
 *
 * @param stream	The media stream.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_start(pjmedia_stream *stream);


/**
 * Get the stream statistics.
 *
 * @param stream	The media stream.
 * @param stat		Media stream statistics.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_get_stat( const pjmedia_stream *stream,
					      pjmedia_stream_stat *stat);

/**
 * Pause the individual channel in the stream.
 *
 * @param channel	The media channel.
 * @param dir		Which direction to pause.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_stream_pause( pjmedia_stream *stream,
					   pjmedia_dir dir);

/**
 * Resume the individual channel in the stream.
 *
 * @param channel	The media channel.
 * @param dir		Which direction to resume.
 *
 * @return		PJ_SUCCESS on success;
 */
PJ_DECL(pj_status_t) pjmedia_stream_resume(pjmedia_stream *stream,
					   pjmedia_dir dir);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_STREAM_H__ */
