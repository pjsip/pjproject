/* makefilter.c */

#include <stdio.h>
#include <math.h>
#include "stdefs.h"
#include "filterkit.h"
#include "resample.h"
#define MAXNWING   8192		/* FIXME: flush */

/* LIBRARIES needed:
 *
 * 1. filterkit
 *       makeFilter()  - designs a Kaiser-windowed low-pass filter
 *       writeFilter() - writes a filter to a standard filter file
 *       GetUShort()   - prompt user for a UHWORD with help
 *       GetDouble()   - prompt user for a double with help
 *
 * 2. math
 */

char NmultHelp[] =
    "\n   Nmult is the length of the symmetric FIR lowpass filter used"
    "\n   by the sampling rate converter. It must be odd."
    "\n   This is the number of multiplies per output sample for"
    "\n   up-conversions (Factor>1), and is the number of multiplies"
    "\n   per input sample for down-conversions (Factor<1). Thus if"
    "\n   the rate conversion is Srate2 = Factor*Srate1, then you have"
    "\n   Nmult*Srate1*MAXof(Factor,1) multiplies per second of real time."
    "\n   Naturally, higher Nmult gives better lowpass-filtering at the"
    "\n   expense of longer compute times. Nmult should be odd because"
    "\n   it is the length of a symmetric FIR filter, and the current"
    "\n   implementation requires a coefficient at the time origin.\n";      

char FrollHelp[] =
    "\n   Froll determines the frequency at which the lowpass filter begins to"
    "\n   roll-off. If Froll=1, then there is no 'guard zone' and the filter"
    "\n   roll-off region will be aliased. If Froll is 0.90, for example, then"
    "\n   the filter begins rolling off at 0.90*Srate/2, so that by Srate/2,"
    "\n   the filter is well down and aliasing is reduced.  Since aliasing"
    "\n   distortion is typically worse than loss of the high-frequency spectral"
    "\n   amplitude, Froll<1 is highly recommended. The default of 0.90"
    "\n   sacrifices the upper 10 percent of the spectrum as an anti-aliasing"
    "\n   guard zone.\n";

char BetaHelp[] =
    "\n   Beta trades the rejection of the lowpass filter against the"
    "\n   transition width from passband to stopband. Larger Beta means"
    "\n   a slower transition and greater stopband rejection. See Rabiner"
    "\n   and Gold (Th. and App. of DSP) under Kaiser windows for more about"
    "\n   Beta. The following table from Rabiner and Gold (p. 101) gives some"
    "\n   feel for the effect of Beta:"
    "\n"
    "\n               BETA    D       PB RIP   SB RIP"
    "\n               2.120   1.50  +-0.27      -30"
    "\n               3.384   2.23    0.0864    -40"
    "\n               4.538   2.93    0.0274    -50"
    "\n               5.658   3.62    0.00868   -60"
    "\n               6.764   4.32    0.00275   -70"
    "\n               7.865   5.0     0.000868  -80"
    "\n               8.960   5.7     0.000275  -90"
    "\n               10.056  6.4     0.000087  -100"
   "\n"
   "\n   Above, ripples are in dB, and the transition band width is "
   "\n   approximately D*Fs/N, where Fs = sampling rate, "
   "\n   and N = window length.  PB = 'pass band' and SB = 'stop band'."
   "\n   Alternatively, D is the transition widths in bins given a"
   "\n   length N DFT (i.e. a window transform with no zero padding."
   "\n";


int main(void)
{
   HWORD Imp[MAXNWING];               /* Filter coefficients */
   HWORD ImpD[MAXNWING];              /* ImpD[i] = ImpD[i+1] - ImpD[i] */
   double Froll, Beta;
   UHWORD Nmult, Nwing, LpScl;
   int err;

   Froll = 0.90;
   Beta  = 9;
   Nmult = 65;
   while (1)
      {
      Froll = GetDouble("Normalized Roll-off freq (0<Froll<=1)",
         Froll, FrollHelp);
      Beta = GetDouble("Beta", Beta, BetaHelp);
      Nmult = GetUHWORD("Odd filter length 'Nmult'", Nmult, NmultHelp);
      if ((Nmult&1) == 0) {
	  Nmult++;
	  printf("Filter length increased to %d to make it odd.\n",Nmult);
      }
      Nwing = Npc*(Nmult-1)/2;   /* # of filter coeffs in right wing */
      printf("\n");
      if (!(Nmult % 2))
	  printf("Error: Nmult must be odd and greater than zero\n");
      else if ((err = makeFilter(Imp, ImpD, &LpScl, Nwing, Froll, Beta))) {
	  printf("*** Error: Unable to make filter.\n");
	  if (err == 1)
	      printf("\tNmult=%d too large for MAXNWING=%d\n",Nmult,MAXNWING);
	  else if (err == 2)
	      printf("\tNormalized roll-off freq Froll must be between 0 and 1\n");
	  else if (err == 3)
	      printf("\tHeisenberg says Beta must be greater or equal to 1\n");
	  else if (err == 4) {
	      printf("\tUnity-gain scale factor overflows 16-bit half-word\n");
	      printf("\tFilter design was probably way off. Try relaxing specs.\n");
	  }
      } else if ((err = writeFilter(Imp, ImpD, LpScl, Nmult, Nwing)))
	  printf("Error: Unable to write filter, err=%d\n", err);
      else
	  break;
  }
   return(0);
}
