/* $Header: /pjproject/pjsip/src/pjsip/sip_types.h 5     6/19/05 6:12p Bennylp $ */
#ifndef __PJSIP_SIP_TYPES_H__
#define __PJSIP_SIP_TYPES_H__

#include <pjsip/sip_config.h>
#include <pj/types.h>

/**
 * Opaque data structure for transports (sip_transport.h).
 */
typedef struct pjsip_transport_t pjsip_transport_t;

/**
 * Opaque data type for transport manager (sip_transport.h).
 */
typedef struct pjsip_transport_mgr pjsip_transport_mgr;

/**
 * Transport types.
 */
typedef enum pjsip_transport_type_e
{
    /** Unspecified. */
    PJSIP_TRANSPORT_UNSPECIFIED,

    /** UDP. */
    PJSIP_TRANSPORT_UDP,

#if PJ_HAS_TCP
    /** TCP. */
    PJSIP_TRANSPORT_TCP,

    /** TLS. */
    PJSIP_TRANSPORT_TLS,

    /** SCTP. */
    PJSIP_TRANSPORT_SCTP,
#endif

} pjsip_transport_type_e;


/**
 * Forward declaration for endpoint (sip_endpoint.h).
 */
typedef struct pjsip_endpoint pjsip_endpoint;

/**
 * Forward declaration for transactions (sip_transaction.h).
 */
typedef struct pjsip_transaction pjsip_transaction;

/**
 * Forward declaration for events (sip_event.h).
 */
typedef struct pjsip_event pjsip_event;

/**
 * Forward declaration for transmit data/buffer (sip_transport.h).
 */
typedef struct pjsip_tx_data pjsip_tx_data;

/**
 * Forward declaration for receive data/buffer (sip_transport.h).
 */
typedef struct pjsip_rx_data pjsip_rx_data;

/**
 * Forward declaration for message (sip_msg.h).
 */
typedef struct pjsip_msg pjsip_msg;

/**
 * Forward declaration for URI (sip_uri.h).
 */
typedef struct pjsip_uri pjsip_uri;

/**
 * Opaque data type for the resolver engine (sip_resolve.h).
 */
typedef struct pjsip_resolver_t pjsip_resolver_t;

/**
 * Forward declaration for credential.
 */
typedef struct pjsip_cred_info pjsip_cred_info;


/**
 * Forward declaration for module (sip_module.h).
 */
typedef struct pjsip_module pjsip_module;

/**
 * Transaction role.
 */
typedef enum pjsip_role_e
{
    PJSIP_ROLE_UAC,	/**< Transaction role is UAC. */
    PJSIP_ROLE_UAS,	/**< Transaction role is UAS. */
} pjsip_role_e;


/**
 * General purpose buffer.
 */
typedef struct pjsip_buffer
{
    /** The start of the buffer. */
    char *start;

    /** Pointer to current end of the buffer, which also indicates the position
        of subsequent buffer write.
     */
    char *cur;

    /** The absolute end of the buffer. */
    char *end;

} pjsip_buffer;


/**
 * General host:port pair, used for example as Via sent-by.
 */
typedef struct pjsip_host_port
{
    unsigned flag;	/**< Flags of pjsip_transport_flags_e (not used in Via). */
    unsigned type;	/**< Transport type (pjsip_transport_type_e), or zero. */
    pj_str_t host;	/**< Host part. */
    int	     port;	/**< Port number. */
} pjsip_host_port;


#endif	/* __PJSIP_SIP_TYPES_H__ */

