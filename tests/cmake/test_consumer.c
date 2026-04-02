/* Minimal consumer test for the installed pjproject CMake package.
 * Verifies that find_package(Pj) works and all transitive dependencies
 * (including Pj::Dep::* aliases) resolve correctly.
 */
#include <pjsua-lib/pjsua.h>

int main(void)
{
    pj_status_t status;
    status = pjsua_create();
    if (status != PJ_SUCCESS)
        return 1;
    pjsua_destroy();
    return 0;
}
