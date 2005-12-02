/* $Header: /pjproject/pjlib/src/test/exception_test.cpp 2     2/24/05 10:34a Bennylp $
 */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pj/except.h>

#define	ID_1	1
#define ID_2	2

static int throw_id_1()
{
    PJ_THROW( ID_1 );
    return -1;
}

static int throw_id_2()
{
    PJ_THROW( ID_2 );
    return -1;
}


static int test()
{
    PJ_USE_EXCEPTION;
    int rc = 0;

    // Must be able to catch exception that's thrown
    PJ_TRY {
	rc = throw_id_1();
    }
    PJ_CATCH( ID_1 ) {
    	rc = 0;
    }
    PJ_DEFAULT {
	rc = -1;
    }
    PJ_END;

    if (rc != 0)
	return -2;

    // Must be able to handle multiple exceptions.
    PJ_TRY {
	rc = throw_id_2();
    }
    PJ_CATCH( ID_1 ) {
	rc = -3;
    }
    PJ_CATCH( ID_2 ) {
	rc = 0;
    }
    PJ_DEFAULT {
	rc = -4;
    }
    PJ_END;

    if (rc != 0)
	return -5;

    // Test default handler.
    PJ_TRY {
	rc = throw_id_1();
    }
    PJ_CATCH( ID_2 ) {
	rc = -6;
    }
    PJ_DEFAULT {
	rc = 0;
    }
    PJ_END;

    if (rc != 0)
	return -7;

    return 0;
}

int exception_test()
{
    int i, rc;
    enum { LOOP = 10 };

    for (i=0; i<LOOP; ++i) {
	if ((rc=test()) != 0)
	    return rc;
    }
    return 0;
}

