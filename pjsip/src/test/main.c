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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pj/argparse.h>

extern const char *system_name;

static void warn(void)
{
    puts("*******************************************************************");
    puts("**                       W A R N I N G                           **");
    puts("*******************************************************************");
    puts("** Due to centralized event processing in PJSIP, events may be   **");
    puts("** read by different thread than the test's thread. This may     **");
    puts("** cause logging to be sent to the wrong test when multithreaded **");
    puts("** testing is used. The test results are correct, but do not     **");
    puts("** trust the log.                                                **");
    puts("** For debugging with correct logging, use \"-w 0 --log-no-cache\" **");
    puts("*******************************************************************");
}

static void usage(void)
{
    puts("Usage:");
    puts("  pjsip-test [OPTIONS] [test_to_run] [test to run] [..]");
    puts("");
    puts("where OPTIONS:");
    puts("");
    puts("  -h,--help        Show this screen");

    ut_usage();

    puts("  -i,--interractive Key input at the end.");
    puts("  -n,--no-trap     Let signals be handled by the OS");
    puts("  --log-level N    Set log level (0-6)");
    puts("  -s,--system NAME Set system name to NAME");
}

#if (PJ_LINUX || PJ_DARWINOS) && defined(PJ_HAS_EXECINFO_H) && PJ_HAS_EXECINFO_H != 0

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

int main(int argc, char *argv[])
{
    int interractive = 0;
    int retval;
    int no_trap = 0;

    warn();

    if (pj_argparse_get_bool("-h", &argc, argv) ||
        pj_argparse_get_bool("--help", &argc, argv))
    {
        usage();
        return 0;
    }

    ut_app_init0(&test_app.ut_app);

    interractive = pj_argparse_get_bool("-i", &argc, argv);
    no_trap = pj_argparse_get_bool("-n", &argc, argv);
    if (pj_argparse_get_str("-s", &argc, argv, (char**)&system_name) ||
        pj_argparse_get_str("--system", &argc, argv, (char**)&system_name))
    {
        usage();
        return 1;
    }

    if (pj_argparse_get_int("--log-level", &argc, argv, &log_level)) {
        usage();
        return 1;
    }

    if (ut_parse_args(&test_app.ut_app, &argc, argv))
        return 1;

    if (!no_trap) {
        init_signals();
    }

    retval = test_main(argc, argv);

    if (interractive) {
        char s[10];
        printf("<Press ENTER to quit>\n"); fflush(stdout);
        if (fgets(s, sizeof(s), stdin) == NULL)
            return retval;
    }

    return retval;
}
