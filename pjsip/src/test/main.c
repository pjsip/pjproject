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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern const char *system_name;

static void usage()
{
    puts("Usage: test-pjsip");
    puts("Options:");
    puts(" -i,--interractive   Key input at the end.");
    puts(" -h,--help           Show this screen");
    puts(" -l,--log-level N    Set log level (0-6)");
}

#if PJ_LINUX || PJ_DARWINOS

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

int main(int argc, char *argv[])
{
    int interractive = 0;
    int retval;
    char **opt_arg;

    PJ_UNUSED_ARG(argc);

    init_signals();

    /* Parse arguments. */
    opt_arg = argv+1;
    while (*opt_arg) {
	if (strcmp(*opt_arg, "-i") == 0 ||
	    strcmp(*opt_arg, "--interractive") == 0)
	{
	    interractive = 1;
	} else if (strcmp(*opt_arg, "-h") == 0 ||
		   strcmp(*opt_arg, "--help") == 0) 
	{
	    usage();
	    return 1;
	} else if (strcmp(*opt_arg, "-l") == 0 ||
		   strcmp(*opt_arg, "--log-level") == 0) 
	{
	    ++opt_arg;
	    if (!opt_arg) {
		usage();
		return 1;
	    }
	    log_level = atoi(*opt_arg);
	} else 	if (strcmp(*opt_arg, "-s") == 0 ||
	    strcmp(*opt_arg, "--system") == 0)
	{
	    ++opt_arg;
	    if (!opt_arg) {
		usage();
		return 1;
	    }
	    system_name = *opt_arg;
	} else {
	    usage();
	    return 1;
	}

	++opt_arg;
    }

    retval = test_main();

    if (interractive) {
	char s[10];
	printf("<Press ENTER to quit>\n"); fflush(stdout);
	if (fgets(s, sizeof(s), stdin) == NULL)
	    return retval;
    }

    return retval;
}
