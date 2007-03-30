/* Copyright (C) 2007 Hong Zhiqian */
/**
   @file ltp_tm.h
   @author Hong Zhiqian
   @brief Various compatibility routines for Speex (TriMedia version)
*/
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <ops/custom_defs.h>
#include "profile_tm.h"

#ifdef FIXED_POINT

#define OVERRIDE_INNER_PROD
Int32 inner_prod(const Int16 * restrict x, const Int16 * restrict y, int len)
{
	register int sum = 0;

	INNERPROD_START();

	if ( (int)x & 0x03 == 0 && (int)y & 0x03 == 0 )
	{
		register int i;

		len >>= 1;
		for ( i=0 ; i<len ; i+=4 )
		{
			register int x0, x1, y0, y1, x2, x3, y2, y3;

			x0 = ld32x(x,i);
			y0 = ld32x(x,i);
			x1 = ld32x(x,i+1);
			y1 = ld32x(y,i+1);
			sum += (ifir16(x0,y0) + ifir16(x1,y1)) >> 6;

			x2 = ld32x(x,i+2);
			y2 = ld32x(x,i+2);
			x3 = ld32x(x,i+3);
			y3 = ld32x(x,i+3);
			sum += (ifir16(x2,y2) + ifir16(x3,y3)) >> 6;

		}
	} else
	{
		len >>= 3;
		while( len-- )
		{
			register int x0, x1, x2, x3, y0, y1, y2, y3;

			x0 =	pack16lsb(x[0],x[1]);
			y0 =	pack16lsb(y[0],y[1]);
			x1 =	pack16lsb(x[2],x[3]);
			y1 =	pack16lsb(y[2],y[3]);
			sum	+=	(ifir16(x0,y0) + ifir16(x1,y1)) >> 6;

			x2 =	pack16lsb(x[4],x[5]);
			y2 =	pack16lsb(y[4],y[5]);
			x3 =	pack16lsb(x[6],x[7]);
			y3 =	pack16lsb(y[6],y[7]);
			sum	+=	(ifir16(x2,y2) + ifir16(x3,y3)) >> 6;

			x += 8;
			y += 8;
		}
	}

	INNERPROD_STOP();
	return sum;
}

#define OVERRIDE_PITCH_XCORR
void pitch_xcorr(const Int16 *_x, const Int16 *_y, Int32 *corr, int len, int nb_pitch, char *stack)
{
	register int sum_1, sum_2, sum_3, sum_4;
	register int y10, y32, y54, y76, y21, y43, y65;
	register int x10, x32;
	register int i, j, k, limit;
	
	TMDEBUG_ALIGNMEM(_x);
	TMDEBUG_ALIGNMEM(_y);

	PITCHXCORR_START();

	limit	= nb_pitch >> 1;
	len		>>= 1;

	for (i=0 ; i<limit ; i+=2 )
	{
		sum_1 = sum_2 = sum_3 = sum_4 = 0;
		
		y10	= ld32x(_y,i);
		y32	= ld32x(_y,i+1);
		
		for ( j=0 ; j<len ; j+=2 )
		{
			x10 = ld32x(_x,j);
			x32 = ld32x(_x,j+1);
			y54 = ld32x(_y,i+j+2);
			y76 = ld32x(_y,i+j+3);
			
			sum_1 += (ifir16(x10,y10) + ifir16(x32,y32)) >> 6;
			sum_3 += (ifir16(x10,y32) + ifir16(x32,y54)) >> 6;

			y21 = funshift2(y32,y10);
			y43 = funshift2(y54,y32);
			y65 = funshift2(y76,y54);

			sum_2 += (ifir16(x10,y21) + ifir16(x32,y43)) >> 6;
			sum_4 += (ifir16(x10,y43) + ifir16(x32,y65)) >> 6;

			y10 = y54;
			y32 = y76;

		}

		k = i << 1;
		corr[nb_pitch-1-k]=sum_1;
		corr[nb_pitch-2-k]=sum_2;
		corr[nb_pitch-3-k]=sum_3;
		corr[nb_pitch-4-k]=sum_4;
	}

#ifndef REMARK_ON
	(void)stack;
#endif

	PITCHXCORR_STOP();
}

#ifndef ttisim
#define OVERRIDE_PITCH_GAIN_SEARCH_3TAP_VQ
static int pitch_gain_search_3tap_vq
(
	const signed char	*gain_cdbk,
	int					gain_cdbk_size,
	Int16				*C16,
	Int16				max_gain
)
{
	register int	pp = 0x00400040, p=64;
	register int	g10, g2, g20, g21, g02, g22, g01;
	register int	cb0, cb1, cb2, cb5432;
	register int	C10, C32, C54, C76, C98, C83, C2;
	register int	acc0, acc1, acc2, acc3, sum, gsum, bsum=-VERY_LARGE32;
	register int    i, best_cdbk=0;
	register Int16	tmp;

	TMDEBUG_ALIGNMEM(C16);
	TMDEBUG_ALIGNMEM(gain_cdbk+2);

	PITCHGAINSEARCH3TAPVQ_START();

	tmp  = ild16(gain_cdbk);
	C98	 = ld32x(C16,4);
	C32	 = ld32x(C16,1);
	C10  = ld32(C16);
	C54	 = ld32x(C16,2);
	C76	 = ld32x(C16,3);

	cb0  = sex8(tmp);
	cb1	 = sex8(tmp>>8);
	C83	 = funshift2(C98,C32);
	C2	 = sex16(C32);
	gain_cdbk += 2;


#if (TM_UNROLL && TM_UNROLL_PITCHGAINSEARCH3TAPVQ > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
	for ( i=0 ; i<gain_cdbk_size ; ++i ) 
	{
         cb5432 = ld32x(gain_cdbk,i);
		 cb2	= sex8(cb5432);
		 gsum	= sex8(cb5432>>8);
		 sum	= 0;
		 
		 g10	=  pack16lsb(cb1 + 32, cb0 + 32);
		 g2		=  cb2 + 32;
		 g02	=  pack16lsb(g10, g2);
		 acc0	=  dspidualmul(g10,pp);
		 sum	+= ifir16(acc0,C10);
		 sum	+= p * g2 * C2;

		 g22	=  pack16lsb(g02, g02);
		 g01	=  funshift2(g10, g10);

		 acc1	=  dspidualmul(g22, g01);
		 sum	-= ifir16(acc1, C54);
		 acc2	=  dspidualmul(g10, g10);
		 sum	-= ifir16(acc2, C76);

		 g20	=  pack16lsb(g2, g10);
		 g21	=  funshift2(g2, g10);
		 acc3	=  dspidualmul(g20, g21);
		 sum	-= ifir16(acc3, C83);
	

		if ( sum>bsum && gsum<=max_gain ) 
		{	bsum = sum;
			best_cdbk=i;
		}

		cb0	= sex8(cb5432 >> 16);
		cb1	= asri(24,cb5432);
	}
#if (TM_UNROLL && TM_UNROLL_PITCHGAINSEARCH3TAPVQ > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif

	PITCHGAINSEARCH3TAPVQ_STOP();
	return best_cdbk;
}
#endif

#define OVERRIDE_COMPUTE_PITCH_ERROR
#ifndef OVERRIDE_PITCH_GAIN_SEARCH_3TAP_VQ
inline Int32 compute_pitch_error(Int16 *C, Int16 *g, Int16 pitch_control)
{
	register int c10, c32, c54, c76, c98, c83;
	register int g10, g32, g02, g22, g01, g21, g20;
	register int pp, tmp0, tmp1, tmp2, tmp3;
	register int sum = 0;

	
	COMPUTEPITCHERROR_START();

	g10  =  ld32(g);
	g32  =  ld32x(g,1);
	pp   =  pack16lsb(pitch_control,pitch_control);
	c10  =  ld32(C);
	c32  =  ld32x(C,1);
	g02  =  pack16lsb(g10,g32);
	g22	 =  pack16lsb(g32,g32);
	g01  =  funshift2(g10,g10);
	tmp0 =  dspidualmul(g10,pp);
	sum  += ifir16(tmp0, c10);
	sum  += pitch_control * sex16(g32) * sex16(c32);
	c54  =  ld32x(C,2);
	c76  =  ld32x(C,3);
	c98  =  ld32x(C,4);
	tmp1 =  dspidualmul(g22,g01);
	sum  -= ifir16(tmp1, c54);
	tmp2 =  dspidualmul(g10,g10);
	sum  -= ifir16(tmp2,c76);
	c83  =  funshift2(c98,c32);
	g20	 =  funshift2(g02,g02);
	g21  =  funshift2(g02,g10);
	tmp3 =  dspidualmul(g20,g21);
	sum	 -= ifir16(tmp3,c83);

	COMPUTEPITCHERROR_STOP();
    return sum;
}
#endif

#define OVERRIDE_OPEN_LOOP_NBEST_PITCH
void open_loop_nbest_pitch(Int16 *sw, int start, int end, int len, int *pitch, Int16 *gain, int N, char *stack)
{
	VARDECL(int *best_score);
	VARDECL(int *best_ener);
	VARDECL(Int32 *corr);
	VARDECL(Int16 *corr16);
	VARDECL(Int16 *ener16);
	register int i, j, k, l, N4, N2;
	register int _sw10, _sw32, _s0, _s2, limit;
	register int *energy;
	register int cshift=0, eshift=0;
	register int scaledown = 0;
	register int e0, _energy0;

	ALLOC(corr16, end-start+1, Int16);
	ALLOC(ener16, end-start+1, Int16);
	ALLOC(corr, end-start+1, Int32);
	ALLOC(best_score, N, int);
	ALLOC(best_ener, N, int);
	energy = corr;
	N4 = N << 2;
	N2 = N >> 1;

	TMDEBUG_ALIGNMEM(sw);
	TMDEBUG_ALIGNMEM(pitch);
	TMDEBUG_ALIGNMEM(gain);
	TMDEBUG_ALIGNMEM(best_score);
	TMDEBUG_ALIGNMEM(best_ener);
	TMDEBUG_ALIGNMEM(corr16);
	TMDEBUG_ALIGNMEM(ener16);

	OPENLOOPNBESTPITCH_START();

	for ( i=0 ; i<N4 ; i+=4 )
	{	st32d(i,best_score,-1);
		st32d(i,best_ener,0);
		st32d(i,pitch,start);
	}

	for ( j=asri(1,-end) ; j<N2 ; ++j )
	{	register int _sw10;

		_sw10 = ld32x(sw,j);
		_sw10 = dspidualabs(_sw10);

		if ( _sw10 & 0xC000C000 )
		{	scaledown = 1;
			break;
		}
	}

	if ( scaledown )
	{
		for ( j=asri(1,-end),k=asli(1,-end) ; j<N2 ; ++j,k+=4 )
		{	register int _sw10;
		
			_sw10 = ld32x(sw,j);
			_sw10 = dualasr(_sw10,1);
			st32d(k, sw, _sw10);
		}
	}      

	energy[0] = _energy0 = inner_prod(sw-start, sw-start, len);
	e0 = inner_prod(sw, sw, len);

	j=asri(1,-start-1); k=j+20;
	_sw10 = ld32x(sw,j);
	_sw32 = ld32x(sw,k);
	limit = end-1-start;

	for ( i=1,--j,--k ; i<limit ; i+=2,--j,--k )
	{	register int _energy1, __sw10, __sw32, __s0, __s2;
      
		_s0    = sex16(_sw10);
		_s2	   = sex16(_sw32);
		_energy1 = (_energy0 + ((_s0 * _s0) >> 6)) -  ((_s2 * _s2) >> 6);
		_energy0 = imax(0,_energy1);
		energy[i] = _energy0;
		__sw10 = ld32x(sw,j);
		__sw32 = ld32x(sw,k);
		__s0   = asri(16,__sw10);
		__s2   = asri(16,__sw32);
		_energy1 = (_energy0 + ((__s0 * __s0) >> 6)) -  ((__s2 * __s2) >> 6);
		_energy0 = imax(0,_energy1);
		energy[i+1] = _energy0;
		_sw10 = __sw10;
		_sw32 = __sw32;
	}

	_s0    = sex16(_sw10);
	_s2	   = sex16(_sw32);
	_energy0 = imax(0,(_energy0 + ((_s0 * _s0) >> 6)) -  ((_s2 * _s2) >> 6));
	energy[i] = _energy0;


	eshift = normalize16(energy, ener16, 32766, end-start+1); 
	/* In fixed-point, this actually overrites the energy array (aliased to corr) */
	pitch_xcorr(sw, sw-end, corr, len, end-start+1, stack);
	/* Normalize to 180 so we can square it and it still fits in 16 bits */
	cshift = normalize16(corr, corr16, 180, end-start+1);
	/* If we scaled weighted input down, we need to scale it up again (OK, so we've just lost the LSB, who cares?) */
   
	if ( scaledown )
	{
		for ( j=asri(1,-end),k=asli(1,-end) ; j<N2 ; ++j,k+=4 )
		{	register int _sw10;
			
			_sw10 = ld32x(sw,j);
			_sw10 = dualasl(_sw10,1);
			st32d(k, sw, _sw10);
		}
	}      

	/* Search for the best pitch prediction gain */
	for ( i=start,l=0 ; i<end ; i+=2,++l )
	{	register int _corr16, _c0, _c1;
		register int _ener16, _e0, _e1;

		_corr16 = ld32x(corr16,l);
		_corr16 = dspidualmul(_corr16,_corr16);
		_c0     = sex16(_corr16);
		_c1     = asri(16,_corr16);

		_ener16 = ld32x(ener16,l);
		_ener16 = dspidualadd(_ener16,0x00010001);
		_e0	    = sex16(_ener16);
		_e1     = asri(16,_ener16);

      /* Instead of dividing the tmp by the energy, we multiply on the other side */
      
		if ( (_c0 * best_ener[N-1]) > (best_score[N-1] * _e0) )
		{	
			best_score[N-1] = _c0;
			best_ener[N-1] = _e0;
			pitch[N-1] = i;

			for( j=0 ; j<N-1 ; ++j )
			{	if ( (_c0 * best_ener[j]) > best_score[j] * _e0 )
				{	for( k=N-1 ; k>j ; --k )
					{
						best_score[k]=best_score[k-1];
						best_ener[k]=best_ener[k-1];
						pitch[k]=pitch[k-1];
					}

			        best_score[j]=_c0;
					best_ener[j]=_e0;
					pitch[j]=i;
					break;
				}
			}
		}

		if ( (_c1 * best_ener[N-1]) > (best_score[N-1] * _e1) )
		{	
			best_score[N-1] = _c1;
			best_ener[N-1] = _e1;
			pitch[N-1] = i+1;

			for( j=0 ; j<N-1 ; ++j )
			{	if ( (_c1 * best_ener[j]) > best_score[j] * _e1 )
				{	for( k=N-1 ; k>j ; --k )
					{
						best_score[k]=best_score[k-1];
						best_ener[k]=best_ener[k-1];
						pitch[k]=pitch[k-1];
					}

			        best_score[j]=_c1;
					best_ener[j]=_e1;
					pitch[j]=i+1;
					break;
				}
			}
		}
   }
   
   /* Compute open-loop gain if necessary */
   if (gain)
   {
		for (j=0;j<N;j++)
		{
			spx_word16_t g;
			i=pitch[j];
			g = DIV32(SHL32(EXTEND32(corr16[i-start]),cshift), 10+SHR32(MULT16_16(spx_sqrt(e0),spx_sqrt(SHL32(EXTEND32(ener16[i-start]),eshift))),6));
		 	gain[j] = imax(0,g);
		}
	}

	OPENLOOPNBESTPITCH_STOP();
}


#endif

