/* Tier 1: the module and the public headers must both be usable. */
@import PJSIP;
#include <pjsua.h>

int probe(void);
int probe(void)
{
    pj_status_t status = pj_init();
    return status == PJ_SUCCESS ? (int)pjsua_create() : (int)status;
}
