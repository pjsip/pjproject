/* $Header: /pjproject-0.3/pjlib/src/pj/log_writer_printk.c 2     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/log_writer_printk.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     9/22/05 10:37a Bennylp
 * Created.
 * 
 */
#include <pj/log.h>
#include <pj/os.h>

PJ_DEF(void) pj_log_write(int level, const char *buffer, int len)
{
    PJ_CHECK_STACK();
    printk(KERN_INFO "%s", buffer);
}

