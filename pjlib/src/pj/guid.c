/* $Header: /pjproject-0.3/pjlib/src/pj/guid.c 12    10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/guid.c $
 * 
 * 12    10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 11    9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#include <pj/guid.h>
#include <pj/pool.h>

PJ_DEF(void) pj_create_unique_string(pj_pool_t *pool, pj_str_t *str)
{
    str->ptr = pj_pool_alloc(pool, PJ_GUID_STRING_LENGTH);
    pj_generate_unique_string(str);
}
