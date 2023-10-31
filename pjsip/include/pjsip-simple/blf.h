/*
 * Copyright (C) 2013 Maxim Kondratenko <max.kondr@gmail.com>
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
#ifndef __PJSIP_SIMPLE_BLF_H__
#define __PJSIP_SIMPLE_BLF_H__

/**
 * @file blf.h
 * @brief SIP Extension for blf (RFC 4235)
 */
#include <pjsip-simple/evsub.h>
#include <pjsip-simple/dialog-info.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJSIP_SIMPLE_BLF SIP Extension for BLF (RFC 4235)
 * @ingroup PJSIP_SIMPLE
 * @brief Support for SIP Extension for BLF (RFC 4235)
 * @{
 *
 * This module contains the implementation of SIP Presence Extension as
 * described in RFC 3856. It uses the SIP Event Notification framework
 * (evsub.h) and extends the framework by implementing "presence"
 * event package.
 */


/**
 * Initialize the blf module and register it as endpoint module and
 * package to the event subscription module.
 *
 * @param endpt     The endpoint instance.
 * @param mod_evsub The event subscription module instance.
 *
 * @return      PJ_SUCCESS if the module is successfully
 *          initialized and registered to both endpoint
 *          and the event subscription module.
 */
PJ_DECL(pj_status_t) pjsip_blf_init_module(pjsip_endpoint *endpt,
                        pjsip_module *mod_evsub);


/**
 * Get the presence module instance.
 *
 * @return      The presence module instance.
 */
PJ_DECL(pjsip_module*) pjsip_blf_instance(void);


/**
 * Maximum blf status info.
 */
#define PJSIP_BLF_STATUS_MAX_INFO  8


/**
 * This structure describes blf status of a presentity.
 */
struct pjsip_blf_status
{
    unsigned        info_cnt;                   /**< Number of info in the status.  */
    struct {

            pj_str_t   dialog_info_state;       /**< Dialog-Info state              */
            pj_str_t   dialog_info_entity;       /**< Dialog-Info entity              */
            pj_str_t   dialog_call_id;          /**< Dialog's call_id      */
            pj_str_t   dialog_remote_tag;       /**< Dialog's remote-tag      */
            pj_str_t   dialog_local_tag;       /**< Dialog's local-tag      */
            pj_str_t   dialog_direction;        /**< Dialog's direction      */
            pj_str_t   dialog_id;               /**< Dialog's id      */
            pj_str_t   dialog_state;            /**< Dialog state              */
            pj_str_t   dialog_duration;         /**< Dialog duration      */

            pj_xml_node    *dialog_node;             /**< Pointer to tuple XML node of
                                                parsed dialog-info body received from
                                                remote agent. Only valid for
                                                client subscription. If the
                                                last received NOTIFY request
                                                does not contain any dialog-info body,
                                                this valud will be set to NULL */
            pj_str_t    local_identity;
            pj_str_t    local_identity_display;
            pj_str_t    local_target_uri;

            pj_str_t    remote_identity;
            pj_str_t    remote_identity_display;
            pj_str_t    remote_target_uri;


    } info[PJSIP_BLF_STATUS_MAX_INFO];          /**< Array of info.         */

    pj_bool_t       _is_valid;                  /**< Internal flag.         */
};


/**
 * @see pjsip_pres_status
 */
typedef struct pjsip_blf_status pjsip_blf_status;


/**
 * Create blf client subscription session.
 *
 * @param dlg       The underlying dialog to use.
 * @param user_cb   Pointer to callbacks to receive presence subscription
 *          events.
 * @param options   Option flags. Currently only PJSIP_EVSUB_NO_EVENT_ID
 *          is recognized.
 * @param p_evsub   Pointer to receive the presence subscription
 *          session.
 *
 * @return      PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_blf_create_uac( pjsip_dialog *dlg,
                        const pjsip_evsub_blf_user *user_cb,
                        unsigned options,
                        pjsip_evsub **p_evsub );


/**
 * Forcefully destroy the presence subscription. This function should only
 * be called on special condition, such as when the subscription
 * initialization has failed. For other conditions, application MUST terminate
 * the subscription by sending the appropriate un(SUBSCRIBE) or NOTIFY.
 *
 * @param sub       The presence subscription.
 * @param notify    Specify whether the state notification callback
 *          should be called.
 *
 * @return      PJ_SUCCESS if subscription session has been destroyed.
 */
PJ_DECL(pj_status_t) pjsip_blf_terminate( pjsip_evsub *sub,
                       pj_bool_t notify );



/**
 * Call this function to create request to initiate presence subscription, to
 * refresh subcription, or to request subscription termination.
 *
 * @param sub       Client subscription instance.
 * @param expires   Subscription expiration. If the value is set to zero,
 *          this will request unsubscription.
 * @param p_tdata   Pointer to receive the request.
 *
 * @return      PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_blf_initiate( pjsip_evsub *sub,
                      pj_int32_t expires,
                      pjsip_tx_data **p_tdata);


/**
 * Add a list of headers to the subscription instance. The list of headers
 * will be added to outgoing presence subscription requests.
 *
 * @param sub       Subscription instance.
 * @param hdr_list  List of headers to be added.
 *
 * @return      PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_blf_add_header( pjsip_evsub *sub,
                        const pjsip_hdr *hdr_list );


/**
 * Accept the incoming subscription request by sending 2xx response to
 * incoming SUBSCRIBE request.
 *
 * @param sub       Server subscription instance.
 * @param rdata     The incoming subscription request message.
 * @param st_code   Status code, which MUST be final response.
 * @param hdr_list  Optional list of headers to be added in the response.
 *
 * @return      PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_blf_accept( pjsip_evsub *sub,
                    pjsip_rx_data *rdata,
                        int st_code,
                    const pjsip_hdr *hdr_list );

/**
 * Send request message that was previously created with initiate(), notify(),
 * or current_notify(). Application may also send request created with other
 * functions, e.g. authentication. But the request MUST be either request
 * that creates/refresh subscription or NOTIFY request.
 *
 * @param sub       The subscription object.
 * @param tdata     Request message to be sent.
 *
 * @return      PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_blf_send_request( pjsip_evsub *sub,
                          pjsip_tx_data *tdata );


/**
 * Get the presence status. Client normally would call this function
 * after receiving NOTIFY request from server.
 *
 * @param sub       The client or server subscription.
 * @param status    The structure to receive presence status.
 *
 * @return      PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_blf_get_status( pjsip_evsub *sub,
                        pjsip_blf_status *status );


/**
 * This is a utility function to parse PIDF body into PJSIP presence status.
 *
 * @param rdata     The incoming SIP message containing the PIDF body.
 * @param pool      Pool to allocate memory to copy the strings into
 *          the presence status structure.
 * @param status    The presence status to be initialized.
 *
 * @return      PJ_SUCCESS on success.
 *
 * @see pjsip_pres_parse_pidf2()
 */
PJ_DECL(pj_status_t) pjsip_blf_parse_dialog_info(pjsip_rx_data *rdata,
                       pj_pool_t *pool,
                       pjsip_blf_status *blf_status);

/**
 * @}
 */

PJ_END_DECL


#endif  /* __PJSIP_SIMPLE_BLF_H__ */
