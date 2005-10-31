/* $Header: /pjproject-0.3/pjlib/src/pjlib-test/test.c 4     29/10/05 21:33 Bennylp $
 */
/* 
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/test.c $
 * 
 * 4     29/10/05 21:33 Bennylp
 * Changed echo_server() to echo_srv_sync()
 * 
 * 3     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 1     10/05/05 5:13p Bennylp
 * Created.
 *
 */
#include "test.h"
#include <pjlib.h>
#ifdef _MSC_VER
#  pragma warning(disable:4127)
#endif

#define DO_TEST(test)	do { \
			    PJ_LOG(3, ("test", "Running %s...", #test));  \
			    rc = test; \
			    PJ_LOG(3, ("test",  \
				       "%s(%d)",  \
				       (rc ? "..ERROR" : "..success"), rc)); \
			    if (rc!=0) goto on_return; \
			} while (0)


pj_pool_factory *mem;

int param_echo_sock_type;
const char *param_echo_server = ECHO_SERVER_ADDRESS;
int param_echo_port = ECHO_SERVER_START_PORT;

int test_inner(void)
{
    pj_caching_pool caching_pool;
    const char *filename;
    int line;
    int rc = 0;

    mem = &caching_pool.factory;

    rc = pj_init();
    if (rc != 0) {
	app_perror("pj_init() error!!", rc);
	return rc;
    }
    
    pj_log_set_level(3);
    pj_log_set_decor(PJ_LOG_HAS_NEWLINE);
    pj_dump_config();
    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0 );

#if INCLUDE_ERRNO_TEST
    DO_TEST( errno_test() );
#endif

#if INCLUDE_TIMESTAMP_TEST
    DO_TEST( timestamp_test() );
#endif

#if INCLUDE_EXCEPTION_TEST
    DO_TEST( exception_test() );
#endif

#if INCLUDE_RAND_TEST
    DO_TEST( rand_test() );
#endif

#if INCLUDE_LIST_TEST
    DO_TEST( list_test() );
#endif

#if INCLUDE_POOL_TEST
    DO_TEST( pool_test() );
#endif

#if INCLUDE_POOL_PERF_TEST
    DO_TEST( pool_perf_test() );
#endif

#if INCLUDE_STRING_TEST
    DO_TEST( string_test() );
#endif
    
#if INCLUDE_FIFOBUF_TEST
    DO_TEST( fifobuf_test() );
#endif

#if INCLUDE_RBTREE_TEST
    DO_TEST( rbtree_test() );
#endif

#if INCLUDE_ATOMIC_TEST
    DO_TEST( atomic_test() );
#endif

#if INCLUDE_MUTEX_TEST
    DO_TEST( mutex_test() );
#endif

#if INCLUDE_TIMER_TEST
    DO_TEST( timer_test() );
#endif

#if INCLUDE_SLEEP_TEST
    DO_TEST( sleep_test() );
#endif

#if INCLUDE_THREAD_TEST
    DO_TEST( thread_test() );
#endif

#if INCLUDE_SOCK_TEST
    DO_TEST( sock_test() );
#endif

#if INCLUDE_SOCK_PERF_TEST
    DO_TEST( sock_perf_test() );
#endif

#if INCLUDE_SELECT_TEST
    DO_TEST( select_test() );
#endif

#if INCLUDE_UDP_IOQUEUE_TEST
    DO_TEST( udp_ioqueue_test() );
#endif

#if PJ_HAS_TCP && INCLUDE_TCP_IOQUEUE_TEST
    DO_TEST( tcp_ioqueue_test() );
#endif

#if INCLUDE_IOQUEUE_PERF_TEST
    DO_TEST( ioqueue_perf_test() );
#endif

#if INCLUDE_XML_TEST
    DO_TEST( xml_test() );
#endif

#if INCLUDE_ECHO_SERVER
    //echo_server();
    echo_srv_sync();
#elif INCLUDE_ECHO_CLIENT
    if (param_echo_sock_type == 0)
        param_echo_sock_type = PJ_SOCK_DGRAM;

    echo_client( param_echo_sock_type, 
                 param_echo_server, 
                 param_echo_port);
#endif

    goto on_return;

on_return:

    pj_caching_pool_destroy( &caching_pool );

    PJ_LOG(3,("test", ""));
 
    pj_thread_get_stack_info(pj_thread_this(), &filename, &line);
    PJ_LOG(3,("test", "Stack max usage: %u, deepest: %s:%u", 
	              pj_thread_get_stack_max_usage(pj_thread_this()),
		      filename, line));
    if (rc == 0)
	PJ_LOG(3,("test", "Looks like everything is okay!.."));
    else
	PJ_LOG(3,("test", "Test completed with error(s)"));
    return 0;
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
