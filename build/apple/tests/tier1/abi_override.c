/*
 * Tier 1: a consumer must not be able to change the ABI from its own command
 * line. These macros decide the size of public structures; the shipped
 * config_site.h pins them to the values the binary was built with, so an
 * overriding -D is overridden in turn rather than silently obeyed.
 *
 * Compiled with -DPJSIP_MAX_MODULE=9999 and friends. If the freeze is working
 * the values below are the build's, not 9999.
 */
#include <pjsip/sip_config.h>
#include <pjmedia/config.h>
#include <pj/config.h>

#if PJSIP_MAX_MODULE == 9999
#  error "a consumer -D changed PJSIP_MAX_MODULE: public structure layouts differ"
#endif
#if PJSIP_MAX_URL_SIZE == 9999
#  error "a consumer -D changed PJSIP_MAX_URL_SIZE"
#endif
#if PJMEDIA_MAX_SDP_FMT == 9999
#  error "a consumer -D changed PJMEDIA_MAX_SDP_FMT"
#endif
#if PJ_MAX_OBJ_NAME == 9999
#  error "a consumer -D changed PJ_MAX_OBJ_NAME"
#endif

int main(void) { return 0; }
