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
#include <pj/math.h>
#include <pj/pool.h>


#if 0
#   define TRACE__(x) PJ_LOG(3,x)
#else
#   define TRACE__(x)
#endif

/* Operation types of delay buffer */
enum OP
{
    OP_PUT,
    OP_GET
};

/* Specify time for delaybuf to recalculate effective delay, in ms.
 */
#define RECALC_TIME	    2000

/* Default value of maximum delay, in ms, this value is used when 
 * maximum delay requested is less than ptime (one frame length).
 */
#define DEFAULT_MAX_DELAY   400

/* Number of frames to add to learnt level for additional stability.
 */
#define SAFE_MARGIN	    0

/* This structure describes internal delaybuf settings and states.
 */
struct pjmedia_delay_buf
{
    /* Properties and configuration */
    char	     obj_name[PJ_MAX_OBJ_NAME];
    pj_lock_t	    *lock;		/**< Lock object.		     */
    pj_int16_t	    *frame_buf;
    unsigned	     samples_per_frame; /**< Number of samples in one frame  */
    unsigned	     ptime;		/**< Frame time, in ms		     */
    unsigned	     channel_count;	/**< Channel count, in ms	     */
    unsigned	     max_cnt;		/**< Max buffered samples, in samples*/

    /* Buffer pointer and counter */
    unsigned	     put_pos;		/**< Position for put op, in samples */
    unsigned	     get_pos;		/**< Position for get op, in samples */
    unsigned	     buf_cnt;		/**< Number of buffered samples	     */
    unsigned	     eff_cnt;		/**< Effective count of buffered 
					     samples to keep the optimum
					     balance between delay and 
					     stability. This is calculated 
					     based on burst level.	     */

    /* Learning vars */
    unsigned	     level;		/**< Burst level counter	     */
    enum OP	     last_op;		/**< Last op (GET or PUT) of learning*/
    int		     recalc_timer;	/**< Timer for recalculating max_level*/
    unsigned	     max_level;		/**< Current max burst level	     */

    /* Drift handler */
    pjmedia_wsola   *wsola;		/**< Drift handler		     */
};


PJ_DEF(pj_status_t) pjmedia_delay_buf_create( pj_pool_t *pool,
					      const char *name,
					      unsigned clock_rate,
					      unsigned samples_per_frame,
					      unsigned channel_count,
					      unsigned max_delay,
					      unsigned options,
					      pjmedia_delay_buf **p_b)
{
    pjmedia_delay_buf *b;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && samples_per_frame && clock_rate && channel_count &&
		     p_b, PJ_EINVAL);
    PJ_ASSERT_RETURN(options==0, PJ_EINVAL);

    PJ_UNUSED_ARG(options);

    if (!name) {
	name = "delaybuf";
    }

    b = PJ_POOL_ZALLOC_T(pool, pjmedia_delay_buf);

    pj_ansi_strncpy(b->obj_name, name, PJ_MAX_OBJ_NAME-1);

    b->samples_per_frame = samples_per_frame;
    b->channel_count = channel_count;
    b->ptime = samples_per_frame * 1000 / clock_rate / channel_count;
    if (max_delay < b->ptime)
	max_delay = PJ_MAX(DEFAULT_MAX_DELAY, b->ptime);

    b->max_cnt = samples_per_frame * max_delay / b->ptime;
    b->eff_cnt = b->max_cnt >> 1;
    b->recalc_timer = RECALC_TIME;

    status = pj_lock_create_recursive_mutex(pool, b->obj_name, 
					    &b->lock);
    if (status != PJ_SUCCESS)
	return status;

    b->frame_buf = (pj_int16_t*)
		    pj_pool_zalloc(pool, b->max_cnt * sizeof(pj_int16_t));

    status = pjmedia_wsola_create(pool, clock_rate, samples_per_frame, 1,
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

	PJ_LOG(5,(b->obj_name,"%d samples reduced, buf_cnt=%d", 
	       erase_cnt, b->buf_cnt));
    }
}

/* Fast increase, slow decrease */
#define AGC_UP(cur, target) cur = (cur + target*3) >> 2
#define AGC_DOWN(cur, target) cur = (cur*3 + target) >> 2
#define AGC(cur, target) \
    if (cur < target) AGC_UP(cur, target); \
    else AGC_DOWN(cur, target)

static void update(pjmedia_delay_buf *b, enum OP op)
{
    /* Sequential operation */
    if (op == b->last_op) {
	++b->level;
	return;
    } 

    /* Switching operation */
    if (b->level > b->max_level)
	b->max_level = b->level;

    b->recalc_timer -= (b->level * b->ptime) >> 1;

    b->last_op = op;
    b->level = 1;

    /* Recalculate effective count based on max_level */
    if (b->recalc_timer <= 0) {
	unsigned new_eff_cnt = (b->max_level+SAFE_MARGIN)*b->samples_per_frame;

	/* Smoothening effective count transition */
	AGC(b->eff_cnt, new_eff_cnt);
	
	/* Make sure the new effective count is multiplication of 
	 * channel_count, so let's round it up.
	 */
	if (b->eff_cnt % b->channel_count)
	    b->eff_cnt += b->channel_count - (b->eff_cnt % b->channel_count);

	TRACE__((b->obj_name,"Cur eff_cnt=%d", b->eff_cnt));
	
	b->max_level = 0;
	b->recalc_timer = RECALC_TIME;
    }

    /* See if we need to shrink the buffer to reduce delay */
    if (b->buf_cnt > b->samples_per_frame + b->eff_cnt) {
	unsigned erase_cnt = b->samples_per_frame >> 1;
	unsigned old_buf_cnt = b->buf_cnt;

	shrink_buffer(b, erase_cnt);
	PJ_LOG(4,(b->obj_name,"Buffer size adjusted from %d to %d (eff_cnt=%d)",
	          old_buf_cnt, b->buf_cnt, b->eff_cnt));
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
	unsigned erase_cnt;
	
	/* shrink one frame or just the diff? */
	//erase_cnt = b->samples_per_frame;
	erase_cnt = b->buf_cnt + b->samples_per_frame - b->max_cnt;

	shrink_buffer(b, erase_cnt);

	/* Check if shrinking failed or erased count is less than requested,
	 * delaybuf needs to drop eldest samples, this is bad since the voice
	 * samples get rough transition which may produce tick noise.
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

	PJ_LOG(4,(b->obj_name,"Underflow, buf_cnt=%d, will generate 1 frame",
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
	    /* Give all what delay buffer has, then pad with zeroes */
	    PJ_LOG(4,(b->obj_name,"Error generating frame, status=%d", 
		      status));

	    if (b->get_pos + b->buf_cnt <= b->max_cnt) {
		pjmedia_copy_samples(frame, &b->frame_buf[b->get_pos], b->buf_cnt);
	    } else {
		int remainder = b->get_pos + b->buf_cnt - b->max_cnt;

		pjmedia_copy_samples(frame, &b->frame_buf[b->get_pos],
				     b->buf_cnt - remainder);
		pjmedia_copy_samples(&frame[b->buf_cnt - remainder],
				     &b->frame_buf[0], remainder);
	    }

	    pjmedia_zero_samples(&frame[b->buf_cnt], 
				 b->samples_per_frame - b->buf_cnt);

	    /* Delay buf is empty */
	    b->get_pos = b->put_pos = 0;
	    b->buf_cnt = 0;

	    pj_lock_release(b->lock);

	    return PJ_SUCCESS;
	}
    }

    if (b->get_pos + b->samples_per_frame <= b->max_cnt) {
	pjmedia_copy_samples(frame, &b->frame_buf[b->get_pos], 
			     b->samples_per_frame);
    } else {
	int remainder = b->get_pos + b->samples_per_frame - b->max_cnt;

	pjmedia_copy_samples(frame, &b->frame_buf[b->get_pos],
			     b->samples_per_frame - remainder);
	pjmedia_copy_samples(&frame[b->samples_per_frame - remainder],
			     &b->frame_buf[0], 
			     remainder);
    }

    b->get_pos = (b->get_pos + b->samples_per_frame) % b->max_cnt;
    b->buf_cnt -= b->samples_per_frame;

    pj_lock_release(b->lock);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_delay_buf_reset(pjmedia_delay_buf *b)
{
    PJ_ASSERT_RETURN(b, PJ_EINVAL);

    pj_lock_acquire(b->lock);

    b->recalc_timer = RECALC_TIME;

    /* clean up buffer */
    b->buf_cnt = 0;
    b->put_pos = b->get_pos = 0;
    pjmedia_wsola_reset(b->wsola, 0);

    pj_lock_release(b->lock);

    PJ_LOG(5,(b->obj_name,"Delay buffer is reset"));

    return PJ_SUCCESS;
}

