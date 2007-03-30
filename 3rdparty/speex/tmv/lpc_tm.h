/* Copyright (C) 2007 Hong Zhiqian */
/**
   @file lpc_tm.h
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

#define OVERRIDE_SPEEX_AUTOCORR
void _spx_autocorr(const Int16 *x, Int16 *ac, int lag, int n)
{
	register int i, j;
	register int shift, ac_shift;
	register int n_2;	
	register int ac0;

	TMDEBUG_ALIGNMEM(x);
	TMDEBUG_ALIGNMEM(ac);

	_SPX_AUTOCORR_START();

	n_2		= n >> 1;
	ac0		= n + 1;

#if (TM_UNROLL && TM_UNROLL__SPXAUTOCORR)
#pragma TCS_unroll=5
#pragma TCS_unrollexact=1
#endif
	for ( j=0 ; j<n_2 ; j+=4 )
	{	register int x10, x32, x54, x76;
		
		x10 = ld32x(x,j);
		x32 = ld32x(x,j+1);
		x54 = ld32x(x,j+2);
		x76 = ld32x(x,j+3);

		ac0 += ifir16(x10, x10) >> 8;
		ac0	+= ifir16(x32, x32) >> 8;
		ac0 += ifir16(x54, x54) >> 8;
		ac0 += ifir16(x76, x76) >> 8;
	}
#if (TM_UNROLL && TM_UNROLL__SPXAUTOCORR)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif

	shift = 8;
	while (shift && ac0<0x40000000)
	{	shift--;
		ac0 <<= 1;
	}

	ac_shift = 18;
	while (ac_shift && ac0<0x40000000)
	{	ac_shift--;
		ac0 <<= 1;
	}

	if ( shift == 0 )
	{
		for ( i=0 ; i<lag ; ++i )
		{
			register int acc0, acc1, acc2;
			register int k, l, m; 
			register int x10, x32, y10, y32;
			
			acc2 = acc1 = acc0 = 0;

			for ( j=i ; j<16 ; ++j )
			{	acc0 += (x[j] * x[j-i]);
			}

			for ( k=16,l=8,m=16-i ; k<n ; k+=4,l+=2,m+=4 )
			{	
				x10  = ld32x(x,l);
				y10  = pack16lsb(x[m+1],x[m]);
				x32  = ld32x(x,l+1);
				y32  = pack16lsb(x[m+3],x[m+2]);
				
				acc1 += ifir16(x10,y10);
				acc2 += ifir16(x32,y32);
			}

			ac[i] = (acc0 + acc1 + acc2) >> ac_shift;
		}
	} else
	{
		for ( i=0 ; i<lag ; ++i )
		{
			register int acc0, acc1, acc2;
			register int k, l, m; 
			register int x10, x32, y10, y32;
			
			acc2 = acc1 = acc0 = 0;

			for ( j=i ; j<16 ; ++j )
			{	acc0 += (x[j] * x[j-i]) >> shift;
			}

			for ( k=16,l=8,m=16-i ; k<n ; k+=4,l+=2,m+=4 )
			{	
				x10  = ld32x(x,l);
				y10  = pack16lsb(x[m+1],x[m]);
				x32  = ld32x(x,l+1);
				y32  = pack16lsb(x[m+3],x[m+2]);
				
				acc1 += ifir16(x10,y10) >> shift;
				acc2 += ifir16(x32,y32) >> shift;
			}

			ac[i] = (acc0 + acc1 + acc2) >> ac_shift;
		}
	}
	
	_SPX_AUTOCORR_STOP();
}

#endif
