/* $Header: /pjproject/pjsip/src/pjsip_simple/presence.c 7     8/24/05 10:33a Bennylp $ */
/* 
 * PJSIP - SIP Stack
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <pjsip_simple/presence.h>
#include <pjsip/sip_transport.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/guid.h>
#include <pj/os.h>
#include <stdio.h>

/* Forward declarations. */
static void on_query_subscribe(pjsip_rx_data *rdata, int *status);
static void on_subscribe(pjsip_event_sub *sub, pjsip_rx_data *rdata,
			 pjsip_event_sub_cb **cb, int *expires);
static void on_sub_terminated(pjsip_event_sub *sub, const pj_str_t *reason);
static void on_sub_received_refresh(pjsip_event_sub *sub, pjsip_rx_data *rdata);
static void on_received_notify(pjsip_event_sub *sub, pjsip_rx_data *rdata);

/* Some string constants. */
static pj_str_t PRESENCE_EVENT = { "presence", 8 };

/* Accept types. */
static pj_str_t accept_names[] = {
    { "application/pidf+xml", 20 },
    { "application/xpidf+xml", 21 }
};
static pjsip_media_type accept_types[] = {
    {
	{ "application", 11 },
	{ "pidf+xml", 8 }
    },
    {
	{ "application", 11 },
	{ "xpidf+xml", 9 }
    }
};

/* Callback that is registered by application. */
static pjsip_presence_cb cb;

/* Package callback to be register to event_notify */
static pjsip_event_sub_pkg_cb pkg_cb = { &on_query_subscribe,
					 &on_subscribe };

/* Global/static callback to be registered to event_notify */
static pjsip_event_sub_cb sub_cb = { &on_sub_terminated,
				     &on_sub_received_refresh,
				     NULL,
				     &on_received_notify,
				     NULL };

/*
 * Initialize presence module.
 * This will register event package "presence" to event framework.
 */
PJ_DEF(void) pjsip_presence_init(const pjsip_presence_cb *pcb)
{
    pj_memcpy(&cb, pcb, sizeof(*pcb));
    pjsip_event_sub_register_pkg( &PRESENCE_EVENT, 
				  sizeof(accept_names)/sizeof(accept_names[0]),
				  accept_names,
				  &pkg_cb);
}

/*
 * Create presence subscription.
 */
PJ_DEF(pjsip_presentity*) pjsip_presence_create( pjsip_endpoint *endpt,
						 const pj_str_t *local_url,
						 const pj_str_t *remote_url,
						 int expires,
						 void *user_data )
{
    pjsip_event_sub *sub;
    pjsip_presentity *pres;

    if (expires < 0)
	expires = 300;

    /* Create event subscription */
    sub = pjsip_event_sub_create(endpt, local_url, remote_url, &PRESENCE_EVENT, 
				 expires, 
				 sizeof(accept_names)/sizeof(accept_names[0]),
				 accept_names,
				 NULL, &sub_cb);
    if (!sub)
	return NULL;

    /* Allocate presence descriptor. */
    pres = pj_pool_calloc(sub->pool, 1, sizeof(*pres));
    pres->sub = sub;
    pres->user_data = user_data;
    sub->user_data = pres;

    return pres;
}

/*
 * Send SUBSCRIBE.
 */
PJ_DEF(pj_status_t) pjsip_presence_subscribe( pjsip_presentity *pres )
{
    return pjsip_event_sub_subscribe( pres->sub );
}

/*
 * Set credentials to be used for outgoing requests.
 */
PJ_DEF(pj_status_t) pjsip_presence_set_credentials( pjsip_presentity *pres,
						    int count,
						    const pjsip_cred_info cred[])
{
    return pjsip_event_sub_set_credentials(pres->sub, count, cred);
}

/*
 * Set route-set.
 */
PJ_DEF(pj_status_t) pjsip_presence_set_route_set( pjsip_presentity *pres,
						  const pjsip_route_hdr *hdr )
{
    return pjsip_event_sub_set_route_set( pres->sub, hdr );
}

/*
 * Unsubscribe.
 */
PJ_DEF(pj_status_t) pjsip_presence_unsubscribe( pjsip_presentity *pres )
{
    return pjsip_event_sub_unsubscribe(pres->sub);
}

/*
 * This is the pjsip_msg_body callback to print XML body.
 */
static int print_xml(pjsip_msg_body *body, char *buf, pj_size_t size)
{
    return pj_xml_print( body->data, buf, size, PJ_TRUE );
}

/*
 * Create and initialize PIDF document and msg body (notifier only).
 */
static pj_status_t init_presence_info( pjsip_presentity *pres )
{
    pj_str_t uri;
    pj_pool_t *pool = pres->sub->pool;
    char tmp[PJSIP_MAX_URL_SIZE];
    pjpidf_tuple *tuple;
    const pjsip_media_type *content_type = NULL;

    pj_assert(pres->uas_body == NULL);

    /* Make entity_id */
    uri.ptr = tmp;
    uri.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, pres->sub->from->uri, 
			      tmp, sizeof(tmp));
    if (uri.slen < 0)
	return -1;

    if (pres->pres_type == PJSIP_PRES_TYPE_PIDF) {
	pj_str_t s;

	/* Create <presence>. */
	pres->uas_data.pidf = pjpidf_create(pool, &s);

	/* Create <tuple> */
	pj_create_unique_string(pool, &s);
	tuple = pjpidf_pres_add_tuple(pool, pres->uas_data.pidf, &s);

	/* Set <contact> */
	s.ptr = tmp;
	s.slen = pjsip_uri_print(PJSIP_URI_IN_REQ_URI, pres->sub->contact->uri, tmp, sizeof(tmp));
	if (s.slen < 0)
	    return -1;
	pjpidf_tuple_set_contact(pool, tuple, &s);

	/* Content-Type */
	content_type = &accept_types[PJSIP_PRES_TYPE_PIDF];

    } else if (pres->pres_type == PJSIP_PRES_TYPE_XPIDF) {

	/* Create XPIDF */
	pres->uas_data.xpidf = pjxpidf_create(pool, &uri);

	/* Content-Type. */
	content_type = &accept_types[PJSIP_PRES_TYPE_XPIDF];
    }

    /* Create message body */
    pres->uas_body = pj_pool_alloc(pool, sizeof(pjsip_msg_body));
    pres->uas_body->content_type = *content_type;
    pres->uas_body->data = pres->uas_data.pidf;
    pres->uas_body->len = 0;
    pres->uas_body->print_body = &print_xml;

    return 0;
}

/*
 * Send NOTIFY and set subscription state.
 */
PJ_DEF(pj_status_t) pjsip_presence_notify( pjsip_presentity *pres,
					   pjsip_event_sub_state state,
					   pj_bool_t is_online )
{
    pj_str_t reason = { "", 0 };

    if (pres->uas_data.pidf == NULL) {
	if (init_presence_info(pres) != 0)
	    return -1;
    }

    /* Update basic status in PIDF/XPIDF document. */
    if (pres->pres_type == PJSIP_PRES_TYPE_PIDF) {
	pjpidf_tuple *first;
	pjpidf_status *status;
	pj_time_val now;
	pj_parsed_time pnow;

	first = pjpidf_op.pres.get_first_tuple(pres->uas_data.pidf);
	pj_assert(first);
	status = pjpidf_op.tuple.get_status(first);
	pj_assert(status);
	pjpidf_op.status.set_basic_open(status, is_online);

	/* Update timestamp. */
	if (pres->timestamp.ptr == 0) {
	    pres->timestamp.ptr = pj_pool_alloc(pres->sub->pool, 24);
	}
	pj_gettimeofday(&now);
	pj_time_decode(&now, &pnow);
	pres->timestamp.slen = sprintf(pres->timestamp.ptr,
				       "%04d-%02d-%02dT%02d:%02d:%02dZ",
				       pnow.year, pnow.mon, pnow.day,
				       pnow.hour, pnow.min, pnow.sec);
	pjpidf_op.tuple.set_timestamp_np(pres->sub->pool, first, &pres->timestamp);

    } else if (pres->pres_type == PJSIP_PRES_TYPE_XPIDF) {
	pjxpidf_set_status( pres->uas_data.xpidf, is_online );

    } else {
	pj_assert(0);
    }

    /* Send notify. */
    return pjsip_event_sub_notify( pres->sub, state, &reason, pres->uas_body);
}

/*
 * Destroy subscription (can be called for both subscriber and notifier).
 */
PJ_DEF(pj_status_t) pjsip_presence_destroy( pjsip_presentity *pres )
{
    return pjsip_event_sub_destroy(pres->sub);
}

/*
 * This callback is called by event framework to query whether we want to
 * accept an incoming subscription.
 */
static void on_query_subscribe(pjsip_rx_data *rdata, int *status)
{
    if (cb.accept_presence) {
	(*cb.accept_presence)(rdata, status);
    }
}

/*
 * This callback is called by event framework after we accept the incoming
 * subscription, to notify about the new subscription instance.
 */
static void on_subscribe(pjsip_event_sub *sub, pjsip_rx_data *rdata,
			 pjsip_event_sub_cb **set_sub_cb, int *expires)
{
    pjsip_presentity *pres;
    pjsip_accept_hdr *accept;

    pres = pj_pool_calloc(sub->pool, 1, sizeof(*pres));
    pres->sub = sub;
    pres->pres_type = PJSIP_PRES_TYPE_PIDF;
    sub->user_data = pres;
    *set_sub_cb = &sub_cb;

    accept = pjsip_msg_find_hdr(rdata->msg, PJSIP_H_ACCEPT, NULL);
    if (accept) {
	unsigned i;
	int found = 0;
	for (i=0; i<accept->count && !found; ++i) {
	    int j;
	    for (j=0; j<sizeof(accept_names)/sizeof(accept_names[0]); ++j) {
		if (!pj_stricmp(&accept->values[i], &accept_names[j])) {
		    pres->pres_type = j;
		    found = 1;
		    break;
		}
	    }
	}
	pj_assert(found );
    }

    (*cb.on_received_request)(pres, rdata, expires);
}

/*
 * This callback is called by event framework when the subscription is
 * terminated.
 */
static void on_sub_terminated(pjsip_event_sub *sub, const pj_str_t *reason)
{
    pjsip_presentity *pres = sub->user_data;
    if (cb.on_terminated)
	(*cb.on_terminated)(pres, reason);
}

/*
 * This callback is called by event framework when it receives incoming
 * SUBSCRIBE request to refresh the subscription.
 */
static void on_sub_received_refresh(pjsip_event_sub *sub, pjsip_rx_data *rdata)
{
    pjsip_presentity *pres = sub->user_data;
    if (cb.on_received_refresh)
	(*cb.on_received_refresh)(pres, rdata);
}

/*
 * This callback is called by event framework when it receives incoming
 * NOTIFY request.
 */
static void on_received_notify(pjsip_event_sub *sub, pjsip_rx_data *rdata)
{
    pjsip_presentity *pres = sub->user_data;

    if (cb.on_received_update) {
	pj_status_t is_open;
	pjsip_msg_body *body;
	int i;

	body = rdata->msg->body;
	if (!body)
	    return;

	for (i=0; i<sizeof(accept_types)/sizeof(accept_types[0]); ++i) {
	    if (!pj_stricmp(&body->content_type.type, &accept_types[i].type) &&
		!pj_stricmp(&body->content_type.subtype, &accept_types[i].subtype))
	    {
		break;
	    }
	}

	if (i==PJSIP_PRES_TYPE_PIDF) {
	    pjpidf_pres *pres;
	    pjpidf_tuple *tuple;
	    pjpidf_status *status;

	    pres = pjpidf_parse(rdata->pool, body->data, body->len);
	    if (!pres)
		return;
	    tuple = pjpidf_pres_get_first_tuple(pres);
	    if (!tuple)
		return;
	    status = pjpidf_tuple_get_status(tuple);
	    if (!status)
		return;
	    is_open = pjpidf_status_is_basic_open(status);

	} else if (i==PJSIP_PRES_TYPE_XPIDF) {
	    pjxpidf_pres *pres;

	    pres = pjxpidf_parse(rdata->pool, body->data, body->len);
	    if (!pres)
		return;
	    is_open = pjxpidf_get_status(pres);

	} else {
	    return;
	}

	(*cb.on_received_update)(pres, is_open);
    }
}

