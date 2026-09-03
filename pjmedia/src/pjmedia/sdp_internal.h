/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_SDP_INTERNAL_H__
#define __PJMEDIA_SDP_INTERNAL_H__

/* Private API shared by the SDP modules, not for application use. */

#include <pjmedia/sdp.h>

/* Return PJ_TRUE if the media is a bundle-only media as defined by RFC 9143,
 * i.e. it has zero port, an a=bundle-only attribute, and a MID listed in a
 * session level group:BUNDLE attribute. Unlike other zero port media, such
 * media is not disabled, hence it must be validated and negotiated as if it
 * had a non-zero port.
 */
pj_bool_t pjmedia_sdp_media_is_bundle_only(const pjmedia_sdp_session *sdp,
                                           const pjmedia_sdp_media *media);

#endif  /* __PJMEDIA_SDP_INTERNAL_H__ */
