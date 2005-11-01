/* $Header: /pjproject-0.3/pjlib/src/pj/types.c 4     9/17/05 10:37a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/types.c $
 * 
 * 4     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#include <pj/types.h>
#include <pj/os.h>

void pj_time_val_normalize(pj_time_val *t)
{
    PJ_CHECK_STACK();

    if (t->msec >= 1000) {
	do {
	    t->sec++;
	    t->msec -= 1000;
        } while (t->msec >= 1000);
    }
    else if (t->msec <= -1000) {
	do {
	    t->sec--;
	    t->msec += 1000;
        } while (t->msec <= -1000);
    }

    if (t->sec >= 1 && t->msec < 0) {
	t->sec--;
	t->msec += 1000;

    } else if (t->sec < 0 && t->msec > 0) {
	t->sec++;
	t->msec -= 1000;
    }
}
