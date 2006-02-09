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

#include <pjmedia/endpoint.h>
#include <pjmedia/stream.h>
#include <pjmedia/sdp.h>

PJ_BEGIN_DECL 

/**
 * @defgroup PJMED_SES Media session
 * @ingroup PJMEDIA
 * @{
 */


/**
 * Create new session offering based on the local and remote SDP.
 * The session will start immediately.
 *
 * @param endpt		The PJMEDIA endpoint instance.
 * @param stream_cnt	Maximum number of streams to be created. This
 *			also denotes the number of elements in the
 *			socket information.
 * @param skinfo	Array of socket informations. The argument stream_cnt
 *			specifies the number of elements in this array. One
 *			element is needed for each media stream to be
 *			created in the session.
 * @param local_sdp	The SDP describing local capability.
 * @param rem_sdp	The SDP describing remote capability.
 * @param p_session	Pointer to receive the media session.
 *
 * @return		PJ_SUCCESS if media session can be created 
 *			successfully.
 */
PJ_DECL(pj_status_t) pjmedia_session_create( pjmedia_endpt *endpt, 
					     unsigned stream_cnt,
					     const pjmedia_sock_info skinfo[],
					     const pjmedia_sdp_session *local_sdp,
					     const pjmedia_sdp_session *rem_sdp,
					     pjmedia_session **p_session );


/**
 * Activate all streams in media session for the specified direction.
 *
 * @param session	The media session.
 * @param dir		The direction to activate.
 *
 * @return		PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pjmedia_session_resume(pjmedia_session *session,
					    pjmedia_dir dir);


/**
 * Suspend receipt and transmission of all streams in media session
 * for the specified direction.
 *
 * @param session	The media session.
 * @param dir		The media direction to suspend.
 *
 * @return		PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pjmedia_session_pause(pjmedia_session *session,
					   pjmedia_dir dir);

/**
 * Suspend receipt and transmission of individual stream in media session
 * for the specified direction.
 *
 * @param session	The media session.
 * @param index		The stream index.
 * @param dir		The media direction to pause.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_pause_stream( pjmedia_session *session,
						   unsigned index,
						   pjmedia_dir dir);

/**
 * Activate individual stream in media session for the specified direction.
 *
 * @param session	The media session.
 * @param index		The stream index.
 * @param dir		The media direction to activate.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_resume_stream(pjmedia_session *session,
						   unsigned index,
						   pjmedia_dir dir);

/**
 * Enumerate media streams in the session.
 *
 * @param session	The media session.
 * @param count		On input, specifies the number of elements in
 *			the array. On output, the number will be filled
 *			with number of streams in the session.
 * @param strm_info	Array of stream info.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_enum_streams(const pjmedia_session *session,
						  unsigned *count, 
						  pjmedia_stream_info strm_info[]);


/**
 * Get session statistics. The stream statistic shows various
 * indicators such as packet count, packet lost, jitter, delay, etc.
 *
 * @param session	The media session.
 * @param count		On input, specifies the number of elements in
 *			the array. On output, the number will be filled
 *			with number of streams in the session.
 * @param stat		Array of stream statistics.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_get_stat(const pjmedia_session *session,
					      unsigned *count,
					      pjmedia_stream_stat stat[]);

/**
 * Get individual stream statistics. The stream statistic shows various
 * indicators such as packet count, packet lost, jitter, delay, etc.
 *
 * @param s		The media session.
 * @param index		The stream index.
 * @param stat		Stream statistics.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_session_get_stream_stat(const pjmedia_session *s,
						     unsigned index,
						     pjmedia_stream_stat *stat);

/**
 * Destroy media session.
 *
 * @param session	The media session.
 *
 * @return		PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pjmedia_session_destroy(pjmedia_session *session);



/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJMEDIA_SESSION_H__ */
