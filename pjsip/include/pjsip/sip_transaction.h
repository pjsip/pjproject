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
#include <pjsip/sip_util.h>
#include <pj/timer.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_TRANSACT SIP Transaction
 * @ingroup PJSIP
 * @{
 */

/**
 * This enumeration represents transaction state.
 */
typedef enum pjsip_tsx_state_e
{
    PJSIP_TSX_STATE_NULL,	/**< For UAC, before any message is sent.   */
    PJSIP_TSX_STATE_CALLING,	/**< For UAC, just after request is sent.   */
    PJSIP_TSX_STATE_TRYING,	/**< For UAS, just after request is received.*/
    PJSIP_TSX_STATE_PROCEEDING,	/**< For UAS/UAC, after provisional response.*/
    PJSIP_TSX_STATE_COMPLETED,	/**< For UAS/UAC, after final response.	    */
    PJSIP_TSX_STATE_CONFIRMED,	/**< For UAS, after ACK is received.	    */
    PJSIP_TSX_STATE_TERMINATED,	/**< For UAS/UAC, before it's destroyed.    */
    PJSIP_TSX_STATE_DESTROYED,	/**< For UAS/UAC, will be destroyed now.    */
    PJSIP_TSX_STATE_MAX,	/**< Number of states.			    */
} pjsip_tsx_state_e;


/**
 * This structure describes SIP transaction object. The transaction object
 * is used to handle both UAS and UAC transaction.
 */
struct pjsip_transaction
{
    /*
     * Administrivia
     */
    pj_pool_t		       *pool;           /**< Pool owned by the tsx. */
    pjsip_module	       *tsx_user;	/**< Transaction user.	    */
    pjsip_endpoint	       *endpt;          /**< Endpoint instance.     */
    pj_mutex_t		       *mutex;          /**< Mutex for this tsx.    */

    /*
     * Transaction identification.
     */
    char			obj_name[PJ_MAX_OBJ_NAME];  /**< Log info.  */
    pjsip_role_e		role;           /**< Role (UAS or UAC)      */
    pjsip_method		method;         /**< The method.            */
    int				cseq;           /**< The CSeq               */
    pj_str_t			transaction_key;/**< Hash table key.        */
    pj_uint32_t			hashed_key;	/**< Key's hashed value.    */
    pj_str_t			branch;         /**< The branch Id.         */

    /*
     * State and status.
     */
    int				status_code;    /**< Last status code seen. */
    pjsip_tsx_state_e		state;          /**< State.                 */
    int				handle_200resp; /**< UAS 200/INVITE  retrsm.*/
    int                         tracing;        /**< Tracing enabled?       */

    /** Handler according to current state. */
    pj_status_t (*state_handler)(struct pjsip_transaction *, pjsip_event *);

    /*
     * Transport.
     */
    pjsip_transport	       *transport;      /**< Transport to use.      */
    pj_bool_t			is_reliable;	/**< Transport is reliable. */
    pj_sockaddr			addr;		/**< Destination address.   */
    int				addr_len;	/**< Address length.	    */
    pjsip_response_addr		res_addr;	/**< Response address.	    */
    unsigned			transport_flag;	/**< Miscelaneous flag.	    */

    /*
     * Messages and timer.
     */
    pjsip_tx_data	       *last_tx;        /**< Msg kept for retrans.  */
    int				retransmit_count;/**< Retransmission count. */
    pj_timer_entry		retransmit_timer;/**< Retransmit timer.     */
    pj_timer_entry		timeout_timer;  /**< Timeout timer.         */

    /** Module specific data. */
    void		       *mod_data[PJSIP_MAX_MODULE];
};


/**
 * Create and register transaction layer module to the specified endpoint.
 *
 * @param endpt	    The endpoint instance.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_tsx_layer_init(pjsip_endpoint *endpt);

/**
 * Get the instance of the transaction layer module.
 *
 * @return	    The transaction layer module.
 */
PJ_DECL(pjsip_module*) pjsip_tsx_layer_instance(void);

/**
 * Unregister and destroy transaction layer module.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_tsx_layer_destroy(void);

/**
 * Find a transaction with the specified key. The transaction key normally
 * is created by calling #pjsip_tsx_create_key() from an incoming message.
 *
 * @param key	    The key string to find the transaction.
 * @param lock	    If non-zero, transaction will be locked before the
 *		    function returns, to make sure that it's not deleted
 *		    by other threads.
 *
 * @return	    The matching transaction instance, or NULL if transaction
 *		    can not be found.
 */
PJ_DECL(pjsip_transaction*) pjsip_tsx_layer_find_tsx( const pj_str_t *key,
						      pj_bool_t lock );

/**
 * Create, initialize, and register a new transaction as UAC from the 
 * specified transmit data (\c tdata). The transmit data must have a valid
 * \c Request-Line and \c CSeq header. 
 *
 * If \c Via header does not exist, it will be created along with a unique
 * \c branch parameter. If it exists and contains branch parameter, then
 * the \c branch parameter will be used as is as the transaction key. If
 * it exists but branch parameter doesn't exist, a unique branch parameter
 * will be created.
 *
 * @param tsx_user  Module to be registered as transaction user of the new
 *		    transaction, which will receive notification from the
 *		    transaction via on_tsx_state() callback.
 * @param tdata     The outgoing request message.
 * @param p_tsx	    On return will contain the new transaction instance.
 *
 * @return          PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjsip_tsx_create_uac( pjsip_module *tsx_user,
					   pjsip_tx_data *tdata,
					   pjsip_transaction **p_tsx);

/**
 * Create, initialize, and register a new transaction as UAS from the
 * specified incoming request in \c rdata.
 *
 * @param tsx_user  Module to be registered as transaction user of the new
 *		    transaction, which will receive notification from the
 *		    transaction via on_tsx_state() callback.
 * @param rdata     The received incoming request.
 * @param p_tsx	    On return will contain the new transaction instance.
 *
 * @return	    PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjsip_tsx_create_uas( pjsip_module *tsx_user,
					   pjsip_rx_data *rdata,
					   pjsip_transaction **p_tsx );

/**
 * Transmit message in tdata with this transaction. It is possible to
 * pass NULL in tdata for UAC transaction, which in this case the last 
 * message transmitted, or the request message which was specified when
 * calling #pjsip_tsx_create_uac(), will be sent.
 *
 * This function decrements the reference counter of the transmit buffer
 * only when it returns PJ_SUCCESS;
 *
 * @param tsx       The transaction.
 * @param tdata     The outgoing message. If NULL is specified, then the
 *		    last message transmitted (or the message specified 
 *		    in UAC initialization) will be sent.
 *
 * @return	    PJ_SUCCESS if successfull.
 */
PJ_DECL(pj_status_t) pjsip_tsx_send_msg( pjsip_transaction *tsx,
					 pjsip_tx_data *tdata);


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
 * Force terminate transaction.
 *
 * @param tsx       The transaction.
 * @param code      The status code to report.
 */
PJ_DECL(pj_status_t) pjsip_tsx_terminate( pjsip_transaction *tsx,
					  int st_code );


/**
 * Get the transaction instance in the incoming message. If the message
 * has a corresponding transaction, this function will return non NULL
 * value.
 *
 * @param rdata	    The incoming message buffer.
 *
 * @return	    The transaction instance associated with this message,
 *		    or NULL if the message doesn't match any transactions.
 */
PJ_DECL(pjsip_transaction*) pjsip_rdata_get_tsx( pjsip_rx_data *rdata );


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


PJ_END_DECL

#endif	/* __PJSIP_TRANSACT_H__ */

