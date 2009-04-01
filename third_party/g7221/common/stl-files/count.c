/*
 ***************************************************************************
 *
 *   This file contains functions for the automatic complexity calculation
 * $Id $
 ***************************************************************************
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "typedef.h"
#include "count.h"

/* Global counter variable for calculation of complexity weight */

BASIC_OP multiCounter[MAXCOUNTERS];
int currCounter=0; /* Zero equals global counter */

/*BASIC_OP counter;*/
const BASIC_OP op_weight =
{
  /* G.729 & G.723.1 common operators */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 4, 15, 18, 30, 1, 2, 1, 2, 2, 
  /* G.723.1 exclusives */ 
  6, 1, 1, 
  /* shiftless 32-bit operators */ 
  1, 1, 1,
  /* G.722.1 exclusives */ 
  1, 1
};

/* function prototypes */
Word32 TotalWeightedOperation (void);
Word32 DeltaWeightedOperation (void);

/* local variable */
#if WMOPS

/* Counters for separating counting for different objects */
static int maxCounter=0;
static char* objectName[MAXCOUNTERS+1];
static Word16 fwc_corr[MAXCOUNTERS+1];

#define NbFuncMax  1024

static Word16 funcid[MAXCOUNTERS], nbframe[MAXCOUNTERS];
static Word32 glob_wc[MAXCOUNTERS], wc[MAXCOUNTERS][NbFuncMax];
static float total_wmops[MAXCOUNTERS];

static Word32 LastWOper[MAXCOUNTERS];

static char* my_strdup(const char *s)
/*
 * duplicates UNIX function strdup() which is not ANSI standard:
 * -- malloc() memory area big enough to hold the string s
 * -- copy string into new area
 * -- return pointer to new area
 *
 * returns NULL if either s==NULL or malloc() fails
 */
{
    char *dup;
    
    if (s == NULL)
        return NULL;

    /* allocate memory for copy of ID string (including string terminator) */
    /* NOTE: the ID strings will never be deallocated because there is no
             way to "destroy" a counter that is not longer needed          */
    if ((dup = (char *) malloc(strlen(s)+1)) == NULL)
        return NULL;

    return strcpy(dup, s);
}

#endif

int getCounterId(char *objectNameArg)
{
#if WMOPS
  if(maxCounter>=MAXCOUNTERS-1) return 0;
  objectName[++maxCounter]=my_strdup(objectNameArg);
  return maxCounter;
#else
  return 0; /* Dummy */
#endif
}

void setCounter(int counterId)
{
#if WMOPS
  if(counterId>maxCounter || counterId<0)
    {
      currCounter=0;
      return;
    }
  currCounter=counterId;
#endif
}

#if WMOPS
static Word32 WMOPS_frameStat()
/* calculate the WMOPS seen so far and update the global
   per-frame maximum (glob_wc)
 */
{
    Word32 tot;

    tot = TotalWeightedOperation ();
    if (tot > glob_wc[currCounter])
        glob_wc[currCounter] = tot;

    /* check if fwc() was forgotten at end of last frame */
    if (tot > LastWOper[currCounter]) {
        if (!fwc_corr[currCounter]) {
            fprintf(stderr,
                    "count: operations counted after last fwc() for '%s'; "
                    "-> fwc() called\n",
                    objectName[currCounter]?objectName[currCounter]:"");
        }
        fwc();
    }
    
    return tot;
}

static void WMOPS_clearMultiCounter()
{
    Word16 i;
    
    Word32 *ptr = (Word32 *) &multiCounter[currCounter];
    for (i = 0; i < (sizeof (multiCounter[currCounter])/ sizeof (Word32)); i++)
    {
        *ptr++ = 0;
    }
}
#endif

Word32 TotalWeightedOperation ()
{
#if WMOPS
    Word16 i;
    Word32 tot, *ptr, *ptr2;

    tot = 0;
    ptr = (Word32 *) &multiCounter[currCounter];
    ptr2 = (Word32 *) &op_weight;
    for (i = 0; i < (sizeof (multiCounter[currCounter])/ sizeof (Word32)); i++)
    {
        tot += ((*ptr++) * (*ptr2++));
    }

    return ((Word32) tot);
#else
    return 0; /* Dummy */
#endif
}

Word32 DeltaWeightedOperation ()
{
#if WMOPS
    Word32 NewWOper, delta;

    NewWOper = TotalWeightedOperation ();
    delta = NewWOper - LastWOper[currCounter];
    LastWOper[currCounter] = NewWOper;
    return (delta);
#else
    return 0; /* Dummy */
#endif
}

void move16 (void)
{
#if WMOPS
    multiCounter[currCounter].DataMove16++;
#endif
}

void move32 (void)
{
#if WMOPS
    multiCounter[currCounter].DataMove32++;
#endif
}

void test (void)
{
#if WMOPS
    multiCounter[currCounter].Test++;
#endif
}

void logic16 (void)
{
#if WMOPS
    multiCounter[currCounter].Logic16++;
#endif
}

void logic32 (void)
{
#if WMOPS
    multiCounter[currCounter].Logic32++;
#endif
}

void Init_WMOPS_counter (void)
{
#if WMOPS
    Word16 i;

    /* reset function weight operation counter variable */

    for (i = 0; i < NbFuncMax; i++)
        wc[currCounter][i] = (Word32) 0;
    glob_wc[currCounter] = 0;
    nbframe[currCounter] = 0;
    total_wmops[currCounter] = 0.0;

    /* initially clear all counters */
    WMOPS_clearMultiCounter();
    LastWOper[currCounter] = 0;
    funcid[currCounter] = 0;
#endif
}


void Reset_WMOPS_counter (void)
{
#if WMOPS
    Word32 tot = WMOPS_frameStat();
        
    /* increase the frame counter --> a frame is counted WHEN IT BEGINS */
    nbframe[currCounter]++;
    /* add wmops used in last frame to count, then reset counter */
    /* (in first frame, this is a no-op                          */
    total_wmops[currCounter] += ((float) tot) * 0.00005;
    
    /* clear counter before new frame starts */
    WMOPS_clearMultiCounter();
    LastWOper[currCounter] = 0;
    funcid[currCounter] = 0;           /* new frame, set function id to zero */
#endif
}

Word32 fwc (void)                      /* function worst case */
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

void WMOPS_output (Word16 dtx_mode)
{
#if WMOPS
    Word16 i;
    Word32 tot, tot_wm, tot_wc;

    /* get operations since last reset (or init),
       but do not update the counters (except the glob_wc[] maximum)
       so output CAN be called in each frame without problems.
       The frame counter is NOT updated!
     */
    tot = WMOPS_frameStat();
    tot_wm = total_wmops[currCounter] + ((float) tot) * 0.00005;

    fprintf (stdout, "%10s:WMOPS=%.3f",
	     objectName[currCounter]?objectName[currCounter]:"",
	     ((float) tot) * 0.00005);

    if (nbframe[currCounter] != 0)
        fprintf (stdout, "  Average=%.3f",
                 tot_wm / (float) nbframe[currCounter]);
    
    fprintf (stdout, "  WorstCase=%.3f",
             ((float) glob_wc[currCounter]) * 0.00005);

    /* Worst worst case printed only when not in DTX mode */
    if (dtx_mode == 0)
    {
        tot_wc = 0L;
        for (i = 0; i < funcid[currCounter]; i++)
            tot_wc += wc[currCounter][i];
        fprintf (stdout, "  WorstWC=%.3f", ((float) tot_wc) * 0.00005);
    }
    fprintf (stdout, " (%d frames)\n", nbframe[currCounter]);
    
#endif
}
