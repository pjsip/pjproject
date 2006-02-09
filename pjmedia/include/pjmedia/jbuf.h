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
#ifndef __PJMEDIA_JBUF_H__
#define __PJMEDIA_JBUF_H__


/**
 * @file jbuf.h
 * @brief Adaptive jitter buffer implementation.
 */
/**
 * @defgroup PJMED_JBUF Adaptive jitter buffer
 * @ingroup PJMEDIA
 * @{
 */

#include <pjmedia/types.h>


PJ_BEGIN_DECL


enum pjmedia_jb_frame_type 
{
    PJMEDIA_JB_MISSING_FRAME   = 0,
    PJMEDIA_JB_NORMAL_FRAME    = 1,
    PJMEDIA_JB_ZERO_FRAME      = 2,
};


#define PJMEDIA_JB_DEFAULT_INIT_PREFETCH    15


PJ_DECL(pj_status_t) pjmedia_jbuf_create(pj_pool_t *pool, 
					int frame_size, 
					int initial_prefetch, 
					int max_count,
					pjmedia_jbuf **p_jb);
PJ_DECL(pj_status_t) pjmedia_jbuf_destroy(pjmedia_jbuf *jb);
PJ_DECL(pj_status_t) pjmedia_jbuf_put_frame(pjmedia_jbuf *jb, 
					    const void *frame, 
					    pj_size_t frame_size, 
					    int frame_seq);
PJ_DECL(pj_status_t) pjmedia_jbuf_get_frame( pjmedia_jbuf *jb, 
					     void *frame, 
					     char *p_frame_type);
PJ_DECL(unsigned)    pjmedia_jbuf_get_prefetch_size(pjmedia_jbuf *jb);
PJ_DECL(unsigned)    pjmedia_jbuf_get_current_size(pjmedia_jbuf *jb);



PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_JBUF_H__ */
