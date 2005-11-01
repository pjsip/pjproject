/* $Header: /pjproject-0.3/pjlib/src/pj/rand.c 3     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/rand.c $
 * 
 * 3     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 2     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 * 1     9/15/05 8:40p Bennylp
 * Created.
 */
#include <pj/rand.h>
#include <pj/os.h>
#include <pj/compat/rand.h>

PJ_DEF(void) pj_srand(unsigned int seed)
{
    PJ_CHECK_STACK();
    platform_srand(seed);
}

PJ_DEF(int) pj_rand(void)
{
    PJ_CHECK_STACK();
    return platform_rand();
}

