/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#ifndef __PJSIP_SIP_CONFIG_H__
#define __PJSIP_SIP_CONFIG_H__

/**
 * @file sip_config.h
 * @brief Compile time configuration.
 */
#include <pj/config.h>

/**
 * @defgroup PJSIP PJSIP Library Collection
 */

/**
 * @defgroup PJSIP_CORE Core SIP Library
 * @ingroup PJSIP
 * @brief The core framework from which all other SIP components depends on.
 * 
 * The PJSIP Core library only provides transport framework, event
 * dispatching/module framework, and SIP message representation and
 * parsing. It doesn't do anything usefull in itself!
 *
 * If application wants the stack to do anything usefull at all,
 * it must registers @ref PJSIP_MOD to the core library. Examples
 * of modules are @ref PJSIP_TRANSACT and @ref PJSUA_UA.
 */

/**
 * @defgroup PJSIP_BASE Base Types
 * @ingroup PJSIP_CORE
 * @brief Basic PJSIP types and configurations.
 */

/**
 * @defgroup PJSIP_CONFIG Compile Time Configuration
 * @ingroup PJSIP_BASE
 * @brief PJSIP compile time configurations.
 * @{
 */

/*
 * Include sip_autoconf.h if autoconf is used (PJ_AUTOCONF is set)
 */
#if defined(PJ_AUTOCONF)
#   include <pjsip/sip_autoconf.h>
#endif


/**
 * Specify maximum transaction count in transaction hash table.
 * Default value is 16*1024
 */
#ifndef PJSIP_MAX_TSX_COUNT
#   define PJSIP_MAX_TSX_COUNT		(16*1024)
#endif

/**
 * Specify maximum number of dialogs in the dialog hash table.
 * Default value is 16*1024.
 */
#ifndef PJSIP_MAX_DIALOG_COUNT
#   define PJSIP_MAX_DIALOG_COUNT	(16*1024)
#endif


/**
 * Specify maximum number of transports.
 * Default value is equal to maximum number of handles in ioqueue.
 * See also PJSIP_TPMGR_HTABLE_SIZE.
 */
#ifndef PJSIP_MAX_TRANSPORTS
#   define PJSIP_MAX_TRANSPORTS		(PJ_IOQUEUE_MAX_HANDLES)
#endif


/**
 * Transport manager hash table size (must be 2^n-1). 
 * See also PJSIP_MAX_TRANSPORTS
 */
#ifndef PJSIP_TPMGR_HTABLE_SIZE
#   define PJSIP_TPMGR_HTABLE_SIZE	31
#endif


/**
 * Specify maximum URL size.
 * This constant is used mainly when printing the URL for logging purpose 
 * only.
 */
#ifndef PJSIP_MAX_URL_SIZE
#   define PJSIP_MAX_URL_SIZE		256
#endif


/**
 * Specify maximum number of modules.
 * This mainly affects the size of mod_data array in various components.
 */
#ifndef PJSIP_MAX_MODULE
#   define PJSIP_MAX_MODULE		16
#endif


/**
 * Maximum packet length. We set it more than MTU since a SIP PDU
 * containing presence information can be quite large (>1500).
 */
#ifndef PJSIP_MAX_PKT_LEN
#   define PJSIP_MAX_PKT_LEN		2000
#endif


/**
 * Encode SIP headers in their short forms to reduce size. By default,
 * SIP headers in outgoing messages will be encoded in their full names. 
 * If this option is enabled, then SIP headers for outgoing messages
 * will be encoded in their short forms, to reduce message size. 
 * Note that this does not affect the ability of PJSIP to parse incoming
 * SIP messages, as the parser always supports parsing both the long
 * and short version of the headers.
 *
 * Note that there is also an undocumented variable defined in sip_msg.c
 * to control whether compact form should be used for encoding SIP
 * headers. The default value of this variable is PJSIP_ENCODE_SHORT_HNAME.
 * To change PJSIP behavior during run-time, application can use the 
 * following construct:
 *
 \verbatim
   extern pj_bool_t pjsip_use_compact_form;
 
   // enable compact form
   pjsip_use_compact_form = PJ_TRUE;
 \endverbatim
 *
 * Default is 0 (no)
 */
#ifndef PJSIP_ENCODE_SHORT_HNAME
#   define PJSIP_ENCODE_SHORT_HNAME	0
#endif


/**
 * Send Allow header in dialog establishing requests?
 * RFC 3261 Allow header SHOULD be included in dialog establishing
 * requests to inform remote agent about which SIP requests are
 * allowed within dialog.
 *
 * Note that there is also an undocumented variable defined in sip_dialog.c
 * to control whether Allow header should be included. The default value 
 * of this variable is PJSIP_INCLUDE_ALLOW_HDR_IN_DLG.
 * To change PJSIP behavior during run-time, application can use the 
 * following construct:
 *
 \verbatim
   extern pj_bool_t pjsip_include_allow_hdr_in_dlg;
 
   // do not transmit Allow header
   pjsip_include_allow_hdr_in_dlg = PJ_FALSE;
 \endverbatim
 *
 * Default is 1 (Yes)
 */
#ifndef PJSIP_INCLUDE_ALLOW_HDR_IN_DLG
#   define PJSIP_INCLUDE_ALLOW_HDR_IN_DLG	1
#endif


/**
 * Allow SIP modules removal or insertions during operation?
 * If yes, then locking will be employed when endpoint need to
 * access module.
 */
#ifndef PJSIP_SAFE_MODULE
#   define PJSIP_SAFE_MODULE		1
#endif


/**
 * Perform Via sent-by checking as specified in RFC 3261 Section 18.1.2,
 * which says that UAC MUST silently discard responses with Via sent-by
 * containing values that the UAC doesn't recognize as its transport
 * address.
 *
 * In PJSIP, this will cause response to be discarded and a message is
 * written to the log, saying something like:
 *  "Dropping response Response msg 200/INVITE/cseq=608594373 (rdata00A99EF4)
 *   from 1.2.3.4:5060 because sent-by is mismatch"
 *
 * The default behavior is yes, but when the UA supports IP address change
 * for the SIP transport, it will need to turn this checking off since
 * when the transport address is changed between request is sent and 
 * response is received, the response will be discarded since its Via
 * sent-by now contains address that is different than the transport
 * address.
 */
#ifndef PJSIP_CHECK_VIA_SENT_BY
#   define PJSIP_CHECK_VIA_SENT_BY	1
#endif


/**
 * If non-zero, SIP parser will unescape the escape characters ('%')
 * in the original message, which means that it will modify the
 * original message. Otherwise the parser will create a copy of
 * the string and store the unescaped string to the new location.
 *
 * Unescaping in-place is faster, but less elegant (and it may
 * break certain applications). So normally it's disabled, unless
 * when benchmarking (to show off big performance).
 *
 * Default: 0
 */
#ifndef PJSIP_UNESCAPE_IN_PLACE
#   define PJSIP_UNESCAPE_IN_PLACE	0
#endif


/**
 * This macro controls maximum numbers of ioqueue events to be processed
 * in a single pjsip_endpt_handle_events() poll. When PJSIP detects that
 * there are probably more events available from the network and total
 * events so far is less than this value, PJSIP will call pj_ioqueue_poll()
 * again to get more events.
 *
 * Value 1 works best for ioqueue with select() back-end, while for IOCP it is
 * probably best to set this value equal to PJSIP_MAX_TIMED_OUT_ENTRIES
 * since IOCP only processes one event at a time.
 *
 * Default: 1
 */
#ifndef PJSIP_MAX_NET_EVENTS
#   define PJSIP_MAX_NET_EVENTS		1
#endif


/**
 * Max entries to process in timer heap per poll. 
 * 
 * Default: 10
 */
#ifndef PJSIP_MAX_TIMED_OUT_ENTRIES
#   define PJSIP_MAX_TIMED_OUT_ENTRIES	10
#endif


/**
 * Idle timeout interval to be applied to transports with no usage
 * before the transport is destroyed. Value is in seconds.
 *
 * Default: 600
 */
#ifndef PJSIP_TRANSPORT_IDLE_TIME
#   define PJSIP_TRANSPORT_IDLE_TIME	600
#endif


/**
 * Maximum number of usages for a transport before a new transport is
 * created. This only applies for ephemeral transports such as TCP.
 *
 * Currently this is not used.
 * 
 * Default: -1
 */
#ifndef PJSIP_MAX_TRANSPORT_USAGE
#   define PJSIP_MAX_TRANSPORT_USAGE	((unsigned)-1)
#endif


/**
 * The TCP incoming connection backlog number to be set in accept().
 *
 * Default: 5
 *
 * @see PJSIP_TLS_TRANSPORT_BACKLOG
 */
#ifndef PJSIP_TCP_TRANSPORT_BACKLOG
#   define PJSIP_TCP_TRANSPORT_BACKLOG	5
#endif


/**
 * Set the interval to send keep-alive packet for TCP transports.
 * If the value is zero, keep-alive will be disabled for TCP.
 *
 * Default: 90 (seconds)
 *
 * @see PJSIP_TCP_KEEP_ALIVE_DATA
 */
#ifndef PJSIP_TCP_KEEP_ALIVE_INTERVAL
#   define PJSIP_TCP_KEEP_ALIVE_INTERVAL    90
#endif


/**
 * Set the payload of the TCP keep-alive packet.
 *
 * Default: CRLF
 */
#ifndef PJSIP_TCP_KEEP_ALIVE_DATA
#   define PJSIP_TCP_KEEP_ALIVE_DATA	    { "\r\n", 2 }
#endif


/**
 * Set the interval to send keep-alive packet for TLS transports.
 * If the value is zero, keep-alive will be disabled for TLS.
 *
 * Default: 90 (seconds)
 *
 * @see PJSIP_TLS_KEEP_ALIVE_DATA
 */
#ifndef PJSIP_TLS_KEEP_ALIVE_INTERVAL
#   define PJSIP_TLS_KEEP_ALIVE_INTERVAL    90
#endif


/**
 * Set the payload of the TLS keep-alive packet.
 *
 * Default: CRLF
 */
#ifndef PJSIP_TLS_KEEP_ALIVE_DATA
#   define PJSIP_TLS_KEEP_ALIVE_DATA	    { "\r\n", 2 }
#endif


/**
 * This macro specifies whether full DNS resolution should be used.
 * When enabled, #pjsip_resolve() will perform asynchronous DNS SRV and
 * A (or AAAA, when IPv6 is supported) resolution to resolve the SIP
 * domain.
 *
 * Note that even when this setting is enabled, asynchronous DNS resolution
 * will only be done when application calls #pjsip_endpt_create_resolver(),
 * configure the nameservers with #pj_dns_resolver_set_ns(), and configure
 * the SIP endpoint's DNS resolver with #pjsip_endpt_set_resolver(). If
 * these steps are not followed, the domain will be resolved with normal
 * pj_gethostbyname() function.
 *
 * Turning off this setting will save the footprint by about 16KB, since
 * it should also exclude dns.o and resolve.o from PJLIB-UTIL.
 *
 * Default: 1 (enabled)
 *
 * @see PJSIP_MAX_RESOLVED_ADDRESSES
 */
#ifndef PJSIP_HAS_RESOLVER
#   define PJSIP_HAS_RESOLVER		1
#endif


/** 
 * Maximum number of addresses returned by the resolver. The number here 
 * will slightly affect stack usage, since each entry will occupy about
 * 32 bytes of stack memory.
 *
 * Default: 8
 *
 * @see PJSIP_HAS_RESOLVER
 */
#ifndef PJSIP_MAX_RESOLVED_ADDRESSES
#   define PJSIP_MAX_RESOLVED_ADDRESSES	    8
#endif


/**
 * Enable TLS SIP transport support. For most systems this means that
 * OpenSSL must be installed.
 *
 * Default: 0 (for now)
 */
#ifndef PJSIP_HAS_TLS_TRANSPORT
#   define PJSIP_HAS_TLS_TRANSPORT	    0
#endif


/**
 * The TLS pending incoming connection backlog number to be set in accept().
 *
 * Default: 5
 *
 * @see PJSIP_TCP_TRANSPORT_BACKLOG
 */
#ifndef PJSIP_TLS_TRANSPORT_BACKLOG
#   define PJSIP_TLS_TRANSPORT_BACKLOG	    5
#endif



/* Endpoint. */
#define PJSIP_MAX_TIMER_COUNT		(2*PJSIP_MAX_TSX_COUNT + 2*PJSIP_MAX_DIALOG_COUNT)

/**
 * Initial memory block for the endpoint.
 */
#ifndef PJSIP_POOL_LEN_ENDPT
#   define PJSIP_POOL_LEN_ENDPT		(4000)
#endif

/**
 * Memory increment for endpoint.
 */
#ifndef PJSIP_POOL_INC_ENDPT
#   define PJSIP_POOL_INC_ENDPT		(4000)
#endif


/* Transport related constants. */

/**
 * Initial memory block for rdata.
 */
#ifndef PJSIP_POOL_RDATA_LEN
#   define PJSIP_POOL_RDATA_LEN		4000
#endif

/**
 * Memory increment for rdata.
 */
#ifndef PJSIP_POOL_RDATA_INC
#   define PJSIP_POOL_RDATA_INC		4000
#endif

#define PJSIP_POOL_LEN_TRANSPORT	512
#define PJSIP_POOL_INC_TRANSPORT	512

/**
 * Initial memory block size for tdata.
 */
#ifndef PJSIP_POOL_LEN_TDATA
#   define PJSIP_POOL_LEN_TDATA		4000
#endif

/**
 * Memory increment for tdata.
 */
#ifndef PJSIP_POOL_INC_TDATA
#   define PJSIP_POOL_INC_TDATA		4000
#endif

/**
 * Initial memory size for UA layer
 */
#ifndef PJSIP_POOL_LEN_UA
#   define PJSIP_POOL_LEN_UA		4000
#endif

/**
 * Memory increment for UA layer.
 */
#ifndef PJSIP_POOL_INC_UA
#   define PJSIP_POOL_INC_UA		4000
#endif

#define PJSIP_MAX_FORWARDS_VALUE	70

#define PJSIP_RFC3261_BRANCH_ID		"z9hG4bK"
#define PJSIP_RFC3261_BRANCH_LEN	7

/* Transaction related constants. */

/**
 * Initial memory size for transaction layer
 */
#ifndef PJSIP_POOL_TSX_LAYER_LEN
#   define PJSIP_POOL_TSX_LAYER_LEN	4000
#endif

/**
 * Memory increment for transaction layer.
 */
#ifndef PJSIP_POOL_TSX_LAYER_INC
#   define PJSIP_POOL_TSX_LAYER_INC	4000
#endif

/**
 * Initial memory size for a SIP transaction object.
 */
#ifndef PJSIP_POOL_TSX_LEN
#   define PJSIP_POOL_TSX_LEN		1536 /* 768 */
#endif

/**
 * Memory increment for transaction object.
 */
#ifndef PJSIP_POOL_TSX_INC
#   define PJSIP_POOL_TSX_INC		256
#endif

#define PJSIP_MAX_TSX_KEY_LEN		(PJSIP_MAX_URL_SIZE*2)

/* User agent. */
#define PJSIP_POOL_LEN_USER_AGENT	1024
#define PJSIP_POOL_INC_USER_AGENT	1024

/* Message/URL related constants. */
#define PJSIP_MAX_CALL_ID_LEN		pj_GUID_STRING_LENGTH()
#define PJSIP_MAX_TAG_LEN		pj_GUID_STRING_LENGTH()
#define PJSIP_MAX_BRANCH_LEN		(PJSIP_RFC3261_BRANCH_LEN + pj_GUID_STRING_LENGTH() + 2)
#define PJSIP_MAX_HNAME_LEN		64

/* Dialog related constants. */
#define PJSIP_POOL_LEN_DIALOG		1200
#define PJSIP_POOL_INC_DIALOG		512

/* Maximum header types. */
#define PJSIP_MAX_HEADER_TYPES		64

/* Maximum URI types. */
#define PJSIP_MAX_URI_TYPES		4

/*****************************************************************************
 *  Default timeout settings, in miliseconds. 
 */

/** Transaction T1 timeout value. */
#if !defined(PJSIP_T1_TIMEOUT)
#  define PJSIP_T1_TIMEOUT	500
#endif

/** Transaction T2 timeout value. */
#if !defined(PJSIP_T2_TIMEOUT)
#  define PJSIP_T2_TIMEOUT	4000
#endif

/** Transaction completed timer for non-INVITE */
#if !defined(PJSIP_T4_TIMEOUT)
#  define PJSIP_T4_TIMEOUT	5000
#endif

/** Transaction completed timer for INVITE */
#if !defined(PJSIP_TD_TIMEOUT)
#  define PJSIP_TD_TIMEOUT	32000
#endif


/*****************************************************************************
 *  Authorization
 */

/**
 * If this flag is set, the stack will keep the Authorization/Proxy-Authorization
 * headers that are sent in a cache. Future requests with the same realm and
 * the same method will use the headers in the cache (as long as no qop is
 * required by server).
 *
 * Turning on this flag will make authorization process goes faster, but
 * will grow the memory usage undefinitely until the dialog/registration
 * session is terminated.
 *
 * Default: 1
 */
#if !defined(PJSIP_AUTH_HEADER_CACHING)
#   define PJSIP_AUTH_HEADER_CACHING	    0
#endif

/**
 * If this flag is set, the stack will proactively send Authorization/Proxy-
 * Authorization header for next requests. If next request has the same method
 * with any of previous requests, then the last header which is saved in
 * the cache will be used (if PJSIP_AUTH_CACHING is set). Otherwise a fresh
 * header will be recalculated. If a particular server has requested qop, then
 * a fresh header will always be calculated.
 *
 * If this flag is NOT set, then the stack will only send Authorization/Proxy-
 * Authorization headers when it receives 401/407 response from server.
 *
 * Turning ON this flag will grow memory usage of a dialog/registration pool
 * indefinitely until it is terminated, because the stack needs to keep the
 * last WWW-Authenticate/Proxy-Authenticate challenge.
 *
 * Default: 1
 */
#if !defined(PJSIP_AUTH_AUTO_SEND_NEXT)
#   define PJSIP_AUTH_AUTO_SEND_NEXT	    0
#endif

/**
 * Support qop="auth" directive.
 * This option also requires client to cache the last challenge offered by
 * server.
 *
 * Default: 1
 */
#if !defined(PJSIP_AUTH_QOP_SUPPORT)
#   define PJSIP_AUTH_QOP_SUPPORT	    1
#endif


/**
 * Maximum number of stale retries when server keeps rejecting our request
 * with stale=true.
 */
#ifndef PJSIP_MAX_STALE_COUNT
#   define PJSIP_MAX_STALE_COUNT	    3
#endif


/**
 * Specify support for IMS/3GPP digest AKA authentication version 1 and 2
 * (AKAv1-MD5 and AKAv2-MD5 respectively).
 *
 * Default: 0 (disabled, for now)
 */
#ifndef PJSIP_HAS_DIGEST_AKA_AUTH
#   define PJSIP_HAS_DIGEST_AKA_AUTH	    0
#endif


/**
 * @}
 */

#include <pj/config.h>


#endif	/* __PJSIP_SIP_CONFIG_H__ */

