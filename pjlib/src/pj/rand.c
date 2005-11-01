/* $Id$
 *
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

