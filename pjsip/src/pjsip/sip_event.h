/* $Header: /pjproject/pjsip/src/pjsip/sip_event.h 5     6/17/05 11:16p Bennylp $ */
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
#ifndef __PJSIP_SIP_EVENT_H__
#define __PJSIP_SIP_EVENT_H__

/**
 * @file sip_event.h
 * @brief SIP Event
 */

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_EVENT SIP Event
 * @ingroup PJSIP
 * @{
 */
#include <pj/types.h>


/** 
 * Event IDs.
 */
typedef enum pjsip_event_id_e
{
    /** Unidentified event. */
    PJSIP_EVENT_UNIDENTIFIED,

    /** Timer event, normally only used internally in transaction. */
    PJSIP_EVENT_TIMER,

    /** Message transmission event. */
    PJSIP_EVENT_TX_MSG,

    /** Message received event. */
    PJSIP_EVENT_RX_MSG,

    /** Transport error event. */
    PJSIP_EVENT_TRANSPORT_ERROR,

    /** Transaction state changed event. */
    PJSIP_EVENT_TSX_STATE_CHANGED,

    /** 2xx response received event. */
    PJSIP_EVENT_RX_200_RESPONSE,

    /** ACK request received event. */
    PJSIP_EVENT_RX_ACK_MSG,

    /** Message discarded event. */
    PJSIP_EVENT_DISCARD_MSG,

    /** Indicates that the event was triggered by user action. */
    PJSIP_EVENT_USER,

    /** On before transmitting message. */
    PJSIP_EVENT_BEFORE_TX,

} pjsip_event_id_e;


/**
 * \struct
 * \brief Event descriptor to fully identify a SIP event.
 *
 * Events are the only way for a lower layer object to inform something
 * to higher layer objects. Normally this is achieved by means of callback,
 * i.e. the higher layer objects register a callback to handle the event on
 * the lower layer objects.
 *
 * This event descriptor is used for example by transactions, to inform
 * endpoint about events, and by transports, to inform endpoint about
 * unexpected transport error.
 */
struct pjsip_event
{
    /** This is necessary so that we can put events as a list. */
    PJ_DECL_LIST_MEMBER(struct pjsip_event)

    /** The event type, can be any value of \b pjsip_event_id_e.
     *  @see pjsip_event_id_e
     */
    pjsip_event_id_e type;

    /** This field determines what is the content of \b src (source data). 
     */
    pjsip_event_id_e src_type;

    /** Source data, which content is dependent on \b src_type.
     *  - if src_type==PJSIP_EVENT_RX_MSG, src.rdata is valid.
     *  - if src_type==PJSIP_EVENT_TX_MSG, src.tdata is valid.
     *  - if src_type==PJSIP_EVENT_TIMER, src.timer is valid.
     */
    union
    {
	pjsip_rx_data	*rdata;
	pjsip_tx_data	*tdata;
	pj_timer_entry	*timer;
	void		*data;
	unsigned long	 udata;
    } src;

    /** The object that generates this event. */
    union
    {
	pjsip_transaction  *tsx;
	void		   *ptr;
	unsigned long	    udata;
    } obj;

    /** Other data. */
    union
    {
	long		    long_data;
	void *		    ptr_data;
    } data;
};

/**
 * Get the event string from the event ID.
 * @param e the event ID.
 * @notes defined in sip_misc.c
 */
PJ_DEF(const char *) pjsip_event_str(pjsip_event_id_e e);

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJSIP_SIP_EVENT_H__ */
