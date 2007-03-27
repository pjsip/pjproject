/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#ifndef __pjmedia_ice_H__
#define __pjmedia_ice_H__


/**
 * @file transport_ice.h
 * @brief Stream transport with ICE.
 */

#include <pjmedia/stream.h>
#include <pjnath/ice_stream_transport.h>


/**
 * @defgroup PJMEDIA_TRANSPORT_ICE ICE Socket Transport
 * @ingroup PJMEDIA_TRANSPORT_H
 * @brief Implementation of media transport with ICE.
 * @{
 */

PJ_BEGIN_DECL


PJ_DECL(pj_status_t) pjmedia_ice_create(pjmedia_endpt *endpt,
					const char *name,
					unsigned comp_cnt,
					pj_stun_config *stun_cfg,
					pjmedia_transport **p_tp);
PJ_DECL(pj_status_t) pjmedia_ice_destroy(pjmedia_transport *tp);

PJ_DECL(pj_status_t) pjmedia_ice_start_init(pjmedia_transport *tp,
					    unsigned options,
					    const pj_sockaddr_in *start_addr,
					    const pj_sockaddr_in *stun_srv,
					    const pj_sockaddr_in *turn_srv);
PJ_DECL(pj_status_t) pjmedia_ice_get_init_status(pjmedia_transport *tp);

PJ_DECL(pj_status_t) pjmedia_ice_get_comp(pjmedia_transport *tp,
					  unsigned comp_id,
					  pj_ice_st_comp *comp);

PJ_DECL(pj_status_t) pjmedia_ice_init_ice(pjmedia_transport *tp,
					  pj_ice_role role,
					  const pj_str_t *local_ufrag,
					  const pj_str_t *local_passwd);
PJ_DECL(pj_status_t) pjmedia_ice_modify_sdp(pjmedia_transport *tp,
					    pj_pool_t *pool,
					    pjmedia_sdp_session *sdp);
PJ_DECL(pj_status_t) pjmedia_ice_start_ice(pjmedia_transport *tp,
					   pj_pool_t *pool,
					   pjmedia_sdp_session *rem_sdp,
					   unsigned media_index);
PJ_DECL(pj_status_t) pjmedia_ice_stop_ice(pjmedia_transport *tp);





PJ_END_DECL


/**
 * @}
 */


#endif	/* __pjmedia_ice_H__ */


