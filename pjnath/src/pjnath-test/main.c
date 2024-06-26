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
#include <pj/argparse.h>

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

#define boost()

static void usage()
{
    puts("Usage:");
    puts("  pjnath-test [OPTION] [test_to_run] [..]");
    puts("");
    puts("where OPTIONS:");
    puts("");
    puts("  -h, --help       Show this help screen");

    ut_usage();

    puts("  -i               Ask ENTER before quitting");
    puts("  -n               Do not trap signals");
}

int main(int argc, char *argv[])
{
    int rc;
    int interractive = 0;
    int no_trap = 0;

    boost();

    if (pj_argparse_get_bool("-h", &argc, argv) ||
        pj_argparse_get_bool("--help", &argc, argv))
    {
        usage();
        return 0;
    }

    ut_app_init0(&test_app.ut_app);

    interractive = pj_argparse_get_bool("-i", &argc, argv);
    no_trap = pj_argparse_get_bool("-n", &argc, argv);
    if (ut_parse_args(&test_app.ut_app, &argc, argv))
        return 1;

    if (!no_trap) {
        init_signals();
    }

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

