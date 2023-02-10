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
#include <stdio.h>

extern int param_echo_sock_type;
extern const char *param_echo_server;
extern int param_echo_port;
extern pj_bool_t param_ci_mode;


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

int main(int argc, char *argv[])
{
    int iarg=1, rc;
    int interractive = 0;
    int no_trap = 0;

    boost();

    while (iarg < argc) {
        char *arg = argv[iarg++];

        if (*arg=='-' && *(arg+1)=='i') {
            interractive = 1;

        } else if (*arg=='-' && *(arg+1)=='n') {
            no_trap = 1;
        } else if (*arg=='-' && *(arg+1)=='p') {
            pj_str_t port = pj_str(argv[iarg++]);

            param_echo_port = pj_strtoul(&port);

        } else if (*arg=='-' && *(arg+1)=='s') {
            param_echo_server = argv[iarg++];

        } else if (*arg=='-' && *(arg+1)=='t') {
            pj_str_t type = pj_str(argv[iarg++]);
            
            if (pj_stricmp2(&type, "tcp")==0)
                param_echo_sock_type = pj_SOCK_STREAM();
            else if (pj_stricmp2(&type, "udp")==0)
                param_echo_sock_type = pj_SOCK_DGRAM();
            else {
                printf("Error: unknown socket type %s\n", type.ptr);
                return 1;
            }
        } else if (strcmp(arg, "--ci-mode")==0) {
            param_ci_mode = PJ_TRUE;

        } else {
            printf("Error in argument \"%s\"\n", arg);
            return 1;
        }
    }

    if (!no_trap) {
        init_signals();
    }

    rc = test_main();

    if (interractive) {
        char s[10];
        puts("");
        puts("Press <ENTER> to exit");
        if (!fgets(s, sizeof(s), stdin))
            return rc;
    }

    return rc;
}

