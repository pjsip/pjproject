/* Copyright (C) 2007 Hong Zhiqian */
/**
   @file cb_search_tm.h
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

#define OVERRIDE_COMPUTE_WEIGHTED_CODEBOOK
static void compute_weighted_codebook(
	const signed char * restrict shape_cb, 
	const Int16 * restrict r, 
	Int16 * restrict resp, 
	Int16 * restrict resp2, 
	Int32 * restrict E, 
	int shape_cb_size, int subvect_size, char *stack)
{
	register int i, j;
	register int quadsize;

	TMDEBUG_ALIGNMEM(r);
	TMDEBUG_ALIGNMEM(resp);
	TMDEBUG_ALIGNMEM(resp2);

	COMPUTEWEIGHTEDCODEBOOK_START();

	quadsize = subvect_size << 2;

	for ( i=0 ; i<shape_cb_size ; i+=4 )
	{	register int e0, e1, e2, e3;
		
		e0 = e1 = e2 = e3 = 0;
		for( j=0 ; j<subvect_size ; ++j )
		{
			register int k, rr;
			register int resj0, resj1, resj2, resj3;
			
			resj0 = resj1 = resj2 = resj3 = 0;

			for ( k=0 ; k<=j ; ++k )
			{	rr = r[j-k];
				
				resj0 += shape_cb[k] * rr;
				resj1 += shape_cb[k+subvect_size] * rr;
				resj2 += shape_cb[k+2*subvect_size] * rr;
				resj3 += shape_cb[k+3*subvect_size] * rr;
			}

			resj0 >>= 13;
			resj1 >>= 13;
			resj2 >>= 13;
			resj3 >>= 13;

			e0 += resj0 * resj0;
			e1 += resj1 * resj1;
			e2 += resj2 * resj2;
			e3 += resj3 * resj3;
			
			resp[j]					= resj0;
			resp[j+subvect_size]	= resj1;
			resp[j+2*subvect_size]	= resj2;
			resp[j+3*subvect_size]	= resj3;
		}

		E[i]		= e0;
		E[i+1]		= e1;
		E[i+2]		= e2;
		E[i+3]		= e3;

		resp		+= quadsize;
		shape_cb	+= quadsize;
	}
	
#ifndef	REMARK_ON
	(void)resp2;
	(void)stack;
#endif

	COMPUTEWEIGHTEDCODEBOOK_STOP();
}

#define OVERRIDE_TARGET_UPDATE
static inline void target_update(Int16 * restrict t, Int16 g, Int16 * restrict r, int len)
{
	register int n;
	register int gr1, gr2, t1, t2, r1, r2;
	register int quadsize;

	TARGETUPDATE_START();

	quadsize = len & 0xFFFFFFFC;
	
	for ( n=0; n<quadsize ; n+=4 )
	{	gr1 = pack16lsb(PSHR32((g * r[n]),13)  , PSHR32((g * r[n+1]),13));
		gr2 = pack16lsb(PSHR32((g * r[n+2]),13), PSHR32((g * r[n+3]),13));
		
		t1	= pack16lsb(t[n],  t[n+1]);
		t2	= pack16lsb(t[n+2],t[n+3]);

		r1	= dspidualsub(t1, gr1);
		r2	= dspidualsub(t2, gr2);

		t[n] 	= asri(16,r1);
		t[n+1]	= sex16(r1);
		t[n+2]	= asri(16,r2);
		t[n+3]	= sex16(r2);
	}

	for ( n=quadsize ; n<len ; ++n )
	{	t[n] = SUB16(t[n],PSHR32(MULT16_16(g,r[n]),13));
	}

	TARGETUPDATE_STOP();
}

#endif

