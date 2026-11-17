/* 
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
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
#include <pjsip-simple/publish.h>
#include <pjsip/sip_auth.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_transaction.h>
#include <pjsip/sip_uri.h>
#include <pjsip/sip_util.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/timer.h>


#define REFRESH_TIMER           1
#define DELAY_BEFORE_REFRESH    PJSIP_PUBLISHC_DELAY_BEFORE_REFRESH

/* This defines the minimum value for the refresh timer in milliseconds.
 * If the calculated expiry for the refresh timer falls below this; it will be clamped to this value.
 */
#define MIN_REFRESH_MSEC        500
#define THIS_FILE               "publishc.c"


/* Let's define this enum, so that it'll trigger compilation error
 * when somebody define the same enum in sip_msg.h
 */
enum
{
    PJSIP_PUBLISH_METHOD = PJSIP_OTHER_METHOD,
};

const pjsip_method pjsip_publish_method = 
{
    (pjsip_method_e)PJSIP_PUBLISH_METHOD,
    { "PUBLISH", 7 }
};


/**
 * Pending request list.
 */
typedef struct pending_publish
{
    PJ_DECL_LIST_MEMBER(struct pending_publish);
    pjsip_tx_data               *tdata;
} pending_publish;


/**
 * SIP client publication structure.
 */
struct pjsip_publishc
{
    pj_pool_t                   *pool;
    pjsip_endpoint              *endpt;
    pj_bool_t                    _delete_flag;
    int                          pending_tsx;
    /* Outstanding async auth tokens. Kept separate from pending_tsx,
     * which also gates request sending in pjsip_publishc_send().
     */
    int                          pending_auth_tokens;
    pj_bool_t                    in_callback;
    /* Response callbacks running: requests sent meanwhile are queued, to
     * be sent in order with the updated ETag afterwards. A count, since a
     * refresh may complete on another thread while a callback runs.
     */
    int                          in_resp_cb;
    pj_mutex_t                  *mutex;

    pjsip_publishc_opt           opt;
    void                        *token;
    pjsip_publishc_cb           *cb;

    pj_str_t                     event;
    pj_str_t                     str_target_uri;
    pjsip_uri                   *target_uri;
    pjsip_cid_hdr               *cid_hdr;
    pjsip_cseq_hdr              *cseq_hdr;
    pj_str_t                     from_uri;
    pjsip_from_hdr              *from_hdr;
    pjsip_to_hdr                *to_hdr;
    pj_str_t                     etag;
    pjsip_expires_hdr           *expires_hdr;
    pj_uint32_t                  expires;
    pjsip_route_hdr              route_set;
    pjsip_hdr                    usr_hdr;
    pjsip_host_port              via_addr;
    const void                  *via_tp;

    /* Authorization sessions. */
    pjsip_auth_clt_sess          auth_sess;

    /* Auto refresh publication. */
    pj_bool_t                    auto_refresh;
    pj_time_val                  last_refresh;
    pj_time_val                  next_refresh;
    pj_timer_entry               timer;
    /* Refresh timer callbacks already dispatched by the timer heap when
     * the timer was cancelled; they still run and must skip the refresh.
     */
    int                          timer_cancelled;

    /* Pending PUBLISH request */
    pending_publish              pending_reqs;
    pending_publish              pending_reqs_empty;
};


static pj_status_t pubc_async_auth_send_impl(
                                pjsip_auth_clt_sess *auth_sess,
                                void *user_data,
                                pjsip_tx_data *tdata);
static void pubc_async_auth_abandon_impl(
                                pjsip_auth_clt_sess *auth_sess,
                                void *user_data);
static pj_status_t pubc_send(pjsip_publishc *pubc, pjsip_tx_data *tdata);
static void pubc_send_queued(pjsip_publishc *pubc);

PJ_DEF(void) pjsip_publishc_opt_default(pjsip_publishc_opt *opt)
{
    pj_bzero(opt, sizeof(*opt));
    opt->queue_request = PJSIP_PUBLISHC_QUEUE_REQUEST;
}


/*
 * Initialize client publication module.
 */
PJ_DEF(pj_status_t) pjsip_publishc_init_module(pjsip_endpoint *endpt)
{
    /* Note:
        Commented out the capability registration below, since it's
        wrong to include PUBLISH in Allow header of INVITE requests/
        responses.

        13.2.1 Creating the Initial INVITE
          An Allow header field (Section 20.5) SHOULD be present in the 
          INVITE. It indicates what methods can be invoked within a dialog

        20.5 Allow
          The Allow header field lists the set of methods supported by the
          UA generating the message.

        While the semantic of Allow header in non-dialog requests is unclear,
        it's probably best not to include PUBLISH in Allow header for now
        until we can find out how to customize the inclusion of methods in
        Allow header for in-dialog vs out-dialog requests.

    return pjsip_endpt_add_capability( endpt, NULL, PJSIP_H_ALLOW, NULL,
                                       1, &pjsip_publish_method.name);
     */
    PJ_UNUSED_ARG(endpt);
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_publishc_create( pjsip_endpoint *endpt, 
                                           const pjsip_publishc_opt *opt,
                                           void *token,
                                           pjsip_publishc_cb *cb,       
                                           pjsip_publishc **p_pubc)
{
    pj_pool_t *pool;
    pjsip_publishc *pubc;
    pjsip_publishc_opt default_opt;
    pj_status_t status;

    /* Verify arguments. */
    PJ_ASSERT_RETURN(endpt && cb && p_pubc, PJ_EINVAL);

    pool = pjsip_endpt_create_pool(endpt, "pubc%p", 1024, 1024);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    pubc = PJ_POOL_ZALLOC_T(pool, pjsip_publishc);

    pubc->pool = pool;
    pubc->endpt = endpt;
    pubc->token = token;
    pubc->cb = cb;
    pubc->expires = PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED;

    if (!opt) {
        pjsip_publishc_opt_default(&default_opt);
        opt = &default_opt;
    }
    pj_memcpy(&pubc->opt, opt, sizeof(*opt));
    pj_list_init(&pubc->pending_reqs);
    pj_list_init(&pubc->pending_reqs_empty);

    status = pj_mutex_create_recursive(pubc->pool, "pubc%p", &pubc->mutex);
    if (status != PJ_SUCCESS) {
        pj_pool_release(pool);
        return status;
    }

    status = pjsip_auth_clt_init(&pubc->auth_sess, endpt, pubc->pool, 0);
    if (status != PJ_SUCCESS) {
        pj_mutex_destroy(pubc->mutex);
        pj_pool_release(pool);
        return status;
    }

    pj_list_init(&pubc->route_set);
    pj_list_init(&pubc->usr_hdr);

    /* Done */
    *p_pubc = pubc;
    return PJ_SUCCESS;
}


/* Cancel the refresh timer. Called with the mutex held. If the timer heap
 * has already dispatched the callback, it will still run: count it as a
 * running callback so that pubc is not freed under it, and mark it to be
 * skipped.
 */
static void pubc_cancel_timer(pjsip_publishc *pubc)
{
    if (pubc->timer.id == 0)
        return;

    if (pj_timer_heap_cancel(pjsip_endpt_get_timer_heap(pubc->endpt),
                             &pubc->timer) == 0)
    {
        ++pubc->timer_cancelled;
        ++pubc->in_callback;
    }
    pubc->timer.id = 0;
}

/* Release queued requests. Called with the mutex held. */
static void pubc_flush_queue(pjsip_publishc *pubc)
{
    while (!pj_list_empty(&pubc->pending_reqs)) {
        pending_publish *pp = pubc->pending_reqs.next;

        pj_list_erase(pp);
        pj_list_push_back(&pubc->pending_reqs_empty, pp);
        pjsip_tx_data_dec_ref(pp->tdata);
    }
}

/* Free pubc. Called without the mutex held, once nothing is outstanding. */
static void pubc_free(pjsip_publishc *pubc)
{
    if (pubc->mutex)
        pj_mutex_destroy(pubc->mutex);

    pjsip_auth_clt_deinit(&pubc->auth_sess);
    pjsip_endpt_release_pool(pubc->endpt, pubc->pool);
}

/* Release the mutex, then complete a deferred destroy if the last
 * outstanding transaction, callback or auth token has just finished.
 */
static void pubc_unlock(pjsip_publishc *pubc)
{
    pj_bool_t can_free = pubc->_delete_flag && !pubc->pending_tsx &&
                         !pubc->in_callback && !pubc->pending_auth_tokens;

    pj_mutex_unlock(pubc->mutex);
    if (can_free)
        pubc_free(pubc);
}

PJ_DEF(pj_status_t) pjsip_publishc_destroy(pjsip_publishc *pubc)
{
    PJ_ASSERT_RETURN(pubc, PJ_EINVAL);

    pj_mutex_lock(pubc->mutex);

    pubc_cancel_timer(pubc);
    pubc_flush_queue(pubc);

    if (pubc->pending_tsx || pubc->in_callback || pubc->pending_auth_tokens) {
        pubc->_delete_flag = 1;
        pubc->cb = NULL;
        pj_mutex_unlock(pubc->mutex);
    } else {
        pj_mutex_unlock(pubc->mutex);
        pubc_free(pubc);
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_pool_t*) pjsip_publishc_get_pool(pjsip_publishc *pubc)
{
    return pubc->pool;
}

static void set_expires( pjsip_publishc *pubc, pj_uint32_t expires)
{
    if (expires != pubc->expires && 
        expires != PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED) 
    {
        pubc->expires_hdr = pjsip_expires_hdr_create(pubc->pool, expires);
    } else {
        pubc->expires_hdr = NULL;
    }
}


PJ_DEF(pj_status_t) pjsip_publishc_init(pjsip_publishc *pubc,
                                        const pj_str_t *event,
                                        const pj_str_t *target_uri,
                                        const pj_str_t *from_uri,
                                        const pj_str_t *to_uri,
                                        pj_uint32_t expires)
{
    pj_str_t tmp;

    PJ_ASSERT_RETURN(pubc && event && target_uri && from_uri && to_uri && 
                     expires, PJ_EINVAL);

    /* Copy event type */
    pj_strdup_with_null(pubc->pool, &pubc->event, event);

    /* Copy server URL. */
    pj_strdup_with_null(pubc->pool, &pubc->str_target_uri, target_uri);

    /* Set server URL. */
    tmp = pubc->str_target_uri;
    pubc->target_uri = pjsip_parse_uri( pubc->pool, tmp.ptr, tmp.slen, 0);
    if (pubc->target_uri == NULL) {
        return PJSIP_EINVALIDURI;
    }

    /* Set "From" header. */
    pj_strdup_with_null(pubc->pool, &pubc->from_uri, from_uri);
    tmp = pubc->from_uri;
    pubc->from_hdr = pjsip_from_hdr_create(pubc->pool);
    pubc->from_hdr->uri = pjsip_parse_uri(pubc->pool, tmp.ptr, tmp.slen, 
                                          PJSIP_PARSE_URI_AS_NAMEADDR);
    if (!pubc->from_hdr->uri) {
        return PJSIP_EINVALIDURI;
    }

    /* Set "To" header. */
    pj_strdup_with_null(pubc->pool, &tmp, to_uri);
    pubc->to_hdr = pjsip_to_hdr_create(pubc->pool);
    pubc->to_hdr->uri = pjsip_parse_uri(pubc->pool, tmp.ptr, tmp.slen, 
                                        PJSIP_PARSE_URI_AS_NAMEADDR);
    if (!pubc->to_hdr->uri) {
        return PJSIP_EINVALIDURI;
    }


    /* Set "Expires" header, if required. */
    set_expires( pubc, expires);

    /* Set "Call-ID" header. */
    pubc->cid_hdr = pjsip_cid_hdr_create(pubc->pool);
    pj_create_unique_string(pubc->pool, &pubc->cid_hdr->id);

    /* Set "CSeq" header. */
    pubc->cseq_hdr = pjsip_cseq_hdr_create(pubc->pool);
    pubc->cseq_hdr->cseq = pj_rand() % 0xFFFF;
    pjsip_method_set( &pubc->cseq_hdr->method, PJSIP_REGISTER_METHOD);

    /* Done. */
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_publishc_set_credentials( pjsip_publishc *pubc,
                                                int count,
                                                const pjsip_cred_info cred[] )
{
    pj_status_t status;

    PJ_ASSERT_RETURN(pubc && count && cred, PJ_EINVAL);

    pj_mutex_lock(pubc->mutex);
    status = pjsip_auth_clt_set_credentials(&pubc->auth_sess, count, cred);
    pj_mutex_unlock(pubc->mutex);

    return status;
}

PJ_DEF(pj_status_t) pjsip_publishc_set_auth_sess(
                                    pjsip_publishc *pubc,
                                    pjsip_auth_clt_sess *session)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(pubc, PJ_EINVAL);

    pj_mutex_lock(pubc->mutex);
    status = pjsip_auth_clt_set_parent(&pubc->auth_sess, session);
    pj_mutex_unlock(pubc->mutex);

    return status;
}

PJ_DEF(pjsip_auth_clt_sess*) pjsip_publishc_get_auth_sess(
                                    pjsip_publishc *pubc)
{
    PJ_ASSERT_RETURN(pubc, NULL);
    return &pubc->auth_sess;
}

PJ_DEF(pj_status_t) pjsip_publishc_set_route_set( pjsip_publishc *pubc,
                                              const pjsip_route_hdr *route_set)
{
    const pjsip_route_hdr *chdr;

    PJ_ASSERT_RETURN(pubc && route_set, PJ_EINVAL);

    pj_mutex_lock(pubc->mutex);

    pj_list_init(&pubc->route_set);

    chdr = route_set->next;
    while (chdr != route_set) {
        pj_list_push_back(&pubc->route_set, pjsip_hdr_clone(pubc->pool, chdr));
        chdr = chdr->next;
    }

    pj_mutex_unlock(pubc->mutex);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_publishc_set_headers( pjsip_publishc *pubc,
                                                const pjsip_hdr *hdr_list)
{
    const pjsip_hdr *h;

    PJ_ASSERT_RETURN(pubc && hdr_list, PJ_EINVAL);

    pj_mutex_lock(pubc->mutex);

    pj_list_init(&pubc->usr_hdr);
    h = hdr_list->next;
    while (h != hdr_list) {
        pj_list_push_back(&pubc->usr_hdr, pjsip_hdr_clone(pubc->pool, h));
        h = h->next;
    }

    pj_mutex_unlock(pubc->mutex);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_publishc_set_via_sent_by(pjsip_publishc *pubc,
                                                   pjsip_host_port *via_addr,
                                                   pjsip_transport *via_tp)
{
    PJ_ASSERT_RETURN(pubc, PJ_EINVAL);

    pj_mutex_lock(pubc->mutex);

    if (!via_addr)
        pj_bzero(&pubc->via_addr, sizeof(pubc->via_addr));
    else {
        if (pj_strcmp(&pubc->via_addr.host, &via_addr->host))
            pj_strdup(pubc->pool, &pubc->via_addr.host, &via_addr->host);
        pubc->via_addr.port = via_addr->port;
    }
    pubc->via_tp = via_tp;

    pj_mutex_unlock(pubc->mutex);

    return PJ_SUCCESS;
}

static pj_status_t create_request(pjsip_publishc *pubc, 
                                  pjsip_tx_data **p_tdata)
{
    const pj_str_t STR_EVENT = { "Event", 5 };
    pj_status_t status;
    pjsip_generic_string_hdr *hdr;
    pjsip_tx_data *tdata;

    PJ_ASSERT_RETURN(pubc && p_tdata, PJ_EINVAL);

    /* Create the request. */
    status = pjsip_endpt_create_request_from_hdr( pubc->endpt, 
                                                  &pjsip_publish_method,
                                                  pubc->target_uri,
                                                  pubc->from_hdr,
                                                  pubc->to_hdr,
                                                  NULL,
                                                  pubc->cid_hdr,
                                                  pubc->cseq_hdr->cseq,
                                                  NULL,
                                                  &tdata);
    if (status != PJ_SUCCESS)
        return status;

    /* Add cached authorization headers. */
    pjsip_auth_clt_init_req( &pubc->auth_sess, tdata );

    /* Add Route headers from route set, ideally after Via header */
    if (!pj_list_empty(&pubc->route_set)) {
        pjsip_hdr *route_pos;
        const pjsip_route_hdr *route;

        route_pos = (pjsip_hdr*)
                    pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
        if (!route_pos)
            route_pos = &tdata->msg->hdr;

        route = pubc->route_set.next;
        while (route != &pubc->route_set) {
            pjsip_hdr *new_hdr = (pjsip_hdr*)
                                 pjsip_hdr_shallow_clone(tdata->pool, route);
            pj_list_insert_after(route_pos, new_hdr);
            route_pos = new_hdr;
            route = route->next;
        }
    }

    /* Add Event header */
    hdr = pjsip_generic_string_hdr_create(tdata->pool, &STR_EVENT,
                                          &pubc->event);
    if (hdr)
        pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);


    /* Add SIP-If-Match if we have etag */
    if (pubc->etag.slen) {
        const pj_str_t STR_HNAME = { "SIP-If-Match", 12 };

        hdr = pjsip_generic_string_hdr_create(tdata->pool, &STR_HNAME,
                                              &pubc->etag);
        if (hdr)
            pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)hdr);
    }

    /* Add user headers */
    if (!pj_list_empty(&pubc->usr_hdr)) {
        const pjsip_hdr *uhdr;

        uhdr = pubc->usr_hdr.next;
        while (uhdr != &pubc->usr_hdr) {
            pjsip_hdr *new_hdr = (pjsip_hdr*)
                                 pjsip_hdr_shallow_clone(tdata->pool, uhdr);
            pjsip_msg_add_hdr(tdata->msg, new_hdr);
            uhdr = uhdr->next;
        }
    }


    /* Done. */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}


/* Create a PUBLISH request with the current Expires. Called with the
 * mutex held.
 */
static pj_status_t pubc_create_publish(pjsip_publishc *pubc,
                                       pjsip_tx_data **p_tdata)
{
    pj_status_t status;
    pjsip_tx_data *tdata;

    status = create_request(pubc, &tdata);
    if (status != PJ_SUCCESS)
        return status;

    /* Add Expires header */
    if (pubc->expires_hdr) {
        pjsip_hdr *dup;

        dup = (pjsip_hdr*)
              pjsip_hdr_shallow_clone(tdata->pool, pubc->expires_hdr);
        if (dup)
            pjsip_msg_add_hdr(tdata->msg, dup);
    }

    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_publishc_publish(pjsip_publishc *pubc, 
                                           pj_bool_t auto_refresh,
                                           pjsip_tx_data **p_tdata)
{
    pj_status_t status;
    pjsip_tx_data *tdata;

    PJ_ASSERT_RETURN(pubc && p_tdata, PJ_EINVAL);

    pj_mutex_lock(pubc->mutex);

    status = pubc_create_publish(pubc, &tdata);
    if (status != PJ_SUCCESS) {
        pj_mutex_unlock(pubc->mutex);
        return status;
    }

    pubc_cancel_timer(pubc);

    pubc->auto_refresh = auto_refresh;

    pj_mutex_unlock(pubc->mutex);

    /* Done */
    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_publishc_unpublish(pjsip_publishc *pubc,
                                             pjsip_tx_data **p_tdata)
{
    pjsip_tx_data *tdata;
    pjsip_msg *msg;
    pjsip_expires_hdr *expires;
    pj_status_t status;

    PJ_ASSERT_RETURN(pubc && p_tdata, PJ_EINVAL);

    pj_mutex_lock(pubc->mutex);

    pubc_cancel_timer(pubc);

    status = create_request(pubc, &tdata);

    pj_mutex_unlock(pubc->mutex);

    if (status != PJ_SUCCESS)
        return status;

    msg = tdata->msg;

    /* Add Expires:0 header */
    expires = pjsip_expires_hdr_create(tdata->pool, 0);
    pjsip_msg_add_hdr( msg, (pjsip_hdr*)expires);

    *p_tdata = tdata;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_publishc_update_expires( pjsip_publishc *pubc,
                                                   pj_uint32_t expires )
{
    PJ_ASSERT_RETURN(pubc, PJ_EINVAL);

    pj_mutex_lock(pubc->mutex);
    set_expires( pubc, expires );
    pj_mutex_unlock(pubc->mutex);

    return PJ_SUCCESS;
}


/* Snapshot the callback under the mutex. It is cleared when the app
 * destroys the pubc while a request or an async auth token is still
 * outstanding.
 */
static pjsip_publishc_cb *pubc_get_cb(pjsip_publishc *pubc)
{
    pjsip_publishc_cb *cb;

    pj_mutex_lock(pubc->mutex);
    cb = pubc->cb;
    pj_mutex_unlock(pubc->mutex);

    return cb;
}

/* Invoke cb, a snapshot of pubc->cb taken under the mutex, without the
 * mutex held. The caller keeps pubc alive across the call.
 */
static void call_callback(pjsip_publishc *pubc, pjsip_publishc_cb *cb,
                          pj_status_t status, int st_code,
                          const pj_str_t *reason, pjsip_rx_data *rdata,
                          pj_uint32_t expiration)
{
    struct pjsip_publishc_cbparam cbparam;

    if (!cb)
        return;

    cbparam.pubc = pubc;
    cbparam.token = pubc->token;
    cbparam.status = status;
    cbparam.code = st_code;
    cbparam.reason = *reason;
    cbparam.rdata = rdata;
    cbparam.expiration = expiration;

    (*cb)(&cbparam);
}

static void pubc_refresh_timer_cb( pj_timer_heap_t *timer_heap,
                                   struct pj_timer_entry *entry)
{
    pjsip_publishc *pubc = (pjsip_publishc*) entry->user_data;
    pjsip_tx_data *tdata;
    pj_status_t status;
    
    PJ_UNUSED_ARG(timer_heap);

    pj_mutex_lock(pubc->mutex);

    /* Cancelled after the timer heap dispatched us; pubc_cancel_timer()
     * already counted this callback.
     */
    if (pubc->timer_cancelled) {
        --pubc->timer_cancelled;
        --pubc->in_callback;
        pubc_unlock(pubc);
        return;
    }

    entry->id = 0;
    ++pubc->in_callback;

    /* Build and claim the refresh without releasing the mutex, so that a
     * concurrent publish(), unpublish() or destroy() either cancels this
     * timer first or runs after the refresh is in flight. If the app's own
     * request is in flight, its 2xx re-arms the timer instead.
     */
    if (pubc->pending_tsx == 0) {
        status = pubc_create_publish(pubc, &tdata);
        if (status != PJ_SUCCESS) {
            char errmsg[PJ_ERR_MSG_SIZE];
            pj_str_t reason = pj_strerror(status, errmsg, sizeof(errmsg));
            pjsip_publishc_cb *cb = pubc->cb;

            pj_mutex_unlock(pubc->mutex);
            call_callback(pubc, cb, status, 400, &reason, NULL,
                          PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED);
            pj_mutex_lock(pubc->mutex);
        } else {
            pubc_send(pubc, tdata);
        }
    }

    /* A request queued while the mutex was released for the send stays
     * queued if that send created no transaction, so send it now.
     */
    pubc_send_queued(pubc);

    --pubc->in_callback;
    pubc_unlock(pubc);
}

/* Release the hold an async auth token took on pubc at creation,
 * completing a destroy that was deferred while the token was outstanding.
 */
static void pubc_auth_token_release(pjsip_publishc *pubc)
{
    pj_mutex_lock(pubc->mutex);
    pj_assert(pubc->pending_auth_tokens > 0);
    --pubc->pending_auth_tokens;
    pubc_unlock(pubc);
}

/* Async auth send callback: send the authenticated PUBLISH request.
 * user_data points to pjsip_publishc*. No auth lock is held: the auth
 * client releases it before send_impl.
 */
static pj_status_t pubc_async_auth_send_impl(
                                pjsip_auth_clt_sess *auth_sess,
                                void *user_data,
                                pjsip_tx_data *tdata)
{
    pjsip_publishc *pubc = (pjsip_publishc *)user_data;
    pj_status_t status;

    PJ_UNUSED_ARG(auth_sess);

    status = pjsip_publishc_send(pubc, tdata);

    pubc_auth_token_release(pubc);

    return status;
}

/* Async auth abandon callback: notify the app that PUBLISH auth failed.
 * user_data points to pjsip_publishc*.  No lock is held by the caller.
 */
static void pubc_async_auth_abandon_impl(pjsip_auth_clt_sess *auth_sess,
                                         void *user_data)
{
    pjsip_publishc *pubc = (pjsip_publishc *)user_data;
    pj_str_t reason = { "Authentication abandoned", 24 };

    PJ_UNUSED_ARG(auth_sess);

    /* The hold the token took at creation keeps pubc alive across the
     * callback, which may destroy it.
     */
    call_callback(pubc, pubc_get_cb(pubc), PJ_ECANCELLED,
                  PJSIP_SC_UNAUTHORIZED, &reason, NULL,
                  PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED);

    pubc_auth_token_release(pubc);
}

static void tsx_callback(void *token, pjsip_event *event)
{
    pj_status_t status;
    pjsip_publishc *pubc = (pjsip_publishc*) token;
    pjsip_transaction *tsx = event->body.tsx_state.tsx;
    pjsip_publishc_cb *cb;

    /* Hold the mutex for the whole callback, releasing it only around
     * calls into the application, so that an app thread sending or
     * destroying concurrently sees consistent state.
     */
    pj_mutex_lock(pubc->mutex);

    /* Decrement pending transaction counter. */
    pj_assert(pubc->pending_tsx > 0);
    --pubc->pending_tsx;

    /* Mark that we're in callback to prevent deletion (#1164) */
    ++pubc->in_callback;

    /* If publication data has been deleted by user then remove publication 
     * data from transaction's callback, and don't call callback.
     */
    if (pubc->_delete_flag) {

        /* Nothing to do */
        ;

    } else if (tsx->status_code == PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED ||
               tsx->status_code == PJSIP_SC_UNAUTHORIZED)
    {
        pjsip_rx_data *rdata = event->body.tsx_state.src.rdata;
        pjsip_tx_data *tdata;
        pjsip_auth_clt_async_on_chal_param chal_param = { 0 };

        /* Per-challenge token allocated from tsx->pool, kept alive
         * by the grp_lock ref until consumed (send or abandon).
         */
        {
            pjsip_auth_clt_async_impl_token *auth_token;
            auth_token = PJ_POOL_ZALLOC_T(tsx->pool,
                                          pjsip_auth_clt_async_impl_token);
            auth_token->user_data    = pubc;
            auth_token->send_impl    = &pubc_async_auth_send_impl;
            auth_token->abandon_impl = &pubc_async_auth_abandon_impl;
            auth_token->grp_lock     = tsx->grp_lock;
            pj_grp_lock_add_ref(tsx->grp_lock);

            /* The grp_lock ref only keeps the transaction alive. The app
             * may defer the challenge and consume the token long after
             * pubc would otherwise have been destroyed, so hold pubc too.
             */
            ++pubc->pending_auth_tokens;

            pj_bzero(&chal_param, sizeof(chal_param));
            chal_param.rdata = rdata;
            chal_param.tdata = tsx->last_tx;

            /* May call the application's challenge callback. */
            pj_mutex_unlock(pubc->mutex);
            status = pjsip_auth_clt_async_impl_on_challenge(
                                            &pubc->auth_sess,
                                            auth_token, &chal_param);
            pj_mutex_lock(pubc->mutex);
            if (status != PJ_SUCCESS) {
                pj_grp_lock_dec_ref(tsx->grp_lock);
                --pubc->pending_auth_tokens;
            }
        }
        if (status != PJ_SUCCESS) {
            status = pjsip_auth_clt_reinit_req(&pubc->auth_sess, rdata,
                                               tsx->last_tx, &tdata);
            if (status != PJ_SUCCESS) {
                cb = pubc->cb;
                pj_mutex_unlock(pubc->mutex);
                call_callback(pubc, cb, status, tsx->status_code,
                              &rdata->msg_info.msg->line.status.reason,
                              rdata, PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED);
                pj_mutex_lock(pubc->mutex);
            } else {
                /* Don't hold the mutex while sending (see #1247). */
                pj_mutex_unlock(pubc->mutex);
                status = pjsip_publishc_send(pubc, tdata);
                if (status == PJ_EBUSY) {
                    /* Another request got in during the challenge
                     * callback and queueing is off, so the challenged
                     * request is dropped. pubc_send() reports every
                     * other failure itself.
                     */
                    call_callback(pubc, pubc_get_cb(pubc), status,
                                  tsx->status_code,
                                  &rdata->msg_info.msg->line.status.reason,
                                  rdata, PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED);
                }
                pj_mutex_lock(pubc->mutex);
            }
        }

    } else {
        pjsip_rx_data *rdata;
        pj_uint32_t expiration = PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED;

        if (tsx->status_code/100 == 2) {
            pjsip_msg *msg;
            pjsip_expires_hdr *expires;
            pjsip_generic_string_hdr *etag_hdr;
            const pj_str_t STR_ETAG = { "SIP-ETag", 8 };

            rdata = event->body.tsx_state.src.rdata;
            msg = rdata->msg_info.msg;

            /* Save ETag value */
            etag_hdr = (pjsip_generic_string_hdr*)
                       pjsip_msg_find_hdr_by_name(msg, &STR_ETAG, NULL);
            if (etag_hdr) {
                pj_strdup(pubc->pool, &pubc->etag, &etag_hdr->hvalue);
            } else {
                pubc->etag.slen = 0;
            }

            /* Update expires value */
            expires = (pjsip_expires_hdr*)
                      pjsip_msg_find_hdr(msg, PJSIP_H_EXPIRES, NULL);

            if (pubc->auto_refresh && expires)
                expiration = expires->ivalue;

            if (expiration == 0) {
                /* The publication is gone: a refresh re-armed by an
                 * earlier response must not go out.
                 */
                pubc_cancel_timer(pubc);
            } else if (pubc->auto_refresh &&
                       expiration!=PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED)
            {
                const pj_time_val min_delay = {MIN_REFRESH_MSEC / 1000,
                                               MIN_REFRESH_MSEC % 1000};
                pj_time_val delay = {0, 0};

                pubc_cancel_timer(pubc);

                /* prevent underflow in case the remote answers with an unexpectedly low expiry value */
                if (expiration > DELAY_BEFORE_REFRESH) {
                    delay.sec = expiration - DELAY_BEFORE_REFRESH;
                }
                if (pubc->expires != PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED && 
                    delay.sec > (pj_int32_t)pubc->expires) 
                {
                    delay.sec = pubc->expires;
                }
                if (delay.sec < DELAY_BEFORE_REFRESH) {
                    if (expiration <= DELAY_BEFORE_REFRESH) {
                        /* Very small expiration: refresh before expiry. */
                        delay.sec  = (expiration > 1) ? (long)expiration - 1 : 0;
                        delay.msec = (expiration > 1) ? 0 : 500;
                    } else {
                        delay.sec = DELAY_BEFORE_REFRESH;
                    }
                }
                
                /* make sure we don't go below the minimum */
                if (PJ_TIME_VAL_LT(delay, min_delay)) {
                    delay = min_delay;
                }
                pubc->timer.cb = &pubc_refresh_timer_cb;
                pubc->timer.id = REFRESH_TIMER;
                pubc->timer.user_data = pubc;
                status = pjsip_endpt_schedule_timer(pubc->endpt, &pubc->timer,
                                                    &delay);
                if (status != PJ_SUCCESS) {
                    /* Keep timer.id consistent with the heap, since
                     * pubc_cancel_timer() infers a dispatched callback
                     * from a failed cancel.
                     */
                    pubc->timer.id = 0;
                    PJ_PERROR(4,(THIS_FILE, status,
                                 "Error scheduling refresh timer"));
                } else {
                    pj_gettimeofday(&pubc->last_refresh);
                    pubc->next_refresh = pubc->last_refresh;
                    PJ_TIME_VAL_ADD(pubc->next_refresh, delay);
                }
            }

        } else {
            rdata = (event->body.tsx_state.type==PJSIP_EVENT_RX_MSG) ? 
                        event->body.tsx_state.src.rdata : NULL;
        }


        /* Call callback. */
        // if (expiration == 0xFFFF) expiration = -1;

        /* Make requests sent during the callback queue behind this
         * response, so the loop below sends them in order with the
         * updated ETag (#996). This is kept apart from pending_tsx so
         * that a refresh timer firing meanwhile still sees no transaction
         * in flight. in_callback already prevents destruction.
         */
        ++pubc->in_resp_cb;
        cb = pubc->cb;

        pj_mutex_unlock(pubc->mutex);
        call_callback(pubc, cb, PJ_SUCCESS, tsx->status_code, 
                      (rdata ? &rdata->msg_info.msg->line.status.reason 
                        : pjsip_get_status_text(tsx->status_code)),
                      rdata, expiration);
        pj_mutex_lock(pubc->mutex);

        --pubc->in_resp_cb;

        /* If we have pending request(s), send them now */
        pubc_send_queued(pubc);
    }

    /* No longer in callback. */
    --pubc->in_callback;

    /* Delete the record if user destroy pubc during the callback. */
    pubc_unlock(pubc);
}


/* Send tdata as pubc's one outstanding transaction. Called with the mutex
 * held, no transaction pending, and a hold on pubc (in_callback or an auth
 * token), since tsx_callback() may run before send_request() returns and
 * complete a deferred destroy. The mutex is released around the transport
 * send (see #1247) and is held again on return. On failure the callback
 * has been called, except when the pubc has been destroyed (PJ_EGONE).
 */
static pj_status_t pubc_send(pjsip_publishc *pubc, pjsip_tx_data *tdata)
{
    pj_status_t status;
    pjsip_cseq_hdr *cseq_hdr;
    pj_uint32_t cseq;
    pj_bool_t no_tsx = PJ_FALSE;

    pj_assert(pubc->pending_tsx == 0);

    if (pubc->_delete_flag) {
        pjsip_tx_data_dec_ref(tdata);
        return PJ_EGONE;
    }

    /* If via_addr is set, use this address for the Via header. */
    if (pubc->via_addr.host.slen > 0) {
        tdata->via_addr = pubc->via_addr;
        tdata->via_tp = pubc->via_tp;
    }

    /* Invalidate message buffer. */
    pjsip_tx_data_invalidate_msg(tdata);

    /* Increment CSeq */
    cseq = ++pubc->cseq_hdr->cseq;
    cseq_hdr = (pjsip_cseq_hdr*)
               pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CSEQ, NULL);
    cseq_hdr->cseq = cseq;

    /* Increment pending transaction first, since transaction callback
     * may be called even before send_request() returns!
     */
    ++pubc->pending_tsx;

    pj_mutex_unlock(pubc->mutex);

    status = pjsip_endpt_send_request(pubc->endpt, tdata, -1, pubc, 
                                      &tsx_callback);

    pj_mutex_lock(pubc->mutex);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4,(THIS_FILE, status, "Error sending request"));

        /* tsx_callback() is not called when the transaction could not be
         * created. If it has run, or another request has been sent since,
         * the counter or the CSeq has changed (see also #1936).
         */
        no_tsx = (pubc->pending_tsx == 1 && cseq == pubc->cseq_hdr->cseq);
    }

    if (no_tsx) {
        char errmsg[PJ_ERR_MSG_SIZE];
        pj_str_t reason = pj_strerror(status, errmsg, sizeof(errmsg));
        pjsip_publishc_cb *cb = pubc->cb;

        /* Report it as tsx_callback() would, keeping the transaction
         * claimed across the callback so that requests sent meanwhile
         * queue behind those already queued.
         */
        pj_mutex_unlock(pubc->mutex);
        call_callback(pubc, cb, status, PJSIP_SC_TSX_TRANSPORT_ERROR,
                      &reason, NULL, PJSIP_PUBC_EXPIRATION_NOT_SPECIFIED);
        pj_mutex_lock(pubc->mutex);

        --pubc->pending_tsx;
    }

    return status;
}

/* Send the queued requests, oldest first, until one is in flight. Called
 * like pubc_send(): with the mutex held and a hold on pubc; the mutex is
 * held on return. A failed send may run tsx_callback() nested or let
 * another thread in, so the transaction count is rechecked each round.
 */
static void pubc_send_queued(pjsip_publishc *pubc)
{
    while (pubc->pending_tsx == 0 && !pubc->in_resp_cb &&
           !pj_list_empty(&pubc->pending_reqs))
    {
        pending_publish *pp = pubc->pending_reqs.next;
        pjsip_tx_data *tdata = pp->tdata;

        /* Remove the request from pending request list,
         * and keep the unused entry into pending_reqs_empty pool.
         */
        pj_list_erase(pp);
        pj_list_push_back(&pubc->pending_reqs_empty, pp);

        /* Add SIP-If-Match if we have etag and the request doesn't have
         * one (https://github.com/pjsip/pjproject/issues/996)
         */
        if (pubc->etag.slen) {
            const pj_str_t STR_HNAME = { "SIP-If-Match", 12 };
            pjsip_generic_string_hdr *sim_hdr;

            sim_hdr = (pjsip_generic_string_hdr*)
                      pjsip_msg_find_hdr_by_name(tdata->msg, &STR_HNAME, NULL);
            if (!sim_hdr) {
                /* Create the header */
                sim_hdr = pjsip_generic_string_hdr_create(tdata->pool,
                                                          &STR_HNAME,
                                                          &pubc->etag);
                pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)sim_hdr);

            } else {
                /* Update */
                if (pj_strcmp(&pubc->etag, &sim_hdr->hvalue))
                    pj_strdup(tdata->pool, &sim_hdr->hvalue, &pubc->etag);
            }
        }

        /* pubc_send() claims the transaction before releasing the mutex,
         * so a concurrent pjsip_publishc_send() queues behind this request
         * instead of overtaking it.
         */
        pubc_send(pubc, tdata);
    }
}


PJ_DEF(pj_status_t) pjsip_publishc_send(pjsip_publishc *pubc, 
                                        pjsip_tx_data *tdata)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(pubc && tdata, PJ_EINVAL);

    pj_mutex_lock(pubc->mutex);

    if (pubc->_delete_flag) {
        pjsip_tx_data_dec_ref(tdata);
        pj_mutex_unlock(pubc->mutex);
        return PJ_EGONE;
    }

    /* Make sure we don't have pending transaction. */
    if (pubc->pending_tsx || pubc->in_resp_cb) {
        if (pubc->opt.queue_request) {
            pending_publish *pp = NULL;
            if (pj_list_empty(&pubc->pending_reqs_empty)) {
                pp = PJ_POOL_ZALLOC_T(pubc->pool, pending_publish);
            } else {
                pp = pubc->pending_reqs_empty.next;
                pj_list_erase(pp);
            }
            pp->tdata = tdata;
            pj_list_push_back(&pubc->pending_reqs, pp);
            pj_mutex_unlock(pubc->mutex);
            PJ_LOG(4,(THIS_FILE, "Request is queued, pubc has another "
                                 "transaction pending"));
            return PJ_EPENDING;
        } else {
            pjsip_tx_data_dec_ref(tdata);
            pj_mutex_unlock(pubc->mutex);
            PJ_LOG(4,(THIS_FILE, "Unable to send request, pubc has another "
                                 "transaction pending"));
            return PJ_EBUSY;
        }
    }

    ++pubc->in_callback;

    status = pubc_send(pubc, tdata);

    /* A failure that created no transaction leaves nothing in flight, so
     * send what was queued meanwhile.
     */
    pubc_send_queued(pubc);

    --pubc->in_callback;
    pubc_unlock(pubc);

    return status;
}

