/* $Id$
 */
#include <pj/log.h>

/**
 * \page page_pjlib_samples_log_c Example: Log, Hello World
 *
 * Very simple program to write log.
 *
 * \includelineno pjlib-samples/log.c
 */

int main()
{
    pj_status_t rc;

    // Error handling omited for clarity
    
    // Must initialize PJLIB first!
    rc = pj_init();

    PJ_LOG(3, ("main.c", "Hello world!"));

    return 0;
}

