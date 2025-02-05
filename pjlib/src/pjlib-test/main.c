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


static void usage()
{
    puts("Usage:");
    puts("  pjlib-test [OPTION] [test_to_run] [..]");
    puts("");
    puts("where OPTIONS:");
    puts("");
    puts("  -h, --help       Show this help screen");
    
    ut_usage();

    puts("  --skip-e         Skip essential tests");
    puts("  --ci-mode        Running in slow CI  mode");
    puts("  -i               Ask ENTER before quitting");
    puts("  -p PORT          Use port PORT for echo port");
    puts("  -s SERVER        Use SERVER as ech oserver");
    puts("  -t ucp,tcp       Set echo socket type to UDP or TCP");
}


int main(int argc, char *argv[])
{
    int rc;
    int interractive = 0;

    boost();
    ut_app_init0(&test_app.ut_app);

    /* 
     * Parse arguments
     */
    if (pj_argparse_get_bool(&argc, argv, "-h") ||
        pj_argparse_get_bool(&argc, argv, "--help"))
    {
        usage();
        return 0;
    }
    interractive = pj_argparse_get_bool(&argc, argv, "-i");
    if (pj_argparse_get_int(&argc, argv, "-p", &test_app.param_echo_port)) {
        usage();
        return 1;
    }
    if (pj_argparse_get_str(&argc, argv, "-s",
                            (char**)&test_app.param_echo_server))
    {
        usage();
        return 1;
    }

    if (pj_argparse_exists(argv, "-t")) {
        char *sock_type;
        if (pj_argparse_get_str(&argc, argv, "-t", &sock_type)==PJ_SUCCESS) {
            if (pj_ansi_stricmp(sock_type, "tcp")==0)
                test_app.param_echo_sock_type = pj_SOCK_STREAM();
            else if (pj_ansi_stricmp(sock_type, "udp")==0)
                test_app.param_echo_sock_type = pj_SOCK_DGRAM();
            else {
                printf("Error: unknown socket type %s for -t option\n",
                       sock_type);
                usage();
                return 1;
            }
        } else {
            usage();
            return 1;
        }
    }

    if (ut_parse_args(&test_app.ut_app, &argc, argv)) {
        usage();
        return 1;
    }
    test_app.param_skip_essentials = pj_argparse_get_bool(&argc, argv,
                                                          "--skip-e");
    test_app.param_ci_mode = pj_argparse_get_bool(&argc, argv, "--ci-mode");


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

