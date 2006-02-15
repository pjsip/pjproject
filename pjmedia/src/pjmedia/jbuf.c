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
#include <pjmedia/jbuf.h>
#include <pjmedia/errno.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/string.h>



struct jb_framelist
{
    char	*flist_buffer;
    int		*flist_frame_type;
    unsigned	 flist_frame_size;
    unsigned	 flist_max_count;
    unsigned	 flist_empty;
    unsigned	 flist_head;
    unsigned	 flist_tail;
    unsigned	 flist_origin;
};


typedef struct jb_framelist jb_framelist;

struct pjmedia_jbuf
{
    jb_framelist    jb_framelist;
    pj_size_t	    jb_frame_size;	  // frame size	
    pj_size_t	    jb_max_count;	  // max frames in the jitter framelist->flist_buffer

    int		    jb_level;		  // delay between source & destination
					  // (calculated according of the number of get/put operations)
    int		    jb_last_level;	  // last delay	
    int		    jb_last_jitter;	  // last jitter calculated
    int		    jb_max_hist_jitter;   // max jitter	during the last	jitter calculations
    int		    jb_stable_hist;	  // num of times the delay has	been lower then	the prefetch num
    int		    jb_last_op;		  // last operation executed on	the framelist->flist_buffer (put/get)
    int		    jb_last_seq_no;	  // seq no. of	the last frame inserted	to the framelist->flist_buffer
    int		    jb_prefetch;	  // no. of frame to insert before removing some
					  // (at the beginning of the framelist->flist_buffer operation)
    int		    jb_prefetch_cnt;	  // prefetch counter
    int		    jb_status;		  // status is 'init' until the	first 'put' operation


};


#define JB_STATUS_INITIALIZING	0
#define JB_STATUS_PROCESSING	1

#define	PJ_ABS(x)	((x > 0) ? (x) : -(x))
#define	PJ_MAX(x, y)	((x > y) ? (x) : (y))
#define	PJ_MIN(x, y)	((x < y) ? (x) : (y))



static pj_status_t jb_framelist_init( pj_pool_t *pool,
				      jb_framelist *framelist,
				      unsigned frame_size,
				      unsigned max_count) 
{
    PJ_ASSERT_RETURN(pool && framelist, PJ_EINVAL);

    pj_memset(framelist, 0, sizeof(jb_framelist));

    framelist->flist_frame_size = frame_size;
    framelist->flist_max_count = max_count;
    framelist->flist_buffer = pj_pool_zalloc(pool,
					     framelist->flist_frame_size * 
					     framelist->flist_max_count);

    framelist->flist_frame_type = pj_pool_zalloc(pool, 
						 sizeof(framelist->flist_frame_type[0]) * 
						 framelist->flist_max_count);

    framelist->flist_empty = 1;
    framelist->flist_head = framelist->flist_tail = framelist->flist_origin = 0;

    return PJ_SUCCESS;

}

static pj_status_t jb_framelist_destroy(jb_framelist *framelist) 
{
    PJ_UNUSED_ARG(framelist);
    return PJ_SUCCESS;
}


static unsigned jb_framelist_size(jb_framelist *framelist) 
{
    if (framelist->flist_tail == framelist->flist_head) {
	return framelist->flist_empty ? 0 : framelist->flist_max_count;
    } else {
	return (framelist->flist_tail - framelist->flist_head + 
		framelist->flist_max_count) % framelist->flist_max_count;
    }
}


static pj_bool_t jb_framelist_get(jb_framelist *framelist,
				  void *frame,
				  pjmedia_jb_frame_type *p_type) 
{
    if (!framelist->flist_empty) {
	pj_memcpy(frame, 
		  framelist->flist_buffer + framelist->flist_head * framelist->flist_frame_size,
		  framelist->flist_frame_size);
	*p_type = (pjmedia_jb_frame_type) framelist->flist_frame_type[framelist->flist_head];

	pj_memset(framelist->flist_buffer + framelist->flist_head * framelist->flist_frame_size,
		  0, framelist->flist_frame_size);
	framelist->flist_frame_type[framelist->flist_head] = 0;

	framelist->flist_origin++;
	framelist->flist_head = ++framelist->flist_head % framelist->flist_max_count;
	if (framelist->flist_head == framelist->flist_tail) 
	    framelist->flist_empty = PJ_TRUE;
	
	return PJ_TRUE;

    } else {
	pj_memset(frame, 0, framelist->flist_frame_size);
	return PJ_FALSE;
    }
}


static void jb_framelist_remove_head( jb_framelist *framelist,
				      unsigned count) 
{
    unsigned cur_size;

    cur_size = jb_framelist_size(framelist);
    if (count > cur_size) 
	count = cur_size;

    if (count) {
	// may be done in two steps if overlapping
	unsigned step1,step2;
	unsigned tmp = framelist->flist_head+count;

	if (tmp > framelist->flist_max_count) {
	    step1 = framelist->flist_max_count - framelist->flist_head;
	    step2 = count-step1;
	} else {
	    step1 = count;
	    step2 = 0;
	}

	pj_memset(framelist->flist_buffer + framelist->flist_head * framelist->flist_frame_size,
	          0,
	          step1*framelist->flist_frame_size);
	pj_memset(framelist->flist_frame_type+framelist->flist_head,
		  0,
		  step1*sizeof(framelist->flist_frame_type[0]));

	if (step2) {
	    pj_memset(framelist->flist_buffer,
		      0,
		      step2*framelist->flist_frame_size);
	    pj_memset(framelist->flist_frame_type,
		      0,
		      step2*sizeof(framelist->flist_frame_type[0]));
	}

	// update pointers
	framelist->flist_origin += count;
	framelist->flist_head = (framelist->flist_head+count) % framelist->flist_max_count;
	if (framelist->flist_head == framelist->flist_tail) 
	    framelist->flist_empty = PJ_TRUE;
    }
}


static pj_bool_t jb_framelist_put_at(jb_framelist *framelist,
				     unsigned index,
				     const void *frame,
				     unsigned frame_size) 
{
    unsigned where;

    // too late
    if (index < framelist->flist_origin) 
	return PJ_FALSE;

    // too soon
    if ((index > (framelist->flist_origin + framelist->flist_max_count - 1)) && !framelist->flist_empty) 
	return PJ_FALSE;

    assert(frame_size <= framelist->flist_frame_size);

    if (!framelist->flist_empty) {
	unsigned cur_size;

	where = (index - framelist->flist_origin + framelist->flist_head) % framelist->flist_max_count;

	// update framelist->flist_tail pointer
	cur_size = jb_framelist_size(framelist);
	if (index >= framelist->flist_origin + cur_size) {
	    unsigned diff = (index - (framelist->flist_origin + cur_size));
	    framelist->flist_tail = (framelist->flist_tail + diff + 1) % framelist->flist_max_count;
	}
    } else {
	where = framelist->flist_tail;
	framelist->flist_origin = index;
	framelist->flist_tail = (++framelist->flist_tail % framelist->flist_max_count);
	framelist->flist_empty = PJ_FALSE;
    }

    pj_memcpy(framelist->flist_buffer + where * framelist->flist_frame_size, 
	      frame, frame_size);

    framelist->flist_frame_type[where] = PJMEDIA_JB_NORMAL_FRAME;

    return PJ_TRUE;
}



enum pjmedia_jb_op
{
    JB_OP_INIT  = -1,
    JB_OP_PUT   = 1,
    JB_OP_GET   = 2
};


PJ_DEF(pj_status_t) pjmedia_jbuf_create(pj_pool_t *pool, 
					int frame_size, 
					int initial_prefetch, 
					int max_count,
					pjmedia_jbuf **p_jb)
{
    pjmedia_jbuf *jb;
    pj_status_t status;

    jb = pj_pool_zalloc(pool, sizeof(pjmedia_jbuf));

    status = jb_framelist_init(pool, &jb->jb_framelist, frame_size, max_count);
    if (status != PJ_SUCCESS)
	return status;

    jb->jb_frame_size	 = frame_size;
    jb->jb_last_seq_no	 = -1;
    jb->jb_level	 = 0;
    jb->jb_last_level	 = 0;
    jb->jb_last_jitter	 = 0;
    jb->jb_last_op	 = JB_OP_INIT;
    jb->jb_prefetch	 = PJ_MIN(initial_prefetch, max_count*4/5);
    jb->jb_prefetch_cnt	 = 0;
    jb->jb_stable_hist	 = 0;
    jb->jb_status	 = JB_STATUS_INITIALIZING;
    jb->jb_max_hist_jitter = 0;
    jb->jb_max_count	 = max_count;

    *p_jb = jb;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_jbuf_destroy(pjmedia_jbuf *jb)
{
    return jb_framelist_destroy(&jb->jb_framelist);
}


static void jbuf_calculate_jitter(pjmedia_jbuf *jb)
{
    enum { STABLE_HISTORY_LIMIT = (500/20) };

    jb->jb_last_jitter = PJ_ABS(jb->jb_level-jb->jb_last_level);
    jb->jb_last_level = jb->jb_level;
    jb->jb_max_hist_jitter = PJ_MAX(jb->jb_max_hist_jitter,jb->jb_last_jitter);
    
    if (jb->jb_last_jitter< jb->jb_prefetch) {
	jb->jb_stable_hist += jb->jb_last_jitter;
	if (jb->jb_stable_hist > STABLE_HISTORY_LIMIT) {
	    int seq_diff = (jb->jb_prefetch - jb->jb_max_hist_jitter)/3;
	    if (seq_diff<1) seq_diff = 1;

	    jb->jb_prefetch -= seq_diff;
	    if (jb->jb_prefetch < 1) jb->jb_prefetch = 1;

	    jb->jb_stable_hist = 0;
	    jb->jb_max_hist_jitter = 0;
	}
    } else {
	jb->jb_prefetch = PJ_MIN(jb->jb_last_jitter,(int)(jb->jb_max_count*4/5));
	jb->jb_stable_hist = 0;
	jb->jb_max_hist_jitter = 0;
    }
}

static void jbuf_update(pjmedia_jbuf *jb, int oper)
{
    if(jb->jb_last_op != oper) {
	jbuf_calculate_jitter(jb);
	jb->jb_last_op = oper;
    }
}

PJ_DEF(pj_status_t) pjmedia_jbuf_put_frame(pjmedia_jbuf *jb, 
					   const void *frame, 
					   pj_size_t frame_size, 
					   int frame_seq)
{
    pj_size_t min_frame_size;
    int seq_diff;

    if (jb->jb_last_seq_no == -1)	{
	jb->jb_last_seq_no = frame_seq - 1;
    }

    seq_diff = frame_seq - jb->jb_last_seq_no;
    jb->jb_last_seq_no = PJ_MAX(jb->jb_last_seq_no, frame_seq);
    if (seq_diff > 0) jb->jb_level += seq_diff;

    if(jb->jb_status ==	JB_STATUS_INITIALIZING) {
	jb->jb_status = JB_STATUS_PROCESSING;
	jb->jb_level = 0;
	jb->jb_last_level = 0;
	jb->jb_last_jitter = 0;
    } else {
	jbuf_update(jb, JB_OP_PUT);
    }

    min_frame_size = PJ_MIN(frame_size, jb->jb_frame_size);
    if (seq_diff > 0) {

	while (!jb_framelist_put_at(&jb->jb_framelist,frame_seq,frame,min_frame_size)) {
	    jb_framelist_remove_head(&jb->jb_framelist,PJ_MAX(jb->jb_max_count/4,1));
	}

	if (jb->jb_prefetch_cnt < jb->jb_prefetch)	
	    jb->jb_prefetch_cnt += seq_diff;

    }
    else
    {
	jb_framelist_put_at(&jb->jb_framelist,frame_seq,frame,min_frame_size);
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_jbuf_get_frame( pjmedia_jbuf *jb, 
					    void *frame, 
					    char *p_frame_type)
{
    pjmedia_jb_frame_type ftype;

    jb->jb_level--;

    jbuf_update(jb, JB_OP_GET);

    if (jb_framelist_size(&jb->jb_framelist) == 0) {
	jb->jb_prefetch_cnt = 0;
    }

    if ((jb->jb_prefetch_cnt < jb->jb_prefetch) || !jb_framelist_get(&jb->jb_framelist,frame,&ftype)) {
	pj_memset(frame, 0, jb->jb_frame_size);
	*p_frame_type = PJMEDIA_JB_ZERO_FRAME;
	return PJ_SUCCESS;
    }

    if (ftype == PJMEDIA_JB_NORMAL_FRAME) {
	*p_frame_type	= PJMEDIA_JB_NORMAL_FRAME;
    } else {
	*p_frame_type	= PJMEDIA_JB_MISSING_FRAME;
    }

    return PJ_SUCCESS;
}

PJ_DEF(unsigned) pjmedia_jbuf_get_min_delay_size(pjmedia_jbuf *jb)
{
    return jb->jb_prefetch;
}

PJ_DEF(unsigned) pjmedia_jbuf_get_delay(pjmedia_jbuf *jb)
{
    return jb_framelist_size(&jb->jb_framelist);
}


