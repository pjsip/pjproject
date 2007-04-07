/*:filterkit.h */

#include	"stdefs.h"

/*
 * LpFilter() - Calculates the filter coeffs for a Kaiser-windowed low-pass
 *                   filter with a given roll-off frequency.  These coeffs
 *                   are stored into a array of doubles.
 * writeFilter() - Writes a filter to a file.
 * makeFilter() - Calls LpFilter() to create a filter, then scales the double
 *                   coeffs into an array of half words.
 * readFilter() - Reads a filter from a file.
 * FilterUp() - Applies a filter to a given sample when up-converting.
 * FilterUD() - Applies a filter to a given sample when up- or down-
 *                   converting.
 * initZerox() - Initialization routine for the zerox() function.  Must
 *                   be called before zerox() is called.  This routine loads
 *                   the correct filter so zerox() can use it.
 * zerox() - Given a pointer into a sample, finds a zero-crossing on the
 *                   interval [pointer-1:pointer+2] by iteration.
 * Query() - Ask the user for a yes/no question with prompt, default, 
 *                   and optional help.
 * GetUShort() - Ask the user for a unsigned short with prompt, default,
 *                   and optional help.
 * GetDouble() - Ask the user for a double with prompt, default, and
 *                   optional help.
 * GetString() - Ask the user for a string with prompt, default, and
 *                   optional help.
 */

void LpFilter(double c[], int N, double frq, double Beta, int Num);
/*
 * reference: "Digital Filters, 2nd edition"
 *            R.W. Hamming, pp. 178-179
 *
 * LpFilter() computes the coeffs of a Kaiser-windowed low pass filter with
 *    the following characteristics:
 *
 *       c[]  = array in which to store computed coeffs
 *       frq  = roll-off frequency of filter
 *       N    = Half the window length in number of coeffs
 *       Beta = parameter of Kaiser window
 *       Num  = number of coeffs before 1/frq
 *
 * Beta trades the rejection of the lowpass filter against the transition
 *    width from passband to stopband.  Larger Beta means a slower
 *    transition and greater stopband rejection.  See Rabiner and Gold
 *    (Theory and Application of DSP) under Kaiser windows for more about
 *    Beta.  The following table from Rabiner and Gold gives some feel
 *    for the effect of Beta:
 *
 * All ripples in dB, width of transition band = D*N where N = window length
 *
 *               BETA    D       PB RIP   SB RIP
 *               2.120   1.50  +-0.27      -30
 *               3.384   2.23    0.0864    -40
 *               4.538   2.93    0.0274    -50
 *               5.658   3.62    0.00868   -60
 *               6.764   4.32    0.00275   -70
 *               7.865   5.0     0.000868  -80
 *               8.960   5.7     0.000275  -90
 *               10.056  6.4     0.000087  -100
 */

int writeFilter(HWORD Imp[], HWORD ImpD[], UHWORD LpScl, UHWORD Nmult, UHWORD Nwing);
/*
 * Write a filter to a file
 *    Filter file format:
 *       file name: "F" Nmult "T" Nhc ".filter"
 *       1st line:  the string "ScaleFactor" followed by its value.
 *       2nd line:  the string "Length" followed by Nwing's value.
 *       3rd line:  the string "Coeffs:" on a separate line.
 *       following lines:  Nwing number of 16-bit impulse response values
 *          in the right wing of the impulse response (the Imp[] array).
 *         (Nwing is equal to Npc*(Nmult+1)/2+1, where Npc is defined in the
 *         file "resample.h".)  Each coefficient is on a separate line.
 *       next line:  the string "Differences:" on a separate line.
 *       following lines:  Nwing number of 16-bit impulse-response
 *          successive differences:  ImpD[i] = Imp[i+1] - Imp[i].
 * ERROR codes:
 *   0 - no error
 *   1 - could not open file
 */

int makeFilter(HWORD Imp[], HWORD ImpD[], UHWORD *LpScl, UHWORD Nwing,
	       double Froll, double Beta);
/*
 * makeFilter
 * ERROR return codes:
 *    0 - no error
 *    1 - Nwing too large (Nwing is > MAXNWING)
 *    2 - Froll is not in interval [0:1)
 *    3 - Beta is < 1.0
 *    4 - LpScl will not fit in 16-bits
 */

int readFilter(char *filterFile,
	       HWORD **ImpP, HWORD **ImpDP, UHWORD *LpScl, 
	       UHWORD *Nmult, UHWORD *Nwing);
/*
 * Read-in a filter
 *    Filter file format:
 *       Default file name: "F" Nmult "T" Nhc ".filter"
 *       1st line:  the string "ScaleFactor" followed by its value.
 *       2nd line:  the string "Length" followed by Nwing's value.
 *       3rd line:  the string "Coeffs:" on separate line.
 *       Nwing number of 16-bit impulse response values in the right
 *          wing of the impulse response.  (Length=Npc*(Nmult+1)/2+1,
 *          where originally Npc=2^9, and Nmult=13.)   Each on separate line.
 *       The string "Differences:" on separate line.
 *       Nwing number of 16-bit impulse-response successive differences:
 *          ImpDiff[i] = Imp[i+1] - Imp[i].
 *
 * ERROR return codes:
 *    0 - no error
 *    1 - file not found
 *    2 - invalid ScaleFactor in file
 *    3 - invalid Length in file
 */

WORD FilterUp(HWORD Imp[], HWORD ImpD[], UHWORD Nwing, BOOL Interp,
	      HWORD *Xp, HWORD Inc, HWORD Ph);

WORD FilterUD(HWORD Imp[], HWORD ImpD[], UHWORD Nwing, BOOL Interp,
	      HWORD *Xp, HWORD Ph, HWORD Inc, UHWORD dhb);

int initZerox(UHWORD tempNmult);
/*
 * initZerox
 * ERROR return values:
 *   0 - no error
 *   1 - Nmult is even (should be odd)
 *   2 - filter file not found
 *   3 - invalid ScaleFactor in input file
 *   4 - invalid Length in file
 */

/*
 * zerox
 *    Given a pointer into a sound sample, this function uses a low-pass
 * filter to estimate the x coordinate of the zero-crossing which must ocurr
 * between Data[0] and Data[1].  This value is returned as the value of the
 * function.  A return value of -100 indicates there was no zero-crossing in
 * the x interval [-1,2].  Factor is the resampling factor: Rate(out) /
 * Rate(in).  Nmult (which determines which filter is used) is passed the
 * zerox's initialization routine: initZerox(Nmult)
 */
double zerox(HWORD *Data, double Factor);

BOOL Query(char *prompt, BOOL deflt, char *help);

unsigned short GetUShort(char *title, unsigned short deflt, char *help);

double GetDouble(char *title, double deflt, char *help);

char *GetString(char *prompt, char *deflt, char *help);

#define GetUHWORD(x,y,z) GetUShort(x,y,z)

