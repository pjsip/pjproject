/* $Id$
 *

 */
/* 
 * $Log: /pjproject-0.3/pjlib/src/pjlib-test/main.c $
 * 
 * 4     29/10/05 21:32 Bennylp
 * Boost process priority in Win32
 * 
 * 3     10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 2     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 *
 */
#include "test.h"

#include <pj/string.h>
#include <pj/sock.h>
#include <pj/log.h>

extern int param_echo_sock_type;
extern const char *param_echo_server;
extern int param_echo_port;


#if defined(PJ_WIN32) && PJ_WIN32!=0
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

#else
#define init_signals()
#endif

int main(int argc, char *argv[])
{
    int rc;

    boost();
    init_signals();

    while (argc > 1) {
        char *arg = argv[--argc];

        if (*arg=='-' && *(arg+1)=='p') {
            pj_str_t port = pj_str(argv[--argc]);

            param_echo_port = pj_strtoul(&port);

        } else if (*arg=='-' && *(arg+1)=='s') {
            param_echo_server = argv[--argc];

        } else if (*arg=='-' && *(arg+1)=='t') {
            pj_str_t type = pj_str(argv[--argc]);
            
            if (pj_stricmp2(&type, "tcp")==0)
                param_echo_sock_type = PJ_SOCK_STREAM;
            else if (pj_stricmp2(&type, "udp")==0)
                param_echo_sock_type = PJ_SOCK_DGRAM;
            else {
                PJ_LOG(3,("", "error: unknown socket type %s", type.ptr));
                return 1;
            }
        }
    }

    rc = test_main();

    return rc;
}

