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
#include <pjsip/sip_util.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_errno.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>

#define THIS_FILE   "sip_util_statefull.c"

/* The transport of the latest attempt of a pending request, kept alive. In
 * the pool of the original request, linked from the module data of the
 * original and, while their attempt lasts, of the copies.
 */
struct req_state
{
    pjsip_tx_data *cur;             /* The request of the current attempt */
    pjsip_transport *tp;
};

struct tsx_data
{
    void *token;
    void (*cb)(void*, pjsip_event*);
    struct req_state *state;
    pj_bool_t allow_failover;
    unsigned first_addr;            /* Where this transaction started in
                                       the address list */
    pjsip_tx_data *orig_tdata;
    /* The Via settings of the application, as the send updates them */
    pjsip_host_port via_addr;
    const void *via_tp;
    unsigned failed_servers_gen;    /* When the request was sent */
};

/* Defined in sip_endpoint.c */
unsigned pjsip_endpt_failed_servers_gen(pjsip_endpoint *endpt);
pj_status_t pjsip_endpt_set_server_failed_gen(pjsip_endpoint *endpt,
                                              const pj_sockaddr_t *addr,
                                              unsigned duration, unsigned gen);

/* Guards the state of the pending requests: the transport reports its
 * sends from its own threads.
 */
static pj_pool_t *mod_pool;
static pj_mutex_t *req_mutex;

static pj_status_t mod_util_load(pjsip_endpoint *endpt);
static pj_status_t mod_util_unload(void);
static pj_status_t mod_util_on_tx_request(pjsip_tx_data *tdata);
static void mod_util_on_tsx_state(pjsip_transaction*, pjsip_event*);

/* This module will be registered in pjsip_endpt.c */

pjsip_module mod_stateful_util = 
{
    NULL, NULL,                     /* prev, next.                      */
    { "mod-stateful-util", 17 },    /* Name.                            */
    -1,                             /* Id                               */
    PJSIP_MOD_PRIORITY_APPLICATION, /* Priority                         */
    &mod_util_load,                 /* load()                           */
    NULL,                           /* start()                          */
    NULL,                           /* stop()                           */
    &mod_util_unload,               /* unload()                         */
    NULL,                           /* on_rx_request()                  */
    NULL,                           /* on_rx_response()                 */
    &mod_util_on_tx_request,        /* on_tx_request.                   */
    NULL,                           /* on_tx_response()                 */
    &mod_util_on_tsx_state,         /* on_tsx_state()                   */
};

static pj_status_t mod_util_load(pjsip_endpoint *endpt)
{
    pj_status_t status;

    mod_pool = pjsip_endpt_create_pool(endpt, "stfutil%p", 256, 256);
    if (!mod_pool)
        return PJ_ENOMEM;
    status = pj_mutex_create_simple(mod_pool, "stfutil%p", &req_mutex);
    if (status != PJ_SUCCESS) {
        pj_pool_release(mod_pool);
        mod_pool = NULL;
    }
    return status;
}

static pj_status_t mod_util_unload(void)
{
    if (req_mutex) {
        pj_mutex_destroy(req_mutex);
        req_mutex = NULL;
    }
    if (mod_pool) {
        pj_pool_release(mod_pool);
        mod_pool = NULL;
    }
    return PJ_SUCCESS;
}

/* The transport sends the request, or a copy, to an address: note the
 * transport it uses.
 */
static pj_status_t mod_util_on_tx_request(pjsip_tx_data *tdata)
{
    struct req_state *state;
    pjsip_transport *old_tp = NULL;

    if (mod_stateful_util.id < 0 || !req_mutex ||
        !tdata->mod_data[mod_stateful_util.id])
    {
        return PJ_SUCCESS;
    }

    pj_mutex_lock(req_mutex);
    state = (struct req_state*) tdata->mod_data[mod_stateful_util.id];
    if (state && state->cur == tdata &&
        state->tp != tdata->tp_info.transport)
    {
        old_tp = state->tp;
        state->tp = tdata->tp_info.transport;
        if (state->tp)
            pjsip_transport_add_ref(state->tp);
    }
    pj_mutex_unlock(req_mutex);

    if (old_tp)
        pjsip_transport_dec_ref(old_tp);
    return PJ_SUCCESS;
}

/* The attempt is over: a late send of its request, e.g: to another address
 * once a connection fails, is not reported. A copy may outlive the state.
 */
static void end_attempt(struct req_state *state, pjsip_tx_data *tdata,
                        pj_bool_t is_copy)
{
    if (!state || !req_mutex)
        return;
    pj_mutex_lock(req_mutex);
    if (state->cur == tdata)
        state->cur = NULL;
    if (is_copy)
        tdata->mod_data[mod_stateful_util.id] = NULL;
    pj_mutex_unlock(req_mutex);
}

PJ_DEF(pj_status_t) pjsip_endpt_follow_request_transport(
                                                    pjsip_endpoint *endpt,
                                                    pjsip_tx_data *tdata)
{
    PJ_ASSERT_RETURN(endpt && tdata, PJ_EINVAL);
    PJ_UNUSED_ARG(endpt);

    if (mod_stateful_util.id < 0 || !req_mutex)
        return PJ_EINVALIDOP;

    /* A request sent again, e.g: with credentials, has one already */
    pj_mutex_lock(req_mutex);
    if (!tdata->mod_data[mod_stateful_util.id]) {
        tdata->mod_data[mod_stateful_util.id] =
            PJ_POOL_ZALLOC_T(tdata->pool, struct req_state);
    }
    pj_mutex_unlock(req_mutex);
    return PJ_SUCCESS;
}

PJ_DEF(pjsip_transport*) pjsip_endpt_get_request_transport(
                                                    pjsip_endpoint *endpt,
                                                    pjsip_tx_data *tdata)
{
    struct req_state *state;
    pjsip_transport *tp = NULL;

    PJ_ASSERT_RETURN(endpt && tdata, NULL);
    PJ_UNUSED_ARG(endpt);

    if (mod_stateful_util.id < 0 || !req_mutex)
        return NULL;

    pj_mutex_lock(req_mutex);
    state = (struct req_state*) tdata->mod_data[mod_stateful_util.id];
    if (state)
        tp = state->tp;
    pj_mutex_unlock(req_mutex);
    return tp;
}

/* RFC 3263 section 4.3: a 503, or no response at all before a timeout or
 * a transport error, is a failure of the server.
 */
static pj_bool_t is_server_failure(pjsip_transaction *tsx, pjsip_event *event)
{
    switch (event->body.tsx_state.type) {
    case PJSIP_EVENT_RX_MSG:
        return tsx->status_code == PJSIP_SC_SERVICE_UNAVAILABLE;
    case PJSIP_EVENT_TIMER:
    case PJSIP_EVENT_TRANSPORT_ERROR:
        return event->body.tsx_state.prev_state == PJSIP_TSX_STATE_CALLING;
    default:
        return PJ_FALSE;
    }
}

/* Whether the request is bound to a connection: the transport manager
 * uses a selected reliable transport for every address, so a copy could
 * only go to the same server.
 */
static pj_bool_t is_pinned(const pjsip_tx_data *tdata)
{
    const pjsip_transport *tp = tdata->tp_sel.u.transport;

    return tdata->tp_sel.type == PJSIP_TPSELECTOR_TRANSPORT && tp &&
           (pjsip_transport_get_flag_from_type(
                (pjsip_transport_type_e)tp->key.type) &
            PJSIP_TRANSPORT_RELIABLE) != 0;
}

/* Mark the server of the transaction as failed, or clear the mark once it
 * answers. RFC 3261 section 21.5.4 avoids a server that answers 503 only for
 * the time in Retry-After.
 */
static void update_server_state(pjsip_transaction *tsx,
                                const struct tsx_data *tsx_data,
                                pjsip_event *event, pj_bool_t failed)
{
    pjsip_tx_data *tdata = tsx->last_tx;
    pjsip_event_id_e type = event->body.tsx_state.type;
    unsigned idx = tdata->dest_info.cur_addr;
    unsigned max_duration = pjsip_cfg()->endpt.failed_server_timeout;
    unsigned duration = 0;

    /* A request bound to a connection gets the address of its peer as the
     * only entry, see pjsip_endpt_send_request_stateless().
     */
    if (max_duration == 0 || idx >= tdata->dest_info.addr.count)
        return;

    if (failed && type == PJSIP_EVENT_RX_MSG) {
        pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
        pjsip_retry_after_hdr *ra;

        ra = (pjsip_retry_after_hdr*)
             pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_RETRY_AFTER,
                                NULL);
        if (!ra || ra->ivalue <= 0)
            return;
        duration = (unsigned)ra->ivalue < max_duration ?
                   (unsigned)ra->ivalue : max_duration;
    } else if (failed && type == PJSIP_EVENT_TRANSPORT_ERROR) {
        /* May be the local network, see mark_refused_servers() */
        return;
    } else if (failed) {
        duration = max_duration;
    } else if (type != PJSIP_EVENT_RX_MSG) {
        return;
    }

    pjsip_endpt_set_server_failed_gen(tsx->endpt,
                                      &tdata->dest_info.addr.entry[idx].addr,
                                      duration, tsx_data->failed_servers_gen);
}

/* Whether the address at index idx was already tried, see
 * send_to_next_server(). The server at cur_addr has failed whatever the
 * transport type, e.g: the same address is listed with TCP and UDP for a
 * large request (RFC 3261 section 18.1.1). The ones before cur_addr were
 * skipped by the transport, which may be specific to their type.
 */
static pj_bool_t is_tried(const pjsip_tx_data *tdata, unsigned idx)
{
    const pjsip_server_addresses *addr = &tdata->dest_info.addr;
    unsigned cur = tdata->dest_info.cur_addr;
    unsigned i;

    if (cur < addr->count &&
        pj_sockaddr_cmp(&addr->entry[cur].addr, &addr->entry[idx].addr) == 0)
    {
        return PJ_TRUE;
    }
    for (i = 0; i < cur && i < addr->count; ++i) {
        if (addr->entry[i].type == addr->entry[idx].type &&
            pj_sockaddr_cmp(&addr->entry[i].addr, &addr->entry[idx].addr) == 0)
        {
            return PJ_TRUE;
        }
    }
    return PJ_FALSE;
}

/* The transport moved on from the addresses this transaction started from
 * up to cur_addr, as they could not be sent to, e.g: the connection was
 * refused. Mark the ones of the transport type of the address that has
 * answered: then the local network and transport are not the cause.
 */
static void mark_refused_servers(pjsip_transaction *tsx,
                                 const struct tsx_data *tsx_data)
{
    const pjsip_tx_data *tdata = tsx->last_tx;
    const pjsip_server_addresses *addr = &tdata->dest_info.addr;
    unsigned i, duration = pjsip_cfg()->endpt.failed_server_timeout;

    if (tdata->dest_info.cur_addr >= addr->count)
        return;

    for (i = tsx_data->first_addr; duration && i < tdata->dest_info.cur_addr;
         ++i)
    {
        if (addr->entry[i].type != addr->entry[tdata->dest_info.cur_addr].type)
            continue;
        pjsip_endpt_set_server_failed_gen(tsx->endpt,
                                          &tdata->dest_info.addr.entry[i].addr,
                                          duration,
                                          tsx_data->failed_servers_gen);
    }
}

/* Which of the addresses after cur_addr are known to have failed, asked
 * once: a mark may be set or expire at any time.
 */
static void snapshot_failed(pjsip_endpoint *endpt, const pjsip_tx_data *tdata,
                            pj_bool_t failed[PJSIP_MAX_RESOLVED_ADDRESSES])
{
    const pjsip_server_addresses *addr = &tdata->dest_info.addr;
    unsigned i;

    for (i = tdata->dest_info.cur_addr + 1; i < addr->count; ++i)
        failed[i] = pjsip_endpt_is_server_failed(endpt, &addr->entry[i].addr);
}

/* Find the next address to try, skipping the ones already tried, and the
 * ones known to have failed while there are others. Return the address
 * count if there is none.
 */
static unsigned find_next_server(const pjsip_tx_data *tdata,
                                 const pj_bool_t *failed)
{
    const pjsip_server_addresses *addr = &tdata->dest_info.addr;
    unsigned i, failed_next = addr->count;

    for (i = tdata->dest_info.cur_addr + 1; i < addr->count; ++i) {
        if (is_tried(tdata, i))
            continue;
        if (!failed[i])
            return i;
        if (failed_next == addr->count)
            failed_next = i;
    }
    return failed_next;
}

/* Send a copy, as the request may still be queued in a connecting transport.
 * The original is kept until the callback: the token may be in its pool.
 */
static pj_status_t send_to_next_server(pjsip_transaction *tsx,
                                       const struct tsx_data *tsx_data,
                                       unsigned next, const pj_bool_t *failed)
{
    pjsip_tx_data *old_tdata = tsx->last_tx;
    pjsip_tx_data *tdata;
    pjsip_transaction *new_tsx;
    struct tsx_data *new_data;
    const pjsip_server_addresses *old_addr;
    pjsip_server_addresses *addr;
    pjsip_via_hdr *via;
    unsigned i, pass;
    pj_status_t status;

    /* Sending applies the strict route again */
    pjsip_restore_strict_route_set(old_tdata);

    status = pjsip_tx_data_clone(old_tdata, 0, &tdata);
    if (status != PJ_SUCCESS)
        return status;

    /* The copy only gets the addresses not tried yet, starting with the
     * next one and with the failed ones last, as the transport moves on to
     * the following addresses by itself, and they may be sorted again by IP
     * version.
     */
    tdata->dest_info = old_tdata->dest_info;
    pj_strdup(tdata->pool, &tdata->dest_info.name, &old_tdata->dest_info.name);
    old_addr = &old_tdata->dest_info.addr;
    addr = &tdata->dest_info.addr;
    addr->entry[0] = old_addr->entry[next];
    pj_strdup(tdata->pool, &addr->entry[0].name, &old_addr->entry[next].name);
    addr->count = 1;
    for (pass = 0; pass < 2; ++pass) {
        for (i = old_tdata->dest_info.cur_addr + 1; i < old_addr->count; ++i)
        {
            if (i == next || is_tried(old_tdata, i) || failed[i] != (pass == 1))
                continue;
            addr->entry[addr->count] = old_addr->entry[i];
            pj_strdup(tdata->pool, &addr->entry[addr->count].name,
                      &old_addr->entry[i].name);
            ++addr->count;
        }
    }
    tdata->dest_info.cur_addr = 0;
    pjsip_tx_data_set_transport(tdata, &old_tdata->tp_sel);
    pj_strdup(tdata->pool, &tdata->via_addr.host, &tsx_data->via_addr.host);
    tdata->via_addr.port = tsx_data->via_addr.port;
    tdata->via_tp = tsx_data->via_tp;

    /* A new transaction needs a new branch */
    via = (pjsip_via_hdr*) pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
    if (via)
        via->branch_param.slen = 0;

    status = pjsip_tsx_create_uac(&mod_stateful_util, tdata, &new_tsx);
    if (status != PJ_SUCCESS) {
        pjsip_tx_data_dec_ref(tdata);
        return status;
    }

    {
        char buf[PJ_INET6_ADDRSTRLEN + 10];

        PJ_LOG(4,(THIS_FILE, "%s failed with %d, trying %s",
                  pjsip_tx_data_get_info(old_tdata), tsx->status_code,
                  pj_sockaddr_print(&addr->entry[0].addr, buf, sizeof(buf),
                                    3)));
    }

    pjsip_tsx_set_transport(new_tsx, &tdata->tp_sel);

    /* The copy keeps the generation of the original: it is sent on the
     * same network.
     */
    new_data = PJ_POOL_ALLOC_T(new_tsx->pool, struct tsx_data);
    *new_data = *tsx_data;
    new_data->first_addr = 0;
    if (!new_data->orig_tdata) {
        new_data->orig_tdata = old_tdata;
        pjsip_tx_data_add_ref(old_tdata);
    }
    new_tsx->mod_data[mod_stateful_util.id] = new_data;
    if (new_data->state && req_mutex) {
        pj_mutex_lock(req_mutex);
        tdata->mod_data[mod_stateful_util.id] = new_data->state;
        new_data->state->cur = tdata;
        pj_mutex_unlock(req_mutex);
    }

    pj_grp_lock_add_ref(new_tsx->grp_lock);
    status = pjsip_tsx_send_msg(new_tsx, NULL);
    if (status != PJ_SUCCESS) {
        pjsip_tx_data_dec_ref(tdata);
        pjsip_tsx_terminate(new_tsx, new_tsx->status_code ?
                            new_tsx->status_code :
                            PJSIP_SC_SERVICE_UNAVAILABLE);
    }
    pj_grp_lock_dec_ref(new_tsx->grp_lock);

    return PJ_SUCCESS;
}

static void mod_util_on_tsx_state(pjsip_transaction *tsx, pjsip_event *event)
{
    struct tsx_data *tsx_data;

    /* Check if the module has been unregistered (see ticket #1535) and also
     * verify the event type.
     */
    if (mod_stateful_util.id < 0 || event->type != PJSIP_EVENT_TSX_STATE)
        return;

    tsx_data = (struct tsx_data*) tsx->mod_data[mod_stateful_util.id];
    if (tsx_data == NULL)
        return;

    if (tsx->status_code < 200)
        return;

    /* Call the callback, if any, and prevent the callback to be called again
     * by clearing the transaction's module_data.
     */
    tsx->mod_data[mod_stateful_util.id] = NULL;
    end_attempt(tsx_data->state, tsx->last_tx, tsx_data->orig_tdata != NULL);

    /* A request sent before the failed servers were cleared, e.g: on the
     * previous network, says nothing about the servers.
     */
    if (pjsip_cfg()->endpt.server_failover &&
        tsx->role == PJSIP_ROLE_UAC && tsx->last_tx &&
        tsx->method.id != PJSIP_INVITE_METHOD &&
        tsx->method.id != PJSIP_CANCEL_METHOD &&
        tsx_data->failed_servers_gen ==
            pjsip_endpt_failed_servers_gen(tsx->endpt))
    {
        pjsip_tx_data *tdata = tsx->last_tx;
        pj_bool_t failed = is_server_failure(tsx, event);
        pj_bool_t failed_addr[PJSIP_MAX_RESOLVED_ADDRESSES];
        unsigned next;

        update_server_state(tsx, tsx_data, event, failed);
        if (event->body.tsx_state.type == PJSIP_EVENT_RX_MSG)
            mark_refused_servers(tsx, tsx_data);

        if (failed && tsx_data->allow_failover && !is_pinned(tdata)) {
            snapshot_failed(tsx->endpt, tdata, failed_addr);
            next = find_next_server(tdata, failed_addr);
            if (next < tdata->dest_info.addr.count &&
                send_to_next_server(tsx, tsx_data, next, failed_addr) ==
                    PJ_SUCCESS)
            {
                return;
            }
        }
    }

    if (tsx_data->cb) {
        (*tsx_data->cb)(tsx_data->token, event);
    }

    /* Release the transport, unless the callback has sent the request
     * again, e.g: with credentials: then it is the one of that attempt.
     */
    if (tsx_data->state && req_mutex) {
        pjsip_transport *tp = NULL;

        pj_mutex_lock(req_mutex);
        if (!tsx_data->state->cur) {
            tp = tsx_data->state->tp;
            tsx_data->state->tp = NULL;
        }
        pj_mutex_unlock(req_mutex);
        if (tp)
            pjsip_transport_dec_ref(tp);
    }
    if (tsx_data->orig_tdata)
        pjsip_tx_data_dec_ref(tsx_data->orig_tdata);
}


PJ_DEF(pj_status_t) pjsip_endpt_send_request(  pjsip_endpoint *endpt,
                                               pjsip_tx_data *tdata,
                                               pj_int32_t timeout,
                                               void *token,
                                               pjsip_endpt_send_callback cb)
{
    return pjsip_endpt_send_request2(endpt, tdata, timeout, token, cb, NULL);
}


PJ_DEF(pj_status_t) pjsip_endpt_send_request2( pjsip_endpoint *endpt,
                                               pjsip_tx_data *tdata,
                                               pj_int32_t timeout,
                                               void *token,
                                               pjsip_endpt_send_callback cb,
                                               pjsip_transaction **p_tsx)
{
    pjsip_transaction *tsx;
    struct tsx_data *tsx_data;
    pj_status_t status;

    /* Reset the output first, so it is also reset when the checks below
     * fail.
     */
    if (p_tsx) *p_tsx = NULL;

    PJ_ASSERT_RETURN(endpt && tdata && (timeout==-1 || timeout>0), PJ_EINVAL);

    /* Check that transaction layer module is registered to endpoint */
    PJ_ASSERT_RETURN(mod_stateful_util.id != -1, PJ_EINVALIDOP);

    PJ_UNUSED_ARG(timeout);

    status = pjsip_tsx_create_uac(&mod_stateful_util, tdata, &tsx);
    if (status != PJ_SUCCESS) {
        pjsip_tx_data_dec_ref(tdata);
        return status;
    }

    pjsip_tsx_set_transport(tsx, &tdata->tp_sel);

    tsx_data = PJ_POOL_ZALLOC_T(tsx->pool, struct tsx_data);
    tsx_data->token = token;
    tsx_data->cb = cb;
    if (req_mutex && tdata->mod_data[mod_stateful_util.id]) {
        /* See pjsip_endpt_follow_request_transport() */
        pj_mutex_lock(req_mutex);
        tsx_data->state = (struct req_state*)
                          tdata->mod_data[mod_stateful_util.id];
        tsx_data->state->cur = tdata;
        pj_mutex_unlock(req_mutex);
    }
    /* The caller can't follow a replaced transaction */
    tsx_data->allow_failover = (p_tsx == NULL);
    tsx_data->first_addr = tdata->dest_info.cur_addr;
    if (pjsip_cfg()->endpt.server_failover) {
        tsx_data->failed_servers_gen = pjsip_endpt_failed_servers_gen(endpt);
        if (tsx_data->allow_failover) {
            pj_strdup(tdata->pool, &tsx_data->via_addr.host,
                      &tdata->via_addr.host);
            tsx_data->via_addr.port = tdata->via_addr.port;
            tsx_data->via_tp = tdata->via_tp;
        }
    } else {
        /* Never counted, even if the option is enabled later */
        tsx_data->failed_servers_gen = (unsigned)-1;
    }

    tsx->mod_data[mod_stateful_util.id] = tsx_data;

    /* Prevent the transaction from being deleted before we have a chance
     * to terminate it if sending fails.
     */
    pj_grp_lock_add_ref(tsx->grp_lock);

    status = pjsip_tsx_send_msg(tsx, NULL);
    if (status == PJ_SUCCESS) {
        /* Only hand over the transaction after a successful send, as the
         * send may fail after the callback has been called. Our reference
         * above keeps the transaction alive here, even when it has already
         * been completed by the callback.
         */
        if (p_tsx) {
            pj_grp_lock_add_ref(tsx->grp_lock);
            *p_tsx = tsx;
        }
    } else {
        pjsip_tx_data_dec_ref(tdata);
        pjsip_tsx_terminate(tsx, tsx->status_code? tsx->status_code:
                            PJSIP_SC_SERVICE_UNAVAILABLE);
    }

    pj_grp_lock_dec_ref(tsx->grp_lock);

    return status;
}


/*
 * Send response statefully.
 */
PJ_DEF(pj_status_t) pjsip_endpt_respond(  pjsip_endpoint *endpt,
                                          pjsip_module *tsx_user,
                                          pjsip_rx_data *rdata,
                                          int st_code,
                                          const pj_str_t *st_text,
                                          const pjsip_hdr *hdr_list,
                                          const pjsip_msg_body *body,
                                          pjsip_transaction **p_tsx )
{
    pj_status_t status;
    pjsip_tx_data *tdata;
    pjsip_transaction *tsx;

    /* Validate arguments. */
    PJ_ASSERT_RETURN(endpt && rdata, PJ_EINVAL);

    if (p_tsx) *p_tsx = NULL;

    /* Create response message */
    status = pjsip_endpt_create_response( endpt, rdata, st_code, st_text, 
                                          &tdata);
    if (status != PJ_SUCCESS)
        return status;

    /* Add the message headers, if any */
    if (hdr_list) {
        const pjsip_hdr *hdr = hdr_list->next;
        while (hdr != hdr_list) {
            pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)
                              pjsip_hdr_clone(tdata->pool, hdr) );
            hdr = hdr->next;
        }
    }

    /* Add the message body, if any. */
    if (body) {
        tdata->msg->body = pjsip_msg_body_clone( tdata->pool, body );
        if (tdata->msg->body == NULL) {
            pjsip_tx_data_dec_ref(tdata);
            return status;
        }
    }

    /* Create UAS transaction. */
    status = pjsip_tsx_create_uas(tsx_user, rdata, &tsx);
    if (status != PJ_SUCCESS) {
        pjsip_tx_data_dec_ref(tdata);
        return status;
    }

    /* Prevent the transaction from being deleted before we have a chance
     * to terminate it if sending fails.
     */
    pj_grp_lock_add_ref(tsx->grp_lock);

    /* Feed the request to the transaction. */
    pjsip_tsx_recv_msg(tsx, rdata);

    /* Send the message. */
    status = pjsip_tsx_send_msg(tsx, tdata);
    if (status != PJ_SUCCESS) {
        pjsip_tx_data_dec_ref(tdata);
        pjsip_tsx_terminate(tsx, tsx->status_code? tsx->status_code:
                            PJSIP_SC_INTERNAL_SERVER_ERROR);
    } else if (p_tsx) {
        *p_tsx = tsx;
    }

    pj_grp_lock_dec_ref(tsx->grp_lock);

    return status;
}


