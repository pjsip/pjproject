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

#include <pjmedia/delaybuf.h>
#include <pjmedia/errno.h>
#include <pjmedia/wsola.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/pool.h>


#if 0
#   define TRACE__(x) PJ_LOG(3,x)
#else
#   define TRACE__(x)
#endif


/* Type of states of delay buffer */
enum state
{
    STATE_WAITING,
    STATE_LEARNING,
    STATE_RUNNING
};

/* Type of operation of delay buffer */
enum OP
{
    OP_PUT,
    OP_GET,
    OP_UNDEFINED
};

/* The following macros represent cycles of test. Since there are two
 * operations performed (get & put), these macros minimum value must be 2 
 * and should be even number.
 */
#define WAITING_COUNT	4
#define LEARN_COUNT	16

/* Number of buffers to add to learnt level for additional stability
 * Please note that wsola_discard() needs minimum 3 frames, so max buffer 
 * count should be minimally 3, setting SAFE_MARGIN to 2 will guarantees 
 * this.
 */
#define SAFE_MARGIN	2

/*
 * Some experimental data (with SAFE_MARGIN=1):
 *
 * System 1:
 *  - XP, WMME, 10ms ptime, 
 *  - Sennheiser Headset+USB sound card
 *  - Stable delaybuf level: 6, on WAITING_COUNT=4 and LEARNING_COUNT=48
 *
 * System 2:
 *  - XP, WMME, 10ms ptime
 *  - Onboard SoundMAX Digital Audio
 *  - Stable delaybuf level: 6, on WAITING_COUNT=4 and LEARNING_COUNT=48
 *
 * System 3:
 *  - MacBook Core 2 Duo, OSX 10.5, 10ms ptime
 *  - Stable delaybuf level: 2, on WAITING_COUNT=4 and LEARNING_COUNT=8
 */

struct pjmedia_delay_buf
{
    char	     obj_name[PJ_MAX_OBJ_NAME];

    pj_lock_t	    *lock;		/**< Lock object.		     */

    pj_int16_t	    *frame_buf;
    enum state	     state;		/**< State of delay buffer	     */
    unsigned	     samples_per_frame; /**< Number of samples in one frame  */
    unsigned	     max_frames;	/**< Buffer allocated, in frames     */

    unsigned	     put_pos;		/**< Position for put op, in samples */
    unsigned	     get_pos;		/**< Position for get op, in samples */
    unsigned	     buf_cnt;		/**< Number of buffered samples	     */
    unsigned	     max_cnt;		/**< Max number of buffered samples  */

    struct {
	unsigned     level;		/**< Burst level storage on learning */
    } op[2];
    enum OP	     last_op;		/**< Last op (GET or PUT) of learning*/
    unsigned	     state_count;	/**< Counter of op cycles of learning*/

    unsigned	     max_level;		/**< Learning result: burst level    */

    pjmedia_wsola   *wsola;		/**< Drift handler		     */
};

PJ_DEF(pj_status_t) pjmedia_delay_buf_create( pj_pool_t *pool,
					      const char *name,
					      unsigned clock_rate,
					      unsigned samples_per_frame,
					      unsigned max_frames,
					      int delay,
					      unsigned options,
					      pjmedia_delay_buf **p_b)
{
    pjmedia_delay_buf *b;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && samples_per_frame && max_frames && p_b, PJ_EINVAL);
    PJ_ASSERT_RETURN((int)max_frames >= delay, PJ_EINVAL);
    PJ_ASSERT_RETURN(options==0, PJ_EINVAL);

    PJ_UNUSED_ARG(options);

    if (!name) {
	name = "delaybuf";
    }

    b = PJ_POOL_ZALLOC_T(pool, pjmedia_delay_buf);

    pj_ansi_strncpy(b->obj_name, name, PJ_MAX_OBJ_NAME-1);
    b->samples_per_frame = samples_per_frame;
    b->max_frames = max_frames;

    status = pj_lock_create_recursive_mutex(pool, b->obj_name, 
					    &b->lock);
    if (status != PJ_SUCCESS)
	return status;

    b->frame_buf = (pj_int16_t*)
		   pj_pool_zalloc(pool, samples_per_frame * max_frames * 
				  sizeof(pj_int16_t));

    if (delay >= 0) {
	if (delay == 0)
	    delay = 1;
	b->max_level = delay;
	b->max_cnt = delay * samples_per_frame;
	b->state = STATE_RUNNING;
    } else {
	b->max_cnt = max_frames * samples_per_frame;
	b->last_op = OP_UNDEFINED;
	b->state = STATE_WAITING;
    }

    status = pjmedia_wsola_create(pool, clock_rate, samples_per_frame, 
				  PJMEDIA_WSOLA_NO_PLC, &b->wsola);
    if (status != PJ_SUCCESS)
	return status;

    *p_b = b;

    TRACE__((b->obj_name,"Delay buffer created"));

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_delay_buf_destroy(pjmedia_delay_buf *b)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(b, PJ_EINVAL);

    pj_lock_acquire(b->lock);

    status = pjmedia_wsola_destroy(b->wsola);
    if (status == PJ_SUCCESS)
	b->wsola = NULL;

    pj_lock_release(b->lock);
    pj_lock_destroy(b->lock);
    b->lock = NULL;

    return status;
}

/* This function will erase samples from delay buffer.
 * The number of erased samples is guaranteed to be >= erase_cnt.
 */
static void shrink_buffer(pjmedia_delay_buf *b, unsigned erase_cnt)
{
    unsigned buf1len;
    unsigned buf2len;
    pj_status_t status;

    pj_assert(b && erase_cnt && b->buf_cnt);

    if (b->get_pos < b->put_pos) {
	/* sssss .. sssss
	 *  ^          ^
	 *  G          P
	 */
	buf1len = b->put_pos - b->get_pos;
	buf2len = 0;
    } else {
	/* sssss .. sssss
	 *  ^ ^
	 *  P G
	 */
	buf1len = b->max_cnt - b->get_pos;
	buf2len = b->put_pos;
    }

    /* Consistency checking */
    pj_assert((buf1len + buf2len) == b->buf_cnt);

    if (buf1len != 0)
	status = pjmedia_wsola_discard(b->wsola, 
				       &b->frame_buf[b->get_pos], buf1len,
				       b->frame_buf, buf2len,
				       &erase_cnt);
    else
	status = pjmedia_wsola_discard(b->wsola, 
				       b->frame_buf, buf2len,
				       NULL, 0,
				       &erase_cnt);

    if ((status == PJ_SUCCESS) && (erase_cnt > 0)) {
	/* WSOLA discard will shrink only the second buffer, but it may
	 * also shrink first buffer if second buffer is 'destroyed', so
	 * it is safe to just set the new put_pos.
	 */
	if (b->put_pos >= erase_cnt)
	    b->put_pos -= erase_cnt;
	else
	    b->put_pos = b->max_cnt - (erase_cnt - b->put_pos);

	b->buf_cnt -= erase_cnt;

	PJ_LOG(5,(b->obj_name,"Successfully shrinking %d samples, "
		  "buf_cnt=%d", erase_cnt, b->buf_cnt));
    }

    /* Shrinking failed or erased count is less than requested,
     * delaybuf needs to drop eldest samples, this is bad since the voice
     * samples may not have smooth transition.
     */
    if (b->buf_cnt + b->samples_per_frame > b->max_cnt) {
	erase_cnt = b->buf_cnt + b->samples_per_frame - b->max_cnt;

	b->buf_cnt -= erase_cnt;

	/* Shift get_pos forward */
	b->get_pos = (b->get_pos + erase_cnt) % b->max_cnt;

	PJ_LOG(4,(b->obj_name,"Shrinking failed or insufficient, dropping"
		  " %d eldest samples, buf_cnt=%d", erase_cnt, b->buf_cnt));
    }
}

static void set_max_cnt(pjmedia_delay_buf *b, unsigned new_max_cnt)
{
    unsigned old_max_cnt = b->max_cnt;

    /* nothing to adjust */
    if (old_max_cnt == new_max_cnt)
	return;

    /* For now, only support shrinking */
    pj_assert(old_max_cnt > new_max_cnt);

    /* Buffer empty, only need to reset pointers then set new max directly */
    if (b->buf_cnt == 0) {
	b->put_pos = b->get_pos = 0;
	b->max_cnt = new_max_cnt;
	return;
    }

    shrink_buffer(b, old_max_cnt - new_max_cnt);

    /* Adjust buffer to accomodate the new max_cnt so the samples is secured.
     * This is done by make get_pos = 0 
     */
    if (b->get_pos <= b->put_pos) {
	/* sssss .. sssss
	 *   ^         ^
	 *   G         P
	 */
	/* Consistency checking */
	pj_assert((b->put_pos - b->get_pos) <= new_max_cnt);
	pj_assert((b->put_pos - b->get_pos) == b->buf_cnt);

	pjmedia_move_samples(b->frame_buf, &b->frame_buf[b->get_pos], 
			     b->get_pos);
	b->put_pos -= b->get_pos;
	b->get_pos = 0;
    } else {
	/* sssss .. sssss
	 *          ^  ^
	 *          P  G
	 */
	unsigned d = old_max_cnt - b->get_pos;

	/* Consistency checking */
	pj_assert((b->get_pos - b->put_pos) >= (old_max_cnt - new_max_cnt));

	/* Make get_pos = 0, shift right the leftmost block first */
	pjmedia_move_samples(&b->frame_buf[d], b->frame_buf, d);
	pjmedia_copy_samples(b->frame_buf, &b->frame_buf[b->get_pos], d);
	b->put_pos += d;
	b->get_pos = 0;
    }

    b->max_cnt = new_max_cnt;
}

static void update(pjmedia_delay_buf *b, enum OP op)
{
    enum OP other = (enum OP) !op;

    switch (b->state) {
    case STATE_RUNNING:
	break;
    case STATE_WAITING:
	++b->op[op].level;
	if (b->op[other].level != 0) {
	    ++b->state_count;
	    if (b->state_count == WAITING_COUNT) {
		/* Start learning */
		pjmedia_delay_buf_learn(b);
	    }
	}
	b->last_op = op;
	break;
    case STATE_LEARNING:
	++b->op[op].level;
	if (b->last_op == other) {
	    unsigned last_level = b->op[other].level;
	    if (last_level > b->max_level)
		b->max_level = last_level;
	    b->op[other].level = 0;
	    b->state_count++;
	    if (b->state_count == LEARN_COUNT) {
		/* give SAFE_MARGIN compensation for added stability */
		b->max_level += SAFE_MARGIN;
		
		/* buffer not enough! */
		if (b->max_level > b->max_frames) {
		    PJ_LOG(4,(b->obj_name,"Delay buffer learning result (%d)"
			      " exceeds the maximum delay allowed (%d)",
			      b->max_level,
			      b->max_frames));

		    b->max_level = b->max_frames;
		}

		/* we need to set new max_cnt & adjust buffer */
		set_max_cnt(b, b->max_level * b->samples_per_frame);

		b->state = STATE_RUNNING;

		PJ_LOG(4,(b->obj_name,"Delay buffer start running, level=%u",
			  b->max_level));
	    }
	}
	b->last_op = op;
	break;
    }

    
}

PJ_DEF(pj_status_t) pjmedia_delay_buf_put(pjmedia_delay_buf *b,
					   pj_int16_t frame[])
{
    pj_status_t status;

    PJ_ASSERT_RETURN(b && frame, PJ_EINVAL);

    pj_lock_acquire(b->lock);

    update(b, OP_PUT);
    
    status = pjmedia_wsola_save(b->wsola, frame, PJ_FALSE);
    if (status != PJ_SUCCESS) {
	pj_lock_release(b->lock);
	return status;
    }

    /* Overflow checking */
    if (b->buf_cnt + b->samples_per_frame > b->max_cnt)
    {
	/* shrink one frame or just the diff? */
	//unsigned erase_cnt = b->samples_per_frame;
	unsigned erase_cnt = b->buf_cnt + b->samples_per_frame - b->max_cnt;

	shrink_buffer(b, erase_cnt);
    }

    /* put the frame on put_pos */
    if (b->put_pos + b->samples_per_frame <= b->max_cnt) {
	pjmedia_copy_samples(&b->frame_buf[b->put_pos], frame, 
			     b->samples_per_frame);
    } else {
	int remainder = b->put_pos + b->samples_per_frame - b->max_cnt;

	pjmedia_copy_samples(&b->frame_buf[b->put_pos], frame, 
			     b->samples_per_frame - remainder);
	pjmedia_copy_samples(&b->frame_buf[0], 
			     &frame[b->samples_per_frame - remainder], 
			     remainder);
    }

    /* Update put_pos & buf_cnt */
    b->put_pos = (b->put_pos + b->samples_per_frame) % b->max_cnt;
    b->buf_cnt += b->samples_per_frame;

    pj_lock_release(b->lock);
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_delay_buf_get( pjmedia_delay_buf *b,
					   pj_int16_t frame[])
{
    pj_status_t status;

    PJ_ASSERT_RETURN(b && frame, PJ_EINVAL);

    pj_lock_acquire(b->lock);

    update(b, OP_GET);

    /* Starvation checking */
    if (b->buf_cnt < b->samples_per_frame) {

	PJ_LOG(5,(b->obj_name,"Underflow, buf_cnt=%d, will generate 1 frame",
		  b->buf_cnt));

	status = pjmedia_wsola_generate(b->wsola, frame);

	if (status == PJ_SUCCESS) {
	    TRACE__((b->obj_name,"Successfully generate 1 frame"));
	    if (b->buf_cnt == 0) {
		pj_lock_release(b->lock);
		return PJ_SUCCESS;
	    }

	    /* Put generated frame into buffer */
	    if (b->put_pos + b->samples_per_frame <= b->max_cnt) {
		pjmedia_copy_samples(&b->frame_buf[b->put_pos], frame, 
				     b->samples_per_frame);
	    } else {
		int remainder = b->put_pos + b->samples_per_frame - b->max_cnt;

		pjmedia_copy_samples(&b->frame_buf[b->put_pos], &frame[0], 
				     b->samples_per_frame - remainder);
		pjmedia_copy_samples(&b->frame_buf[0], 
				     &frame[b->samples_per_frame - remainder], 
				     remainder);
	    }
    	
	    b->put_pos = (b->put_pos + b->samples_per_frame) % b->max_cnt;
	    b->buf_cnt += b->samples_per_frame;

	} else {
	    /* Give all what delay buffer has, then pad zeroes */
	    PJ_LOG(4,(b->obj_name,"Error generating frame, status=%d", 
		      status));

	    pjmedia_copy_samples(frame, &b->frame_buf[b->get_pos], b->buf_cnt);
	    pjmedia_zero_samples(&frame[b->buf_cnt], 
				 b->samples_per_frame - b->buf_cnt);
	    b->get_pos += b->buf_cnt;
	    b->buf_cnt = 0;

	    /* Consistency checking */
	    pj_assert(b->get_pos == b->put_pos);

	    pj_lock_release(b->lock);

	    return PJ_SUCCESS;
	}
    }

    pjmedia_copy_samples(frame, &b->frame_buf[b->get_pos],
			 b->samples_per_frame);

    b->get_pos = (b->get_pos + b->samples_per_frame) % b->max_cnt;
    b->buf_cnt -= b->samples_per_frame;

    pj_lock_release(b->lock);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_delay_buf_learn(pjmedia_delay_buf *b)
{
    PJ_ASSERT_RETURN(b, PJ_EINVAL);

    pj_lock_acquire(b->lock);

    b->last_op = OP_UNDEFINED;
    b->op[OP_GET].level = b->op[OP_PUT].level = 0;
    b->state = STATE_LEARNING;
    b->state_count = 0;
    b->max_level = 0;
    b->max_cnt = b->max_frames * b->samples_per_frame;

    pjmedia_delay_buf_reset(b);

    pj_lock_release(b->lock);

    PJ_LOG(5,(b->obj_name,"Delay buffer start learning"));

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_delay_buf_reset(pjmedia_delay_buf *b)
{
    PJ_ASSERT_RETURN(b, PJ_EINVAL);

    pj_lock_acquire(b->lock);

    /* clean up buffer */
    b->buf_cnt = 0;
    b->put_pos = b->get_pos = 0;
    pjmedia_wsola_reset(b->wsola, 0);

    pj_lock_release(b->lock);

    PJ_LOG(5,(b->obj_name,"Delay buffer resetted"));

    return PJ_SUCCESS;
}

