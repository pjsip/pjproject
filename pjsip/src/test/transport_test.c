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

#define THIS_FILE   "transport_test.c"

///////////////////////////////////////////////////////////////////////////////
/*
 * Generic testing for transport, to make sure that basic
 * attributes have been initialized properly.
 */
int generic_transport_test(pjsip_transport *tp)
{
    PJ_LOG(3,(THIS_FILE, "  structure test..."));

    /* Check that local address name is valid. */
    {
        pj_in_addr addr;

        if (pj_inet_pton(pj_AF_INET(), &tp->local_name.host,
                         &addr) == PJ_SUCCESS)
        {
            PJ_TEST_TRUE(addr.s_addr!=PJ_INADDR_ANY && addr.s_addr!=PJ_INADDR_NONE,
                         "invalid address name", return -420);
        } else {
            /* It's okay. local_name.host may be a hostname instead of
             * IP address.
             */
        }
    }

    /* Check that port is valid. */
    PJ_TEST_GT(tp->local_name.port, 0, NULL, return -430);

    /* Check length of address (for now we only check against sockaddr_in). */
    PJ_TEST_EQ(tp->addr_len, sizeof(pj_sockaddr_in), NULL, return -440);

    /* Check type. */
    PJ_TEST_NEQ(tp->key.type, PJSIP_TRANSPORT_UNSPECIFIED, NULL, return -450);

    /* That's it. */
    return PJ_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
/* 
 * Send/receive test.
 *
 * This test sends a request to loopback address; as soon as request is 
 * received, response will be sent, and time is recorded.
 *
 * The main purpose is to test that the basic transport functionalities works,
 * before we continue with more complicated tests.
 */
#define SEND_RECV_FROM_HDR      "Bob <sip:transport_send_recv_test@example.com>"
#define SEND_RECV_CALL_ID_HDR   "Transport-SendRecv-Test"
#define RT_FROM_HDR             "Bob <sip:transport_rt_test@example.com>"
#define CONTACT_HDR             "Bob <sip:transport_test@127.0.0.1>"
#define CSEQ_VALUE              100
#define BODY                    "Hello World!"

static pj_bool_t my_on_rx_request(pjsip_rx_data *rdata);
static pj_bool_t my_on_rx_response(pjsip_rx_data *rdata);

/* Flag to indicate message has been received
 * (or failed to send)
 */
#define NO_STATUS   -2

struct send_recv_test_global_t
{
    int             send_status;
    int             recv_status;
    pj_timestamp    my_send_time;
    pj_timestamp    my_recv_time;
};
static struct send_recv_test_global_t sr_g[PJSIP_TRANSPORT_START_OTHER];

/* Module to receive messages for this test. */
static pjsip_module send_recv_module = 
{
    NULL, NULL,                         /* prev and next        */
    { "Transport-Test", 14},            /* Name.                */
    -1,                                 /* Id                   */
    PJSIP_MOD_PRIORITY_TSX_LAYER-1,     /* Priority             */
    NULL,                               /* load()               */
    NULL,                               /* start()              */
    NULL,                               /* stop()               */
    NULL,                               /* unload()             */
    &my_on_rx_request,                  /* on_rx_request()      */
    &my_on_rx_response,                 /* on_rx_response()     */
    NULL,                               /* on_tsx_state()       */
};


static pj_bool_t my_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_to_hdr *to_hdr = rdata->msg_info.to;
    pjsip_sip_uri *target = (pjsip_sip_uri*)pjsip_uri_get_uri(to_hdr->uri);
    unsigned tid;

    if (!is_user_equal(rdata->msg_info.from, "transport_send_recv_test"))
        return PJ_FALSE;

    tid = (unsigned)pj_strtoul(&target->user);
    pj_assert(tid < PJSIP_TRANSPORT_START_OTHER);

    /* Check that this is our request. */
    if (pj_strcmp2(&rdata->msg_info.cid->id, SEND_RECV_CALL_ID_HDR) == 0) {
        /* It is! */
        /* Send response. */
        pjsip_tx_data *tdata;
        pjsip_response_addr res_addr;
        pj_status_t status;

        status = pjsip_endpt_create_response( endpt, rdata, 200, NULL, &tdata);
        if (status != PJ_SUCCESS) {
            sr_g[tid].recv_status = status;
            return PJ_TRUE;
        }
        status = pjsip_get_response_addr( tdata->pool, rdata, &res_addr);
        if (status != PJ_SUCCESS) {
            sr_g[tid].recv_status = status;
            pjsip_tx_data_dec_ref(tdata);
            return PJ_TRUE;
        }
        status = pjsip_endpt_send_response( endpt, &res_addr, tdata, NULL, NULL);
        if (status != PJ_SUCCESS) {
            sr_g[tid].recv_status = status;
            pjsip_tx_data_dec_ref(tdata);
            return PJ_TRUE;
        }
        return PJ_TRUE;
    }
    
    /* Not ours. */
    return PJ_FALSE;
}

static pj_bool_t my_on_rx_response(pjsip_rx_data *rdata)
{
    pjsip_to_hdr *to_hdr = rdata->msg_info.to;
    pjsip_sip_uri *target = (pjsip_sip_uri*)pjsip_uri_get_uri(to_hdr->uri);
    unsigned tid;

    if (!is_user_equal(rdata->msg_info.from, "transport_send_recv_test"))
        return PJ_FALSE;

    tid = (unsigned)pj_strtoul(&target->user);
    pj_assert(tid < PJSIP_TRANSPORT_START_OTHER);

    if (pj_strcmp2(&rdata->msg_info.cid->id, SEND_RECV_CALL_ID_HDR) == 0) {
        pj_get_timestamp(&sr_g[tid].my_recv_time);
        sr_g[tid].recv_status = PJ_SUCCESS;
        return PJ_TRUE;
    }
    return PJ_FALSE;
}

/* Transport callback. */
static void send_msg_callback(pjsip_send_state *stateless_data,
                              pj_ssize_t sent, pj_bool_t *cont)
{
    unsigned tid = (unsigned)(long)stateless_data->token;
    pj_assert(tid < PJSIP_TRANSPORT_START_OTHER);

    if (sent < 1) {
        /* Obtain the error code. */
        PJ_LOG(3,(THIS_FILE, "   Sending %s got callback error %ld",
                  stateless_data->tdata->info, -sent));
        sr_g[tid].send_status = (int)-sent;
    } else {
        sr_g[tid].send_status = PJ_SUCCESS;
    }

    /* Don't want to continue. */
    *cont = PJ_FALSE;
}


/* Test that we receive loopback message. */
int transport_send_recv_test( pjsip_transport_type_e tp_type,
                              pjsip_transport *ref_tp,
                              const char *host_port_transport,
                              int *p_usec_rtt)
{
    unsigned tid = tp_type;
    pj_bool_t msg_log_enabled;
    char target_url[64];
    pj_status_t status;
    pj_str_t target, from, to, contact, call_id, body;
    pjsip_method method;
    pjsip_tx_data *tdata;
    pj_time_val timeout;

    PJ_UNUSED_ARG(tp_type);
    PJ_UNUSED_ARG(ref_tp);

    PJ_LOG(3,(THIS_FILE, "  single message round-trip test..."));

    /* Register out test module to receive the message (if necessary). */
    if (send_recv_module.id == -1) {
        status = pjsip_endpt_register_module( endpt, &send_recv_module );
        if (status != PJ_SUCCESS) {
            app_perror("   error: unable to register module", status);
            return -500;
        }
    }

    /* Disable message logging. */
    msg_log_enabled = msg_logger_set_enabled(0);

    pj_ansi_snprintf(target_url, sizeof(target_url), "sip:%d@%s",
                     tp_type, host_port_transport);

    /* Create a request message. */
    target = pj_str(target_url);
    from = pj_str(SEND_RECV_FROM_HDR);
    to = pj_str(target_url);
    contact = pj_str(CONTACT_HDR);
    call_id = pj_str(SEND_RECV_CALL_ID_HDR);
    body = pj_str(BODY);

    pjsip_method_set(&method, PJSIP_OPTIONS_METHOD);
    status = pjsip_endpt_create_request( endpt, &method, &target, &from, &to,
                                         &contact, &call_id, CSEQ_VALUE, 
                                         &body, &tdata );
    if (status != PJ_SUCCESS) {
        app_perror("   error: unable to create request", status);
        return -510;
    }

    /* Reset statuses */
    sr_g[tid].send_status = sr_g[tid].recv_status = NO_STATUS;

    /* Start time. */
    pj_get_timestamp(&sr_g[tid].my_send_time);

    /* Send the message (statelessly). */
    PJ_LOG(3,(THIS_FILE, "Sending request to %.*s", 
                         (int)target.slen, target.ptr));
    status = pjsip_endpt_send_request_stateless( endpt, tdata, (void*)(long)tid,
                                                 &send_msg_callback);
    if (status != PJ_SUCCESS) {
        /* Immediate error! */
        PJ_LOG(3,(THIS_FILE, "   Sending %s to %.*s got immediate error %d",
                  tdata->info, (int)target.slen, target.ptr, status));
        pjsip_tx_data_dec_ref(tdata);
        sr_g[tid].send_status = status;
    }

    /* Set the timeout (2 seconds from now) */
    pj_gettimeofday(&timeout);
    timeout.sec += 2;

    /* Loop handling events until we get status */
    do {
        pj_time_val now;
        pj_time_val poll_interval = { 0, 10 };

        pj_gettimeofday(&now);
        if (PJ_TIME_VAL_GTE(now, timeout)) {
            PJ_LOG(3,(THIS_FILE, "   error: timeout in send/recv test"));
            status = -540;
            goto on_return;
        }

        if (sr_g[tid].send_status!=NO_STATUS && sr_g[tid].send_status!=PJ_SUCCESS) {
            app_perror("   error sending message", sr_g[tid].send_status);
            status = -550;
            goto on_return;
        }

        if (sr_g[tid].recv_status!=NO_STATUS && sr_g[tid].recv_status!=PJ_SUCCESS) {
            app_perror("   error receiving message", sr_g[tid].recv_status);
            status = -560;
            goto on_return;
        }

        if (sr_g[tid].send_status!=NO_STATUS && sr_g[tid].recv_status!=NO_STATUS) {
            /* Success! */
            break;
        }

        pjsip_endpt_handle_events(endpt, &poll_interval);

    } while (1);

    if (status == PJ_SUCCESS) {
        unsigned usec_rt;
        usec_rt = pj_elapsed_usec(&sr_g[tid].my_send_time, &sr_g[tid].my_recv_time);

        PJ_LOG(3,(THIS_FILE, "    round-trip = %d usec", usec_rt));

        *p_usec_rtt = usec_rt;
    }

    /* Restore message logging. */
    msg_logger_set_enabled(msg_log_enabled);

    status = PJ_SUCCESS;

on_return:
    return status;
}


///////////////////////////////////////////////////////////////////////////////
/* 
 * Multithreaded round-trip test
 *
 * This test will spawn multiple threads, each of them send a request. As soon
 * as request is received, response will be sent, and time is recorded.
 *
 * The main purpose of this test is to ensure there's no crash when multiple
 * threads are sending/receiving messages.
 *
 */
static pj_bool_t rt_on_rx_request(pjsip_rx_data *rdata);
static pj_bool_t rt_on_rx_response(pjsip_rx_data *rdata);

static pjsip_module rt_module = 
{
    NULL, NULL,                         /* prev and next        */
    { "Transport-RT-Test", 17},         /* Name.                */
    -1,                                 /* Id                   */
    PJSIP_MOD_PRIORITY_TSX_LAYER-1,     /* Priority             */
    NULL,                               /* load()               */
    NULL,                               /* start()              */
    NULL,                               /* stop()               */
    NULL,                               /* unload()             */
    &rt_on_rx_request,                  /* on_rx_request()      */
    &rt_on_rx_response,                 /* on_rx_response()     */
    NULL,                               /* tsx_handler()        */
};

static struct rt_global_t {
    struct {
        pj_thread_t *thread;
        pj_timestamp send_time;
        pj_timestamp total_rt_time;
        int sent_request_count, recv_response_count;
        pj_str_t call_id;
        pj_timer_entry timeout_timer;
        pj_timer_entry tx_timer;
        pj_mutex_t *mutex;
    } rt_test_data[16];

    char      rt_target_uri[64];
    pj_bool_t rt_stop;
    pj_str_t  rt_call_id;

} g_rt[PJSIP_TRANSPORT_START_OTHER];

static pj_bool_t rt_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_to_hdr *to_hdr = rdata->msg_info.to;
    pjsip_sip_uri *target = (pjsip_sip_uri*)pjsip_uri_get_uri(to_hdr->uri);
    unsigned tid;

    if (!is_user_equal(rdata->msg_info.from, "transport_rt_test"))
        return PJ_FALSE;

    tid = (unsigned)pj_strtoul(&target->user);
    pj_assert(tid < PJSIP_TRANSPORT_START_OTHER);

    if (!pj_strncmp(&rdata->msg_info.cid->id, &g_rt[tid].rt_call_id,
                    g_rt[tid].rt_call_id.slen))
    {
        pjsip_tx_data *tdata;
        pjsip_response_addr res_addr;
        pj_status_t status;

        status = pjsip_endpt_create_response( endpt, rdata, 200, NULL, &tdata);
        if (status != PJ_SUCCESS) {
            app_perror("    error creating response", status);
            return PJ_TRUE;
        }
        status = pjsip_get_response_addr( tdata->pool, rdata, &res_addr);
        if (status != PJ_SUCCESS) {
            app_perror("    error in get response address", status);
            pjsip_tx_data_dec_ref(tdata);
            return PJ_TRUE;
        }
        status = pjsip_endpt_send_response( endpt, &res_addr, tdata, NULL, NULL);
        if (status != PJ_SUCCESS) {
            app_perror("    error sending response", status);
            pjsip_tx_data_dec_ref(tdata);
            return PJ_TRUE;
        }
        return PJ_TRUE;
        
    }
    return PJ_FALSE;
}

static pj_status_t rt_send_request(unsigned tid, int thread_id)
{
    pj_status_t status;
    pj_str_t target, from, to, contact, call_id;
    pjsip_tx_data *tdata;
    pj_time_val timeout_delay;

    pj_mutex_lock(g_rt[tid].rt_test_data[thread_id].mutex);

    /* Create a request message. */
    target = pj_str(g_rt[tid].rt_target_uri);
    from = pj_str(RT_FROM_HDR);
    to = pj_str(g_rt[tid].rt_target_uri);
    contact = pj_str(CONTACT_HDR);
    call_id = g_rt[tid].rt_test_data[thread_id].call_id;

    status = pjsip_endpt_create_request( endpt, &pjsip_options_method, 
                                         &target, &from, &to,
                                         &contact, &call_id, -1, 
                                         NULL, &tdata );
    if (status != PJ_SUCCESS) {
        app_perror("    error: unable to create request", status);
        pj_mutex_unlock(g_rt[tid].rt_test_data[thread_id].mutex);
        return -610;
    }

    /* Start time. */
    pj_get_timestamp(&g_rt[tid].rt_test_data[thread_id].send_time);

    /* Send the message (statelessly). */
    status = pjsip_endpt_send_request_stateless( endpt, tdata, NULL, NULL);
    if (status != PJ_SUCCESS) {
        /* Immediate error! */
        app_perror("    error: send request", status);
        pjsip_tx_data_dec_ref(tdata);
        pj_mutex_unlock(g_rt[tid].rt_test_data[thread_id].mutex);
        return -620;
    }

    /* Update counter. */
    g_rt[tid].rt_test_data[thread_id].sent_request_count++;

    /* Set timeout timer. */
    if (g_rt[tid].rt_test_data[thread_id].timeout_timer.user_data != NULL) {
        pjsip_endpt_cancel_timer(endpt, &g_rt[tid].rt_test_data[thread_id].timeout_timer);
    }
    timeout_delay.sec = 100; timeout_delay.msec = 0;
    g_rt[tid].rt_test_data[thread_id].timeout_timer.user_data = (void*)(pj_ssize_t)1;
    pjsip_endpt_schedule_timer(endpt, &g_rt[tid].rt_test_data[thread_id].timeout_timer,
                               &timeout_delay);

    pj_mutex_unlock(g_rt[tid].rt_test_data[thread_id].mutex);
    return PJ_SUCCESS;
}

static pj_bool_t rt_on_rx_response(pjsip_rx_data *rdata)
{
    pjsip_to_hdr *to_hdr = rdata->msg_info.to;
    pjsip_sip_uri *target = (pjsip_sip_uri*)pjsip_uri_get_uri(to_hdr->uri);
    unsigned tid;

    if (!is_user_equal(rdata->msg_info.from, "transport_rt_test"))
        return PJ_FALSE;

    tid = (unsigned)pj_strtoul(&target->user);
    pj_assert(tid < PJSIP_TRANSPORT_START_OTHER);

    if (!pj_strncmp(&rdata->msg_info.cid->id, &g_rt[tid].rt_call_id,
                    g_rt[tid].rt_call_id.slen))
    {
        char *pos = pj_strchr(&rdata->msg_info.cid->id, '/')+1;
        int thread_id = (*pos - '0');
        pj_timestamp recv_time;

        pj_mutex_lock(g_rt[tid].rt_test_data[thread_id].mutex);

        /* Stop timer. */
        pjsip_endpt_cancel_timer(endpt, &g_rt[tid].rt_test_data[thread_id].timeout_timer);

        /* Update counter and end-time. */
        g_rt[tid].rt_test_data[thread_id].recv_response_count++;
        pj_get_timestamp(&recv_time);

        pj_sub_timestamp(&recv_time, &g_rt[tid].rt_test_data[thread_id].send_time);
        pj_add_timestamp(&g_rt[tid].rt_test_data[thread_id].total_rt_time, &recv_time);

        if (!g_rt[tid].rt_stop) {
            pj_time_val tx_delay = { 0, 0 };
            pj_assert(g_rt[tid].rt_test_data[thread_id].tx_timer.user_data == NULL);
            g_rt[tid].rt_test_data[thread_id].tx_timer.user_data = (void*)(pj_ssize_t)1;
            pjsip_endpt_schedule_timer(endpt, &g_rt[tid].rt_test_data[thread_id].tx_timer,
                                       &tx_delay);
        }

        pj_mutex_unlock(g_rt[tid].rt_test_data[thread_id].mutex);

        return PJ_TRUE;
    }
    return PJ_FALSE;
}

static void rt_timeout_timer( pj_timer_heap_t *timer_heap,
                              struct pj_timer_entry *entry )
{
    unsigned tid = entry->id >> 16;
    unsigned thread_id = entry->id & 0xFFFF;

    pj_assert(tid < PJSIP_TRANSPORT_START_OTHER);
    pj_mutex_lock(g_rt[tid].rt_test_data[thread_id].mutex);

    PJ_UNUSED_ARG(timer_heap);
    PJ_LOG(3,(THIS_FILE, "    timeout waiting for response"));
    g_rt[tid].rt_test_data[thread_id].timeout_timer.user_data = NULL;
    
    if (g_rt[tid].rt_test_data[thread_id].tx_timer.user_data == NULL) {
        pj_time_val delay = { 0, 0 };
        g_rt[tid].rt_test_data[thread_id].tx_timer.user_data = (void*)(pj_ssize_t)1;
        pjsip_endpt_schedule_timer(endpt, &g_rt[tid].rt_test_data[thread_id].tx_timer,
                                   &delay);
    }

    pj_mutex_unlock(g_rt[tid].rt_test_data[thread_id].mutex);
}

static void rt_tx_timer( pj_timer_heap_t *timer_heap,
                         struct pj_timer_entry *entry )
{
    unsigned tid = entry->id >> 16;
    unsigned thread_id = entry->id & 0xFFFF;

    pj_assert(tid < PJSIP_TRANSPORT_START_OTHER);
    pj_mutex_lock(g_rt[tid].rt_test_data[thread_id].mutex);

    PJ_UNUSED_ARG(timer_heap);
    pj_assert(g_rt[tid].rt_test_data[thread_id].tx_timer.user_data != NULL);
    g_rt[tid].rt_test_data[thread_id].tx_timer.user_data = NULL;
    rt_send_request(tid, thread_id);

    pj_mutex_unlock(g_rt[tid].rt_test_data[thread_id].mutex);
}


static int rt_worker_thread(void *arg)
{
    unsigned tid;
    int i;
    pj_time_val poll_delay = { 0, 10 };

    tid = (unsigned)(long)arg;
    pj_assert(tid < PJSIP_TRANSPORT_START_OTHER);

    /* Sleep to allow main threads to run. */
    pj_thread_sleep(10);

    while (!g_rt[tid].rt_stop) {
        pjsip_endpt_handle_events(endpt, &poll_delay);
    }

    /* Exhaust responses. */
    for (i=0; i<100; ++i)
        pjsip_endpt_handle_events(endpt, &poll_delay);

    return 0;
}

int transport_rt_test( pjsip_transport_type_e tp_type,
                       pjsip_transport *ref_tp,
                       const char *host_port_transport,
                       int *lost)
{
    enum { THREADS = 4, INTERVAL = 10 };
    int i, tid=tp_type;
    char target_url[80];
    pj_status_t status;
    pj_pool_t *pool;
    pj_bool_t logger_enabled;

    pj_timestamp zero_time, total_time;
    unsigned usec_rt;
    unsigned total_sent;
    unsigned total_recv;

    PJ_UNUSED_ARG(tp_type);
    PJ_UNUSED_ARG(ref_tp);

    PJ_LOG(3,(THIS_FILE, "  multithreaded round-trip test (%d threads)...",
                  THREADS));
    PJ_LOG(3,(THIS_FILE, "    this will take approx %d seconds, please wait..",
                INTERVAL));

    /* Make sure msg logger is disabled. */
    logger_enabled = msg_logger_set_enabled(0);

    /* Register module (if not yet registered) */
    if (rt_module.id == -1) {
        status = pjsip_endpt_register_module( endpt, &rt_module );
        if (status != PJ_SUCCESS) {
            app_perror("   error: unable to register module", status);
            return -600;
        }
    }

    /* Create pool for this test. */
    pool = pjsip_endpt_create_pool(endpt, NULL, 4000, 4000);
    if (!pool)
        return -610;

    /* Initialize static test data. */
    pj_ansi_snprintf(target_url, sizeof(target_url), "sip:%d@%s",
                     tp_type, host_port_transport);
    pj_ansi_strxcpy(g_rt[tid].rt_target_uri, target_url,
                    sizeof(g_rt[tid].rt_target_uri));
    g_rt[tid].rt_call_id = pj_str("RT-Call-Id/");
    g_rt[tid].rt_stop = PJ_FALSE;

    /* Initialize thread data. */
    for (i=0; i<THREADS; ++i) {
        char buf[1];
        pj_str_t str_id;
        
        pj_strset(&str_id, buf, 1);
        pj_bzero(&g_rt[tid].rt_test_data[i], sizeof(g_rt[tid].rt_test_data[i]));

        /* Init timer entry */
        g_rt[tid].rt_test_data[i].tx_timer.id = (tid << 16) | i;
        g_rt[tid].rt_test_data[i].tx_timer.cb = &rt_tx_timer;
        g_rt[tid].rt_test_data[i].timeout_timer.id = (tid << 16) | i;
        g_rt[tid].rt_test_data[i].timeout_timer.cb = &rt_timeout_timer;

        /* Generate Call-ID for each thread. */
        g_rt[tid].rt_test_data[i].call_id.ptr = (char*) pj_pool_alloc(pool, g_rt[tid].rt_call_id.slen+1);
        pj_strcpy(&g_rt[tid].rt_test_data[i].call_id, &g_rt[tid].rt_call_id);
        buf[0] = '0' + (char)i;
        pj_strcat(&g_rt[tid].rt_test_data[i].call_id, &str_id);

        /* Init mutex. */
        status = pj_mutex_create_recursive(pool, "rt", &g_rt[tid].rt_test_data[i].mutex);
        if (status != PJ_SUCCESS) {
            app_perror("   error: unable to create mutex", status);
            return -615;
        }

        /* Create thread, suspended. */
        status = pj_thread_create(pool, "rttest%p", &rt_worker_thread, 
                                  (void*)(pj_ssize_t)tid, 0,
                                  PJ_THREAD_SUSPENDED, &g_rt[tid].rt_test_data[i].thread);
        if (status != PJ_SUCCESS) {
            app_perror("   error: unable to create thread", status);
            return -620;
        }
    }

    /* Start threads! */
    for (i=0; i<THREADS; ++i) {
        pj_time_val delay = {0,0};
        pj_thread_resume(g_rt[tid].rt_test_data[i].thread);

        /* Schedule first message transmissions. */
        g_rt[tid].rt_test_data[i].tx_timer.user_data = (void*)(pj_ssize_t)1;
        pjsip_endpt_schedule_timer(endpt, &g_rt[tid].rt_test_data[i].tx_timer, &delay);
    }

    /* Sleep for some time. */
    pj_thread_sleep(INTERVAL * 1000);

    /* Signal thread to stop. */
    g_rt[tid].rt_stop = PJ_TRUE;

    /* Wait threads to complete. */
    for (i=0; i<THREADS; ++i) {
        pj_thread_join(g_rt[tid].rt_test_data[i].thread);
        pj_thread_destroy(g_rt[tid].rt_test_data[i].thread);
    }

    /* Destroy rt_test_data */
    for (i=0; i<THREADS; ++i) {
        pj_mutex_destroy(g_rt[tid].rt_test_data[i].mutex);
        pjsip_endpt_cancel_timer(endpt, &g_rt[tid].rt_test_data[i].timeout_timer);
    }

    /* Gather statistics. */
    pj_bzero(&total_time, sizeof(total_time));
    pj_bzero(&zero_time, sizeof(zero_time));
    usec_rt = total_sent = total_recv = 0;
    for (i=0; i<THREADS; ++i) {
        total_sent += g_rt[tid].rt_test_data[i].sent_request_count;
        total_recv +=  g_rt[tid].rt_test_data[i].recv_response_count;
        pj_add_timestamp(&total_time, &g_rt[tid].rt_test_data[i].total_rt_time);
    }

    /* Display statistics. */
    if (total_recv)
        total_time.u64 = total_time.u64/total_recv;
    else
        total_time.u64 = 0;
    usec_rt = pj_elapsed_usec(&zero_time, &total_time);
    PJ_LOG(3,(THIS_FILE, "    done."));
    PJ_LOG(3,(THIS_FILE, "    total %d messages sent", total_sent));
    PJ_LOG(3,(THIS_FILE, "    average round-trip=%d usec", usec_rt));

    pjsip_endpt_release_pool(endpt, pool);

    *lost = total_sent-total_recv;

    /* Flush events. */
    flush_events(500);

    /* Restore msg logger. */
    msg_logger_set_enabled(logger_enabled);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Transport load testing
 */
static pj_bool_t load_on_rx_request(pjsip_rx_data *rdata);

static struct load_test_global_t
{
    pj_int32_t      next_seq;
    pj_bool_t       err;
} g_lt[PJSIP_TRANSPORT_START_OTHER];


static pjsip_module mod_load_test = {
    NULL, NULL,                         /* prev and next        */
    { "mod-load-test", 13},             /* Name.                */
    -1,                                 /* Id                   */
    PJSIP_MOD_PRIORITY_TSX_LAYER-1,     /* Priority             */
    NULL,                               /* load()               */
    NULL,                               /* start()              */
    NULL,                               /* stop()               */
    NULL,                               /* unload()             */
    &load_on_rx_request,                /* on_rx_request()      */
    NULL,                               /* on_rx_response()     */
    NULL,                               /* tsx_handler()        */
};


static pj_bool_t load_on_rx_request(pjsip_rx_data *rdata)
{
    pjsip_to_hdr *to_hdr = rdata->msg_info.to;
    pjsip_sip_uri *target = (pjsip_sip_uri*)pjsip_uri_get_uri(to_hdr->uri);
    unsigned tid;

    if (!is_user_equal(rdata->msg_info.from, "transport_load_test"))
        return PJ_FALSE;

    tid = (unsigned)pj_strtoul(&target->user);
    pj_assert(tid < PJSIP_TRANSPORT_START_OTHER);

    if (rdata->msg_info.cseq->cseq != g_lt[tid].next_seq) {
        PJ_LOG(1,(THIS_FILE, "    err: expecting cseq %u, got %u", 
                  g_lt[tid].next_seq, rdata->msg_info.cseq->cseq));
        g_lt[tid].err = PJ_TRUE;
        g_lt[tid].next_seq = rdata->msg_info.cseq->cseq + 1;
    } else 
        g_lt[tid].next_seq++;
    return PJ_TRUE;
}

int transport_load_test(pjsip_transport_type_e tp_type,
                        const char *host_port_transport)
{
#define ERR(rc__)   { rc=rc__; goto on_return; }
    enum { COUNT = 2000 };
    char target_url[64];
    unsigned i, tid=tp_type;
    int rc;

    pj_ansi_snprintf(target_url, sizeof(target_url), "sip:%d@%s",
                     tp_type, host_port_transport);

    /* exhaust packets */
    flush_events(500);

    PJ_LOG(3,(THIS_FILE, "  transport load test..."));

    if (mod_load_test.id == -1) {
        rc = pjsip_endpt_register_module( endpt, &mod_load_test);
        if (rc != PJ_SUCCESS) {
            app_perror("error registering module", rc);
            return -610;
        }
    }
    g_lt[tid].err = PJ_FALSE;
    g_lt[tid].next_seq = 0;

    for (i=0; i<COUNT && !g_lt[tid].err; ++i) {
        pj_str_t target, from, call_id;
        pjsip_tx_data *tdata;

        target = pj_str(target_url);
        from = pj_str("<sip:transport_load_test@host>");
        call_id = pj_str("thecallid");
        PJ_TEST_SUCCESS(pjsip_endpt_create_request(endpt, &pjsip_invite_method, 
                                            &target, &from, 
                                            &target, &from, &call_id, 
                                            i, NULL, &tdata ),
                        NULL, ERR(-620));

        PJ_TEST_SUCCESS(pjsip_endpt_send_request_stateless(endpt, tdata,
                                                           NULL, NULL),
                        NULL, ERR(-630));

        flush_events(20);
    }

    flush_events(1000);

    PJ_TEST_EQ(g_lt[tid].next_seq, COUNT, "message count mismatch",
               ERR(-640));

    rc = 0;

on_return:
    if (mod_load_test.id != -1) {
        /* Don't unregister if there are multiple transport_load_test() */
        pjsip_endpt_unregister_module( endpt, &mod_load_test);
        mod_load_test.id = -1;
    }
    return rc;
}


