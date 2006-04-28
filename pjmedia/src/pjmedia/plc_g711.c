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
#include <pjmedia/types.h>
#include <pj/assert.h>
#include <pj/pool.h>


/*
 * Only build when PJMEDIA_HAS_G711_PLC is enabled.
 */
#if defined(PJMEDIA_HAS_G711_PLC) && PJMEDIA_HAS_G711_PLC!=0


#include <math.h>

typedef float Float;

#define PITCH_MIN	40			/* minimum allowed pitch, 200 Hz */
#define PITCH_MAX	120			/* maximum allowed pitch, 66 Hz */
#define PITCHDIFF	(PITCH_MAX - PITCH_MIN)
#define POVERLAPMAX	(PITCH_MAX >> 2)	/* maximum pitch OLA window */
#define HISTORYLEN	(PITCH_MAX * 3 + POVERLAPMAX) /* history buffer length*/
#define NDEC		2			/* 2:1 decimation */
#define CORRLEN		160			/* 20 ms correlation length */
#define CORRBUFLEN	(CORRLEN + PITCH_MAX)	/* correlation buffer length */
#define CORRMINPOWER	((Float)250.)		/* minimum power */
#define EOVERLAPINCR	32			/* end OLA increment per frame, 4 ms */
#define FRAMESZ		80			/* 10 ms at 8 KHz */
#define ATTENFAC	((Float).2)		/* attenuation factor per 10 ms frame */
#define ATTENINCR	(ATTENFAC/FRAMESZ)	/* attenuation per sample */


typedef struct LowcFE LowcFE;

static void dofe(LowcFE *sess, short *s); /* synthesize speech for erasure */
static void addtohistory(LowcFE *sess, short *s); /* add a good frame to history buffer */
static void scalespeech(LowcFE *sess, short *out);
static void getfespeech(LowcFE *sess, short *out, int sz);
static void savespeech(LowcFE *sess, short *s);
static int findpitch(LowcFE *sess);
static void overlapadd_r(Float *l, Float *r, Float *o, int cnt);
static void overlapadd_i(short *l, short *r, short *o, int cnt);
static void overlapaddatend(LowcFE *sess, short *s, short *f, int cnt);
static void convertsf(short *f, Float *t, int cnt);
static void convertfs(Float *f, short *t, int cnt);
static void copyf(Float *f, Float *t, int cnt);
static void copys(short *f, short *t, int cnt);
static void zeros(short *s, int cnt);

struct LowcFE
{
    unsigned samples_per_frame;

    int erasecnt;		/* consecutive erased frames */
    int poverlap;		/* overlap based on pitch */
    int poffset;		/* offset into pitch period */
    int pitch;			/* pitch estimate */
    int pitchblen;		/* current pitch buffer length */
    Float *pitchbufend;		/* end of pitch buffer */
    Float *pitchbufstart;	/* start of pitch buffer */
    Float pitchbuf[HISTORYLEN]; /* buffer for cycles of speech */
    Float lastq[POVERLAPMAX];	/* saved last quarter wavelength */
    short history[HISTORYLEN];	/* history buffer */
};



static void convertsf(short *f, Float *t, int cnt)
{
    int i;
    for (i = 0; i < cnt; i++)
	t[i] = (Float)f[i];
}

static void convertfs(Float *f, short *t, int cnt)
{
    int i;
    for (i = 0; i < cnt; i++)
	t[i] = (short)f[i];
}

void copyf(Float *f, Float *t, int cnt)
{
    int i;
    for (i = 0; i < cnt; i++)
	t[i] = f[i];
}

void copys(short *f, short *t, int cnt)
{
    int i;
    for (i = 0; i < cnt; i++)
	t[i] = f[i];
}

void zeros(short *s, int cnt)
{
    int i;
    for (i = 0; i < cnt; i++)
	s[i] = 0;
}


void init_LowcFE(LowcFE *sess)
{
    sess->erasecnt = 0;
    sess->pitchbufend = &sess->pitchbuf[HISTORYLEN];
    zeros(sess->history, HISTORYLEN);
}


/*
 * Save a frames worth of new speech in the history buffer.
 * Return the output speech delayed by POVERLAPMAX.
 */
static void savespeech(LowcFE *sess, short *s)
{
    /* make room for new signal */
    copys(&sess->history[FRAMESZ], sess->history, HISTORYLEN - FRAMESZ);

    /* copy in the new frame */
    copys(s, &sess->history[HISTORYLEN - FRAMESZ], FRAMESZ);

    /* copy out the delayed frame */
    copys(&sess->history[HISTORYLEN - FRAMESZ - POVERLAPMAX], s, FRAMESZ);
}



/*
 * A good frame was received and decoded.
 * If right after an erasure, do an overlap add with the synthetic signal.
 * Add the frame to history buffer.
 */
static void addtohistory(LowcFE *sess, short *s)
{
    if (sess->erasecnt) {
	short overlapbuf[FRAMESZ];

	/*
	 * longer erasures require longer overlaps
	 * to smooth the transition between the synthetic
	 * and real signal.
	 */
	int olen = sess->poverlap + (sess->erasecnt - 1) * EOVERLAPINCR;
	if (olen > FRAMESZ)
	    olen = FRAMESZ;

	getfespeech(sess, overlapbuf, olen);
	overlapaddatend(sess, s, overlapbuf, olen);
	sess->erasecnt = 0;
    }

    savespeech(sess, s);
}



/*
 * Generate the synthetic signal.
 * At the beginning of an erasure determine the pitch, and extract
 * one pitch period from the tail of the signal. Do an OLA for 1/4
 * of the pitch to smooth the signal. Then repeat the extracted signal
 * for the length of the erasure. If the erasure continues for more than
 * 10 ms, increase the number of periods in the pitchbuffer. At the end
 * of an erasure, do an OLA with the start of the first good frame.
 * The gain decays as the erasure gets longer.
 */
static void dofe(LowcFE *sess, short *out)
{
    if (sess->erasecnt == 0) {
	convertsf(sess->history, sess->pitchbuf, HISTORYLEN); /* get history */
	sess->pitch = findpitch(sess); /* find pitch */
	sess->poverlap = sess->pitch >> 2; /* OLA 1/4 wavelength */
	/* save original last poverlap samples */
	copyf(sess->pitchbufend - sess->poverlap, sess->lastq, sess->poverlap);
	sess->poffset = 0; /* create pitch buffer with 1 period */
	sess->pitchblen = sess->pitch;
	sess->pitchbufstart = sess->pitchbufend - sess->pitchblen;
	overlapadd_r(sess->lastq, sess->pitchbufstart - sess->poverlap,
		     sess->pitchbufend - sess->poverlap, sess->poverlap);
	/* update last 1/4 wavelength in history buffer */
	convertfs(sess->pitchbufend - sess->poverlap, &sess->history[HISTORYLEN-sess->poverlap],
		  sess->poverlap);
	getfespeech(sess, out, FRAMESZ); /* get synthesized speech */

    } else if (sess->erasecnt == 1 || sess->erasecnt == 2) {
	/* tail of previous pitch estimate */
	short tmp[POVERLAPMAX];
	int saveoffset = sess->poffset; /* save offset for OLA */
	getfespeech(sess, tmp, sess->poverlap); /* continue with old pitchbuf */
	/* add periods to the pitch buffer */
	sess->poffset = saveoffset;
	while (sess->poffset > sess->pitch)
	    sess->poffset -= sess->pitch;
	sess->pitchblen += sess->pitch; /* add a period */
	sess->pitchbufstart = sess->pitchbufend - sess->pitchblen;
	overlapadd_r(sess->lastq, sess->pitchbufstart - sess->poverlap,
		     sess->pitchbufend - sess->poverlap, sess->poverlap);
	/* overlap add old pitchbuffer with new */
	getfespeech(sess, out, FRAMESZ);
	overlapadd_i(tmp, out, out, sess->poverlap);
	scalespeech(sess, out);
    } else if (sess->erasecnt > 5) {
	zeros(out, FRAMESZ);
    } else {
	getfespeech(sess, out, FRAMESZ);
	scalespeech(sess, out);
    }
    sess->erasecnt++;
    savespeech(sess, out);
}


/*
 * Estimate the pitch.
 * l - pointer to first sample in last 20 ms of speech.
 * r - points to the sample PITCH_MAX before l
 */
static int findpitch(LowcFE *sess)
{
    int i, j, k;
    int bestmatch;
    Float bestcorr;
    Float corr; /* correlation */
    Float energy; /* running energy */
    Float scale; /* scale correlation by average power */
    Float *rp; /* segment to match */
    Float *l = sess->pitchbufend - CORRLEN;
    Float *r = sess->pitchbufend - CORRBUFLEN;

    /* coarse search */
    rp = r;
    energy = 0.f;
    corr = 0.f;
    for (i = 0; i < CORRLEN; i += NDEC) {
	energy += rp[i] * rp[i];
	corr += rp[i] * l[i];
    }
    scale = energy;
    if (scale < CORRMINPOWER)
	scale = CORRMINPOWER;
    corr = corr / (Float)sqrt(scale);
    bestcorr = corr;
    bestmatch = 0;
    for (j = NDEC; j <= PITCHDIFF; j += NDEC) {
	energy -= rp[0] * rp[0];
	energy += rp[CORRLEN] * rp[CORRLEN];
	rp += NDEC;
	corr = 0.f;
	for (i = 0; i < CORRLEN; i += NDEC)
	    corr += rp[i] * l[i];
	scale = energy;
	if (scale < CORRMINPOWER)
	    scale = CORRMINPOWER;
	corr /= (Float)sqrt(scale);
	if (corr >= bestcorr) {
	    bestcorr = corr;
	    bestmatch = j;
	}
    }
    /* fine search */
    j = bestmatch - (NDEC - 1);
    if (j < 0)
	j = 0;
    k = bestmatch + (NDEC - 1);
    if (k > PITCHDIFF)
	k = PITCHDIFF;
    rp = &r[j];
    energy = 0.f;
    corr = 0.f;
    for (i = 0; i < CORRLEN; i++) {
	energy += rp[i] * rp[i];
	corr += rp[i] * l[i];
    }
    scale = energy;
    if (scale < CORRMINPOWER)
	scale = CORRMINPOWER;
    corr = corr / (Float)sqrt(scale);
    bestcorr = corr;
    bestmatch = j;
    for (j++; j <= k; j++) {
	energy -= rp[0] * rp[0];
	energy += rp[CORRLEN] * rp[CORRLEN];
	rp++;
	corr = 0.f;
	for (i = 0; i < CORRLEN; i++)
	    corr += rp[i] * l[i];
	scale = energy;
	if (scale < CORRMINPOWER)
	    scale = CORRMINPOWER;
	corr = corr / (Float)sqrt(scale);
	if (corr > bestcorr) {
	    bestcorr = corr;
	    bestmatch = j;
	}
    }
    return PITCH_MAX - bestmatch;
}



/*
 * Get samples from the circular pitch buffer. Update poffset so
 * when subsequent frames are erased the signal continues.
 */
static void getfespeech(LowcFE *sess, short *out, int sz)
{
    while (sz) {
	int cnt = sess->pitchblen - sess->poffset;
	if (cnt > sz)
	    cnt = sz;
	convertfs(&sess->pitchbufstart[sess->poffset], out, cnt);
	sess->poffset += cnt;
	if (sess->poffset == sess->pitchblen)
	    sess->poffset = 0;
	out += cnt;
	sz -= cnt;
    }
}

static void scalespeech(LowcFE *sess, short *out)
{
    Float g = (Float)1. - (sess->erasecnt - 1) * ATTENFAC;
    int i;
    for (i = 0; i < FRAMESZ; i++) {
	out[i] = (short)(out[i] * g);
	g -= ATTENINCR;
    }
}



static void overlapadd_r(Float *l, Float *r, Float *o, int cnt)
{
    Float incr = (Float)1. / cnt;
    Float lw = (Float)1. - incr;
    Float rw = incr;
    int i;
    for (i = 0; i < cnt; i++) {
	Float t = lw * l[i] + rw * r[i];
	if (t > 32767.)
	    t = 32767.;
	else if (t < -32768.)
	    t = -32768.;
	o[i] = t;
	lw -= incr;
	rw += incr;
    }
}

static void overlapadd_i(short *l, short *r, short *o, int cnt)
{
    Float incr = (Float)1. / cnt;
    Float lw = (Float)1. - incr;
    Float rw = incr;
    int i;
    for (i = 0; i < cnt; i++) {
	Float t = lw * l[i] + rw * r[i];
	if (t > 32767.)
	    t = 32767.;
	else if (t < -32768.)
	    t = -32768.;
	o[i] = (short)t;
	lw -= incr;
	rw += incr;
    }
}

/*
 * Overlap add the end of the erasure with the start of the first good frame
 * Scale the synthetic speech by the gain factor before the OLA.
 */
static void overlapaddatend(LowcFE *sess, short *s, short *f, int cnt)
{
    Float incr = (Float)1. / cnt;
    Float gain;
    Float incrg;
    Float lw;
    Float rw;
    int i;

    gain = (Float)1. - (sess->erasecnt - 1) * ATTENFAC;
    if (gain < 0.)
	gain = (Float)0.;
    incrg = incr * gain;
    lw = ((Float)1. - incr) * gain;
    rw = incr;


    for (i = 0; i < cnt; i++) {
	Float t = lw * f[i] + rw * s[i];
	if (t > 32767.)
	    t = 32767.; 
	else if (t < -32768.)
	    t = -32768.;
	s[i] = (short)t;
	lw -= incrg;
	rw += incr;
    }
}


void* pjmedia_plc_g711_create(pj_pool_t *pool, unsigned clock_rate, 
			      unsigned samples_per_frame)
{
    LowcFE *o;

    pj_assert(clock_rate == 8000 && (samples_per_frame % FRAMESZ) == 0);

    o = pj_pool_zalloc(pool, sizeof(LowcFE));
    o->samples_per_frame = samples_per_frame;
    init_LowcFE(o);

    return o;
}


void  pjmedia_plc_g711_save(void *plc, pj_int16_t *frame)
{
    LowcFE *o = plc;
    unsigned pos;

    pos = 0;
    while (pos < o->samples_per_frame) {
	addtohistory(o, frame + pos);
	pos += FRAMESZ;
    }
}


void  pjmedia_plc_g711_generate(void *plc, pj_int16_t *frame)
{
    LowcFE *o = plc;
    unsigned pos;

    pos = 0;
    while (pos < o->samples_per_frame) {
	dofe(o, frame+pos);
	pos += FRAMESZ;
    }
}


#endif	/* PJMEDIA_HAS_G711_PLC */

