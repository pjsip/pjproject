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

#define THIS_FILE   "tsx_uac_test.c"


/*****************************************************************************
 **
 ** UAC tests.
 **
 ** This file performs various tests for UAC transactions. Each test will have
 ** a different Via branch param so that message receiver module and 
 ** transaction user module can identify which test is being carried out.
 **
 ** TEST1_BRANCH_ID
 **	Perform basic retransmission and timeout test. Message receiver will
 **	verify that retransmission is received at correct time.
 **     This test verifies the following requirements:
 **	    - retransmit timer doubles for INVITE
 **	    - retransmit timer doubles and caps off for non-INVITE
 **	    - retransmit timer timer is precise
 **	    - correct timeout and retransmission count
 **     Requirements not tested:
 **	    - retransmit timer only starts after resolving has completed.
 **
 ** TEST2_BRANCH_ID
 **	Test scenario where resolver is unable to resolve destination host.
 **
 ** TEST3_BRANCH_ID
 **	Test scenario where transaction is terminated while resolver is still
 **	running.
 **
 ** TEST4_BRANCH_ID
 **	Test scenario where transport failed after several retransmissions.
 **
 ** TEST5_BRANCH_ID
 **	Test scenario where transaction is terminated by user after several
 **	retransmissions.
 **
 ** TEST6_BRANCH_ID
 **	Test successfull non-INVITE transaction.
 **     It tests the following requirements:
 **	    - transaction correctly moves to COMPLETED state.
 **	    - retransmission must cease.
 **	    - tx_data must be maintained until state is terminated.
 **
 ** TEST7_BRANCH_ID
 **	Test successfull non-INVITE transaction, with provisional response.
 **
 ** TEST8_BRANCH_ID
 **	Test failed INVITE transaction (e.g. ACK must be received)
 **
 ** TEST9_BRANCH_ID
 **	Test failed INVITE transaction with provisional response.
 **
 **	
 *****************************************************************************
 */

static char *TEST1_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-Test1";
static char *TEST2_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-Test2";
static char *TEST3_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-Test3";
static char *TEST4_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-Test4";
static char *TEST5_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-Test5";
static char *TEST6_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-Test6";
static char *TEST7_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-Test7";
static char *TEST8_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-Test8";
static char *TEST9_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-Test9";

#define      TEST1_ALLOWED_DIFF	    (150)
#define      TEST4_RETRANSMIT_CNT   3
#define	     TEST5_RETRANSMIT_CNT   3


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
    NULL,				/* on_tx_request()	*/
    NULL,				/* on_tx_response()	*/
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

/*
 * This is the handler to receive state changed notification from the
 * transaction. It is used to verify that the transaction behaves according
 * to the test scenario.
 */
static void tsx_user_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e)
{
    if (pj_strcmp2(&tsx->branch, TEST1_BRANCH_ID)==0) {
	/*
	 * Transaction with TEST1_BRANCH_ID should terminate with transaction
	 * timeout status.
	 */
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    if (test_complete == 0)
		test_complete = 1;

	    /* Test the status code. */
	    if (tsx->status_code != PJSIP_SC_TSX_TIMEOUT) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: status code is %d instead of %d",
			  tsx->status_code, PJSIP_SC_TSX_TIMEOUT));
		test_complete = -710;
	    }
	}

    } else if (pj_strcmp2(&tsx->branch, TEST2_BRANCH_ID)==0) {
	/*
	 * Transaction with TEST2_BRANCH_ID should terminate with transport error.
	 */
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    /* Test the status code. */
	    if (tsx->status_code != PJSIP_SC_TSX_TRANSPORT_ERROR) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: status code is %d instead of %d",
			  tsx->status_code, PJSIP_SC_TSX_TRANSPORT_ERROR));
		test_complete = -720;
	    }

	    if (test_complete == 0)
		test_complete = 1;
	}

    } else if (pj_strcmp2(&tsx->branch, TEST3_BRANCH_ID)==0) {
	/*
	 * This test terminates the transaction while resolver is still
	 * running. 
	 */
	if (tsx->state == PJSIP_TSX_STATE_CALLING) {

	    /* Terminate the transaction. */
	    pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);

	} else if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    /* Check if status code is correct. */
	    if (tsx->status_code != PJSIP_SC_REQUEST_TERMINATED) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: status code is %d instead of %d",
			  tsx->status_code, PJSIP_SC_REQUEST_TERMINATED));
		test_complete = -730;
	    }

	    if (test_complete == 0)
		test_complete = 1;

	}

    } else if (pj_strcmp2(&tsx->branch, TEST4_BRANCH_ID)==0) {
	/* 
	 * This test simulates transport failure after several 
	 * retransmissions.
	 */
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    /* Status code must be transport error. */
	    if (tsx->status_code != PJSIP_SC_TSX_TRANSPORT_ERROR) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: status code is %d instead of %d",
			  tsx->status_code, PJSIP_SC_TSX_TRANSPORT_ERROR));
		test_complete = -730;
	    }

	    /* Must have correct retransmission count. */
	    if (tsx->retransmit_count != TEST4_RETRANSMIT_CNT) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: retransmit cnt is %d instead of %d",
			  tsx->retransmit_count, TEST4_RETRANSMIT_CNT));
		test_complete = -731;
	    }

	    if (test_complete == 0)
		test_complete = 1;
	}


    } else if (pj_strcmp2(&tsx->branch, TEST5_BRANCH_ID)==0) {
	/* 
	 * This test simulates transport failure after several 
	 * retransmissions.
	 */
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    /* Status code must be PJSIP_SC_REQUEST_TERMINATED. */
	    if (tsx->status_code != PJSIP_SC_REQUEST_TERMINATED) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: status code is %d instead of %d",
			  tsx->status_code, PJSIP_SC_REQUEST_TERMINATED));
		test_complete = -733;
	    }

	    /* Must have correct retransmission count. */
	    if (tsx->retransmit_count != TEST5_RETRANSMIT_CNT) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: retransmit cnt is %d instead of %d",
			  tsx->retransmit_count, TEST5_RETRANSMIT_CNT));
		test_complete = -734;
	    }

	    if (test_complete == 0)
		test_complete = 1;
	}


    } else if (pj_strcmp2(&tsx->branch, TEST6_BRANCH_ID)==0) {
	/* 
	 * Successfull non-INVITE transaction.
	 */
	if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

	    /* Status code must be 202. */
	    if (tsx->status_code != 202) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: status code is %d instead of %d",
			  tsx->status_code, 202));
		test_complete = -736;
	    }

	    /* Must have correct retransmission count. */
	    if (tsx->retransmit_count != 0) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: retransmit cnt is %d instead of %d",
			  tsx->retransmit_count, 0));
		test_complete = -737;
	    }

	    /* Must still keep last_tx */
	    if (tsx->last_tx == NULL) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: transaction lost last_tx"));
		test_complete = -738;
	    }

	    if (test_complete == 0) {
		test_complete = 1;
		pjsip_tsx_terminate(tsx, 202);
	    }
	}
    }
}

#define DIFF(a,b)   ((a<b) ? (b-a) : (a-b))

/*
 * This is the handler to receive message for this test. It is used to
 * control and verify the behavior of the message transmitted by the
 * transaction.
 */
static pj_bool_t msg_receiver_on_rx_request(pjsip_rx_data *rdata)
{
    if (pj_strcmp2(&rdata->msg_info.via->branch_param, TEST1_BRANCH_ID) == 0) {
	/*
	 * The TEST1_BRANCH_ID test performs the verifications for transaction
	 * retransmission mechanism. It will not answer the incoming request
	 * with any response.
	 */
	pjsip_msg *msg = rdata->msg_info.msg;

	PJ_LOG(4,(THIS_FILE, "    received request"));

	/* Only wants to take INVITE or OPTIONS method. */
	if (msg->line.req.method.id != PJSIP_INVITE_METHOD &&
	    msg->line.req.method.id != PJSIP_OPTIONS_METHOD)
	{
	    PJ_LOG(3,(THIS_FILE, "    error: received unexpected method %.*s",
			  msg->line.req.method.name.slen,
			  msg->line.req.method.name.ptr));
	    test_complete = -600;
	    return PJ_TRUE;
	}

	if (recv_count == 0) {
	    recv_count++;
	    //pj_gettimeofday(&recv_last);
	    recv_last = rdata->pkt_info.timestamp;
	} else {
	    pj_time_val now;
	    unsigned msec_expected, msec_elapsed;
	    int max_received;

	    //pj_gettimeofday(&now);
	    now = rdata->pkt_info.timestamp;
	    PJ_TIME_VAL_SUB(now, recv_last);
	    msec_elapsed = now.sec*1000 + now.msec;

	    ++recv_count;
    	    msec_expected = (1<<(recv_count-2))*PJSIP_T1_TIMEOUT;

	    if (msg->line.req.method.id != PJSIP_INVITE_METHOD) {
		if (msec_expected > PJSIP_T2_TIMEOUT)
		    msec_expected = PJSIP_T2_TIMEOUT;
		max_received = 11;
	    } else {
		max_received = 7;
	    }

	    if (DIFF(msec_expected, msec_elapsed) > TEST1_ALLOWED_DIFF) {
		PJ_LOG(3,(THIS_FILE,
			  "    error: expecting retransmission no. %d in %d "
			  "ms, received in %d ms",
			  recv_count-1, msec_expected, msec_elapsed));
		test_complete = -610;
	    }

	    
	    if (recv_count > max_received) {
		PJ_LOG(3,(THIS_FILE, 
			  "    error: too many messages (%d) received",
			  recv_count));
		test_complete = -620;
	    }

	    //pj_gettimeofday(&recv_last);
	    recv_last = rdata->pkt_info.timestamp;
	}
	return PJ_TRUE;

    } else
    if (pj_strcmp2(&rdata->msg_info.via->branch_param, TEST4_BRANCH_ID) == 0) {
	/*
	 * The TEST4_BRANCH_ID test simulates transport failure after several
	 * retransmissions.
	 */
	recv_count++;

	if (recv_count == TEST4_RETRANSMIT_CNT) {
	    /* Simulate transport failure. */
	    pjsip_loop_set_failure(loop, 2, NULL);

	} else if (recv_count > TEST4_RETRANSMIT_CNT) {
	    PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
		      recv_count));
	    test_complete = -631;
	}

	return PJ_TRUE;


    } else
    if (pj_strcmp2(&rdata->msg_info.via->branch_param, TEST5_BRANCH_ID) == 0) {
	/*
	 * The TEST5_BRANCH_ID test simulates user terminating the transaction
	 * after several retransmissions.
	 */
	recv_count++;

	if (recv_count == TEST5_RETRANSMIT_CNT+1) {
	    pj_str_t key;
	    pjsip_transaction *tsx;

	    pjsip_tsx_create_key( rdata->tp_info.pool, &key, PJSIP_ROLE_UAC,
				  &rdata->msg_info.msg->line.req.method, rdata);
	    tsx = pjsip_tsx_layer_find_tsx(&key, PJ_TRUE);
	    if (tsx) {
		pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
		pj_mutex_unlock(tsx->mutex);
	    } else {
		PJ_LOG(3,(THIS_FILE, "    error: uac transaction not found!"));
		test_complete = -633;
	    }

	} else if (recv_count > TEST5_RETRANSMIT_CNT+1) {
	    PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
		      recv_count));
	    test_complete = -634;
	}

	return PJ_TRUE;

    } else
    if (pj_strcmp2(&rdata->msg_info.via->branch_param, TEST6_BRANCH_ID) == 0) {
	/*
	 * The TEST5_BRANCH_ID test successfull non-INVITE transaction.
	 */
	pjsip_tx_data *tdata;
	pjsip_response_addr res_addr;
	pj_status_t status;

	recv_count++;

	if (recv_count > 1) {
	    PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
		      recv_count));
	    test_complete = -635;
	}

	status = pjsip_endpt_create_response(endpt, rdata, 202, NULL, &tdata);
	if (status != PJ_SUCCESS) {
	    app_perror("    error: unable to create response", status);
	    test_complete = -636;
	}

	status = pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
	if (status != PJ_SUCCESS) {
	    app_perror("    error: unable to get response addr", status);
	    test_complete = -637;
	}

	status = pjsip_endpt_send_response(endpt, &res_addr, tdata, NULL,NULL);
	if (status != PJ_SUCCESS) {
	    app_perror("    error: unable to send response", status);
	    test_complete = -638;
	    pjsip_tx_data_dec_ref(tdata);
	}

	return PJ_TRUE;
    }

    return PJ_FALSE;
}

/* 
 * The generic test framework, used by most of the tests. 
 */
static int perform_tsx_test(int dummy, char *target_uri, char *from_uri, 
			    char *branch_param, int test_time, 
			    const pjsip_method *method)
{
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;
    pj_str_t target, from, tsx_key;
    pjsip_via_hdr *via;
    pj_time_val timeout;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, 
	      "   please standby, this will take at most %d seconds..",
	      test_time));

    /* Reset test. */
    recv_count = 0;
    test_complete = 0;

    /* Init headers. */
    target = pj_str(target_uri);
    from = pj_str(from_uri);

    /* Create request. */
    status = pjsip_endpt_create_request( endpt, method, &target,
					 &from, &target, NULL, NULL, -1, 
					 NULL, &tdata);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to create request", status);
	return -100;
    }

    /* Set the branch param for test 1. */
    via = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
    via->branch_param = pj_str(branch_param);

    /* Add additional reference to tdata to prevent transaction from
     * deleting it.
     */
    pjsip_tx_data_add_ref(tdata);

    /* Create transaction. */
    status = pjsip_tsx_create_uac( &tsx_user, tdata, &tsx);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to create UAC transaction", status);
	pjsip_tx_data_dec_ref(tdata);
	return -110;
    }

    /* Get transaction key. */
    pj_strdup(tdata->pool, &tsx_key, &tsx->transaction_key);

    /* Send the message. */
    status = pjsip_tsx_send_msg(tsx, NULL);
    // Ignore send result. Some tests do deliberately triggers error
    // when sending message.
    //if (status != PJ_SUCCESS) {
    //	app_perror("   Error: unable to send request", status);
    //  pjsip_tx_data_dec_ref(tdata);
    //	return -120;
    //}


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
	    return -130;
	}
    }

    if (status < 0) {
	pjsip_tx_data_dec_ref(tdata);
	return status;
    }

    if (test_complete < 0) {
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
	return -140;
    }

    /* Check tdata reference counter. */
    if (pj_atomic_get(tdata->ref_cnt) != 1) {
	PJ_LOG(3,(THIS_FILE, "   Error: tdata reference counter is %d",
		      pj_atomic_get(tdata->ref_cnt)));
	pjsip_tx_data_dec_ref(tdata);
	return -150;
    }

    /* Destroy txdata */
    pjsip_tx_data_dec_ref(tdata);

    return PJ_SUCCESS;
}

/*****************************************************************************
 **
 ** TEST1_BRANCH_ID: UAC basic retransmission and timeout test.
 **
 ** This will test the retransmission of the UAC transaction. Remote will not
 ** answer the transaction, so the transaction should fail. The Via branch prm
 ** TEST1_BRANCH_ID will be used for this test.
 **
 *****************************************************************************
 */
static int tsx_uac_retransmit_test(void)
{
    int status, enabled;
    int i;
    struct {
	const pjsip_method *method;
	unsigned      delay;
    } sub_test[] = 
    {
	{ &pjsip_invite_method, 0},
	{ &pjsip_invite_method, TEST1_ALLOWED_DIFF*2},
	{ &pjsip_options_method, 0},
	{ &pjsip_options_method, TEST1_ALLOWED_DIFF*2}
    };

    PJ_LOG(3,(THIS_FILE, "  test1: basic uac retransmit and timeout test"));


    /* For this test. message printing shound be disabled because it makes
     * incorrect timing.
     */
    enabled = msg_logger_set_enabled(0);

    for (i=0; i<PJ_ARRAY_SIZE(sub_test); ++i) {

	PJ_LOG(3,(THIS_FILE, 
		  "   variant %c: %s with %d ms network delay",
		  ('a' + i),
		  sub_test[i].method->name.ptr,
		  sub_test[i].delay));

	/* Configure transport */
	pjsip_loop_set_failure(loop, 0, NULL);
	pjsip_loop_set_recv_delay(loop, sub_test[i].delay, NULL);

	/* Do the test. */
	status = perform_tsx_test(-500, "sip:bob@127.0.0.1;transport=loop-dgram",
				  "sip:alice@127.0.0.1;transport=loop-dgram", 
				  TEST1_BRANCH_ID,
				  35, sub_test[i].method);
	if (status != 0)
	    break;
    }

    /* Restore transport. */
    pjsip_loop_set_recv_delay(loop, 0, NULL);

    /* Restore msg logger. */
    msg_logger_set_enabled(enabled);

    /* Done. */
    return status;
}

/*****************************************************************************
 **
 ** TEST2_BRANCH_ID: UAC resolve error test.
 **
 ** Test the scenario where destination host is unresolvable. There are
 ** two variants:
 **  (a) resolver returns immediate error
 **  (b) resolver returns error via the callback.
 **
 *****************************************************************************
 */
static int tsx_resolve_error_test(void)
{
    int status;

    PJ_LOG(3,(THIS_FILE, "  test2: resolve error test"));

    /*
     * Variant (a): immediate resolve error.
     */
    PJ_LOG(3,(THIS_FILE, "   variant a: immediate resolving error"));

    status = perform_tsx_test(-800, 
			      "sip:bob@unresolved-host;transport=loop-dgram",
			      "sip:alice@127.0.0.1;transport=loop-dgram", 
			      TEST2_BRANCH_ID, 10, 
			      &pjsip_options_method);
    if (status != 0)
	return status;

    /*
     * Variant (b): error via callback.
     */
    PJ_LOG(3,(THIS_FILE, "   variant b: error via callback"));

    /* Set loop transport to return delayed error. */
    pjsip_loop_set_failure(loop, 2, NULL);
    pjsip_loop_set_send_callback_delay(loop, 10, NULL);

    status = perform_tsx_test(-800, "sip:bob@127.0.0.1;transport=loop-dgram",
			      "sip:alice@127.0.0.1;transport=loop-dgram", 
			      TEST2_BRANCH_ID, 2, 
			      &pjsip_options_method);
    if (status != 0)
	return status;

    /* Restore loop transport settings. */
    pjsip_loop_set_failure(loop, 0, NULL);
    pjsip_loop_set_send_callback_delay(loop, 0, NULL);

    return status;
}


/*****************************************************************************
 **
 ** TEST3_BRANCH_ID: UAC terminate while resolving test.
 **
 ** Terminate the transaction while resolver is still running.
 **
 *****************************************************************************
 */
static int tsx_terminate_resolving_test(void)
{
    unsigned prev_delay;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  test3: terminate while resolving test"));

    /* Configure transport delay. */
    pjsip_loop_set_send_callback_delay(loop, 100, &prev_delay);

    /* Start the test. */
    status = perform_tsx_test(-900, "sip:127.0.0.1;transport=loop-dgram",
			      "sip:127.0.0.1;transport=loop-dgram",
			      TEST3_BRANCH_ID, 2, &pjsip_options_method);

    /* Restore delay. */
    pjsip_loop_set_send_callback_delay(loop, prev_delay, NULL);

    return status;
}


/*****************************************************************************
 **
 ** TEST4_BRANCH_ID: Transport failed after several retransmissions
 **
 ** There are two variants of this test: (a) failure occurs immediately when
 ** transaction calls pjsip_transport_send() or (b) failure is reported via
 ** transport callback.
 **
 *****************************************************************************
 */
static int tsx_retransmit_fail_test(void)
{
    int i;
    unsigned delay[] = {0, 10};
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, 
	      "  test4: transport fails after several retransmissions test"));


    for (i=0; i<PJ_ARRAY_SIZE(delay); ++i) {

	PJ_LOG(3,(THIS_FILE, 
		  "   variant %c: transport delay %d ms", ('a'+i), delay[i]));

	/* Configure transport delay. */
	pjsip_loop_set_send_callback_delay(loop, delay[i], NULL);

	/* Restore transport failure mode. */
	pjsip_loop_set_failure(loop, 0, 0);

	/* Start the test. */
	status = perform_tsx_test(-1000, "sip:127.0.0.1;transport=loop-dgram",
				  "sip:127.0.0.1;transport=loop-dgram",
				  TEST4_BRANCH_ID, 6, &pjsip_options_method);

	if (status != 0)
	    break;

    }

    /* Restore delay. */
    pjsip_loop_set_send_callback_delay(loop, 0, NULL);

    /* Restore transport failure mode. */
    pjsip_loop_set_failure(loop, 0, 0);

    return status;
}


/*****************************************************************************
 **
 ** TEST5_BRANCH_ID: Terminate transaction after several retransmissions
 **
 *****************************************************************************
 */
static int tsx_terminate_after_retransmit_test(void)
{
    int status;

    PJ_LOG(3,(THIS_FILE, "  test5: terminate after retransmissions"));

    /* Do the test. */
    status = perform_tsx_test(-1100, "sip:bob@127.0.0.1;transport=loop-dgram",
			      "sip:alice@127.0.0.1;transport=loop-dgram", 
			      TEST5_BRANCH_ID,
			      6, &pjsip_options_method);

    /* Done. */
    return status;
}


/*****************************************************************************
 **
 ** TEST6_BRANCH_ID: Successfull non-invite transaction
 **
 *****************************************************************************
 */
static int tsx_successfull_non_invite_test(void)
{
    int i, status;
    unsigned delay[] = { 1, 200 };

    PJ_LOG(3,(THIS_FILE, "  test6: successfull non-invite transaction"));

    /* Do the test. */
    for (i=0; i<PJ_ARRAY_SIZE(delay); ++i) {
	
	PJ_LOG(3,(THIS_FILE, "   variant %c: with %d ms transport delay",
			     ('a'+i), delay[i]));

	pjsip_loop_set_delay(loop, delay[i]);

	status = perform_tsx_test(-1200, 
				  "sip:bob@127.0.0.1;transport=loop-dgram",
				  "sip:alice@127.0.0.1;transport=loop-dgram",
				  TEST6_BRANCH_ID,
				  2, &pjsip_options_method);
	if (status != 0)
	    return status;
    }

    pjsip_loop_set_delay(loop, 0);

    /* Done. */
    return status;
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
    pj_status_t status;

    /* Check if loop transport is configured. */
    status = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_LOOP_DGRAM, 
				      &addr, sizeof(addr), &loop);
    if (status != PJ_SUCCESS) {
	PJ_LOG(3,(THIS_FILE, "  Error: loop transport is not configured!"));
	return -10;
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
	return -40;
    }

#if 0
    /* TEST1_BRANCH_ID: Basic retransmit and timeout test. */
    status = tsx_uac_retransmit_test();
    if (status != 0)
	return status;

    /* TEST2_BRANCH_ID: Resolve error test. */
    status = tsx_resolve_error_test();
    if (status != 0)
	return status;

    /* TEST3_BRANCH_ID: UAC terminate while resolving test. */
    status = tsx_terminate_resolving_test();
    if (status != 0)
	return status;

    /* TEST4_BRANCH_ID: Transport failed after several retransmissions */
    status = tsx_retransmit_fail_test();
    if (status != 0)
	return status;

    /* TEST5_BRANCH_ID: Terminate transaction after several retransmissions */
    status = tsx_terminate_after_retransmit_test();
    if (status != 0)
	return status;
#endif

    /* TEST6_BRANCH_ID: Successfull non-invite transaction */
    status = tsx_successfull_non_invite_test();
    if (status != 0)
	return status;


    pjsip_transport_dec_ref(loop);
    return 0;
}

