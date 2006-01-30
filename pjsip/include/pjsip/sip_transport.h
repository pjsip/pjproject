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
#ifndef __PJSIP_SIP_TRANSPORT_H__
#define __PJSIP_SIP_TRANSPORT_H__

/**
 * @file sip_transport.h
 * @brief SIP Transport
 */

#include <pjsip/sip_msg.h>
#include <pjsip/sip_parser.h>
#include <pj/sock.h>
#include <pj/list.h>
#include <pj/ioqueue.h>
#include <pj/timer.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_TRANSPORT SIP Transport
 * @ingroup PJSIP
 *
 * This is the low-level transport layer. Application normally won't need to 
 * use this function, but instead can use transaction or higher layer API to
 * send and receive messages.
 *
 * @{
 */

/*****************************************************************************
 *
 * GENERAL TRANSPORT (NAMES, TYPES, ETC.)
 *
 *****************************************************************************/

/**
 * Flags for SIP transports.
 */
enum pjsip_transport_flags_e
{
    PJSIP_TRANSPORT_RELIABLE	    = 1,    /**< Transport is reliable.	    */
    PJSIP_TRANSPORT_SECURE	    = 2,    /**< Transport is secure.	    */
    PJSIP_TRANSPORT_DATAGRAM	    = 4,    /**< Datagram based transport.  */
};

/**
 * Check if transport tp is reliable.
 */
#define PJSIP_TRANSPORT_IS_RELIABLE(tp)	    \
	    ((tp)->flag & PJSIP_TRANSPORT_RELIABLE)

/**
 * Check if transport tp is secure.
 */
#define PJSIP_TRANSPORT_IS_SECURE(tp)	    \
	    ((tp)->flag & PJSIP_TRANSPORT_SECURE)

/**
 * Get the transport type from the transport name.
 *
 * @param name	    Transport name, such as "TCP", or "UDP".
 *
 * @return	    The transport type, or PJSIP_TRANSPORT_UNSPECIFIED if 
 *		    the name is not recognized as the name of supported 
 *		    transport.
 */
PJ_DECL(pjsip_transport_type_e) 
pjsip_transport_get_type_from_name(const pj_str_t *name);

/**
 * Get the transport type for the specified flags.
 *
 * @param flag	    The transport flag.
 *
 * @return	    Transport type.
 */
PJ_DECL(pjsip_transport_type_e) 
pjsip_transport_get_type_from_flag(unsigned flag);

/**
 * Get transport flag from type.
 *
 * @param type	    Transport type.
 *
 * @return	    Transport flags.
 */
PJ_DECL(unsigned)
pjsip_transport_get_flag_from_type( pjsip_transport_type_e type );

/**
 * Get the default SIP port number for the specified type.
 *
 * @param type	    Transport type.
 *
 * @return	    The port number, which is the default SIP port number for
 *		    the specified type.
 */
PJ_DECL(int) 
pjsip_transport_get_default_port_for_type(pjsip_transport_type_e type);

/**
 * Get transport type name.
 *
 * @param t	    Transport type.
 *
 * @return	    Transport name.
 */
PJ_DECL(const char*) pjsip_transport_get_type_name(pjsip_transport_type_e t);


/*****************************************************************************
 *
 * RECEIVE DATA BUFFER.
 *
 *****************************************************************************/

/** 
 * A customized ioqueue async operation key which is used by transport
 * to locate rdata when a pending read operation completes.
 */
typedef struct pjsip_rx_data_op_key
{
    pj_ioqueue_op_key_t		op_key;
    pjsip_rx_data	       *rdata;
} pjsip_rx_data_op_key;


/**
 * Incoming message buffer.
 * This structure keep all the information regarding the received message. This
 * buffer lifetime is only very short, normally after the transaction has been
 * called, this buffer will be deleted/recycled. So care must be taken when
 * allocating storage from the pool of this buffer.
 */
struct pjsip_rx_data
{

    /**
     * tp_info is part of rdata that remains static for the duration of the
     * buffer. It is initialized when the buffer was created by transport.
     */
    struct 
    {
	/** Memory pool for this buffer. */
	pj_pool_t		*pool;

	/** The transport object which received this packet. */
	pjsip_transport		*transport;

	/** Ioqueue key. */
	pjsip_rx_data_op_key	 op_key;

    } tp_info;


    /**
     * pkt_info is initialized by transport when it receives an incoming
     * packet.
     */
    struct
    {
	/** Time when the message was received. */
	pj_time_val		 timestamp;

	/** Pointer to the original packet. */
	char			 packet[PJSIP_MAX_PKT_LEN];

	/** Zero termination for the packet. */
	pj_uint32_t		 zero;

	/** The length of the packet received. */
	pj_ssize_t		 len;

	/** The source address from which the packet was received. */
	pj_sockaddr		 src_addr;

	/** The length of the source address. */
	int			 src_addr_len;

	/** The IP source address string (NULL terminated). */
	char			 src_name[16];

	/** The IP source port number. */
	int			 src_port;

    } pkt_info;


    /**
     * msg_info is initialized by transport mgr (tpmgr) before this buffer
     * is passed to endpoint.
     */
    struct
    {
	/** Start of msg buffer. */
	char			*msg_buf;

	/** Length fo message. */
	int			 len;

	/** The parsed message, if any. */
	pjsip_msg		*msg;

	/** Short description about the message. 
	 *  Application should use #pjsip_rx_data_get_info() instead.
	 */
	char			*info;

	/** The Call-ID header as found in the message. */
	pjsip_cid_hdr		*cid;

	/** The From header as found in the message. */
	pjsip_from_hdr		*from;

	/** The To header as found in the message. */
	pjsip_to_hdr		*to;

	/** The topmost Via header as found in the message. */
	pjsip_via_hdr		*via;

	/** The CSeq header as found in the message. */
	pjsip_cseq_hdr		*cseq;

	/** Max forwards header. */
	pjsip_max_fwd_hdr	*max_fwd;

	/** The first route header. */
	pjsip_route_hdr		*route;

	/** The first record-route header. */
	pjsip_rr_hdr		*record_route;

	/** Content-type header. */
	pjsip_ctype_hdr		*ctype;

	/** Content-length header. */
	pjsip_clen_hdr		*clen;

	/** The first Require header. */
	pjsip_require_hdr	*require;

	/** The list of error generated by the parser when parsing 
	    this message. 
	 */
	pjsip_parser_err_report parse_err;

    } msg_info;


    /**
     * endpt_info is initialized by endpoint after this buffer reaches
     * endpoint.
     */
    struct
    {
	/** 
	 * Data attached by modules to this message. 
	 */
	void	*mod_data[PJSIP_MAX_MODULE];

    } endpt_info;

};

/**
 * Get printable information about the message in the rdata.
 *
 * @param rdata	    The receive data buffer.
 *
 * @return	    Printable information.
 */
PJ_DECL(char*) pjsip_rx_data_get_info(pjsip_rx_data *rdata);


/*****************************************************************************
 *
 * TRANSMIT DATA BUFFER MANIPULATION.
 *
 *****************************************************************************/

/** Customized ioqueue async operation key, used by transport to keep
 *  callback parameters.
 */
typedef struct pjsip_tx_data_op_key
{
    pj_ioqueue_op_key_t	    key;
    pjsip_tx_data	   *tdata;
    void		   *token;
    void		  (*callback)(pjsip_transport*,void*,pj_ssize_t);
} pjsip_tx_data_op_key;


/**
 * Data structure for sending outgoing message. Application normally creates
 * this buffer by calling #pjsip_endpt_create_tdata.
 *
 * The lifetime of this buffer is controlled by the reference counter in this
 * structure, which is manipulated by calling #pjsip_tx_data_add_ref and
 * #pjsip_tx_data_dec_ref. When the reference counter has reached zero, then
 * this buffer will be destroyed.
 *
 * A transaction object normally will add reference counter to this buffer
 * when application calls #pjsip_tsx_on_tx_msg, because it needs to keep the
 * message for retransmission. The transaction will release the reference
 * counter once its state has reached final state.
 */
struct pjsip_tx_data
{
    /** This is for transmission queue; it's managed by transports. */
    PJ_DECL_LIST_MEMBER(struct pjsip_tx_data);

    /** Memory pool for this buffer. */
    pj_pool_t		*pool;

    /** A name to identify this buffer. */
    char		 obj_name[PJ_MAX_OBJ_NAME];

    /** Short information describing this buffer and the message in it. 
     *  Application should use #pjsip_tx_data_get_info() instead of
     *  directly accessing this member.
     */
    char		*info;

    /** For response message, this contains the reference to timestamp when 
     *  the original request message was received. The value of this field
     *  is set when application creates response message to a request by
     *  calling #pjsip_endpt_create_response.
     */
    pj_time_val		 rx_timestamp;

    /** The transport manager for this buffer. */
    pjsip_tpmgr		*mgr;

    /** Ioqueue asynchronous operation key. */
    pjsip_tx_data_op_key op_key;

    /** Lock object. */
    pj_lock_t		*lock;

    /** The message in this buffer. */
    pjsip_msg 		*msg;

    /** Buffer to the printed text representation of the message. When the
     *  content of this buffer is set, then the transport will send the content
     *  of this buffer instead of re-printing the message structure. If the
     *  message structure has changed, then application must invalidate this
     *  buffer by calling #pjsip_tx_data_invalidate_msg.
     */
    pjsip_buffer	 buf;

    /** Reference counter. */
    pj_atomic_t		*ref_cnt;

    /** Being processed by transport? */
    int			 is_pending;

    /** Transport manager internal. */
    void		*token;
    void	       (*cb)(void*, pjsip_tx_data*, pj_ssize_t);

    /** Transport information, only valid during on_tx_request() and 
     *  on_tx_response() callback.
     */
    struct
    {
	pjsip_transport	    *transport;	    /**< Transport being used.	*/
	pj_sockaddr	     dst_addr;	    /**< Destination address.	*/
	int		     dst_addr_len;  /**< Length of address.	*/
	char		     dst_name[16];  /**< Destination address.	*/
	int		     dst_port;	    /**< Destination port.	*/
    } tp_info;
};


/**
 * Create a new, blank transmit buffer. The reference count is initialized
 * to zero.
 *
 * @param mgr		The transport manager.
 * @param tdata		Pointer to receive transmit data.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 *
 * @see pjsip_endpt_create_tdata
 */
pj_status_t pjsip_tx_data_create( pjsip_tpmgr *mgr,
                                  pjsip_tx_data **tdata );

/**
 * Add reference counter to the transmit buffer. The reference counter controls
 * the life time of the buffer, ie. when the counter reaches zero, then it 
 * will be destroyed.
 *
 * @param tdata	    The transmit buffer.
 */
PJ_DECL(void) pjsip_tx_data_add_ref( pjsip_tx_data *tdata );

/**
 * Decrement reference counter of the transmit buffer.
 * When the transmit buffer is no longer used, it will be destroyed and
 * caller is informed with PJSIP_EBUFDESTROYED return status.
 *
 * @param tdata	    The transmit buffer data.
 * @return	    This function will always succeeded eventhough the return
 *		    status is non-zero. A status PJSIP_EBUFDESTROYED will be
 *		    returned to inform that buffer is destroyed.
 */
PJ_DECL(pj_status_t) pjsip_tx_data_dec_ref( pjsip_tx_data *tdata );

/**
 * Check if transmit data buffer contains a valid message.
 *
 * @param tdata	    The transmit buffer.
 * @return	    Non-zero (PJ_TRUE) if buffer contains a valid message.
 */
PJ_DECL(pj_bool_t) pjsip_tx_data_is_valid( pjsip_tx_data *tdata );

/**
 * Invalidate the print buffer to force message to be re-printed. Call
 * when the message has changed after it has been printed to buffer. The
 * message is printed to buffer normally by transport when it is about to be 
 * sent to the wire. Subsequent sending of the message will not cause
 * the message to be re-printed, unless application invalidates the buffer
 * by calling this function.
 *
 * @param tdata	    The transmit buffer.
 */
PJ_DECL(void) pjsip_tx_data_invalidate_msg( pjsip_tx_data *tdata );

/**
 * Get short printable info about the transmit data. This will normally return
 * short information about the message.
 *
 * @param tdata	    The transmit buffer.
 *
 * @return	    Null terminated info string.
 */
PJ_DECL(char*) pjsip_tx_data_get_info( pjsip_tx_data *tdata );


/*****************************************************************************
 *
 * TRANSPORT
 *
 *****************************************************************************/

/**
 * This structure represent the "public" interface of a SIP transport.
 * Applications normally extend this structure to include transport
 * specific members.
 */
typedef struct pjsip_transport
{
    char		    obj_name[PJ_MAX_OBJ_NAME];	/**< Name. */

    pj_pool_t		   *pool;	    /**< Pool used by transport.    */
    pj_atomic_t		   *ref_cnt;	    /**< Reference counter.	    */
    pj_lock_t		   *lock;	    /**< Lock object.		    */
    int			    tracing;	    /**< Tracing enabled?	    */

    /** Key for indexing this transport in hash table. */
    struct {
	pjsip_transport_type_e  type;	    /**< Transport type.	    */
	pj_sockaddr		rem_addr;   /**< Remote addr (zero for UDP) */
    } key;

    char		   *type_name;	    /**< Type name.		    */
    unsigned		    flag;	    /**< #pjsip_transport_flags_e   */
    char		   *info;	    /**< Transport info/description.*/

    int			    addr_len;	    /**< Length of addresses.	    */
    pj_sockaddr		    local_addr;	    /**< Bound address.		    */
    pjsip_host_port	    local_name;	    /**< Published name (eg. STUN). */
    pjsip_host_port	    remote_name;    /**< Remote address name.	    */
    
    pjsip_endpoint	   *endpt;	    /**< Endpoint instance.	    */
    pjsip_tpmgr		   *tpmgr;	    /**< Transport manager.	    */
    pj_timer_entry	    idle_timer;	    /**< Timer when ref cnt is zero.*/

    /**
     * Function to be called by transport manager to send SIP message.
     *
     * @param transport	    The transport to send the message.
     * @param packet	    The buffer to send.
     * @param length	    The length of the buffer to send.
     * @param op_key	    Completion token, which will be supplied to
     *			    caller when pending send operation completes.
     * @param rem_addr	    The remote destination address.
     * @param addr_len	    Size of remote address.
     * @param callback	    If supplied, the callback will be called
     *			    once a pending transmission has completed. If
     *			    the function completes immediately (i.e. return
     *			    code is not PJ_EPENDING), the callback will not
     *			    be called.
     *
     * @return		    Should return PJ_SUCCESS only if data has been
     *			    succesfully queued to operating system for 
     *			    transmission. Otherwise it may return PJ_EPENDING
     *			    if the underlying transport can not send the
     *			    data immediately and will send it later, which in
     *			    this case caller doesn't have to do anything 
     *			    except wait the calback to be called, if it 
     *			    supplies one.
     *			    Other return values indicate the error code.
     */
    pj_status_t (*send_msg)(pjsip_transport *transport, 
			    pjsip_tx_data *tdata,
			    const pj_sockaddr_t *rem_addr,
			    int addr_len,
			    void *token,
			    void (*callback)(pjsip_transport *transport,
					     void *token, 
					     pj_ssize_t sent_bytes));

    /**
     * Destroy this transport.
     */
    pj_status_t (*destroy)(pjsip_transport *transport);

    /*
     * Application may extend this structure..
     */
} pjsip_transport;


/**
 * Register a transport.
 */
PJ_DECL(pj_status_t) pjsip_transport_register( pjsip_tpmgr *mgr,
					       pjsip_transport *tp );


/**
 * Unregister transport. This will eventually call the transport to
 * destroy itself.
 */
PJ_DECL(pj_status_t) pjsip_transport_unregister( pjsip_tpmgr *mgr,
						 pjsip_transport *tp);

/**
 * Add ref.
 */
PJ_DECL(pj_status_t) pjsip_transport_add_ref( pjsip_transport *tp );

/**
 * Dec ref.
 */
PJ_DECL(pj_status_t) pjsip_transport_dec_ref( pjsip_transport *tp );


/**
 * Call for incoming message.
 */
PJ_DECL(pj_ssize_t) pjsip_tpmgr_receive_packet(pjsip_tpmgr *mgr,
					       pjsip_rx_data *rdata);


/*****************************************************************************
 *
 * TRANSPORT FACTORY
 *
 *****************************************************************************/


/**
 * Transport factory.
 */
typedef struct pjsip_tpfactory pjsip_tpfactory;

/**
 * Transport factory.
 */
struct pjsip_tpfactory
{
    /* This list is managed by transport manager. */
    PJ_DECL_LIST_MEMBER(struct pjsip_tpfactory);

    pj_pool_t		   *pool;
    pj_lock_t		   *lock;

    pjsip_transport_type_e  type;
    char		    type_name[8];
    unsigned		    flag;

    pj_sockaddr		    local_addr;
    pjsip_host_port	    addr_name;

    /**
     * Create new outbound connection.
     * Note that the factory is responsible for both creating the
     * transport and registering it to the transport manager.
     */
    pj_status_t (*create_transport)(pjsip_tpfactory *factory,
				    pjsip_tpmgr *mgr,
				    pjsip_endpoint *endpt,
				    const pj_sockaddr *rem_addr,
				    int addr_len,
				    pjsip_transport **transport);

    /*
     * Application may extend this structure..
     */
};



/**
 * Register a transport factory.
 *
 * @param mgr		The transport manager.
 * @param factory	Transport factory.
 *
 * @return		PJ_SUCCESS if listener was successfully created.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_register_tpfactory(pjsip_tpmgr *mgr,
						    pjsip_tpfactory *tpf);

/**
 * Unregister factory.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_unregister_tpfactory(pjsip_tpmgr *mgr,
						      pjsip_tpfactory *tpf);


/*****************************************************************************
 *
 * TRANSPORT MANAGER
 *
 *****************************************************************************/

/**
 * Create a new transport manager.
 *
 * @param pool	    Pool.
 * @param endpt	    Endpoint instance.
 * @param rx_cb	    Callback to receive incoming message.
 * @param tx_cb	    Callback to be called before transport manager is sending
 *		    outgoing message.
 * @param p_mgr	    Pointer to receive the new transport manager.
 *
 * @return	    PJ_SUCCESS or the appropriate error code on error.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_create( pj_pool_t *pool,
					 pjsip_endpoint * endpt,
					 void (*rx_cb)(pjsip_endpoint*,
						       pj_status_t,
						       pjsip_rx_data *),
					 pj_status_t (*tx_cb)(pjsip_endpoint*,
							      pjsip_tx_data*),
					 pjsip_tpmgr **p_mgr);


/**
 * Destroy transport manager.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_destroy(pjsip_tpmgr *mgr);


/**
 * Dump transport info.
 */
PJ_DECL(void) pjsip_tpmgr_dump_transports(pjsip_tpmgr *mgr);


/*****************************************************************************
 *
 * PUBLIC API
 *
 *****************************************************************************/


/**
 * Find transport to be used to send message to remote destination. If no
 * suitable transport is found, a new one will be created.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_acquire_transport(pjsip_tpmgr *mgr,
						   pjsip_transport_type_e type,
						   const pj_sockaddr_t *remote,
						   int addr_len,
						   pjsip_transport **tp);


/**
 * Send a SIP message using the specified transport.
 */
PJ_DECL(pj_status_t) pjsip_transport_send( pjsip_transport *tr, 
					   pjsip_tx_data *tdata,
					   const pj_sockaddr_t *addr,
					   int addr_len,
					   void *token,
					   void (*cb)(void *token, 
						      pjsip_tx_data *tdata,
						      pj_ssize_t bytes_sent));


/**
 * @}
 */


PJ_END_DECL

#endif	/* __PJSIP_SIP_TRANSPORT_H__ */

