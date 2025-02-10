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
    puts("********************************************************************");
    puts("**                        W A R N I N G                           **");
    puts("********************************************************************");
    puts("** Due to centralized event processing in PJSIP, events may be    **");
    puts("** read by different thread than the test's thread. This may      **");
    puts("** cause logs to be saved by the wrong test when multithreaded    **");
    puts("** testing is used. The test results are correct, but the log     **");
    puts("** may not be accurate.                                           **");
    puts("** For debugging with correct logging, use \"-w 0 --log-no-cache\"  **");
    puts("********************************************************************");
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

int main(int argc, char *argv[])
{
    int interractive = 0;
    int retval;

    warn();

    if (pj_argparse_get_bool(&argc, argv, "-h") ||
        pj_argparse_get_bool(&argc, argv, "--help"))
    {
        usage();
        return 0;
    }

    ut_app_init0(&test_app.ut_app);

    interractive = pj_argparse_get_bool(&argc, argv, "-i");
    if (pj_argparse_get_str(&argc, argv, "-s", (char**)&system_name) ||
        pj_argparse_get_str(&argc, argv, "--system", (char**)&system_name))
    {
        usage();
        return 1;
    }

    if (pj_argparse_get_int(&argc, argv, "--log-level", &log_level)) {
        usage();
        return 1;
    }

    if (ut_parse_args(&test_app.ut_app, &argc, argv))
        return 1;

    retval = test_main(argc, argv);

    if (interractive) {
        char s[10];
        printf("<Press ENTER to quit>\n"); fflush(stdout);
        if (fgets(s, sizeof(s), stdin) == NULL)
            return retval;
    }

    return retval;
}
