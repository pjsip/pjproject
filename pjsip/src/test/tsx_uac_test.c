/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjsip.h>
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
 **     Perform basic retransmission and timeout test. Message receiver will
 **     verify that retransmission is received at correct time.
 **     This test verifies the following requirements:
 **         - retransmit timer doubles for INVITE
 **         - retransmit timer doubles and caps off for non-INVITE
 **         - retransmit timer timer is precise
 **         - correct timeout and retransmission count
 **     Requirements not tested:
 **         - retransmit timer only starts after resolving has completed.
 **
 ** TEST2_BRANCH_ID
 **     Test scenario where resolver is unable to resolve destination host.
 **
 ** TEST3_BRANCH_ID
 **     Test scenario where transaction is terminated while resolver is still
 **     running.
 **
 ** TEST4_BRANCH_ID
 **     Test scenario where transport failed after several retransmissions.
 **
 ** TEST5_BRANCH_ID
 **     Test scenario where transaction is terminated by user after several
 **     retransmissions.
 **
 ** TEST6_BRANCH_ID
 **     Test successfull non-INVITE transaction.
 **     It tests the following requirements:
 **         - transaction correctly moves to COMPLETED state.
 **         - retransmission must cease.
 **         - tx_data must be maintained until state is terminated.
 **
 ** TEST7_BRANCH_ID
 **     Test successfull non-INVITE transaction, with provisional response.
 **
 ** TEST8_BRANCH_ID
 **     Test failed INVITE transaction (e.g. ACK must be received)
 **
 ** TEST9_BRANCH_ID
 **     Test failed INVITE transaction with provisional response.
 **
 **     
 *****************************************************************************
 */

static char *TEST1_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAC-Test01";
static char *TEST2_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAC-Test02";
static char *TEST3_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAC-Test03";
static char *TEST4_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAC-Test04";
static char *TEST5_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAC-Test05";
static char *TEST6_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAC-Test06";
static char *TEST7_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAC-Test07";
static char *TEST8_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAC-Test08";
static char *TEST9_BRANCH_ID = PJSIP_RFC3261_BRANCH_ID "-UAC-Test09";

#define BRANCH_LEN   (7+11)

// An effort to accommodate CPU load spike on some test machines.
#define      TEST1_ALLOWED_DIFF     500 //(150)
#define      TEST4_RETRANSMIT_CNT   3
#define      TEST5_RETRANSMIT_CNT   3

struct my_timer
{
    pj_timer_entry  entry;
    char            key_buf[1024];
    pj_str_t        tsx_key;
};


static struct tsx_uac_test_global_t
{
    char TARGET_URI[128];
    char FROM_URI[128];
    unsigned tp_flag;
    struct tsx_test_param *test_param;

    /* Static vars, which will be reset on each test. */
    int recv_count;
    pj_time_val recv_last;
    pj_bool_t test_complete;

    /* Loop transport instance. */
    pjsip_transport *loop;

    struct my_timer timer;

} g[MAX_TSX_TESTS];

static void tsx_user_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e);
static pj_bool_t msg_receiver_on_rx_request(pjsip_rx_data *rdata);

/* UAC transaction user module. */
static pjsip_module tsx_user = 
{
    NULL, NULL,                         /* prev and next        */
    { "Tsx-UAC-User", 12},              /* Name.                */
    -1,                                 /* Id                   */
    PJSIP_MOD_PRIORITY_APPLICATION-1,   /* Priority             */
    NULL,                               /* load()               */
    NULL,                               /* start()              */
    NULL,                               /* stop()               */
    NULL,                               /* unload()             */
    NULL,                               /* on_rx_request()      */
    NULL,                               /* on_rx_response()     */
    NULL,                               /* on_tx_request()      */
    NULL,                               /* on_tx_response()     */
    &tsx_user_on_tsx_state,             /* on_tsx_state()       */
};

/* Module to receive the loop-backed request and also process tx msgs. */
static pjsip_module msg_receiver = 
{
    NULL, NULL,                         /* prev and next        */
    { "Msg-Receiver", 12},              /* Name.                */
    -1,                                 /* Id                   */
    /* Note:
     * Priority needs to be more important than UA layer, because UA layer
     * silently absorbs ACK with To tag.
     */
    PJSIP_MOD_PRIORITY_UA_PROXY_LAYER-1,/* Priority             */
    NULL,                               /* load()               */
    NULL,                               /* start()              */
    NULL,                               /* stop()               */
    NULL,                               /* unload()             */
    &msg_receiver_on_rx_request,        /* on_rx_request()      */
    NULL,                               /* on_rx_response()     */
    NULL,                               /* on_tx_request()      */
    NULL,                               /* on_tx_response()     */
    NULL,                               /* on_tsx_state()       */
};

/* Init uac test */
static int init_test(unsigned tid)
{
    pj_sockaddr_in addr;

    pj_enter_critical_section();
    if (tsx_user.id == -1) {
        PJ_TEST_SUCCESS(pjsip_endpt_register_module(endpt, &tsx_user), NULL,
                        { pj_leave_critical_section(); return -30; });
    }

    if (msg_receiver.id == -1) {
        PJ_TEST_SUCCESS(pjsip_endpt_register_module(endpt, &msg_receiver), NULL,
                        { pj_leave_critical_section(); return -40; });
    }
    pj_leave_critical_section();

    pj_assert(g[tid].loop==NULL);
    PJ_TEST_SUCCESS(pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_LOOP_DGRAM, 
                                      &addr, sizeof(addr), NULL, &g[tid].loop),
                    NULL, return -50);

    return PJ_SUCCESS;
}

/* Finish test */
static void finish_test(unsigned tid)
{
    /* Note: don't unregister modules on_tsx_state() may be called when
     *       transaction layer is shutdown later, which will cause
     *       get_tsx_tid() to be called and it needs the module id.
     */    
    if (g[tid].loop) {
        pjsip_transport_dec_ref(g[tid].loop);
        g[tid].loop = 0;
    }
}

/* Get test ID from transaction instance */
static unsigned get_tsx_tid(const pjsip_transaction *tsx)
{
    pj_assert(tsx_user.id >= 0);
    return (unsigned)(long)tsx->mod_data[tsx_user.id];
}

/* Set test ID to transaction instance */
static void set_tsx_tid(pjsip_transaction *tsx, unsigned tid)
{
    pj_assert(tsx_user.id >= 0);
    tsx->mod_data[tsx_user.id] = (void*)(long)tid;
}

/*
 * This is the handler to receive state changed notification from the
 * transaction. It is used to verify that the transaction behaves according
 * to the test scenario.
 */
static void tsx_user_on_tsx_state(pjsip_transaction *tsx, pjsip_event *e)
{
    unsigned tid = get_tsx_tid(tsx);

    PJ_LOG(3,(THIS_FILE, 
                "    on_tsx_state state: %s, event: %s (%s)",
                pjsip_tsx_state_str(tsx->state),
                pjsip_event_str(e->type),
                pjsip_event_str(e->body.tsx_state.type)
                ));

    if (pj_strnicmp2(&tsx->branch, TEST1_BRANCH_ID, BRANCH_LEN)==0) {
        /*
         * Transaction with TEST1_BRANCH_ID should terminate with transaction
         * timeout status.
         */
        if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

            if (g[tid].test_complete == 0)
                g[tid].test_complete = 1;

            /* Test the status code. */
            if (tsx->status_code != PJSIP_SC_TSX_TIMEOUT) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: status code is %d instead of %d",
                          tsx->status_code, PJSIP_SC_TSX_TIMEOUT));
                g[tid].test_complete = -710;
            }


            /* If transport is reliable, then there must not be any
             * retransmissions.
             */
            if (g[tid].tp_flag & PJSIP_TRANSPORT_RELIABLE) {
                if (g[tid].recv_count != 1) {
                    PJ_LOG(3,(THIS_FILE, 
                           "    error: there were %d (re)transmissions",
                           g[tid].recv_count));
                    g[tid].test_complete = -715;
                }
            } else {
                /* Check the number of (re)transmissions, which must be
                 * 6 or 7 for INVITE and 10 or 11 for non-INVITE.
                 * Theoretically the total (re)transmission time is 31,500ms
                 * (plus 300ms for delayed transport), and tsx timeout is 32s.
                 * In some test machines (e.g: MacOS), sometime the tsx timeout
                 * fires first which causes recv_count fall short (by one).
                 */
                //if (tsx->method.id==PJSIP_INVITE_METHOD && recv_count != 7) {
                if (tsx->method.id==PJSIP_INVITE_METHOD && g[tid].recv_count < 6) {
                    PJ_LOG(3,(THIS_FILE, 
                           "    error: there were %d (re)transmissions",
                           g[tid].recv_count));
                    g[tid].test_complete = -716;
                } else
                //if (tsx->method.id==PJSIP_OPTIONS_METHOD && recv_count != 11) {
                if (tsx->method.id==PJSIP_OPTIONS_METHOD && g[tid].recv_count < 10) {
                    PJ_LOG(3,(THIS_FILE, 
                           "    error: there were %d (re)transmissions",
                           g[tid].recv_count));
                    g[tid].test_complete = -717;
                } else
                if (tsx->method.id!=PJSIP_INVITE_METHOD && 
                    tsx->method.id!=PJSIP_OPTIONS_METHOD)
                {
                    PJ_LOG(3,(THIS_FILE, "    error: unexpected method"));
                    g[tid].test_complete = -718;
                }
            }
        }

    } else if (pj_strnicmp2(&tsx->branch, TEST2_BRANCH_ID, BRANCH_LEN)==0) {
        /*
         * Transaction with TEST2_BRANCH_ID should terminate with transport error.
         */
        if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

            /* Test the status code. */
            if (tsx->status_code != PJSIP_SC_TSX_TRANSPORT_ERROR &&
                tsx->status_code != PJSIP_SC_BAD_GATEWAY)
            {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: status code is %d instead of %d or %d",
                          tsx->status_code, PJSIP_SC_TSX_TRANSPORT_ERROR,
                          PJSIP_SC_BAD_GATEWAY));
                g[tid].test_complete = -720;
            }

            if (g[tid].test_complete == 0)
                g[tid].test_complete = 1;
        }

    } else if (pj_strnicmp2(&tsx->branch, TEST3_BRANCH_ID, BRANCH_LEN)==0) {
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
                g[tid].test_complete = -730;
            }

            if (g[tid].test_complete == 0)
                g[tid].test_complete = 1;

        }

    } else if (pj_strnicmp2(&tsx->branch, TEST4_BRANCH_ID, BRANCH_LEN)==0) {
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
                g[tid].test_complete = -730;
            }

            /* Must have correct retransmission count. */
            if (tsx->retransmit_count != TEST4_RETRANSMIT_CNT) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: retransmit cnt is %d instead of %d",
                          tsx->retransmit_count, TEST4_RETRANSMIT_CNT));
                g[tid].test_complete = -731;
            }

            if (g[tid].test_complete == 0)
                g[tid].test_complete = 1;
        }


    } else if (pj_strnicmp2(&tsx->branch, TEST5_BRANCH_ID, BRANCH_LEN)==0) {
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
                g[tid].test_complete = -733;
            }

            /* Must have correct retransmission count. */
            if (tsx->retransmit_count != TEST5_RETRANSMIT_CNT) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: retransmit cnt is %d instead of %d",
                          tsx->retransmit_count, TEST5_RETRANSMIT_CNT));
                g[tid].test_complete = -734;
            }

            if (g[tid].test_complete == 0)
                g[tid].test_complete = 1;
        }


    } else if (pj_strnicmp2(&tsx->branch, TEST6_BRANCH_ID, BRANCH_LEN)==0) {
        /* 
         * Successfull non-INVITE transaction.
         */
        if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

            /* Status code must be 202. */
            if (tsx->status_code != 202) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: status code is %d instead of %d",
                          tsx->status_code, 202));
                g[tid].test_complete = -736;
            }

            /* Must have correct retransmission count. */
            if (tsx->retransmit_count != 0) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: retransmit cnt is %d instead of %d",
                          tsx->retransmit_count, 0));
                g[tid].test_complete = -737;
            }

            /* Must still keep last_tx */
            if (tsx->last_tx == NULL) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: transaction lost last_tx"));
                g[tid].test_complete = -738;
            }

            if (g[tid].test_complete == 0) {
                g[tid].test_complete = 1;
                pjsip_tsx_terminate(tsx, 202);
            }

        } else if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

            /* Previous state must be COMPLETED. */
            if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
                g[tid].test_complete = -7381;
            }

        }

    } else if (pj_strnicmp2(&tsx->branch, TEST7_BRANCH_ID, BRANCH_LEN)==0) {
        /* 
         * Successfull non-INVITE transaction.
         */
        if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

            /* Check prev state. */
            if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_PROCEEDING) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: prev state is %s instead of %s",
                          pjsip_tsx_state_str((pjsip_tsx_state_e)e->body.tsx_state.prev_state),
                          pjsip_tsx_state_str(PJSIP_TSX_STATE_PROCEEDING)));
                g[tid].test_complete = -739;
            }

            /* Status code must be 202. */
            if (tsx->status_code != 202) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: status code is %d instead of %d",
                          tsx->status_code, 202));
                g[tid].test_complete = -740;
            }

            /* Must have correct retransmission count. */
            if (tsx->retransmit_count != 0) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: retransmit cnt is %d instead of %d",
                          tsx->retransmit_count, 0));
                g[tid].test_complete = -741;
            }

            /* Must still keep last_tx */
            if (tsx->last_tx == NULL) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: transaction lost last_tx"));
                g[tid].test_complete = -741;
            }

            if (g[tid].test_complete == 0) {
                g[tid].test_complete = 1;
                pjsip_tsx_terminate(tsx, 202);
            }

        } else if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

            /* Previous state must be COMPLETED. */
            if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
                g[tid].test_complete = -742;
            }

        }


    } else if (pj_strnicmp2(&tsx->branch, TEST8_BRANCH_ID, BRANCH_LEN)==0) {
        /* 
         * Failed INVITE transaction.
         */
        if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

            /* Status code must be 301. */
            if (tsx->status_code != 301) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: status code is %d instead of %d",
                          tsx->status_code, 301));
                g[tid].test_complete = -745;
            }

            /* Must have correct retransmission count. */
            if (tsx->retransmit_count != 0) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: retransmit cnt is %d instead of %d",
                          tsx->retransmit_count, 0));
                g[tid].test_complete = -746;
            }

            /* Must still keep last_tx */
            if (tsx->last_tx == NULL) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: transaction lost last_tx"));
                g[tid].test_complete = -747;
            }

            /* last_tx MUST be the INVITE request
             * (authorization depends on this behavior)
             */
            if (tsx->last_tx && tsx->last_tx->msg->line.req.method.id !=
                PJSIP_INVITE_METHOD)
            {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: last_tx is not INVITE"));
                g[tid].test_complete = -748;
            }
        }
        else if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

            g[tid].test_complete = 1;

            /* Previous state must be COMPLETED. */
            if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: expecting last state=COMLETED instead of %d",
                          e->body.tsx_state.prev_state));
                g[tid].test_complete = -750;
            }

            /* Status code must be 301. */
            if (tsx->status_code != 301) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: status code is %d instead of %d",
                          tsx->status_code, 301));
                g[tid].test_complete = -751;
            }

        }


    } else if (pj_strnicmp2(&tsx->branch, TEST9_BRANCH_ID, BRANCH_LEN)==0) {
        /* 
         * Failed INVITE transaction with provisional response.
         */
        if (tsx->state == PJSIP_TSX_STATE_COMPLETED) {

            /* Previous state must be PJSIP_TSX_STATE_PROCEEDING. */
            if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_PROCEEDING) {
                g[tid].test_complete = -760;
            }

            /* Status code must be 302. */
            if (tsx->status_code != 302) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: status code is %d instead of %d",
                          tsx->status_code, 302));
                g[tid].test_complete = -761;
            }

            /* Must have correct retransmission count. */
            if (tsx->retransmit_count != 0) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: retransmit cnt is %d instead of %d",
                          tsx->retransmit_count, 0));
                g[tid].test_complete = -762;
            }

            /* Must still keep last_tx */
            if (tsx->last_tx == NULL) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: transaction lost last_tx"));
                g[tid].test_complete = -763;
            }

            /* last_tx MUST be INVITE. 
             * (authorization depends on this behavior)
             */
            if (tsx->last_tx && tsx->last_tx->msg->line.req.method.id !=
                PJSIP_INVITE_METHOD)
            {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: last_tx is not INVITE"));
                g[tid].test_complete = -764;
            }

        }
        else if (tsx->state == PJSIP_TSX_STATE_TERMINATED) {

            g[tid].test_complete = 1;

            /* Previous state must be COMPLETED. */
            if (e->body.tsx_state.prev_state != PJSIP_TSX_STATE_COMPLETED) {
                g[tid].test_complete = -767;
            }

            /* Status code must be 302. */
            if (tsx->status_code != 302) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: status code is %d instead of %d",
                          tsx->status_code, 302));
                g[tid].test_complete = -768;
            }

        }

    }
}

/*
 * This timer callback is called to send delayed response.
 */
struct response
{
    pjsip_response_addr  res_addr;
    pjsip_tx_data       *tdata;
};

static void send_response_callback( pj_timer_heap_t *timer_heap,
                                    struct pj_timer_entry *entry)
{
    struct response *r = (struct response*) entry->user_data;
    pjsip_transport *tp = r->res_addr.transport;
    pj_status_t status;

    PJ_UNUSED_ARG(timer_heap);

    status = pjsip_endpt_send_response(endpt, &r->res_addr, r->tdata, NULL, NULL);
    if (status != PJ_SUCCESS) pjsip_tx_data_dec_ref(r->tdata);
    if (tp)
        pjsip_transport_dec_ref(tp);
}

/* Timer callback to terminate a transaction. */
static void terminate_tsx_callback( pj_timer_heap_t *timer_heap,
                                    struct pj_timer_entry *entry)
{
    struct my_timer *m = (struct my_timer *)entry;
    pjsip_transaction *tsx = pjsip_tsx_layer_find_tsx(&m->tsx_key, PJ_FALSE);
    int status_code = entry->id;

    PJ_UNUSED_ARG(timer_heap);

    if (tsx) {
        pjsip_tsx_terminate(tsx, status_code);
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
    pjsip_to_hdr *to_hdr = rdata->msg_info.to;
    pjsip_sip_uri *target = (pjsip_sip_uri*)pjsip_uri_get_uri(to_hdr->uri);
    pjsip_to_hdr *from_hdr = rdata->msg_info.from;
    pjsip_sip_uri *from_uri = (pjsip_sip_uri*)pjsip_uri_get_uri(from_hdr->uri);
    unsigned tid;

    if (pj_strcmp2(&from_uri->user, "tsx_uac_test")) {
        /* Not our message */
        return PJ_FALSE;
    }

    tid = (unsigned)pj_strtol(&target->user);

    PJ_LOG(3,(THIS_FILE, "   on_rx_request %s (recv_count: %d) on %s, branch: %.*s", 
                pjsip_rx_data_get_info(rdata),
                g[tid].recv_count, rdata->tp_info.transport->info,
                (int)rdata->msg_info.via->branch_param.slen,
                rdata->msg_info.via->branch_param.ptr));

    if (pj_strnicmp2(&rdata->msg_info.via->branch_param, TEST1_BRANCH_ID,
                     BRANCH_LEN) == 0) {
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
                          (int)msg->line.req.method.name.slen,
                          msg->line.req.method.name.ptr));
            g[tid].test_complete = -600;
            return PJ_TRUE;
        }

        if (g[tid].recv_count == 0) {
            g[tid].recv_count++;
            //pj_gettimeofday(&recv_last);
            g[tid].recv_last = rdata->pkt_info.timestamp;
        } else {
            pj_time_val now;
            unsigned msec_expected, msec_elapsed;
            int max_received;

            //pj_gettimeofday(&now);
            now = rdata->pkt_info.timestamp;
            PJ_TIME_VAL_SUB(now, g[tid].recv_last);
            msec_elapsed = now.sec*1000 + now.msec;

            ++g[tid].recv_count;
            msec_expected = (1<<(g[tid].recv_count-2))*pjsip_cfg()->tsx.t1;

            if (msg->line.req.method.id != PJSIP_INVITE_METHOD) {
                if (msec_expected > pjsip_cfg()->tsx.t2)
                    msec_expected = pjsip_cfg()->tsx.t2;
                max_received = 11;
            } else {
                max_received = 7;
            }

            if (DIFF(msec_expected, msec_elapsed) > TEST1_ALLOWED_DIFF) {
                PJ_LOG(3,(THIS_FILE,
                          "    error: expecting retransmission no. %d in %d "
                          "ms, received in %d ms",
                          g[tid].recv_count-1, msec_expected, msec_elapsed));
                g[tid].test_complete = -610;
            }

            
            if (g[tid].recv_count > max_received) {
                PJ_LOG(3,(THIS_FILE, 
                          "    error: too many messages (%d) received",
                          g[tid].recv_count));
                g[tid].test_complete = -620;
            }

            //pj_gettimeofday(&recv_last);
            g[tid].recv_last = rdata->pkt_info.timestamp;
        }
        return PJ_TRUE;

    } else
    if (pj_strnicmp2(&rdata->msg_info.via->branch_param, TEST4_BRANCH_ID,
                     BRANCH_LEN) == 0) {
        /*
         * The TEST4_BRANCH_ID test simulates transport failure after several
         * retransmissions.
         */
        g[tid].recv_count++;

        if (g[tid].recv_count == TEST4_RETRANSMIT_CNT) {
            /* Simulate transport failure. */
            pjsip_loop_set_failure(g[tid].loop, 2, NULL);

        } else if (g[tid].recv_count > TEST4_RETRANSMIT_CNT) {
            PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
                      g[tid].recv_count));
            g[tid].test_complete = -631;
        }

        return PJ_TRUE;


    } else
    if (pj_strnicmp2(&rdata->msg_info.via->branch_param, TEST5_BRANCH_ID,
                     BRANCH_LEN) == 0) {
        /*
         * The TEST5_BRANCH_ID test simulates user terminating the transaction
         * after several retransmissions.
         */
        g[tid].recv_count++;

        if (g[tid].recv_count == TEST5_RETRANSMIT_CNT+1) {
            pj_str_t key;
            pjsip_transaction *tsx;

            pjsip_tsx_create_key( rdata->tp_info.pool, &key, PJSIP_ROLE_UAC,
                                  &rdata->msg_info.msg->line.req.method, rdata);
            tsx = pjsip_tsx_layer_find_tsx(&key, PJ_TRUE);
            if (tsx) {
                pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
                pj_grp_lock_release(tsx->grp_lock);
            } else {
                PJ_LOG(3,(THIS_FILE, "    error: uac transaction not found!"));
                g[tid].test_complete = -633;
            }

        } else if (g[tid].recv_count > TEST5_RETRANSMIT_CNT+1) {
            PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
                      g[tid].recv_count));
            g[tid].test_complete = -634;
        }

        return PJ_TRUE;

    } else
    if (pj_strnicmp2(&rdata->msg_info.via->branch_param, TEST6_BRANCH_ID,
                     BRANCH_LEN) == 0) {
        /*
         * The TEST6_BRANCH_ID test successfull non-INVITE transaction.
         */
        pj_status_t status;

        g[tid].recv_count++;

        if (g[tid].recv_count > 1) {
            PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
                      g[tid].recv_count));
            g[tid].test_complete = -635;
        }

        status = pjsip_endpt_respond_stateless(endpt, rdata, 202, NULL,
                                               NULL, NULL);
        if (status != PJ_SUCCESS) {
            app_perror("    error: unable to send response", status);
            g[tid].test_complete = -636;
        }

        return PJ_TRUE;


    } else
    if (pj_strnicmp2(&rdata->msg_info.via->branch_param, TEST7_BRANCH_ID,
                     BRANCH_LEN) == 0) {
        /*
         * The TEST7_BRANCH_ID test successfull non-INVITE transaction
         * with provisional response.
         */
        pj_status_t status;
        pjsip_response_addr res_addr;
        struct response *r;
        pjsip_tx_data *tdata;
        pj_time_val delay = { 2, 0 };

        g[tid].recv_count++;

        if (g[tid].recv_count > 1) {
            PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
                      g[tid].recv_count));
            g[tid].test_complete = -640;
            return PJ_TRUE;
        }

        /* Respond with provisional response */
        status = pjsip_endpt_create_response(endpt, rdata, 100, NULL, &tdata);
        pj_assert(status == PJ_SUCCESS);

        status = pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
        pj_assert(status == PJ_SUCCESS);

        status = pjsip_endpt_send_response(endpt, &res_addr, tdata, 
                                           NULL, NULL);
        pj_assert(status == PJ_SUCCESS);

        /* Create the final response. */
        status = pjsip_endpt_create_response(endpt, rdata, 202, NULL, &tdata);
        pj_assert(status == PJ_SUCCESS);

        /* Schedule sending final response in couple of of secs. */
        r = PJ_POOL_ALLOC_T(tdata->pool, struct response);
        r->res_addr = res_addr;
        r->tdata = tdata;
        if (r->res_addr.transport)
            pjsip_transport_add_ref(r->res_addr.transport);

        g[tid].timer.entry.cb = &send_response_callback;
        g[tid].timer.entry.user_data = r;
        pjsip_endpt_schedule_timer(endpt, &g[tid].timer.entry, &delay);

        return (status == PJ_SUCCESS);

    } else
    if (pj_strnicmp2(&rdata->msg_info.via->branch_param, TEST8_BRANCH_ID,
                     BRANCH_LEN) == 0) {
        /*
         * The TEST8_BRANCH_ID test failed INVITE transaction.
         */
        pjsip_method *method;
        pj_status_t status;

    
        method = &rdata->msg_info.msg->line.req.method;
        g[tid].recv_count++;

        if (method->id == PJSIP_INVITE_METHOD) {

            if (g[tid].recv_count > 1) {
                PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
                          g[tid].recv_count));
                g[tid].test_complete = -635;
            }

            status = pjsip_endpt_respond_stateless(endpt, rdata, 301, NULL,
                                                   NULL, NULL);
            if (status != PJ_SUCCESS) {
                app_perror("    error: unable to send response", status);
                g[tid].test_complete = -636;
            }

        } else if (method->id == PJSIP_ACK_METHOD) {

            if (g[tid].recv_count == 2) {
                pj_str_t key;
                pj_time_val delay = { 5, 0 };
                
                /* Schedule timer to destroy transaction after 5 seconds.
                 * This is to make sure that transaction does not 
                 * retransmit ACK.
                 */
                pjsip_tsx_create_key(rdata->tp_info.pool, &key,
                                     PJSIP_ROLE_UAC, &pjsip_invite_method,
                                     rdata);

                pj_strcpy(&g[tid].timer.tsx_key, &key);
                g[tid].timer.entry.id = 301;
                g[tid].timer.entry.cb = &terminate_tsx_callback;

                pjsip_endpt_schedule_timer(endpt, &g[tid].timer.entry, &delay);
            }

            if (g[tid].recv_count > 2) {
                PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
                          g[tid].recv_count));
                g[tid].test_complete = -638;
            }


        } else {
            PJ_LOG(3,(THIS_FILE,"   error: not expecting %s",
                      pjsip_rx_data_get_info(rdata)));
            g[tid].test_complete = -639;

        }


    } else
    if (pj_strnicmp2(&rdata->msg_info.via->branch_param, TEST9_BRANCH_ID,
                     BRANCH_LEN) == 0) {
        /*
         * The TEST9_BRANCH_ID test failed INVITE transaction with
         * provisional response.
         */
        pjsip_method *method;
        pj_status_t status = PJ_SUCCESS;

        method = &rdata->msg_info.msg->line.req.method;

        g[tid].recv_count++;

        if (method->id == PJSIP_INVITE_METHOD) {

            pjsip_response_addr res_addr;
            struct response *r;
            pjsip_tx_data *tdata;
            pj_time_val delay = { 2, 0 };

            if (g[tid].recv_count > 1) {
                PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
                          g[tid].recv_count));
                g[tid].test_complete = -650;
                return PJ_TRUE;
            }

            /* Respond with provisional response */
            status = pjsip_endpt_create_response(endpt, rdata, 100, NULL, 
                                                 &tdata);
            pj_assert(status == PJ_SUCCESS);

            status = pjsip_get_response_addr(tdata->pool, rdata, &res_addr);
            pj_assert(status == PJ_SUCCESS);

            status = pjsip_endpt_send_response(endpt, &res_addr, tdata, 
                                               NULL, NULL);
            pj_assert(status == PJ_SUCCESS);

            /* Create the final response. */
            status = pjsip_endpt_create_response(endpt, rdata, 302, NULL, 
                                                 &tdata);
            pj_assert(status == PJ_SUCCESS);

            /* Schedule sending final response in couple of of secs. */
            r = PJ_POOL_ALLOC_T(tdata->pool, struct response);
            r->res_addr = res_addr;
            r->tdata = tdata;
            if (r->res_addr.transport)
                pjsip_transport_add_ref(r->res_addr.transport);

            g[tid].timer.entry.cb = &send_response_callback;
            g[tid].timer.entry.user_data = r;
            pjsip_endpt_schedule_timer(endpt, &g[tid].timer.entry, &delay);

        } else if (method->id == PJSIP_ACK_METHOD) {

            if (g[tid].recv_count == 2) {
                pj_str_t key;
                pj_time_val delay = { 5, 0 };
                
                /* Schedule timer to destroy transaction after 5 seconds.
                 * This is to make sure that transaction does not 
                 * retransmit ACK.
                 */
                pjsip_tsx_create_key(rdata->tp_info.pool, &key,
                                     PJSIP_ROLE_UAC, &pjsip_invite_method,
                                     rdata);

                pj_strcpy(&g[tid].timer.tsx_key, &key);
                g[tid].timer.entry.id = 302;
                g[tid].timer.entry.cb = &terminate_tsx_callback;

                pjsip_endpt_schedule_timer(endpt, &g[tid].timer.entry, &delay);
            }

            if (g[tid].recv_count > 2) {
                PJ_LOG(3,(THIS_FILE,"   error: not expecting %d-th packet!",
                          g[tid].recv_count));
                g[tid].test_complete = -638;
            }


        } else {
            PJ_LOG(3,(THIS_FILE,"   error: not expecting %s",
                      pjsip_rx_data_get_info(rdata)));
            g[tid].test_complete = -639;

        }

        return (status == PJ_SUCCESS);

    }

    return PJ_FALSE;
}

/* 
 * The generic test framework, used by most of the tests. 
 */
static int perform_tsx_test(unsigned tid, int dummy, char *target_uri,
                            char *from_uri, char *branch_param, int test_time,
                            const pjsip_method *method)
{
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;
    pj_str_t target, from, tsx_key;
    pjsip_via_hdr *via;
    char branch_buf[BRANCH_LEN+20];
    pj_time_val timeout;
    pj_status_t status;

    PJ_UNUSED_ARG(dummy);
    PJ_TEST_EQ(strlen(branch_param), BRANCH_LEN, NULL, return -99);

    PJ_LOG(3,(THIS_FILE, 
              "   please standby, this will take at most %d seconds..",
              test_time));

    /* Reset test. */
    g[tid].recv_count = 0;
    g[tid].test_complete = 0;

    /* Init headers. */
    target = pj_str(target_uri);
    from = pj_str(from_uri);

    /* Create request. */
    status = pjsip_endpt_create_request( endpt, method, &target,
                                         &from, &target, NULL, NULL, -1, 
                                         NULL, &tdata);
    if (status != PJ_SUCCESS) {
        app_perror("   Error: unable to create request", status);
        return -105;
    }

    /* Set the branch param. Note that other tsx_uac_test() instances may
     * be running simultaneously, thus the branch ID needs to be made unique
     * by adding tid */
    via = (pjsip_via_hdr*) pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
    pj_ansi_snprintf(branch_buf, sizeof(branch_buf),
                     "%s-%02d", branch_param, tid);
    pj_strdup2(tdata->pool, &via->branch_param, branch_buf);

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
    set_tsx_tid(tsx, tid);

    /* Get transaction key. */
    pj_strdup(tdata->pool, &tsx_key, &tsx->transaction_key);

    /* Send the message. */
    status = pjsip_tsx_send_msg(tsx, NULL);
    // Ignore send result. Some tests do deliberately triggers error
    // when sending message.
    if (status != PJ_SUCCESS) {
        // app_perror("   Error: unable to send request", status);
        pjsip_tx_data_dec_ref(tdata);
        // return -120;
    }


    /* Set test completion time. */
    pj_gettimeofday(&timeout);
    timeout.sec += test_time;

    /* Wait until test complete. */
    while (!g[tid].test_complete) {
        pj_time_val now, poll_delay = {0, 10};

        pjsip_endpt_handle_events(endpt, &poll_delay);

        pj_gettimeofday(&now);
        if (now.sec > timeout.sec) {
            PJ_LOG(3,(THIS_FILE, "   Error: test has timed out"));
            pjsip_tx_data_dec_ref(tdata);
            return -130;
        }
    }

    if (g[tid].test_complete < 0) {
        tsx = pjsip_tsx_layer_find_tsx(&tsx_key, PJ_TRUE);
        if (tsx) {
            pjsip_tsx_terminate(tsx, PJSIP_SC_REQUEST_TERMINATED);
            pj_grp_lock_release(tsx->grp_lock);
            flush_events(1000);
        }
        pjsip_tx_data_dec_ref(tdata);
        return g[tid].test_complete;

    } else {
        pj_time_val now;

        /* Allow transaction to destroy itself */
        flush_events(500);

        /* Wait until test completes */
        pj_gettimeofday(&now);

        if (PJ_TIME_VAL_LT(now, timeout)) {
            pj_time_val interval;
            interval = timeout;
            PJ_TIME_VAL_SUB(interval, now);
            flush_events(PJ_TIME_VAL_MSEC(interval));
        }
    }

    /* Make sure transaction has been destroyed. */
    if (pjsip_tsx_layer_find_tsx(&tsx_key, PJ_FALSE) != NULL) {
        PJ_LOG(3,(THIS_FILE, "   Error: transaction has not been destroyed"));
        pjsip_tx_data_dec_ref(tdata);
        return -140;
    }

    /* Check tdata reference counter. */
    if (pj_atomic_get(tdata->ref_cnt) != 1) {
        PJ_LOG(3,(THIS_FILE, "   Error: tdata reference counter is %ld",
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
static int tsx_uac_retransmit_test(unsigned tid)
{
    int status = 0, enabled;
    int i;
    struct {
        const pjsip_method *method;
        unsigned      delay;
    } sub_test[] = 
    {
        { &pjsip_invite_method, 0},
        //{ &pjsip_invite_method, TEST1_ALLOWED_DIFF*2},
        { &pjsip_invite_method, 300},

        { &pjsip_options_method, 0},
        //{ &pjsip_options_method, TEST1_ALLOWED_DIFF*2}
        { &pjsip_options_method, 300}
    };

    PJ_LOG(3,(THIS_FILE, "  test1: basic uac retransmit and timeout test"));

    /* For this test. message printing shound be disabled because it makes
     * incorrect timing.
     */
    enabled = msg_logger_set_enabled(0);

    for (i=0; i<(int)PJ_ARRAY_SIZE(sub_test); ++i) {

        PJ_LOG(3,(THIS_FILE, 
                  "   variant %c: %s with %d ms network delay",
                  ('a' + i),
                  sub_test[i].method->name.ptr,
                  sub_test[i].delay));

        /* Configure transport */
        if (g[tid].test_param->type == PJSIP_TRANSPORT_LOOP_DGRAM) {
            pjsip_loop_set_failure(g[tid].loop, 0, NULL);
            pjsip_loop_set_recv_delay(g[tid].loop, sub_test[i].delay, NULL);
        }

        /* Do the test. */
        status = perform_tsx_test(tid, -500, g[tid].TARGET_URI,
                                  g[tid].FROM_URI, TEST1_BRANCH_ID,
                                  35, sub_test[i].method);
        if (status != 0)
            break;
    }

    /* Restore transport. */
    if (g[tid].test_param->type == PJSIP_TRANSPORT_LOOP_DGRAM) {
        pjsip_loop_set_recv_delay(g[tid].loop, 0, NULL);
    }

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
static int tsx_resolve_error_test(unsigned tid)
{
    int status = 0;

    PJ_LOG(3,(THIS_FILE, "  test2: resolve error test"));

    /*
     * Variant (a): immediate resolve error.
     */
    PJ_LOG(3,(THIS_FILE, "   variant a: immediate resolving error"));

    status = perform_tsx_test(tid, -800, 
                              "sip:bob@unresolved-host",
                              g[tid].FROM_URI,  TEST2_BRANCH_ID, 20, 
                              &pjsip_options_method);
    if (status != 0)
        return status;

    /*
     * Variant (b): error via callback.
     */
    PJ_LOG(3,(THIS_FILE, "   variant b: error via callback"));

    /* This only applies to "loop-dgram" transport */
    if (g[tid].test_param->type == PJSIP_TRANSPORT_LOOP_DGRAM) {
        int prev_fail = 0;
        unsigned prev_delay = 0;

        /* Set loop transport to return delayed error. */
        pjsip_loop_set_failure(g[tid].loop, 2, &prev_fail);
        pjsip_loop_set_send_callback_delay(g[tid].loop, 10, &prev_delay);

        status = perform_tsx_test(tid, -800, g[tid].TARGET_URI,
                                  g[tid].FROM_URI, TEST2_BRANCH_ID, 2,
                                  &pjsip_options_method);

        /* Restore loop transport settings. */
        pjsip_loop_set_failure(g[tid].loop, prev_fail, NULL);
        pjsip_loop_set_send_callback_delay(g[tid].loop, prev_delay, NULL);

        if (status != 0)
            return status;
    }

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
static int tsx_terminate_resolving_test(unsigned tid)
{
    unsigned prev_delay;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "  test3: terminate while resolving test"));

    /* Configure transport delay. */
    if (g[tid].test_param->type == PJSIP_TRANSPORT_LOOP_DGRAM) {
        pjsip_loop_set_send_callback_delay(g[tid].loop, 100, &prev_delay);
    }

    /* Start the test. */
    status = perform_tsx_test(tid, -900, g[tid].TARGET_URI, g[tid].FROM_URI,
                              TEST3_BRANCH_ID, 2, &pjsip_options_method);

    /* Restore delay. */
    if (g[tid].test_param->type == PJSIP_TRANSPORT_LOOP_DGRAM) {
        pjsip_loop_set_send_callback_delay(g[tid].loop, prev_delay, NULL);
    }

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
static int tsx_retransmit_fail_test(unsigned tid)
{
    int i;
    unsigned delay[] = {0, 10};
    pj_status_t status = PJ_SUCCESS;

    PJ_LOG(3,(THIS_FILE, 
              "  test4: transport fails after several retransmissions test"));


    for (i=0; i<(int)PJ_ARRAY_SIZE(delay); ++i) {

        PJ_LOG(3,(THIS_FILE, 
                  "   variant %c: transport delay %d ms", ('a'+i), delay[i]));

        /* Configure transport delay. */
        pjsip_loop_set_send_callback_delay(g[tid].loop, delay[i], NULL);

        /* Restore transport failure mode. */
        pjsip_loop_set_failure(g[tid].loop, 0, 0);

        /* Start the test. */
        status = perform_tsx_test(tid, -1000, g[tid].TARGET_URI, g[tid].FROM_URI,
                                  TEST4_BRANCH_ID, 6, &pjsip_options_method);

        if (status != 0)
            break;

    }

    /* Restore delay. */
    pjsip_loop_set_send_callback_delay(g[tid].loop, 0, NULL);

    /* Restore transport failure mode. */
    pjsip_loop_set_failure(g[tid].loop, 0, 0);

    return status;
}


/*****************************************************************************
 **
 ** TEST5_BRANCH_ID: Terminate transaction after several retransmissions
 **
 *****************************************************************************
 */
static int tsx_terminate_after_retransmit_test(unsigned tid)
{
    int status;

    PJ_LOG(3,(THIS_FILE, "  test5: terminate after retransmissions"));

    /* Do the test. */
    status = perform_tsx_test(tid, -1100, g[tid].TARGET_URI, g[tid].FROM_URI,
                              TEST5_BRANCH_ID,
                              6, &pjsip_options_method);

    /* Done. */
    return status;
}


/*****************************************************************************
 **
 ** TEST6_BRANCH_ID: Successfull non-invite transaction
 ** TEST7_BRANCH_ID: Successfull non-invite transaction with provisional
 ** TEST8_BRANCH_ID: Failed invite transaction
 ** TEST9_BRANCH_ID: Failed invite transaction with provisional
 **
 *****************************************************************************
 */
static int perform_generic_test( unsigned tid,
                                 const char *title,
                                 char *branch_id,
                                 const pjsip_method *method)
{
    int i, status = 0;
    unsigned delay[] = { 1, 200 };

    PJ_LOG(3,(THIS_FILE, "  %s", title));
    
    /* Do the test. */
    for (i=0; i<(int)PJ_ARRAY_SIZE(delay); ++i) {
        
        if (g[tid].test_param->type == PJSIP_TRANSPORT_LOOP_DGRAM) {
            PJ_LOG(3,(THIS_FILE, "   variant %c: with %d ms transport delay",
                                 ('a'+i), delay[i]));

            pjsip_loop_set_failure(g[tid].loop, 0, 0);
            pjsip_loop_set_delay(g[tid].loop, delay[i]);
        }

        status = perform_tsx_test(tid, -1200, g[tid].TARGET_URI, g[tid].FROM_URI,
                                  branch_id, 10, method);
        if (status != 0)
            return status;

        if (g[tid].test_param->type != PJSIP_TRANSPORT_LOOP_DGRAM)
            break;
    }

    if (g[tid].test_param->type == PJSIP_TRANSPORT_LOOP_DGRAM) {
        pjsip_loop_set_delay(g[tid].loop, 0);
    }

    /* Done. */
    return status;
}


/*****************************************************************************
 **
 ** UAC Transaction Test.
 **
 *****************************************************************************
 */
int tsx_uac_test(unsigned tid)
{
#define ERR(rc__)   { status=rc__; goto on_return; }
    struct tsx_test_param *param = &tsx_test[tid];
    int status;

    g[tid].timer.tsx_key.ptr = g[tid].timer.key_buf;

    g[tid].test_param = param;

    /* Get transport flag */
    g[tid].tp_flag = pjsip_transport_get_flag_from_type(
                        (pjsip_transport_type_e)g[tid].test_param->type);

    pj_ansi_snprintf(g[tid].TARGET_URI, sizeof(g[tid].TARGET_URI),
                     "sip:%d@127.0.0.1:%d;transport=%s", 
                    tid, param->port, param->tp_type);
    pj_ansi_snprintf(g[tid].FROM_URI, sizeof(g[tid].FROM_URI),
                     "sip:tsx_uac_test@127.0.0.1:%d;transport=%s", 
                    param->port, param->tp_type);

    if ((status=init_test(tid)) != 0)
        return status;

    status = tsx_uac_retransmit_test(tid);
    if (status != 0)
        goto on_return;

    /* TEST2_BRANCH_ID: Resolve error test. */
    status = tsx_resolve_error_test(tid);
    if (status != 0)
        goto on_return;

    /* TEST3_BRANCH_ID: UAC terminate while resolving test. */
    status = tsx_terminate_resolving_test(tid);
    if (status != 0)
        goto on_return;

    /* TEST4_BRANCH_ID: Transport failed after several retransmissions.
     *                  Only applies to loop transport.
     */
    if (g[tid].test_param->type == PJSIP_TRANSPORT_LOOP_DGRAM) {
        status = tsx_retransmit_fail_test(tid);
        if (status != 0)
            goto on_return;
    }

    /* TEST5_BRANCH_ID: Terminate transaction after several retransmissions 
     *                  Only applicable to non-reliable transports.
     */
    if ((g[tid].tp_flag & PJSIP_TRANSPORT_RELIABLE) == 0) {
        status = tsx_terminate_after_retransmit_test(tid);
        if (status != 0)
            goto on_return;
    }

    /* TEST6_BRANCH_ID: Successfull non-invite transaction */
    status = perform_generic_test(tid,
                                  "test6: successfull non-invite transaction",
                                  TEST6_BRANCH_ID, &pjsip_options_method);
    if (status != 0)
        goto on_return;

    /* TEST7_BRANCH_ID: Successfull non-invite transaction */
    status = perform_generic_test(tid,
                                  "test7: successfull non-invite transaction "
                                  "with provisional response",
                                  TEST7_BRANCH_ID, &pjsip_options_method);
    if (status != 0)
        goto on_return;

    /* TEST8_BRANCH_ID: Failed invite transaction */
    status = perform_generic_test(tid,
                                  "test8: failed invite transaction",
                                  TEST8_BRANCH_ID, &pjsip_invite_method);

    if (status != 0)
        goto on_return;

    /* TEST9_BRANCH_ID: Failed invite transaction with provisional response */
    status = perform_generic_test(tid,
                                  "test9: failed invite transaction with "
                                  "provisional response",
                                  TEST9_BRANCH_ID, &pjsip_invite_method);
    if (status != 0)
        goto on_return;

    flush_events(500);

on_return:
    finish_test(tid);
    return status;
#undef ERR
}

