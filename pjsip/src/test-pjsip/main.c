/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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

static void usage()
{
    puts("Usage: test-pjsip");
    puts("Options:");
    puts(" -i,--interractive   Key input at the end.");
    puts(" -h,--help           Show this screen");
    puts(" -l,--log-level N    Set log level (0-6)");
}

int main(int argc, char *argv[])
{
    int interractive = 0;
    int retval;
    char **opt_arg;

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
	} else {
	    usage();
	    return 1;
	}

	++opt_arg;
    }

    retval = test_main();

    if (argc != 1) {
	char s[10];
	printf("<Press ENTER to quit>\n");
	fgets(s, sizeof(s), stdin);
    }

    return retval;
}
