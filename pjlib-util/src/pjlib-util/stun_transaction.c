/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#include <pjlib-util/stun_transaction.h>
#include <pjlib-util/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/timer.h>


#define TIMER_ACTIVE		1


struct pj_stun_client_tsx
{
    char		 obj_name[PJ_MAX_OBJ_NAME];
    pj_pool_t		*pool;
    pj_stun_endpoint	*endpt;
    pj_stun_tsx_cb	 cb;
    void		*user_data;

    pj_uint32_t		 tsx_id[4];

    pj_bool_t		 require_retransmit;
    pj_timer_entry	 timer;
    unsigned		 transmit_count;
    pj_time_val		 retransmit_time;

    pj_uint8_t		 last_pkt[PJ_STUN_MAX_PKT_LEN];
    unsigned		 last_pkt_size;
};


static void retransmit_timer_callback(pj_timer_heap_t *timer_heap, 
				      pj_timer_entry *timer);

static void stun_perror(pj_stun_client_tsx *tsx, const char *title,
			pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(4,(tsx->obj_name, "%s: %s", title, errmsg));
}


/*
 * Create a STUN client transaction.
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_create(pj_stun_endpoint *endpt,
					      const pj_stun_tsx_cb *cb,
					      pj_stun_client_tsx **p_tsx)
{
    pj_pool_t *pool;
    pj_stun_client_tsx *tsx;

    PJ_ASSERT_RETURN(endpt && cb && p_tsx, PJ_EINVAL);
    PJ_ASSERT_RETURN(cb->on_send_msg, PJ_EINVAL);

    pool = pj_pool_create(endpt->pf, "tsx", 1000, 1000, NULL);
    tsx = PJ_POOL_ZALLOC_TYPE(pool, pj_stun_client_tsx);
    tsx->pool = pool;
    tsx->endpt = endpt;
    pj_memcpy(&tsx->cb, cb, sizeof(*cb));

    tsx->timer.cb = &retransmit_timer_callback;
    tsx->timer.user_data = tsx;

    pj_ansi_snprintf(tsx->obj_name, sizeof(tsx->obj_name), "stuntsx%p", pool);

    *p_tsx = tsx;

    PJ_LOG(4,(tsx->obj_name, "STUN client transaction created"));
    return PJ_SUCCESS;
}


/*
 * .
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_destroy(pj_stun_client_tsx *tsx)
{
    PJ_ASSERT_RETURN(tsx, PJ_EINVAL);

    if (tsx->timer.id != 0) {
	pj_timer_heap_cancel(tsx->endpt->timer_heap, &tsx->timer);
	tsx->timer.id = 0;
    }
    pj_pool_release(tsx->pool);
    return PJ_SUCCESS;
}


/*
 * Set user data.
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_set_data(pj_stun_client_tsx *tsx,
						void *data)
{
    PJ_ASSERT_RETURN(tsx, PJ_EINVAL);
    tsx->user_data = data;
    return PJ_SUCCESS;
}


/*
 * Get the user data
 */
PJ_DEF(void*) pj_stun_client_tsx_get_data(pj_stun_client_tsx *tsx)
{
    PJ_ASSERT_RETURN(tsx, NULL);
    return tsx->user_data;
}


/*
 * Transmit message.
 */
static pj_status_t tsx_transmit_msg(pj_stun_client_tsx *tsx)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(tsx->timer.id == 0, PJ_EBUSY);

    if (tsx->require_retransmit) {
	/* Calculate retransmit/timeout delay */
	if (tsx->transmit_count == 0) {
	    tsx->retransmit_time.sec = 0;
	    tsx->retransmit_time.msec = tsx->endpt->rto_msec;

	} else if (tsx->transmit_count < PJ_STUN_MAX_RETRANSMIT_COUNT) {
	    unsigned msec;

	    msec = PJ_TIME_VAL_MSEC(tsx->retransmit_time);
	    msec = (msec >> 1) + 100;
	    tsx->retransmit_time.sec = msec / 1000;
	    tsx->retransmit_time.msec = msec % 100;

	} else {
	    tsx->retransmit_time.sec = PJ_STUN_TIMEOUT_VALUE / 1000;
	    tsx->retransmit_time.msec = PJ_STUN_TIMEOUT_VALUE % 1000;
	}

	/* Schedule timer first because when send_msg() failed we can
	 * cancel it (as opposed to when schedule_timer() failed we cannot
	 * cancel transmission).
	 */
	status = pj_timer_heap_schedule(tsx->endpt->timer_heap, &tsx->timer,
					&tsx->retransmit_time);
	if (status != PJ_SUCCESS) {
	    tsx->timer.id = 0;
	    return status;
	}
    }


    /* Send message */
    status = tsx->cb.on_send_msg(tsx, tsx->last_pkt, tsx->last_pkt_size);
    if (status != PJ_SUCCESS) {
	if (tsx->timer.id != 0) {
	    pj_timer_heap_cancel(tsx->endpt->timer_heap, &tsx->timer);
	    tsx->timer.id = 0;
	}
	stun_perror(tsx, "STUN error sending message", status);
	return status;
    }

    tsx->transmit_count++;

    PJ_LOG(4,(tsx->obj_name, "STUN sending message (transmit count=%d)",
	      tsx->transmit_count));
    return status;
}


/*
 * Send outgoing message and start STUN transaction.
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_send_msg(pj_stun_client_tsx *tsx,
						pj_bool_t retransmit,
						const pj_stun_msg *msg)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(tsx && msg, PJ_EINVAL);
    PJ_ASSERT_RETURN(tsx->timer.id != 0, PJ_EBUSY);

    /* Encode message */
    status = pj_stun_msg_encode(msg, tsx->last_pkt, sizeof(tsx->last_pkt), 
				0, &tsx->last_pkt_size);
    if (status != PJ_SUCCESS) {
	stun_perror(tsx, "STUN msg_encode() failed", status);
	return status;
    }

    /* Update STUN transaction ID */
    tsx->tsx_id[0] = msg->hdr.magic;
    pj_memcpy(&tsx->tsx_id[1], msg->hdr.tsx_id, 12);

    /* Update STUN retransmit flag */
    tsx->require_retransmit = retransmit;

    /* Send the message */
    return tsx_transmit_msg(tsx);
}


/* Retransmit timer callback */
static void retransmit_timer_callback(pj_timer_heap_t *timer_heap, 
				      pj_timer_entry *timer)
{
    pj_stun_client_tsx *tsx = (pj_stun_client_tsx *) timer->user_data;
    pj_status_t status;

    PJ_UNUSED_ARG(timer_heap);

    if (tsx->transmit_count >= PJ_STUN_MAX_RETRANSMIT_COUNT) {
	/* Retransmission count exceeded. Transaction has failed */
	tsx->timer.id = 0;
	PJ_LOG(4,(tsx->obj_name, "STUN timeout waiting for response"));
	if (tsx->cb.on_complete) {
	    tsx->cb.on_complete(tsx, PJLIB_UTIL_ESTUNNOTRESPOND, NULL);
	}
	return;
    }

    tsx->timer.id = 0;
    status = tsx_transmit_msg(tsx);
    if (status != PJ_SUCCESS) {
	tsx->timer.id = 0;
	if (tsx->cb.on_complete) {
	    tsx->cb.on_complete(tsx, status, NULL);
	}
    }
}


/*
 * Notify the STUN transaction about the arrival of STUN response.
 */
PJ_DEF(pj_status_t) pj_stun_client_tsx_on_rx_msg(pj_stun_client_tsx *tsx,
						 const void *packet,
						 pj_size_t pkt_size,
						 unsigned *parsed_len)
{
    pj_stun_msg *msg;
    pj_stun_error_code_attr *err_attr;
    pj_status_t status;

    PJ_ASSERT_RETURN(tsx && packet && pkt_size, PJ_EINVAL);

    /* Try to parse the message */
    status = pj_stun_msg_decode(tsx->pool, (const pj_uint8_t*)packet,
			        pkt_size, 0, &msg, parsed_len,
				NULL, NULL, NULL);
    if (status != PJ_SUCCESS) {
	stun_perror(tsx, "STUN msg_decode() error", status);
	return status;
    }

    /* Must be STUN response message */
    if (!PJ_STUN_IS_RESPONSE(msg->hdr.type) && 
	!PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type))
    {
	PJ_LOG(4,(tsx->obj_name, 
		  "STUN rx_msg() error: not response message"));
	return PJLIB_UTIL_ESTUNNOTRESPONSE;
    }

    /* Compare response's transaction ID */
    if (msg->hdr.magic != tsx->tsx_id[0] ||
	pj_memcmp(msg->hdr.tsx_id, &tsx->tsx_id[1], 12) != 0)
    {
	return PJLIB_UTIL_ESTUNINVALIDID;
    }

    /* We have a response with matching transaction ID. 
     * We can cancel retransmit timer now.
     */
    if (tsx->timer.id) {
	pj_timer_heap_cancel(tsx->endpt->timer_heap, &tsx->timer);
	tsx->timer.id = 0;
    }

    /* Find STUN error code attribute */
    err_attr = (pj_stun_error_code_attr*) 
		pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_ERROR_CODE, 0);

    if (err_attr && err_attr->err_class <= 2) {
	/* draft-ietf-behave-rfc3489bis-05.txt Section 8.3.2:
	 * Any response between 100 and 299 MUST result in the cessation
	 * of request retransmissions, but otherwise is discarded.
	 */
	PJ_LOG(4,(tsx->obj_name, 
		  "STUN rx_msg() error: received provisional %d code (%.*s)",
		  err_attr->err_class * 100 + err_attr->number,
		  (int)err_attr->reason.slen,
		  err_attr->reason.ptr));
	return PJ_SUCCESS;
    }

    if (err_attr == NULL) {
	status = PJ_SUCCESS;
    } else {
	status = PJ_STATUS_FROM_STUN_CODE(err_attr->err_class * 100 +
					  err_attr->number);
    }

    /* Call callback */
    if (tsx->cb.on_complete) {
	tsx->cb.on_complete(tsx, status, msg);
    }

    return PJ_SUCCESS;
}

