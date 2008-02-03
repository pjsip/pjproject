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

#ifndef __PJMEDIA_JBUF2_H__
#define __PJMEDIA_JBUF2_H__


/**
 * @file jbuf2.h
 * @brief Adaptive jitter buffer implementation.
 */
#include <pjmedia/types.h>

/**
 * @defgroup PJMED_JBUF2 Adaptive jitter buffer
 * @ingroup PJMEDIA_FRAME_OP
 * @{
 * This section describes PJMEDIA's implementation of de-jitter buffer.
 * The de-jitter buffer may be set to operate in adaptive mode or fixed
 * delay mode.
 *
 * The jitter buffer is also able to report the status of the current
 * frame (#pjmedia_jb_frame_type). This status is used for examply by
 * @ref PJMED_STRM to invoke the codec's @ref PJMED_PLC algorithm.
 */


PJ_BEGIN_DECL


/**
 * Opaque declaration for jitter buffer.
 */
typedef struct pjmedia_jb2_t pjmedia_jb2_t;

/**
 * Types of jitter buffer phase.
 */
typedef enum pjmedia_jb2_phase 
{
    PJMEDIA_JB_PH_IDLE	    = 0,	/**< No activity in PUT/GET or both */
    PJMEDIA_JB_PH_LEARNING  = 1,	/**< Normal encoded frame */
    PJMEDIA_JB_PH_RUNNING   = 2,	/**< Normal PCM frame */
} pjmedia_jb2_phase;

/**
 * Types of frame returned by the jitter buffer.
 */
typedef enum pjmedia_jb2_frame_type 
{
    PJMEDIA_JB_FT_NULL_FRAME	   = 0, /**< Frame not available */
    PJMEDIA_JB_FT_NORMAL_FRAME	   = 1, /**< Normal encoded frame */
    PJMEDIA_JB_FT_NORMAL_RAW_FRAME = 2, /**< Normal PCM frame */
    PJMEDIA_JB_FT_INTERP_RAW_FRAME = 3, /**< Interpolation frame */
} pjmedia_jb2_frame_type ;

/**
 * Jitter buffer frame definition.
 */
typedef struct pjmedia_jb2_frame
{
    pjmedia_jb2_frame_type    type;
    void		     *buffer;
    pj_size_t		      size; /* in bytes */
    pj_uint8_t		      pt;
    pj_uint16_t		      seq;
    pj_uint32_t		      ts;
} pjmedia_jb2_frame;

/*
 * Jitter buffer state.
 */
typedef struct pjmedia_jb2_state
{
    pjmedia_jb2_phase phase;

    /* in frames */
    pj_uint16_t	 level;
    pj_uint32_t	 frame_cnt;
    
    /* in samples */
    pj_int32_t	 drift;
    pj_uint32_t	 drift_span;
    pj_uint32_t	 cur_size;
    pj_uint32_t	 opt_size;

} pjmedia_jb2_state;


/*
 * Jitter buffer statistic.
 */
typedef struct pjmedia_jb2_stat
{
    /* in frames */
    pj_uint32_t	 lost;
    pj_uint32_t	 late;
    pj_uint32_t	 ooo;
    pj_uint32_t	 out;
    pj_uint32_t	 in;

    /* in ticks */
    pj_uint32_t	 full;
    pj_uint32_t	 empty;

    /* in samples */
    pj_uint32_t	 max_size;
    pj_int32_t	 max_drift;
    pj_int32_t	 max_drift_span;
    pj_int32_t	 max_comp;

    /* in frames */
    pj_uint16_t	 max_level;
} pjmedia_jb2_stat;

/**
 * @see pjmedia_jb_frame_type.
 */
typedef enum pjmedia_jb2_frame_type pjmedia_jb2_frame_type;


/**
 * This structure describes jitter buffer current status.
 */
typedef struct pjmedia_jb2_setting
{
    /**
     * The maximum bytes number of each raw/PCM frame that will be kept in 
     * the jitter buffer.
     */
    pj_size_t	frame_size;

    /**
     * The number of samples for each frame in GET or PUT operation.
     * == timestamp interval between two consecutive frames.
     */
    unsigned	samples_per_frame;

    /**
     * Maximum number of frames that can be kept in the jitter buffer. This 
     * effectively means the maximum delay that may be introduced by this jitter
     * buffer. Set this to zero for adaptive jitter buffer and non-zero for 
     * fixed jitter buffer.
     */
    unsigned    max_frames;

} pjmedia_jb2_setting;

typedef struct pjmedia_jb2_cb
{
    pj_status_t (*decode) (pjmedia_jb2_frame *frame);
    pj_status_t (*plc)    (pjmedia_jb2_frame *frame);
    pj_status_t (*cng)    (pjmedia_jb2_frame *frame);
} pjmedia_jb2_cb;


/**
 * Create an adaptive jitter buffer according to the specification. If
 * application wants to have a fixed jitter buffer, it may call
 * #pjmedia_jb2_set_fixed() after the jitter buffer is created.
 *
 * This function may allocate large chunk of memory to keep the frames in 
 * the buffer.
 *
 * @param pool		The pool to allocate memory.
 * @param name		Name to identify the jitter buffer for logging
 *			purpose.
 * @param p_jb		Pointer to receive jitter buffer instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jb2_create(pj_pool_t *pool,
					  const pj_str_t *name,
					  const pjmedia_jb2_setting *setting,
					  const pjmedia_jb2_cb *cb,
					  pjmedia_jb2_t **p_jb);


/**
 * Destroy jitter buffer instance.
 *
 * @param jb		The jitter buffer.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jb2_destroy(pjmedia_jb2_t *jb);


/**
 * Restart jitter. This function flushes all packets in the buffer and
 * reset the internal sequence number.
 *
 * @param jb		The jitter buffer.
 * 
 * @param setting	New setting to be applied, it can be NULL
 *			if user want to use the current setting.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jb2_reset(pjmedia_jb2_t *jb);


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
PJ_DECL(pj_status_t) pjmedia_jb2_put_frame(pjmedia_jb2_t *jb, 
					   const pjmedia_jb2_frame *frm);

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
PJ_DECL(pj_status_t) pjmedia_jb2_get_frame(pjmedia_jb2_t *jb, 
				           pjmedia_jb2_frame *frm);
				      

/**
 * Get jitter buffer current state.
 *
 * @param jb		The jitter buffer.
 * @param state		Buffer to receive jitter buffer state.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jb2_get_state(pjmedia_jb2_t *jb,
					   pjmedia_jb2_state *state);


/**
 * Get jitter buffer current statistic.
 *
 * @param jb		The jitter buffer.
 * @param statistic	Buffer to receive jitter buffer statistic.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_jb2_get_stat(pjmedia_jb2_t *jb,
					  pjmedia_jb2_stat *stat);


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_JBUF2_H__ */
