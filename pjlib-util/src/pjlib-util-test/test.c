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
#include "test.h"
#include <pjlib.h>

void app_perror(const char *msg, pj_status_t rc)
{
    char errbuf[256];

    PJ_CHECK_STACK();

    pj_strerror(rc, errbuf, sizeof(errbuf));
    PJ_LOG(1,("test", "%s: [pj_status_t=%d] %s", msg, rc, errbuf));
}

#define DO_TEST(test)	do { \
			    PJ_LOG(3, ("test", "Running %s...", #test));  \
			    rc = test; \
			    PJ_LOG(3, ("test",  \
				       "%s(%d)",  \
				       (char*)(rc ? "..ERROR" : "..success"), rc)); \
			    if (rc!=0) goto on_return; \
			} while (0)


pj_pool_factory *mem;


static int test_inner(void)
{
    pj_caching_pool caching_pool;
    int rc = 0;

    mem = &caching_pool.factory;

    pj_log_set_level(3);
    pj_log_set_decor(PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME | 
                     PJ_LOG_HAS_MICRO_SEC);

    rc = pj_init();
    if (rc != 0) {
	app_perror("pj_init() error!!", rc);
	return rc;
    }
    
    pj_dump_config();
    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0 );
    DO_TEST(xml_test());

on_return:
    return rc;
}

int test_main(void)
{
    PJ_USE_EXCEPTION;

    PJ_TRY {
        return test_inner();
    }
    PJ_DEFAULT {
        int id = PJ_GET_EXCEPTION();
        PJ_LOG(3,("test", "FATAL: unhandled exception id %d (%s)", 
                  id, pj_exception_id_name(id)));
    }
    PJ_END;

    return -1;
}

