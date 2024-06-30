/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjsip.h>

#define THIS_FILE   "test.c"

pjsip_endpoint *endpt;
pj_caching_pool caching_pool;
int log_level = 3;
int param_log_decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME | PJ_LOG_HAS_SENDER |
                      PJ_LOG_HAS_MICRO_SEC | PJ_LOG_HAS_INDENT;

static pj_oshandle_t fd_report;
const char *system_name = "Unknown";
static char buf[1024];
struct test_app_t test_app;
struct tsx_test_param tsx_test[MAX_TSX_TESTS];

void app_perror(const char *msg, pj_status_t rc)
{
    char errbuf[256];

    PJ_CHECK_STACK();

    pj_strerror(rc, errbuf, sizeof(errbuf));
    PJ_LOG(3,(THIS_FILE, "%s: [pj_status_t=%d] %s", msg, rc, errbuf));

}

void flush_events(unsigned duration)
{
    pj_time_val stop_time;

    pj_gettimeofday(&stop_time);
    stop_time.msec += duration;
    pj_time_val_normalize(&stop_time);

    /* Process all events for the specified duration. */
    for (;;) {
        pj_time_val timeout = {0, 1}, now;

        pjsip_endpt_handle_events(endpt, &timeout);

        pj_gettimeofday(&now);
        if (PJ_TIME_VAL_GTE(now, stop_time))
            break;
    }
}

pj_status_t register_static_modules(pj_size_t *count, pjsip_module **modules)
{
    PJ_UNUSED_ARG(modules);

    *count = 0;
    return PJ_SUCCESS;
}

static pj_status_t init_report(void)
{
    char tmp[80];
    pj_time_val timestamp;
    pj_parsed_time date_time;
    pj_ssize_t len;
    pj_status_t status;
    
    pj_ansi_snprintf(tmp, sizeof(tmp),
                     "pjsip-static-bench-%s-%s.htm", PJ_OS_NAME, PJ_CC_NAME);

    status = pj_file_open(NULL, tmp, PJ_O_WRONLY, &fd_report);
    if (status != PJ_SUCCESS)
        return status;

    /* Title */
    len = pj_ansi_snprintf(buf, sizeof(buf),
                               "<HTML>\n"
                               " <HEAD>\n"
                               "  <TITLE>PJSIP %s (%s) - Static Benchmark</TITLE>\n"
                               " </HEAD>\n"
                               "<BODY>\n"
                               "\n", 
                               PJ_VERSION,
                               (PJ_DEBUG ? "Debug" : "Release"));
    pj_file_write(fd_report, buf, &len);


    /* Title */
    len = pj_ansi_snprintf(buf, sizeof(buf),
                               "<H1>PJSIP %s (%s) - Static Benchmark</H1>\n", 
                               PJ_VERSION,
                               (PJ_DEBUG ? "Debug" : "Release"));
    pj_file_write(fd_report, buf, &len);

    len = pj_ansi_snprintf(buf, sizeof(buf), 
                               "<P>Below is the benchmark result generated "
                               "by <b>test-pjsip</b> program. The program "
                               "is single-threaded only.</P>\n");
    pj_file_write(fd_report, buf, &len);


    /* Write table heading */
    len = pj_ansi_snprintf(buf, sizeof(buf), 
                               "<TABLE border=\"1\" cellpadding=\"4\">\n"
                               "  <TR><TD bgColor=\"aqua\" align=\"center\">Variable</TD>\n"
                               "      <TD bgColor=\"aqua\" align=\"center\">Value</TD>\n"
                               "      <TD bgColor=\"aqua\" align=\"center\">Description</TD>\n"
                               "  </TR>\n");
    pj_file_write(fd_report, buf, &len);


    /* Write version */
    report_sval("version", PJ_VERSION, "", "PJLIB/PJSIP version");


    /* Debug or release */
    report_sval("build-type", (PJ_DEBUG ? "Debug" : "Release"), "", "Build type");


    /* Write timestamp */
    pj_gettimeofday(&timestamp);
    report_ival("timestamp", timestamp.sec, "", "System timestamp of the test");


    /* Write time of day */
    pj_time_decode(&timestamp, &date_time);
    len = pj_ansi_snprintf(tmp, sizeof(tmp), "%04d-%02d-%02d %02d:%02d:%02d",
                               date_time.year, date_time.mon+1, date_time.day,
                               date_time.hour, date_time.min, date_time.sec);
    report_sval("date-time", tmp, "", "Date/time of the test");


    /* Write System */
    report_sval("system", system_name, "", "System description");


    /* Write OS type */
    report_sval("os-family", PJ_OS_NAME, "", "Operating system family");


    /* Write CC name */
    len = pj_ansi_snprintf(tmp, sizeof(tmp), "%s-%d.%d.%d", PJ_CC_NAME,
                          PJ_CC_VER_1, PJ_CC_VER_2, PJ_CC_VER_2);
    report_sval("cc-name", tmp, "", "Compiler name and version");


    return PJ_SUCCESS;
}

void report_sval(const char *name, const char* value, const char *valname, 
                 const char *desc)
{
    pj_ssize_t len;

    len = pj_ansi_snprintf(buf, sizeof(buf),
                               "  <TR><TD><TT>%s</TT></TD>\n"
                               "      <TD align=\"right\"><B>%s %s</B></TD>\n"
                               "      <TD>%s</TD>\n"
                               "  </TR>\n",
                               name, value, valname, desc);
    pj_file_write(fd_report, buf, &len);
}


void report_ival(const char *name, int value, const char *valname, 
                 const char *desc)
{
    pj_ssize_t len;

    len = pj_ansi_snprintf(buf, sizeof(buf),
                               "  <TR><TD><TT>%s</TT></TD>\n"
                               "      <TD align=\"right\"><B>%d %s</B></TD>\n"
                               "      <TD>%s</TD>\n"
                               "  </TR>\n",
                               name, value, valname, desc);
    pj_file_write(fd_report, buf, &len);

}

static void close_report(void)
{
    pj_ssize_t len;

    if (fd_report) {
        len = pj_ansi_snprintf(buf, sizeof(buf), "</TABLE>\n</BODY>\n</HTML>\n");
        pj_file_write(fd_report, buf, &len);

        pj_file_close(fd_report);
    }
}


int test_main(int argc, char *argv[])
{
    pj_status_t rc;
    const char *filename;
    unsigned tsx_test_cnt=0;
#if INCLUDE_TSX_TEST
    unsigned i;
    pjsip_transport *tp;
#if PJ_HAS_TCP
    pjsip_tpfactory *tpfactory;
#endif  /* PJ_HAS_TCP */
#endif  /* INCLUDE_TSX_TEST */
    int line;

    pj_log_set_level(log_level);
    pj_log_set_decor(param_log_decor);

    PJ_TEST_SUCCESS(pj_init(), NULL, return 10);
    PJ_TEST_SUCCESS(pjlib_util_init(), NULL, return 20);
    PJ_TEST_SUCCESS(init_report(), NULL, return 30);

    if (test_app.ut_app.prm_config) {
        pj_dump_config();
    }

    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 
                          PJSIP_TEST_MEM_SIZE );

    PJ_TEST_SUCCESS(pjsip_endpt_create(&caching_pool.factory, "endpt", &endpt),
                    NULL, { 
                        pj_caching_pool_destroy(&caching_pool); 
                        return 40; 
                    });

    /* Init logger module. */
    init_msg_logger();
    msg_logger_set_enabled(1);

    /* Start transaction layer module. */
    PJ_TEST_SUCCESS(rc = pjsip_tsx_layer_init_module(endpt),
                    NULL, goto on_return);

    /* Create loop transport. */
    PJ_TEST_SUCCESS(rc = pjsip_loop_start(endpt, NULL),
                    NULL, goto on_return);

    tsx_test[tsx_test_cnt].port = 5060;
    tsx_test[tsx_test_cnt].tp_type = "loop-dgram";
    tsx_test[tsx_test_cnt].type = PJSIP_TRANSPORT_LOOP_DGRAM;
    ++tsx_test_cnt;

    PJ_TEST_SUCCESS(rc=ut_app_init1(&test_app.ut_app, &caching_pool.factory), 
                    NULL, goto on_return);

#if INCLUDE_URI_TEST
    UT_ADD_TEST(&test_app.ut_app, uri_test, 0);
#endif

#if INCLUDE_MSG_TEST
    UT_ADD_TEST(&test_app.ut_app, msg_test, 0);
    UT_ADD_TEST(&test_app.ut_app, msg_err_test, 0);
#endif

#if INCLUDE_MULTIPART_TEST
    UT_ADD_TEST(&test_app.ut_app, multipart_test, 0);
#endif

#if INCLUDE_TXDATA_TEST
    UT_ADD_TEST(&test_app.ut_app, txdata_test, 0);
#endif

#if INCLUDE_TSX_BENCH
    UT_ADD_TEST(&test_app.ut_app, tsx_bench, 0);
#endif

#if INCLUDE_RESOLVE_TEST
    UT_ADD_TEST(&test_app.ut_app, resolve_test, 0);
#endif

#if INCLUDE_INV_OA_TEST
    UT_ADD_TEST(&test_app.ut_app, inv_offer_answer_test, 0);
#endif

#if INCLUDE_REGC_TEST
    UT_ADD_TEST(&test_app.ut_app, regc_test, 0);
#endif

#if INCLUDE_TSX_TEST
    PJ_TEST_SUCCESS(rc=pjsip_udp_transport_start(endpt, NULL, NULL, 1, &tp),
                    NULL, goto on_return);
    tsx_test[tsx_test_cnt].port = tp->local_name.port;
    tsx_test[tsx_test_cnt].tp_type = "udp";
    tsx_test[tsx_test_cnt].type = PJSIP_TRANSPORT_UDP;
    ++tsx_test_cnt;

#if PJ_HAS_TCP
    PJ_TEST_SUCCESS(rc=pjsip_tcp_transport_start(endpt, NULL, 1, &tpfactory),
                    NULL, goto on_return);
    tsx_test[tsx_test_cnt].port = tpfactory->addr_name.port;
    tsx_test[tsx_test_cnt].tp_type = "tcp";
    tsx_test[tsx_test_cnt].type = PJSIP_TRANSPORT_TCP;
    ++tsx_test_cnt;
#endif

    pj_assert(tsx_test[0].type == PJSIP_TRANSPORT_LOOP_DGRAM);

    for (i = 0; i < tsx_test_cnt; ++i) {
        UT_ADD_TEST1(&test_app.ut_app, tsx_basic_test, (void*)(long)i, 0);
        /* tsx_uac_test for loop dgram will be added later because it's exclusive */
        if (i!=0)
            UT_ADD_TEST1(&test_app.ut_app, tsx_uac_test, (void*)(long)i, 0);
        UT_ADD_TEST1(&test_app.ut_app, tsx_uas_test, (void*)(long)i, 0);
    }

    if (tsx_test_cnt) {
        /* tsx_uac_test for loop dgram sets various delay to the loop dgram.
         * This will mess up other tests that are also uses loop-dgram
         * such as tsx_uas_test(0). So run it exclusively. */
        UT_ADD_TEST1(&test_app.ut_app, tsx_uac_test, (void*)(long)0,
                     PJ_TEST_EXCLUSIVE);
        
        /* tsx_transport_failure_test() also sets transport loop delay, hence
         * it must run exclusively. */
        UT_ADD_TEST1(&test_app.ut_app, tsx_transport_failure_test,
                     (void*)(long)0, PJ_TEST_EXCLUSIVE);

    }
#endif

    /* Note: put exclusive tests last */
    
#if INCLUDE_UDP_TEST
    /* Transport tests share same testing codes which are not reentrant */
    UT_ADD_TEST(&test_app.ut_app, transport_udp_test, PJ_TEST_EXCLUSIVE);
#endif

#if INCLUDE_LOOP_TEST
    UT_ADD_TEST(&test_app.ut_app, transport_loop_test, PJ_TEST_EXCLUSIVE);
#endif

#if INCLUDE_TCP_TEST
    UT_ADD_TEST(&test_app.ut_app, transport_tcp_test, PJ_TEST_EXCLUSIVE);
#endif

    /*
     * Better be last because it recreates the endpt
     */
#if INCLUDE_TSX_DESTROY_TEST
    UT_ADD_TEST(&test_app.ut_app, tsx_destroy_test, PJ_TEST_EXCLUSIVE);
#endif

    if (ut_run_tests(&test_app.ut_app, "pjsip tests", argc, argv)) {
        rc = 99;
    } else {
        rc = 0;
    }

    ut_app_destroy(&test_app.ut_app);

on_return:
    flush_events(500);

    /* Show additional info on the log. e.g: not released memory pool. */
    // Commented out to make the output tidy :D (blp)
    //pj_log_set_level(4);

    /* Dumping memory pool usage */
    /* Field peak_used_size is deprecated by #3897 */
    //PJ_LOG(3,(THIS_FILE, "Peak memory size=%lu MB",
    //                     (unsigned long)
    //                     (caching_pool.peak_used_size / 1000000)));

    pjsip_endpt_destroy(endpt);
    pj_caching_pool_destroy(&caching_pool);

    PJ_LOG(3,(THIS_FILE, " "));
 
    pj_thread_get_stack_info(pj_thread_this(), &filename, &line);
    PJ_LOG(3,(THIS_FILE, "Stack max usage: %u, deepest: %s:%u", 
                      pj_thread_get_stack_max_usage(pj_thread_this()),
                      filename, line));
    if (rc == 0)
        PJ_LOG(3,(THIS_FILE, "Looks like everything is okay!.."));
    else
        PJ_LOG(3,(THIS_FILE, "Test completed with error(s)"));

    report_ival("test-status", rc, "", "Overall test status/result (0==success)");
    close_report();
    return rc;
}

