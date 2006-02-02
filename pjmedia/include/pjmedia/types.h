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

