/* $Id$
 *

 */
/*
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/sleep.c $
 * 
 * 3     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     10/11/05 12:53a Bennylp
 * Created.
 *
 */
#include "test.h"

/**
 * \page page_pjlib_sleep_test Test: Sleep, Time, and Timestamp
 *
 * This file provides implementation of \b sleep_test().
 *
 * \section sleep_test_sec Scope of the Test
 *
 * This tests:
 *  - whether pj_thread_sleep() works.
 *  - whether pj_gettimeofday() works.
 *  - whether pj_get_timestamp() and friends works.
 *
 * API tested:
 *  - pj_thread_sleep()
 *  - pj_gettimeofday()
 *  - PJ_TIME_VAL_SUB()
 *  - PJ_TIME_VAL_LTE()
 *  - pj_get_timestamp()
 *  - pj_get_timestamp_freq() (implicitly)
 *  - pj_elapsed_time()
 *  - pj_elapsed_usec()
 *
 *
 * This file is <b>pjlib-test/sleep.c</b>
 *
 * \include pjlib-test/sleep.c
 */

#if INCLUDE_SLEEP_TEST

#include <pjlib.h>

#define THIS_FILE   "sleep_test"

static int simple_sleep_test(void)
{
    enum { COUNT = 5 };
    int i;
    pj_status_t rc;
    
    PJ_LOG(3,(THIS_FILE, "..will write messages every 1 second:"));
    
    for (i=0; i<COUNT; ++i) {
	rc = pj_thread_sleep(1000);
	if (rc != PJ_SUCCESS) {
	    app_perror("...error: pj_thread_sleep()", rc);
	    return -10;
	}
	PJ_LOG(3,(THIS_FILE, "...wake up.."));
    }

    return 0;
}

static int sleep_duration_test(void)
{
    enum { MIS = 20, DURATION = 1000, DURATION2 = 500 };
    pj_status_t rc;

    PJ_LOG(3,(THIS_FILE, "..running sleep duration test"));

    /* Test pj_thread_sleep() and pj_gettimeofday() */
    {
        pj_time_val start, stop;
	pj_uint32_t msec;

        /* Mark start of test. */
        rc = pj_gettimeofday(&start);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: pj_gettimeofday()", rc);
            return -10;
        }

        /* Sleep */
        rc = pj_thread_sleep(DURATION);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: pj_thread_sleep()", rc);
            return -20;
        }

        /* Mark end of test. */
        rc = pj_gettimeofday(&stop);

        /* Calculate duration (store in stop). */
        PJ_TIME_VAL_SUB(stop, start);

	/* Convert to msec. */
	msec = PJ_TIME_VAL_MSEC(stop);

	/* Check if it's within range. */
	if (msec < DURATION * (100-MIS)/100 ||
	    msec > DURATION * (100+MIS)/100)
	{
	    PJ_LOG(3,(THIS_FILE, 
		      "...error: slept for %d ms instead of %d ms "
		      "(outside %d%% err window)",
		      msec, DURATION, MIS));
	    return -30;
	}
    }


    /* Test pj_thread_sleep() and pj_get_timestamp() and friends */
    {
	pj_time_val t1, t2;
        pj_timestamp start, stop;
        pj_time_val elapsed;
	pj_uint32_t msec;

        /* Mark start of test. */
        rc = pj_get_timestamp(&start);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: pj_get_timestamp()", rc);
            return -60;
        }

	/* ..also with gettimeofday() */
	pj_gettimeofday(&t1);

        /* Sleep */
        rc = pj_thread_sleep(DURATION2);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: pj_thread_sleep()", rc);
            return -70;
        }

        /* Mark end of test. */
        pj_get_timestamp(&stop);

	/* ..also with gettimeofday() */
	pj_gettimeofday(&t2);

	/* Compare t1 and t2. */
	if (PJ_TIME_VAL_LTE(t2, t1)) {
	    PJ_LOG(3,(THIS_FILE, "...error: t2 is less than t1!!"));
	    return -75;
	}

        /* Get elapsed time in time_val */
        elapsed = pj_elapsed_time(&start, &stop);

	msec = PJ_TIME_VAL_MSEC(elapsed);

	/* Check if it's within range. */
	if (msec < DURATION2 * (100-MIS)/100 ||
	    msec > DURATION2 * (100+MIS)/100)
	{
	    PJ_LOG(3,(THIS_FILE, 
		      "...error: slept for %d ms instead of %d ms "
		      "(outside %d%% err window)",
		      msec, DURATION2, MIS));
	    return -30;
	}
    }

    /* All done. */
    return 0;
}

int sleep_test()
{
    int rc;

    rc = simple_sleep_test();
    if (rc != PJ_SUCCESS)
	return rc;

    rc = sleep_duration_test();
    if (rc != PJ_SUCCESS)
	return rc;

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_sleep_test;
#endif  /* INCLUDE_SLEEP_TEST */
