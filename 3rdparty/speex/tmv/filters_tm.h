#include <ops/custom_defs.h>
#include "profile_tm.h"

#ifdef FIXED_POINT

#define iadd(a,b)	((a) + (b))

#define OVERRIDE_BW_LPC
void bw_lpc(Int16 gamma, const Int16 *lpc_in, Int16 *lpc_out, int order)
{
	register int tmp, g, i;
	
	TMDEBUG_ALIGNMEM(lpc_in);
	TMDEBUG_ALIGNMEM(lpc_out);

	BWLPC_START();

	tmp = g = gamma;
	for ( i=0 ; i<4 ; i+=2,lpc_out+=4 )
	{	register int in10, y1, y0, y10;
		register int in32, y3, y2, y32;

		in10 = ld32x(lpc_in,i);
		y0	 = ((tmp * sex16(in10)) + 16384) >> 15;
		tmp  = ((tmp * g) + 16384) >> 15;
		y1	 = ((tmp * asri(16,in10)) + 16384) >> 15;
		tmp  = ((tmp * g) + 16384) >> 15;
		y10  =  pack16lsb(y1,y0);
		st32(lpc_out,y10);

		in32 = ld32x(lpc_in,i+1);
		y2	 = ((tmp * sex16(in32)) + 16384) >> 15;
		tmp  = ((tmp * g) + 16384) >> 15;
		y3	 = ((tmp * asri(16,in32)) + 16384) >> 15;
		tmp  = ((tmp * g) + 16384) >> 15;
		y32  =  pack16lsb(y3,y2);
		st32d(4,lpc_out,y32);
	}

	if ( order == 10 )
	{	register int in10, y1, y0, y10;
		
		in10 = ld32x(lpc_in,i);
		y0	 = ((tmp * sex16(in10)) + 16384) >> 15;
		tmp  = ((tmp * g) + 16384) >> 15;
		y1	 = ((tmp * asri(16,in10)) + 16384) >> 15;
		tmp  = ((tmp * g) + 16384) >> 15;
		y10  =  pack16lsb(y1,y0);
		st32(lpc_out,y10);
	}

	BWLPC_STOP();
}


#define OVERRIDE_HIGHPASS
void highpass(const Int16 *x, Int16 *y, int len, int filtID, Int32 *mem)
{
   	const Int16 Pcoef[5][3] = {{16384, -31313, 14991}, {16384, -31569, 15249}, {16384, -31677, 15328}, {16384, -32313, 15947}, {16384, -22446, 6537}};
	const Int16 Zcoef[5][3] = {{15672, -31344, 15672}, {15802, -31601, 15802}, {15847, -31694, 15847}, {16162, -32322, 16162}, {14418, -28836, 14418}};
	register int i;
	register int den1, den2, num0, num1, num2, den11, den22, mem0, mem1;
   
	TMDEBUG_ALIGNMEM(mem);

	HIGHPASS_START();

	filtID = imin(4, filtID);

	den1 = -Pcoef[filtID][1];
	den2 = -Pcoef[filtID][2];
	num0 = Zcoef[filtID][0];
	num1 = Zcoef[filtID][1];
	num2 = Zcoef[filtID][2];
	den11 = den1 << 1;
	den22 = den2 << 1;
	mem0 = mem[0];
	mem1 = mem[1];
	
#if (TM_UNROLL && TM_UNROLL_HIGHPASS)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
	for ( i=0 ; i<len ; ++i )
	{
		register int yi;
		register int vout, xi, vout_i, vout_d;
			
		xi = x[i];

		vout	= num0 * xi + mem0;
		vout_i	= vout >> 15;
		vout_d	= vout & 0x7FFF;
		yi		= iclipi(PSHR32(vout,14),32767);
		mem0	= (mem1 + num1 * xi) + (den11 * vout_i) + (((den1 * vout_d) >> 15) << 1);
		mem1	= (num2 * xi) + (den22 * vout_i) + (((den2 *  vout_d) >> 15) << 1);
		
		y[i] = yi;
	}
#if (TM_UNROLL && TM_UNROLL_HIGHPASS)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif

	mem[0] = mem0;
	mem[1] = mem1;

	HIGHPASS_STOP();
}


#define OVERRIDE_SIGNALMUL
void signal_mul(const Int32 *x, Int32 *y, Int32 scale, int len)
{
	register int i, scale_i, scale_d;
   
	SIGNALMUL_START();

	scale_i = scale >> 14;
	scale_d = scale & 0x3FFF;

#if (TM_UNROLL && TM_UNROLL_SIGNALMUL)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
	for ( i=0 ; i<len ; ++i)
	{
		register int xi;

		xi = x[i] >> 7;

		y[i] = ((xi * scale_i + ((xi * scale_d) >> 14)) << 7);
			
	}
#if (TM_UNROLL && TM_UNROLL_SIGNALMUL)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif

	SIGNALMUL_STOP();
}

#define OVERRIDE_SIGNALDIV
void signal_div(const Int16 *x, Int16 *y, Int32 scale, int len)
{
	register int i;
   
	SIGNALDIV_START();

	if (scale > SHL32(EXTEND32(SIG_SCALING), 8))
	{
		register int scale_1;
		scale	= PSHR32(scale, SIG_SHIFT);
		scale_1 = DIV32_16(SHL32(EXTEND32(SIG_SCALING),7),scale);
#if (TM_UNROLL && TM_UNROLL_SIGNALDIV)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
		for ( i=0 ; i<len ; ++i)
		{
			y[i] = MULT16_16_P15(scale_1, x[i]);
		}
#if (TM_UNROLL && TM_UNROLL_SIGNALDIV)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
	} else if (scale > SHR32(EXTEND32(SIG_SCALING), 2)) {
      
		register int scale_1;
		scale	= PSHR32(scale, SIG_SHIFT-5);
		scale_1 = DIV32_16(SHL32(EXTEND32(SIG_SCALING),3),scale);
#if (TM_UNROLL && TM_UNROLL_SIGNALDIV)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
		for (i=0;i<len;i++)
		{
			y[i] = PSHR32(MULT16_16(scale_1, SHL16(x[i],2)),8);
		}
#if (TM_UNROLL && TM_UNROLL_SIGNALDIV)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
	} else {
		
		register int scale_1;
		scale = PSHR32(scale, SIG_SHIFT-7);
		scale = imax(5,scale);
		scale_1 = DIV32_16(SHL32(EXTEND32(SIG_SCALING),3),scale);

#if (TM_UNROLL && TM_UNROLL_SIGNALDIV)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
		for (i=0;i<len;i++)
		{
			 y[i] = PSHR32(MULT16_16(scale_1, SHL16(x[i],2)),6);
		}
#if (TM_UNROLL && TM_UNROLL_SIGNALDIV)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
	}

	SIGNALMUL_STOP();
}


#define OVERRIDE_COMPUTE_RMS
Int16 compute_rms(const Int32 *x, int len)
{
	register int i;
	register int sum=0;
	register int max_val=1;
	register int sig_shift;
	
	TMDEBUG_ALIGNMEM(x);

	for ( i=0 ; i<len ; i+=4 )
	{
		register int tmp0, tmp1, tmp2, tmp3;

		tmp0 = ld32x(x,i);
		tmp1 = ld32x(x,i+1);

		tmp0 = iabs(tmp0);
		tmp1 = iabs(tmp1);

		tmp2 = ld32x(x,i+2);
		tmp3 = ld32x(x,i+3);

		tmp2 = iabs(tmp2);
		tmp3 = iabs(tmp3);

		tmp0 = imax(tmp0, tmp1);
		max_val = imax(max_val, tmp0); 
		tmp2 = imax(tmp2, tmp3);		
		max_val = imax(max_val, tmp2);
	}

	sig_shift = 0;
	while ( max_val>16383 )
	{	sig_shift++;
		max_val >>= 1;
	}


	for ( i=0 ; i<len ; i+=4 )
	{
		register int acc0, acc1, acc2;

		acc0 =  pack16lsb(ld32x(x,i) >> sig_shift, ld32x(x,i+1) >> sig_shift);
		acc1 =  pack16lsb(ld32x(x,i+2) >> sig_shift, ld32x(x,i+3) >> sig_shift);
		acc2 =  ifir16(acc0,acc0) + ifir16(acc1, acc1);
		sum	 += acc2 >> 6;
	}
   
   return EXTRACT16(PSHR32(SHL32(EXTEND32(spx_sqrt(DIV32(sum,len))),(sig_shift+3)),SIG_SHIFT));
}

#define OVERRIDE_COMPUTE_RMS16
Int16 compute_rms16(const Int16 *x, int len)
{
	register int max_val, i;
	
	COMPUTERMS16_START();
	
	max_val = 10;

#if 0

	{
		register int len2 = len >> 1;
#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif
		for ( i=0 ; i<len2 ; i+=2 )
		{
			register int x10, x32;

			x10 = ld32x(x,i);
			x32 = ld32x(x,i+1);
			 
			x10 = dspidualabs(x10);
			x32 = dspidualabs(x32);
			
			x10 = imax(sex16(x10), asri(16,x10));
			x32 = imax(sex16(x32), asri(16,x32));
			
			max_val = imax(max_val,x10);
			max_val = imax(max_val,x32);
		}
#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
		if (max_val>16383)
		{
			register int sum = 0;
#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif
			for ( i=0 ; i<len_2; i+=2 )
			{
				register int x10, x32;
				register int acc0, acc1;

				x10 = ld32x(x,i);
				x32 = ld32x(x,i+1);
				
				x10 = dualasr(x10,1);
				x32 = dualasr(x32,1);

				acc0 = ifir16(x10,x10);
				acc1 = ifir16(x32,x32);
				sum  += (acc0 + acc1) >> 6;
			}
#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
			COMPUTERMS16_STOP();
			return spx_sqrt(sum/len) << 4;
		} else 
		{
			register int sig_shift;
			register int sum=0;

			sig_shift = mux(max_val < 8192, 1, 0);
			sig_shift = mux(max_val < 4096, 2, sig_shift);
			sig_shift = mux(max_val < 2048, 3, sig_shift);

#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif
			for ( i=0 ; i<len_2 ; i+=2 )
			{
				register int x10, x32;
				register int acc0, acc1;

				x10 = ld32x(x,i);
				x32 = ld32x(x,i+1);

				x10 = dualasl(x10,sig_shift);
				x32 = dualasl(x32,sig_shift);
				
				acc0 = ifir16(x10,x10);
				acc1 = ifir16(x32,x32);
				sum  += (acc0 + acc1) >> 6;
			}
#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
			COMPUTERMS16_STOP();
			return spx_sqrt(sum/len) << (3 - sig_shift);  
		}
	}

#else
	{
#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
		for ( i=0 ; i<len ; ++i )
		{
			register int xi;

			xi = x[i];
			xi = iabs(xi);
			max_val = imax(xi,max_val);
		}
#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
		if (max_val>16383)
		{
			register int sum = 0;
#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif
			for ( i=0 ; i<len ; i+=4 )
			{
				register int x10, x32;
				register int acc0, acc1;

				x10 = pack16lsb(x[i+1],x[i]);
				x32 = pack16lsb(x[i+3],x[i+2]);
				
				x10 = dualasr(x10,1);
				x32 = dualasr(x32,1);

				acc0 = ifir16(x10,x10);
				acc1 = ifir16(x32,x32);
				sum  += (acc0 + acc1) >> 6;
			}
#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
			COMPUTERMS16_STOP();
			return spx_sqrt(sum/len) << 4;
		} else {
			register int sig_shift;
			register int sum=0;

			sig_shift = mux(max_val < 8192, 1, 0);
			sig_shift = mux(max_val < 4096, 2, sig_shift);
			sig_shift = mux(max_val < 2048, 3, sig_shift);

#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif
			for ( i=0 ; i<len ; i+=4 )
			{
				register int x10, x32;
				register int acc0, acc1;

				x10 = pack16lsb(x[i+1],x[i]);
				x32 = pack16lsb(x[i+3],x[i+2]);

				x10 = dualasl(x10,sig_shift);
				x32 = dualasl(x32,sig_shift);
				
				acc0 = ifir16(x10,x10);
				acc1 = ifir16(x32,x32);
				sum  += (acc0 + acc1) >> 6;
			}
#if (TM_UNROLL && TM_UNROLL_COMPUTERMS16)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
			COMPUTERMS16_STOP();
			return spx_sqrt(sum/len) << (3 - sig_shift);  
		}
	}
#endif
}

int normalize16_9(const Int32* restrict x, Int16 * restrict y, Int32 max_scale)
{
	register int x0, x1, x2, x3, x4, x5, x6, x7, x8;
	register int max_val, m0, m1, m2, m3, m4;
	register int sig_shift;
	register int y10, y32, y54, y76;

	TMDEBUG_ALIGNMEM(x);

	x0 = ld32(x); 
	x1 = ld32x(x,1); x2 = ld32x(x,2); x3 = ld32x(x,3); x4 = ld32x(x,4); 
	x5 = ld32x(x,5); x6 = ld32x(x,6); x7 = ld32x(x,7); x8 = ld32x(x,8);

	m0 = imax(iabs(x0), iabs(x1));
	m1 = imax(iabs(x2), iabs(x3));
	m2 = imax(iabs(x4), iabs(x5));
	m3 = imax(iabs(x6), iabs(x7));
	m4 = imax(m0, iabs(x8));
	m1 = imax(m1, m2);
	m3 = imax(m3, m4);
	max_val = imax(1,imax(m1,m3));

	sig_shift=0;
	while (max_val>max_scale)
	{	sig_shift++;
		max_val >>= 1;
	}

	if ( sig_shift != 0 )
	{
		y10 = pack16lsb(x1 >> sig_shift, x0 >> sig_shift);
		y32 = pack16lsb(x3 >> sig_shift, x2 >> sig_shift);
		y54 = pack16lsb(x5 >> sig_shift, x4 >> sig_shift);
		y76 = pack16lsb(x7 >> sig_shift, x6 >> sig_shift);

		y[8] = x8 >> sig_shift;
		st32(y,y10);
		st32d(4,y,y32);
		st32d(8,y,y54);
		st32d(12,y,y76);
	}
	return sig_shift;
}

int normalize16_mod8(const Int32 * restrict x, Int16 * restrict y, Int32 max_scale,int len)
{
	register int i, max_val, sig_shift;
	
	TMDEBUG_ALIGNMEM(x);

	max_val = 1;

	for ( i=0 ; i<len ; i+=4 )
	{
		register int tmp0, tmp1, tmp2, tmp3;

		tmp0 = ld32x(x,i);
		tmp1 = ld32x(x,i+1);

		tmp0 = iabs(tmp0);
		tmp1 = iabs(tmp1);

		tmp2 = ld32x(x,i+2);
		tmp3 = ld32x(x,i+3);

		tmp2 = iabs(tmp2);
		tmp3 = iabs(tmp3);

		tmp0 = imax(tmp0, tmp1);
		max_val = imax(max_val, tmp0); 
		tmp2 = imax(tmp2, tmp3);		
		max_val = imax(max_val, tmp2);
	}

	sig_shift=0;
	while (max_val>max_scale)
	{	sig_shift++;
		max_val >>= 1;
	}

	if ( sig_shift != 0 )
	{
		for ( i=0 ; i<len ; i+=8, y+=8 )
		{
			register int x0, x1, x2, x3, x4, x5, x6, x7;
			register int y10, y32, y54, y76;

			x0 = ld32x(x,i);   x1 = ld32x(x,i+1); x2 = ld32x(x,i+2); x3 = ld32x(x,i+3); 
			x4 = ld32x(x,i+4); x5 = ld32x(x,i+5); x6 = ld32x(x,i+6); x7 = ld32x(x,i+7);

			y10 = pack16lsb(x1 >> sig_shift, x0 >> sig_shift);
			y32 = pack16lsb(x3 >> sig_shift, x2 >> sig_shift);
			y54 = pack16lsb(x5 >> sig_shift, x4 >> sig_shift);
			y76 = pack16lsb(x7 >> sig_shift, x6 >> sig_shift);

			st32(y,y10);
			st32d(4,y,y32);
			st32d(8,y,y54);
			st32d(12,y,y76);
		}
	}
	return sig_shift;
}


#define OVERRIDE_NORMALIZE16
int normalize16(const Int32 *x, Int16 *y, Int32 max_scale, int len)
{
	TMDEBUG_ALIGNMEM(x);
	TMDEBUG_ALIGNMEM(y);

	NORMALIZE16_START();

	if ( len == 9 )
	{	NORMALIZE16_STOP();
		return normalize16_9(x,y,max_scale);
	} else
	{	NORMALIZE16_STOP();
		return normalize16_mod8(x,y,max_scale,len);
	}
}


void filter_mem16_10(const Int16 *x, const Int16 *num, const Int16 *den, Int16 *y, int N, Int32 *mem)
{
	register int i;
	register int c9, c8, c7, c6, c5;
	register int c4, c3, c2, c1, c0;
	register int input;
	register int output_0, output_1, output_2, output_3, output_4;
	register int output_5, output_6, output_7, output_8, output_9;
    register Int16 xi, yi;

	c9 = pack16lsb(-den[9],num[9]);
	c8 = pack16lsb(-den[8],num[8]);
	c7 = pack16lsb(-den[7],num[7]);
	c6 = pack16lsb(-den[6],num[6]);
	c5 = pack16lsb(-den[5],num[5]);
	c4 = pack16lsb(-den[4],num[4]);
	c3 = pack16lsb(-den[3],num[3]);
	c2 = pack16lsb(-den[2],num[2]);
	c1 = pack16lsb(-den[1],num[1]);
	c0 = pack16lsb(-den[0],num[0]);

	output_0 = mem[0];
	output_1 = mem[1];
	output_2 = mem[2];
	output_3 = mem[3];
	output_4 = mem[4];
	output_5 = mem[5];
	output_6 = mem[6];
	output_7 = mem[7];
	output_8 = mem[8];
	output_9 = mem[9];

#if (TM_UNROLL && TM_UNROLL_FILTER)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif

	for ( i=0 ; i<N ; i++ )
    {
		xi = (int)(x[i]);
		yi = iclipi(iadd(xi,PSHR32(output_0,LPC_SHIFT)),32767);

		input	= pack16lsb(yi,xi);
		output_0= iadd(ifir16(c0,input),output_1);
		output_1= iadd(ifir16(c1,input),output_2);
		output_2= iadd(ifir16(c2,input),output_3);
		output_3= iadd(ifir16(c3,input),output_4);
		output_4= iadd(ifir16(c4,input),output_5);
		output_5= iadd(ifir16(c5,input),output_6);
		output_6= iadd(ifir16(c6,input),output_7);
		output_7= iadd(ifir16(c7,input),output_8);
		output_8= iadd(ifir16(c8,input),output_9);
		output_9= ifir16(c9,input);

		y[i] = yi;
   }

#if (TM_UNROLL && TM_UNROLL_FILTER)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif

	mem[0] = output_0;
	mem[1] = output_1;
	mem[2] = output_2;
	mem[3] = output_3;
	mem[4] = output_4;
	mem[5] = output_5;
	mem[6] = output_6;
	mem[7] = output_7;
	mem[8] = output_8;
	mem[9] = output_9;
}

void filter_mem16_8(const Int16 *x, const Int16 *num, const Int16 *den, Int16 *y, int N, Int32 *mem)
{
	register int i;
	register int c7, c6, c5, c4, c3, c2, c1, c0;
	register int output_0, output_1, output_2, output_3, output_4, output_5, output_6, output_7;
	register int input;
    register Int16 xi, yi;

	c7 = pack16lsb(-den[7],num[7]);
	c6 = pack16lsb(-den[6],num[6]);
	c5 = pack16lsb(-den[5],num[5]);
	c4 = pack16lsb(-den[4],num[4]);
	c3 = pack16lsb(-den[3],num[3]);
	c2 = pack16lsb(-den[2],num[2]);
	c1 = pack16lsb(-den[1],num[1]);
	c0 = pack16lsb(-den[0],num[0]);

	output_0 = mem[0];
	output_1 = mem[1];
	output_2 = mem[2];
	output_3 = mem[3];
	output_4 = mem[4];
	output_5 = mem[5];
	output_6 = mem[6];
	output_7 = mem[7];

#if (TM_UNROLL && TM_UNROLL_FILTER)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif

	for ( i=0 ; i<N ; i++ )
    {
		xi = x[i];
		yi = iclipi(iadd((int)(xi),PSHR32(output_0,LPC_SHIFT)),32767);

		input	= pack16lsb(yi,xi);
		output_0= iadd(ifir16(c0,input),output_1);
		output_1= iadd(ifir16(c1,input),output_2);
		output_2= iadd(ifir16(c2,input),output_3);
		output_3= iadd(ifir16(c3,input),output_4);
		output_4= iadd(ifir16(c4,input),output_5);
		output_5= iadd(ifir16(c5,input),output_6);
		output_6= iadd(ifir16(c6,input),output_7);
		output_7= ifir16(c7,input);

		y[i] = yi;
   }

#if (TM_UNROLL && TM_UNROLL_FILTER)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif


	mem[0] = output_0;
	mem[1] = output_1;
	mem[2] = output_2;
	mem[3] = output_3;
	mem[4] = output_4;
	mem[5] = output_5;
	mem[6] = output_6;
	mem[7] = output_7;
}

#define OVERRIDE_FILTER_MEM16
void filter_mem16(const Int16 *x, const Int16 *num, const Int16 *den, Int16 *y, int N, int ord, Int32 *mem, char *stack)
{
	TMDEBUG_ALIGNMEM(x);
	TMDEBUG_ALIGNMEM(y);
	TMDEBUG_ALIGNMEM(num);
	TMDEBUG_ALIGNMEM(den);

	FILTERMEM16_START();

	if(ord==10)
		filter_mem16_10(x, num, den, y, N, mem);
	else if (ord==8)
		filter_mem16_8(x, num, den, y, N, mem);

#ifndef REMARK_ON
	(void)stack;
#endif

	FILTERMEM16_STOP();
}

void iir_mem16_8(const Int16 *x, const Int16 *den, Int16 *y, int N, Int32 *mem)
{
	register int i;
	register int c67, c45, c23, c01;
	register int r1, r2, r3;
	register int y10, y32, y54, y76, yi;
	
	c67 = pack16lsb(-den[6],-den[7]);
	c45 = pack16lsb(-den[4],-den[5]);
	c23 = pack16lsb(-den[2],-den[3]);
	c01 = pack16lsb(-den[0],-den[1]);

	y10 = mem[0];
	y32 = mem[1];
	y54 = mem[2];
	y76 = mem[3];

#if (TM_UNROLL && TM_UNROLL_IIR)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif

	for ( i=0 ; i < N ; ++i )
	{		
		r2 = iadd(ifir16(y10,c67),ifir16(y32,c45));
		r3 = iadd(ifir16(y54,c23),ifir16(y76,c01));
		r1 = iadd(r2,r3);

		y10 = funshift2(y32, y10);
		y32 = funshift2(y54, y32);
		y54 = funshift2(y76, y54);
		
		yi	= iclipi(iadd((int)(x[i]),PSHR32(r1,LPC_SHIFT)),32767);
		y[i]= yi;
		y76 = funshift2(yi, y76);
	}

#if (TM_UNROLL && TM_UNROLL_IIR)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif

	mem[0] = y10;
	mem[1] = y32;
	mem[2] = y54;
	mem[3] = y76;

}

void iir_mem16_10(const Int16 *x, const Int16 *den, Int16 *y, int N, Int32 *mem)
{
	register int i;
	register int c89, c67, c45, c23, c01;
	register int r1, r2, r3, r4, r5;
	register int y10, y32, y54, y76, y98, yi;
	
	c89 = pack16lsb(-den[8],-den[9]);
	c67 = pack16lsb(-den[6],-den[7]);
	c45 = pack16lsb(-den[4],-den[5]);
	c23 = pack16lsb(-den[2],-den[3]);
	c01 = pack16lsb(-den[0],-den[1]);

	y10 = mem[0];
	y32 = mem[1];
	y54 = mem[2];
	y76 = mem[3];
	y98 = mem[4];

#if (TM_UNROLL && TM_UNROLL_IIR)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif

	for ( i=0 ; i < N ; ++i )
	{		
		r2 = iadd(ifir16(y10,c89),ifir16(y32,c67));
		r3 = iadd(ifir16(y54,c45),ifir16(y76,c23));
		r4 = ifir16(y98,c01);
		r5 = iadd(r2,r3);
		r1 = iadd(r4,r5);

		y10 = funshift2(y32, y10);
		y32 = funshift2(y54, y32);
		y54 = funshift2(y76, y54);
		y76 = funshift2(y98, y76);
		
		yi	= iclipi(iadd((int)(x[i]),PSHR32(r1,LPC_SHIFT)),32767);
		y[i]= yi;
		y98 = funshift2(yi, y98);		
	}

#if (TM_UNROLL && TM_UNROLL_IIR)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif

	mem[0] = y10;
	mem[1] = y32;
	mem[2] = y54;
	mem[3] = y76;
	mem[4] = y98;
}

#define OVERRIDE_IIR_MEM16
void iir_mem16(const Int16 *x, const Int16 *den, Int16 *y, int N, int ord, Int32 *mem, char *stack)
{
	TMDEBUG_ALIGNMEM(den);

	IIRMEM16_START();

	if(ord==10)
		iir_mem16_10(x, den, y, N, mem);
	else if (ord==8)
		iir_mem16_8(x, den, y, N, mem);

#ifndef REMARK_ON
	(void)stack;
#endif

	IIRMEM16_STOP();
}

void fir_mem16_8(const Int16 *x, const Int16 *num, Int16 *y, int N, Int32 *mem)
{
	register int i, N_2;
	register int c67, c45, c23, c01;
	register int b0, b1, b2, b3;
	register int r1, r2, r3;
	register int x10, x32, x54, x76, xi;
	register Int16 *a; 

	N_2 = N >> 1;

	c67 = ld32x(num,3);
	c45 = ld32x(num,2);
	c23 = ld32x(num,1);
	c01 = ld32x(num,0);

	c67 = funshift2(c67,c67);
	c45 = funshift2(c45,c45);
	c23 = funshift2(c23,c23);
	c01 = funshift2(c01,c01);

	b3 = x76 = ld32x(x,N_2-1);
	b2 = x54 = ld32x(x,N_2-2);
	b1 = x32 = ld32x(x,N_2-3);
	b0 = x10 = ld32x(x,N_2-4);

#if (TM_UNROLL && TM_UNROLL_FILTER > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif

	for ( i=N-1 ; i >= 8 ; --i )
	{
		xi  = asri(16,x76);
		x76 = funshift2(x76, x54);
		x54 = funshift2(x54, x32);
		x32 = funshift2(x32, x10);
		x10 = pack16lsb(x10, (int)x[i-8]);

		r2 = iadd(ifir16(x10,c67),ifir16(x32,c45));
		r3 = iadd(ifir16(x54,c23),ifir16(x76,c01));
		r1 = iadd(r2,r3);
		
		y[i] = iclipi(iadd(xi,PSHR32(r1,LPC_SHIFT)),32767);
	}
	for ( i=7, a=(Int16*)mem ; i>=0 ; --i )
	{
		xi  = asri(16,x76);
		x76 = funshift2(x76, x54);
		x54 = funshift2(x54, x32);
		x32 = funshift2(x32, x10);
		x10 = pack16lsb(x10, (int)a[i]);
	
		r2 = iadd(ifir16(x10,c67),ifir16(x32,c45));
		r3 = iadd(ifir16(x54,c23),ifir16(x76,c01));
		r1 = iadd(r2,r3);
		
		y[i] = iclipi(iadd(xi,PSHR32(r1,LPC_SHIFT)),32767);
	}

#if (TM_UNROLL && TM_UNROLL_FILTER > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif

	mem[0] = b0;
	mem[1] = b1;
	mem[2] = b2;
	mem[3] = b3;
}

void fir_mem16_10(const Int16  *x, const Int16 *num, Int16  *y, int N, Int32  *mem)
{
	register int N_2, i;
	register int c89, c67, c45, c23, c01;
	register int b0, b1, b2, b3, b4;
	register int r1, r2, r3, r4, r5;
	register int x10, x32, x54, x76, x98, xi;
	register Int16 *a; 

	N_2 = N >> 1;

	c89 = ld32x(num,4);
	c67 = ld32x(num,3);
	c45 = ld32x(num,2);
	c23 = ld32x(num,1);
	c01 = ld32x(num,0);

	c89 = funshift2(c89,c89);
	c67 = funshift2(c67,c67);
	c45 = funshift2(c45,c45);
	c23 = funshift2(c23,c23);
	c01 = funshift2(c01,c01);

	b4 = x98 = ld32x(x,N_2-1);
	b3 = x76 = ld32x(x,N_2-2);
	b2 = x54 = ld32x(x,N_2-3);
	b1 = x32 = ld32x(x,N_2-4);
	b0 = x10 = ld32x(x,N_2-5);

#if (TM_UNROLL && TM_UNROLL_FIR > 0)
#pragma TCS_unroll=5
#pragma TCS_unrollexact=1
#endif

	for ( i=N-1 ; i >= 10 ; --i )
	{
		xi  = asri(16,x98);
		x98 = funshift2(x98, x76);
		x76 = funshift2(x76, x54);
		x54 = funshift2(x54, x32);
		x32 = funshift2(x32, x10);
		x10 = pack16lsb(x10, (int)(x[i-10]));

		r2 = iadd(ifir16(x10,c89),ifir16(x32,c67));
		r3 = iadd(ifir16(x54,c45),ifir16(x76,c23));
		r4 = ifir16(x98,c01);
		r5 = iadd(r2,r3);
		r1 = iadd(r4,r5);
		
		y[i] = iclipi(iadd(xi,PSHR32(r1,LPC_SHIFT)),32767);
	}

	for ( i=9,a =(Int16*)mem ; i>=0 ; --i )
	{
		xi  = asri(16,x98);
		x98 = funshift2(x98, x76);
		x76 = funshift2(x76, x54);
		x54 = funshift2(x54, x32);
		x32 = funshift2(x32, x10);
		x10 = pack16lsb(x10, (int)(a[i]));

		r2 = iadd(ifir16(x10,c89),ifir16(x32,c67));
		r3 = iadd(ifir16(x54,c45),ifir16(x76,c23));
		r4 = ifir16(x98,c01);
		r5 = iadd(r2,r3);
		r1 = iadd(r4,r5);
		
		y[i] = iclipi(iadd(xi,PSHR32(r1,LPC_SHIFT)),32767);
	}

#if (TM_UNROLL && TM_UNROLL_FIR > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif

	mem[0] = b0;
	mem[1] = b1;
	mem[2] = b2;
	mem[3] = b3;
	mem[4] = b4;


}

#define OVERRIDE_FIR_MEM16
void fir_mem16(const spx_word16_t *x, const Int16 *num, spx_word16_t *y, int N, int ord, Int32 *mem, char *stack)
{
	TMDEBUG_ALIGNMEM(x);
	TMDEBUG_ALIGNMEM(y);
	TMDEBUG_ALIGNMEM(num);

	FIRMEM16_START();

	if(ord==10)
		fir_mem16_10(x, num, y, N, mem);
	else if (ord==8)
		fir_mem16_8(x, num, y, N, mem);

#ifndef REMARK_ON
   	(void)stack;
#endif

	FIRMEM16_STOP();
}



#define OVERRIDE_SYN_PERCEP_ZERO16
void syn_percep_zero16(const Int16 *xx, const Int16 *ak, const Int16 *awk1, const Int16 *awk2, Int16 *y, int N, int ord, char *stack)
{
	register int i,j;
	VARDECL(Int32 *mem);
	ALLOC(mem, ord, Int32);
	
	TMDEBUG_ALIGNMEM(mem);

	for ( i=0,j=0 ; i<ord ; ++i,j+=4 )
		st32d(j,mem,0);
	iir_mem16(xx, ak, y, N, ord, mem, stack);
	for ( i=0,j=0 ; i<ord ; ++i,j+=4 )
		st32d(j,mem,0);
	filter_mem16(y, awk1, awk2, y, N, ord, mem, stack);
}


#define OVERRIDE_RESIDUE_PERCEP_ZER016
void residue_percep_zero16(const Int16 *xx, const Int16 *ak, const Int16 *awk1, const Int16 *awk2, Int16 *y, int N, int ord, char *stack)
{
	register int i,j;
	VARDECL(Int32 *mem);
	ALLOC(mem, ord, Int32);

	TMDEBUG_ALIGNMEM(mem);

	for ( i=0,j=0 ; i<ord ; ++i,j+=4 )
		st32d(j,mem,0);
	filter_mem16(xx, ak, awk1, y, N, ord, mem, stack);
	for ( i=0,j=0 ; i<ord ; ++i,j+=4 )
		st32d(j,mem,0);
	fir_mem16(y, awk2, y, N, ord, mem, stack);
}



void compute_impulse_response_10(const Int16 *ak, const Int16 *awk1, const Int16 *awk2, Int16 *y, int N)
{
	register int awk_01, awk_23, awk_45, awk_67, awk_89;
	register int y10, y32, y54, y76, y98, yi;
	register int i, acc0, acc1, N_2;

	N_2		= N << 1;

	awk_01 = ld32(awk1); 
	awk_23 = ld32x(awk1,1); 
	awk_45 = ld32x(awk1,2); 
	awk_67 = ld32x(awk1,3); 
	awk_89 = ld32x(awk1,4);

	y10 = funshift2(awk_01, LPC_SCALING << 16);
	st32d(0, y, y10);
	y32 = funshift2(awk_23, awk_01);
	st32d(4, y, y32);
	y54 = funshift2(awk_45, awk_23);
	st32d(8, y, y54);
	y76 = funshift2(awk_67, awk_45);
	st32d(12, y, y76);
	y98 = funshift2(awk_89, awk_67);
	st32d(16, y, y98);
	y10 = funshift2(0, awk_89);
	st32d(20, y, y10);
#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif
	for ( i=24 ; i<N_2 ; i+=4 )
	{	st32d(i, y, 0);
	}
#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
	y10 = y32 = y54 = y76 = y98 = 0;
	awk_01 = ld32(awk2); 
	awk_23 = ld32x(awk2,1); 
	awk_45 = ld32x(awk2,2); 
	awk_67 = ld32x(awk2,3); 
	awk_89 = ld32x(awk2,4);	
	
	awk_01 = funshift2(awk_01, awk_01);
	awk_23 = funshift2(awk_23, awk_23);
	awk_45 = funshift2(awk_45, awk_45);
	awk_67 = funshift2(awk_67, awk_67);
	awk_89 = funshift2(awk_89, awk_89);

#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
	for ( i=0 ; i<N ; ++i )
	{
		yi	= y[i];

		acc0 = ifir16(y10, awk_89) + ifir16(y32, awk_67);
		acc1 = ifir16(y54, awk_45) + ifir16(y76, awk_23);
		yi   += PSHR32(acc0 + acc1 + ifir16(y98, awk_01),LPC_SHIFT);
		y[i] = yi;

		y10 = funshift2(y32, y10);
		y32 = funshift2(y54, y32);
		y54 = funshift2(y76, y54);
		y76 = funshift2(y98, y76);
		y98 = funshift2(ineg(yi), y98);
	}
#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
	y10 = y32 = y54 = y76 = y98 = 0;
	awk_01 = ld32(ak); 
	awk_23 = ld32x(ak,1); 
	awk_45 = ld32x(ak,2); 
	awk_67 = ld32x(ak,3); 
	awk_89 = ld32x(ak,4);
	awk_01 = funshift2(awk_01, awk_01);
	awk_23 = funshift2(awk_23, awk_23);
	awk_45 = funshift2(awk_45, awk_45);
	awk_67 = funshift2(awk_67, awk_67);
	awk_89 = funshift2(awk_89, awk_89);
	
#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
	for ( i=0 ; i<N ; ++i )
	{
		yi	= y[i];

		acc0 = ifir16(y10, awk_89) + ifir16(y32, awk_67);
		acc1 = ifir16(y54, awk_45) + ifir16(y76, awk_23);
		yi   = PSHR32(SHL32(yi,LPC_SHIFT+1) + acc0 + acc1 + ifir16(y98, awk_01),LPC_SHIFT);
		y[i] = yi;

		y10 = funshift2(y32, y10);
		y32 = funshift2(y54, y32);
		y54 = funshift2(y76, y54);
		y76 = funshift2(y98, y76);
		y98 = funshift2(ineg(yi), y98);
	}
#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
}

void compute_impulse_response_8(const Int16 *ak, const Int16 *awk1, const Int16 *awk2, Int16 *y, int N)
{
	register int awk_01, awk_23, awk_45, awk_67;
	register int y10, y32, y54, y76, yi;
	register int i, acc0, acc1, N_2;

	N_2		= N << 1;

	awk_01 = ld32(awk1); 
	awk_23 = ld32x(awk1,1); 
	awk_45 = ld32x(awk1,2); 
	awk_67 = ld32x(awk1,3);

	y10 = funshift2(awk_01, LPC_SCALING << 16);
	st32d(0, y, y10);
	y32 = funshift2(awk_23, awk_01);
	st32d(4, y, y32);
	y54 = funshift2(awk_45, awk_23);
	st32d(8, y, y54);
	y76 = funshift2(awk_67, awk_45);
	st32d(12, y, y76);
	y10 = funshift2(0, awk_67);
	st32d(16, y, y10);
	st32d(20, y, 0);

#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif
	for ( i=24 ; i<N_2 ; i+=4 )
	{	st32d(i, y, 0);
	}
#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
	y10 = y32 = y54 = y76 = 0;
	awk_01 = ld32(awk2); 
	awk_23 = ld32x(awk2,1); 
	awk_45 = ld32x(awk2,2); 
	awk_67 = ld32x(awk2,3); 
	
	awk_01 = funshift2(awk_01, awk_01);
	awk_23 = funshift2(awk_23, awk_23);
	awk_45 = funshift2(awk_45, awk_45);
	awk_67 = funshift2(awk_67, awk_67);
#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif
	for ( i=0 ; i<N ; ++i )
	{
		yi	= y[i];

		acc0 = ifir16(y10, awk_67) + ifir16(y32, awk_45);
		acc1 = ifir16(y54, awk_23) + ifir16(y76, awk_01);
		yi   += PSHR32(acc0 + acc1,LPC_SHIFT);
		y[i] = yi;

		y10 = funshift2(y32, y10);
		y32 = funshift2(y54, y32);
		y54 = funshift2(y76, y54);
		y76 = funshift2(ineg(yi), y76);
	}
#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
	y10 = y32 = y54 = y76 = 0;
	awk_01 = ld32(ak); 
	awk_23 = ld32x(ak,1); 
	awk_45 = ld32x(ak,2); 
	awk_67 = ld32x(ak,3);
	awk_01 = funshift2(awk_01, awk_01);
	awk_23 = funshift2(awk_23, awk_23);
	awk_45 = funshift2(awk_45, awk_45);
	awk_67 = funshift2(awk_67, awk_67);
#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif	
	for ( i=0 ; i<N ; ++i )
	{
		yi	= y[i];

		acc0 = ifir16(y10, awk_67) + ifir16(y32, awk_45);
		acc1 = ifir16(y54, awk_23) + ifir16(y76, awk_01);
		yi   = PSHR32(SHL32(yi,LPC_SHIFT+1) + acc0 + acc1,LPC_SHIFT);
		y[i] = yi;

		y10 = funshift2(y32, y10);
		y32 = funshift2(y54, y32);
		y54 = funshift2(y76, y54);
		y76 = funshift2(ineg(yi), y76);
	}
#if (TM_UNROLL && TM_UNROLL_COMPUTEIMPULSERESPONSE > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
}


#define OVERRIDE_COMPUTE_IMPULSE_RESPONSE
void compute_impulse_response(const Int16 *ak, const Int16 *awk1, const Int16 *awk2, Int16 *y, int N, int ord, char *stack)
{
	TMDEBUG_ALIGNMEM(ak);
	TMDEBUG_ALIGNMEM(awk1);
	TMDEBUG_ALIGNMEM(awk2);
	TMDEBUG_ALIGNMEM(y);

	COMPUTEIMPULSERESPONSE_START();
	if ( ord == 10 )
		compute_impulse_response_10(ak,awk1,awk2,y,N);
	else
		compute_impulse_response_8(ak,awk1,awk2,y,N);

	(void)stack;

	COMPUTEIMPULSERESPONSE_STOP();
}


#define OVERRIDE_QMFSYNTH
void qmf_synth(const Int16 *x1, const Int16 *x2, const Int16 *a, Int16 *y, int N, int M, Int32 *mem1, Int32 *mem2, char *stack)
   /* assumptions:
      all odd x[i] are zero -- well, actually they are left out of the array now
      N and M are multiples of 4 */
{
	register int i, j;
	register int M2, N2;
	VARDECL(int *x12);
	M2 = M>>1;
	N2 = N>>1;
	ALLOC(x12, M2+N2, int);


	TMDEBUG_ALIGNMEM(a);
	TMDEBUG_ALIGNMEM(x12);
	TMDEBUG_ALIGNMEM(mem1);
	TMDEBUG_ALIGNMEM(mem2);

	QMFSYNTH_START();
   
#if (TM_UNROLL && TM_UNROLL_QMFSYNTH > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif	
	for ( i=0 ; i<N2 ; ++i )
	{	register int index = N2-1-i;
		x12[i] = pack16lsb(x1[index],x2[index]);
	}

	for ( j= 0; j<M2 ; ++j)
	{	register int index = (j << 1) + 1;
		x12[N2+j] = pack16lsb(mem1[index],mem2[index]);
	}
#if (TM_UNROLL && TM_UNROLL_QMFSYNTH > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif	 
	for (i = 0; i < N2; i += 2) 
	{
		register int y0, y1, y2, y3;
		register int x12_0;

		y0 = y1 = y2 = y3 = 0;
		x12_0 = x12[N2-2-i];

		for (j = 0; j < M2; j += 2) 
		{
			register int x12_1;
			register int a10, a11, a0_0;
			register int _a10, _a11, _a0_0;

			a10 = ld32x(a,j);
			a11 = pack16msb(a10,a10);
			a0_0= pack16lsb(a10,ineg(sex16(a10)));
			x12_1 = x12[N2-1+j-i];

			y0 += ifir16(a0_0,x12_1);
			y1 += ifir16(a11, x12_1);
			y2 += ifir16(a0_0,x12_0);
			y3 += ifir16(a11 ,x12_0);


			_a10   = ld32x(a,j+1);
			_a11   = pack16msb(_a10,_a10);
			_a0_0  = pack16lsb(_a10,ineg(sex16(_a10)));
			x12_0  = x12[N2+j-i];
			
			y0 += ifir16(_a0_0,x12_0);
			y1 += ifir16(_a11, x12_0);
			y2 += ifir16(_a0_0,x12_1);
			y3 += ifir16(_a11 ,x12_1);

		}
		y[2*i]   = EXTRACT16(SATURATE32(PSHR32(y0,15),32767));
		y[2*i+1] = EXTRACT16(SATURATE32(PSHR32(y1,15),32767));
		y[2*i+2] = EXTRACT16(SATURATE32(PSHR32(y2,15),32767));
		y[2*i+3] = EXTRACT16(SATURATE32(PSHR32(y3,15),32767));
	}

#if (TM_UNROLL && TM_UNROLL_QMFSYNTH > 0)
#pragma TCS_unroll=4
#pragma TCS_unrollexact=1
#endif	
	for (i = 0; i < M2; ++i)
	{	mem1[2*i+1] = asri(16,x12[i]);
		mem2[2*i+1] = sex16(x12[i]);
	}
#if (TM_UNROLL && TM_UNROLL_QMFSYNTH > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif	 

	QMFSYNTH_STOP();
}


#define OVERRIDE_QMFDECOMP
void qmf_decomp(const Int16 *xx, const Int16 *aa, Int16 *y1, Int16 *y2, int N, int M, Int16 *mem, char *stack)
{
	VARDECL(int *_a);
	VARDECL(int *_x);
	register int i, j, k, MM, M2, N2;  
	register int _xx10, _mm10;
	register int *_x2;

	M2=M>>1;
	N2=N>>1;
	MM=(M-2)<<1;

	ALLOC(_a, M2, int);
	ALLOC(_x, N2+M2, int);
	_x2 = _x + M2 - 1;

	TMDEBUG_ALIGNMEM(xx);
	TMDEBUG_ALIGNMEM(aa);
	TMDEBUG_ALIGNMEM(y1);
	TMDEBUG_ALIGNMEM(y2);
	TMDEBUG_ALIGNMEM(mem);
	TMDEBUG_ALIGNMEM(_a);
	TMDEBUG_ALIGNMEM(_x);

	QMFDECOMP_START();

	_xx10 = ld32(xx);
	_xx10 = dualasr(_xx10,1);
	_mm10 = ld32(mem);
	_x2[0] = pack16lsb(_xx10,_mm10);

#if (TM_UNROLL && TM_UNROLL_QMFSYNTH > 0)
#pragma TCS_unroll=2
#pragma TCS_unrollexact=1
#endif	
	for ( i=0 ; i<M2 ; ++i )
	{	register int a10;
		
		a10 = ld32x(aa,i);
		a10 = funshift2(a10, a10);
		_a[M2-i-1] = a10;		
	}

	for ( j=1 ; j<N2 ; ++j )
	{	register int _xx32;

		_xx32   = ld32x(xx,j);
		_xx32   = dualasr(_xx32,1);
		_x2[j]  = funshift2(_xx32, _xx10);
		_xx10   = _xx32;
	}

	for ( k=1 ; k<M2; ++k )
	{	register int _mm32;

		_mm32   = ld32x(mem,k);
		_mm10   = funshift2(_mm10,_mm10);
		_x2[-k] = pack16lsb(_mm10,_mm32);
		_mm10 = _mm32;
	}


	for ( i=N2-1,j=0 ; j<MM ; --i,j+=4 )
	{	register int _xx;

		_xx = ld32x(xx,i);
		_xx = dualasr(_xx,1);
		_xx = funshift2(_xx,_xx);
		st32d(j, mem, _xx);
	}
#if (TM_UNROLL && TM_UNROLL_QMFSYNTH > 0)
#pragma TCS_unrollexact=0
#pragma TCS_unroll=0
#endif
	mem[M-2] = xx[N-M+1] >> 1;


	M2 >>= 1;
	for ( i=0 ; i<N2 ; ++i )
	{	register int y1k, y2k;

		y1k = y2k = 0;

		for ( j=0 ; j<M2 ; j+=2 )
		{	register int _aa, _acc0, _acc1;
			register int __xx10, __mm10, __acc0, __acc1, __aa;
			register int _tmp0, _tmp1, _tmp2, _tmp3;

			_xx10 = ld32x(_x, i+j);
			_mm10 = ld32x(_x2,i-j);
			_aa	  = ld32x(_a, j);
			_mm10 = funshift2(_mm10,_mm10);
			_acc0 = dspidualadd(_xx10, _mm10);
			_acc1 = dspidualsub(_xx10, _mm10);

			__xx10 = ld32x(_x, i+j+1);
			__mm10 = ld32x(_x2,i-j-1);
			__aa   = ld32x(_a, j+1);
			__mm10 = funshift2(__mm10,__mm10);
			__acc0 = dspidualadd(__xx10, __mm10);
			__acc1 = dspidualsub(__xx10, __mm10);

			y1k   += ifir16(_aa, _acc0);
			y1k   += ifir16(__aa, __acc0);

			_tmp0 = pack16lsb(_aa,__aa);
			_tmp1 = pack16msb(_aa,__aa);
			_tmp2 = pack16lsb(_acc1, __acc1);
			_tmp3 = pack16msb(_acc1, __acc1);

			y2k	  -= ifir16(_tmp0, _tmp2);
			y2k	  += ifir16(_tmp1, _tmp3);

		}

		y1[i] = iclipi(PSHR32(y1k,15),32767);
		y2[i] = iclipi(PSHR32(y2k,15),32767);
	}

	QMFDECOMP_STOP();
}

#endif

