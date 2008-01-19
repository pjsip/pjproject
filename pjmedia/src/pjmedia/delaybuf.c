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
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>

enum state
{
    STATE_WAITING,
    STATE_LEARNING,
    STATE_RUNNING
};

enum OP
{
    OP_PUT,
    OP_GET,
    OP_UNDEFINED
};

/* The following macros represent cycles of test. */
/* Since there are two operations performed (get & put), */
/* these macros value must be minimum 2 and should be even number */
#define WAITING_COUNT	4
#define LEARN_COUNT	16

/* Number of buffers to add to learnt level for additional stability */
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
    char	 obj_name[PJ_MAX_OBJ_NAME];

    pj_int16_t	*frame_buf;
    unsigned	 put_pos;
    unsigned	 get_pos;
    unsigned	 buf_cnt;

    unsigned	 samples_per_frame;
    unsigned	 max_cnt;
    enum state	 state;

    struct {
	unsigned level;
    } op[2];

    enum OP	 last_op;
    unsigned	 max_level;
    unsigned	 state_count;
};

PJ_DEF(pj_status_t) pjmedia_delay_buf_create( pj_pool_t *pool,
					      const char *name,
					      unsigned samples_per_frame,
					      unsigned max_cnt,
					      int delay,
					      pjmedia_delay_buf **p_b)
{
    pjmedia_delay_buf *b;

    PJ_ASSERT_RETURN(pool && samples_per_frame && max_cnt && p_b, PJ_EINVAL);

    if (!name) {
	name = "delaybuf";
    }

    b = PJ_POOL_ZALLOC_T(pool, pjmedia_delay_buf);

    pj_ansi_strncpy(b->obj_name, name, PJ_MAX_OBJ_NAME-1);
    b->frame_buf = (pj_int16_t*)
		   pj_pool_zalloc(pool, samples_per_frame * max_cnt * 
					 sizeof(pj_int16_t));
    b->samples_per_frame = samples_per_frame;
    b->max_cnt = max_cnt;

    if (delay >= 0) {
	PJ_ASSERT_RETURN(delay <= (int)max_cnt, PJ_EINVAL);
	b->max_level = delay;
	b->state = STATE_RUNNING;
    } else {
	b->last_op = OP_UNDEFINED;
	b->state = STATE_WAITING;
	b->buf_cnt = 0;
    }

    *p_b = b;

    PJ_LOG(5,(b->obj_name,"Delay buffer created"));

    return PJ_SUCCESS;
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
		
		PJ_LOG(5,(b->obj_name,"Delay buffer start running, level=%u",
		    b->max_level));

		/* buffer not enough! */
		if (b->max_level > b->max_cnt) {
		    b->max_level = b->max_cnt;
		    PJ_LOG(2,(b->obj_name,"Delay buffer %s learning result " \
			"exceeds the maximum delay allowed",
			b->max_level));
		}

		b->state = STATE_RUNNING;
	    }
	}
	b->last_op = op;
	break;
    }

    
}

PJ_DEF(pj_status_t) pjmedia_delay_buf_put(pjmedia_delay_buf *b,
					   pj_int16_t frame[])
{
    update(b, OP_PUT);
    
    if (b->state != STATE_RUNNING)
	return PJ_EPENDING;

    pj_memcpy(&b->frame_buf[b->put_pos * b->samples_per_frame], frame, 
	b->samples_per_frame*sizeof(pj_int16_t));

    /* overflow case */
    if (b->put_pos == b->get_pos && b->buf_cnt) {
	if (++b->get_pos == b->max_level)
	    b->get_pos = 0;

	b->put_pos = b->get_pos;

	PJ_LOG(5,(b->obj_name,"Warning: buffer overflow"));

	return PJ_ETOOMANY;
    }

    ++b->buf_cnt;

    if (++b->put_pos == b->max_level)
	b->put_pos = 0;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_delay_buf_get(pjmedia_delay_buf *b,
					   pj_int16_t frame[])
{
    update(b, OP_GET);

    if (b->state != STATE_RUNNING || !b->buf_cnt) {
	if (b->state == STATE_RUNNING)
	    PJ_LOG(5,(b->obj_name,"Warning: delay buffer empty"));

	pj_bzero(frame, b->samples_per_frame*sizeof(pj_int16_t));
	return PJ_EPENDING;
    }

    pj_memcpy(frame, &b->frame_buf[b->get_pos * b->samples_per_frame],
	b->samples_per_frame*sizeof(pj_int16_t));

    if (++b->get_pos == b->max_level)
	b->get_pos = 0;

    --b->buf_cnt;

    return PJ_SUCCESS;
}

PJ_DECL(pj_status_t) pjmedia_delay_buf_learn(pjmedia_delay_buf *b)
{
    PJ_ASSERT_RETURN(b, PJ_EINVAL);

    b->last_op = OP_UNDEFINED;
    b->op[OP_GET].level = b->op[OP_PUT].level = 0;
    b->state = STATE_LEARNING;
    b->state_count = 0;
    b->max_level = 0;

    /* clean up buffer */
    b->buf_cnt = 0;
    b->put_pos = b->get_pos = 0;

    PJ_LOG(5,(b->obj_name,"Delay buffer start learning"));

    return PJ_SUCCESS;
}
