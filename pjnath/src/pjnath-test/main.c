/* $Id$ */
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

#if defined(PJ_SUNOS) && PJ_SUNOS!=0

#include <signal.h>
static void init_signals()
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;

    sigaction(SIGALRM, &act, NULL);
}

#elif PJ_LINUX || PJ_DARWINOS

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

static void init_signals()
{
    signal(SIGSEGV, &print_stack);
}

#else
#define init_signals()
#endif

#define boost()

int main(int argc, char *argv[])
{
    int rc;

    PJ_UNUSED_ARG(argc);
    PJ_UNUSED_ARG(argv);

    boost();
    init_signals();

    rc = test_main();

    if (argc == 2 && pj_ansi_strcmp(argv[1], "-i")==0) {
	char buf[10];

	puts("Press <ENTER> to exit");
	if (fgets(buf, sizeof(buf), stdin) == NULL)
	    return rc;
    }

    return rc;
}

