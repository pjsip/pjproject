/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>


/* Options */
static struct global_struct
{
    pj_caching_pool	 cp;
    pjsip_endpoint	*endpt;
    int			 port;
    pj_pool_t		*pool;

    pj_thread_t		*thread;
    pj_bool_t		 quit_flag;

    pj_bool_t		 record_route;

    unsigned		 name_cnt;
    pjsip_host_port	 name[16];
} global;



static void app_perror(const char *msg, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(THIS_FILE, "%s: %s", msg, errmsg));
}


static void usage(void)
{
    puts("Options:\n"
	 "\n"
	 " -p, --port N    Set local listener port to N\n"
	 " -R, --rr        Perform record routing\n"
	 " -h, --help      Show this help screen\n"
	 );
}


static pj_status_t init_options(int argc, char *argv[])
{
    struct pj_getopt_option long_opt[] = {
	{ "port",1, 0, 'p'},
	{ "rr",  1, 0, 'R'},
	{ "help",1, 0, 'h'},
    };
    int c;
    int opt_ind;

    pj_optind = 0;
    while((c=pj_getopt_long(argc, argv, "pRh", long_opt, &opt_ind))!=-1) {
	switch (c) {
	case 'p':
	    global.port = atoi(pj_optarg);
	    printf("Port is set to %d\n", global.pool);
	    break;
	
	case 'R':
	    global.record_route = PJ_TRUE;
	    printf("Using record route mode\n");
	    break;

	case 'h':
	    usage();
	    return -1;

	default:
	    puts("Unknown option ignored");
	    break;
	}
    }

    return PJ_SUCCESS;
}


static pj_status_t init_stack(void)
{
    pj_status_t status;

    /* Must init PJLIB first: */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Then init PJLIB-UTIL: */
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);


    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&global.cp, &pj_pool_factory_default_policy, 0);

    /* Create the endpoint: */
    status = pjsip_endpt_create(&global.cp.factory, NULL, &global.endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);

    /* Init transaction layer for stateful proxy only */
#if STATEFUL
    status = pjsip_tsx_layer_init_module(global.endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, status);
#endif

    /* Create listening transport */
    {
	pj_sockaddr_in addr;

	addr.sin_family = PJ_AF_INET;
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons((pj_uint16_t)global.port);

	status = pjsip_udp_transport_start( global.endpt, &addr, 
					    NULL, 1, NULL);
	if (status != PJ_SUCCESS)
	    return status;
    }

    /* Create pool for the application */
    global.pool = pj_pool_create(&global.cp.factory, "proxyapp", 
				 4000, 4000, NULL);

    return PJ_SUCCESS;
}


static pj_status_t init_proxy(void)
{
    pj_in_addr addr;
    unsigned i;

    /* List all names matching local endpoint.
     * Note that PJLIB version 0.6 and newer has a function to
     * enumerate local IP interface (pj_enum_ip_interface()), so
     * by using it would be possible to list all IP interfaces in
     * this host.
     */

    /* The first address is important since this would be the one
     * to be added in Record-Route.
     */
    if (pj_gethostip(&addr) == PJ_SUCCESS) {
	pj_strdup2(global.pool, &global.name[global.name_cnt].host,
		   pj_inet_ntoa(addr));
	global.name[global.name_cnt].port = global.port;
	global.name_cnt++;
    }

    global.name[global.name_cnt].host = pj_str("127.0.0.1");
    global.name[global.name_cnt].port = global.port;
    global.name_cnt++;

    global.name[global.name_cnt].host = *pj_gethostname();
    global.name[global.name_cnt].port = global.port;
    global.name_cnt++;

    global.name[global.name_cnt].host = pj_str("localhost");
    global.name[global.name_cnt].port = global.port;
    global.name_cnt++;

    PJ_LOG(3,(THIS_FILE, "Proxy started, listening on port %d", global.port));
    PJ_LOG(3,(THIS_FILE, "Local host aliases:"));
    for (i=0; i<global.name_cnt; ++i) {
	PJ_LOG(3,(THIS_FILE, " %.*s:%d", 
		  (int)global.name[i].host.slen,
		  global.name[i].host.ptr,
		  global.name[i].port));
    }

    if (global.record_route) {
	PJ_LOG(3,(THIS_FILE, "Using Record-Route mode"));
    }

    return PJ_SUCCESS;
}


#if PJ_HAS_THREADS
static int worker_thread(void *p)
{
    pj_time_val delay = {0, 10};

    PJ_UNUSED_ARG(p);

    while (!global.quit_flag) {
	pjsip_endpt_handle_events(global.endpt, &delay);
    }

    return 0;
}
#endif


/* Utility to determine if URI is local to this host. */
static pj_bool_t is_uri_local(const pjsip_sip_uri *uri)
{
    unsigned i;
    for (i=0; i<global.name_cnt; ++i) {
	if ((uri->port == global.name[i].port ||
	     (uri->port==0 && global.name[i].port==5060)) &&
	    pj_stricmp(&uri->host, &global.name[i].host)==0)
	{
	    /* Match */
	    return PJ_TRUE;
	}
    }

    /* Doesn't match */
    return PJ_FALSE;
}


/* Proxy utility to verify incoming requests.
 * Return non-zero if verification failed.
 */
static pj_status_t proxy_verify_request(pjsip_rx_data *rdata)
{
    const pj_str_t STR_PROXY_REQUIRE = {"Proxy-Require", 13};

    /* RFC 3261 Section 16.3 Request Validation */

    /* Before an element can proxy a request, it MUST verify the message's
     * validity.  A valid message must pass the following checks:
     * 
     * 1. Reasonable Syntax
     * 2. URI scheme
     * 3. Max-Forwards
     * 4. (Optional) Loop Detection
     * 5. Proxy-Require
     * 6. Proxy-Authorization
     */

    /* 1. Reasonable Syntax.
     * This would have been checked by transport layer.
     */

    /* 2. URI scheme.
     * We only want to support "sip:" URI scheme for this simple proxy.
     */
    if (!PJSIP_URI_SCHEME_IS_SIP(rdata->msg_info.msg->line.req.uri)) {
	pjsip_endpt_respond_stateless(global.endpt, rdata, 
				      PJSIP_SC_UNSUPPORTED_URI_SCHEME, NULL,
				      NULL, NULL);
	return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_UNSUPPORTED_URI_SCHEME);
    }

    /* 3. Max-Forwards.
     * Send error if Max-Forwards is 1 or lower.
     */
    if (rdata->msg_info.max_fwd && rdata->msg_info.max_fwd->ivalue <= 1) {
	pjsip_endpt_respond_stateless(global.endpt, rdata, 
				      PJSIP_SC_TOO_MANY_HOPS, NULL,
				      NULL, NULL);
	return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_TOO_MANY_HOPS);
    }

    /* 4. (Optional) Loop Detection.
     * Nah, we don't do that with this simple proxy.
     */

    /* 5. Proxy-Require */
    if (pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &STR_PROXY_REQUIRE, 
				   NULL) != NULL) 
    {
	pjsip_endpt_respond_stateless(global.endpt, rdata, 
				      PJSIP_SC_BAD_EXTENSION, NULL,
				      NULL, NULL);
	return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_EXTENSION);
    }

    /* 6. Proxy-Authorization.
     * Nah, we don't require any authorization with this sample.
     */

    return PJ_SUCCESS;
}


/* Process route information in the reqeust */
static pj_status_t proxy_process_routing(pjsip_tx_data *tdata)
{
    pjsip_sip_uri *target;
    pjsip_route_hdr *hroute;

    /* RFC 3261 Section 16.4 Route Information Preprocessing */

    target = (pjsip_sip_uri*) tdata->msg->line.req.uri;

    /* The proxy MUST inspect the Request-URI of the request.  If the
     * Request-URI of the request contains a value this proxy previously
     * placed into a Record-Route header field (see Section 16.6 item 4),
     * the proxy MUST replace the Request-URI in the request with the last
     * value from the Route header field, and remove that value from the
     * Route header field.  The proxy MUST then proceed as if it received
     * this modified request.
     */
    /* Nah, we don't want to support this */

    /* If the Request-URI contains a maddr parameter, the proxy MUST check
     * to see if its value is in the set of addresses or domains the proxy
     * is configured to be responsible for.  If the Request-URI has a maddr
     * parameter with a value the proxy is responsible for, and the request
     * was received using the port and transport indicated (explicitly or by
     * default) in the Request-URI, the proxy MUST strip the maddr and any
     * non-default port or transport parameter and continue processing as if
     * those values had not been present in the request.
     */
    if (target->maddr_param.slen != 0) {
	pjsip_sip_uri maddr_uri;

	maddr_uri.host = target->maddr_param;
	maddr_uri.port = global.port;

	if (is_uri_local(&maddr_uri)) {
	    target->maddr_param.slen = 0;
	    target->port = 0;
	    target->transport_param.slen = 0;
	}
    }

    /* If the first value in the Route header field indicates this proxy,
     * the proxy MUST remove that value from the request.
     */
    hroute = (pjsip_route_hdr*) 
	      pjsip_msg_find_hdr(tdata->msg, PJSIP_H_ROUTE, NULL);
    if (hroute && is_uri_local((pjsip_sip_uri*)hroute->name_addr.uri)) {
	pj_list_erase(hroute);
    }

    return PJ_SUCCESS;
}


/* Postprocess the request before forwarding it */
static void proxy_postprocess(pjsip_tx_data *tdata)
{
    /* Optionally record-route */
    if (global.record_route) {
	char uribuf[128];
	pj_str_t uri;
	const pj_str_t H_RR = { "Record-Route", 12 };
	pjsip_generic_string_hdr *rr;

	pj_ansi_snprintf(uribuf, sizeof(uribuf), "<sip:%.*s:%d;lr>",
			 (int)global.name[0].host.slen,
			 global.name[0].host.ptr,
			 global.name[0].port);
	uri = pj_str(uribuf);
	rr = pjsip_generic_string_hdr_create(tdata->pool,
					     &H_RR, &uri);
	pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)rr);
    }
}


/* Calculate new target for the request */
static pj_status_t proxy_calculate_target(pjsip_rx_data *rdata,
					  pjsip_tx_data *tdata)
{
    pjsip_sip_uri *target;

    /* RFC 3261 Section 16.5 Determining Request Targets */

    target = (pjsip_sip_uri*) tdata->msg->line.req.uri;

    /* If the Request-URI of the request contains an maddr parameter, the
     * Request-URI MUST be placed into the target set as the only target
     * URI, and the proxy MUST proceed to Section 16.6.
     */
    if (target->maddr_param.slen) {
	proxy_postprocess(tdata);
	return PJ_SUCCESS;
    }


    /* If the domain of the Request-URI indicates a domain this element is
     * not responsible for, the Request-URI MUST be placed into the target
     * set as the only target, and the element MUST proceed to the task of
     * Request Forwarding (Section 16.6).
     */
    if (!is_uri_local(target)) {
	proxy_postprocess(tdata);
	return PJ_SUCCESS;
    }

    /* If the target set for the request has not been predetermined as
     * described above, this implies that the element is responsible for the
     * domain in the Request-URI, and the element MAY use whatever mechanism
     * it desires to determine where to send the request. 
     */

    /* We're not interested to receive request destined to us, so
     * respond with 404/Not Found.
     */
    pjsip_endpt_respond_stateless(global.endpt, rdata,
				  PJSIP_SC_NOT_FOUND, NULL,
				  NULL, NULL);

    /* Delete the request since we're not forwarding it */
    pjsip_tx_data_dec_ref(tdata);

    return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_NOT_FOUND);
}


/* Destroy stack */
static void destroy_stack(void)
{
    pjsip_endpt_destroy(global.endpt);
    pj_pool_release(global.pool);
    pj_caching_pool_destroy(&global.cp);

    pj_shutdown();
}

