/* $Header: /pjproject/pjsip/src/pjsip/sip_transaction.h 5     6/17/05 11:16p Bennylp $ */
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
#ifndef __PJSIP_SIP_TRANSACTION_H__
#define __PJSIP_SIP_TRANSACTION_H__

/**
 * @file sip_transaction.h
 * @brief SIP Transaction
 */

#include <pjsip/sip_msg.h>
#include <pjsip/sip_resolve.h>
//#include <pjsip/sip_config.h>
//#include <pjsip/sip_endpoint.h>
#include <pj/timer.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_TRANSACT SIP Transaction
 * @ingroup PJSIP
 * @{
 */

struct pjsip_transaction;


/**
 * Transaction state.
 */
typedef enum pjsip_tsx_state_e
{
    PJSIP_TSX_STATE_NULL,
    PJSIP_TSX_STATE_CALLING,
    PJSIP_TSX_STATE_TRYING,
    PJSIP_TSX_STATE_PROCEEDING,
    PJSIP_TSX_STATE_COMPLETED,
    PJSIP_TSX_STATE_CONFIRMED,
    PJSIP_TSX_STATE_TERMINATED,
    PJSIP_TSX_STATE_DESTROYED,
    PJSIP_TSX_STATE_MAX,
} pjsip_tsx_state_e;


/**
 * State of the transport in the transaction.
 * The transport is progressing independently of the transaction.
 */
typedef enum pjsip_tsx_transport_state_e
{
    PJSIP_TSX_TRANSPORT_STATE_NULL,
    PJSIP_TSX_TRANSPORT_STATE_RESOLVING,
    PJSIP_TSX_TRANSPORT_STATE_CONNECTING,
    PJSIP_TSX_TRANSPORT_STATE_FINAL,
} pjsip_tsx_transport_state_e;


/**
 * Transaction state.
 */
struct pjsip_transaction
{
    pj_pool_t		       *pool;
    pjsip_endpoint	       *endpt;
    char			obj_name[PJ_MAX_OBJ_NAME];
    pjsip_role_e		role;
    int				status_code;
    pjsip_tsx_state_e		state;
    int (*state_handler)(struct pjsip_transaction *, pjsip_event *);

    pj_mutex_t		       *mutex;
    pjsip_method		method;
    int				cseq;
    pj_str_t			transaction_key;
    pj_str_t			branch;

    pjsip_tsx_transport_state_e	transport_state;
    pjsip_host_port		dest_name;
    int				current_addr;
    pjsip_server_addresses	remote_addr;
    pjsip_transport_t	       *transport;

    pjsip_tx_data	       *last_tx;
    int				has_unsent_msg;
    int				handle_ack;
    int				retransmit_count;

    pj_timer_entry		retransmit_timer;
    pj_timer_entry		timeout_timer;
    void		       *module_data[PJSIP_MAX_MODULE];
};


/** 
 * Init transaction as UAC.
 * @param tsx the transaction.
 * @param tdata the transmit data.
 * @return PJ_OK if successfull.
 */
PJ_DECL(pj_status_t) pjsip_tsx_init_uac( pjsip_transaction *tsx, 
					 pjsip_tx_data *tdata);

/**
 * Init transaction as UAS.
 * @param tsx the transaction to be initialized.
 * @param rdata the received incoming request.
 * @return PJ_OK if successfull.
 */
PJ_DECL(pj_status_t) pjsip_tsx_init_uas( pjsip_transaction *tsx,
					 pjsip_rx_data *rdata);

/**
 * Process incoming message for this transaction.
 * @param tsx the transaction.
 * @param rdata the incoming message.
 */
PJ_DECL(void) pjsip_tsx_on_rx_msg( pjsip_transaction *tsx,
				   pjsip_rx_data *rdata);

/**
 * Transmit message with this transaction.
 * @param tsx the transaction.
 * @param tdata the outgoing message.
 */
PJ_DECL(void) pjsip_tsx_on_tx_msg( pjsip_transaction *tsx,
				   pjsip_tx_data *tdata);


/**
 * Transmit ACK message for 2xx/INVITE with this transaction. The ACK for
 * non-2xx/INVITE is automatically sent by the transaction.
 * This operation is only valid if the transaction is configured to handle ACK
 * (tsx->handle_ack is non-zero). If this attribute is not set, then the
 * transaction will comply with RFC-3261, i.e. it will set itself to 
 * TERMINATED state when it receives 2xx/INVITE.
 * @param tsx The transaction.
 * @param tdata The ACK request.
 */
PJ_DECL(void) pjsip_tsx_on_tx_ack( pjsip_transaction *tsx,
				   pjsip_tx_data *tdata);

/**
 * Forcely terminate transaction.
 * @param tsx the transaction.
 * @param code the status code to report.
 */
PJ_DECL(void) pjsip_tsx_terminate( pjsip_transaction *tsx,
				   int code );

/**
 * Create transaction key, which is used to match incoming requests 
 * or response (retransmissions) against transactions.
 * @param pool The pool
 * @param key Output key.
 * @param role The role of the transaction.
 * @param method The method to be put as a key. 
 * @param rdata The received data to calculate.
 */
PJ_DECL(void) pjsip_tsx_create_key( pj_pool_t *pool,
				    pj_str_t *key,
				    pjsip_role_e role,
				    const pjsip_method *method,
				    const pjsip_rx_data *rdata );

/**
 * @}
 */

/*
 * Internal.
 */

/*
 * Get the string name for the state.
 */
PJ_DECL(const char *) pjsip_tsx_state_str(pjsip_tsx_state_e state);

/*
 * Get the role name.
 */
PJ_DECL(const char *) pjsip_role_name(pjsip_role_e role);


/* Thread Local Storage ID for transaction lock (initialized by endpoint) */
extern int pjsip_tsx_lock_tls_id;

PJ_END_DECL

#endif	/* __PJSIP_TRANSACT_H__ */

