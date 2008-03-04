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
#include <pjmedia/wsola.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>

#define THIS_FILE   "wsola.c"

//#undef PJ_HAS_FLOATING_POINT

/* History size, in percentage of samples_per_frame */
#define HISTSZ		(1.5)

/* Number of frames in buffer (excluding history) */
#define FRAME_CNT	4

/* Template size in msec */
#define TEMPLATE_PTIME	(5)

/* Generate extra samples, in msec */
#define GEN_EXTRA_PTIME	(5)

/* Number of frames in erase buffer */
#define ERASE_CNT	((unsigned)3)


#ifndef M_PI
#   define  M_PI	3.14159265358979323846
#endif

#if 0
#   define TRACE_(x)	PJ_LOG(4,x)
#else
#   define TRACE_(x)
#endif


/* Buffer content:
 *
 *   t0   time    tn
 *   ---------------->
 *
 * +---------+-------+-------+---          --+
 * | history | frame | extra | ...empty...   |
 * +---------+-------+-------+----        ---+
 * ^         ^               ^               ^
 * buf    hist_cnt        cur_cnt          buf_cnt
 *           or
 *       frm pointer
 *
 * History count (hist_cnt) is a constant value, initialized upon 
 * creation.
 *
 * At any particular time, the buffer will contain at least 
 * (hist_cnt+samples_per_frame) samples.
 *
 * A "save" operation will append the frame to the end of the 
 * buffer, return the samples right after history (the frm pointer),
 * and shift the buffer by one frame.
 */

/* WSOLA structure */
struct pjmedia_wsola
{
    unsigned    clock_rate;	/* Sampling rate.			*/
    pj_uint16_t samples_per_frame;/* Samples per frame (const)		*/
    pj_uint16_t channel_count;	/* Samples per frame (const)		*/
    pj_uint16_t options;	/* Options.				*/
    pj_uint16_t hist_cnt;       /* # of history samples (const)		*/
    pj_uint16_t buf_cnt;        /* Total buffer capacity (const)	*/
    pj_uint16_t cur_cnt;        /* Cur # of samples, inc. history	*/
    pj_uint16_t template_size;	/* Template size (const)		*/
    pj_uint16_t min_extra;	/* Min extra samples for merging (const)*/
    pj_uint16_t gen_extra;	/* Generate extra samples (const)	*/
    pj_uint16_t expand_cnt;     /* Number of expansion currently done   */

    short    *buf;		/* The buffer.				*/
    short    *frm;	        /* Pointer to next frame to play in buf */
    short    *mergebuf;	        /* Temporary merge buffer.		*/
    short    *erasebuf;		/* Temporary erase buffer.		*/
#if defined(PJ_HAS_FLOATING_POINT) && PJ_HAS_FLOATING_POINT!=0
    float    *hanning;		/* Hanning window.			*/
#else
    pj_uint16_t *hanning;	/* Hanning window.			*/
#endif

    pj_timestamp ts;	        /* Running timestamp.			*/
};


#if defined(PJ_HAS_FLOATING_POINT) && PJ_HAS_FLOATING_POINT!=0
/*
 * Floating point version.
 */
#include <math.h>

static short *find_pitch(short *frm, short *beg, short *end, 
			 unsigned template_cnt, int first)
{
    short *sr, *best=beg;
    double best_corr = 0;

    for (sr=beg; sr!=end; ++sr) {
	double corr = 0;
	unsigned i;

	for (i=0; i<template_cnt; i += 8) {
	    corr += ((float)frm[i+0]) * ((float)sr[i+0]) + 
		    ((float)frm[i+1]) * ((float)sr[i+1]) + 
		    ((float)frm[i+2]) * ((float)sr[i+2]) + 
		    ((float)frm[i+3]) * ((float)sr[i+3]) + 
		    ((float)frm[i+4]) * ((float)sr[i+4]) + 
		    ((float)frm[i+5]) * ((float)sr[i+5]) + 
		    ((float)frm[i+6]) * ((float)sr[i+6]) + 
		    ((float)frm[i+7]) * ((float)sr[i+7]);
	}
	for (; i<template_cnt; ++i) {
	    corr += ((float)frm[i]) * ((float)sr[i]);
	}

	if (first) {
	    if (corr > best_corr) {
		best_corr = corr;
		best = sr;
	    }
	} else {
	    if (corr >= best_corr) {
		best_corr = corr;
		best = sr;
	    }
	}
    }

    /*TRACE_((THIS_FILE, "found pitch at %u", best-beg));*/
    return best;
}

static void overlapp_add(short dst[], unsigned count,
			 short l[], short r[],
			 float w[])
{
    unsigned i;

    for (i=0; i<count; ++i) {
	dst[i] = (short)(l[i] * w[count-1-i] + r[i] * w[i]);
    }
}

static void overlapp_add_simple(short dst[], unsigned count,
				short l[], short r[])
{
    float step = (float)(1.0 / count), stepdown = 1.0;
    unsigned i;

    for (i=0; i<count; ++i) {
	dst[i] = (short)(l[i] * stepdown + r[i] * (1-stepdown));
	stepdown -= step;
    }
}

static void create_win(pj_pool_t *pool, float **pw, unsigned count)
{
    unsigned i;
    float *w = (float*)pj_pool_calloc(pool, count, sizeof(float));

    *pw = w;

    for (i=0;i<count; i++) {
	w[i] = (float)(0.5 - 0.5 * cos(2.0 * M_PI * i / (count*2-1)) );
    }
}

#else	/* PJ_HAS_FLOATING_POINT */
/*
 * Fixed point version.
 */
#define WINDOW_BITS	15
enum { WINDOW_MAX_VAL = (1 << WINDOW_BITS)-1 };

static short *find_pitch(short *frm, short *beg, short *end, 
			 unsigned template_cnt, int first)
{
    short *sr, *best=beg;
    pj_int64_t best_corr = 0;

    
    for (sr=beg; sr!=end; ++sr) {
	pj_int64_t corr = 0;
	unsigned i;

	for (i=0; i<template_cnt; i+=8) {
	    corr += ((int)frm[i+0]) * ((int)sr[i+0]) + 
		    ((int)frm[i+1]) * ((int)sr[i+1]) + 
		    ((int)frm[i+2]) * ((int)sr[i+2]) +
		    ((int)frm[i+3]) * ((int)sr[i+3]) +
		    ((int)frm[i+4]) * ((int)sr[i+4]) +
		    ((int)frm[i+5]) * ((int)sr[i+5]) +
		    ((int)frm[i+6]) * ((int)sr[i+6]) +
		    ((int)frm[i+7]) * ((int)sr[i+7]);
	}
	for (; i<template_cnt; ++i) {
	    corr += ((int)frm[i]) * ((int)sr[i]);
	}

	if (first) {
	    if (corr > best_corr) {
		best_corr = corr;
		best = sr;
	    }
	} else {
	    if (corr >= best_corr) {
		best_corr = corr;
		best = sr;
	    }
	}
    }

    /*TRACE_((THIS_FILE, "found pitch at %u", best-beg));*/
    return best;
}

static void overlapp_add(short dst[], unsigned count,
			 short l[], short r[],
			 pj_uint16_t w[])
{
    unsigned i;

    for (i=0; i<count; ++i) {
	dst[i] = (short)(((int)(l[i]) * (int)(w[count-1-i]) + 
	                  (int)(r[i]) * (int)(w[i])) >> WINDOW_BITS);
    }
}

static void overlapp_add_simple(short dst[], unsigned count,
				short l[], short r[])
{
    int step = ((WINDOW_MAX_VAL+1) / count), 
	stepdown = WINDOW_MAX_VAL;
    unsigned i;

    for (i=0; i<count; ++i) {
	dst[i]=(short)((l[i] * stepdown + r[i] * (1-stepdown)) >> WINDOW_BITS);
	stepdown -= step;
    }
}

#if PJ_HAS_INT64
/* approx_cos():
 *   see: http://www.audiomulch.com/~rossb/code/sinusoids/ 
 */
static pj_uint32_t approx_cos( pj_uint32_t x )
{
    pj_uint32_t i,j,k;

    if( x == 0 )
	return 0xFFFFFFFF;

    i = x << 1;
    k = ((x + 0xBFFFFFFD) & 0x80000000) >> 30;
    j = i - i * ((i & 0x80000000)>>30);
    j = j >> 15;
    j = (j * j + j) >> 1;
    j = j - j * k;

    return j;
}
#endif	/* PJ_HAS_INT64 */

static void create_win(pj_pool_t *pool, pj_uint16_t **pw, unsigned count)
{
    
    unsigned i;
    pj_uint16_t *w = (pj_uint16_t*)pj_pool_calloc(pool, count, 
						  sizeof(pj_uint16_t));

    *pw = w;

    for (i=0; i<count; i++) {
#if PJ_HAS_INT64
	pj_uint32_t phase;
	pj_uint64_t cos_val;

	/* w[i] = (float)(0.5 - 0.5 * cos(2.0 * M_PI * i / (count*2-1)) ); */

	phase = (pj_uint32_t)(PJ_INT64(0xFFFFFFFF) * i / (count*2-1));
	cos_val = approx_cos(phase);

	w[i] = (pj_uint16_t)(WINDOW_MAX_VAL - 
			      (WINDOW_MAX_VAL * cos_val) / 0xFFFFFFFF);
#else
	/* Revert to linear */
	w[i] = i * WINDOW_MAX_VAL / count;
#endif
    }
}

#endif	/* PJ_HAS_FLOATING_POINT */


PJ_DEF(pj_status_t) pjmedia_wsola_create( pj_pool_t *pool, 
					  unsigned clock_rate,
					  unsigned samples_per_frame,
					  unsigned channel_count,
					  unsigned options,
					  pjmedia_wsola **p_wsola)
{
    pjmedia_wsola *wsola;

    PJ_ASSERT_RETURN(pool && clock_rate && samples_per_frame && p_wsola,
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(clock_rate <= 65535, PJ_EINVAL);
    PJ_ASSERT_RETURN(samples_per_frame < clock_rate, PJ_EINVAL);
    PJ_ASSERT_RETURN(channel_count > 0, PJ_EINVAL);

    wsola = PJ_POOL_ZALLOC_T(pool, pjmedia_wsola);
    
    wsola->clock_rate= (pj_uint16_t) clock_rate;
    wsola->samples_per_frame = (pj_uint16_t) samples_per_frame;
    wsola->channel_count = (pj_uint16_t) channel_count;
    wsola->options   = (pj_uint16_t) options;
    wsola->hist_cnt  = (pj_uint16_t)(samples_per_frame * HISTSZ);
    wsola->buf_cnt   = (pj_uint16_t)(wsola->hist_cnt + 
				    (samples_per_frame * FRAME_CNT));

    wsola->cur_cnt   = (pj_uint16_t)(wsola->hist_cnt + 
					wsola->samples_per_frame);
    wsola->min_extra = 0;

    if ((options & PJMEDIA_WSOLA_NO_PLC) == 0)
	wsola->gen_extra = (pj_uint16_t)(GEN_EXTRA_PTIME * clock_rate / 1000);

    wsola->template_size = (pj_uint16_t) (clock_rate * TEMPLATE_PTIME / 1000);
    if (wsola->template_size > samples_per_frame)
	wsola->template_size = wsola->samples_per_frame;

    wsola->buf = (short*)pj_pool_calloc(pool, wsola->buf_cnt, 
					sizeof(short));
    wsola->frm = wsola->buf + wsola->hist_cnt;
    
    wsola->mergebuf = (short*)pj_pool_calloc(pool, samples_per_frame, 
					     sizeof(short));

    if ((options & PJMEDIA_WSOLA_NO_HANNING) == 0) {
	create_win(pool, &wsola->hanning, wsola->samples_per_frame);
    }

    if ((options & PJMEDIA_WSOLA_NO_DISCARD) == 0) {
	wsola->erasebuf = (short*)pj_pool_calloc(pool, samples_per_frame *
						       ERASE_CNT,
						 sizeof(short));
    }

    *p_wsola = wsola;
    return PJ_SUCCESS;

}

PJ_DEF(pj_status_t) pjmedia_wsola_destroy(pjmedia_wsola *wsola)
{
    /* Nothing to do */
    PJ_UNUSED_ARG(wsola);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_wsola_reset( pjmedia_wsola *wsola,
					 unsigned options)
{
    PJ_ASSERT_RETURN(wsola && options==0, PJ_EINVAL);
    PJ_UNUSED_ARG(options);

    wsola->cur_cnt = (pj_uint16_t)(wsola->hist_cnt + 
				   wsola->samples_per_frame);
    pjmedia_zero_samples(wsola->buf, wsola->cur_cnt);
    return PJ_SUCCESS;
}


static void expand(pjmedia_wsola *wsola, unsigned needed)
{
    unsigned generated = 0;
    unsigned frmsz = wsola->samples_per_frame;
    unsigned rep;

    for (rep=1;; ++rep) {
	short *start, *frm;
	unsigned min_dist, max_dist, dist;

	frm = wsola->buf + wsola->cur_cnt - frmsz;
	pj_assert(frm - wsola->buf >= wsola->hist_cnt);

	max_dist = wsola->hist_cnt;
	min_dist = frmsz >> 1;

	start = find_pitch(frm, frm - max_dist, frm - min_dist,
			   wsola->template_size, 1);

	/* Should we make sure that "start" is really aligned to
	 * channel #0, in case of stereo? Probably not necessary, as
	 * find_pitch() should have found the best match anyway.
	 */

	if (wsola->options & PJMEDIA_WSOLA_NO_HANNING) {
	    overlapp_add_simple(wsola->mergebuf, frmsz,frm, start);
	} else {
	    overlapp_add(wsola->mergebuf, frmsz, frm, start, wsola->hanning);
	}

	/* How many new samples do we have */
	dist = frm - start;

	/* Copy the "tail" (excess frame) to the end */
	pjmedia_move_samples(frm + frmsz, start + frmsz, 
			     wsola->buf+wsola->cur_cnt - (start+frmsz));

	/* Copy the merged frame */
	pjmedia_copy_samples(frm, wsola->mergebuf, frmsz);

	/* We have new samples */
	wsola->cur_cnt = (pj_uint16_t)(wsola->cur_cnt + dist);

	pj_assert(wsola->cur_cnt <= wsola->buf_cnt);

	generated += dist;

	if (generated >= needed) {
	    TRACE_((THIS_FILE, "WSOLA frame expanded after %d iterations", 
		    rep));
	    break;
	}
    }
}


static unsigned compress(pjmedia_wsola *wsola, short *buf, unsigned count,
			 unsigned del_cnt)
{
    unsigned samples_del = 0, rep;

    for (rep=1; ; ++rep) {
	short *start, *end;
	unsigned frmsz = wsola->samples_per_frame;
	unsigned dist;

	if (count <= (del_cnt << 1)) {
	    TRACE_((THIS_FILE, "Not enough samples to compress!"));
	    return samples_del;
	}

	start = buf + (frmsz >> 1);
	end = start + frmsz;

	if (end + frmsz > buf + count)
	    end = buf+count-frmsz;

	start = find_pitch(buf, start, end, wsola->template_size, 0);
	dist = start - buf;

	if (wsola->options & PJMEDIA_WSOLA_NO_HANNING) {
	    overlapp_add_simple(buf, wsola->samples_per_frame, buf, start);
	} else {
	    overlapp_add(buf, wsola->samples_per_frame, buf, start, 
			 wsola->hanning);
	}

	pjmedia_move_samples(buf + frmsz, buf + frmsz + dist,
			     count - frmsz - dist);

	count -= dist;
	samples_del += dist;

	if (samples_del >= del_cnt) {
	    TRACE_((THIS_FILE, 
		    "Erased %d of %d requested after %d iteration(s)",
		    samples_del, del_cnt, rep));
	    break;
	}
    }

    return samples_del;
}



PJ_DEF(pj_status_t) pjmedia_wsola_save( pjmedia_wsola *wsola, 
					short frm[], 
					pj_bool_t prev_lost)
{
    unsigned extra;

    extra = wsola->cur_cnt - wsola->hist_cnt - wsola->samples_per_frame;
    pj_assert(extra >= 0);

    if (prev_lost && extra >= wsola->min_extra) {
	short *dst = wsola->buf + wsola->hist_cnt + wsola->samples_per_frame;

	/* Smoothen the transition. This will also erase the excess
	 * samples
	 */
	overlapp_add_simple(dst, extra, dst, frm);

	/* Copy remaining samples from the frame */
	pjmedia_copy_samples(dst+extra, frm+extra, 
			     wsola->samples_per_frame-extra);

	/* Retrieve frame */
	pjmedia_copy_samples(frm, wsola->frm, wsola->samples_per_frame);

	/* Remove excess samples */
	wsola->cur_cnt = (pj_uint16_t)(wsola->hist_cnt + 
				       wsola->samples_per_frame);

	/* Shift buffer */
	pjmedia_move_samples(wsola->buf, wsola->buf+wsola->samples_per_frame,
			      wsola->cur_cnt);

    } else {
	/* Just append to end of buffer */
	if (prev_lost) {
	    TRACE_((THIS_FILE, 
		    "Appending new frame without interpolation"));
	}

	/* Append frame */
	pjmedia_copy_samples(wsola->buf + wsola->cur_cnt, frm, 
			     wsola->samples_per_frame);

	/* Retrieve frame */
	pjmedia_copy_samples(frm, wsola->frm, 
			     wsola->samples_per_frame);

	/* Shift buffer */
	pjmedia_move_samples(wsola->buf, wsola->buf+wsola->samples_per_frame,
			     wsola->cur_cnt);
    }
    
    wsola->expand_cnt = 0;
    wsola->ts.u64 += wsola->samples_per_frame;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_wsola_generate( pjmedia_wsola *wsola, 
					    short frm[])
{
    unsigned extra;

    extra = wsola->cur_cnt - wsola->hist_cnt - wsola->samples_per_frame;

    if (extra >= wsola->samples_per_frame) {

	/* We have one extra frame in the buffer, just return this frame
	 * rather than generating a new one.
	 */
	pjmedia_copy_samples(frm, wsola->frm, wsola->samples_per_frame);
	pjmedia_move_samples(wsola->buf, wsola->buf+wsola->samples_per_frame,
			     wsola->cur_cnt - wsola->samples_per_frame);

	pj_assert(wsola->cur_cnt >= 
		    wsola->hist_cnt + (wsola->samples_per_frame << 1));
	wsola->cur_cnt = (pj_uint16_t)(wsola->cur_cnt - 
				       wsola->samples_per_frame);

    } else {
	unsigned new_samples;

	/* Calculate how many samples are need for a new frame */
	new_samples = ((wsola->samples_per_frame << 1) + wsola->gen_extra - 
		       (wsola->cur_cnt - wsola->hist_cnt));

	/* Expand buffer */
	expand(wsola, new_samples);

	pjmedia_copy_samples(frm, wsola->frm, wsola->samples_per_frame);
	pjmedia_move_samples(wsola->buf, wsola->buf+wsola->samples_per_frame,
			     wsola->cur_cnt - wsola->samples_per_frame);

	pj_assert(wsola->cur_cnt >= 
		   wsola->hist_cnt + (wsola->samples_per_frame << 1));

	wsola->cur_cnt = (pj_uint16_t)(wsola->cur_cnt - 
					wsola->samples_per_frame);
	wsola->expand_cnt++;
    }

    wsola->ts.u64 += wsola->samples_per_frame;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_wsola_discard( pjmedia_wsola *wsola, 
					   short buf1[],
					   unsigned buf1_cnt, 
					   short buf2[],
					   unsigned buf2_cnt,
					   unsigned *del_cnt)
{
    PJ_ASSERT_RETURN(wsola && buf1 && buf1_cnt && del_cnt, PJ_EINVAL);
    PJ_ASSERT_RETURN(*del_cnt, PJ_EINVAL);

    if (buf2_cnt == 0) {
	/* Buffer is contiguous space, no need to use temporary
	 * buffer.
	 */
	*del_cnt = compress(wsola, buf1, buf1_cnt, *del_cnt);

    } else {
	PJ_ASSERT_RETURN(buf2, PJ_EINVAL);

	if (buf1_cnt < ERASE_CNT * wsola->samples_per_frame &&
	    buf2_cnt < ERASE_CNT * wsola->samples_per_frame &&
	    wsola->erasebuf == NULL)
	{
	    /* We need erasebuf but WSOLA was created with 
	     * PJMEDIA_WSOLA_NO_DISCARD flag.
	     */
	    pj_assert(!"WSOLA need erase buffer!");
	    return PJ_EINVALIDOP;
	}

	if (buf2_cnt >= ERASE_CNT * wsola->samples_per_frame) {
	    *del_cnt = compress(wsola, buf2, buf2_cnt, *del_cnt);
	} else if (buf1_cnt >= ERASE_CNT * wsola->samples_per_frame) {
	    unsigned max;

	    *del_cnt = compress(wsola, buf1, buf1_cnt, *del_cnt);

	    max = *del_cnt;
	    if (max > buf2_cnt)
		max = buf2_cnt;

	    pjmedia_move_samples(buf1+buf1_cnt-(*del_cnt), buf2, 
				 max);
	    if (max < buf2_cnt) {
		pjmedia_move_samples(buf2, buf2+(*del_cnt), 
				     buf2_cnt-max);
	    }

	} else {
	    unsigned buf_cnt = buf1_cnt + buf2_cnt;
	    short *rem;	/* remainder */
	    unsigned rem_cnt;

	    if (buf_cnt > ERASE_CNT * wsola->samples_per_frame) {
		buf_cnt = ERASE_CNT * wsola->samples_per_frame;
		
		rem_cnt = buf1_cnt + buf2_cnt - buf_cnt;
		rem = buf2 + buf2_cnt - rem_cnt;
		
	    } else {
		rem = NULL;
		rem_cnt = 0;
	    }

	    pjmedia_copy_samples(wsola->erasebuf, buf1, buf1_cnt);
	    pjmedia_copy_samples(wsola->erasebuf+buf1_cnt, buf2, 
				 buf_cnt-buf1_cnt);

	    *del_cnt = compress(wsola, wsola->erasebuf, buf_cnt, *del_cnt);

	    buf_cnt -= (*del_cnt);

	    /* Copy back to buffers */
	    if (buf_cnt == buf1_cnt) {
		pjmedia_copy_samples(buf1, wsola->erasebuf, buf_cnt);
		if (rem_cnt) {
		    pjmedia_move_samples(buf2, rem, rem_cnt);
		}
	    } else if (buf_cnt < buf1_cnt) {
		pjmedia_copy_samples(buf1, wsola->erasebuf, buf_cnt);
		if (rem_cnt) {
		    unsigned c = rem_cnt;
		    if (c > buf1_cnt-buf_cnt) {
			c = buf1_cnt-buf_cnt;
		    }
		    pjmedia_copy_samples(buf1+buf_cnt, rem, c);
		    rem += c;
		    rem_cnt -= c;
		    if (rem_cnt)
			pjmedia_move_samples(buf2, rem, rem_cnt);
		}
	    } else {
		pjmedia_copy_samples(buf1, wsola->erasebuf, buf1_cnt);
		pjmedia_copy_samples(buf2, wsola->erasebuf+buf1_cnt, 
				     buf_cnt-buf1_cnt);
		if (rem_cnt) {
		    pjmedia_move_samples(buf2+buf_cnt-buf1_cnt, rem,
				         rem_cnt);
		}
	    }

	}
    }

    return (*del_cnt) > 0 ? PJ_SUCCESS : PJ_ETOOSMALL;
}


