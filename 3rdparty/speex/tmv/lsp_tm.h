/* Copyright (C) 2007 Hong Zhiqian */
/**
   @file lsp_tm.h
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

#define OVERRIDE_LSP_INTERPOLATE
void lsp_interpolate(Int16 *old_lsp, Int16 *new_lsp, Int16 *interp_lsp, int len, int subframe, int nb_subframes)
{
	register int tmp  =  DIV32_16(SHL32(EXTEND32(1 + subframe),14),nb_subframes);
	register int tmp2 =  16384-tmp;
	register int in_0, in_1, factor, out_1, out_2, olsp, nlsp, ilsp;
	register int i;

	TMDEBUG_ALIGNMEM(old_lsp);
	TMDEBUG_ALIGNMEM(new_lsp);
	TMDEBUG_ALIGNMEM(interp_lsp);

	LSPINTERPOLATE_START();

	factor = pack16lsb(tmp,tmp2);

	len >>= 1;
	for ( i=0 ; i<len ; ++i )
	{	
		olsp = ld32x(old_lsp,i);							// olsp[i+1],olsp[i]
		nlsp = ld32x(new_lsp,i);							// nlsp[i+1],nlsp[i]

		in_0 = pack16lsb(nlsp,olsp);
		in_1 = pack16msb(nlsp,olsp);
	  
		out_1 = 8192 + ifir16(in_0,factor);
		out_2 = 8192 + ifir16(in_1,factor);

		ilsp = pack16lsb(out_2 >> 14, out_1 >> 14);
		st32d(i << 2, interp_lsp, ilsp);

	}

	LSPINTERPOLATE_STOP();
}


#define OVERRIDE_CHEB_POLY_EVA
static inline Int32 cheb_poly_eva(Int16 *coef, Int16 x, int m, char *stack)
{
	register int	c10, c32, c54;
    register int	sum, b0, f0, f1, f2, f3;
	register int	xx, f32, f10;

	CHEBPOLYEVA_START();

	xx = sex16(x);
	b0 = iclipi(xx,16383);

#if 0
	c10 = ld32(coef);
	c32 = ld32x(coef,1);
	c54 = ld32x(coef,2);
#else	
	c10 = pack16lsb(coef[1],coef[0]);
	c32 = pack16lsb(coef[3],coef[2]);
	c54 = pack16lsb(coef[5],coef[4]);
#endif

	f0   = ((xx * b0) >> 13) - 16384;
	f1   = ((xx * f0) >> 13) - b0;
	f2   = ((xx * f1) >> 13) - f0;

	if ( m == 4 )
	{	sum = sex16(c54);
		f32 = pack16lsb(xx,f0);
		f10 = pack16lsb(f1,f2);

	} else
	{	sum =  asri(16,c54);
		sum += ((sex16(c54) * xx) + 8192) >> 14;

		f3   = ((xx * f2) >> 13) - f1;
		f32 = pack16lsb(f0,f1);
		f10 = pack16lsb(f2,f3);
	}

	sum += (ifir16(c32,f32) + 8192) >> 14;
	sum += (ifir16(c10,f10) + 8192) >> 14;

#ifndef REMARK_ON
   (void)stack;
#endif

	CHEBPOLYEVA_STOP();
    return sum;
}


#define OVERRIDE_LSP_ENFORCE_MARGIN
void lsp_enforce_margin(Int16 *lsp, int len, Int16 margin)
{
	register int i;
	register int m = margin;
	register int m2 = 25736-margin;
	register int lsp0, lsp1, lsp2;

	TMDEBUG_ALIGNMEM(lsp);

	LSPENFORCEMARGIN_START();

	lsp0 = ld32(lsp);
	lsp1 = asri(16,lsp0);
	lsp0 = sex16(lsp0);
	lsp2 = lsp[len-1];

	if ( lsp0 < m )
	{	lsp0 = m;
		lsp[0] = m;
	}

	if ( lsp2 > m2 )
	{	lsp2 = m2;
		lsp[len-1] = m2;
	}

	for ( i=1 ; i<len-1 ; ++i )
	{	
		lsp0 += m;
		lsp2 = lsp[i+1];
		m2   = lsp2 - m;

		if ( lsp1 < lsp0 )
		{	lsp1   = lsp0;
			lsp[i] = lsp0;
		}

		if ( lsp1 > m2 )
		{	lsp1	= (lsp1 >> 1) + (m2 >> 1);
			lsp[i]	= lsp1;			
		}

		lsp0 = lsp1;
		lsp1 = lsp2;
	}

	LSPENFORCEMARGIN_STOP();
}


#define OVERRIDE_LSP_TO_LPC
void lsp_to_lpc(Int16 *freq, Int16 *ak,int lpcrdr, char *stack)
{
    VARDECL(Int16 *freqn);
    VARDECL(int **xp);
    VARDECL(int *xpmem);
    VARDECL(int **xq);
    VARDECL(int *xqmem);

    register int i, j, k;
    register int xout1,xout2,xin;
    register int m;
    
	LSPTOLPC_START();

	m = lpcrdr>>1;

	/* 
    
       Reconstruct P(z) and Q(z) by cascading second order polynomials
       in form 1 - 2cos(w)z(-1) + z(-2), where w is the LSP frequency.
       In the time domain this is:

       y(n) = x(n) - 2cos(w)x(n-1) + x(n-2)
    
       This is what the ALLOCS below are trying to do:

         int xp[m+1][lpcrdr+1+2]; // P matrix in QIMP
         int xq[m+1][lpcrdr+1+2]; // Q matrix in QIMP

       These matrices store the output of each stage on each row.  The
       final (m-th) row has the output of the final (m-th) cascaded
       2nd order filter.  The first row is the impulse input to the
       system (not written as it is known).

       The version below takes advantage of the fact that a lot of the
       outputs are zero or known, for example if we put an inpulse
       into the first section the "clock" it 10 times only the first 3
       outputs samples are non-zero (it's an FIR filter).
    */

    ALLOC(xp, (m+1), int*);
    ALLOC(xpmem, (m+1)*(lpcrdr+1+2), int);

    ALLOC(xq, (m+1), int*);
    ALLOC(xqmem, (m+1)*(lpcrdr+1+2), int);
    
    for ( i=0; i<=m; i++ ) 
	{	xp[i] = xpmem + i*(lpcrdr+1+2);
		xq[i] = xqmem + i*(lpcrdr+1+2);
    }

    /* work out 2cos terms in Q14 */

    ALLOC(freqn, lpcrdr, Int16);
    for ( j=0,k=0 ; j<m ; ++j,k+=2 )
	{	register int f;

		f = ld32x(freq,j);

		freqn[k] = ANGLE2X(sex16(f));
		freqn[k+1] = ANGLE2X(asri(16,f));
	}
    
	#define QIMP  21   /* scaling for impulse */
    xin = SHL32(EXTEND32(1), (QIMP-1)); /* 0.5 in QIMP format */
   
    /* first col and last non-zero values of each row are trivial */
    
    for(i=0;i<=m;i++) 
	{	xp[i][1] = 0;
		xp[i][2] = xin;
		xp[i][2+2*i] = xin;
		xq[i][1] = 0;
		xq[i][2] = xin;
		xq[i][2+2*i] = xin;
    }

    /* 2nd row (first output row) is trivial */

    xp[1][3] = -MULT16_32_Q14(freqn[0],xp[0][2]);
    xq[1][3] = -MULT16_32_Q14(freqn[1],xq[0][2]);

    xout1 = xout2 = 0;

    for( i=1 ; i<m ; ++i) 
	{	register int f, f0, f1, m0, m1;
		
		k  = 2*(i+1)-1;
		f  = ld32x(freqn,i);
		f0 = sex16(f);
		f1 = asri(16,f);

		for( j=1 ; j<k ; ++j) 
		{	register int _m0, _m1;	

			_m0 = MULT16_32_Q14(f0,xp[i][j+1]);
			xp[i+1][j+2] = ADD32(SUB32(xp[i][j+2], _m0), xp[i][j]);
	
			_m1 = MULT16_32_Q14(f1,xq[i][j+1]);
			xq[i+1][j+2] = ADD32(SUB32(xq[i][j+2], _m1), xq[i][j]);
		}

      
		m0 = MULT16_32_Q14(f0,xp[i][j+1]);
		xp[i+1][j+2] = SUB32(xp[i][j], m0);
		m1 = MULT16_32_Q14(f1,xq[i][j+1]);
		xq[i+1][j+2] = SUB32(xq[i][j], m1);
    }


    for( i=0,j=3 ; i<lpcrdr ; ++j,++i ) 
	{	register int _a0, _xp0, _xq0;

		_xp0 = xp[m][j];
		_xq0 = xq[m][j];

      	_a0 = PSHR32(_xp0 + xout1 + _xq0 - xout2, QIMP-13); 
		xout1 = _xp0;
		xout2 = _xq0;
     
		ak[i] = iclipi(_a0,32767); 
    }

	LSPTOLPC_STOP();
}


#endif
