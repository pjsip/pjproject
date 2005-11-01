/* $Id$
 *
 */
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
