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

#include <pjsip/sip_config.h>
#include <pj/log.h>

static const char *id = "sip_config.c";

/* pjsip configuration instance, initialized with default values */
pjsip_cfg_t pjsip_sip_cfg_var =
{
    /* Global settings */
    {
       PJSIP_ALLOW_PORT_IN_FROMTO_HDR,
       PJSIP_ACCEPT_REPLACE_IN_EARLY_STATE,
       0,
       0,
       PJSIP_DONT_SWITCH_TO_TCP,
       PJSIP_DONT_SWITCH_TO_TLS,
       PJSIP_FOLLOW_EARLY_MEDIA_FORK,
       PJSIP_REQ_HAS_VIA_ALIAS,
       PJSIP_RESOLVE_HOSTNAME_TO_GET_INTERFACE,
       0,
       PJSIP_ENCODE_SHORT_HNAME,
       PJSIP_ACCEPT_MULTIPLE_SDP_ANSWERS,
       0
    },

    /* Transaction settings */
    {
       PJSIP_MAX_TSX_COUNT,
       PJSIP_T1_TIMEOUT,
       PJSIP_T2_TIMEOUT,
       PJSIP_T4_TIMEOUT,
       PJSIP_TD_TIMEOUT
    },

    /* Client registration client */
    {
        PJSIP_REGISTER_CLIENT_CHECK_CONTACT
    },

    /* TCP transport settings */
    {
        PJSIP_TCP_KEEP_ALIVE_INTERVAL
    },

    /* TLS transport settings */
    {
        PJSIP_TLS_KEEP_ALIVE_INTERVAL
    }
};

PJ_DEF(void) pjsip_dump_config(void)
{
    PJ_LOG(3, (id, "Dumping PJSIP configurations:"));
    PJ_LOG(3, (id, " PJSIP_MAX_DIALOG_COUNT                             : %d", 
               PJSIP_MAX_DIALOG_COUNT));
    PJ_LOG(3, (id, " PJSIP_MAX_TRANSPORTS                               : %d", 
               PJSIP_MAX_TRANSPORTS));
    PJ_LOG(3, (id, " PJSIP_TPMGR_HTABLE_SIZE                            : %d", 
               PJSIP_TPMGR_HTABLE_SIZE));
    PJ_LOG(3, (id, " PJSIP_MAX_URL_SIZE                                 : %d", 
               PJSIP_MAX_URL_SIZE));
    PJ_LOG(3, (id, " PJSIP_MAX_MODULE                                   : %d", 
               PJSIP_MAX_MODULE));
    PJ_LOG(3, (id, " PJSIP_MAX_PKT_LEN                                  : %d", 
               PJSIP_MAX_PKT_LEN));
    PJ_LOG(3, (id, " PJSIP_HANDLE_EVENTS_HAS_SLEEP_ON_ERR               : %d", 
               PJSIP_HANDLE_EVENTS_HAS_SLEEP_ON_ERR));
    PJ_LOG(3, (id, " PJSIP_ACCEPT_MULTIPLE_SDP_ANSWERS                  : %d", 
               PJSIP_ACCEPT_MULTIPLE_SDP_ANSWERS));
    PJ_LOG(3, (id, " PJSIP_UDP_SIZE_THRESHOLD                           : %d", 
               PJSIP_UDP_SIZE_THRESHOLD));
    PJ_LOG(3, (id, " PJSIP_INCLUDE_ALLOW_HDR_IN_DLG                     : %d", 
               PJSIP_INCLUDE_ALLOW_HDR_IN_DLG));
    PJ_LOG(3, (id, " PJSIP_SAFE_MODULE                                  : %d", 
               PJSIP_SAFE_MODULE));
    PJ_LOG(3, (id, " PJSIP_CHECK_VIA_SENT_BY                            : %d", 
               PJSIP_CHECK_VIA_SENT_BY));
    PJ_LOG(3, (id, " PJSIP_UNESCAPE_IN_PLACE                            : %d", 
               PJSIP_UNESCAPE_IN_PLACE));
    PJ_LOG(3, (id, " PJSIP_MAX_NET_EVENTS                               : %d", 
               PJSIP_MAX_NET_EVENTS));
    PJ_LOG(3, (id, " PJSIP_MAX_TIMED_OUT_ENTRIES                        : %d", 
               PJSIP_MAX_TIMED_OUT_ENTRIES));
    PJ_LOG(3, (id, " PJSIP_TRANSPORT_IDLE_TIME                          : %d", 
               PJSIP_TRANSPORT_IDLE_TIME));
    PJ_LOG(3, (id, " PJSIP_TRANSPORT_SERVER_IDLE_TIME                   : %d",
               PJSIP_TRANSPORT_SERVER_IDLE_TIME));
    PJ_LOG(3, (id, " PJSIP_TRANSPORT_SERVER_IDLE_TIME_FIRST             : %d",
               PJSIP_TRANSPORT_SERVER_IDLE_TIME_FIRST));
    PJ_LOG(3, (id, " PJSIP_MAX_TRANSPORT_USAGE                          : %d", 
               PJSIP_MAX_TRANSPORT_USAGE));
    PJ_LOG(3, (id, " PJSIP_TCP_TRANSPORT_BACKLOG                        : %d", 
               PJSIP_TCP_TRANSPORT_BACKLOG));
    PJ_LOG(3, (id, " PJSIP_TCP_TRANSPORT_REUSEADDR                      : %d", 
               PJSIP_TCP_TRANSPORT_REUSEADDR));
    PJ_LOG(3, (id, " PJSIP_TCP_TRANSPORT_DONT_CREATE_LISTENER           : %d", 
               PJSIP_TCP_TRANSPORT_DONT_CREATE_LISTENER));
    PJ_LOG(3, (id, " PJSIP_TLS_TRANSPORT_DONT_CREATE_LISTENER           : %d", 
               PJSIP_TLS_TRANSPORT_DONT_CREATE_LISTENER));
    PJ_LOG(3, (id, " PJSIP_TCP_KEEP_ALIVE_INTERVAL                      : %d", 
               PJSIP_TCP_KEEP_ALIVE_INTERVAL));
    PJ_LOG(3, (id, " PJSIP_POOL_INC_TRANSPORT                           : %d", 
               PJSIP_POOL_INC_TRANSPORT));
    PJ_LOG(3, (id, " PJSIP_POOL_LEN_TDATA                               : %d", 
               PJSIP_POOL_LEN_TDATA));
    PJ_LOG(3, (id, " PJSIP_POOL_INC_TDATA                               : %d", 
               PJSIP_POOL_INC_TDATA));
    PJ_LOG(3, (id, " PJSIP_POOL_LEN_UA                                  : %d", 
               PJSIP_POOL_LEN_UA));
    PJ_LOG(3, (id, " PJSIP_POOL_INC_UA                                  : %d", 
               PJSIP_POOL_INC_UA));
    PJ_LOG(3, (id, " PJSIP_POOL_EVSUB_LEN                               : %d", 
               PJSIP_POOL_EVSUB_LEN));
    PJ_LOG(3, (id, " PJSIP_POOL_EVSUB_INC                               : %d", 
               PJSIP_POOL_EVSUB_INC));
    PJ_LOG(3, (id, " PJSIP_MAX_FORWARDS_VALUE                           : %d", 
               PJSIP_MAX_FORWARDS_VALUE));
    PJ_LOG(3, (id, " PJSIP_RFC3261_BRANCH_ID                            : %s", 
               PJSIP_RFC3261_BRANCH_ID));
    PJ_LOG(3, (id, " PJSIP_RFC3261_BRANCH_LEN                           : %d", 
               PJSIP_RFC3261_BRANCH_LEN));
    PJ_LOG(3, (id, " PJSIP_POOL_TSX_LAYER_LEN                           : %d", 
               PJSIP_POOL_TSX_LAYER_LEN));
    PJ_LOG(3, (id, " PJSIP_POOL_TSX_LAYER_INC                           : %d", 
               PJSIP_POOL_TSX_LAYER_INC));
    PJ_LOG(3, (id, " PJSIP_POOL_TSX_LEN                                 : %d", 
               PJSIP_POOL_TSX_LEN));
    PJ_LOG(3, (id, " PJSIP_POOL_TSX_INC                                 : %d", 
               PJSIP_POOL_TSX_INC));
    PJ_LOG(3, (id, " PJSIP_TSX_1XX_RETRANS_DELAY                        : %d", 
               PJSIP_TSX_1XX_RETRANS_DELAY));
    PJ_LOG(3, (id, " PJSIP_TSX_UAS_CONTINUE_ON_TP_ERROR                 : %d", 
               PJSIP_TSX_UAS_CONTINUE_ON_TP_ERROR));
    PJ_LOG(3, (id, " PJSIP_MAX_TSX_KEY_LEN                              : %d", 
               PJSIP_MAX_TSX_KEY_LEN));
    PJ_LOG(3, (id, " PJSIP_POOL_LEN_USER_AGENT                          : %d", 
               PJSIP_POOL_LEN_USER_AGENT));
    PJ_LOG(3, (id, " PJSIP_POOL_INC_USER_AGENT                          : %d", 
               PJSIP_POOL_INC_USER_AGENT));
    PJ_LOG(3, (id, " PJSIP_MAX_BRANCH_LEN                               : %d", 
               PJSIP_MAX_HNAME_LEN));
    PJ_LOG(3, (id, " PJSIP_POOL_LEN_DIALOG                              : %d", 
               PJSIP_POOL_LEN_DIALOG));
    PJ_LOG(3, (id, " PJSIP_POOL_INC_DIALOG                              : %d", 
               PJSIP_POOL_INC_DIALOG));
    PJ_LOG(3, (id, " PJSIP_MAX_HEADER_TYPES                             : %d", 
               PJSIP_MAX_HEADER_TYPES));
    PJ_LOG(3, (id, " PJSIP_MAX_URI_TYPES                                : %d", 
               PJSIP_MAX_URI_TYPES));
    PJ_LOG(3, (id, " PJSIP_AUTH_HEADER_CACHING                          : %d", 
               PJSIP_AUTH_HEADER_CACHING));
    PJ_LOG(3, (id, " PJSIP_AUTH_AUTO_SEND_NEXT                          : %d", 
               PJSIP_AUTH_AUTO_SEND_NEXT));
    PJ_LOG(3, (id, " PJSIP_AUTH_QOP_SUPPORT                             : %d", 
               PJSIP_AUTH_QOP_SUPPORT));
    PJ_LOG(3, (id, " PJSIP_MAX_STALE_COUNT                              : %d", 
               PJSIP_MAX_STALE_COUNT));
    PJ_LOG(3, (id, " PJSIP_HAS_DIGEST_AKA_AUTH                          : %d", 
               PJSIP_HAS_DIGEST_AKA_AUTH));
    PJ_LOG(3, (id, " PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH         : %d", 
               PJSIP_REGISTER_CLIENT_DELAY_BEFORE_REFRESH));
    PJ_LOG(3, (id, " PJSIP_REGISTER_ALLOW_EXP_REFRESH                   : %d", 
               PJSIP_REGISTER_ALLOW_EXP_REFRESH));
    PJ_LOG(3, (id, " PJSIP_AUTH_CACHED_POOL_MAX_SIZE                    : %d", 
               PJSIP_AUTH_CACHED_POOL_MAX_SIZE));
    PJ_LOG(3, (id, " PJSIP_AUTH_CNONCE_USE_DIGITS_ONLY                  : %d", 
               PJSIP_AUTH_CNONCE_USE_DIGITS_ONLY));
    PJ_LOG(3, (id, " PJSIP_AUTH_ALLOW_MULTIPLE_AUTH_HEADER              : %d", 
               PJSIP_AUTH_ALLOW_MULTIPLE_AUTH_HEADER));
    PJ_LOG(3, (id, " PJSIP_EVSUB_TIME_UAC_REFRESH                       : %d", 
               PJSIP_EVSUB_TIME_UAC_REFRESH));
    PJ_LOG(3, (id, " PJSIP_PUBLISHC_DELAY_BEFORE_REFRESH                : %d", 
               PJSIP_PUBLISHC_DELAY_BEFORE_REFRESH));
    PJ_LOG(3, (id, " PJSIP_EVSUB_TIME_UAC_TERMINATE                     : %d", 
               PJSIP_EVSUB_TIME_UAC_TERMINATE));
    PJ_LOG(3, (id, " PJSIP_EVSUB_TIME_UAC_WAIT_NOTIFY                   : %d", 
               PJSIP_EVSUB_TIME_UAC_WAIT_NOTIFY));
    PJ_LOG(3, (id, " PJSIP_PRES_DEFAULT_EXPIRES                         : %d", 
               PJSIP_PRES_DEFAULT_EXPIRES));
    PJ_LOG(3, (id, " PJSIP_PRES_BAD_CONTENT_RESPONSE                    : %d", 
               PJSIP_PRES_BAD_CONTENT_RESPONSE));
    PJ_LOG(3, (id, " PJSIP_PRES_PIDF_ADD_TIMESTAMP                      : %d", 
               PJSIP_PRES_PIDF_ADD_TIMESTAMP));
    PJ_LOG(3, (id, " PJSIP_SESS_TIMER_DEF_SE                            : %d", 
               PJSIP_SESS_TIMER_DEF_SE));
    PJ_LOG(3, (id, " PJSIP_SESS_TIMER_RETRY_DELAY                       : %d", 
               PJSIP_SESS_TIMER_RETRY_DELAY));
    PJ_LOG(3, (id, " PJSIP_PUBLISHC_QUEUE_REQUEST                       : %d", 
               PJSIP_PUBLISHC_QUEUE_REQUEST));
    PJ_LOG(3, (id, " PJSIP_MWI_DEFAULT_EXPIRES                          : %d", 
               PJSIP_MWI_DEFAULT_EXPIRES));
    PJ_LOG(3, (id, " PJSIP_HAS_TX_DATA_LIST                             : %d", 
               PJSIP_HAS_TX_DATA_LIST));
    PJ_LOG(3, (id, " PJSIP_INV_ACCEPT_UNKNOWN_BODY                      : %d", 
               PJSIP_INV_ACCEPT_UNKNOWN_BODY));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.allow_port_in_fromto_hdr        : %d", 
               pjsip_cfg()->endpt.allow_port_in_fromto_hdr));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.accept_replace_in_early_state   : %d", 
               pjsip_cfg()->endpt.accept_replace_in_early_state));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.allow_tx_hash_in_uri            : %d", 
               pjsip_cfg()->endpt.allow_tx_hash_in_uri));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.disable_rport                   : %d", 
               pjsip_cfg()->endpt.disable_rport));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.disable_tcp_switch              : %d", 
               pjsip_cfg()->endpt.disable_tcp_switch));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.disable_tls_switch              : %d", 
               pjsip_cfg()->endpt.disable_tls_switch));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.follow_early_media_fork         : %d", 
               pjsip_cfg()->endpt.follow_early_media_fork));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.req_has_via_alias               : %d", 
               pjsip_cfg()->endpt.req_has_via_alias));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.resolve_hostname_to_get_interface:%d", 
               pjsip_cfg()->endpt.resolve_hostname_to_get_interface));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.disable_secure_dlg_check        : %d", 
               pjsip_cfg()->endpt.disable_secure_dlg_check));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.use_compact_form                : %d", 
               pjsip_cfg()->endpt.use_compact_form));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.accept_multiple_sdp_answers     : %d", 
               pjsip_cfg()->endpt.accept_multiple_sdp_answers));
    PJ_LOG(3, (id, " pjsip_cfg()->endpt.keep_inv_after_tsx_timeout      : %d", 
               pjsip_cfg()->endpt.keep_inv_after_tsx_timeout));
    PJ_LOG(3, (id, " pjsip_cfg()->tsx.max_count                         : %d", 
               pjsip_cfg()->tsx.max_count));
    PJ_LOG(3, (id, " pjsip_cfg()->tsx.t1                                : %d", 
               pjsip_cfg()->tsx.t1));
    PJ_LOG(3, (id, " pjsip_cfg()->tsx.t2                                : %d", 
               pjsip_cfg()->tsx.t2));
    PJ_LOG(3, (id, " pjsip_cfg()->tsx.t4                                : %d", 
               pjsip_cfg()->tsx.t4));
    PJ_LOG(3, (id, " pjsip_cfg()->td                                    : %d", 
               pjsip_cfg()->tsx.td));
    PJ_LOG(3, (id, " pjsip_cfg()->regc.check_contact                    : %d", 
               pjsip_cfg()->regc.check_contact));
    PJ_LOG(3, (id, " pjsip_cfg()->regc.add_xuid_param                   : %d", 
               pjsip_cfg()->regc.add_xuid_param));
    PJ_LOG(3, (id, " pjsip_cfg()->tcp.keep_alive_interval               : %ld", 
               pjsip_cfg()->tcp.keep_alive_interval));
    PJ_LOG(3, (id, " pjsip_cfg()->tls.keep_alive_interval               : %ld", 
               pjsip_cfg()->tls.keep_alive_interval));
}


#ifdef PJ_DLL
PJ_DEF(pjsip_cfg_t*) pjsip_cfg(void)
{
    return &pjsip_sip_cfg_var;
}
#endif
