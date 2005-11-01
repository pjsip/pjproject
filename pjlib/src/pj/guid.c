/* $Id$
 *
 */
#include <pj/guid.h>
#include <pj/pool.h>

PJ_DEF(void) pj_create_unique_string(pj_pool_t *pool, pj_str_t *str)
{
    str->ptr = pj_pool_alloc(pool, PJ_GUID_STRING_LENGTH);
    pj_generate_unique_string(str);
}
