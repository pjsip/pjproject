/* $Header: /pjproject-0.3/pjlib/src/pjlib-test/rand.c 1     10/05/05 5:13p Bennylp $ */
/* 
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/rand.c $
 * 
 * 1     10/05/05 5:13p Bennylp
 * Created.
 *
 */
#include <pj/rand.h>
#include <pj/log.h>
#include "test.h"

#if INCLUDE_RAND_TEST

#define COUNT  1024
static int values[COUNT];

/*
 * rand_test(), simply generates COUNT number of random number and
 * check that there's no duplicate numbers.
 */
int rand_test(void)
{
    int i;

    for (i=0; i<COUNT; ++i) {
	int j;

	values[i] = pj_rand();
	for (j=0; j<i; ++j) {
	    if (values[i] == values[j]) {
		PJ_LOG(3,("test", "error: duplicate value %d at %d-th index",
			 values[i], i));
		return -10;
	    }
	}
    }

    return 0;
}

#endif	/* INCLUDE_RAND_TEST */

