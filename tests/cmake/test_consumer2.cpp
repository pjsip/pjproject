/* Minimal PJSUA2 consumer test for the installed pjproject CMake package.
 * Verifies that C++ PJSUA2 API and transitive dependencies resolve correctly.
 */
#include <pjsua2.hpp>

int main()
{
    pj::Endpoint ep;
    ep.libCreate();
    ep.libDestroy();
    return 0;
}
