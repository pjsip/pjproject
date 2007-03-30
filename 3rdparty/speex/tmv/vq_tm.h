/* Copyright (C) 2007 Hong Zhiqian */
/**
   @file vq_tm.h
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

inline void vq_nbest_dist(int i, int N, int dist, int *used, int *nbest, Int32 *best_dist)
{
	register int k;
	
	if (i<N || dist<best_dist[N-1])
	{
		for (k=N-1; (k >= 1) && (k > *used || dist < best_dist[k-1]); k--)
		{	best_dist[k]=best_dist[k-1];
			nbest[k] = nbest[k-1];
		}
		
		best_dist[k]=dist;
		nbest[k]=i;
		*used++;
	}
}

void vq_nbest_5(Int16 *in, const Int16 *codebook, int entries, Int32 *E, int N, int *nbest, Int32 *best_dist)
{
	register int	i, j;
	register int	in10, in32, in4;
	int 			used = 0;
	
	in10 = pack16lsb(in[1],in[0]);			/* Note: memory is not align here */
	in32 = pack16lsb(in[3],in[2]);
	in4 = sex16(in[4]);	

#if (TM_UNROLL && TM_UNROLL_VQNBEST > 0)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif

	for ( i=0,j=0 ; i<entries ; i+=2,j+=5 )
	{
		register int dist1, dist2;
		register int cb10, cb32, cb54, cb76, cb98, cb87, cb65;
		
		cb10  =  ld32x(codebook,j);
		cb32  =  ld32x(codebook,j+1);
		cb54  =  ld32x(codebook,j+2);
		cb76  =  ld32x(codebook,j+3);
		cb98  =  ld32x(codebook,j+4);

		dist1 = sex16(cb54) * in4;
		dist1 += ifir16(in10,cb10) + ifir16(in32,cb32);
		dist1 = (E[i] >> 1) - dist1;
		
		cb65  =  funshift2(cb76,cb54);
		cb87  =  funshift2(cb98,cb76);
		dist2 =  asri(16,cb98) * in4;
		dist2 += ifir16(in10,cb65) + ifir16(in32,cb87);
		dist2 = (E[i+1] >> 1) - dist2;

		vq_nbest_dist(i,N,dist1,&used,nbest,best_dist);
		vq_nbest_dist(i+1,N,dist2,&used,nbest,best_dist);
	}

#if (TM_UNROLL && TM_UNROLL_VQNBEST > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
}


void vq_nbest_8(Int16 *in, const Int16 *codebook, int entries, Int32 *E, int N, int *nbest, Int32 *best_dist)
{
	register int	i, j;
	register int	in10, in32, in54, in76;
	int 			used = 0;
	
	in10 = pack16lsb(in[1],in[0]);				/* Note: memory is not align here */
	in32 = pack16lsb(in[3],in[2]);
	in54 = pack16lsb(in[5],in[4]);	
	in76 = pack16lsb(in[7],in[6]);	

#if (TM_UNROLL && TM_UNROLL_VQNBEST > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
	for ( i=0,j=0 ; i<entries ; ++i,j+=4 )
	{
		register int dist;
		register int cb10, cb32, cb54, cb76;
		
		cb10  =  ld32x(codebook,j);
		cb32  =  ld32x(codebook,j+1);
		cb54  =  ld32x(codebook,j+2);
		cb76  =  ld32x(codebook,j+3);
		
		dist  =  ifir16(in10,cb10) + ifir16(in32,cb32);
		dist  += ifir16(in54,cb54) + ifir16(in76,cb76);
		dist  =  (E[i] >> 1) - dist;
	  
		vq_nbest_dist(i,N,dist,&used,nbest,best_dist);
	}
#if (TM_UNROLL && TM_UNROLL_VQNBEST > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif

}


void vq_nbest_10(Int16 *in, const Int16 *codebook, int entries, Int32 *E, int N, int *nbest, Int32 *best_dist)
{
	register int	i, j;
	register int	in10, in32, in54, in76, in98;
	int			 	used = 0;
	
	in10 = pack16lsb(in[1],in[0]);
	in32 = pack16lsb(in[3],in[2]);
	in54 = pack16lsb(in[5],in[4]);
	in76 = pack16lsb(in[7],in[6]);
	in98 = pack16lsb(in[9],in[8]);

#if (TM_UNROLL && TM_UNROLL_VQNBEST > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
	for ( i=0,j=0 ; i<entries ; ++i,j+=5 )
	{
		register int dist;
		register int cb10, cb32, cb54, cb76, cb98;
		
		cb10  =  ld32x(codebook,j);
		cb32  =  ld32x(codebook,j+1);
		cb54  =  ld32x(codebook,j+2);
		cb76  =  ld32x(codebook,j+3);
		cb98  =  ld32x(codebook,j+4);

		dist  =  ifir16(in10,cb10) + ifir16(in32,cb32);
		dist  += ifir16(in54,cb54) + ifir16(in76,cb76);
		dist  += ifir16(in98,cb98);
		dist  =  (E[i] >> 1) - dist;
	  
		vq_nbest_dist(i,N,dist,&used,nbest,best_dist);	  
	}
#if (TM_UNROLL && TM_UNROLL_VQNBEST > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
}

void vq_nbest_20(Int16 *in, const Int16 *codebook, int entries, Int32 *E, int N, int *nbest, Int32 *best_dist)
{
	register int	i, j;
	register int	in10, in32, in54, in76, in98, in_10, in_32, in_54, in_76, in_98;
	int 			used = 0;
	
	in10  = pack16lsb(in[1],in[0]);					/* Note: memory is not align here */
	in32  = pack16lsb(in[3],in[2]);
	in54  = pack16lsb(in[5],in[4]);
	in76  = pack16lsb(in[6],in[6]);
	in98  = pack16lsb(in[9],in[8]);
	in_10 = pack16lsb(in[11],in[10]);
	in_32 = pack16lsb(in[13],in[12]);
	in_54 = pack16lsb(in[15],in[14]);
	in_76 = pack16lsb(in[17],in[16]);
	in_98 = pack16lsb(in[19],in[18]);

#if (TM_UNROLL && TM_UNROLL_VQNBEST > 0)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif
	for ( i=0,j=0 ; i<entries ; ++i,j+=10 )
	{
		register int dist;
		register int cb10, cb32, cb54, cb76, cb98, cb_10, cb_32, cb_54, cb_76, cb_98;
		
		cb10   =  ld32x(codebook,j);
		cb32   =  ld32x(codebook,j+1);
		cb54   =  ld32x(codebook,j+2);
		cb76   =  ld32x(codebook,j+3);
		cb98   =  ld32x(codebook,j+4);
		cb_10  =  ld32x(codebook,j+5);
		cb_32  =  ld32x(codebook,j+6);
		cb_54  =  ld32x(codebook,j+7);
		cb_76  =  ld32x(codebook,j+8);
		cb_98  =  ld32x(codebook,j+9);

		dist   =  ifir16(in10,cb10) + ifir16(in32,cb32);
		dist   += ifir16(in54,cb54) + ifir16(in76,cb76);
		dist   += ifir16(in98,cb98) + ifir16(in_10,cb_10);
		dist   += ifir16(in_32,cb_32) + ifir16(in_54,cb_54);
		dist   += ifir16(in_76,cb_76) + ifir16(in_98,cb_98);

		dist   =  (E[i] >> 1) - dist;  
		vq_nbest_dist(i,N,dist,&used,nbest,best_dist);
	}
#if (TM_UNROLL && TM_UNROLL_VQNBEST > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
}


#define OVERRIDE_VQ_NBEST
void vq_nbest (Int16 *in, const Int16 *codebook, int len, int entries, Int32 *E, int N, int *nbest, Int32 *best_dist, char *stack)
{
	TMDEBUG_ALIGNMEM(codebook);

	VQNBEST_START();
	if( len==5 )
		vq_nbest_5(in,codebook,entries,E,N,nbest,best_dist);
	else if ( len==8 )
		vq_nbest_8(in,codebook,entries,E,N,nbest,best_dist);
	else if ( len==10 )
		vq_nbest_10(in,codebook,entries,E,N,nbest,best_dist);
	else if ( len==20 )
		vq_nbest_20(in,codebook,entries,E,N,nbest,best_dist);

#ifndef REMARK_ON
	(void)stack;
#endif

	VQNBEST_STOP();
}

inline void vq_nbest_sign_dist(int i, int N, int dist, int sign, int entries, int *used, int *nbest, Int32 *best_dist)
{
	register int k;

	if (i<N || dist<best_dist[N-1])
	{	for (k=N-1; (k >= 1) && (k > *used || dist < best_dist[k-1]); k--)
		{
			best_dist[k]=best_dist[k-1];
			nbest[k] = nbest[k-1];
		}
		
		if ( sign ) i += entries;
		best_dist[k]=dist;
		*used++;
		nbest[k] = i;
	}
}											

void vq_nbest_sign_5(Int16 *in, const Int16 *codebook, int entries, Int32 *E, int N, int *nbest, Int32 *best_dist)
{
	register int 	i, j;
	register int	in10, in32, in4;
	int				used = 0;
	
	in10 = ld32(in);
	in32 = ld32x(in,1);
	in4	 = sex16(in[4]);

#if (TM_UNROLL && TM_UNROLL_VQSIGNNBEST > 0)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif

	for ( i=0,j=0 ; i<entries ; i+=2,j+=5 )
	{
		register int dist1, dist2, sign1, sign2;
		register int cb10, cb32, cb54, cb76, cb98, cb87, cb65;
		
		cb10  =  ld32x(codebook,j);
		cb32  =  ld32x(codebook,j+1);
		cb54  =  ld32x(codebook,j+2);
		cb76  =  ld32x(codebook,j+3);
		cb98  =  ld32x(codebook,j+4);

		dist1 = sex16(cb54) * in4;
		dist1 += ifir16(in10,cb10) + ifir16(in32,cb32);
		
		sign1 = mux(dist1>0,0,1);	
		dist1 = iflip(dist1>0,dist1);
		dist1 =  (E[i] >> 1) + dist1;  
		
		cb65  =  funshift2(cb76,cb54);
		cb87  =  funshift2(cb98,cb76);
		dist2 =  asri(16,cb98) * in4;
		dist2 += ifir16(in10,cb65) + ifir16(in32,cb87);
		
		sign2 = mux(dist2>0,0,1);	
		dist2 = iflip(dist2>0,dist2);
		dist2 =  (E[i] >> 1) + dist2;  
		
		vq_nbest_sign_dist(i,N,dist1,sign1,entries,&used,nbest,best_dist);
		vq_nbest_sign_dist(i+1,N,dist2,sign2,entries,&used,nbest,best_dist);
	}
#if (TM_UNROLL && TM_UNROLL_VQSIGNNBEST > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
}

void vq_nbest_sign_8(Int16 *in, const Int16 *codebook, int entries, Int32 *E, int N, int *nbest, Int32 *best_dist)
{
	register int	i, j;
	register int	sign;
	register int	in10, in32, in54, in76;
	int			 	used = 0;

	in10 = ld32(in);
	in32 = ld32x(in,1);
	in54 = ld32x(in,2);
	in76 = ld32x(in,3);

#if (TM_UNROLL && TM_UNROLL_VQSIGNNBEST > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif

	for ( i=0,j=0 ; i<entries ; ++i,j+=4 )
	{
		register int dist;
		register int cb10, cb32, cb54, cb76;
		
		cb10  =  ld32x(codebook,j);
		cb32  =  ld32x(codebook,j+1);
		cb54  =  ld32x(codebook,j+2);
		cb76  =  ld32x(codebook,j+3);
		
		dist  =  ifir16(in10,cb10) + ifir16(in32,cb32);
		dist  += ifir16(in54,cb54) + ifir16(in76,cb76);
		
		sign = mux(dist>0,0,1);	
		dist = iflip(dist>0,dist);
		dist =  (E[i] >> 1) + dist;  
      
	  	vq_nbest_sign_dist(i,N,dist,sign,entries,&used,nbest,best_dist);
	}
#if (TM_UNROLL && TM_UNROLL_VQSIGNNBEST > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
}

void vq_nbest_sign_10(Int16 *in, const Int16 *codebook, int entries, Int32 *E, int N, int *nbest, Int32 *best_dist)
{
	register int	i, j;
	register int	sign;
	register int	in10, in32, in54, in76, in98;
	int			 	used = 0;
	
	in10 = ld32(in);
	in32 = ld32x(in,1);
	in54 = ld32x(in,2);
	in76 = ld32x(in,3);
	in98 = ld32x(in,4);

#if (TM_UNROLL && TM_UNROLL_VQSIGNNBEST > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
	for ( i=0,j=0 ; i<entries ; ++i,j+=5 )
	{
		register int dist;
		register int cb10, cb32, cb54, cb76, cb98;
		
		cb10  =  ld32x(codebook,j);
		cb32  =  ld32x(codebook,j+1);
		cb54  =  ld32x(codebook,j+2);
		cb76  =  ld32x(codebook,j+3);
		cb98  =  ld32x(codebook,j+4);
		
		dist  =  ifir16(in10,cb10) + ifir16(in32,cb32);
		dist  += ifir16(in54,cb54) + ifir16(in76,cb76);
		dist  += ifir16(in98,cb98);
		
		sign = mux(dist>0,0,1);	
		dist = iflip(dist>0,dist);
		dist =  (E[i] >> 1) + dist;  
      
	  	vq_nbest_sign_dist(i,N,dist,sign,entries,&used,nbest,best_dist);
	}
#if (TM_UNROLL && TM_UNROLL_VQSIGNNBEST > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
}

void vq_nbest_sign_20(Int16 *in, const Int16 *codebook, int entries, Int32 *E, int N, int *nbest, Int32 *best_dist)
{
	register int	i, j;
	register int	sign;
	register int	in10, in32, in54, in76, in98, in_10, in_32, in_54, in_76, in_98;
	int			 	used = 0;
	
	in10  = ld32(in);
	in32  = ld32x(in,1);
	in54  = ld32x(in,2);
	in76  = ld32x(in,3);
	in98  = ld32x(in,4);
	in_10 = ld32x(in,5); 
	in_32 = ld32x(in,6); 
	in_54 = ld32x(in,7); 
	in_76 = ld32x(in,8); 
	in_98 = ld32x(in,9); 

#if (TM_UNROLL && TM_UNROLL_VQSIGNNBEST > 0)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif
	for ( i=0,j=0 ; i<entries ; ++i,j+=10 )
	{
		register int dist;
		register int cb10, cb32, cb54, cb76, cb98, cb_10, cb_32, cb_54, cb_76, cb_98;
		
		cb10   =  ld32x(codebook,j);
		cb32   =  ld32x(codebook,j+1);
		cb54   =  ld32x(codebook,j+2);
		cb76   =  ld32x(codebook,j+3);
		cb98   =  ld32x(codebook,j+4);
		cb_10  =  ld32x(codebook,j+5);
		cb_32  =  ld32x(codebook,j+6);
		cb_54  =  ld32x(codebook,j+7);
		cb_76  =  ld32x(codebook,j+8);
		cb_98  =  ld32x(codebook,j+9);

		dist   =  ifir16(in10,cb10) + ifir16(in32,cb32);
		dist   += ifir16(in54,cb54) + ifir16(in76,cb76);
		dist   += ifir16(in98,cb98) + ifir16(in_10,cb_10);
		dist   += ifir16(in_32,cb_32) + ifir16(in_54,cb_54);
		dist   += ifir16(in_76,cb_76) + ifir16(in_98,cb_98);
				
		sign = mux(dist>0,0,1);	
		dist = iflip(dist>0,dist);
		dist =  (E[i] >> 1) + dist;  
      
	  	vq_nbest_sign_dist(i,N,dist,sign,entries,&used,nbest,best_dist);
	}
#if (TM_UNROLL && TM_UNROLL_VQSIGNNBEST > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
}

#define OVERRIDE_VQ_NBEST_SIGN
void vq_nbest_sign (Int16 *in, const Int16 *codebook, int len, int entries, Int32 *E, int N, int *nbest, Int32 *best_dist, char *stack)
{
	TMDEBUG_ALIGNMEM(in);
	TMDEBUG_ALIGNMEM(codebook);

	VQNBESTSIGN_START();

	if( len==5 )
		vq_nbest_sign_5(in,codebook,entries,E,N,nbest,best_dist);
	else if ( len==8 )
		vq_nbest_sign_8(in,codebook,entries,E,N,nbest,best_dist);
	else if ( len==10 )
		vq_nbest_sign_10(in,codebook,entries,E,N,nbest,best_dist);
	else if ( len==20 )
		vq_nbest_sign_20(in,codebook,entries,E,N,nbest,best_dist);

#ifndef REMARK_ON
	(void)stack;
#endif

	VQNBESTSIGN_STOP();
}

#endif

