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

#define THIS_FILE   "tsx_uas_test.c"


/*****************************************************************************
 **
 ** UAS tests.
 **
 ** This file performs various tests for UAC transactions. Each test will have
 ** a different Via branch param so that message receiver module and 
 ** transaction user module can identify which test is being carried out.
 **
 ** TEST1_BRANCH_ID
 **	Test that non-INVITE transaction returns 2xx response to the correct
 **	transport and correctly terminates the transaction.
 **
 ** TEST2_BRANCH_ID
 **	As above, for non-2xx final response.
 **
 ** TEST3_BRANCH_ID
 **	Transaction correctly progressing to PROCEEDING state when provisional
 **	response is sent.
 **
 ** TEST4_BRANCH_ID
 **	Transaction retransmits last response (if any) without notifying 
 **	transaction user upon receiving request  retransmissions on:
 **	    a. TRYING state.
 **	    a. PROCEEDING state.
 **	    b. COMPLETED state.
 **
 ** TEST5_BRANCH_ID
 **	INVITE transaction MUST retransmit final response. (Note: PJSIP also
 **	retransmit 2xx final response until it's terminated by user).
 **
 ** TEST6_BRANCH_ID
 **	INVITE transaction MUST cease retransmission of final response when
 *	ACK is received. (Note: PJSIP also retransmit 2xx final response 
 *	until it's terminated by user).
 **
 ** TEST7_BRANCH_ID
 **	Test where INVITE UAS transaction never receives ACK
 **
 ** TEST8_BRANCH_ID
 **	When UAS failed to deliver the response with the selected transport,
 **	it should try contacting the client with other transport or begin
 **	RFC 3263 server resolution procedure.
 **	This should be tested on:
 **	    a. TRYING state (when delivering first response).
 **	    b. PROCEEDING state (when failed to retransmit last response
 **	       upon receiving request retransmission).
 **	    c. COMPLETED state.
 **
 ** TEST9_BRANCH_ID
 **	Variant of previous test, where transaction fails to deliver the 
 **	response using any kind of transports. Transaction should report
 **	transport error to its transaction user.
 **
 **/

static char *TEST1_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test1";
static char *TEST2_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test2";
static char *TEST3_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test3";
static char *TEST4_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test4";
static char *TEST5_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test5";
static char *TEST6_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test6";
static char *TEST7_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test7";

#define TEST1_STATUS_CODE	200
#define TEST2_STATUS_CODE	301
#define TEST3_PROVISIONAL_CODE	PJSIP_SC_QUEUED
#define TEST3_STATUS_CODE	202


static void tsx_user_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e);
static pj_bool_t on_rx_message(pjsip_rx_data *rdata);

/* UAC transaction user module. */
static pjsip_module tsx_user = 
{
    NULL, NULL,				/* prev and next	*/
    { "Tsx-UAS-User", 12},		/* Name.		*/
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
    NULL,				/* on_tx_request()	*/
    NULL,				/* on_tx_response()	*/
    &tsx_user_on_tsx_state,		/* on_tsx_state()	*/
};

/* Module to send request. */
static pjsip_module msg_sender = 
{
    NULL, NULL,				/* prev and next	*/
    { "Msg-Sender", 10},		/* Name.		*/
    -1,					/* Id			*/
    PJSIP_MOD_PRIORITY_APPLICATION-1,	/* Priority		*/
    NULL,				/* User data.		*/
    0,					/* Number of methods supported (=0). */
    { 0 },				/* Array of methods (none) */
    NULL,				/* load()		*/
    NULL,				/* start()		*/
    NULL,				/* stop()		*/
    NULL,				/* unload()		*/
    &on_rx_message,			/* on_rx_request()	*/
    &on_rx_message,			/* on_rx_response()	*/
    NULL,				/* on_tx_request()	*/
    NULL,				/* on_tx_response()	*/
    NULL,				/* on_tsx_state()	*/
};

/* Static vars, which will be reset on each test. */
static int recv_count;
static pj_time_val recv_last;
static pj_bool_t test_complete;

/* Loop transport instance. */
static pjsip_transport *loop;

/* UAS transaction key. */
static char key_buf[64];
static pj_str_t tsx_key = { key_buf, 0 };


/* General timer entry to be used by tests. */
static pj_timer_entry timer;

/* Timer to send response via transaction. */
struct response
{
    pj_str_t	     tsx_key;
    pjsip_tx_data   *tdata;
};

static void send_response_timer( pj_timer_heap_t *timer_heap,
				 struct pj_timer_entry *entry)
{
    pjsip_transaction *tsx;
    struct response *r = entry->user_data;
    pj_status_t status;

    tsx = pjsip_tsx_layer_find_tsx(&r->tsx_key, PJ_TRUE);
    if (!tsx) {
	PJ_LOG(3,(THIS_FILE,"    error: timer unable to find transaction"));
	pjsip_tx_data_dec_ref(r->tdata);
	return;
    }

    status = pjsip_tsx_send_msg(tsx, r->tdata);
    if (status != PJ_SUCCESS) {
	PJ_LOG(3,(THIS_FILE,"    error: timer unable to send response"));
	pjsip_tx_data_dec_ref(r->tdata);
	return;
    }
}

/* Schedule timer to send response for the specified UAS transaction */
static void schedule_send_response( pjsip_rx_data *rdata,
				    const pj_str_t *tsx_key,
				    int status_code,
				    int msec_delay )
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    struct response *r;
    pj_time_val delay;

    status = pjsip_endpt_create_response( endpt, rdata, status_code, NULL, 
					  &tdata);
    if (status != PJ_SUCCESS) {
	app_perror("    error: unable to create response", status);
	test_complete = -198;
	return;
    }

    r = pj_pool_alloc(tdata->pool, sizeof(*r));
    pj_strdup(tdata->pool, &r->tsx_key, tsx_key);
    r->tdata = tdata;

    delay.sec = 0;
    delay.msec = msec_delay;
    pj_time_val_normalize(&delay);

    timer.user_data = r;
    timer.cb = &send_response_timer;

    status = pjsip_endpt_schedule_timer(endpt, &timer, &delay);
    if (status != PJ_SUCCESS) {
	app_perror("    error: unable to schedule timer", status);
	test_complete = -199;
	pjsip_tx_data_dec_ref(tdata);
	return;
    }
}

/*
 * This is the handler to receive state changed notification from the
 * transaction. It is used to verify that the transaction behaves according
 * to the test scenario.
 */
static void tsx_user_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e)
{
    if (pj_strcmp2(&tsx->branch, TEST1_BRANCH_ID)==0 ||
	pj_strcmp2(&tsx->branch, TEST2_BRANCH_ID)==0) 
    {
	/*
	 * TEST1_BRANCH_ID tests that non-INVITE transaction transmits final
	 * response using correct transport and terminates transaction after
	 * T4 (PJSIP_T4_TIMEOUT, 5 seconds).
	 *
	 * TEST2_BRANCH_ID does similar test for non-2xx final response.
	 */
	int status_code = (pj_strcmp2(&tsx->branch, TEST1_BRANCH_ID)==0) ?
			  TEST1_STATUS_CODE : TEST2_STATUS_CODE;

	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    test_complete = 1;

	    /* Check that status code is status_code. */
	    if (tsx->status_code != status_code) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -100;
	    }
	    
	    /* Previous state must be completed. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -101;
	    }

	} else if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

	    /* Previous state must be TRYING. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_TRYING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -102;
	    }
	}

    }
    else
    if (pj_strcmp2(&tsx->branch, TEST3_BRANCH_ID)==0) {
	/*
	 * TEST3_BRANCH_ID tests sending provisional response.
	 */
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    test_complete = 1;

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST3_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -110;
	    }
	    
	    /* Previous state must be completed. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -111;
	    }

	} else if (tsx->state == PJSIP_TSX_STATE_PROCEEDING) {

	    /* Previous state must be TRYING. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_TRYING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -112;
	    }

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST3_PROVISIONAL_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -113;
	    }

	    /* Check that event must be TX_MSG */
	    if (e->body.tsx_state.type != PJSIP_EVENT_TX_MSG) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect event"));
		test_complete = -114;
	    }

	} else if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

	    /* Previous state must be PROCEEDING. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_PROCEEDING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -115;
	    }

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST3_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -116;
	    }

	    /* Check that event must be TX_MSG */
	    if (e->body.tsx_state.type != PJSIP_EVENT_TX_MSG) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect event"));
		test_complete = -117;
	    }

	}

    }

}

/* Save transaction key to global variables. */
static void save_key(pjsip_transaction *tsx)
{
    pj_str_t key;

    pj_strdup(tsx->pool, &key, &tsx->transaction_key);
    pj_strcpy(&tsx_key, &key);
}

/*
 * Message receiver handler.
 */
static pj_bool_t on_rx_message(pjsip_rx_data *rdata)
{
    pjsip_msg *msg = rdata->msg_info.msg;
    pj_str_t branch_param = rdata->msg_info.via->branch_param;
    pj_status_t status;

    if (pj_strcmp2(&branch_param, TEST1_BRANCH_ID) == 0 ||
	pj_strcmp2(&branch_param, TEST2_BRANCH_ID) == 0) 
    {
	/*
	 * TEST1_BRANCH_ID tests that non-INVITE transaction transmits 2xx 
	 * final response using correct transport and terminates transaction 
	 * after 32 seconds.
	 *
	 * TEST2_BRANCH_ID performs similar test for non-2xx final response.
	 */
	int status_code = (pj_strcmp2(&branch_param, TEST1_BRANCH_ID) == 0) ?
			  TEST1_STATUS_CODE : TEST2_STATUS_CODE;

	if (msg->type == PJSIP_REQUEST_MSG) {
	    /* On received response, create UAS and respond with final 
	     * response. 
	     */
	    pjsip_transaction *tsx;
	    pjsip_tx_data *tdata;

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -110;
		return PJ_TRUE;
	    }

	    save_key(tsx);

	    status = pjsip_endpt_create_response(endpt, rdata, 
						 status_code, NULL,
						 &tdata);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create response", status);
		test_complete = -111;
		return PJ_TRUE;
	    }

	    status = pjsip_tsx_send_msg(tsx, tdata);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to send response", status);
		test_complete = -112;
		return PJ_TRUE;
	    }

	} else {
	    /* Verify the response received. */

	    ++recv_count;

	    /* Verify status code. */
	    if (msg->line.status.code != status_code) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -113;
	    }

	    /* Verify that no retransmissions is received. */
	    if (recv_count > 1) {
		PJ_LOG(3,(THIS_FILE, "    error: retransmission received"));
		test_complete = -114;
	    }

	}
	return PJ_TRUE;

    } else if (pj_strcmp2(&branch_param, TEST3_BRANCH_ID) == 0) {

	/* TEST3_BRANCH_ID tests provisional response. */

	if (msg->type == PJSIP_REQUEST_MSG) {
	    /* On received response, create UAS and respond with provisional
	     * response, then schedule timer to send final response.
	     */
	    pjsip_transaction *tsx;
	    pjsip_tx_data *tdata;

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -120;
		return PJ_TRUE;
	    }

	    save_key(tsx);

	    status = pjsip_endpt_create_response(endpt, rdata, 
						 TEST3_PROVISIONAL_CODE, NULL,
						 &tdata);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create response", status);
		test_complete = -121;
		return PJ_TRUE;
	    }

	    status = pjsip_tsx_send_msg(tsx, tdata);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to send response", status);
		test_complete = -122;
		return PJ_TRUE;
	    }

	    schedule_send_response(rdata, &tsx->transaction_key, 
				   TEST3_STATUS_CODE, 2000);

	} else {
	    /* Verify the response received. */

	    ++recv_count;

	    if (recv_count == 1) {
		/* Verify status code. */
		if (msg->line.status.code != TEST3_PROVISIONAL_CODE) {
		    PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		    test_complete = -123;
		}
	    } else if (recv_count == 2) {
		/* Verify status code. */
		if (msg->line.status.code != TEST3_STATUS_CODE) {
		    PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		    test_complete = -124;
		}
	    } else {
		PJ_LOG(3,(THIS_FILE, "    error: retransmission received"));
		test_complete = -125;
	    }

	}
	return PJ_TRUE;

    }

    return PJ_FALSE;
}

/* 
 * The generic test framework, used by most of the tests. 
 */
static int perform_test( char *target_uri, char *from_uri, 
			 char *branch_param, int test_time, 
			 const pjsip_method *method )
{
    pjsip_tx_data *tdata;
    pj_str_t target, from;
    pjsip_via_hdr *via;
    pj_time_val timeout;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, 
	      "   please standby, this will take at most %d seconds..",
	      test_time));

    /* Reset test. */
    recv_count = 0;
    test_complete = 0;
    tsx_key.slen = 0;

    /* Init headers. */
    target = pj_str(target_uri);
    from = pj_str(from_uri);

    /* Create request. */
    status = pjsip_endpt_create_request( endpt, method, &target,
					 &from, &target, NULL, NULL, -1, 
					 NULL, &tdata);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to create request", status);
	return -10;
    }

    /* Set the branch param for test 1. */
    via = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
    via->branch_param = pj_str(branch_param);

    /* Add additional reference to tdata to prevent transaction from
     * deleting it.
     */
    pjsip_tx_data_add_ref(tdata);

    /* Send the first message. */
    status = pjsip_endpt_send_request_stateless(endpt, tdata, NULL, NULL);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to send request", status);
	return -20;
    }

    /* Set test completion time. */
    pj_gettimeofday(&timeout);
    timeout.sec += test_time;

    /* Wait until test complete. */
    while (!test_complete) {
	pj_time_val now, poll_delay = {0, 10};

	pjsip_endpt_handle_events(endpt, &poll_delay);

	pj_gettimeofday(&now);
	if (now.sec > timeout.sec) {
	    PJ_LOG(3,(THIS_FILE, "   Error: test has timed out"));
	    pjsip_tx_data_dec_ref(tdata);
	    return -30;
	}
    }

    if (test_complete < 0) {
	pjsip_transaction *tsx;

	tsx = pjsip_tsx_layer_find_tsx(&tsx_key, PJ_TRUE);
	if (tsx) {
	    pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
	    pj_mutex_unlock(tsx->mutex);
	    flush_events(1000);
	}
	pjsip_tx_data_dec_ref(tdata);
	return test_complete;
    }

    /* Allow transaction to destroy itself */
    flush_events(500);

    /* Make sure transaction has been destroyed. */
    if (pjsip_tsx_layer_find_tsx(&tsx_key, PJ_FALSE) != NULL) {
	PJ_LOG(3,(THIS_FILE, "   Error: transaction has not been destroyed"));
	pjsip_tx_data_dec_ref(tdata);
	return -40;
    }

    /* Check tdata reference counter. */
    if (pj_atomic_get(tdata->ref_cnt) != 1) {
	PJ_LOG(3,(THIS_FILE, "   Error: tdata reference counter is %d",
		      pj_atomic_get(tdata->ref_cnt)));
	pjsip_tx_data_dec_ref(tdata);
	return -50;
    }

    /* Destroy txdata */
    pjsip_tx_data_dec_ref(tdata);

    return PJ_SUCCESS;

}


/*****************************************************************************
 **
 ** TEST1_BRANCH_ID: Basic 2xx final response
 ** TEST2_BRANCH_ID: Basic non-2xx final response
 **
 *****************************************************************************
 */
static int tsx_basic_final_response_test(void)
{
    int status;

    PJ_LOG(3,(THIS_FILE,"  test1: basic sending 2xx final response"));

    status = perform_test("sip:129.0.0.1;transport=loop-dgram",
		          "sip:129.0.0.1;transport=loop-dgram",
			  TEST1_BRANCH_ID,
			  33, /* Test duration must be greater than 32 secs */
			  &pjsip_options_method);
    if (status != 0)
	return status;

    PJ_LOG(3,(THIS_FILE,"  test2: basic sending non-2xx final response"));

    status = perform_test("sip:129.0.0.1;transport=loop-dgram",
		          "sip:129.0.0.1;transport=loop-dgram",
			  TEST2_BRANCH_ID,
			  33, /* Test duration must be greater than 32 secs */
			  &pjsip_options_method);
    if (status != 0)
	return status;

    return 0;
}


/*****************************************************************************
 **
 ** TEST3_BRANCH_ID: Sending provisional response
 **
 *****************************************************************************
 */
static int tsx_basic_provisional_response_test(void)
{
    int status;

    PJ_LOG(3,(THIS_FILE,"  test1: basic sending 2xx final response"));

    status = perform_test("sip:129.0.0.1;transport=loop-dgram",
		          "sip:129.0.0.1;transport=loop-dgram",
			  TEST3_BRANCH_ID,
			  35,
			  &pjsip_options_method);
    return status;
}


/*****************************************************************************
 **
 ** UAS Transaction Test.
 **
 *****************************************************************************
 */
int tsx_uas_test(void)
{
    pj_sockaddr_in addr;
    pj_status_t status;

    /* Check if loop transport is configured. */
    status = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_LOOP_DGRAM, 
				      &addr, sizeof(addr), &loop);
    if (status != PJ_SUCCESS) {
	PJ_LOG(3,(THIS_FILE, "  Error: loop transport is not configured!"));
	return -1;
    }

    /* Register modules. */
    status = pjsip_endpt_register_module(endpt, &tsx_user);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to register module", status);
	return -3;
    }
    status = pjsip_endpt_register_module(endpt, &msg_sender);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to register module", status);
	return -4;
    }

#if 0
    /* TEST1_BRANCH_ID: Basic 2xx final response. 
     * TEST2_BRANCH_ID: Basic non-2xx final response. 
     */
    status = tsx_basic_final_response_test();
    if (status != 0)
	return status;
#endif

    /* TEST3_BRANCH_ID: with provisional response
     */
    status = tsx_basic_provisional_response_test();
    if (status != 0)
	return status;


    pjsip_transport_dec_ref(loop);
    return 0;

}

