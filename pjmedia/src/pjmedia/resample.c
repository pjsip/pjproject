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
 * Based on:
 * resample-1.2.tar.Z (from ftp://ccrma-ftp.stanford.edu/pub/NeXT)
 * SOFTWARE FOR SAMPLING-RATE CONVERSION AND FIR DIGITAL FILTER DESIGN
 *
 * COPYING
 * 
 * This software package is Copyright 1994 by Julius O. Smith
 * (jos@ccrma.stanford.edu), all rights reserved.  Permission to use and copy
 * is granted subject to the terms of the "GNU Software General Public
 * License" (see ftp://prep.ai.mit.edu/pub/gnu/COPYING).  In addition, we
 * request that a copy of any modified files be sent by email to
 * jos@ccrma.stanford.edu so that we may incorporate them in the CCRMA
 * version.
 */

/* PJMEDIA modification:
 *  - remove resample(), just use SrcUp, SrcUD, and SrcLinear directly.
 *  - move FilterUp() and FilterUD() from filterkit.c
 *  - move stddefs.h and resample.h to this file.
 *  - const correctness.
 *  - fixed SrcLinear() may write pass output buffer.
 *  - assume the same for SrcUp() and SrcUD(), so put the same
 *    protection.
 */
#include <pjmedia/resample.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>



/*
 * Taken from stddefs.h
 */
#ifndef PI
#define PI (3.14159265358979232846)
#endif

#ifndef PI2
#define PI2 (6.28318530717958465692)
#endif

#define D2R (0.01745329348)          /* (2*pi)/360 */
#define R2D (57.29577951)            /* 360/(2*pi) */

#ifndef MAX
#define MAX(x,y) ((x)>(y) ?(x):(y))
#endif
#ifndef MIN
#define MIN(x,y) ((x)<(y) ?(x):(y))
#endif

#ifndef ABS
#define ABS(x)   ((x)<0   ?(-(x)):(x))
#endif

#ifndef SGN
#define SGN(x)   ((x)<0   ?(-1):((x)==0?(0):(1)))
#endif

typedef char           BOOL;
typedef short          HWORD;
typedef unsigned short UHWORD;
typedef int            WORD;
typedef unsigned int   UWORD;

#define MAX_HWORD (32767)
#define MIN_HWORD (-32768)

#ifdef DEBUG
#define INLINE
#else DEBUG
#define INLINE inline
#endif DEBUG

/*
 * Taken from resample.h
 *
 * The configuration constants below govern
 * the number of bits in the input sample and filter coefficients, the 
 * number of bits to the right of the binary-point for fixed-point math, etc.
 *
 */

/* Conversion constants */
#define Nhc       8
#define Na        7
#define Np       (Nhc+Na)
#define Npc      (1<<Nhc)
#define Amask    ((1<<Na)-1)
#define Pmask    ((1<<Np)-1)
#define Nh       16
#define Nb       16
#define Nhxn     14
#define Nhg      (Nh-Nhxn)
#define NLpScl   13

/* Description of constants:
 *
 * Npc - is the number of look-up values available for the lowpass filter
 *    between the beginning of its impulse response and the "cutoff time"
 *    of the filter.  The cutoff time is defined as the reciprocal of the
 *    lowpass-filter cut off frequence in Hz.  For example, if the
 *    lowpass filter were a sinc function, Npc would be the index of the
 *    impulse-response lookup-table corresponding to the first zero-
 *    crossing of the sinc function.  (The inverse first zero-crossing
 *    time of a sinc function equals its nominal cutoff frequency in Hz.)
 *    Npc must be a power of 2 due to the details of the current
 *    implementation. The default value of 512 is sufficiently high that
 *    using linear interpolation to fill in between the table entries
 *    gives approximately 16-bit accuracy in filter coefficients.
 *
 * Nhc - is log base 2 of Npc.
 *
 * Na - is the number of bits devoted to linear interpolation of the
 *    filter coefficients.
 *
 * Np - is Na + Nhc, the number of bits to the right of the binary point
 *    in the integer "time" variable. To the left of the point, it indexes
 *    the input array (X), and to the right, it is interpreted as a number
 *    between 0 and 1 sample of the input X.  Np must be less than 16 in
 *    this implementation.
 *
 * Nh - is the number of bits in the filter coefficients. The sum of Nh and
 *    the number of bits in the input data (typically 16) cannot exceed 32.
 *    Thus Nh should be 16.  The largest filter coefficient should nearly
 *    fill 16 bits (32767).
 *
 * Nb - is the number of bits in the input data. The sum of Nb and Nh cannot
 *    exceed 32.
 *
 * Nhxn - is the number of bits to right shift after multiplying each input
 *    sample times a filter coefficient. It can be as great as Nh and as
 *    small as 0. Nhxn = Nh-2 gives 2 guard bits in the multiply-add
 *    accumulation.  If Nhxn=0, the accumulation will soon overflow 32 bits.
 *
 * Nhg - is the number of guard bits in mpy-add accumulation (equal to Nh-Nhxn)
 *
 * NLpScl - is the number of bits allocated to the unity-gain normalization
 *    factor.  The output of the lowpass filter is multiplied by LpScl and
 *    then right-shifted NLpScl bits. To avoid overflow, we must have 
 *    Nb+Nhg+NLpScl < 32.
 */


#ifdef _MSC_VER
#   pragma warning(push, 3)
//#   pragma warning(disable: 4245)   // Conversion from uint to ushort
#   pragma warning(disable: 4244)   // Conversion from double to uint
#   pragma warning(disable: 4146)   // unary minus operator applied to unsigned type, result still unsigned
#   pragma warning(disable: 4761)   // integral size mismatch in argument; conversion supplied
#endif

#include "smallfilter.h"
#include "largefilter.h"

#undef INLINE
#define INLINE
#define HAVE_FILTER 0    

#ifndef NULL
#   define NULL	0
#endif


static INLINE HWORD WordToHword(WORD v, int scl)
{
    HWORD out;
    WORD llsb = (1<<(scl-1));
    v += llsb;		/* round */
    v >>= scl;
    if (v>MAX_HWORD) {
	v = MAX_HWORD;
    } else if (v < MIN_HWORD) {
	v = MIN_HWORD;
    }	
    out = (HWORD) v;
    return out;
}

/* Sampling rate conversion using linear interpolation for maximum speed.
 */
static int 
  SrcLinear(const HWORD X[], HWORD Y[], double pFactor, UHWORD nx)
{
    HWORD iconst;
    UWORD time = 0;
    const HWORD *xp;
    HWORD *Ystart, *Yend;
    WORD v,x1,x2;
    
    double dt;                  /* Step through input signal */ 
    UWORD dtb;                  /* Fixed-point version of Dt */
    UWORD endTime;              /* When time reaches EndTime, return to user */
    
    dt = 1.0/pFactor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */
    
    Ystart = Y;
    Yend = Ystart + (unsigned)(nx * pFactor);
    endTime = time + (1<<Np)*(WORD)nx;
    while (time < endTime && Y < Yend)	/* bennylp fix: added Y < Yend */
    {
	iconst = (time) & Pmask;
	xp = &X[(time)>>Np];      /* Ptr to current input sample */
	x1 = *xp++;
	x2 = *xp;
	x1 *= ((1<<Np)-iconst);
	x2 *= iconst;
	v = x1 + x2;
	*Y++ = WordToHword(v,Np);   /* Deposit output */
	time += dtb;		    /* Move to next sample by time increment */
    }
    return (Y - Ystart);            /* Return number of output samples */
}

static WORD FilterUp(const HWORD Imp[], const HWORD ImpD[], 
		     UHWORD Nwing, BOOL Interp,
		     const HWORD *Xp, HWORD Ph, HWORD Inc)
{
    const HWORD *Hp;
    const HWORD *Hdp = NULL;
    const HWORD *End;
    HWORD a = 0;
    WORD v, t;
    
    v=0;
    Hp = &Imp[Ph>>Na];
    End = &Imp[Nwing];
    if (Interp) {
	Hdp = &ImpD[Ph>>Na];
	a = Ph & Amask;
    }
    if (Inc == 1)		/* If doing right wing...              */
    {				/* ...drop extra coeff, so when Ph is  */
	End--;			/*    0.5, we don't do too many mult's */
	if (Ph == 0)		/* If the phase is zero...           */
	{			/* ...then we've already skipped the */
	    Hp += Npc;		/*    first sample, so we must also  */
	    Hdp += Npc;		/*    skip ahead in Imp[] and ImpD[] */
	}
    }
    if (Interp)
      while (Hp < End) {
	  t = *Hp;		/* Get filter coeff */
	  t += (((WORD)*Hdp)*a)>>Na; /* t is now interp'd filter coeff */
	  Hdp += Npc;		/* Filter coeff differences step */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & (1<<(Nhxn-1)))  /* Round, if needed */
	    t += (1<<(Nhxn-1));
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Hp += Npc;		/* Filter coeff step */

	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      } 
    else 
      while (Hp < End) {
	  t = *Hp;		/* Get filter coeff */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & (1<<(Nhxn-1)))  /* Round, if needed */
	    t += (1<<(Nhxn-1));
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Hp += Npc;		/* Filter coeff step */
	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      }
    return(v);
}


static WORD FilterUD(const HWORD Imp[], const HWORD ImpD[],
		     UHWORD Nwing, BOOL Interp,
		     const HWORD *Xp, HWORD Ph, HWORD Inc, UHWORD dhb)
{
    HWORD a;
    const HWORD *Hp, *Hdp, *End;
    WORD v, t;
    UWORD Ho;
    
    v=0;
    Ho = (Ph*(UWORD)dhb)>>Np;
    End = &Imp[Nwing];
    if (Inc == 1)		/* If doing right wing...              */
    {				/* ...drop extra coeff, so when Ph is  */
	End--;			/*    0.5, we don't do too many mult's */
	if (Ph == 0)		/* If the phase is zero...           */
	  Ho += dhb;		/* ...then we've already skipped the */
    }				/*    first sample, so we must also  */
				/*    skip ahead in Imp[] and ImpD[] */
    if (Interp)
      while ((Hp = &Imp[Ho>>Na]) < End) {
	  t = *Hp;		/* Get IR sample */
	  Hdp = &ImpD[Ho>>Na];  /* get interp (lower Na) bits from diff table*/
	  a = Ho & Amask;	/* a is logically between 0 and 1 */
	  t += (((WORD)*Hdp)*a)>>Na; /* t is now interp'd filter coeff */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & 1<<(Nhxn-1))	/* Round, if needed */
	    t += 1<<(Nhxn-1);
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Ho += dhb;		/* IR step */
	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      }
    else 
      while ((Hp = &Imp[Ho>>Na]) < End) {
	  t = *Hp;		/* Get IR sample */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & 1<<(Nhxn-1))	/* Round, if needed */
	    t += 1<<(Nhxn-1);
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Ho += dhb;		/* IR step */
	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      }
    return(v);
}

/* Sampling rate up-conversion only subroutine;
 * Slightly faster than down-conversion;
 */
static int SrcUp(const HWORD X[], HWORD Y[], double pFactor, 
		 UHWORD nx, UHWORD pNwing, UHWORD pLpScl,
		 const HWORD pImp[], const HWORD pImpD[], BOOL Interp)
{
    const HWORD *xp;
    HWORD *Ystart, *Yend;
    WORD v;
    
    double dt;                  /* Step through input signal */ 
    UWORD dtb;                  /* Fixed-point version of Dt */
    UWORD time = 0;
    UWORD endTime;              /* When time reaches EndTime, return to user */
    
    dt = 1.0/pFactor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */
    
    Ystart = Y;
    Yend = Ystart + (unsigned)(nx * pFactor);
    endTime = time + (1<<Np)*(WORD)nx;
    while (time < endTime && Y < Yend)	/* bennylp fix: protect Y */
    {
	xp = &X[time>>Np];      /* Ptr to current input sample */
	/* Perform left-wing inner product */
	v = 0;
	v = FilterUp(pImp, pImpD, pNwing, Interp, xp, (HWORD)(time&Pmask),-1);

	/* Perform right-wing inner product */
	v += FilterUp(pImp, pImpD, pNwing, Interp, xp+1,  (HWORD)((-time)&Pmask),1);

	v >>= Nhg;		/* Make guard bits */
	v *= pLpScl;		/* Normalize for unity filter gain */
	*Y++ = WordToHword(v,NLpScl);   /* strip guard bits, deposit output */
	time += dtb;		/* Move to next sample by time increment */
    }
    return (Y - Ystart);        /* Return the number of output samples */
}


/* Sampling rate conversion subroutine */

static int SrcUD(const HWORD X[], HWORD Y[], double pFactor, 
		 UHWORD nx, UHWORD pNwing, UHWORD pLpScl,
		 const HWORD pImp[], const HWORD pImpD[], BOOL Interp)
{
    const HWORD *xp;
    HWORD *Ystart, *Yend;
    WORD v;
    
    double dh;                  /* Step through filter impulse response */
    double dt;                  /* Step through input signal */
    UWORD time = 0;
    UWORD endTime;              /* When time reaches EndTime, return to user */
    UWORD dhb, dtb;             /* Fixed-point versions of Dh,Dt */
    
    dt = 1.0/pFactor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */
    
    dh = MIN(Npc, pFactor*Npc);  /* Filter sampling period */
    dhb = dh*(1<<Na) + 0.5;     /* Fixed-point representation */
    
    Ystart = Y;
    Yend = Ystart + (unsigned)(nx * pFactor);
    endTime = time + (1<<Np)*(WORD)nx;
    while (time < endTime && Y < Yend) /* bennylp fix: protect Y */
    {
	xp = &X[time>>Np];	/* Ptr to current input sample */
	v = FilterUD(pImp, pImpD, pNwing, Interp, xp, (HWORD)(time&Pmask),
		     -1, dhb);	/* Perform left-wing inner product */
	v += FilterUD(pImp, pImpD, pNwing, Interp, xp+1, (HWORD)((-time)&Pmask),
		      1, dhb);	/* Perform right-wing inner product */
	v >>= Nhg;		/* Make guard bits */
	v *= pLpScl;		/* Normalize for unity filter gain */
	*Y++ = WordToHword(v,NLpScl);   /* strip guard bits, deposit output */
	time += dtb;		/* Move to next sample by time increment */
    }
    return (Y - Ystart);        /* Return the number of output samples */
}


/* ***************************************************************************
 *
 * PJMEDIA RESAMPLE 
 *
 * ***************************************************************************
 */

struct pjmedia_resample
{
    double	 factor;	/* Conversion factor = rate_out / rate_in.  */
    pj_bool_t	 large_filter;	/* Large filter?			    */
    pj_bool_t	 high_quality;	/* Not fast?				    */
    unsigned	 xoff;		/* History and lookahead size, in samples   */
    unsigned	 frame_size;	/* Samples per frame.			    */
    pj_int16_t	*buffer;	/* Input buffer.			    */
};


PJ_DEF(pj_status_t) pjmedia_resample_create( pj_pool_t *pool,
					     pj_bool_t high_quality,
					     pj_bool_t large_filter,
					     unsigned rate_in,
					     unsigned rate_out,
					     unsigned samples_per_frame,
					     pjmedia_resample **p_resample)
{
    pjmedia_resample *resample;

    PJ_ASSERT_RETURN(pool && p_resample && rate_in &&
		     rate_out && samples_per_frame, PJ_EINVAL);

    resample = pj_pool_alloc(pool, sizeof(pjmedia_resample));
    PJ_ASSERT_RETURN(resample, PJ_ENOMEM);

    /*
     * If we're downsampling, always use the fast algorithm since it seems
     * to yield the same performance.
     */
    if (rate_out < rate_in) {
	high_quality = 0;
    }

    resample->factor = rate_out * 1.0 / rate_in;
    resample->large_filter = large_filter;
    resample->high_quality = high_quality;
    resample->xoff = large_filter ? 32 : 6;
    resample->frame_size = samples_per_frame;

    if (high_quality) {
	unsigned size;
	unsigned i;

	size = (samples_per_frame + 2*resample->xoff) * sizeof(pj_int16_t);
	resample->buffer = pj_pool_alloc(pool, size);
	PJ_ASSERT_RETURN(resample->buffer, PJ_ENOMEM);

	for (i=0; i<resample->xoff*2; ++i) {
	    resample->buffer[i] = 0;
	}
    }

    *p_resample = resample;
    return PJ_SUCCESS;
}



PJ_DEF(void) pjmedia_resample_run( pjmedia_resample *resample,
				   const pj_int16_t *input,
				   pj_int16_t *output )
{
    PJ_ASSERT_ON_FAIL(resample, return);

    if (resample->high_quality) {
	unsigned i;
	pj_int16_t *dst_buf;
	const pj_int16_t *src_buf;

	/* Buffer layout:
	 *
	 * run 0 
	 *     +------+------+--------------+
	 *     | 0000 | 0000 |  frame0...   |
	 *     +------+------+--------------+
	 *     ^      ^      ^              ^
         *     0    xoff  2*xoff       size+2*xoff 
         *
	 * run 01
	 *     +------+------+--------------+
	 *     | frm0 | frm0 |  frame1...   |
	 *     +------+------+--------------+
	 *     ^      ^      ^              ^
         *     0    xoff  2*xoff       size+2*xoff 
	 */
	dst_buf = resample->buffer + resample->xoff*2;
	for (i=0; i<resample->frame_size; ++i)
	    dst_buf[i] = input[i];
	    
	if (resample->factor >= 1) {

	    if (resample->large_filter) {
		SrcUp(resample->buffer + resample->xoff, output,
		      resample->factor, resample->frame_size,
		      LARGE_FILTER_NWING, LARGE_FILTER_SCALE,
		      LARGE_FILTER_IMP, LARGE_FILTER_IMPD,
		      PJ_TRUE);
	    } else {
		SrcUp(resample->buffer + resample->xoff, output,
		      resample->factor, resample->frame_size,
		      SMALL_FILTER_NWING, SMALL_FILTER_SCALE,
		      SMALL_FILTER_IMP, SMALL_FILTER_IMPD,
		      PJ_TRUE);
	    }

	} else {

	    if (resample->large_filter) {

		SrcUD( resample->buffer + resample->xoff, output,
		       resample->factor, resample->frame_size,
		       LARGE_FILTER_NWING, 
		       LARGE_FILTER_SCALE * resample->factor + 0.5,
		       LARGE_FILTER_IMP, LARGE_FILTER_IMPD,
		       PJ_TRUE);

	    } else {

		SrcUD( resample->buffer + resample->xoff, output,
		       resample->factor, resample->frame_size,
		       SMALL_FILTER_NWING, 
		       SMALL_FILTER_SCALE * resample->factor + 0.5,
		       SMALL_FILTER_IMP, SMALL_FILTER_IMPD,
		       PJ_TRUE);

	    }

	}

	dst_buf = resample->buffer;
	src_buf = input + resample->frame_size - resample->xoff*2;
	for (i=0; i<resample->xoff * 2; ++i) {
	    dst_buf[i] = src_buf[i];
	}

    } else {
	SrcLinear( input, output, resample->factor, resample->frame_size);
    }
}
