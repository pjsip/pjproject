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
#ifndef __PJMEDIA_SESSION_H__
#define __PJMEDIA_SESSION_H__


/**
 * @file session.h
 * @brief Media Session.
 */

#include <pj/types.h>
#include <pjmedia/mediamgr.h>
#include <pjmedia/stream.h>
#include <pjmedia/sdp.h>

PJ_BEGIN_DECL 

/**
 * @defgroup PJMED_SES Media session
 * @ingroup PJMEDIA
 * @{
 */

/** Opaque declaration of media session. */
typedef struct pj_media_session_t pj_media_session_t;

/** Media socket info. */
typedef struct pj_media_sock_info
{
    pj_sock_t	    rtp_sock, rtcp_sock;
    pj_sockaddr_in  rtp_addr_name;
} pj_media_sock_info;

/** Stream info. */
typedef struct pj_media_stream_info
{
    pj_str_t		type;
    pj_media_dir_t	dir;
    pj_str_t		transport;
    pj_media_sock_info	sock_info;
    pj_str_t		rem_addr;
    unsigned short	rem_port;
    unsigned		fmt_cnt;
    pj_codec_id		fmt[PJSDP_MAX_FMT];

} pj_media_stream_info;

/** Flag for modifying stream. */
enum
{
    PJ_MEDIA_STREAM_MODIFY_DIR = 1,
};

/**
 * Create new session offering.
 */
PJ_DECL(pj_media_session_t*) 
pj_media_session_create ( pj_med_mgr_t *mgr, const pj_media_sock_info *skinfo );

/**
 * Create new session based on peer's offering.
 */
PJ_DECL(pj_media_session_t*) 
pj_media_session_create_from_sdp ( pj_med_mgr_t *mgr, const pjsdp_session_desc *sdp,
				   const pj_media_sock_info *skinfo);

/**
 * Duplicate session. The new session is inactive.
 */
PJ_DECL(pj_media_session_t*)
pj_media_session_clone (const pj_media_session_t *session);

/**
 * Create SDP description from the session.
 */
PJ_DECL(pjsdp_session_desc*)
pj_media_session_create_sdp ( const pj_media_session_t *session, pj_pool_t *pool,
			      pj_bool_t only_first_fmt);

/**
 * Update session with SDP answer from peer. The session must NOT active.
 */
PJ_DECL(pj_status_t)
pj_media_session_update ( pj_media_session_t *session, 
			  const pjsdp_session_desc *sdp);

/**
 * Enumerate media streams in the session.
 * @return the actual number of streams.
 */
PJ_DECL(unsigned)
pj_media_session_enum_streams (const pj_media_session_t *session, 
			       unsigned count, const pj_media_stream_info *info[]);

/**
 * Get stream statistics.
 */
PJ_DECL(pj_status_t)
pj_media_session_get_stat (const pj_media_session_t *session, unsigned index,
			   pj_media_stream_stat *tx_stat,
			   pj_media_stream_stat *rx_stat);

/**
 * Modify stream, only when stream is inactive.
 */
PJ_DECL(pj_status_t)
pj_media_session_modify_stream (pj_media_session_t *session, unsigned index,
				unsigned modify_flag, const pj_media_stream_info *info);

/**
 * Activate all streams in media session.
 */
PJ_DECL(pj_status_t)
pj_media_session_activate (pj_media_session_t *session);

/**
 * Activate individual stream in media session.
 */
PJ_DECL(pj_status_t)
pj_media_session_activate_stream (pj_media_session_t *session, unsigned index);

/**
 * Destroy media session.
 */
PJ_DECL(pj_status_t)
pj_media_session_destroy (pj_media_session_t *session);


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJMEDIA_SESSION_H__ */
