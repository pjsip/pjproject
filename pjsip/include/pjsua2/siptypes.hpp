/* $Id$ */
/*
 * Copyright (C) 2032 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJSUA2_SIPTYPES_HPP__
#define __PJSUA2_SIPTYPES_HPP__

/**
 * @file pjsua2/types.hpp
 * @brief PJSUA2 Base Types
 */
#include <pjsua2/types.hpp>
#include <pjsua2/persistent.hpp>

#include <string>
#include <vector>

/** PJSUA2 API is inside pj namespace */
namespace pj
{

/**
 * @defgroup PJSUA2_SIP_Types SIP Types
 * @ingroup PJSUA2_DS
 * @{
 */

/**
 * Credential information. Credential contains information to authenticate
 * against a service.
 */
struct AuthCredInfo : public PersistentObject
{
    /**
     * The authentication scheme (e.g. "digest").
     */
    string	scheme;

    /**
     * Realm on which this credential is to be used. Use "*" to make
     * a credential that can be used to authenticate against any challenges.
     */
    string	realm;

    /**
     * Authentication user name.
     */
    string	username;

    /**
     * Type of data that is contained in the "data" field. Use 0 if the data
     * contains plain text password.
     */
    int		dataType;

    /**
     * The data, which can be a plain text password or a hashed digest.
     */
    string	data;

    /*
     * Digest AKA credential information. Note that when AKA credential
     * is being used, the \a data field of this pjsip_cred_info is
     * not used, but it still must be initialized to an empty string.
     * Please see PJSIP_AUTH_AKA_API for more information.
     */

    /** Permanent subscriber key. */
    string	akaK;

    /** Operator variant key. */
    string	akaOp;

    /** Authentication Management Field	*/
    string	akaAmf;

public:
    /** Default constructor */
    AuthCredInfo();

    /** Construct a credential with the specified parameters */
    AuthCredInfo(const string &scheme,
                 const string &realm,
                 const string &user_name,
                 const int data_type,
                 const string data);

    /**
     * Convert from pjsip
     */
    void fromPj(const pjsip_cred_info &prm);

    /**
     * Convert to pjsip
     */
    pjsip_cred_info toPj() const;

    /**
     * Read this object from a container node.
     *
     * @param node		Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};


//////////////////////////////////////////////////////////////////////////////

/**
 * TLS transport settings, to be specified in TransportConfig.
 */
struct TlsConfig : public PersistentObject
{
    /**
     * Certificate of Authority (CA) list file.
     */
    string		CaListFile;

    /**
     * Public endpoint certificate file, which will be used as client-
     * side  certificate for outgoing TLS connection, and server-side
     * certificate for incoming TLS connection.
     */
    string		certFile;

    /**
     * Optional private key of the endpoint certificate to be used.
     */
    string		privKeyFile;

    /**
     * Password to open private key.
     */
    string		password;

    /**
     * Certificate of Authority (CA) buffer. If CaListFile, certFile or
     * privKeyFile are set, this setting will be ignored.
     */
    string		CaBuf;

    /**
     * Public endpoint certificate buffer, which will be used as client-
     * side  certificate for outgoing TLS connection, and server-side
     * certificate for incoming TLS connection. If CaListFile, certFile or
     * privKeyFile are set, this setting will be ignored.
     */
    string		certBuf;

    /**
     * Optional private key buffer of the endpoint certificate to be used. 
     * If CaListFile, certFile or privKeyFile are set, this setting will 
     * be ignored.
     */
    string		privKeyBuf;

    /**
     * TLS protocol method from #pjsip_ssl_method. In the future, this field
     * might be deprecated in favor of <b>proto</b> field. For now, this field 
     * is only applicable only when <b>proto</b> field is set to zero.
     *
     * Default is PJSIP_SSL_UNSPECIFIED_METHOD (0), which in turn will
     * use PJSIP_SSL_DEFAULT_METHOD, which default value is PJSIP_TLSV1_METHOD.
     */
    pjsip_ssl_method	method;

    /**
     * TLS protocol type from #pj_ssl_sock_proto. Use this field to enable 
     * specific protocol type. Use bitwise OR operation to combine the protocol 
     * type.
     *
     * Default is PJSIP_SSL_DEFAULT_PROTO.
     */
    unsigned		proto;

    /**
     * Ciphers and order preference. The Endpoint::utilSslGetAvailableCiphers()
     * can be used to check the available ciphers supported by backend.
     * If the array is empty, then default cipher list of the backend
     * will be used.
     */
    IntVector		ciphers;

    /**
     * Specifies TLS transport behavior on the server TLS certificate
     * verification result:
     * - If \a verifyServer is disabled, TLS transport will just notify
     *   the application via pjsip_tp_state_callback with state
     *   PJSIP_TP_STATE_CONNECTED regardless TLS verification result.
     * - If \a verifyServer is enabled, TLS transport will be shutdown
     *   and application will be notified with state
     *   PJSIP_TP_STATE_DISCONNECTED whenever there is any TLS verification
     *   error, otherwise PJSIP_TP_STATE_CONNECTED will be notified.
     *
     * In any cases, application can inspect pjsip_tls_state_info in the
     * callback to see the verification detail.
     *
     * Default value is false.
     */
    bool		verifyServer;

    /**
     * Specifies TLS transport behavior on the client TLS certificate
     * verification result:
     * - If \a verifyClient is disabled, TLS transport will just notify
     *   the application via pjsip_tp_state_callback with state
     *   PJSIP_TP_STATE_CONNECTED regardless TLS verification result.
     * - If \a verifyClient is enabled, TLS transport will be shutdown
     *   and application will be notified with state
     *   PJSIP_TP_STATE_DISCONNECTED whenever there is any TLS verification
     *   error, otherwise PJSIP_TP_STATE_CONNECTED will be notified.
     *
     * In any cases, application can inspect pjsip_tls_state_info in the
     * callback to see the verification detail.
     *
     * Default value is PJ_FALSE.
     */
    bool		verifyClient;

    /**
     * When acting as server (incoming TLS connections), reject incoming
     * connection if client doesn't supply a TLS certificate.
     *
     * This setting corresponds to SSL_VERIFY_FAIL_IF_NO_PEER_CERT flag.
     * Default value is PJ_FALSE.
     */
    bool		requireClientCert;

    /**
     * TLS negotiation timeout to be applied for both outgoing and incoming
     * connection, in milliseconds. If zero, the SSL negotiation doesn't
     * have a timeout.
     *
     * Default: zero
     */
    unsigned		msecTimeout;

    /**
     * QoS traffic type to be set on this transport. When application wants
     * to apply QoS tagging to the transport, it's preferable to set this
     * field rather than \a qosParam fields since this is more portable.
     *
     * Default value is PJ_QOS_TYPE_BEST_EFFORT.
     */
    pj_qos_type 	qosType;

    /**
     * Set the low level QoS parameters to the transport. This is a lower
     * level operation than setting the \a qosType field and may not be
     * supported on all platforms.
     *
     * By default all settings in this structure are disabled.
     */
    pj_qos_params 	qosParams;

    /**
     * Specify if the transport should ignore any errors when setting the QoS
     * traffic type/parameters.
     *
     * Default: PJ_TRUE
     */
    bool		qosIgnoreError;

public:
    /** Default constructor initialises with default values */
    TlsConfig();

    /** Convert to pjsip */
    pjsip_tls_setting toPj() const;

    /** Convert from pjsip */
    void fromPj(const pjsip_tls_setting &prm);

    /**
     * Read this object from a container node.
     *
     * @param node		Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};


/**
 * Parameters to create a transport instance.
 */
struct TransportConfig : public PersistentObject
{
    /**
     * UDP port number to bind locally. This setting MUST be specified
     * even when default port is desired. If the value is zero, the
     * transport will be bound to any available port, and application
     * can query the port by querying the transport info.
     */
    unsigned		port;

    /**
     * Specify the port range for socket binding, relative to the start
     * port number specified in \a port. Note that this setting is only
     * applicable when the start port number is non zero.
     * 
     * Example: \a port=5000, \a portRange=4
     * - Available ports: 5000, 5001, 5002, 5003, 5004 (SIP transport)
     * 
     * Available ports are in the range of [\a port, \a port + \a portRange]. 
     *
     * Default value is zero.
     */
    unsigned		portRange;

    /**
     * Optional address to advertise as the address of this transport.
     * Application can specify any address or hostname for this field,
     * for example it can point to one of the interface address in the
     * system, or it can point to the public address of a NAT router
     * where port mappings have been configured for the application.
     *
     * Note: this option can be used for both UDP and TCP as well!
     */
    string		publicAddress;

    /**
     * Optional address where the socket should be bound to. This option
     * SHOULD only be used to selectively bind the socket to particular
     * interface (instead of 0.0.0.0), and SHOULD NOT be used to set the
     * published address of a transport (the public_addr field should be
     * used for that purpose).
     *
     * Note that unlike public_addr field, the address (or hostname) here
     * MUST correspond to the actual interface address in the host, since
     * this address will be specified as bind() argument.
     */
    string		boundAddress;

    /**
     * This specifies TLS settings for TLS transport. It is only be used
     * when this transport config is being used to create a SIP TLS
     * transport.
     */
    TlsConfig		tlsConfig;

    /**
     * QoS traffic type to be set on this transport. When application wants
     * to apply QoS tagging to the transport, it's preferable to set this
     * field rather than \a qosParam fields since this is more portable.
     *
     * Default is QoS not set.
     */
    pj_qos_type		qosType;

    /**
     * Set the low level QoS parameters to the transport. This is a lower
     * level operation than setting the \a qosType field and may not be
     * supported on all platforms.
     *
     * Default is QoS not set.
     */
    pj_qos_params	qosParams;

public:
    /** Default constructor initialises with default values */
    TransportConfig();

    /** Convert from pjsip */
    void fromPj(const pjsua_transport_config &prm);

    /** Convert to pjsip */
    pjsua_transport_config toPj() const;

    /**
     * Read this object from a container node.
     *
     * @param node		Container to read values from.
     */
    virtual void readObject(const ContainerNode &node) PJSUA2_THROW(Error);

    /**
     * Write this object to a container node.
     *
     * @param node		Container to write values to.
     */
    virtual void writeObject(ContainerNode &node) const PJSUA2_THROW(Error);
};

/**
 * This structure describes transport information returned by
 * Endpoint::transportGetInfo() function.
 */
struct TransportInfo
{
    /** PJSUA transport identification. */
    TransportId	    	    id;

    /** Transport type. */
    pjsip_transport_type_e  type;

    /** Transport type name. */
    string		    typeName;

    /** Transport string info/description. */
    string		    info;

    /** Transport flags (see pjsip_transport_flags_e). */
    unsigned		    flags;

    /** Local/bound address. */
    SocketAddress	    localAddress;

    /** Published address (or transport address name). */
    SocketAddress	    localName;

    /** Current number of objects currently referencing this transport. */
    unsigned		    usageCount;

public:
    /**
     * Default constructor.
     */
    TransportInfo();

    /** Construct from pjsua_transport_info */
    void fromPj(const pjsua_transport_info &info);
};

//////////////////////////////////////////////////////////////////////////////

/**
 * This structure describes an incoming SIP message. It corresponds to the
 * pjsip_rx_data structure in PJSIP library.
 */
struct SipRxData
{
    /**
     * A short info string describing the request, which normally contains
     * the request method and its CSeq.
     */
    string		info;

    /**
     * The whole message data as a string, containing both the header section
     * and message body section.
     */
    string		wholeMsg;

    /**
     * Source address of the message.
     */
    SocketAddress       srcAddress;

    /**
     * Pointer to original pjsip_rx_data. Only valid when the struct
     * is constructed from PJSIP's pjsip_rx_data.
     */
    void               *pjRxData;

public:
    /**
     * Default constructor.
     */
    SipRxData();

    /**
     * Construct from PJSIP's pjsip_rx_data
     */
    void fromPj(pjsip_rx_data &rdata);
};

/**
 * This structure describes an outgoing SIP message. It corresponds to the
 * pjsip_tx_data structure in PJSIP library.
 */
struct SipTxData
{
    /**
     * A short info string describing the request, which normally contains
     * the request method and its CSeq.
     */
    string		info;
    
    /**
     * The whole message data as a string, containing both the header section
     * and message body section.
     */
    string		wholeMsg;
    
    /**
     * Destination address of the message.
     */
    SocketAddress	dstAddress;
    
    /**
     * Pointer to original pjsip_tx_data. Only valid when the struct
     * is constructed from PJSIP's pjsip_tx_data.
     */
    void               *pjTxData;
    
public:
    /**
     * Default constructor.
     */
    SipTxData();

    /**
     * Construct from PJSIP's pjsip_tx_data
     */
    void fromPj(pjsip_tx_data &tdata);
};

/**
 * This structure describes SIP transaction object. It corresponds to the
 * pjsip_transaction structure in PJSIP library.
 */
struct SipTransaction
{
    /* Transaction identification. */
    pjsip_role_e        role;           /**< Role (UAS or UAC)      */
    string              method;         /**< The method.            */
    
    /* State and status. */
    int			statusCode;     /**< Last status code seen. */
    string		statusText;     /**< Last reason phrase.    */
    pjsip_tsx_state_e	state;          /**< State.                 */
    
    /* Messages and timer. */
    SipTxData           lastTx;         /**< Msg kept for retrans.  */
    
    /* Original pjsip_transaction. */
    void               *pjTransaction;  /**< pjsip_transaction.     */
    
public:
    /**
     * Default constructor.
     */
    SipTransaction();

    /**
     * Construct from PJSIP's pjsip_transaction
     */
    void fromPj(pjsip_transaction &tsx);
};

/**
 * This structure describes timer event.
 */
struct TimerEvent
{
    TimerEntry          entry;          /**< The timer entry.           */
};

/**
 * This structure describes transaction state event source.
 */
struct TsxStateEventSrc
{
    SipRxData       rdata;          /**< The incoming message.      */
    SipTxData       tdata;          /**< The outgoing message.      */
    TimerEntry      timer;          /**< The timer.                 */
    pj_status_t     status;         /**< Transport error status.    */
    GenericData     data;           /**< Generic data.              */

    TsxStateEventSrc() : status() {}
};

/**
 * This structure describes transaction state changed event.
 */
struct TsxStateEvent
{
    TsxStateEventSrc    src;            /**< Event source.              */
    SipTransaction      tsx;            /**< The transaction.           */
    pjsip_tsx_state_e   prevState;      /**< Previous state.            */
    pjsip_event_id_e    type;           /**< Type of event source:
                                         *     - PJSIP_EVENT_TX_MSG
                                         *     - PJSIP_EVENT_RX_MSG,
                                         *     - PJSIP_EVENT_TRANSPORT_ERROR
                                         *     - PJSIP_EVENT_TIMER
                                         *     - PJSIP_EVENT_USER
                                         */

    TsxStateEvent();
};

/**
 * This structure describes message transmission event.
 */
struct TxMsgEvent
{
    SipTxData           tdata;          /**< The transmit data buffer.  */
};

/**
 * This structure describes transmission error event.
 */
struct TxErrorEvent
{
    SipTxData           tdata;          /**< The transmit data.         */
    SipTransaction      tsx;            /**< The transaction.           */
};

/**
 * This structure describes message arrival event.
 */
struct RxMsgEvent
{
    SipRxData           rdata;          /**< The receive data buffer.   */
};

/**
 * This structure describes user event.
 */
struct UserEvent
{
    GenericData         user1;          /**< User data 1.               */
    GenericData         user2;          /**< User data 2.               */
    GenericData         user3;          /**< User data 3.               */
    GenericData         user4;          /**< User data 4.               */
};

/**
 * The event body.
 */
struct SipEventBody
{
    /**
     * Timer event.
     */
    TimerEvent      timer;
    
    /**
     * Transaction state has changed event.
     */
    TsxStateEvent   tsxState;
    
    /**
     * Message transmission event.
     */
    TxMsgEvent      txMsg;
    
    /**
     * Transmission error event.
     */
    TxErrorEvent    txError;
    
    /**
     * Message arrival event.
     */
    RxMsgEvent      rxMsg;
    
    /**
     * User event.
     */
    UserEvent       user;
    
};

/**
 * This structure describe event descriptor to fully identify a SIP event. It
 * corresponds to the pjsip_event structure in PJSIP library.
 */
struct SipEvent
{
    /**
     * The event type, can be any value of \b pjsip_event_id_e.
     */
    pjsip_event_id_e    type;
    
    /**
     * The event body, which fields depends on the event type.
     */
    SipEventBody        body;
    
    /**
     * Pointer to its original pjsip_event. Only valid when the struct is
     * constructed from PJSIP's pjsip_event.
     */
    void               *pjEvent;
    
public:
    /**
     * Default constructor.
     */
    SipEvent();

    /**
     * Construct from PJSIP's pjsip_event
     */
    void fromPj(const pjsip_event &ev);
};

//////////////////////////////////////////////////////////////////////////////

/**
 * SIP media type containing type and subtype. For example, for
 * "application/sdp", the type is "application" and the subtype is "sdp".
 */
struct SipMediaType
{
    /** Media type. */
    string		type;

    /** Media subtype. */
    string		subType;

public:
    /**
     * Construct from PJSIP's pjsip_media_type
     */
    void fromPj(const pjsip_media_type &prm);

    /**
     * Convert to PJSIP's pjsip_media_type.
     */
    pjsip_media_type toPj() const;
};

/**
 * Simple SIP header.
 */
struct SipHeader
{
    /**
     * Header name.
     */
    string		hName;

    /**
     * Header value.
     */
    string		hValue;

public:
    /**
     * Initiaize from PJSIP header.
     */
    void fromPj(const pjsip_hdr *) PJSUA2_THROW(Error);

    /**
     * Convert to PJSIP header.
     */
    pjsip_generic_string_hdr &toPj() const;

private:
    /** Interal buffer for conversion to PJSIP header */
    mutable pjsip_generic_string_hdr	pjHdr;
};


/** Array of strings */
typedef std::vector<SipHeader> SipHeaderVector;

/**
 * This describes each multipart part.
 */
struct SipMultipartPart
{
    /**
     * Optional headers to be put in this multipart part.
     */
    SipHeaderVector	headers;

    /**
     * The MIME type of the body part of this multipart part.
     */
    SipMediaType	contentType;

    /**
     * The body part of tthis multipart part.
     */
    string		body;

public:
    /**
     * Initiaize from PJSIP's pjsip_multipart_part.
     */
    void fromPj(const pjsip_multipart_part &prm) PJSUA2_THROW(Error);

    /**
     * Convert to PJSIP's pjsip_multipart_part.
     */
    pjsip_multipart_part& toPj() const;

private:
    /** Interal buffer for conversion to PJSIP pjsip_multipart_part */
    mutable pjsip_multipart_part	pjMpp;
    mutable pjsip_msg_body		pjMsgBody;
};

/** Array of multipart parts */
typedef std::vector<SipMultipartPart> SipMultipartPartVector;

/**
 * Additional options when sending outgoing SIP message. This corresponds to
 * pjsua_msg_data structure in PJSIP library.
 */
struct SipTxOption
{
    /**
     * Optional remote target URI (i.e. Target header). If empty (""), the
     * target will be set to the remote URI (To header). At the moment this
     * field is only used when sending initial INVITE and MESSAGE requests.
     */
    string                  targetUri;

    /**
     * Additional message headers to be included in the outgoing message.
     */
    SipHeaderVector         headers;

    /**
     * MIME type of the message body, if application specifies the messageBody
     * in this structure.
     */
    string                  contentType;

    /**
     * Optional message body to be added to the message, only when the
     * message doesn't have a body.
     */
    string                  msgBody;

    /**
     * Content type of the multipart body. If application wants to send
     * multipart message bodies, it puts the parts in multipartParts and set
     * the content type in multipartContentType. If the message already
     * contains a body, the body will be added to the multipart bodies.
     */
    SipMediaType            multipartContentType;

    /**
     * Array of multipart parts. If application wants to send multipart
     * message bodies, it puts the parts in \a parts and set the content
     * type in \a multipart_ctype. If the message already contains a body,
     * the body will be added to the multipart bodies.
     */
    SipMultipartPartVector  multipartParts;

public:
    /**
     * Check if the options are empty. If the options are set with empty
     * values, there will be no additional information sent with outgoing
     * SIP message.
     *
     * @return              True if the options are empty.
     */
    bool isEmpty() const;
    
    /**
     * Initiaize from PJSUA's pjsua_msg_data.
     */
    void fromPj(const pjsua_msg_data &prm) PJSUA2_THROW(Error);

    /**
     * Convert to PJSUA's pjsua_msg_data.
     */
    void toPj(pjsua_msg_data &msg_data) const;
};

//////////////////////////////////////////////////////////////////////////////

/**
 * This structure contains parameters for sending instance message methods,
 * e.g: Buddy::sendInstantMessage(), Call:sendInstantMessage().
 */
struct SendInstantMessageParam
{
    /**
     * MIME type. Default is "text/plain".
     */
    string      contentType;
    
    /**
     * The message content.
     */
    string      content;
    
    /**
     * List of headers etc to be included in outgoing request.
     */
    SipTxOption txOption;
    
    /**
     * User data, which will be given back when the IM callback is called.
     */
    Token       userData;
    
public:
    /**
     * Default constructor initializes with zero/empty values.
     */
    SendInstantMessageParam();
};


/**
 * This structure contains parameters for sending typing indication methods,
 * e.g: Buddy::sendTypingIndication(), Call:sendTypingIndication().
 */
struct SendTypingIndicationParam
{
    /**
     * True to indicate to remote that local person is currently typing an IM.
     */
    bool         isTyping;
    
    /**
     * List of headers etc to be included in outgoing request.
     */
    SipTxOption  txOption;
    
public:
    /**
     * Default constructor initializes with zero/empty values.
     */
    SendTypingIndicationParam();
};


/* Utilities */
#ifndef SWIG
//! @cond Doxygen_Suppress
void readIntVector( ContainerNode &node,
                    const string &array_name,
                    IntVector &v) PJSUA2_THROW(Error);
void writeIntVector(ContainerNode &node,
                    const string &array_name,
                    const IntVector &v) PJSUA2_THROW(Error);
void readQosParams( ContainerNode &node,
                    pj_qos_params &qos) PJSUA2_THROW(Error);
void writeQosParams( ContainerNode &node,
                     const pj_qos_params &qos) PJSUA2_THROW(Error);
void readSipHeaders( const ContainerNode &node,
                     const string &array_name,
                     SipHeaderVector &headers) PJSUA2_THROW(Error);
void writeSipHeaders(ContainerNode &node,
                     const string &array_name,
                     const SipHeaderVector &headers) PJSUA2_THROW(Error);
//! @endcond
#endif // SWIG

/**
 * @}  PJSUA2
 */

} // namespace pj



#endif	/* __PJSUA2_SIPTYPES_HPP__ */
