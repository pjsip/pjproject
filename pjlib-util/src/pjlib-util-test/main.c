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
#include <pj/argparse.h>

#define boost()

static void usage()
{
    puts("Usage:");
    puts("  pjlib-util-test [OPTION] [test_to_run] [..]");
    puts("");
    puts("where OPTIONS:");
    puts("");
    puts("  -h, --help       Show this help screen");

    ut_usage();

    puts("  -i               Ask ENTER before quitting");
}


int main(int argc, char *argv[])
{
    int rc;
    int interractive = 0;

    boost();

    if (pj_argparse_get_bool(&argc, argv, "-h") ||
        pj_argparse_get_bool(&argc, argv, "--help"))
    {
        usage();
        return 0;
    }

    ut_app_init0(&test_app.ut_app);

    interractive = pj_argparse_get_bool(&argc, argv, "-i");
    if (ut_parse_args(&test_app.ut_app, &argc, argv))
        return 1;
        
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
