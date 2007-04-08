/* $Id$ */
/*
 * Based on:
 * resample-1.8.tar.gz from the 
 * Digital Audio Resampling Home Page located at
 * http://www-ccrma.stanford.edu/~jos/resample/.
 *
 * SOFTWARE FOR SAMPLING-RATE CONVERSION AND FIR DIGITAL FILTER DESIGN
 *
 * Snippet from the resample.1 man page:
 * 
 * HISTORY
 *
 * The first version of this software was written by Julius O. Smith III
 * <jos@ccrma.stanford.edu> at CCRMA <http://www-ccrma.stanford.edu> in
 * 1981.  It was called SRCONV and was written in SAIL for PDP-10
 * compatible machines.  The algorithm was first published in
 * 
 * Smith, Julius O. and Phil Gossett. ``A Flexible Sampling-Rate
 * Conversion Method,'' Proceedings (2): 19.4.1-19.4.4, IEEE Conference
 * on Acoustics, Speech, and Signal Processing, San Diego, March 1984.
 * 
 * An expanded tutorial based on this paper is available at the Digital
 * Audio Resampling Home Page given above.
 * 
 * Circa 1988, the SRCONV program was translated from SAIL to C by
 * Christopher Lee Fraley working with Roger Dannenberg at CMU.
 * 
 * Since then, the C version has been maintained by jos.
 * 
 * Sndlib support was added 6/99 by John Gibson <jgg9c@virginia.edu>.
 * 
 * The resample program is free software distributed in accordance
 * with the Lesser GNU Public License (LGPL).  There is NO warranty; not
 * even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/* PJMEDIA modification:
 *  - remove resample(), just use SrcUp, SrcUD, and SrcLinear directly.
 *  - move FilterUp() and FilterUD() from filterkit.c
 *  - move stddefs.h and resample.h to this file.
 *  - const correctness.
 */
#include <pjmedia/resample.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>


#define THIS_FILE   "resample.c"


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

typedef char           RES_BOOL;
typedef short          RES_HWORD;
typedef int            RES_WORD;
typedef unsigned short RES_UHWORD;
typedef unsigned int   RES_UWORD;

#define MAX_HWORD (32767)
#define MIN_HWORD (-32768)

#ifdef DEBUG
#define INLINE
#else
#define INLINE inline
#endif

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

#if defined(PJMEDIA_HAS_SMALL_FILTER) && PJMEDIA_HAS_SMALL_FILTER!=0
#   include "smallfilter.h"
#else
#   define SMALL_FILTER_NMULT	0
#   define SMALL_FILTER_SCALE	0
#   define SMALL_FILTER_NWING	0
#   define SMALL_FILTER_IMP	NULL
#   define SMALL_FILTER_IMPD	NULL
#endif

#if defined(PJMEDIA_HAS_LARGE_FILTER) && PJMEDIA_HAS_LARGE_FILTER!=0
#   include "largefilter.h"
#else
#   define LARGE_FILTER_NMULT	0
#   define LARGE_FILTER_SCALE	0
#   define LARGE_FILTER_NWING	0
#   define LARGE_FILTER_IMP	NULL
#   define LARGE_FILTER_IMPD	NULL
#endif


#undef INLINE
#define INLINE
#define HAVE_FILTER 0    

#ifndef NULL
#   define NULL	0
#endif


static INLINE RES_HWORD WordToHword(RES_WORD v, int scl)
{
    RES_HWORD out;
    RES_WORD llsb = (1<<(scl-1));
    v += llsb;		/* round */
    v >>= scl;
    if (v>MAX_HWORD) {
	v = MAX_HWORD;
    } else if (v < MIN_HWORD) {
	v = MIN_HWORD;
    }	
    out = (RES_HWORD) v;
    return out;
}

/* Sampling rate conversion using linear interpolation for maximum speed.
 */
static int 
  SrcLinear(const RES_HWORD X[], RES_HWORD Y[], double pFactor, RES_UHWORD nx)
{
    RES_HWORD iconst;
    RES_UWORD time = 0;
    const RES_HWORD *xp;
    RES_HWORD *Ystart, *Yend;
    RES_WORD v,x1,x2;
    
    double dt;                  /* Step through input signal */ 
    RES_UWORD dtb;                  /* Fixed-point version of Dt */
    RES_UWORD endTime;              /* When time reaches EndTime, return to user */
    
    dt = 1.0/pFactor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */
    
    Ystart = Y;
    Yend = Ystart + (unsigned)(nx * pFactor);
    endTime = time + (1<<Np)*(RES_WORD)nx;
    while (time < endTime)
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

static RES_WORD FilterUp(const RES_HWORD Imp[], const RES_HWORD ImpD[], 
		     RES_UHWORD Nwing, RES_BOOL Interp,
		     const RES_HWORD *Xp, RES_HWORD Ph, RES_HWORD Inc)
{
    const RES_HWORD *Hp;
    const RES_HWORD *Hdp = NULL;
    const RES_HWORD *End;
    RES_HWORD a = 0;
    RES_WORD v, t;
    
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
	  t += (((RES_WORD)*Hdp)*a)>>Na; /* t is now interp'd filter coeff */
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


static RES_WORD FilterUD(const RES_HWORD Imp[], const RES_HWORD ImpD[],
		     RES_UHWORD Nwing, RES_BOOL Interp,
		     const RES_HWORD *Xp, RES_HWORD Ph, RES_HWORD Inc, RES_UHWORD dhb)
{
    RES_HWORD a;
    const RES_HWORD *Hp, *Hdp, *End;
    RES_WORD v, t;
    RES_UWORD Ho;
    
    v=0;
    Ho = (Ph*(RES_UWORD)dhb)>>Np;
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
	  t += (((RES_WORD)*Hdp)*a)>>Na; /* t is now interp'd filter coeff */
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
static int SrcUp(const RES_HWORD X[], RES_HWORD Y[], double pFactor, 
		 RES_UHWORD nx, RES_UHWORD pNwing, RES_UHWORD pLpScl,
		 const RES_HWORD pImp[], const RES_HWORD pImpD[], RES_BOOL Interp)
{
    const RES_HWORD *xp;
    RES_HWORD *Ystart, *Yend;
    RES_WORD v;
    
    double dt;                  /* Step through input signal */ 
    RES_UWORD dtb;                  /* Fixed-point version of Dt */
    RES_UWORD time = 0;
    RES_UWORD endTime;              /* When time reaches EndTime, return to user */
    
    dt = 1.0/pFactor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */
    
    Ystart = Y;
    Yend = Ystart + (unsigned)(nx * pFactor);
    endTime = time + (1<<Np)*(RES_WORD)nx;
    while (time < endTime)
    {
	xp = &X[time>>Np];      /* Ptr to current input sample */
	/* Perform left-wing inner product */
	v = 0;
	v = FilterUp(pImp, pImpD, pNwing, Interp, xp, (RES_HWORD)(time&Pmask),-1);

	/* Perform right-wing inner product */
	v += FilterUp(pImp, pImpD, pNwing, Interp, xp+1,  (RES_HWORD)((-time)&Pmask),1);

	v >>= Nhg;		/* Make guard bits */
	v *= pLpScl;		/* Normalize for unity filter gain */
	*Y++ = WordToHword(v,NLpScl);   /* strip guard bits, deposit output */
	time += dtb;		/* Move to next sample by time increment */
    }
    return (Y - Ystart);        /* Return the number of output samples */
}


/* Sampling rate conversion subroutine */

static int SrcUD(const RES_HWORD X[], RES_HWORD Y[], double pFactor, 
		 RES_UHWORD nx, RES_UHWORD pNwing, RES_UHWORD pLpScl,
		 const RES_HWORD pImp[], const RES_HWORD pImpD[], RES_BOOL Interp)
{
    const RES_HWORD *xp;
    RES_HWORD *Ystart, *Yend;
    RES_WORD v;
    
    double dh;                  /* Step through filter impulse response */
    double dt;                  /* Step through input signal */
    RES_UWORD time = 0;
    RES_UWORD endTime;          /* When time reaches EndTime, return to user */
    RES_UWORD dhb, dtb;         /* Fixed-point versions of Dh,Dt */
    
    dt = 1.0/pFactor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */
    
    dh = MIN(Npc, pFactor*Npc);  /* Filter sampling period */
    dhb = dh*(1<<Na) + 0.5;     /* Fixed-point representation */
    
    Ystart = Y;
    Yend = Ystart + (unsigned)(nx * pFactor);
    endTime = time + (1<<Np)*(RES_WORD)nx;
    while (time < endTime)
    {
	xp = &X[time>>Np];	/* Ptr to current input sample */
	v = FilterUD(pImp, pImpD, pNwing, Interp, xp, (RES_HWORD)(time&Pmask),
		     -1, dhb);	/* Perform left-wing inner product */
	v += FilterUD(pImp, pImpD, pNwing, Interp, xp+1, (RES_HWORD)((-time)&Pmask),
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
					     unsigned channel_count,
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

    PJ_UNUSED_ARG(channel_count);

    /*
     * If we're downsampling, always use the fast algorithm since it seems
     * to yield the same quality.
     */
    if (rate_out < rate_in) {
	//no this is not a good idea. It sounds pretty good with speech,
	//but very poor with background noise etc.
	//high_quality = 0;
    }

#if !defined(PJMEDIA_HAS_LARGE_FILTER) || PJMEDIA_HAS_LARGE_FILTER==0
    /*
     * If large filter is excluded in the build, then prevent application
     * from using it.
     */
    if (high_quality && large_filter) {
	large_filter = PJ_FALSE;
	PJ_LOG(5,(THIS_FILE, 
		  "Resample uses small filter because large filter is "
		  "disabled"));
    }
#endif

#if !defined(PJMEDIA_HAS_SMALL_FILTER) || PJMEDIA_HAS_SMALL_FILTER==0
    /*
     * If small filter is excluded in the build and application wants to
     * use it, then drop to linear conversion.
     */
    if (high_quality && large_filter == 0) {
	high_quality = PJ_FALSE;
	PJ_LOG(4,(THIS_FILE, 
		  "Resample uses linear because small filter is disabled"));
    }
#endif

    resample->factor = rate_out * 1.0 / rate_in;
    resample->large_filter = large_filter;
    resample->high_quality = high_quality;
    resample->frame_size = samples_per_frame;

    if (high_quality) {
	unsigned size;

	/* This is a bug in xoff calculation, thanks Stephane Lussier
	 * of Macadamian dot com.
	 *   resample->xoff = large_filter ? 32 : 6;
	 */
	if (large_filter)
	    resample->xoff = (LARGE_FILTER_NMULT + 1) / 2.0  *  
			     MAX(1.0, 1.0/resample->factor);
	else
	    resample->xoff = (SMALL_FILTER_NMULT + 1) / 2.0  *  
			     MAX(1.0, 1.0/resample->factor);


	size = (samples_per_frame + 2*resample->xoff) * sizeof(pj_int16_t);
	resample->buffer = pj_pool_alloc(pool, size);
	PJ_ASSERT_RETURN(resample->buffer, PJ_ENOMEM);

	pjmedia_zero_samples(resample->buffer, resample->xoff*2);


    } else {
	resample->xoff = 0;
    }

    *p_resample = resample;

    PJ_LOG(5,(THIS_FILE, "resample created: %s qualiy, %s filter, in/out "
			  "rate=%d/%d", 
			  (high_quality?"high":"low"),
			  (large_filter?"large":"small"),
			  rate_in, rate_out));
    return PJ_SUCCESS;
}



PJ_DEF(void) pjmedia_resample_run( pjmedia_resample *resample,
				   const pj_int16_t *input,
				   pj_int16_t *output )
{
    PJ_ASSERT_ON_FAIL(resample, return);

    if (resample->high_quality) {
	pj_int16_t *dst_buf;
	const pj_int16_t *src_buf;

	/* Okay chaps, here's how we do resampling.
	 *
	 * The original resample algorithm requires xoff samples *before* the
	 * input buffer as history, and another xoff samples *after* the
	 * end of the input buffer as lookahead. Since application can only
	 * supply framesize buffer on each run, PJMEDIA needs to arrange the
	 * buffer to meet these requirements.
	 *
	 * So here comes the trick.
	 *
	 * First of all, because of the history and lookahead requirement, 
	 * resample->buffer need to accomodate framesize+2*xoff samples in its
	 * buffer. This is done when the buffer is created.
	 *
	 * On the first run, the input frame (supplied by application) is
	 * copied to resample->buffer at 2*xoff position. The first 2*xoff
	 * samples are initially zeroed (in the initialization). The resample
	 * algorithm then invoked at resample->buffer+xoff ONLY, thus giving
	 * it one xoff at the beginning as zero, and one xoff at the end
	 * as the end of the original input. The resample algorithm will see
	 * that the first xoff samples in the input as zero.
	 *
	 * So here's the layout of resample->buffer on the first run.
	 *
	 * run 0 
	 *     +------+------+--------------+
	 *     | 0000 | 0000 |  frame0...   |
	 *     +------+------+--------------+
	 *     ^      ^      ^              ^
         *     0    xoff  2*xoff       size+2*xoff 
         *
	 * (Note again: resample algorithm is called at resample->buffer+xoff)
	 *
	 * At the end of the run, 2*xoff samples from the end of 
	 * resample->buffer are copied to the beginning of resample->buffer.
	 * The first xoff part of this will be used as history for the next
	 * run, and the second xoff part of this is actually the start of
	 * resampling for the next run.
	 *
	 * And the first run completes, the function returns.
	 *
	 * 
	 * On the next run, the input frame supplied by application is again
	 * copied at 2*xoff position in the resample->buffer, and the 
	 * resample algorithm is again invoked at resample->buffer+xoff 
	 * position. So effectively, the resample algorithm will start its
	 * operation on the last xoff from the previous frame, and gets the
	 * history from the last 2*xoff of the previous frame, and the look-
	 * ahead from the last xoff of current frame.
	 *
	 * So on this run, the buffer layout is:
	 *
	 * run 1
	 *     +------+------+--------------+
	 *     | frm0 | frm0 |  frame1...   |
	 *     +------+------+--------------+
	 *     ^      ^      ^              ^
         *     0    xoff  2*xoff       size+2*xoff 
	 *
	 * As you can see from above diagram, the resampling algorithm is
	 * actually called from the last xoff part of previous frame (frm0).
	 *
	 * And so on the process continues for the next frame, and the next,
	 * and the next, ...
	 *
	 */
	dst_buf = resample->buffer + resample->xoff*2;
	pjmedia_copy_samples(dst_buf, input, resample->frame_size);
	    
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
	pjmedia_copy_samples(dst_buf, src_buf, resample->xoff * 2);

    } else {
	SrcLinear( input, output, resample->factor, resample->frame_size);
    }
}

PJ_DEF(unsigned) pjmedia_resample_get_input_size(pjmedia_resample *resample)
{
    PJ_ASSERT_RETURN(resample != NULL, 0);
    return resample->frame_size;
}

PJ_DEF(void) pjmedia_resample_destroy(pjmedia_resample *resample)
{
    PJ_UNUSED_ARG(resample);
}


