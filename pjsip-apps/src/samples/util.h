
/* Include all PJSIP core headers. */
#include <pjsip.h>

/* Include all PJMEDIA headers. */
#include <pjmedia.h>

/* Include all PJMEDIA-CODEC headers. */
#include <pjmedia-codec.h>

/* Include all PJSIP-UA headers */
#include <pjsip_ua.h>

/* Include all PJSIP-SIMPLE headers */
#include <pjsip_simple.h>

/* Include all PJLIB-UTIL headers. */
#include <pjlib-util.h>

/* Include all PJLIB headers. */
#include <pjlib.h>


/* Global endpoint instance. */
static pjsip_endpoint *g_endpt;

/* Global caching pool factory. */
static pj_caching_pool cp;

/* Global media endpoint. */
static pjmedia_endpt *g_med_endpt;

/* 
 * Show error.
 */
static int app_perror( const char *sender, const char *title, 
		       pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(1,(sender, "%s: %s [code=%d]", title, errmsg, status));
    return 1;
}

/*
 * Perform the very basic initialization:
 *  - init PJLIB.
 *  - init memory pool
 *  - create SIP endpoint instance.
 */
static pj_status_t util_init(void)
{
    pj_status_t status;

    /* Init PJLIB */
    status = pj_init();
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "pj_init() error", status);
	return status;
    }

    /* Init PJLIB-UTIL: */
    status = pjlib_util_init();
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "pjlib_util_init() error", status);
	return status;
    }

    /* Init memory pool: */

    /* Init caching pool. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    /* Create global endpoint: */

    {
	const pj_str_t *hostname;
	const char *endpt_name;

	/* Endpoint MUST be assigned a globally unique name.
	 * The name will be used as the hostname in Warning header.
	 */

	/* For this implementation, we'll use hostname for simplicity */
	hostname = pj_gethostname();
	endpt_name = hostname->ptr;

	/* Create the endpoint: */

	status = pjsip_endpt_create(&cp.factory, endpt_name, 
				    &g_endpt);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to create SIP endpoint", status);
	    return status;
	}
    }

    return PJ_SUCCESS;
}

/*
 * Add UDP transport to endpoint.
 */
static pj_status_t util_add_udp_transport(int port)
{
    pj_sockaddr_in addr;
    pj_status_t status;
    
    addr.sin_family = PJ_AF_INET;
    addr.sin_addr.s_addr = 0;
    addr.sin_port = port;

    status = pjsip_udp_transport_start( g_endpt, &addr, NULL, 1, NULL);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to start UDP transport", status);
	return status;
    }

    return status;
}

