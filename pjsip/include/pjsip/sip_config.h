/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pj/types.h>

/**
 * @defgroup PJSIP_CORE Core SIP Library
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
 * @defgroup PJSIP_CONFIG PJSIP Configurations/Settings
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

PJ_BEGIN_DECL

/**
 * This structure describes PJSIP run-time configurations/settings.
 * Application may use #pjsip_cfg() function to modify the settings
 * before creating the stack.
 */
typedef struct pjsip_cfg_t
{
    /** Global settings. */
    struct {
	/**
	 * Specify port number should be allowed to appear in To and From
	 * header. Note that RFC 3261 disallow this, see Table 1 in section
	 * 19.1.1 of the RFC.
	 *
	 * Default is PJSIP_ALLOW_PORT_IN_FROMTO_HDR.
	 */
	pj_bool_t allow_port_in_fromto_hdr;

	/**
	 * Accept call replace in early state when invite is not initiated
	 * by the user agent. RFC 3891 Section 3 disallows this, however,
	 * for better interoperability reason, this might be ignored.
	 *
	 * Default is PJSIP_ACCEPT_REPLACE_IN_EARLY_STATE.
	 */
	pj_bool_t accept_replace_in_early_state;

	/**
	 * Allow hash character ('#') to appear in outgoing URIs. See
	 * https://trac.pjsip.org/repos/ticket/1569.
	 *
	 * Default is PJ_FALSE.
	 */
	pj_bool_t allow_tx_hash_in_uri;

	/**
	 * Disable rport in request.
	 *
	 * Default is PJ_FALSE.
	 */
	pj_bool_t disable_rport;

	/**
	 * Disable automatic switching from UDP to TCP if outgoing request
	 * is greater than 1300 bytes.
	 *
	 * Default is PJSIP_DONT_SWITCH_TO_TCP.
	 */
	pj_bool_t disable_tcp_switch;

	/**
	 * Disable automatic switching to TLS if target-URI does not use
	 * "sips" scheme nor TLS transport, even when request-URI uses
	 * "sips" scheme.
	 *
	 * Default is PJSIP_DONT_SWITCH_TO_TLS.
	 */
	pj_bool_t disable_tls_switch;

	/**
	 * Enable call media session to always be updated to the latest
	 * received early media SDP when receiving forked early media
	 * (multiple 183 responses with different To tag).
	 *
	 * Default is PJSIP_FOLLOW_EARLY_MEDIA_FORK.
	 */
	pj_bool_t follow_early_media_fork;

	/**
	 * Specify whether "alias" param should be added to the Via header
	 * in any outgoing request with connection oriented transport.
	 *
	 * Default is PJSIP_REQ_HAS_VIA_ALIAS.
	 */
	pj_bool_t req_has_via_alias;

	/**
	 * Resolve hostname when trying to get the network interface to be put 
	 * in Via or Contact header.
	 *
	 * Default is PJSIP_RESOLVE_HOSTNAME_TO_GET_INTERFACE.
	 */
	pj_bool_t resolve_hostname_to_get_interface;

	/**
	 * Disable security check on incoming messages in a secure dialog.
	 * A secure dialog is created when the request that creates the dialog
	 * uses "sips" scheme in its request URI. Contact URI should use "sips"
	 * scheme and the top-most Record-Route URI, if any, should use either
	 * "sips" scheme or "transport=tls" param. See also
	 * https://trac.pjsip.org/repos/ticket/1735.
	 *
	 * Default is PJ_FALSE.
	 */
	pj_bool_t disable_secure_dlg_check;

	/**
	 * Encode SIP headers in their short forms to reduce size. By default,
	 * SIP headers in outgoing messages will be encoded in their full names.
	 * If this option is enabled, then SIP headers for outgoing messages
	 * will be encoded in their short forms, to reduce message size. 
	 * Note that this does not affect the ability of PJSIP to parse incoming
	 * SIP messages, as the parser always supports parsing both the long
	 * and short version of the headers.
	 *
	 * Default is PJSIP_ENCODE_SHORT_HNAME
	 */
	pj_bool_t use_compact_form;

        /**
         * Accept multiple SDP answers on non-reliable 18X responses and the 2XX
         * response when they are all received from the same source (same To tag).
         *
         * See also:
         * https://tools.ietf.org/html/rfc6337#section-3.1.1
         *
         * Default is PJSIP_ACCEPT_MULTIPLE_SDP_ANSWERS.
         */
        pj_bool_t accept_multiple_sdp_answers;

	/**
	 * Don't disconnect the INVITE session after an outgoing request
	 * gets timed out or responded with 408 (request timeout).
	 *
	 * Default is PJ_FALSE.
	 */
	pj_bool_t keep_inv_after_tsx_timeout;

    } endpt;

    /** Transaction layer settings. */
    struct {

	/** Maximum number of transactions. The value is initialized with
	 *  PJSIP_MAX_TSX_COUNT
	 */
	unsigned max_count;

	/* Timeout values: */

	/** Transaction T1 timeout, in msec. Default value is PJSIP_T1_TIMEOUT
	 */
	unsigned t1;

	/** Transaction T2 timeout, in msec. Default value is PJSIP_T2_TIMEOUT
	 */
	unsigned t2;

	/** Transaction completed timer for non-INVITE, in msec. Default value
	 *  is PJSIP_T4_TIMEOUT
	 */
	unsigned t4;

	/** Transaction completed timer for INVITE, in msec. Default value is
	 *  PJSIP_TD_TIMEOUT.
	 *
	 *  This setting is also used for transaction timeout timer for both
	 *  INVITE and non-INVITE.
	 */
	unsigned td;

    } tsx;

    /* Dialog layer settings .. TODO */

    /** Client registration settings. */
    struct {
	/**
	 * Specify whether client registration should check for its 
	 * registered contact in Contact header of successful REGISTER 
	 * response to determine whether registration has been successful. 
	 * This setting may be disabled if non-compliant registrar is unable
	 * to return correct Contact header.
	 *
	 * Default is PJSIP_REGISTER_CLIENT_CHECK_CONTACT
	 */
	pj_bool_t   check_contact;

	/**
	 * Specify whether client registration should add "x-uid" extension
	 * parameter in all Contact URIs that it registers to assist the
	 * matching of Contact URIs in the 200/OK REGISTER response, in 
	 * case the registrar is unable to return exact Contact URI in the
	 * 200/OK response.
	 *
	 * Default is PJSIP_REGISTER_CLIENT_ADD_XUID_PARAM.
	 */
	pj_bool_t   add_xuid_param;

    } regc;

    /** TCP transport settings */
    struct {
        /**
         * Set the interval to send keep-alive packet for TCP transports.
         * If the value is zero, keep-alive will be disabled for TCP.
         *
         * Default is PJSIP_TCP_KEEP_ALIVE_INTERVAL.
         */
        long keep_alive_interval;

    } tcp;

    /** TLS transport settings */
    struct {
        /**
         * Set the interval to send keep-alive packet for TLS transports.
         * If the value is zero, keep-alive will be disabled for TLS.
         *
         * Default is PJSIP_TLS_KEEP_ALIVE_INTERVAL.
         */
        long keep_alive_interval;

    } tls;

} pjsip_cfg_t;


#ifdef PJ_DLL
/**
 * Get pjsip configuration instance. Application may modify the
 * settings before creating the SIP endpoint and modules.
 *
 * @return  Configuration instance.
 */
PJ_DECL(pjsip_cfg_t*) pjsip_cfg(void);

#else	/* PJ_DLL */

extern pjsip_cfg_t pjsip_sip_cfg_var;

/**
 * Get pjsip configuration instance. Application may modify the
 * settings before creating the SIP endpoint and modules.
 *
 * @return  Configuration instance.
 */
PJ_INLINE(pjsip_cfg_t*) pjsip_cfg(void)
{
    return &pjsip_sip_cfg_var;
}

#endif	/* PJ_DLL */


/**
 * Specify maximum transaction count in transaction hash table.
 * For efficiency, the value should be 2^n-1 since it will be
 * rounded up to 2^n.
 *
 * Default value is 1023
 */
#ifndef PJSIP_MAX_TSX_COUNT
#   define PJSIP_MAX_TSX_COUNT		(1024-1)
#endif

/**
 * Specify maximum number of dialogs in the dialog hash table.
 * For efficiency, the value should be 2^n-1 since it will be
 * rounded up to 2^n.
 *
 * Default value is 511.
 */
#ifndef PJSIP_MAX_DIALOG_COUNT
#   define PJSIP_MAX_DIALOG_COUNT	(512-1)
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
 */
#ifndef PJSIP_MAX_URL_SIZE
#   define PJSIP_MAX_URL_SIZE		256
#endif


/**
 * Specify maximum number of modules.
 * This mainly affects the size of mod_data array in various components.
 */
#ifndef PJSIP_MAX_MODULE
#   define PJSIP_MAX_MODULE		32
#endif


/**
 * Maximum packet length. We set it more than MTU since a SIP PDU
 * containing presence information can be quite large (>1500).
 */
#ifndef PJSIP_MAX_PKT_LEN
#   define PJSIP_MAX_PKT_LEN		4000
#endif


/**
 * RFC 3261 section 18.1.1:
 * If a request is within 200 bytes of the path MTU, or if it is larger
 * than 1300 bytes and the path MTU is unknown, the request MUST be sent
 * using an RFC 2914 [43] congestion controlled transport protocol, such
 * as TCP.
 *
 * Disable the behavior of automatic switching to TCP whenever UDP packet
 * size exceeds the threshold defined in PJSIP_UDP_SIZE_THRESHOLD.
 *
 * This option can also be controlled at run-time by the \a disable_tcp_switch
 * setting in pjsip_cfg_t.
 *
 * Default is 0 (no).
 */
#ifndef PJSIP_DONT_SWITCH_TO_TCP
#   define PJSIP_DONT_SWITCH_TO_TCP	0
#endif


/**
 * As specified RFC 3261 section 8.1.2, when request-URI uses "sips" scheme,
 * TLS must always be used regardless of the target-URI scheme or transport
 * type.
 *
 * This option will specify whether the behavior of automatic switching to TLS
 * should be disabled, i.e: regard the target-URI scheme or transport type.
 *
 * This option can also be controlled at run-time by the \a disable_tls_switch
 * setting in pjsip_cfg_t.
 *
 * Default is 0 (no).
 */
#ifndef PJSIP_DONT_SWITCH_TO_TLS
#   define PJSIP_DONT_SWITCH_TO_TLS	0
#endif

/**
 * Specify if #pjsip_endpt_handle_events() should sleep if ioqueue poll
 * returns error.
 *
 * Default is 1 (yes).
 */

#ifndef PJSIP_HANDLE_EVENTS_HAS_SLEEP_ON_ERR
#   define PJSIP_HANDLE_EVENTS_HAS_SLEEP_ON_ERR	    1
#endif

/**
 * Specify whether the call media session should be updated to the latest
 * received early media SDP when receiving forked early media (multiple 183
 * responses with different To tag).
 *
 * This option can also be controlled at run-time by the
 * \a follow_early_media_fork setting in pjsip_cfg_t.
 *
 * Default is PJ_TRUE.
 */
#ifndef PJSIP_FOLLOW_EARLY_MEDIA_FORK
#   define PJSIP_FOLLOW_EARLY_MEDIA_FORK	    PJ_TRUE
#endif


/**
 * Accept multiple SDP answers on non-reliable 18X responses and the 2XX
 * response when they are all received from the same source (same To tag).
 *
 * This option can also be controlled at run-time by the
 * \a accept_multiple_sdp_answers setting in pjsip_cfg_t.
 *
 * Default is PJ_TRUE.
 */
#ifndef PJSIP_ACCEPT_MULTIPLE_SDP_ANSWERS
#   define PJSIP_ACCEPT_MULTIPLE_SDP_ANSWERS        PJ_TRUE
#endif


/**
 * Specify whether "alias" param should be added to the Via header
 * in any outgoing request with connection oriented transport.
 *
 * This option can also be controlled at run-time by the
 * \a req_has_via_alias setting in pjsip_cfg_t.
 *
 * Default is PJ_TRUE.
 */
#ifndef PJSIP_REQ_HAS_VIA_ALIAS
#   define PJSIP_REQ_HAS_VIA_ALIAS		    PJ_TRUE
#endif

/**
 * Resolve hostname when trying to get the network interface to be put in Via 
 * or Contact header.
 *
 * This option can also be controlled at run-time by the
 * \a resolve_hostname_to_get_interface setting in pjsip_cfg_t.
 *
 * Default is PJ_FALSE.
 */
#ifndef PJSIP_RESOLVE_HOSTNAME_TO_GET_INTERFACE
#   define PJSIP_RESOLVE_HOSTNAME_TO_GET_INTERFACE  PJ_FALSE
#endif

/**
 * Accept call replace in early state when invite is not initiated
 * by the user agent. RFC 3891 Section 3 disallows this, however,
 * for better interoperability reason, this might be ignored.
 *
 * This option can also be controlled at run-time by the
 * \a accept_replace_in_early_state setting in pjsip_cfg_t.
 *
 * Default is 0 (no).
 */
#ifndef PJSIP_ACCEPT_REPLACE_IN_EARLY_STATE
#   define PJSIP_ACCEPT_REPLACE_IN_EARLY_STATE	    0
#endif


/**
 * This setting controls the threshold of the UDP packet, which if it's
 * larger than this value the request will be sent with TCP. This setting
 * is useful only when PJSIP_DONT_SWITCH_TO_TCP is set to 0.
 *
 * Default is 1300 bytes.
 */
#ifndef PJSIP_UDP_SIZE_THRESHOLD
#   define PJSIP_UDP_SIZE_THRESHOLD	1300
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
 * This option can also be controlled at run-time by the
 * \a use_compact_form setting in pjsip_cfg_t.
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
 *
 * Update:
 * As of version 2.1, the default value is 0. This change was part of
 * https://trac.pjsip.org/repos/ticket/1412
 */
#ifndef PJSIP_CHECK_VIA_SENT_BY
#   define PJSIP_CHECK_VIA_SENT_BY	0
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
 * Specify port number should be allowed to appear in To and From
 * header. Note that RFC 3261 disallow this, see Table 1 in section
 * 19.1.1 of the RFC. This setting can also be altered at run-time
 * via pjsip_cfg setting, see pjsip_cfg_t.allow_port_in_fromto_hdr
 * field.
 *
 * Default: 0
 */
#ifndef PJSIP_ALLOW_PORT_IN_FROMTO_HDR
#   define PJSIP_ALLOW_PORT_IN_FROMTO_HDR	0
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
 * Idle timeout interval to be applied to outgoing transports (i.e. client
 * side) with no usage before the transport is destroyed. Value is in
 * seconds.
 *
 * Note that if the value is put lower than 33 seconds, it may cause some
 * pjsip test units to fail. See the comment on the following link:
 * https://trac.pjsip.org/repos/ticket/1465#comment:4
 *
 * Default: 33
 */
#ifndef PJSIP_TRANSPORT_IDLE_TIME
#   define PJSIP_TRANSPORT_IDLE_TIME	33
#endif


/**
 * Idle timeout interval to be applied to incoming transports (i.e. server
 * side) with no usage before the transport is destroyed. Server typically
 * should let client close the connection, hence set this interval to a large
 * value. Value is in seconds.
 *
 * Default: 600
 */
#ifndef PJSIP_TRANSPORT_SERVER_IDLE_TIME
#   define PJSIP_TRANSPORT_SERVER_IDLE_TIME	600
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
 * Specify whether TCP listener should use SO_REUSEADDR option. This constant
 * will be used as the default value for the "reuse_addr" field in the
 * pjsip_tcp_transport_cfg structure.
 *
 * Default is 0 on Windows and 1 on non-Windows.
 *
 * @see PJSIP_TLS_TRANSPORT_REUSEADDR
 */
#ifndef PJSIP_TCP_TRANSPORT_REUSEADDR
# if (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_WIN64) && PJ_WIN64)
#   define PJSIP_TCP_TRANSPORT_REUSEADDR	0
# else
#   define PJSIP_TCP_TRANSPORT_REUSEADDR	1
# endif
#endif


/**
 * Specify whether TCP transport should skip creating the listener.
 * Not having a listener means that application will not be able to
 * function in server mode and accept incoming connections.
 *
 * When enabling this setting, if you use PJSUA, it is recommended to set 
 * pjsua_acc_config.contact_use_src_port to PJ_TRUE.
 * Warning: If contact_use_src_port is disabled or failed (because it's
 * unsupported in some platforms or automatically turned off due to
 * DNS server resolution), Contact header will be generated from
 * pj_getipinterface()/pj_gethostip(), but the address will not be
 * able to accept connections. 
 *
 * Default is 0 (listener will be created).
 */
#ifndef PJSIP_TCP_TRANSPORT_DONT_CREATE_LISTENER
#   define PJSIP_TCP_TRANSPORT_DONT_CREATE_LISTENER 0
#endif


/**
 * Specify whether TLS transport should skip creating the listener.
 * Not having a listener means that application will not be able to
 * function in server mode and accept incoming connections.
 *
 * When enabling this setting, if you use PJSUA, it is recommended to set 
 * pjsua_acc_config.contact_use_src_port to PJ_TRUE.
 * Warning: If contact_use_src_port is disabled or failed (because it's
 * unsupported in some platforms or automatically turned off due to
 * DNS server resolution), Contact header will be generated from
 * pj_getipinterface()/pj_gethostip(), but the address will not be
 * able to accept connections.
 *
 * Default is 0 (listener will be created).
 */
#ifndef PJSIP_TLS_TRANSPORT_DONT_CREATE_LISTENER
#   define PJSIP_TLS_TRANSPORT_DONT_CREATE_LISTENER 0
#endif


/**
 * Set the interval to send keep-alive packet for TCP transports.
 * If the value is zero, keep-alive will be disabled for TCP.
 *
 * This option can be changed in run-time by settting
 * \a tcp.keep_alive_interval field of pjsip_cfg().
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
#   define PJSIP_TCP_KEEP_ALIVE_DATA	    { "\r\n\r\n", 4 }
#endif


/**
 * Initial timeout interval to be applied to incoming transports (i.e. server
 * side) when no data received after a successful connection. Value is in
 * seconds. Disable the timeout by setting it to 0.
 *
 * Note that even when this is disable, the connection might still get closed
 * when it is idle or not referred anymore. Have a look at \a
 * PJSIP_TRANSPORT_SERVER_IDLE_TIME
 *
 * Default: 0 (disabled)
 */
#ifndef PJSIP_TCP_INITIAL_TIMEOUT
#   define PJSIP_TCP_INITIAL_TIMEOUT	    0
#endif

/**
 * Set the interval to send keep-alive packet for TLS transports.
 * If the value is zero, keep-alive will be disabled for TLS.
 *
 * This option can be changed in run-time by settting
 * \a tls.keep_alive_interval field of pjsip_cfg().
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
#   define PJSIP_TLS_KEEP_ALIVE_DATA	    { "\r\n\r\n", 4 }
#endif


/**
 * This macro specifies whether full DNS resolution should be used.
 * When enabled, #pjsip_resolve() will perform asynchronous DNS SRV and
 * A (or AAAA, when IPv6 is supported) resolution to resolve the SIP
 * domain.
 *
 * Note that even when this setting is enabled, asynchronous DNS resolution
 * will only be done when application calls #pjsip_endpt_create_resolver(),
 * configure the nameservers with pj_dns_resolver_set_ns(), and configure
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
 * Default: 16 (or 32 if IPv6 support is enabled)
 *
 * @see PJSIP_HAS_RESOLVER
 */
#ifndef PJSIP_MAX_RESOLVED_ADDRESSES
#   if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6
#       define PJSIP_MAX_RESOLVED_ADDRESSES	    32
#   else
#       define PJSIP_MAX_RESOLVED_ADDRESSES	    16
#   endif
#endif


/**
 * Enable TLS SIP transport support. For most systems this means that
 * OpenSSL must be installed.
 *
 * Default: follow PJ_HAS_SSL_SOCK setting, which is 0 (disabled) by default.
 */
#ifndef PJSIP_HAS_TLS_TRANSPORT
#   define PJSIP_HAS_TLS_TRANSPORT          PJ_HAS_SSL_SOCK
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


/**
 * Specify whether TLS listener should use SO_REUSEADDR option.
 *
 * Default is 0 on Windows and 1 on non-Windows.
 *
 * @see PJSIP_TCP_TRANSPORT_REUSEADDR
 */
#ifndef PJSIP_TLS_TRANSPORT_REUSEADDR
# if (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_WIN64) && PJ_WIN64)
#   define PJSIP_TLS_TRANSPORT_REUSEADDR	0
# else
#   define PJSIP_TLS_TRANSPORT_REUSEADDR	1
# endif
#endif


/**
 * Specify the maximum number of timer entries initially allocated by
 * endpoint. If the application registers more entries during runtime,
 * then the timer will automatically resize.
 *
 * Default: (2*pjsip_cfg()->tsx.max_count) + (2*PJSIP_MAX_DIALOG_COUNT)
 */
#ifndef PJSIP_MAX_TIMER_COUNT
#   define PJSIP_MAX_TIMER_COUNT	(2*pjsip_cfg()->tsx.max_count + \
					 2*PJSIP_MAX_DIALOG_COUNT)
#endif

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

/**
 * Initial memory block for SIP transport.
 */
#ifndef PJSIP_POOL_LEN_TRANSPORT
#   define PJSIP_POOL_LEN_TRANSPORT	512
#endif

/**
 * Memory increment for SIP transport.
 */
#ifndef PJSIP_POOL_INC_TRANSPORT
#   define PJSIP_POOL_INC_TRANSPORT	512
#endif

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
#   define PJSIP_POOL_LEN_UA		512
#endif

/**
 * Memory increment for UA layer.
 */
#ifndef PJSIP_POOL_INC_UA
#   define PJSIP_POOL_INC_UA		512
#endif

/**
 * Initial memory block for event subscription module.
 */
#ifndef PJSIP_POOL_EVSUB_LEN
#   define PJSIP_POOL_EVSUB_LEN		512
#endif

/**
 * Memory increment for event subscription module.
 */
#ifndef PJSIP_POOL_EVSUB_INC
#   define PJSIP_POOL_EVSUB_INC		512
#endif


#define PJSIP_MAX_FORWARDS_VALUE	70

#define PJSIP_RFC3261_BRANCH_ID		"z9hG4bK"
#define PJSIP_RFC3261_BRANCH_LEN	7

/* Transaction related constants. */

/**
 * Initial memory size for transaction layer. The bulk of pool usage
 * for transaction layer will be used to create the hash table, so 
 * setting this value too high will not help too much with reducing
 * fragmentation and the memory will most likely be wasted.
 */
#ifndef PJSIP_POOL_TSX_LAYER_LEN
#   define PJSIP_POOL_TSX_LAYER_LEN	512
#endif

/**
 * Memory increment for transaction layer. The bulk of pool usage
 * for transaction layer will be used to create the hash table, so 
 * setting this value too high will not help too much with reducing
 * fragmentation and the memory will most likely be wasted.
 */
#ifndef PJSIP_POOL_TSX_LAYER_INC
#   define PJSIP_POOL_TSX_LAYER_INC	512
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

/**
 * Delay for non-100 1xx retransmission, in seconds.
 * Set to 0 to disable this feature.
 *
 * Default: 60 seconds
 */
#ifndef PJSIP_TSX_1XX_RETRANS_DELAY
#   define PJSIP_TSX_1XX_RETRANS_DELAY	60
#endif

/**
 * Setting to determine if certain SIP UAS transaction, such as
 * INVITE UAS tsx that hasn't been confirmed, is allowed to continue
 * upon transport error. If disabled, the transaction will always be
 * terminated, which is the default behavior prior to the introduction
 * of this setting.
 *
 * Default: 1 (transaction will continue)
 */
#ifndef PJSIP_TSX_UAS_CONTINUE_ON_TP_ERROR
#   define PJSIP_TSX_UAS_CONTINUE_ON_TP_ERROR 1
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
#define PJSIP_MAX_HEADER_TYPES		72

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

/**
 * Transaction completed timer for INVITE.
 *
 * This setting is also used for transaction timeout timer for both
 * INVITE and non-INVITE.
 */
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
 * Default: 0
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
 * Default: 0
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
 *
 * Default: 3
 */
#ifndef PJSIP_MAX_STALE_COUNT
#   define PJSIP_MAX_STALE_COUNT	    3
#endif


/**
 * Specify support for IMS/3GPP digest AKA authentication version 1 and 2
 * (AKAv1-MD5 and AKAv2-MD5 respectively).
 *
 * Note that if this is enabled, application would need to link with
 * <b>libmilenage</b> library from \a third_party directory.
 *
 * Default: 0 (for now)
 */
#ifndef PJSIP_HAS_DIGEST_AKA_AUTH
#   define PJSIP_HAS_DIGEST_AKA_AUTH	    0
#endif


/**
 * Specify the number of seconds to refresh the client registration
 * before the registration expires.
 *
 * Default: 5 seconds
 */
#ifndef PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH
#   define PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH  5
#endif


/**
 * Specify whether client registration should check for its registered
 * contact in Contact header of successful REGISTE response to determine
 * whether registration has been successful. This setting may be disabled
 * if non-compliant registrar is unable to return correct Contact header.
 *
 * This setting can be changed in run-time by settting \a regc.check_contact
 * field of pjsip_cfg().
 *
 * Default is 1
 */
#ifndef PJSIP_REGISTER_CLIENT_CHECK_CONTACT
#   define PJSIP_REGISTER_CLIENT_CHECK_CONTACT	1
#endif


/**
 * Specify whether client registration should add "x-uid" extension
 * parameter in all Contact URIs that it registers to assist the
 * matching of Contact URIs in the 200/OK REGISTER response, in 
 * case the registrar is unable to return exact Contact URI in the
 * 200/OK response.
 *
 * This setting can be changed in run-time by setting 
 * \a regc.add_xuid_param field of pjsip_cfg().
 *
 * Default is 0.
 */
#ifndef PJSIP_REGISTER_CLIENT_ADD_XUID_PARAM
#   define PJSIP_REGISTER_CLIENT_ADD_XUID_PARAM	0
#endif


/**
 * Allow client to send refresh registration when the registrar sent a Contact
 * header with expire parameter 0 in the 200/OK REGISTER response.
 * Refer to https://github.com/pjsip/pjproject/pull/2809 for more info.
 *
 * Default is 1.
 */
#ifndef PJSIP_REGISTER_ALLOW_EXP_REFRESH
#   define PJSIP_REGISTER_ALLOW_EXP_REFRESH	1
#endif


/**
 * Maximum size of pool allowed for auth client session in pjsip_regc.
 * After the size exceeds because of Digest authentication processing,
 * the pool is reset.
 *
 * Default is 20 kB
 */
#ifndef PJSIP_AUTH_CACHED_POOL_MAX_SIZE
#   define PJSIP_AUTH_CACHED_POOL_MAX_SIZE	(20 * 1024)
#endif


/**
 * Specify whether the cnonce used for SIP authentication contain digits only.
 * The "cnonce" value is setup using GUID generator, i.e:
 * pj_create_unique_string(), and the GUID string may contain hyphen character
 * ("-"). Some SIP servers do not like this GUID format, so this option will
 * strip any hyphens from the GUID string.
 *
 * Default is 1 (cnonce will not contain any hyphen characters).
 */
#ifndef PJSIP_AUTH_CNONCE_USE_DIGITS_ONLY
#   define PJSIP_AUTH_CNONCE_USE_DIGITS_ONLY	1
#endif

/*****************************************************************************
 *  SIP Event framework and presence settings.
 */

/**
 * Specify the time (in seconds) to send SUBSCRIBE to refresh client 
 * subscription before the actual interval expires.
 *
 * Default: 5 seconds
 */
#ifndef PJSIP_EVSUB_TIME_UAC_REFRESH
#   define PJSIP_EVSUB_TIME_UAC_REFRESH		5
#endif


/**
 * Specify the time (in seconds) to send PUBLISH to refresh client 
 * publication before the actual interval expires.
 *
 * Default: 5 seconds
 */
#ifndef PJSIP_PUBLISHC_DELAY_BEFORE_REFRESH
#   define PJSIP_PUBLISHC_DELAY_BEFORE_REFRESH	5
#endif


/**
 * Specify the time (in seconds) to wait for the final NOTIFY from the
 * server after client has sent un-SUBSCRIBE request.
 *
 * Default: 5 seconds
 */
#ifndef PJSIP_EVSUB_TIME_UAC_TERMINATE
#   define PJSIP_EVSUB_TIME_UAC_TERMINATE	5
#endif


/**
 * Specify the time (in seconds) for client subscription to wait for another
 * NOTIFY from the server, if it has rejected the last NOTIFY with non-2xx
 * final response (such as 401). If further NOTIFY is not received within
 * this period, the client will unsubscribe.
 *
 * Default: 5 seconds
 */
#ifndef PJSIP_EVSUB_TIME_UAC_WAIT_NOTIFY
#   define PJSIP_EVSUB_TIME_UAC_WAIT_NOTIFY	5
#endif


/**
 * Specify the default expiration time for presence event subscription, for
 * both client and server subscription. For client subscription, application
 * can override this by specifying positive non-zero value in "expires" 
 * parameter when calling #pjsip_pres_initiate(). For server subscription,
 * we would take the expiration value from the Expires header sent by client
 * in the SUBSCRIBE request if the header exists and its value is less than 
 * this setting, otherwise this setting will be used.
 *
 * Default: 600 seconds (10 minutes)
 */
#ifndef PJSIP_PRES_DEFAULT_EXPIRES
#   define PJSIP_PRES_DEFAULT_EXPIRES		600
#endif


/**
 * Specify the status code value to respond to bad message body in NOTIFY
 * request for presence. Scenarios that are considered bad include non-
 * PIDF/XML and non-XPIDF/XML body, multipart message bodies without PIDF/XML
 * nor XPIDF/XML part, and bad (parsing error) PIDF and X-PIDF bodies
 * themselves.
 *
 * Default value is 488. Application may change this to 200 to ignore the
 * unrecognised content (this is useful if the application wishes to handle
 * the content itself). Only non-3xx final response code is allowed here.
 *
 * Default: 488 (Not Acceptable Here)
 */
#ifndef PJSIP_PRES_BAD_CONTENT_RESPONSE
#   define PJSIP_PRES_BAD_CONTENT_RESPONSE	488
#endif


/**
 * Add "timestamp" information in generated PIDF document for both server
 * subscription and presence publication.
 *
 * Default: 1 (yes)
 */
#ifndef PJSIP_PRES_PIDF_ADD_TIMESTAMP
#   define PJSIP_PRES_PIDF_ADD_TIMESTAMP	1
#endif


/**
 * Default session interval for Session Timer (RFC 4028) extension, in
 * seconds. As specified in RFC 4028 Section 4, this value must not be 
 * less than the absolute minimum for the Session-Expires header field
 * 90 seconds, and the recommended value is 1800 seconds.
 *
 * Default: 1800 seconds
 */
#ifndef PJSIP_SESS_TIMER_DEF_SE
#   define PJSIP_SESS_TIMER_DEF_SE		1800
#endif


/**
 * Default delay for retrying session refresh request upon
 * receiving transport error (503). Set it to -1 to end the session
 * immediately instead.
 *
 * Default: 10 seconds
 */
#ifndef PJSIP_SESS_TIMER_RETRY_DELAY
#   define PJSIP_SESS_TIMER_RETRY_DELAY		10
#endif


/**
 * Specify whether the client publication session should queue the
 * PUBLISH request should there be another PUBLISH transaction still
 * pending. If this is set to false, the client will return error
 * on the PUBLISH request if there is another PUBLISH transaction still
 * in progress.
 *
 * Default: 1 (yes)
 */
#ifndef PJSIP_PUBLISHC_QUEUE_REQUEST
#   define PJSIP_PUBLISHC_QUEUE_REQUEST		1
#endif


/**
 * Specify the default expiration time for Message Waiting Indication
 * (RFC 3842) event subscription, for both client and server subscription.
 * For client subscription, application can override this by specifying
 * positive non-zero value in "expires" parameter when calling
 * #pjsip_mwi_initiate(). For server subscription, we would take the
 * expiration value from the Expires header sent by client in the SUBSCRIBE
 * request if the header exists and its value is less than  this setting,
 * otherwise this setting will be used.
 *
 * Default: 3600 seconds
 */
#ifndef PJSIP_MWI_DEFAULT_EXPIRES
#   define PJSIP_MWI_DEFAULT_EXPIRES		3600
#endif


/**
 * Specify whether transport manager should maintain a list of transmit
 * buffer instances, so any possible dangling instance can be cleaned up
 * when the transport manager is shutdown (see also ticket #1671).
 * Note that this feature will have slight impact on the performance as
 * mutex is employed in updating the list, i.e: on creation and destruction
 * of transmit data.
 *
 * Default: 0 (no)
 */
#ifndef PJSIP_HAS_TX_DATA_LIST
#   define PJSIP_HAS_TX_DATA_LIST		0
#endif

/** 
 * Specify whether to accept INVITE/re-INVITE with unknown content type,
 * by default the stack will reject this type of message as specified in 
 * RFC3261 section 8.2.3.
 * Application that wishes to process the body could set this to PJ_TRUE,
 * be informed that SDP offer/answer will still be present.
 *
 * Default: PJ_FALSE
 */
#ifndef PJSIP_INV_ACCEPT_UNKNOWN_BODY
#   define PJSIP_INV_ACCEPT_UNKNOWN_BODY    PJ_FALSE
#endif

PJ_END_DECL

/**
 * @}
 */


#include <pj/config.h>


#endif	/* __PJSIP_SIP_CONFIG_H__ */

