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
#include <pjmedia/vad.h>
#include <pjmedia/errno.h>


PJ_DEF(pj_status_t) pjmedia_vad_create( pj_pool_t *pool,
					pjmedia_vad **p_vad)
{
    return PJ_EINVALIDOP;
}

PJ_DEF(pj_uint32_t) pjmedia_vad_calc_avg_signal_level(pj_int16_t samples[],
						      pj_size_t count)
{
    return PJ_EINVALIDOP;
}

PJ_DEF(pj_status_t) pjmedia_vad_detect_silence( pjmedia_vad *vad,
						pj_int16_t samples[],
						pj_size_t count,
						pj_bool_t *p_silence)
{
    return PJ_EINVALIDOP;
}

