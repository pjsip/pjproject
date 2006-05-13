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
/*
 * Based on implementation kindly contributed by Switchlab, Ltd.
 */
#ifndef __PJMEDIA_JBUF_H__
#define __PJMEDIA_JBUF_H__


/**
 * @file jbuf.h
 * @brief Adaptive jitter buffer implementation.
 */
#include <pjmedia/types.h>

/**
 * @defgroup PJMED_JBUF Adaptive jitter buffer
 * @ingroup PJMEDIA
 * @{
 *
 */


PJ_BEGIN_DECL


/**
 * Types of frame returned by the jitter buffer.
 */
enum pjmedia_jb_frame_type 
{
    PJMEDIA_JB_MISSING_FRAME	   = 0, /**< No frame because it's missing  */
    PJMEDIA_JB_NORMAL_FRAME	   = 1, /**< Normal frame is being returned */
    PJMEDIA_JB_ZERO_PREFETCH_FRAME = 2, /**< Zero frame is being returned  
					     because JB is bufferring.	    */
    PJMEDIA_JB_ZERO_EMPTY_FRAME	   = 3	/**< Zero frame is being returned
					     because JB is empty.	    */
};


/**
 * @see pjmedia_jb_frame_type.
 */
typedef enum pjmedia_jb_frame_type pjmedia_jb_frame_type;


/**
 * This structure describes jitter buffer current status.
 */
struct pjmedia_jb_state
{
    unsigned	frame_size;	    /**< Individual frame size, in bytes.   */
    unsigned	prefetch;	    /**< Current prefetch value, in frames  */
    unsigned	min_prefetch;	    /**< Minimum allowed prefetch, in frms. */
    unsigned	max_prefetch;	    /**< Maximum allowed prefetch, in frms. */
    unsigned	size;		    /**< Current buffer size, in frames.    */
};


/**
 * @see pjmedia_jb_state
 */
typedef struct pjmedia_jb_state pjmedia_jb_state;


/**
 * The constant PJMEDIA_JB_DEFAULT_INIT_DELAY specifies default jitter
 * buffer prefetch count during jitter buffer creation.
 */
#define PJMEDIA_JB_DEFAULT_INIT_DELAY    15

/**
 * Opaque declaration for jitter buffer.
 */
typedef struct pjmedia_jbuf pjmedia_jbuf;


/**
 * Create an adaptive jitter buffer according to the specification. If
 * application wants to have a fixed jitter buffer, it may call
 * #pjmedia_jbuf_set_fixed() after the jitter buffer is created.
 *
 * This function may allocate large chunk of memory to keep the frames in 
 * the buffer.
 *
 * @param pool		The pool to allocate memory.
 * @param name		Name to identify the jitter buffer for logging
 *			purpose.
 * @param frame_size	The size of each frame that will be kept in the
 *			jitter buffer, in bytes. This should correspond
 *			to the minimum frame size supported by the codec.
 *			For example, a 10ms frame (80 bytes) would be 
 *			recommended for G.711 codec.
 * @param max_count	Maximum number of frames that can be kept in the
 *			jitter buffer. This effectively means the maximum
 *			delay that may be introduced by this jitter 
 *			buffer.
 * @param p_jb		Pointer to receive jitter buffer instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jbuf_create(pj_pool_t *pool,
					 const pj_str_t *name,
					 unsigned frame_size,
					 unsigned max_count,
					 pjmedia_jbuf **p_jb);

/**
 * Set the jitter buffer to fixed delay mode. The default behavior
 * is to adapt the delay with actual packet delay.
 *
 * @param jb		The jitter buffer
 * @param prefetch	The fixed delay value, in number of frames.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jbuf_set_fixed( pjmedia_jbuf *jb,
					     unsigned prefetch);


/**
 * Set the jitter buffer to adaptive mode.
 *
 * @param jb		The jitter buffer.
 * @param prefetch	The prefetch value to be applied to the jitter
 *			buffer.
 * @param min_prefetch	The minimum delay that must be applied to each
 *			incoming packets, in number of frames. The
 *			default value is zero.
 * @param max_prefetch	The maximum allowable value for prefetch delay,
 *			in number of frames. The default value is equal
 *			to the size of the jitter buffer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jbuf_set_adaptive( pjmedia_jbuf *jb,
					        unsigned prefetch,
					        unsigned min_prefetch,
						unsigned max_prefetch);


/**
 * Destroy jitter buffer instance.
 *
 * @param jb		The jitter buffer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jbuf_destroy(pjmedia_jbuf *jb);


/**
 * Restart jitter. This function flushes all packets in the buffer and
 * reset the internal sequence number.
 *
 * @param jb		The jitter buffer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jbuf_reset(pjmedia_jbuf *jb);


/**
 * Put a frame to the jitter buffer. If the frame can be accepted (based
 * on the sequence number), the jitter buffer will copy the frame and put
 * it in the appropriate position in the buffer.
 *
 * Application MUST manage it's own synchronization when multiple threads
 * are accessing the jitter buffer at the same time.
 *
 * @param jb		The jitter buffer.
 * @param frame		Pointer to frame buffer to be stored in the jitter
 *			buffer.
 * @param size		The frame size.
 * @param frame_seq	The frame sequence number.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(void) pjmedia_jbuf_put_frame( pjmedia_jbuf *jb, 
				      const void *frame, 
				      pj_size_t size, 
				      int frame_seq);

/**
 * Get a frame from the jitter buffer. The jitter buffer will return the
 * oldest frame from it's buffer, when it is available.
 *
 * Application MUST manage it's own synchronization when multiple threads
 * are accessing the jitter buffer at the same time.
 *
 * @param jb		The jitter buffer.
 * @param frame		Buffer to receive the payload from the jitter buffer.
 *			Application MUST make sure that the buffer has
 *			appropriate size (i.e. not less than the frame size,
 *			as specified when the jitter buffer was created).
 *			The jitter buffer only copied a frame to this 
 *			buffer when the frame type returned by this function
 *			is PJMEDIA_JB_NORMAL_FRAME.
 * @param p_frm_type	Pointer to receive frame type. If jitter buffer is
 *			currently empty or bufferring, the frame type will
 *			be set to PJMEDIA_JB_ZERO_FRAME, and no frame will
 *			be copied. If the jitter buffer detects that frame is
 *			missing with current sequence number, the frame type
 *			will be set to PJMEDIA_JB_MISSING_FRAME, and no
 *			frame will be copied. If there is a frame, the jitter
 *			buffer will copy the frame to the buffer, and frame
 *			type will be set to PJMEDIA_JB_NORMAL_FRAME.
 */
PJ_DECL(void) pjmedia_jbuf_get_frame( pjmedia_jbuf *jb, 
				      void *frame, 
				      char *p_frm_type);


/**
 * Get jitter buffer current state/settings.
 *
 * @param jb		The jitter buffer.
 * @param state		Buffer to receive jitter buffer state.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jbuf_get_state( pjmedia_jbuf *jb,
					     pjmedia_jb_state *state );



PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_JBUF_H__ */
