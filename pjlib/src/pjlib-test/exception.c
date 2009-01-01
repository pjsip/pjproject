/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
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
    PJ_UNREACHED(return -1;)
}

static int throw_id_2(void)
{
    PJ_THROW( ID_2 );
    PJ_UNREACHED(return -1;)
}


static int test(void)
{
    int rc = 0;
    PJ_USE_EXCEPTION;

    /*
     * No exception situation.
     */
    PJ_TRY {
        rc = rc;
    }
    PJ_CATCH_ANY {
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
    PJ_CATCH_ANY {
        int id = PJ_GET_EXCEPTION();
	if (id != ID_1) {
	    PJ_LOG(3,("", "...error: got unexpected exception %d (%s)", 
		      id, pj_exception_id_name(id)));
	    if (!rc) rc = -20;
	}
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
    PJ_CATCH_ANY {
	switch (PJ_GET_EXCEPTION()) {
	case ID_1:
	    if (!rc) rc = -30; break;
	case ID_2:
	    if (!rc) rc = 0; break;
	default:
	    if (!rc) rc = -40;
	    break;
	}
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
    PJ_CATCH_ANY {
	switch (PJ_GET_EXCEPTION()) {
	case ID_1:
	    if (!rc) rc = 0;
	    break;
	default:
	    if (!rc) rc = -60;
	    break;
	}
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
	if ((rc=test()) != 0) {
	    PJ_LOG(3,("", "...failed at i=%d (rc=%d)", i, rc));
	    return rc;
	}
    }
    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_exception_test;
#endif	/* INCLUDE_EXCEPTION_TEST */


