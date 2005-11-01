/* $Id$
 *
 */
#include <pj/log.h>
#include <pj/os.h>

PJ_DEF(void) pj_log_write(int level, const char *buffer, int len)
{
    PJ_CHECK_STACK();
    printk(KERN_INFO "%s", buffer);
}

