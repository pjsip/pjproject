/* Copyright (C) 2007 Hong Zhiqian */
/**
   @file quant_lsp_tm.h
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

#define OVERRIDE_COMPUTE_QUANT_WEIGHTS
static void compute_quant_weights(Int16 *qlsp, Int16 *qw, int order)
{
	int		qlspi, qlspii;
	int		w1, w2;
	int		i;

	TMDEBUG_ALIGNMEM(qlsp);
	TMDEBUG_ALIGNMEM(qw);

	COMPUTEQUANTWEIGHTS_START();

	--order;

	qlspi	= (int)qlsp[0];	
	qlspii	= (int)qlsp[1];
	w1		= qlspi;	
	w2		= qlspii - qlspi;

	qw[0]	= 81920 / (300 + imin(w1,w2));

	for ( i=1 ; i<order ; ++i )
	{	qlspi	= qlspii;
		qlspii	= qlsp[i+1];

		w1 = w2;
		w2 = qlspii - qlspi;

		qw[i] = 81920 / (300 + imin(w1,w2));
	}

	w1 = LSP_PI - qlspii;
	qw[i] = 81920 / (300 + imin(w1,w2));

	COMPUTEQUANTWEIGHTS_STOP();
}



#define OVERRIDE_LSP_QUANT
static int lsp_quant(Int16 *x, const signed char *cdbk, int nbVec, int nbDim)
{
	register int best_dist=VERY_LARGE32;
	register int best_id=0;
	register int i, j;
	register int dt0, dt1, dt2, dt3;
	register int cb0, cb1, cb2, cb3, xx;
	register int ptr_inc = nbDim * 3;
	register int five = 5;
    const signed char *ptr;

	TMDEBUG_ALIGNMEM(x);

	LSPQUANT_START();

	for ( i=0, ptr=cdbk ; i<nbVec ; i+=4, ptr += ptr_inc )
	{	dt3 = dt2 = dt1 = dt0 = 0;

		for ( j=0 ; j <nbDim ; j += 2 )
		{
			xx	  =  ld32x(x,j>>1);
			cb0	  =  pack16lsb((int)ptr[1], (int)ptr[0]);
			cb0	  =  dualasl(cb0,five);
			cb0	  =  dspidualsub(xx,cb0);
			dt0	  += ifir16(cb0,cb0);

			cb1	  =	 pack16lsb((int)ptr[nbDim+1], (int)ptr[nbDim]);
			cb1	  =  dualasl(cb1,five);
			cb1   =  dspidualsub(xx,cb1);
			dt1   += ifir16(cb1, cb1);

			cb2	  =	 pack16lsb((int)ptr[nbDim*2+1], (int)ptr[nbDim*2]);
			cb2	  =  dualasl(cb2,five);
			cb2	  =  dspidualsub(xx,cb2);
			dt2   += ifir16(cb2, cb2);

			cb3	  =	 pack16lsb((int)ptr[nbDim*3+1], (int)ptr[nbDim*3]);
			cb3	  =  dualasl(cb3,five);
			cb3	  =  dspidualsub(xx,cb3);
			dt3   += ifir16(cb3, cb3);

			ptr += 2;
		}

		if ( dt0<best_dist )
		{	best_dist	=	dt0;
			best_id		=	i;
		}

		if ( dt1<best_dist )
		{	best_dist	=	dt1;
			best_id		=	i+1;
		}
		
		if ( dt2<best_dist )
		{	best_dist	=	dt2;
			best_id		=	i+2;
		}

		if ( dt3<best_dist )
		{	best_dist	=	dt3;
			best_id		=	i+3;
		}
	}

	for ( j=0,ptr=cdbk+best_id*nbDim ; j<nbDim ; j+=2 )
	{	xx	  =  ld32x(x,j>>1);
		cb0	  =  pack16lsb((int)ptr[j+1], (int)ptr[j]);
		cb0	  =  dualasl(cb0,five);
		dt0	  =  dspidualsub(xx,cb0);
		st32d(j<<1, x, dt0);
 	}

	LSPQUANT_STOP();
	return best_id;
}


#define OVERRIDE_LSP_WEIGHT_QUANT
static int lsp_weight_quant(Int16 *x, Int16 *weight, const signed char *cdbk, int nbVec, int nbDim)
{
	register int best_dist=VERY_LARGE32;
	register int best_id=0;
	register int i, j;
	register int dt1, dt2, dt3, dt4;
	register int cb1, cb2, cb3, cb4, wt, xx;
	register int ptr_inc = nbDim * 3;
	const signed char *ptr;

	LSPWEIGHTQUANT_START();

	for ( i=0, ptr=cdbk ; i<nbVec ; i+=4, ptr += ptr_inc )
	{	dt4 = dt3 = dt2 = dt1 = 0;
			
		for ( j=0 ; j<nbDim ; ++j )
		{	wt	  =  weight[j];
			xx	  =  x[j];

			cb1	  =  xx - (ptr[0]		<< 5);
			cb2	  =  xx - (ptr[nbDim]	<< 5);
			cb3	  =  xx - (ptr[nbDim*2] << 5);
			cb4	  =  xx - (ptr[nbDim*3] << 5);

			++ptr;

			cb1	  *= cb1;
			cb2	  *= cb2;
			cb3	  *= cb3;
			cb4	  *= cb4;

			dt1 += (wt * (cb1 >> 15)) + ((wt * (cb1 & 0x7fff)) >> 15);
			dt2 += (wt * (cb2 >> 15)) + ((wt * (cb2 & 0x7fff)) >> 15);
			dt3 += (wt * (cb3 >> 15)) + ((wt * (cb3 & 0x7fff)) >> 15);
			dt4 += (wt * (cb4 >> 15)) + ((wt * (cb4 & 0x7fff)) >> 15);

		}

		if ( dt1<best_dist )
		{	best_dist	=	dt1;
			best_id		=	i;
		}

		if ( dt2<best_dist )
		{	best_dist	=	dt2;
			best_id		=	i+1;
		}
		
		if ( dt3<best_dist )
		{	best_dist	=	dt3;
			best_id		=	i+2;
		}

		if ( dt4<best_dist )
		{	best_dist	=	dt4;
			best_id		=	i+3;
		}
	}

	for ( j=0 ; j<nbDim ; ++j )
	{	x[j] = x[j] - ((Int16)cdbk[best_id*nbDim+j] << 5);
	}

	LSPWEIGHTQUANT_STOP();

	return best_id;
}

#if 0
// TODO: unroll loops
#define OVERRIDE_LSP_QUANT_NB
void lsp_quant_nb(spx_lsp_t *lsp, spx_lsp_t *qlsp, int order, SpeexBits *bits)
{
   int i;
   int id;
   spx_word16_t quant_weight[10];
   
   for (i=0;i<order;i++)
      qlsp[i]=lsp[i];

   compute_quant_weights(qlsp, quant_weight, order);

   for (i=0;i<order;i++)
      qlsp[i]=SUB16(qlsp[i],LSP_LINEAR(i));

#ifndef FIXED_POINT
   for (i=0;i<order;i++)
      qlsp[i] = LSP_SCALE*qlsp[i];
#endif
   id = lsp_quant(qlsp, cdbk_nb, NB_CDBK_SIZE, order);
   speex_bits_pack(bits, id, 6);

   for (i=0;i<order;i++)
      qlsp[i]*=2;
 
   id = lsp_weight_quant(qlsp, quant_weight, cdbk_nb_low1, NB_CDBK_SIZE_LOW1, 5);
   speex_bits_pack(bits, id, 6);

   for (i=0;i<5;i++)
      qlsp[i]*=2;

   id = lsp_weight_quant(qlsp, quant_weight, cdbk_nb_low2, NB_CDBK_SIZE_LOW2, 5);
   speex_bits_pack(bits, id, 6);

   id = lsp_weight_quant(qlsp+5, quant_weight+5, cdbk_nb_high1, NB_CDBK_SIZE_HIGH1, 5);
   speex_bits_pack(bits, id, 6);

   for (i=5;i<10;i++)
      qlsp[i]*=2;

   id = lsp_weight_quant(qlsp+5, quant_weight+5, cdbk_nb_high2, NB_CDBK_SIZE_HIGH2, 5);
   speex_bits_pack(bits, id, 6);

#ifdef FIXED_POINT
   for (i=0;i<order;i++)
      qlsp[i]=PSHR16(qlsp[i],2);
#else
   for (i=0;i<order;i++)
      qlsp[i]=qlsp[i] * .00097656;
#endif

   for (i=0;i<order;i++)
      qlsp[i]=lsp[i]-qlsp[i];
}


#define OVERRIDE_LSP_UNQUANT_NB
void lsp_unquant_nb(spx_lsp_t *lsp, int order, SpeexBits *bits)
{
   int i, id;
   for (i=0;i<order;i++)
      lsp[i]=LSP_LINEAR(i);

   id=speex_bits_unpack_unsigned(bits, 6);
   for (i=0;i<10;i++)
      lsp[i] = ADD32(lsp[i], LSP_DIV_256(cdbk_nb[id*10+i]));

   id=speex_bits_unpack_unsigned(bits, 6);
   for (i=0;i<5;i++)
      lsp[i] = ADD16(lsp[i], LSP_DIV_512(cdbk_nb_low1[id*5+i]));

   id=speex_bits_unpack_unsigned(bits, 6);
   for (i=0;i<5;i++)
      lsp[i] = ADD32(lsp[i], LSP_DIV_1024(cdbk_nb_low2[id*5+i]));

   id=speex_bits_unpack_unsigned(bits, 6);
   for (i=0;i<5;i++)
      lsp[i+5] = ADD32(lsp[i+5], LSP_DIV_512(cdbk_nb_high1[id*5+i]));
   
   id=speex_bits_unpack_unsigned(bits, 6);
   for (i=0;i<5;i++)
      lsp[i+5] = ADD32(lsp[i+5], LSP_DIV_1024(cdbk_nb_high2[id*5+i]));
}

#define OVERRIDE_LSP_QUANT_LBR
void lsp_quant_lbr(spx_lsp_t *lsp, spx_lsp_t *qlsp, int order, SpeexBits *bits)
{
   int i;
   int id;
   spx_word16_t quant_weight[10];

   for (i=0;i<order;i++)
      qlsp[i]=lsp[i];

   compute_quant_weights(qlsp, quant_weight, order);

   for (i=0;i<order;i++)
      qlsp[i]=SUB16(qlsp[i],LSP_LINEAR(i));
#ifndef FIXED_POINT
   for (i=0;i<order;i++)
      qlsp[i]=qlsp[i]*LSP_SCALE;
#endif
   id = lsp_quant(qlsp, cdbk_nb, NB_CDBK_SIZE, order);
   speex_bits_pack(bits, id, 6);
   
   for (i=0;i<order;i++)
      qlsp[i]*=2;
   
   id = lsp_weight_quant(qlsp, quant_weight, cdbk_nb_low1, NB_CDBK_SIZE_LOW1, 5);
   speex_bits_pack(bits, id, 6);

   id = lsp_weight_quant(qlsp+5, quant_weight+5, cdbk_nb_high1, NB_CDBK_SIZE_HIGH1, 5);
   speex_bits_pack(bits, id, 6);

#ifdef FIXED_POINT
   for (i=0;i<order;i++)
      qlsp[i] = PSHR16(qlsp[i],1);
#else
   for (i=0;i<order;i++)
      qlsp[i] = qlsp[i]*0.0019531;
#endif

   for (i=0;i<order;i++)
      qlsp[i]=lsp[i]-qlsp[i];
}

#define OVERRIDE_LSP_UNQUANT_LBR
void lsp_unquant_lbr(spx_lsp_t *lsp, int order, SpeexBits *bits)
{
   int i, id;
   for (i=0;i<order;i++)
      lsp[i]=LSP_LINEAR(i);


   id=speex_bits_unpack_unsigned(bits, 6);
   for (i=0;i<10;i++)
      lsp[i] += LSP_DIV_256(cdbk_nb[id*10+i]);

   id=speex_bits_unpack_unsigned(bits, 6);
   for (i=0;i<5;i++)
      lsp[i] += LSP_DIV_512(cdbk_nb_low1[id*5+i]);

   id=speex_bits_unpack_unsigned(bits, 6);
   for (i=0;i<5;i++)
      lsp[i+5] += LSP_DIV_512(cdbk_nb_high1[id*5+i]);
   
}

extern const signed char high_lsp_cdbk[];
extern const signed char high_lsp_cdbk2[];

#define OVERRIDE_LSP_UNQUANT_HIGH
void lsp_unquant_high(spx_lsp_t *lsp, int order, SpeexBits *bits)
{

   int i, id;
   for (i=0;i<order;i++)
      lsp[i]=LSP_LINEAR_HIGH(i);


   id=speex_bits_unpack_unsigned(bits, 6);
   for (i=0;i<order;i++)
      lsp[i] += LSP_DIV_256(high_lsp_cdbk[id*order+i]);


   id=speex_bits_unpack_unsigned(bits, 6);
   for (i=0;i<order;i++)
      lsp[i] += LSP_DIV_512(high_lsp_cdbk2[id*order+i]);
}

#define OVERRIDE_LSP_QUANT_HIGH
void lsp_quant_high(spx_lsp_t *lsp, spx_lsp_t *qlsp, int order, SpeexBits *bits)
{
   int i;
   int id;
   spx_word16_t quant_weight[10];

   for (i=0;i<order;i++)
      qlsp[i]=lsp[i];

   compute_quant_weights(qlsp, quant_weight, order);

   /*   quant_weight[0] = 10/(qlsp[1]-qlsp[0]);
   quant_weight[order-1] = 10/(qlsp[order-1]-qlsp[order-2]);
   for (i=1;i<order-1;i++)
   {
      tmp1 = 10/(qlsp[i]-qlsp[i-1]);
      tmp2 = 10/(qlsp[i+1]-qlsp[i]);
      quant_weight[i] = tmp1 > tmp2 ? tmp1 : tmp2;
      }*/

   for (i=0;i<order;i++)
      qlsp[i]=SUB16(qlsp[i],LSP_LINEAR_HIGH(i));
#ifndef FIXED_POINT
   for (i=0;i<order;i++)
      qlsp[i] = qlsp[i]*LSP_SCALE;
#endif
   id = lsp_quant(qlsp, high_lsp_cdbk, 64, order);
   speex_bits_pack(bits, id, 6);

   for (i=0;i<order;i++)
      qlsp[i]*=2;

   id = lsp_weight_quant(qlsp, quant_weight, high_lsp_cdbk2, 64, order);
   speex_bits_pack(bits, id, 6);

#ifdef FIXED_POINT
   for (i=0;i<order;i++)
      qlsp[i] = PSHR16(qlsp[i],1);
#else
   for (i=0;i<order;i++)
      qlsp[i] = qlsp[i]*0.0019531;
#endif

   for (i=0;i<order;i++)
      qlsp[i]=lsp[i]-qlsp[i];
}
#endif

#endif
