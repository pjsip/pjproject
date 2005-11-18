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
#ifndef __PJSIP_SIP_TRANSACTION_H__
#define __PJSIP_SIP_TRANSACTION_H__

/**
 * @file sip_transaction.h
 * @brief SIP Transaction
 */

#include <pjsip/sip_msg.h>
#include <pjsip/sip_resolve.h>
#include <pj/timer.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_TRANSACT SIP Transaction
 * @ingroup PJSIP
 * @{
 */

/* Forward decl. */
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
    /*
     * Administrivia
     */
    pj_pool_t		       *pool;           /**< Pool owned by the tsx. */
    pjsip_endpoint	       *endpt;          /**< Endpoint instance.     */
    pj_mutex_t		       *mutex;          /**< Mutex for this tsx.    */
    char			obj_name[PJ_MAX_OBJ_NAME];  /**< Tsx name.  */
    int                         tracing;        /**< Tracing enabled?       */

    /*
     * Transaction identification.
     */
    pjsip_role_e		role;           /**< Role (UAS or UAC)      */
    pjsip_method		method;         /**< The method.            */
    int				cseq;           /**< The CSeq               */
    pj_str_t			transaction_key;/**< hash table key.        */
    pj_str_t			branch;         /**< The branch Id.         */

    /*
     * State and status.
     */
    int				status_code;    /**< Last status code seen. */
    pjsip_tsx_state_e		state;          /**< State.                 */
    int				handle_ack;     /**< Should we handle ACK?  */

    /** Handler according to current state. */
    pj_status_t (*state_handler)(struct pjsip_transaction *, pjsip_event *);

    /*
     * Transport.
     */
    pjsip_tsx_transport_state_e	transport_state;/**< Transport's state.     */
    pjsip_host_port		dest_name;      /**< Destination address.   */
    pjsip_server_addresses	remote_addr;    /**< Addresses resolved.    */
    int				current_addr;   /**< Address currently used. */

    pjsip_transport_t	       *transport;      /**< Transport to use.      */

    /*
     * Messages and timer.
     */
    pjsip_tx_data	       *last_tx;        /**< Msg kept for retrans.  */
    int				has_unsent_msg; /**< Non-zero if tsx need to 
                                                     transmit msg once resolver
                                                     completes.             */
    int				retransmit_count;/**< Retransmission count. */
    pj_timer_entry		retransmit_timer;/**< Retransmit timer.     */
    pj_timer_entry		timeout_timer;  /**< Timeout timer.         */

    /** Module specific data. */
    void		       *module_data[PJSIP_MAX_MODULE];
};


/** 
 * Init transaction as UAC from the specified transmit data (\c tdata).
 * The transmit data must have a valid \c Request-Line and \c CSeq header.
 * If \c Route headers are present, it will be used to calculate remote
 * destination.
 *
 * If \c Via header does not exist, it will be created along with a unique
 * \c branch parameter. If it exists and contains branch parameter, then
 * the \c branch parameter will be used as is as the transaction key.
 *
 * The \c Route headers in the transmit data, if present, are used to 
 * calculate remote destination.
 *
 * At the end of the function, the transaction will start resolving the
 * addresses of remote server to contact. Transport will be acquired as soon
 * as the resolving job completes.
 *
 * @param tsx       The transaction.
 * @param tdata     The transmit data.
 *
 * @return          PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjsip_tsx_init_uac( pjsip_transaction *tsx, 
					 pjsip_tx_data *tdata);

/**
 * Init transaction as UAS.
 *
 * @param tsx       The transaction to be initialized.
 * @param rdata     The received incoming request.
 *
 * @return PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjsip_tsx_init_uas( pjsip_transaction *tsx,
					 pjsip_rx_data *rdata);

/**
 * Process incoming message for this transaction.
 *
 * @param tsx       The transaction.
 * @param rdata     The incoming message.
 */
PJ_DECL(void) pjsip_tsx_on_rx_msg( pjsip_transaction *tsx,
				   pjsip_rx_data *rdata);

/**
 * Transmit message with this transaction.
 *
 * @param tsx       The transaction.
 * @param tdata     The outgoing message.
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
 *
 * @param tsx       The transaction.
 * @param tdata     The ACK request.
 */
PJ_DECL(void) pjsip_tsx_on_tx_ack( pjsip_transaction *tsx,
				   pjsip_tx_data *tdata);

/**
 * Force terminate transaction.
 *
 * @param tsx       The transaction.
 * @param code      The status code to report.
 */
PJ_DECL(void) pjsip_tsx_terminate( pjsip_transaction *tsx,
				   int code );

/**
 * Create transaction key, which is used to match incoming requests 
 * or response (retransmissions) against transactions.
 *
 * @param pool      The pool
 * @param key       Output key.
 * @param role      The role of the transaction.
 * @param method    The method to be put as a key. 
 * @param rdata     The received data to calculate.
 *
 * @return          PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tsx_create_key( pj_pool_t *pool,
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
extern long pjsip_tsx_lock_tls_id;

PJ_END_DECL

#endif	/* __PJSIP_TRANSACT_H__ */

