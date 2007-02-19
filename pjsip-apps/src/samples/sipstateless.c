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

/**
 * sipcore.c
 *
 * A simple program to respond any incoming requests (except ACK, of course!)
 * with any status code (taken from command line argument, with the default
 * is 501/Not Implemented).
 */


/* Include all headers. */
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>


/* If this macro is set, UDP transport will be initialized at port 5060 */
#define HAS_UDP_TRANSPORT

/* If this macro is set, TCP transport will be initialized at port 5060 */
#define HAS_TCP_TRANSPORT

/* Log identification */
#define THIS_FILE	"sipstateless.c"


/* Global SIP endpoint */
static pjsip_endpoint *sip_endpt;

/* What response code to be sent (default is 501/Not Implemented) */
static int code = PJSIP_SC_NOT_IMPLEMENTED;


/* Callback to handle incoming requests. */
static pj_bool_t on_rx_request( pjsip_rx_data *rdata )
{
    /* Respond (statelessly) all incoming requests (except ACK!) 
     * with 501 (Not Implemented)
     */
    if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
	pjsip_endpt_respond_stateless( sip_endpt, rdata, 
				       code, NULL,
				       NULL, NULL);
    }
    return PJ_TRUE;
}



/*
 * main()
 *
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjsip_module mod_app =
    {
	NULL, NULL,		    /* prev, next.		*/
	{ "mod-app", 7 },	    /* Name.			*/
	-1,				    /* Id		*/
	PJSIP_MOD_PRIORITY_APPLICATION, /* Priority		*/
	NULL,			    /* load()			*/
	NULL,			    /* start()			*/
	NULL,			    /* stop()			*/
	NULL,			    /* unload()			*/
	&on_rx_request,		    /* on_rx_request()		*/
	NULL,			    /* on_rx_response()		*/
	NULL,			    /* on_tx_request.		*/
	NULL,			    /* on_tx_response()		*/
	NULL,			    /* on_tsx_state()		*/
    };


    pj_status_t status;
    
    if (argc == 2)
	code = atoi(argv[1]);

    /* Must init PJLIB first: */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Then init PJLIB-UTIL: */
    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);


    /* Create global endpoint: */
    {
	/* Endpoint MUST be assigned a globally unique name.
	 * Ideally we should put hostname or public IP address, but
	 * we'll just use an arbitrary name here.
	 */

	/* Create the endpoint: */
	status = pjsip_endpt_create(&cp.factory, "sipstateless", 
				    &sip_endpt);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* 
     * Add UDP transport, with hard-coded port 
     */
#ifdef HAS_UDP_TRANSPORT
    {
	pj_sockaddr_in addr;

	addr.sin_family = PJ_AF_INET;
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons(5060);

	status = pjsip_udp_transport_start( sip_endpt, &addr, NULL, 1, NULL);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(3,(THIS_FILE, 
		      "Error starting UDP transport (port in use?)"));
	    return 1;
	}
    }
#endif

#ifdef HAS_TCP_TRANSPORT
    /* 
     * Add UDP transport, with hard-coded port 
     */
    {
	pj_sockaddr_in addr;

	addr.sin_family = PJ_AF_INET;
	addr.sin_addr.s_addr = 0;
	addr.sin_port = pj_htons(5060);

	status = pjsip_tcp_transport_start(sip_endpt, &addr, 1, NULL);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(3,(THIS_FILE, 
		      "Error starting TCP transport (port in use?)"));
	    return 1;
	}
    }
#endif

    /*
     * Register our module to receive incoming requests.
     */
    status = pjsip_endpt_register_module( sip_endpt, &mod_app);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Done. Loop forever to handle incoming events. */
    PJ_LOG(3,(THIS_FILE, "Press Ctrl-C to quit.."));

    for (;;) {
	pjsip_endpt_handle_events(sip_endpt, NULL);
    }
}
