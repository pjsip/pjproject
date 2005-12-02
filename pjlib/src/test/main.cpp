/* $Header: /pjproject/pjlib/src/test/main.cpp 7     6/04/05 4:30p Bennylp $
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
#include "libpj_test.h"
#include <pj/pool.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/compat.h>
#include <stdio.h>
#ifdef _MSC_VER
#  pragma warning(disable:4127)
#endif

#define DO_TEST(test)	do { \
			    printf("Running %s...\n", #test);  \
			    fflush(stdout); \
			    int rc = test; \
			    printf( "%s(%d)\n", (rc ? "..ERROR" : "..success"), rc); \
			} while (0)


#define INCLUDE_POOL_TEST	    1
#define INCLUDE_EXCEPTION_TEST	    1
#define INCLUDE_OS_TEST		    1
#define INCLUDE_TIMER_TEST	    1
#define INCLUDE_RBTREE_TEST	    1
#define INCLUDE_UDP_IOQUEUE_TEST    1
#define INCLUDE_TCP_IOQUEUE_TEST    1
#define INCLUDE_LIST_TEST	    1
#define INCLUDE_FIFOBUF_TEST	    1
#define INCLUDE_XML_TEST	    1

pj_pool_factory *mem;

int main()
{
    pj_caching_pool caching_pool;

    mem = &caching_pool.factory;

    pj_log_set_level(4);
    pj_dump_config();
    pj_init();
    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0 );

#if INCLUDE_FIFOBUF_TEST
    DO_TEST( fifobuf_test() );
    fflush(stdout);
#endif

#if INCLUDE_POOL_TEST
    DO_TEST( pool_test() );
    fflush(stdout);
#endif

#if INCLUDE_EXCEPTION_TEST
    DO_TEST( exception_test() );
    fflush(stdout);
#endif

#if INCLUDE_OS_TEST
    DO_TEST( os_test() );
    fflush(stdout);
#endif

#if INCLUDE_TIMER_TEST
    DO_TEST( timer_test() );
    fflush(stdout);
#endif

#if INCLUDE_RBTREE_TEST
    DO_TEST( rbtree_test() );
    fflush(stdout);
#endif

#if INCLUDE_UDP_IOQUEUE_TEST
    DO_TEST( udp_ioqueue_test() );
    fflush(stdout);
#endif

#if PJ_HAS_TCP && INCLUDE_TCP_IOQUEUE_TEST
    DO_TEST( tcp_ioqueue_test() );
    fflush(stdout);
#endif

#if INCLUDE_LIST_TEST
    DO_TEST( list_test() );
    fflush(stdout);
#endif

#if INCLUDE_XML_TEST
    DO_TEST( xml_test() );
    fflush(stdout);
#endif

    pj_caching_pool_destroy( &caching_pool );

    char temp[3];
    puts("");
    puts("Press <ENTER> to quit");
    fflush(stdout);
    fgets(temp, sizeof(temp), stdin);
    return 0;
}

