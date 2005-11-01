/* $Header: /pjproject-0.3/pjlib/src/pjlib-samples/log.c 2     10/14/05 12:26a Bennylp $ */
/*
 * $Log: /pjproject-0.3/pjlib/src/pjlib-samples/log.c $
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     10/10/05 3:16p Bennylp
 * Created.
 *
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

