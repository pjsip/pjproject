/* $Header: /pjproject/pjsip/src/tests/pjsip_core/main.c 2     2/24/05 10:46a Bennylp $ */
#include "test.h"
#include <stdio.h>

int main()
{
    test_uri();
    test_msg();

#if !IS_PROFILING
    puts("Press <ENTER> to quit.");
    fgets( s, sizeof(s), stdin);
#endif
    return 0;
}
