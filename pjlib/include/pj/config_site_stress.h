/*
 * config_site override used by the pjsua_stress test app.
 * Wraps config_site_test.h and bumps a few compile-time limits so the test
 * can hold up to ~1024 simultaneous call legs.
 *
 * The CI workflow copies this file to config_site.h before ./configure.
 */

#include "config_site_test.h"

/* Up from default 4 — must be >= the largest -n we will pass on the CLI. */
#define PJSUA_MAX_CALLS         2048

/* Up from default 254. Conf bridge needs one port per call leg + headroom
 * for the master/null sound port and any extra ports. */
#define PJSUA_MAX_CONF_PORTS    4096

/* Up from default 64. Each call leg uses RTP+RTCP sockets, plus SIP listener.
 * Linux uses the epoll backend, so this is not bounded by FD_SETSIZE. */
#define PJ_IOQUEUE_MAX_HANDLES  4096

/* Make ioqueue unregistration safe when callbacks may still be in flight. */
#define PJ_IOQUEUE_HAS_SAFE_UNREG 1

/* Stress test exercises video calls; default is off. */
#define PJMEDIA_HAS_VIDEO       1

/* Default is 1, but be explicit — passthrough-codec configs can flip it
 * off and leave the SDP offer with no audio codec. */
#define PJMEDIA_HAS_G711_CODEC  1
