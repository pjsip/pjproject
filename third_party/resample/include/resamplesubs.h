#ifndef __RESAMPLESUBS_H__
#define __RESAMPLESUBS_H__

typedef char           RES_BOOL;
typedef short          RES_HWORD;
typedef int            RES_WORD;
typedef unsigned short RES_UHWORD;
typedef unsigned int   RES_UWORD;

int res_SrcLinear(const RES_HWORD X[], RES_HWORD Y[], 
		  double pFactor, RES_UHWORD nx);
int res_Resample(const RES_HWORD X[], RES_HWORD Y[], double pFactor, 
	         RES_UHWORD nx, RES_BOOL LargeF, RES_BOOL Interp);
int res_GetXOFF(double pFactor, RES_BOOL LargeF);


#endif

