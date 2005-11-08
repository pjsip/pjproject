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

    return rc;
}

