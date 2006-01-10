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
 **	transaction user upon receiving request  retransmissions on TRYING
 **	state
 **
 ** TEST5_BRANCH_ID
 **	As above, in PROCEEDING state.
 **
 ** TEST6_BRANCH_ID
 **	As above, in COMPLETED state, with first sending provisional response.
 **
 ** TEST7_BRANCH_ID
 **	INVITE transaction MUST retransmit non-2xx final response.
 **
 ** TEST8_BRANCH_ID
 **	As above, for INVITE's 2xx final response (this is PJSIP specific).
 **
 ** TEST9_BRANCH_ID
 **	INVITE transaction MUST cease retransmission of final response when
 **	ACK is received. (Note: PJSIP also retransmit 2xx final response 
 **	until it's terminated by user).
 **     Transaction also MUST terminate in T4 seconds.
 **
 ** TEST10_BRANCH_ID
 **	Test where INVITE UAS transaction never receives ACK
 **
 ** TEST11_BRANCH_ID
 **	When UAS failed to deliver the response with the selected transport,
 **	it should try contacting the client with other transport or begin
 **	RFC 3263 server resolution procedure.
 **	This should be tested on:
 **	    a. TRYING state (when delivering first response).
 **	    b. PROCEEDING state (when failed to retransmit last response
 **	       upon receiving request retransmission).
 **	    c. COMPLETED state.
 **
 ** TEST12_BRANCH_ID
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
static char *TEST8_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test8";
static char *TEST9_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test9";
static char *TEST10_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test10";
static char *TEST11_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test11";
static char *TEST12_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAS-Test12";

#define TEST1_STATUS_CODE	200
#define TEST2_STATUS_CODE	301
#define TEST3_PROVISIONAL_CODE	PJSIP_SC_QUEUED
#define TEST3_STATUS_CODE	202
#define TEST4_STATUS_CODE	200
#define TEST4_REQUEST_COUNT	2
#define TEST5_PROVISIONAL_CODE	100
#define TEST5_STATUS_CODE	200	
#define TEST5_REQUEST_COUNT	2
#define TEST5_RESPONSE_COUNT	2
#define TEST6_PROVISIONAL_CODE	100
#define TEST6_STATUS_CODE	200	/* Must be final */
#define TEST6_REQUEST_COUNT	2
#define TEST6_RESPONSE_COUNT	3
#define TEST7_STATUS_CODE	301
#define TEST8_STATUS_CODE	302


#define TEST4_TITLE "test4: absorbing request retransmission"
#define TEST5_TITLE "test5: retransmit last response in PROCEEDING state"
#define TEST6_TITLE "test6: retransmit last response in COMPLETED state"


#define TEST_TIMEOUT_ERROR	-30
#define MAX_ALLOWED_DIFF	150

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

/* Timer callback to send response. */
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

/* Utility to send response. */
static void send_response( pjsip_rx_data *rdata,
			   pjsip_transaction *tsx,
			   int status_code )
{
    pj_status_t status;
    pjsip_tx_data *tdata;

    status = pjsip_endpt_create_response( endpt, rdata, status_code, NULL, 
					  &tdata);
    if (status != PJ_SUCCESS) {
	app_perror("    error: unable to create response", status);
	test_complete = -196;
	return;
    }

    status = pjsip_tsx_send_msg(tsx, tdata);
    if (status != PJ_SUCCESS) {
	app_perror("    error: unable to send response", status);
	pjsip_tx_data_dec_ref(tdata);
	test_complete = -197;
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


/* Find and terminate tsx with the specified key. */
static void terminate_our_tsx(int status_code)
{
    pjsip_transaction *tsx;

    tsx = pjsip_tsx_layer_find_tsx(&tsx_key, PJ_TRUE);
    if (!tsx) {
	PJ_LOG(3,(THIS_FILE,"    error: timer unable to find transaction"));
	return;
    }

    pjsip_tsx_terminate(tsx, status_code);
    pj_mutex_unlock(tsx->mutex);
}

/* Timer callback to terminate transaction. */
static void terminate_tsx_timer( pj_timer_heap_t *timer_heap,
				 struct pj_timer_entry *entry)
{
    terminate_our_tsx(entry->id);
}


/* Schedule timer to terminate transaction. */
static void schedule_terminate_tsx( pjsip_transaction *tsx,
				    int status_code,
				    int msec_delay )
{
    pj_time_val delay;

    delay.sec = 0;
    delay.msec = msec_delay;
    pj_time_val_normalize(&delay);

    pj_assert(pj_strcmp(&tsx->transaction_key, &tsx_key)==0);
    timer.user_data = NULL;
    timer.id = status_code;
    timer.cb = &terminate_tsx_timer;
    pjsip_endpt_schedule_timer(endpt, &timer, &delay);
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

    } else
    if (pj_strcmp2(&tsx->branch, TEST4_BRANCH_ID)==0) {
	/*
	 * TEST4_BRANCH_ID tests receiving retransmissions in TRYING state.
	 */
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST4_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -120;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_TRYING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -121;
	    }

	} else if (tsx->state != PJSIP_TSX_STATE_DESTROYED) 
	{
	    PJ_LOG(3,(THIS_FILE, "    error: unexpected state"));
	    test_complete = -122;

	}


    } else
    if (pj_strcmp2(&tsx->branch, TEST5_BRANCH_ID)==0) {
	/*
	 * TEST5_BRANCH_ID tests receiving retransmissions in PROCEEDING state
	 */
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST5_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -130;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_PROCEEDING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -131;
	    }

	} else if (tsx->state == PJSIP_TSX_STATE_PROCEEDING) {

	    /* Check status code. */
	    if (tsx->status_code != TEST5_PROVISIONAL_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -132;
	    }

	} else if (tsx->state != PJSIP_TSX_STATE_DESTROYED) {
	    PJ_LOG(3,(THIS_FILE, "    error: unexpected state"));
	    test_complete = -133;

	}

    } else
    if (pj_strcmp2(&tsx->branch, TEST6_BRANCH_ID)==0) {
	/*
	 * TEST6_BRANCH_ID tests receiving retransmissions in COMPLETED state
	 */
	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    /* Check that status code is status_code. */
	    if (tsx->status_code != TEST6_STATUS_CODE) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -140;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -141;
	    }

	} else if (tsx->state != PJSIP_TSX_STATE_PROCEEDING &&
		   tsx->state != PJSIP_TSX_STATE_COMPLETED &&
		   tsx->state != PJSIP_TSX_STATE_DESTROYED) 
	{
	    PJ_LOG(3,(THIS_FILE, "    error: unexpected state"));
	    test_complete = -142;

	}


    } else
    if (pj_strcmp2(&tsx->branch, TEST7_BRANCH_ID)==0 ||
	pj_strcmp2(&tsx->branch, TEST8_BRANCH_ID)==0) 
    {
	/*
	 * TEST7_BRANCH_ID and TEST8_BRANCH_ID test retransmission of
	 * INVITE final response
	 */
	int code;

	if (pj_strcmp2(&tsx->branch, TEST7_BRANCH_ID) == 0)
	    code = TEST7_STATUS_CODE;
	else
	    code = TEST8_STATUS_CODE;

	if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

	    if (test_complete == 0)
		test_complete = 1;

	    /* Check status code. */
	    if (tsx->status_code != PJSIP_SC_TSX_TIMEOUT) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -150;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -151;
	    }

	} else if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

	    /* Check that status code is status_code. */
	    if (tsx->status_code != code) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect status code"));
		test_complete = -152;
	    }
	    
	    /* Previous state. */
	    if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_TRYING) {
		PJ_LOG(3,(THIS_FILE, "    error: incorrect prev_state"));
		test_complete = -153;
	    }

	} else if (tsx->state != PJSIP_TSX_STATE_DESTROYED)  {

	    PJ_LOG(3,(THIS_FILE, "    error: unexpected state"));
	    test_complete = -154;

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

#define DIFF(a,b)   ((a<b) ? (b-a) : (a-b))

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

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -110;
		return PJ_TRUE;
	    }

	    save_key(tsx);
	    send_response(rdata, tsx, status_code);

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

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -120;
		return PJ_TRUE;
	    }

	    save_key(tsx);

	    send_response(rdata, tsx, TEST3_PROVISIONAL_CODE);
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

    } else if (pj_strcmp2(&branch_param, TEST4_BRANCH_ID) == 0 ||
	       pj_strcmp2(&branch_param, TEST5_BRANCH_ID) == 0 ||
	       pj_strcmp2(&branch_param, TEST6_BRANCH_ID) == 0) 
    {

	/* TEST4_BRANCH_ID: absorbs retransmissions in TRYING state. */
	/* TEST5_BRANCH_ID: retransmit last response in PROCEEDING state. */
	/* TEST6_BRANCH_ID: retransmit last response in COMPLETED state. */

	if (msg->type == PJSIP_REQUEST_MSG) {
	    /* On received response, create UAS. */
	    pjsip_transaction *tsx;

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -130;
		return PJ_TRUE;
	    }

	    save_key(tsx);

	    if (pj_strcmp2(&branch_param, TEST4_BRANCH_ID) == 0) {

	    } else if (pj_strcmp2(&branch_param, TEST5_BRANCH_ID) == 0) {
		send_response(rdata, tsx, TEST5_PROVISIONAL_CODE);

	    } else if (pj_strcmp2(&branch_param, TEST6_BRANCH_ID) == 0) {
		send_response(rdata, tsx, TEST6_PROVISIONAL_CODE);
		send_response(rdata, tsx, TEST6_STATUS_CODE);
	    }

	} else {
	    /* Verify the response received. */
	    
	    ++recv_count;

	    if (pj_strcmp2(&branch_param, TEST4_BRANCH_ID) == 0) {
		PJ_LOG(3,(THIS_FILE, "    error: not expecting response!"));
		test_complete = -132;

	    } else if (pj_strcmp2(&branch_param, TEST5_BRANCH_ID) == 0) {

		if (rdata->msg_info.msg->line.status.code!=TEST5_PROVISIONAL_CODE) {
		    PJ_LOG(3,(THIS_FILE, "    error: incorrect status code!"));
		    test_complete = -133;

		} 
		if (recv_count > TEST5_RESPONSE_COUNT) {
		    PJ_LOG(3,(THIS_FILE, "    error: not expecting response!"));
		    test_complete = -134;
		}

	    } else if (pj_strcmp2(&branch_param, TEST6_BRANCH_ID) == 0) {

		int code = rdata->msg_info.msg->line.status.code;

		switch (recv_count) {
		case 1:
		    if (code != TEST6_PROVISIONAL_CODE) {
			PJ_LOG(3,(THIS_FILE, "    error: invalid code!"));
			test_complete = -135;
		    }
		    break;
		case 2:
		case 3:
		    if (code != TEST6_STATUS_CODE) {
			PJ_LOG(3,(THIS_FILE, "    error: invalid code!"));
			test_complete = -136;
		    }
		    break;
		default:
		    PJ_LOG(3,(THIS_FILE, "    error: not expecting response"));
		    test_complete = -137;
		    break;
		}
	    }
	}
	return PJ_TRUE;


    } else if (pj_strcmp2(&branch_param, TEST7_BRANCH_ID) == 0 ||
	       pj_strcmp2(&branch_param, TEST8_BRANCH_ID) == 0) 
    {

	/*
	 * TEST7_BRANCH_ID and TEST8_BRANCH_ID test the retransmission
	 * of INVITE final response
	 */
	if (msg->type == PJSIP_REQUEST_MSG) {

	    /* On received response, create UAS. */
	    pjsip_transaction *tsx;

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -140;
		return PJ_TRUE;
	    }

	    save_key(tsx);

	    if (pj_strcmp2(&branch_param, TEST7_BRANCH_ID) == 0) {

		send_response(rdata, tsx, TEST7_STATUS_CODE);

	    } else {

		send_response(rdata, tsx, TEST8_STATUS_CODE);

	    }

	} else {
	    int code;

	    ++recv_count;

	    if (pj_strcmp2(&branch_param, TEST7_BRANCH_ID) == 0)
		code = TEST7_STATUS_CODE;
	    else
		code = TEST8_STATUS_CODE;

	    if (recv_count==1) {
		
		if (rdata->msg_info.msg->line.status.code != code) {
		    PJ_LOG(3,(THIS_FILE,"    error: invalid status code"));
		    test_complete = -141;
		}

		recv_last = rdata->pkt_info.timestamp;

	    } else {

		pj_time_val now;
		unsigned msec, msec_expected;

		now = rdata->pkt_info.timestamp;

		PJ_TIME_VAL_SUB(now, recv_last);
	    
		msec = now.sec*1000 + now.msec;
		msec_expected = (1 << (recv_count-2)) * PJSIP_T1_TIMEOUT;
		if (msec_expected > PJSIP_T2_TIMEOUT)
		    msec_expected = PJSIP_T2_TIMEOUT;

		if (DIFF(msec, msec_expected) > MAX_ALLOWED_DIFF) {
		    PJ_LOG(3,(THIS_FILE,
			      "    error: incorrect retransmission "
			      "time (%d ms expected, %d ms received",
			      msec_expected, msec));
		    test_complete = -142;
		}

		if (recv_count > 11) {
		    PJ_LOG(3,(THIS_FILE,"    error: too many responses (%d)",
					recv_count));
		    test_complete = -143;
		}

		recv_last = rdata->pkt_info.timestamp;
	    }

	}
	return PJ_TRUE;

    } else if (pj_strcmp2(&branch_param, TEST9_BRANCH_ID)) {

	/*
	 * TEST9_BRANCH_ID tests that the retransmission of INVITE final 
	 * response should cease when ACK is received. Transaction also MUST
	 * terminate in T4 seconds.
	 */
	if (msg->type == PJSIP_REQUEST_MSG) {

	    /* On received response, create UAS. */
	    pjsip_transaction *tsx;

	    status = pjsip_tsx_create_uas(&tsx_user, rdata, &tsx);
	    if (status != PJ_SUCCESS) {
		app_perror("    error: unable to create transaction", status);
		test_complete = -140;
		return PJ_TRUE;
	    }

	    save_key(tsx);

	    if (pj_strcmp2(&branch_param, TEST7_BRANCH_ID) == 0) {

		send_response(rdata, tsx, TEST7_STATUS_CODE);

	    } else {

		send_response(rdata, tsx, TEST8_STATUS_CODE);

	    }

	} else {
	    int code;

	    ++recv_count;

	    if (pj_strcmp2(&branch_param, TEST7_BRANCH_ID) == 0)
		code = TEST7_STATUS_CODE;
	    else
		code = TEST8_STATUS_CODE;

	    if (recv_count==1) {
		
		if (rdata->msg_info.msg->line.status.code != code) {
		    PJ_LOG(3,(THIS_FILE,"    error: invalid status code"));
		    test_complete = -141;
		}

		recv_last = rdata->pkt_info.timestamp;

	    } else {

		pj_time_val now;
		unsigned msec, msec_expected;

		now = rdata->pkt_info.timestamp;

		PJ_TIME_VAL_SUB(now, recv_last);
	    
		msec = now.sec*1000 + now.msec;
		msec_expected = (1 << (recv_count-2)) * PJSIP_T1_TIMEOUT;
		if (msec_expected > PJSIP_T2_TIMEOUT)
		    msec_expected = PJSIP_T2_TIMEOUT;

		if (DIFF(msec, msec_expected) > MAX_ALLOWED_DIFF) {
		    PJ_LOG(3,(THIS_FILE,
			      "    error: incorrect retransmission "
			      "time (%d ms expected, %d ms received",
			      msec_expected, msec));
		    test_complete = -142;
		}

		if (recv_count > 11) {
		    PJ_LOG(3,(THIS_FILE,"    error: too many responses (%d)",
					recv_count));
		    test_complete = -143;
		}

		recv_last = rdata->pkt_info.timestamp;
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
			 const pjsip_method *method,
			 int request_cnt, int request_interval_msec,
			 int expecting_timeout)
{
    pjsip_tx_data *tdata;
    pj_str_t target, from;
    pjsip_via_hdr *via;
    pj_time_val timeout, next_send;
    int sent_cnt;
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

    /* Schedule first send. */
    sent_cnt = 0;
    pj_gettimeofday(&next_send);
    pj_time_val_normalize(&next_send);

    /* Set test completion time. */
    pj_gettimeofday(&timeout);
    timeout.sec += test_time;

    /* Wait until test complete. */
    while (!test_complete) {
	pj_time_val now, poll_delay = {0, 10};

	pjsip_endpt_handle_events(endpt, &poll_delay);

	pj_gettimeofday(&now);

	if (sent_cnt < request_cnt && PJ_TIME_VAL_GTE(now, next_send)) {
	    /* Add additional reference to tdata to prevent transaction from
	     * deleting it.
	     */
	    pjsip_tx_data_add_ref(tdata);

	    /* (Re)Send the request. */
	    status = pjsip_endpt_send_request_stateless(endpt, tdata, 0, 0);
	    if (status != PJ_SUCCESS) {
		app_perror("   Error: unable to send request", status);
		pjsip_tx_data_dec_ref(tdata);
		return -20;
	    }

	    /* Schedule next send, if any. */
	    sent_cnt++;
	    if (sent_cnt < request_cnt) {
		pj_gettimeofday(&next_send);
		next_send.msec += request_interval_msec;
		pj_time_val_normalize(&next_send);
	    }
	}

	if (now.sec > timeout.sec) {
	    if (!expecting_timeout)
		PJ_LOG(3,(THIS_FILE, "   Error: test has timed out"));
	    pjsip_tx_data_dec_ref(tdata);
	    return TEST_TIMEOUT_ERROR;
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
			  &pjsip_options_method, 1, 0, 0);
    if (status != 0)
	return status;

    PJ_LOG(3,(THIS_FILE,"  test2: basic sending non-2xx final response"));

    status = perform_test("sip:129.0.0.1;transport=loop-dgram",
		          "sip:129.0.0.1;transport=loop-dgram",
			  TEST2_BRANCH_ID,
			  33, /* Test duration must be greater than 32 secs */
			  &pjsip_options_method, 1, 0, 0);
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
			  &pjsip_options_method, 1, 0, 0);

    return status;
}


/*****************************************************************************
 **
 ** TEST4_BRANCH_ID: Absorbs retransmissions in TRYING state
 ** TEST5_BRANCH_ID: Absorbs retransmissions in PROCEEDING state
 ** TEST6_BRANCH_ID: Absorbs retransmissions in COMPLETED state
 **
 *****************************************************************************
 */
static int tsx_retransmit_last_response_test(const char *title,
					     char *branch_id,
					     int request_cnt,
					     int status_code)
{
    int status;

    PJ_LOG(3,(THIS_FILE,"  %s", title));

    status = perform_test("sip:129.0.0.1;transport=loop-dgram",
		          "sip:129.0.0.1;transport=loop-dgram",
			  branch_id,
			  5,
			  &pjsip_options_method, 
			  request_cnt, 1000, 1);
    if (status && status != TEST_TIMEOUT_ERROR)
	return status;
    if (!status) {
	PJ_LOG(3,(THIS_FILE, "   error: expecting timeout"));
	return -31;
    }

    terminate_our_tsx(status_code);
    flush_events(100);

    if (test_complete != 1)
	return test_complete;

    flush_events(100);
    return 0;
}

/*****************************************************************************
 **
 ** TEST7_BRANCH_ID: INVITE non-2xx final response retransmission test
 ** TEST8_BRANCH_ID: INVITE 2xx final response retransmission test
 **
 *****************************************************************************
 */
static int tsx_final_response_retransmission_test(void)
{
    int status;

    PJ_LOG(3,(THIS_FILE,
	      "  test7: INVITE non-2xx final response retransmission"));

    status = perform_test("sip:129.0.0.1;transport=loop-dgram",
		          "sip:129.0.0.1;transport=loop-dgram",
			  TEST7_BRANCH_ID,
			  33, /* Test duration must be greater than 32 secs */
			  &pjsip_invite_method, 1, 0, 0);
    if (status != 0)
	return status;

    PJ_LOG(3,(THIS_FILE,
	      "  test8: INVITE 2xx final response retransmission"));

    status = perform_test("sip:129.0.0.1;transport=loop-dgram",
		          "sip:129.0.0.1;transport=loop-dgram",
			  TEST8_BRANCH_ID,
			  33, /* Test duration must be greater than 32 secs */
			  &pjsip_invite_method, 1, 0, 0);
    if (status != 0)
	return status;

    return 0;
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

    /* TEST3_BRANCH_ID: with provisional response
     */
    status = tsx_basic_provisional_response_test();
    if (status != 0)
	return status;

    /* TEST4_BRANCH_ID: absorbs retransmissions in TRYING state
     */
    status = tsx_retransmit_last_response_test(TEST4_TITLE,
					       TEST4_BRANCH_ID, 
					       TEST4_REQUEST_COUNT,
					       TEST4_STATUS_CODE);
    if (status != 0)
	return status;

    /* TEST5_BRANCH_ID: retransmit last response in PROCEEDING state
     */
    status = tsx_retransmit_last_response_test(TEST5_TITLE,
					       TEST5_BRANCH_ID, 
					       TEST5_REQUEST_COUNT,
					       TEST5_STATUS_CODE);
    if (status != 0)
	return status;

    /* TEST6_BRANCH_ID: retransmit last response in PROCEEDING state
     */
    status = tsx_retransmit_last_response_test(TEST6_TITLE,
					       TEST6_BRANCH_ID, 
					       TEST6_REQUEST_COUNT,
					       TEST6_STATUS_CODE);
    if (status != 0)
	return status;

    /* TEST7_BRANCH_ID: INVITE non-2xx final response retransmission test
     * TEST8_BRANCH_ID: INVITE 2xx final response retransmission test
     */

    status = tsx_final_response_retransmission_test();
    if (status != 0)
	return status;

#endif

    pjsip_transport_dec_ref(loop);
    return 0;

}

