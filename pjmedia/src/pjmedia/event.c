/* $Id$ */
/* 
 * Copyright (C) 2011-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia/event.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/string.h>

#define THIS_FILE	"event.c"

#if 1
#   define TRACE_(x)	PJ_LOG(6,x)
#else
#   define TRACE_(x)
#endif

PJ_DEF(void) pjmedia_event_init( pjmedia_event *event,
                                 pjmedia_event_type type,
                                 const pj_timestamp *ts,
                                 const pjmedia_event_publisher *epub)
{
    pj_bzero(event, sizeof(*event));
    event->type = type;
    if (ts)
	event->timestamp.u64 = ts->u64;
    event->epub = epub;
    if (epub)
	event->epub_sig = epub->sig;
}

PJ_DEF(void) pjmedia_event_publisher_init(pjmedia_event_publisher *epub,
                                          pjmedia_obj_sig sig)
{
    pj_bzero(epub, sizeof(*epub));
    pj_list_init(&epub->subscription_list);
    epub->sig = sig;
}

PJ_DEF(void) pjmedia_event_subscription_init( pjmedia_event_subscription *esub,
                                              pjmedia_event_cb *cb,
                                              void *user_data)
{
    pj_bzero(esub, sizeof(*esub));
    esub->cb = cb;
    esub->user_data = user_data;
}

PJ_DEF(pj_bool_t)
pjmedia_event_publisher_has_sub(pjmedia_event_publisher *epub)
{
    PJ_ASSERT_RETURN(epub, PJ_FALSE);
    return epub->subscription_list.next &&
	    (!pj_list_empty(&epub->subscription_list));
}

PJ_DEF(pj_status_t) pjmedia_event_subscribe( pjmedia_event_publisher *epub,
                                             pjmedia_event_subscription *esub)
{
    PJ_ASSERT_RETURN(epub && esub && esub->cb, PJ_EINVAL);
    /* Must not currently subscribe to anything */
    PJ_ASSERT_RETURN(esub->subscribe_to == NULL, PJ_EINVALIDOP);

    pj_list_push_back(&epub->subscription_list, esub);
    esub->subscribe_to = epub;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_event_unsubscribe(pjmedia_event_subscription *esub)
{
    PJ_ASSERT_RETURN(esub, PJ_EINVAL);
    if (esub->subscribe_to) {
	PJ_ASSERT_RETURN(
	    pj_list_find_node(&esub->subscribe_to->subscription_list,
	                      esub)==esub, PJ_ENOTFOUND);
	pj_list_erase(esub);
	esub->subscribe_to = NULL;
    }
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_event_publish( pjmedia_event_publisher *epub,
                                           pjmedia_event *event)
{
    pjmedia_event_subscription *esub;
    char event_name[5];
    char epub_name[5];
    pj_status_t err = PJ_SUCCESS;

    PJ_ASSERT_RETURN(epub && event, PJ_EINVAL);

    TRACE_((THIS_FILE, "Event %s is published by publisher %s",
		       pjmedia_fourcc_name(event->type, event_name),
		       pjmedia_fourcc_name(epub->sig, epub_name)));
    /* Suppress compiler warning if trace is disabled */
    PJ_UNUSED_ARG(event_name);
    PJ_UNUSED_ARG(epub_name);

    esub = epub->subscription_list.next;
    if (!esub)
	return err;

    while (esub != &epub->subscription_list) {
	pjmedia_event_subscription *next;
	pj_status_t status;

	/* just in case esub is destroyed in the callback */
	next = esub->next;

	status = (*esub->cb)(esub, event);
	if (status != PJ_SUCCESS && err == PJ_SUCCESS)
	    err = status;

	esub = next;
    }

    return err;
}

static pj_status_t republisher_cb(pjmedia_event_subscription *esub,
       				  pjmedia_event *event)
{
    return pjmedia_event_publish((pjmedia_event_publisher*)esub->user_data,
                                 event);
}

PJ_DEF(pj_status_t) pjmedia_event_republish(pjmedia_event_publisher *esrc,
                                             pjmedia_event_publisher *epub,
                                             pjmedia_event_subscription *esub)
{
    PJ_ASSERT_RETURN(esrc && epub && esub, PJ_EINVAL);

    pjmedia_event_subscription_init(esub, &republisher_cb, epub);
    return pjmedia_event_subscribe(esrc, esub);
}

