/* Minimal consumer test for the installed pjproject CMake package.
 * Verifies that find_package(Pj) works and all transitive dependencies
 * (including Pj::Dep::* aliases) resolve correctly.
 */
#include <pjlib.h>

int main(void)
{
    pj_status_t status;
    status = pj_init();
    if (status != PJ_SUCCESS)
        return 1;
    pj_shutdown();
    return 0;
}
