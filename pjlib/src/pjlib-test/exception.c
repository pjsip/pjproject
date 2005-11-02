/* $Id$
 */
#include "test.h"


/**
 * \page page_pjlib_exception_test Test: Exception Handling
 *
 * This file provides implementation of \b exception_test(). It tests the
 * functionality of the exception handling API.
 *
 * @note This test use static ID not acquired through proper registration.
 * This is not recommended, since it may create ID collissions.
 *
 * \section exception_test_sec Scope of the Test
 *
 * Some scenarios tested:
 *  - no exception situation
 *  - basic TRY/CATCH
 *  - multiple exception handlers
 *  - default handlers
 *
 *
 * This file is <b>pjlib-test/exception.c</b>
 *
 * \include pjlib-test/exception.c
 */


#if INCLUDE_EXCEPTION_TEST

#include <pjlib.h>

#define	ID_1	1
#define ID_2	2

static int throw_id_1(void)
{
    PJ_THROW( ID_1 );
    return -1;
}

static int throw_id_2(void)
{
    PJ_THROW( ID_2 );
    return -1;
}


static int test(void)
{
    PJ_USE_EXCEPTION;
    int rc = 0;

    /*
     * No exception situation.
     */
    PJ_TRY {
        rc = rc;
    }
    PJ_CATCH( ID_1 ) {
        rc = -2;
    }
    PJ_DEFAULT {
        rc = -3;
    }
    PJ_END;

    if (rc != 0)
	return rc;


    /*
     * Basic TRY/CATCH
     */ 
    PJ_TRY {
	rc = throw_id_1();

	// should not reach here.
	rc = -10;
    }
    PJ_CATCH( ID_1 ) {
	if (!rc) rc = 0;
    }
    PJ_DEFAULT {
	if (!rc) rc = -20;
    }
    PJ_END;

    if (rc != 0)
	return rc;

    /*
     * Multiple exceptions handlers
     */
    PJ_TRY {
	rc = throw_id_2();
	// should not reach here.
	rc = -25;
    }
    PJ_CATCH( ID_1 ) {
	if (!rc) rc = -30;
    }
    PJ_CATCH( ID_2 ) {
	if (!rc) rc = 0;
    }
    PJ_DEFAULT {
	if (!rc) rc = -40;
    }
    PJ_END;

    if (rc != 0)
	return rc;

    /*
     * Test default handler.
     */
    PJ_TRY {
	rc = throw_id_1();
	// should not reach here
	rc = -50;
    }
    PJ_CATCH( ID_2 ) {
	if (!rc) rc = -60;
    }
    PJ_DEFAULT {
	if (!rc) rc = 0;
    }
    PJ_END;

    if (rc != 0)
	return rc;

    return 0;
}

int exception_test(void)
{
    int i, rc;
    enum { LOOP = 10 };

    for (i=0; i<LOOP; ++i) {
	if ((rc=test()) != 0)
	    return rc;
    }
    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_exception_test;
#endif	/* INCLUDE_EXCEPTION_TEST */


