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

#include <pj/string.h>
#include <pj/sock.h>
#include <pj/log.h>
#include <pj/unittest.h>
#include <stdio.h>

//#if defined(PJ_WIN32) && PJ_WIN32!=0
#if 0
#include <windows.h>
static void boost(void)
{
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
}
#else
#define boost()
#endif


#if defined(PJ_SUNOS) && PJ_SUNOS!=0

#include <signal.h>
static void init_signals()
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;

    sigaction(SIGALRM, &act, NULL);
}

#elif (PJ_LINUX || PJ_DARWINOS) && defined(PJ_HAS_EXECINFO_H) && PJ_HAS_EXECINFO_H != 0

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static void print_stack(int sig)
{
    void *array[16];
    size_t size;

    size = backtrace(array, 16);
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

static void init_signals(void)
{
    signal(SIGSEGV, &print_stack);
    signal(SIGABRT, &print_stack);
}

#else
#define init_signals()
#endif

static void usage()
{
    puts("Usage:");
    puts("  pjlib-test [OPTION] [test_to_run] [..]");
    puts("");
    puts("where OPTIONS:");
    puts("");
    puts("  -h, --help   Show this help screen");
    puts("  --ci-mode    Running in slow CI  mode");
    puts("  -l 0,1,2     0: Don't show logging after tests");
    puts("               1: Show logs of failed tests (default)");
    puts("               2: Show logs of all tests");
    puts("  -w N         Set N worker threads (0: disable worker threads)");
    puts("  -L, --list   List the tests and exit");
    puts("  --stop-err   Stop testing on error");
    puts("  --skip-e     Skip essential tests");
    puts("  -i           Ask ENTER before quitting");
    puts("  -n           Do not trap signals");
    puts("  -p PORT      Use port PORT for echo port");
    puts("  -s SERVER    Use SERVER as ech oserver");
    puts("  -t ucp,tcp   Set echo socket type to UDP or TCP");
}


int main(int argc, char *argv[])
{
    int i, rc;
    int interractive = 0;
    int no_trap = 0;
    pj_status_t status;
    char *s;

    boost();

    /* 
     * Parse arguments
     */
    if (pj_argparse_get("-h", &argc, argv) ||
        pj_argparse_get("--help", &argc, argv))
    {
        usage();
        return 0;
    }
    interractive = pj_argparse_get("-i", &argc, argv);
    no_trap = pj_argparse_get("-n", &argc, argv);
    status = pj_argparse_get_int("-p", &argc, argv, &test_app.param_echo_port);
    if (status!=PJ_SUCCESS && status!=PJ_ENOTFOUND) {
        puts("Error: invalid/missing value for -p option");
        usage();
        return 1;
    }
    status = pj_argparse_get_str("-s", &argc, argv, 
                                 (char**)&test_app.param_echo_server);
    if (status!=PJ_SUCCESS && status!=PJ_ENOTFOUND) {
        puts("Error: value is required for -s option");
        usage();
        return 1;
    }

    status = pj_argparse_get_str("-t", &argc, argv, &s);
    if (status==PJ_SUCCESS) {
        if (pj_ansi_stricmp(s, "tcp")==0)
            test_app.param_echo_sock_type = pj_SOCK_STREAM();
        else if (pj_ansi_stricmp(s, "udp")==0)
            test_app.param_echo_sock_type = pj_SOCK_DGRAM();
        else {
            printf("Error: unknown socket type %s for -t option\n", s);
            usage();
            return 1;
        }
    } else if (status!=PJ_ENOTFOUND) {
        puts("Error: value is required for -t option");
        usage();
        return 1;
    }

    test_app.param_list_test = pj_argparse_get("-L", &argc, argv) ||
                               pj_argparse_get("--list", &argc, argv);
    test_app.param_stop_on_error = pj_argparse_get("--stop-err", &argc, argv);
    test_app.param_skip_essentials = pj_argparse_get("--skip-e", &argc, argv);
    test_app.param_ci_mode = pj_argparse_get("--ci-mode", &argc, argv);

    status = pj_argparse_get_int("-l", &argc, argv, &i);
    if (status==PJ_SUCCESS) {
        if (i==0)
            test_app.param_unittest_logging_policy = PJ_TEST_NO_TEST;
        else if (i==1)
            test_app.param_unittest_logging_policy = PJ_TEST_FAILED_TESTS;
        else if (i==2)
            test_app.param_unittest_logging_policy = PJ_TEST_ALL_TESTS;
        else {
            printf("Error: invalid value %d for -l option\n", i);
            usage();
            return 1;
        }
    } else if (status!=PJ_ENOTFOUND) {
        puts("Error: invalid/missing value for -l option");
        usage();
        return 1;
    }

    status = pj_argparse_get_int("-w", &argc, argv, 
                                 (int*)&test_app.param_unittest_nthreads);
    if (status==PJ_SUCCESS) {
        if (test_app.param_unittest_nthreads > 100 || 
            test_app.param_unittest_nthreads < 0) 
        {
            printf("Error: value %d is not valid for -w option\n", 
                   test_app.param_unittest_nthreads);
            usage();
            return 1;
        }
    } else if (status!=PJ_ENOTFOUND) {
        puts("Error: invalid/missing value for -w option");
        usage();
    }

    if (!no_trap) {
        init_signals();
    }

    if (pj_argparse_peek_next_option(argv)) {
        printf("Error: unknown argument %s\n", 
               pj_argparse_peek_next_option(argv));
        usage();
        return 1;
    }

    /* argc/argv now contains option values only, if any */
    rc = test_main(argc, argv);

    if (interractive) {
        char s[10];
        puts("");
        puts("Press <ENTER> to exit");
        if (!fgets(s, sizeof(s), stdin))
            return rc;
    }

    return rc;
}

