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

#include <pj/types.h>


PJ_BEGIN_DECL


/**
 * Opaque declaration of internal frame type used by jitter buffer. 
 */
struct pj_jbframe;


/**
 * Miscelaneous operation result/status. 
 */ 
typedef enum jb_op_status
{
    PJ_JB_STATUS_TOO_OLD = -2,		/** The packet is too old. */
    PJ_JB_STATUS_TOO_SOON = -3,		/** The packet is too soon. */
    PJ_JB_STATUS_FRAME_NULL = -4,	/** No packet can be retrieved */
    PJ_JB_STATUS_FRAME_MISSING = -5,	/** The specified packet is missing/lost */
} jb_op_status;


/*
 * Frame list, container abstraction for ordered list with fixed maximum
 * size. It is used internally by the jitter buffer.
 */  
typedef struct pj_jbframelist
{
    unsigned		head, count, maxcount;
    struct pj_jbframe  *frames;
} pj_jbframelist;


/**
 * Jitter buffer implementation.
 */ 
typedef struct pj_jitter_buffer
{
    pj_jbframelist  lst;	    /** The frame list. */
    int		    level;	    /** Current, real-time jitter level. */
    int		    max_level;	    /** Maximum level for the period.	 */
    unsigned	    prefetch;	    /** Prefetch count currently used. */
    unsigned	    get_cnt;	    /** Number of get operation during prefetch state. */
    unsigned	    min;	    /** Minimum jitter size, in packets. */
    unsigned	    max;	    /** Maximum jitter size, in packets. */
    pj_uint32_t	    lastseq;	    /** Last sequence number put to jitter buffer. */
    unsigned	    upd_count;	    /** Internal counter to manage update interval. */
    int		    state;	    /** Jitter buffer state (1==operational) */
    int		    last_op;	    /** Last jitter buffer operation. */
} pj_jitter_buffer;


/**
 * Initialize jitter buffer with the specified parameters.
 * This function will allocate internal frame buffer from the specified pool.
 * @param jb The jitter buffer to be initialized.
 * @param pool Pool where memory will be allocated for the frame buffer.
 * @param min The minimum value of jitter buffer, in packets.
 * @param max The maximum value of jitter buffer, in packets.
 * @param maxcount The maximum number of delay, in packets, which must be
 *		   greater than max.
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_jb_init(pj_jitter_buffer *jb, pj_pool_t *pool, 
				unsigned min, unsigned max, unsigned maxcount);

/**
 * Reset jitter buffer according to the parameters specified when the jitter
 * buffer was initialized. Any packets currently in the buffer will be 
 * discarded.
 */
PJ_DECL(void) pj_jb_reset(pj_jitter_buffer *jb);

/**
 * Put a pointer to the buffer with the specified sequence number. The pointer
 * normally points to a buffer held by application, and this pointer will be
 * returned later on when pj_jb_get() is called. The jitter buffer will not try
 * to interpret the content of this pointer.
 * @return:
 *   - PJ_SUCCESS on success.
 *   - PJ_JB_STATUS_TOO_OLD when the packet is too old.
 *   - PJ_JB_STATUS_TOO_SOON when the packet is too soon.
 */
PJ_DECL(pj_status_t) pj_jb_put( pj_jitter_buffer *jb, pj_uint32_t extseq, void *buf );

/**
 * Get the earliest data from the jitter buffer. ONLY when the operation succeeds,
 * the function returns both sequence number and a pointer in the parameters.
 * This returned data corresponds to sequence number and pointer that were
 * given to jitter buffer in the previous pj_jb_put operation.
 * @return 
 *  - PJ_SUCCESS on success
 *  - PJ_JB_STATUS_FRAME_NULL when there is no frames to be returned.
 *  - PJ_JB_STATUS_FRAME_MISSING if the jitter buffer detects that the packet 
 *     is lost. Application may run packet concealment algorithm when it 
 *     receives this status.
 */
PJ_DECL(pj_status_t) pj_jb_get( pj_jitter_buffer *jb, pj_uint32_t *extseq, void **buf );



PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_JBUF_H__ */
