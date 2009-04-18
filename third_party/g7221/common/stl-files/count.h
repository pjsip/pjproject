/*
 ===========================================================================
 COUNT.H
 ~~~~~~~

 Prototypes and definitions for counting operations

 These functions, and the ones in basop32.h, makes it possible to
 measure the wMOPS of a codec.

 All functions in this file, and in basop32.h, updates a structure
 so that it will be possible the see how many calls to add, mul mulAdd
 ... that the code made, and estimate the wMOPS (and MIPS) for a
 sertain part of code

 It is also possible to measure the wMOPS separatly for different
 parts of the codec.

 This is done by creating a counter group (getCounterId) for each part
 of the code that one wants a separte measure for. Before a part of
 the code is executed a call to the "setCounter" function is needed to
 identify which counter group to use.

 Currently there is a limit of 255 different counter groups.

 In the end of this file there is a pice of code illustration how the
 functions can be used.

 History
 ~~~~~~~
 09.Aug.1999 V1.0.0 Input to UGST from ETSI AMR (count.h); 
 26.Jan.2000 V1.1.0 Added counter entries for G.723.1's 
                    L_mls(), div_l(), i_mult() [from basop32.c]
 05.Jul.2000 V1.2.0 Added counter entries for 32bit shiftless 
                    operators L_mult0(), L_mac0(), L_msu0()
 ===========================================================================
*/
#if 0
#ifndef COUNT_H
#define COUNT_H "$Id $"

#define MAXCOUNTERS 256

int getCounterId(char *objectName);
/*
 * Create a counter group, the "objectname" will be used when printing
 * statistics for this counter group.
 *
 * Returns 0 if no more counter groups are available.
 */

void setCounter(int counterId);
/*
 * Defines which counter group to use, default is zero.
 */

void Init_WMOPS_counter (void);
/*
 * Initiates the current counter group.
 */

void Reset_WMOPS_counter (void);
/*
 * Resets the current counter group.
 */

void WMOPS_output (Word16 notPrintWorstWorstCase);
/*
 * Prints the statistics to the screen, if the argument if non zero
 * the statistics for worst worst case will not be printed. This is typically
 * done for dtx frames.
 *
 */

PJ_INLINE(Word32) fwc (void)
{
#if WMOPS
    Word32 tot;

    tot = DeltaWeightedOperation ();
    if (tot > wc[currCounter][funcid[currCounter]])
        wc[currCounter][funcid[currCounter]] = tot;

    funcid[currCounter]++;

    return (tot);
#else
    return 0; /* Dummy */
#endif
}

/*
 * worst worst case counter.
 *
 * This function calculates the worst possible case that can be reached.
 *
 * This is done by calling this function for each subpart of the calculations
 * for a frame. This function then stores the maximum wMOPS for each part.
 *
 * The WMOPS_output function add together all parts and presents the sum.
 */
PJ_INLINE(void) move16 (void)
{
#if WMOPS
    multiCounter[currCounter].DataMove16++;
#endif
}

PJ_INLINE(void) move32 (void)
{
#if WMOPS
    multiCounter[currCounter].DataMove32++;
#endif
}

PJ_INLINE(void )logic16 (void)
{
#if WMOPS
    multiCounter[currCounter].Logic16++;
#endif
}

PJ_INLINE(void) logic32 (void)
{
#if WMOPS
    multiCounter[currCounter].Logic32++;
#endif
}

PJ_INLINE(void) test (void)
{
#if WMOPS
    multiCounter[currCounter].Test++;
#endif
}


/*
 * The functions above increases the corresponding operation counter for
 * the current counter group.
 */

typedef struct
{
    Word32 add;        /* Complexity Weight of 1 */
    Word32 sub;
    Word32 abs_s;
    Word32 shl;
    Word32 shr;
    Word32 extract_h;
    Word32 extract_l;
    Word32 mult;
    Word32 L_mult;
    Word32 negate;
    Word32 round;
    Word32 L_mac;
    Word32 L_msu;
    Word32 L_macNs;
    Word32 L_msuNs;
    Word32 L_add;      /* Complexity Weight of 2 */
    Word32 L_sub;
    Word32 L_add_c;
    Word32 L_sub_c;
    Word32 L_negate;
    Word32 L_shl;
    Word32 L_shr;
    Word32 mult_r;
    Word32 shr_r;
    Word32 shift_r;
    Word32 mac_r;
    Word32 msu_r;
    Word32 L_deposit_h;
    Word32 L_deposit_l;
    Word32 L_shr_r;    /* Complexity Weight of 3 */
    Word32 L_shift_r;
    Word32 L_abs;
    Word32 L_sat;      /* Complexity Weight of 4 */
    Word32 norm_s;     /* Complexity Weight of 15 */
    Word32 div_s;      /* Complexity Weight of 18 */
    Word32 norm_l;     /* Complexity Weight of 30 */
    Word32 DataMove16; /* Complexity Weight of 1 */
    Word32 DataMove32; /* Complexity Weight of 2 */
    Word32 Logic16;    /* Complexity Weight of 1 */
    Word32 Logic32;    /* Complexity Weight of 2 */
    Word32 Test;       /* Complexity Weight of 2 */
                       /* Counters for G.723.1 basic operators*/
    Word32 L_mls;      /* Complexity Weight of 1 */
    Word32 div_l;      /* Complexity Weight of 1 */
    Word32 i_mult;     /* Complexity Weight of 1 */
    Word32 L_mult0;    /* Complexity Weight of 1 */
    Word32 L_mac0;     /* Complexity Weight of 1 */
    Word32 L_msu0;     /* Complexity Weight of 1 */
                       /* Counters for G.722.1 basic operators*/
    Word32 LU_shl;     /* Complexity Weight of 1 */
    Word32 LU_shr;     /* Complexity Weight of 1 */
}
BASIC_OP;

#ifdef THISISANEXAMPLE_0123456789
/*
  -----------------------------------------------------------------------
  Example of how count.h could be used.
 
  In the example below it is assumed that the init_OBJECT functions
  does not use any calls to counter.h or basic_op.h. If this is the
  case a call to the function Reset_WMOPS_counter() must be done after
  each call to init_OBJECT if these operations is not to be included
  in the statistics.
  -----------------------------------------------------------------------
*/

int main()
{ 
  int spe1Id,spe2Id,cheId;
 
  /* initiate counters and objects */
  spe1Id=getCounterId("Spe 5k8"); 
  setCounter(spe1Id); 
  Init_WMOPS_counter (); 
  init_spe1(...);
 
  spe2Id=getCounterId("Spe 12k2"); 
  setCounter(spe2Id); 
  Init_WMOPS_counter (); 
  init_spe2(...);
 
  cheId=getCounterId("Channel encoder"); 
  setCounter(cheId); 
  Init_WMOPS_counter (); 
  init_che(...); 
  ...
 
  while(data)
  { 
    test();             /* Note this call to test(); */
    if(useSpe1)
      setCounter(spe1Id); 
    else 
      setCounter(spe2Id); 
    Reset_WMOPS_counter();
    speEncode(...);
    WMOPS_output(0);    /* Normal routine for displaying WMOPS info */
    
    setCounter(cheId); 
    Reset_WMOPS_counter();
    preChannelInter(...); fwc(); /* Note the call to fwc() for each part */
    convolve(...); fwc();        /* of the channel encoder. */
    interleave(...); fwc();
    WMOPS_output(0);    /* Normal routine for displaying WMOPS info */
  }
}
#endif /* Example */

#endif /* COUNT_H */
#else

#define move16()
#define move32()
#define logic16()
#define logic32()
#define test()

#endif
