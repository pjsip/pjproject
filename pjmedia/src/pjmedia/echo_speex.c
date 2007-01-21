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

#include <pjmedia/echo.h>
#include <pjmedia/errno.h>
#include <pjmedia/silencedet.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>


#define THIS_FILE	"echo_speex.c"
#define BUF_COUNT	16
#define MIN_PREFETCH	2
#define MAX_PREFETCH	(BUF_COUNT*2/3)



#if 0
# define TRACE_(expr)  PJ_LOG(5,expr)
#else
# define TRACE_(expr)
#endif


typedef struct pjmedia_frame_queue pjmedia_frame_queue;

struct fq_frame
{
    PJ_DECL_LIST_MEMBER(struct fq_frame);
    void	*buf;
    unsigned	 size;
    pj_uint32_t	 seq;
};

struct pjmedia_frame_queue
{
    char	     obj_name[PJ_MAX_OBJ_NAME];
    unsigned	     frame_size;
    int		     samples_per_frame;
    unsigned	     count;
    unsigned	     max_count;
    struct fq_frame  frame_list;
    struct fq_frame  free_list;

    int		     seq_delay;
    int		     prefetch_count;
};

PJ_DEF(pj_status_t) pjmedia_frame_queue_create( pj_pool_t *pool,
					        const char *name,
					        unsigned frame_size,
					        unsigned samples_per_frame,
					        unsigned max_count,
					        pjmedia_frame_queue **p_fq)
{
    pjmedia_frame_queue *fq;
    unsigned i;

    fq = pj_pool_zalloc(pool, sizeof(pjmedia_frame_queue));

    pj_ansi_snprintf(fq->obj_name, sizeof(fq->obj_name), name, fq);
    fq->obj_name[sizeof(fq->obj_name)-1] = '\0';

    fq->max_count = max_count;
    fq->frame_size = frame_size;
    fq->samples_per_frame = samples_per_frame;
    fq->count = 0;

    pj_list_init(&fq->frame_list);
    pj_list_init(&fq->free_list);

    for (i=0; i<max_count; ++i) {
	struct fq_frame *f;

	f = pj_pool_zalloc(pool, sizeof(struct fq_frame));
	f->buf = pj_pool_alloc(pool, frame_size);

	pj_list_push_back(&fq->free_list, f);
	
    }

    *p_fq = fq;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_frame_queue_init( pjmedia_frame_queue *fq,
					      int seq_delay,
					      int prefetch_count)
{
    if (prefetch_count > MAX_PREFETCH)
	prefetch_count = MAX_PREFETCH;

    fq->seq_delay = seq_delay;
    fq->prefetch_count = prefetch_count;
    fq->count = 0;
    pj_list_merge_first(&fq->free_list, &fq->frame_list);

    PJ_LOG(5,(fq->obj_name, "AEC reset, delay=%d, prefetch=%d", 
	      fq->seq_delay, fq->prefetch_count));

    return PJ_SUCCESS;
}

PJ_DEF(pj_bool_t) pjmedia_frame_queue_empty( pjmedia_frame_queue *fq )
{
    return pj_list_empty(&fq->frame_list);
}

PJ_DEF(int) pjmedia_frame_queue_get_prefetch( pjmedia_frame_queue *fq )
{
    return fq->prefetch_count;
}

PJ_DEF(pj_status_t) pjmedia_frame_queue_put( pjmedia_frame_queue *fq,
					     const void *framebuf,
					     unsigned size,
					     pj_uint32_t timestamp )
{
    struct fq_frame *f;

    TRACE_((fq->obj_name, "PUT seq=%d, count=%d", 
	    timestamp / fq->samples_per_frame, fq->count));

    if (pj_list_empty(&fq->free_list)) {
	PJ_LOG(5,(fq->obj_name, 
		  " AEC info: queue is full, frame discarded "
		  "[count=%d, seq=%d]",
		  fq->max_count, timestamp / fq->samples_per_frame));
	//pjmedia_frame_queue_init(fq, fq->seq_delay, fq->prefetch_count);
	return PJ_ETOOMANY;
    }

    PJ_ASSERT_RETURN(size <= fq->frame_size, PJ_ETOOBIG);

    f = fq->free_list.next;
    pj_list_erase(f);

    pj_memcpy(f->buf, framebuf, size);
    f->size = size;
    f->seq = timestamp / fq->samples_per_frame;

    pj_list_push_back(&fq->frame_list, f);
    ++fq->count;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_frame_queue_get( pjmedia_frame_queue *fq,
					     pj_uint32_t get_timestamp,
					     void **framebuf,
					     unsigned *size )
{
    pj_uint32_t frame_seq;
    struct fq_frame *f;

    frame_seq = get_timestamp/fq->samples_per_frame + fq->seq_delay -
		fq->prefetch_count;

    TRACE_((fq->obj_name, "GET seq=%d for seq=%d delay=%d, prefetch=%d", 
	    get_timestamp/fq->samples_per_frame, frame_seq, fq->seq_delay, 
	    fq->prefetch_count));

    *size = 0;

    /* Remove old frames */
    for (;!pj_list_empty(&fq->frame_list);) {
	f = fq->frame_list.next;
	if (f->seq >= frame_seq)
	    break;

	PJ_LOG(5,(fq->obj_name, 
		  " AEC Info: old frame removed (seq=%d, want=%d, count=%d)",
		  f->seq, frame_seq, fq->count));
	pj_list_erase(f);
	--fq->count;
	pj_list_push_back(&fq->free_list, f);
    }

    if (pj_list_empty(&fq->frame_list)) {
	PJ_LOG(5,(fq->obj_name, 
		  " AEC Info: empty queue for seq=%d!",
		  frame_seq));
	return PJ_ENOTFOUND;
    }

    f = fq->frame_list.next;

    if (f->seq > frame_seq) {
	PJ_LOG(5,(fq->obj_name, 
		  " AEC Info: prefetching (first seq=%d)",
		  f->seq));
	return -1;
    }

    pj_list_erase(f);
    --fq->count;

    *framebuf = (void*)f->buf;
    *size = f->size;

    TRACE_((fq->obj_name, " returning frame with seq=%d, count=%d", 
	    f->seq, fq->count));

    pj_list_push_front(&fq->free_list, f);
    return PJ_SUCCESS;
}

/*
 * Prototypes
 */
PJ_DECL(pj_status_t) speex_aec_create(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned samples_per_frame,
				      unsigned tail_ms,
				      unsigned latency_ms,
				      unsigned options,
				      void **p_state );
PJ_DECL(pj_status_t) speex_aec_destroy(void *state );
PJ_DECL(pj_status_t) speex_aec_playback(void *state,
				        pj_int16_t *play_frm );
PJ_DECL(pj_status_t) speex_aec_capture(void *state,
				       pj_int16_t *rec_frm,
				       unsigned options );
PJ_DECL(pj_status_t) speex_aec_cancel_echo(void *state,
					   pj_int16_t *rec_frm,
					   const pj_int16_t *play_frm,
					   unsigned options,
					   void *reserved );


enum
{
    TS_FLAG_PLAY = 1,
    TS_FLAG_REC	 = 2,
    TS_FLAG_OK	 = 3,
};

typedef struct speex_ec
{
    SpeexEchoState	 *state;
    SpeexPreprocessState *preprocess;

    unsigned		  samples_per_frame;
    unsigned		  prefetch;
    unsigned		  options;
    pj_int16_t		 *tmp_frame;
    spx_int32_t		 *residue;

    pj_uint32_t		  play_ts,
			  rec_ts,
			  ts_flag;

    pjmedia_frame_queue	 *frame_queue;
    pj_lock_t		 *lock;		/* To protect buffers, if required  */
} speex_ec;



/*
 * Create the AEC. 
 */
PJ_DEF(pj_status_t) speex_aec_create(pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned samples_per_frame,
				     unsigned tail_ms,
				     unsigned latency_ms,
				     unsigned options,
				     void **p_echo )
{
    speex_ec *echo;
    int sampling_rate;
    pj_status_t status;

    *p_echo = NULL;

    echo = pj_pool_zalloc(pool, sizeof(speex_ec));
    PJ_ASSERT_RETURN(echo != NULL, PJ_ENOMEM);

    if (options & PJMEDIA_ECHO_NO_LOCK) {
	status = pj_lock_create_null_mutex(pool, "aec%p", &echo->lock);
	if (status != PJ_SUCCESS)
	    return status;
    } else {
	status = pj_lock_create_simple_mutex(pool, "aec%p", &echo->lock);
	if (status != PJ_SUCCESS)
	    return status;
    }

    echo->samples_per_frame = samples_per_frame;
    echo->prefetch = (latency_ms * clock_rate / 1000) / samples_per_frame;
    if (echo->prefetch < MIN_PREFETCH)
	echo->prefetch = MIN_PREFETCH;
    if (echo->prefetch > MAX_PREFETCH)
	echo->prefetch = MAX_PREFETCH;
    echo->options = options;

    echo->state = speex_echo_state_init(samples_per_frame,
					clock_rate * tail_ms / 1000);
    if (echo->state == NULL) {
	pj_lock_destroy(echo->lock);
	return PJ_ENOMEM;
    }

    /* Set sampling rate */
    sampling_rate = clock_rate;
    speex_echo_ctl(echo->state, SPEEX_ECHO_SET_SAMPLING_RATE, 
		   &sampling_rate);

    echo->preprocess = speex_preprocess_state_init(samples_per_frame, 
						   clock_rate);
    if (echo->preprocess == NULL) {
	speex_echo_state_destroy(echo->state);
	pj_lock_destroy(echo->lock);
	return PJ_ENOMEM;
    }

    /* Disable all preprocessing, we only want echo cancellation */
#if 0
    disabled = 0;
    enabled = 1;
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_DENOISE, 
			 &enabled);
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_AGC, 
			 &disabled);
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_VAD, 
			 &disabled);
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_DEREVERB, 
			 &disabled);
#endif

    /* Control echo cancellation in the preprocessor */
   speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_ECHO_STATE, 
			echo->state);


    /* Create temporary frame for echo cancellation */
    echo->tmp_frame = pj_pool_zalloc(pool, 2 * samples_per_frame);
    PJ_ASSERT_RETURN(echo->tmp_frame != NULL, PJ_ENOMEM);

    /* Create temporary frame to receive residue */
    echo->residue = pj_pool_zalloc(pool, sizeof(spx_int32_t) * 
					    (samples_per_frame+1));
    PJ_ASSERT_RETURN(echo->residue != NULL, PJ_ENOMEM);

    /* Create frame queue */
    status = pjmedia_frame_queue_create(pool, "aec%p", samples_per_frame*2,
					samples_per_frame, BUF_COUNT, 
					&echo->frame_queue);
    if (status != PJ_SUCCESS) {
	speex_preprocess_state_destroy(echo->preprocess);
	speex_echo_state_destroy(echo->state);
	pj_lock_destroy(echo->lock);
	return status;
    }

    /* Done */
    *p_echo = echo;

    PJ_LOG(4,(THIS_FILE, "Speex Echo canceller/AEC created, clock_rate=%d, "
			 "samples per frame=%d, tail length=%d ms, "
			 "latency=%d ms", 
			 clock_rate, samples_per_frame, tail_ms, latency_ms));
    return PJ_SUCCESS;

}


/*
 * Destroy AEC
 */
PJ_DEF(pj_status_t) speex_aec_destroy(void *state )
{
    speex_ec *echo = state;

    PJ_ASSERT_RETURN(echo && echo->state, PJ_EINVAL);

    if (echo->lock)
	pj_lock_acquire(echo->lock);

    if (echo->state) {
	speex_echo_state_destroy(echo->state);
	echo->state = NULL;
    }

    if (echo->preprocess) {
	speex_preprocess_state_destroy(echo->preprocess);
	echo->preprocess = NULL;
    }

    if (echo->lock) {
	pj_lock_destroy(echo->lock);
	echo->lock = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Let the AEC knows that a frame has been played to the speaker.
 */
PJ_DEF(pj_status_t) speex_aec_playback(void *state,
				       pj_int16_t *play_frm )
{
    speex_ec *echo = state;

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && play_frm, PJ_EINVAL);

    /* The AEC must be configured to support internal playback buffer */
    PJ_ASSERT_RETURN(echo->frame_queue!= NULL, PJ_EINVALIDOP);

    pj_lock_acquire(echo->lock);

    /* Inc timestamp */
    echo->play_ts += echo->samples_per_frame;

    /* Initialize frame delay. */
    if ((echo->ts_flag & TS_FLAG_PLAY) == 0) {
	echo->ts_flag |= TS_FLAG_PLAY;

	if (echo->ts_flag == TS_FLAG_OK) {
	    int seq_delay;

	    seq_delay = ((int)echo->play_ts - (int)echo->rec_ts) / 
			    (int)echo->samples_per_frame;
	    pjmedia_frame_queue_init(echo->frame_queue, seq_delay, 
				     echo->prefetch);
	}
    }

    if (pjmedia_frame_queue_put(echo->frame_queue, play_frm, 
				echo->samples_per_frame*2, 
				echo->play_ts) != PJ_SUCCESS)
    {
	int seq_delay;

	/* On full reset frame queue */
	seq_delay = ((int)echo->play_ts - (int)echo->rec_ts) / 
			(int)echo->samples_per_frame;
	pjmedia_frame_queue_init(echo->frame_queue, seq_delay,
				 echo->prefetch);

	/* And re-put */
	pjmedia_frame_queue_put(echo->frame_queue, play_frm, 
				echo->samples_per_frame*2, 
				echo->play_ts);
    }

    pj_lock_release(echo->lock);

    return PJ_SUCCESS;
}


/*
 * Let the AEC knows that a frame has been captured from the microphone.
 */
PJ_DEF(pj_status_t) speex_aec_capture( void *state,
				       pj_int16_t *rec_frm,
				       unsigned options )
{
    speex_ec *echo = state;
    pj_status_t status = PJ_SUCCESS;

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && rec_frm, PJ_EINVAL);

    /* The AEC must be configured to support internal playback buffer */
    PJ_ASSERT_RETURN(echo->frame_queue!= NULL, PJ_EINVALIDOP);

    /* Lock mutex */
    pj_lock_acquire(echo->lock);

    /* Inc timestamp */
    echo->rec_ts += echo->samples_per_frame;

    /* Init frame delay. */
    if ((echo->ts_flag & TS_FLAG_REC) == 0) {
	echo->ts_flag |= TS_FLAG_REC;

	if (echo->ts_flag == TS_FLAG_OK) {
	    int seq_delay;

	    seq_delay = ((int)echo->play_ts - (int)echo->rec_ts) / 
			    (int)echo->samples_per_frame;
	    pjmedia_frame_queue_init(echo->frame_queue, seq_delay, 
				     echo->prefetch);
	}
    }

    /* Cancel echo */
    if (echo->ts_flag == TS_FLAG_OK) {
	void *play_buf;
	unsigned size = 0;
	
	if (pjmedia_frame_queue_empty(echo->frame_queue)) {
	    int seq_delay;

	    seq_delay = ((int)echo->play_ts - (int)echo->rec_ts) / 
			    (int)echo->samples_per_frame;
	    pjmedia_frame_queue_init(echo->frame_queue, seq_delay, 
				     echo->prefetch);
	    status = -1;

	} else {
	    status = pjmedia_frame_queue_get(echo->frame_queue, echo->rec_ts,
					     &play_buf, &size);
	    if (size != 0) {
		speex_aec_cancel_echo(echo, rec_frm, (pj_int16_t*)play_buf,
				      options, NULL);
	    }	
	}

	if (status != PJ_SUCCESS)
	    speex_echo_state_reset(echo->state);
    }

    pj_lock_release(echo->lock);
    return PJ_SUCCESS;
}


/*
 * Perform echo cancellation.
 */
PJ_DEF(pj_status_t) speex_aec_cancel_echo( void *state,
					   pj_int16_t *rec_frm,
					   const pj_int16_t *play_frm,
					   unsigned options,
					   void *reserved )
{
    speex_ec *echo = state;

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && rec_frm && play_frm && options==0 &&
		     reserved==NULL, PJ_EINVAL);

    /* Cancel echo, put output in temporary buffer */
    speex_echo_cancellation(echo->state, (const spx_int16_t*)rec_frm,
			    (const spx_int16_t*)play_frm,
			    (spx_int16_t*)echo->tmp_frame);


    /* Preprocess output */
    speex_preprocess_run(echo->preprocess, (spx_int16_t*)echo->tmp_frame);

    /* Copy temporary buffer back to original rec_frm */
    pjmedia_copy_samples(rec_frm, echo->tmp_frame, echo->samples_per_frame);

    return PJ_SUCCESS;

}

