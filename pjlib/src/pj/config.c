/* $Id$
 *
 */
#include <pj/config.h>
#include <pj/log.h>

static const char *id = "config.c";
const char *PJ_VERSION = "0.3.0-pre1";

PJ_DEF(void) pj_dump_config(void)
{
    PJ_LOG(3, (id, "PJLIB (c)2005 Benny Prijono"));
    PJ_LOG(3, (id, "Dumping configurations:"));
    PJ_LOG(3, (id, " PJ_VERSION               : %s", PJ_VERSION));
    PJ_LOG(3, (id, " PJ_DEBUG                 : %d", PJ_DEBUG));
    PJ_LOG(3, (id, " PJ_FUNCTIONS_ARE_INLINED : %d", PJ_FUNCTIONS_ARE_INLINED));
    PJ_LOG(3, (id, " PJ_POOL_DEBUG            : %d", PJ_POOL_DEBUG));
    PJ_LOG(3, (id, " PJ_HAS_THREADS           : %d", PJ_HAS_THREADS));
    PJ_LOG(3, (id, " PJ_LOG_MAX_LEVEL         : %d", PJ_LOG_MAX_LEVEL));
    PJ_LOG(3, (id, " PJ_LOG_MAX_SIZE          : %d", PJ_LOG_MAX_SIZE));
    PJ_LOG(3, (id, " PJ_LOG_USE_STACK_BUFFER  : %d", PJ_LOG_USE_STACK_BUFFER));
    PJ_LOG(3, (id, " PJ_HAS_TCP               : %d", PJ_HAS_TCP));
    PJ_LOG(3, (id, " PJ_MAX_HOSTNAME          : %d", PJ_MAX_HOSTNAME));
    PJ_LOG(3, (id, " PJ_HAS_SEMAPHORE         : %d", PJ_HAS_SEMAPHORE));
    PJ_LOG(3, (id, " PJ_HAS_EVENT_OBJ         : %d", PJ_HAS_EVENT_OBJ));
    PJ_LOG(3, (id, " PJ_HAS_HIGH_RES_TIMER    : %d", PJ_HAS_HIGH_RES_TIMER));
    PJ_LOG(3, (id, " PJ_(endianness)          : %s", (PJ_IS_BIG_ENDIAN?"big-endian":"little-endian")));
    PJ_LOG(3, (id, " PJ_IOQUEUE_MAX_HANDLES   : %d", PJ_IOQUEUE_MAX_HANDLES));
}

