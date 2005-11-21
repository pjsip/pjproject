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
#include <pj/log.h>
#include <pj/pool.h>
#include <string.h>	/* memset() */

/*
 * At the current state, this is basicly an ugly jitter buffer.
 * It worked before by observing level, bit it doesn't work.
 * Then I used the size, which makes the level obsolete.
 * That's why it's ugly!
 */

#define MAX_SEQ_RANGE	1000	/* Range in which sequence is considered still within session */
#define UPDATE_DURATION	20	/* Number of frames retrieved before jitter is updated */

#define THIS_FILE   "jbuf"

/* Individual frame in the frame list. */ 
struct pj_jbframe
{
    pj_uint32_t  extseq;
    void	*buf;
};


/* Jitter buffer state. */ 
typedef enum jb_state_t
{
    JB_PREFETCH,
    JB_NORMAL,
} jb_state_t;


/* Jitter buffer last operation. */ 
typedef enum jb_op_t
{
    JB_PUT,
    JB_GET,
} jb_op_t;


/* Short name for convenience. */ 
typedef struct pj_jitter_buffer JB;


/* Initialize framelist. */ 
static pj_status_t
pj_framelist_init( pj_jbframelist *lst, pj_pool_t *pool, unsigned maxcount )
{
    PJ_LOG(5, (THIS_FILE, "..pj_frame_list_init [lst=%p], maxcount=%d", lst, maxcount));

    memset(lst, 0, sizeof(*lst));
    lst->maxcount = maxcount;
    lst->frames = pj_pool_calloc( pool, maxcount, sizeof(*lst->frames) );
    if (lst->frames == NULL) {
	PJ_LOG(1,(THIS_FILE, "Unable to allocate frame list!"));
	return -1;
    }
    return 0;    
}

/* Reset framelist. */
static void 
pj_framelist_reset( pj_jbframelist *lst )
{
    PJ_LOG(6, (THIS_FILE, "..pj_frame_list_reset [lst=%p]", lst));

    lst->count = 0;
    lst->head = 0;
    lst->frames[0].extseq = 0;
}

/* Put a buffer with the specified sequence into the ordered list. */ 
static int 
pj_framelist_put( pj_jbframelist *lst, pj_uint32_t extseq, void *buf )
{
    unsigned pos = (unsigned)-1;
    pj_uint32_t startseq = lst->frames[lst->head].extseq;

    if (lst->count == 0) {
	/* Empty list. Initialize frame list. */
	PJ_LOG(6, (THIS_FILE, "    ..pj_frame_list_put [lst=%p], empty, seq=%u@pos=%d", 
		   lst, extseq, lst->head));

	lst->head = 0;
	lst->count = 1;
	lst->frames[0].buf = buf;
	lst->frames[0].extseq = extseq;
	return 0;
	
    } else if (extseq < startseq) {
	/* The sequence number is lower than our oldest packet. This can mean
	   two things:
	    - old packet has been receieved, or
	    - the sequence number has wrapped around.
	 */  
	if (startseq + lst->maxcount <= extseq) {
	    /* The sequence number has wrapped around, but it is beyond
	       the capacity of the list (i.e. too soon).
	     */
	    PJ_LOG(5, (THIS_FILE, "    ..pj_frame_list_put TOO_SOON! [lst=%p] seq=%u, startseq=%d", 
		       lst, extseq, startseq));
	    return PJ_JB_STATUS_TOO_SOON;

	} else if (startseq-extseq > lst->maxcount && startseq+lst->maxcount > extseq) {
	    /* The sequence number has wrapped around, and it is still inside
	       the 'window' of the framelist.
	     */
	    pos = extseq - startseq;
	} else {
	    /* The new frame is too old. */
	    PJ_LOG(5, (THIS_FILE, "    ..pj_frame_list_put TOO_OLD! [lst=%p] seq=%u, startseq=%d", 
		       lst, extseq, startseq));
	    return PJ_JB_STATUS_TOO_OLD;
	}
	
    } else if (extseq > startseq + lst->maxcount) {
	/* Two possibilities here. Either:
	    - packet is really too soon, or
	    - sequence number of startseq has just wrapped around, and old packet
	      which hasn't wrapped is received.
	 */
	if (extseq < MAX_SEQ_RANGE /*approx 20 seconds with 50 fps*/) {
	    PJ_LOG(5, (THIS_FILE, "    ..pj_frame_list_put TOO_SOON! [lst=%p] seq=%u, startseq=%d", 
		       lst, extseq, startseq));
	    return PJ_JB_STATUS_TOO_SOON;
	} else {
	    PJ_LOG(5, (THIS_FILE, "    ..pj_frame_list_put TOO_OLD! [lst=%p] seq=%u, startseq=%d", 
		       lst, extseq, startseq));
	    return PJ_JB_STATUS_TOO_OLD;
	}
    } 

    /* The new frame is within the framelist capacity.
       Calculate position where to put it in the list.
     */
    if (pos == (unsigned)-1)
	pos = ((extseq - startseq) + lst->head) % lst->maxcount;

    pj_assert(pos < lst->maxcount);
    
    /* Update count only if we're not overwriting existing frame. */
    if (lst->frames[pos].buf == NULL)
        ++lst->count;

    lst->frames[pos].buf = buf;
    lst->frames[pos].extseq = extseq;

    PJ_LOG(6, (THIS_FILE, "    ..pj_frame_list_put [lst=%p] seq=%u, startseq=%d, head=%d, pos=%d", 
	       lst, extseq, startseq, lst->head, pos));
    return 0;
}

/* Get the first element of the list. */ 
static int 
pj_framelist_get( pj_jbframelist *lst, pj_uint32_t *extseq, void **buf )
{
    if (lst->count == 0) {
	/* Empty. */
	*buf = NULL;
	*extseq = 0;
	PJ_LOG(6, (THIS_FILE, "    ..pj_frame_list_get [lst=%p], empty!", lst));
	return -1;
	
    } else {
	*buf = lst->frames[lst->head].buf;
	*extseq = lst->frames[lst->head].extseq;
	lst->frames[lst->head].buf = NULL;

	PJ_LOG(6, (THIS_FILE, "    ..pj_frame_list_get [lst=%p] seq=%u, head=%d", 
		   lst, *extseq, lst->head));

	lst->head = (lst->head + 1) % lst->maxcount;
	--lst->count;
	return 0;
    }
}


/*****************************************************************************
 * Reset jitter buffer. 
 ****************************************************************************
*/
PJ_DEF(void) pj_jb_reset(JB *jb)
{
    PJ_LOG(6, (THIS_FILE, "pj_jb_reset [jb=%p]", jb));

    jb->level = jb->max_level = 1;
    jb->prefetch = jb->min;
    jb->get_cnt = 0;
    jb->lastseq = 0;
    jb->state = JB_PREFETCH;
    jb->upd_count = 0;
    jb->last_op = -1;
    pj_framelist_reset( &jb->lst );
}


/*****************************************************************************
 * Create jitter buffer.
 *****************************************************************************
 */ 
PJ_DEF(pj_status_t) pj_jb_init( pj_jitter_buffer *jb, pj_pool_t *pool, 
			        unsigned min, unsigned max, unsigned maxcount)
{
    pj_status_t status;

    if (maxcount <= max) {
	maxcount = max * 5 / 4;
	PJ_LOG(3,(THIS_FILE, "Jitter buffer maximum count was adjusted."));
    }

    jb->min = min;
    jb->max = max;

    status = pj_framelist_init( &jb->lst, pool, maxcount );
    if (status != PJ_SUCCESS)
	return status;

    pj_jb_reset(jb);

    PJ_LOG(4, (THIS_FILE, "pj_jb_init success [jb=%p], min=%d, max=%d, maxcount=%d", 
			  jb, min, max, maxcount));
    return PJ_SUCCESS;
}


/*****************************************************************************
 * Put a packet to the jitter buffer.
 *****************************************************************************
 */ 
PJ_DEF(pj_status_t) pj_jb_put( JB *jb, pj_uint32_t extseq, void *buf )
{
    unsigned distance;
    int status;
    
    PJ_LOG(6, (THIS_FILE, "==> pj_jb_put [jb=%p], seq=%u, buf=%p", jb, extseq, buf));

    if (jb->lastseq == 0)
	jb->lastseq = extseq - 1;

    /* Calculate distance between this packet and last received packet
       to detect long jump (indicating probably remote has just been
       restarted.
     */
    distance = (extseq > jb->lastseq) ? extseq - jb->lastseq : jb->lastseq - extseq;
    if (distance > MAX_SEQ_RANGE) {
	/* Distance is out of range, reset jitter while maintaining current jitter
	   level.
	 */
	int old_level = jb->level;
	int old_prefetch = jb->prefetch;

	PJ_LOG(4, (THIS_FILE, "    ..[jb=%p] distance out of range, resetting", jb));

	pj_jb_reset(jb);
	jb->level = old_level;
	jb->prefetch = old_prefetch;
	distance = 1;
	jb->lastseq = extseq - 1;
    }
    
    jb->lastseq = extseq;

    status = pj_framelist_put( &jb->lst, extseq, buf );
    if (status == PJ_JB_STATUS_TOO_OLD)
	return -1;

    if (status == PJ_JB_STATUS_TOO_SOON) {
	/* TODO: discard old packets.. */
	/* No, don't do it without putting a way to inform application so that
	   it can free the memory */
    }


    if (jb->last_op != JB_PUT) {
	if (jb->state != JB_PREFETCH)
	    jb->level--;
    } else {
	jb->level++;
    }

    if (jb->lst.count > jb->max_level)
	jb->max_level++;

    jb->last_op = JB_PUT;
    return 0;
}


/*
 * Update jitter buffer algorithm.
 */
static void jb_update(JB *jb, int apply, int log_info)
{
    unsigned abs_level = jb->max_level > 0 ? jb->max_level : -jb->max_level;
    unsigned new_prefetch;

    /* Update prefetch count */
    if (abs_level > jb->prefetch)
	new_prefetch = (jb->prefetch + abs_level*9 + 1) / 10;
    else {
	new_prefetch = (jb->prefetch*4 + abs_level) / 5;
	pj_assert(new_prefetch <= jb->prefetch);
    }

    if (log_info) {
	PJ_LOG(5, (THIS_FILE, "    ..jb_update [jb=%p], level=%d, max_level=%d, old_prefetch=%d, new_prefetch=%d", 
			      jb, jb->level, jb->max_level, jb->prefetch, new_prefetch));
    } else {
	PJ_LOG(6, (THIS_FILE, "    ..jb_update [jb=%p], level=%d, max_level=%d, old_prefetch=%d, new_prefetch=%d", 
			      jb, jb->level, jb->max_level, jb->prefetch, new_prefetch));
    }

    if (new_prefetch < jb->min) new_prefetch = jb->min;
    if (new_prefetch > jb->max) new_prefetch = jb->max;

    /* If jitter buffer is empty, set state to JB_PREFETCH, taking care of the
       new prefetch setting.
     */
    if (jb->lst.count == 0) {
	jb->state = JB_PREFETCH;
	jb->get_cnt = 0;
    } else {
	/* Check if delay is too long, which in this case probably better to
	   discard some frames..
	 */
	/* No, don't do it without putting a way to inform application so that
	   it can free the memory */
    }


    if (apply) {
	jb->prefetch = new_prefetch;
	if (jb->max_level > 0)
	    jb->max_level--;
    } else {
	jb->level = new_prefetch;
    }
}


/*****************************************************************************
 * Get the oldest frame from jitter buffer.
 *****************************************************************************
 */ 
PJ_DEF(pj_status_t) pj_jb_get( JB *jb, pj_uint32_t *extseq, void **buf )
{
    pj_status_t status;
    
    PJ_LOG(6, (THIS_FILE, "<== pj_jb_get [jb=%p]", jb));

    /*
     * Check whether we're ready to give frame. When we're in JB_PREFETCH state,
     * only give frames only when:
     *	- the buffer has enough frames in it (jb->list.count > jb->prefetch), OR
     *	- after 'prefetch' attempts, there's still no frame, which in this
     *	  case PJ_JB_STATUS_FRAME_NULL will be returned by the next check.
     */
    if (jb->state == JB_PREFETCH && jb->lst.count <= jb->prefetch && jb->get_cnt < jb->prefetch) {
	jb->get_cnt++;   
	jb->last_op = JB_GET;
	PJ_LOG(5, (THIS_FILE, "    ..[jb=%p] bufferring...", jb));
	return PJ_JB_STATUS_FRAME_NULL;
    }

    /* Attempt to get one frame from the list. */
    status = pj_framelist_get( &jb->lst, extseq, buf );
    if (status != 0) {
	PJ_LOG(6, (THIS_FILE, "    ..[jb=%p] no packet!", jb));
	status = jb->lst.count ? PJ_JB_STATUS_FRAME_MISSING : PJ_JB_STATUS_FRAME_NULL;
	jb_update(jb, 1, 0);
	return status;
    }

    /* Force state to NORMAL */
    jb->state = JB_NORMAL;

    /* Increase level only when last operation is GET.
     * This is to prevent level from increasing during silence period, which
     * no packets is receieved.
     */
    if (jb->last_op != JB_GET) {
	int apply;

	//jb->level++;
	jb->last_op = JB_GET;

	apply = (++jb->upd_count > UPDATE_DURATION);
	if (apply)
	    jb->upd_count = 0;

	jb_update(jb, apply, apply);
    }

    PJ_LOG(6, (THIS_FILE, "    ..[jb=%p] seq=%u, level=%d, prefetch=%d, size=%u, delay=%d", 
			  jb, *extseq, jb->level, jb->prefetch, jb->lst.count,
			  jb->lastseq - *extseq));
    return 0;
}


