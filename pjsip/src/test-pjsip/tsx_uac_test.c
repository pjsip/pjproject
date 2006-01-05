/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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

#include "test.h"
#include <pjsip_core.h>
#include <pjlib.h>

/*****************************************************************************
 **
 ** UAC basic retransmission and timeout test.
 **
 ** This will test the retransmission of the UAC transaction. Remote will not
 ** answer the transaction, so the transaction should fail.
 **
 *****************************************************************************
 */

static char *CALL_ID1 = "UAC-Tsx-Basic-Test1";
static void tsx_user_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e);
static pj_bool_t msg_receiver_on_rx_request(pjsip_rx_data *rdata);

/* UAC transaction user module. */
static pjsip_module tsx_user = 
{
    NULL, NULL,				/* prev and next	*/
    { "Tsx-User", 8},			/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION-1,	/* Priority		*/
    NULL,				/* User data.		*/
    0,					/* Number of methods supported (=0). */
    { 0 },				/* Array of methods (none) */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    NULL,				/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    &tsx_user_on_tsx_state,		/* on_tsx_state()	*/
};

/* Module to receive the loop-backed request. */
static pjsip_module msg_receiver = 
{
    NULL, NULL,				/* prev and next	*/
    { "Test", 4},			/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION-1,	/* Priority		*/
    NULL,				/* User data.		*/
    0,					/* Number of methods supported (=0). */
    { 0 },				/* Array of methods (none) */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &msg_receiver_on_rx_request,	/* on_rx_request()	*/
    NULL,				/* on_rx_response()	*/
    NULL,				/* on_tsx_state()	*/
};

/* Static vars. */
static int recv_count;
static pj_time_val recv_last;
static pj_bool_t test_complete;

static void tsx_user_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e)
{
    if (tsx->state == PJSIP_TSX_STATE_TERMINATED && test_complete==0)
	test_complete = 1;
}

#define DIFF(a,b)   ((a<b) ? (b-a) : (a-b))

static pj_bool_t msg_receiver_on_rx_request(pjsip_rx_data *rdata)
{
    if (pj_strcmp2(&rdata->msg_info.call_id, CALL_ID1) == 0) {
	/*
	 * The CALL_ID1 test performs the verifications for transaction
	 * retransmission mechanism. It will not answer the incoming request
	 * with any response.
	 */
	pjsip_msg *msg = rdata->msg_info.msg;

	PJ_LOG(4,("", "   received request"));

	/* Only wants to take INVITE or OPTIONS method. */
	if (msg->line.req.method.id != PJSIP_INVITE_METHOD &&
	    msg->line.req.method.id != PJSIP_OPTIONS_METHOD)
	{
	    PJ_LOG(3,("", "   error: received unexpected method %.*s",
			  msg->line.req.method.name.slen,
			  msg->line.req.method.name.ptr));
	    test_complete = -600;
	    return PJ_TRUE;
	}

	if (recv_count == 0) {
	    recv_count++;
	    pj_gettimeofday(&recv_last);
	} else {
	    pj_time_val now;
	    unsigned msec_expected, msec_elapsed;

	    pj_gettimeofday(&now);
	    PJ_TIME_VAL_SUB(now, recv_last);
	    msec_elapsed = now.sec*1000 + now.msec;

	    ++recv_count;
    	    msec_expected = (1<<(recv_count-2))*PJSIP_T1_TIMEOUT;

	    if (msg->line.req.method.id != PJSIP_INVITE_METHOD) {
		if (msec_expected > PJSIP_T2_TIMEOUT)
		    msec_expected = PJSIP_T2_TIMEOUT;
	    }

	    if (DIFF(msec_expected, msec_elapsed) > 100) {
		PJ_LOG(3,("","   error: expecting %d-th retransmission in %d "
			     "ms, received in %d ms",
			     recv_count-1, msec_expected, msec_elapsed));
		test_complete = -610;
	    }

	    if (recv_count > 7) {
		PJ_LOG(3,("", "   error: too many messages (%d) received",
			      recv_count));
		test_complete = -620;
	    }

	    pj_gettimeofday(&recv_last);
	}
	return PJ_TRUE;
    }
    return PJ_FALSE;
}

/*****************************************************************************
 **
 ** UAC basic retransmission and timeout test.
 **
 ** This will test the retransmission of the UAC transaction. Remote will not
 ** answer the transaction, so the transaction should fail. The Call-ID
 ** CALL_ID1 will be used for this test.
 **
 *****************************************************************************
 */
static int tsx_uac_retransmit_test(const pjsip_method *method)
{
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;
    char buf[80];
    pj_str_t target, from, call_id, tsx_key;
    pj_time_val timeout;
    pj_status_t status;

    PJ_LOG(3,("", "  basic uac retransmission and timeout test"));

    pj_sprintf(buf, "sip:alice@127.0.0.1:%d", TEST_UDP_PORT);
    target = pj_str(buf);
    from = pj_str("sip:bob@127.0.0.1");
    call_id = pj_str(CALL_ID1);

    /* Create request. */
    status = pjsip_endpt_create_request( endpt, method, &target,
					 &from, &target, NULL, &call_id, -1, 
					 NULL, &tdata);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to create request", status);
	return -500;
    }

    /* Add additional reference to tdata to prevent transaction from
     * deleting it.
     */
    pjsip_tx_data_add_ref(tdata);

    /* Create transaction. */
    status = pjsip_tsx_create_uac( &tsx_user, tdata, &tsx);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to create UAC transaction", status);
	return -510;
    }

    /* Get transaction key. */
    pj_strdup(tdata->pool, &tsx_key, &tsx->transaction_key);

    /* Send the message. */
    status = pjsip_tsx_send_msg(tsx, NULL);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to send request", status);
	return -520;
    }

    /* Set test completion time. */
    pj_gettimeofday(&timeout);
    timeout.sec += 33;

    /* Wait until test complete. */
    while (!test_complete) {
	pj_time_val now;

	pjsip_endpt_handle_events(endpt, NULL);

	pj_gettimeofday(&now);
	if (now.sec > timeout.sec) {
	    PJ_LOG(3,("", "   Error: test has timed out"));
	    return -530;
	}
    }

    if (status < 0)
	return status;

    /* Make sure transaction has been destroyed. */
    if (pjsip_tsx_layer_find_tsx(&tsx_key, PJ_FALSE) != NULL) {
	PJ_LOG(3,("", "   Error: transaction has not been destroyed"));
	return -540;
    }

    /* Check tdata reference counter. */
    if (pj_atomic_get(tdata->ref_cnt) != 1) {
	PJ_LOG(3,("", "   Error: tdata reference counter is %d",
		      pj_atomic_get(tdata->ref_cnt)));
	return -550;
    }

    /* Destroy txdata */
    pjsip_tx_data_dec_ref(tdata);

    return PJ_SUCCESS;
}

/*****************************************************************************
 **
 ** UAC Transaction Test.
 **
 *****************************************************************************
 */
int tsx_uac_test(void)
{
    pj_sockaddr_in addr;
    pj_str_t tmp;
    pjsip_transport *tp;
    pj_status_t status;

    pj_sockaddr_in_init(&addr, pj_cstr(&tmp, "127.0.0.1"), TEST_UDP_PORT);

    /* Start UDP transport if necessary. */
    if (pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_UDP, &addr,
				      sizeof(addr), &tp) != PJ_SUCCESS)
    {
	addr.sin_addr.s_addr = 0;
	status = pjsip_udp_transport_start( endpt, &addr, NULL, 1, NULL);
	if (status != PJ_SUCCESS) {
	    app_perror("   Error: unable to start UDP transport", status);
	    return -10;
	}
    } else {
	pjsip_transport_dec_ref(tp);
    }

    /* Start transaction layer module. */
    status = pjsip_tsx_layer_init(endpt);
    if (status != PJ_SUCCESS) {
	app_perror("   Error initializing transaction module", status);
	return -20;
    }

    /* Register modules. */
    status = pjsip_endpt_register_module(endpt, &tsx_user);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to register module", status);
	return -30;
    }
    status = pjsip_endpt_register_module(endpt, &msg_receiver);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to register module", status);
	return -30;
    }

    /* Basic retransmit and timeout test for INVITE. */
    status = tsx_uac_retransmit_test(&pjsip_invite_method);
    if (status != 0)
	return status;

    /* Basic retransmit and timeout test for non-INVITE. */
    status = tsx_uac_retransmit_test(&pjsip_options_method);
    if (status != 0)
	return status;

    return 0;
}
