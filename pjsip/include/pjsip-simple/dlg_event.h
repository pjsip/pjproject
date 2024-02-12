/*
 * Copyright (C) 2024 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJSIP_SIMPLE_DLG_EVENT_H__
#define __PJSIP_SIMPLE_DLG_EVENT_H__

/**
 * @file dlg_event.h
 * @brief SIP Extension for INVITE-Initiated Dialog Event (RFC 4235)
 */
#include <pjsip-simple/evsub.h>
#include <pjsip-simple/dialog_info.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJSIP_SIMPLE_DLG_EVENT SIP Extension for Dialog Event (RFC 4235)
 * @ingroup PJSIP_SIMPLE
 * @brief Support for SIP Extension for Dialog Event (RFC 4235)
 * @{
 *
 * This module contains the implementation of INVITE-Initiated Dialog Event
 * Package as described in RFC 4235. It uses the SIP Event Notification
 * framework (evsub.h) and extends the framework by implementing
 * "dialog-info+xml" event package. This could be useful for features such as
 * Busy Lamp Field (BLF).
 */


/**
 * Initialize the dialog event module and register it as endpoint module and
 * package to the event subscription module.
 *
 * @param endpt     The endpoint instance.
 * @param mod_evsub The event subscription module instance.
 *
 * @return          PJ_SUCCESS if the module is successfully
 *                  initialized and registered to both endpoint
 *                  and the event subscription module.
 */
PJ_DECL(pj_status_t) pjsip_dlg_event_init_module(pjsip_endpoint *endpt,
                                                 pjsip_module *mod_evsub);


/**
 * Get the dialog event module instance.
 *
 * @return          The dialog event module instance.
 */
PJ_DECL(pjsip_module*) pjsip_bdlg_event_instance(void);


/**
 * Maximum dialog event status info items which can handled by application.
 *
 */
#define PJSIP_DLG_EVENT_STATUS_MAX_INFO  8


/**
 * This structure describes dialog event status of an entity.
 */
struct pjsip_dlg_event_status
{
    unsigned        info_cnt;             /**< Number of info in the status */
    struct {

        pj_str_t    dialog_info_state;    /**< Dialog-Info state            */
        pj_str_t    dialog_info_entity;   /**< Dialog-Info entity           */
        pj_str_t    dialog_call_id;       /**< Dialog's call_id             */
        pj_str_t    dialog_remote_tag;    /**< Dialog's remote-tag          */
        pj_str_t    dialog_local_tag;     /**< Dialog's local-tag           */
        pj_str_t    dialog_direction;     /**< Dialog's direction           */
        pj_str_t    dialog_id;            /**< Dialog's id                  */
        pj_str_t    dialog_state;         /**< Dialog state                 */
        pj_str_t    dialog_duration;      /**< Dialog duration              */

        pj_xml_node *dialog_node;         /**< Pointer to tuple XML node of
                                               parsed dialog-info body received
                                               from remote agent. Only valid
                                               for client subscription. If the
                                               last received NOTIFY request
                                               does not contain any dialog-info
                                               body, this will be set to NULL*/
        pj_str_t    local_identity;
        pj_str_t    local_identity_display;
        pj_str_t    local_target_uri;

        pj_str_t    remote_identity;
        pj_str_t    remote_identity_display;
        pj_str_t    remote_target_uri;

    } info[PJSIP_DLG_EVENT_STATUS_MAX_INFO]; /**< Array of info.            */
};


/**
 * @see pjsip_dlg_event_status
 */
typedef struct pjsip_dlg_event_status pjsip_dlg_event_status;


/**
 * Create dialog event client subscription session.
 *
 * @param dlg       The underlying dialog to use.
 * @param user_cb   Pointer to callbacks to receive dialog events.
 * @param options   Option flags. Currently only PJSIP_EVSUB_NO_EVENT_ID
 *                  is recognized.
 * @param p_evsub   Pointer to receive the dialog event subscription
 *                  session.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjsip_dlg_event_create_uac(pjsip_dialog *dlg,
                           const pjsip_evsub_user *user_cb,
                           unsigned options,
                           pjsip_evsub **p_evsub );


/**
 * Forcefully destroy the dialog event subscription. This function should only
 * be called on special condition, such as when the subscription
 * initialization has failed. For other conditions, application MUST terminate
 * the subscription by sending the appropriate un(SUBSCRIBE) or NOTIFY.
 *
 * @param sub       The dialog event subscription.
 * @param notify    Specify whether the state notification callback
 *                  should be called.
 *
 * @return          PJ_SUCCESS if subscription session has been destroyed.
 */
PJ_DECL(pj_status_t) pjsip_dlg_event_terminate(pjsip_evsub *sub,
                                               pj_bool_t notify );



/**
 * Call this function to create request to initiate dialog event subscription,
 * to refresh subcription, or to request subscription termination.
 *
 * @param sub       Client subscription instance.
 * @param expires   Subscription expiration. If the value is set to zero,
 *                  this will request unsubscription.
 * @param p_tdata   Pointer to receive the request.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_dlg_event_initiate(pjsip_evsub *sub,
                                              pj_int32_t expires,
                                              pjsip_tx_data **p_tdata);


/**
 * Add a list of headers to the subscription instance. The list of headers
 * will be added to outgoing dialog event subscription requests.
 *
 * @param sub       Subscription instance.
 * @param hdr_list  List of headers to be added.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_dlg_event_add_header(pjsip_evsub *sub,
                                                const pjsip_hdr *hdr_list);


/**
 * Accept the incoming subscription request by sending 2xx response to
 * incoming SUBSCRIBE request.
 *
 * @param sub       Server subscription instance.
 * @param rdata     The incoming subscription request message.
 * @param st_code   Status code, which MUST be final response.
 * @param hdr_list  Optional list of headers to be added in the response.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_dlg_event_accept(pjsip_evsub *sub,
                                            pjsip_rx_data *rdata,
                                            int st_code,
                                            const pjsip_hdr *hdr_list );

/**
 * Send request message. Application may also send request created with other
 * functions, e.g. authentication. But the request MUST be either request
 * that creates/refresh subscription or NOTIFY request.
 *
 * @param sub       The subscription object.
 * @param tdata     Request message to be sent.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_dlg_event_send_request(pjsip_evsub *sub,
                                                  pjsip_tx_data *tdata);


/**
 * Get the dialog event status. Client normally would call this function
 * after receiving NOTIFY request from server.
 *
 * @param sub       The client or server subscription.
 * @param status    The structure to receive dialog event status.
 *
 * @return          PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjsip_dlg_event_get_status(pjsip_evsub *sub, pjsip_dlg_event_status *status);


/**
 * This is a utility function to parse XML body into PJSIP dialog event status.
 *
 * @param rdata     The incoming SIP message containing the XML body.
 * @param pool      Pool to allocate memory to copy the strings into
 *                  the dialog event status structure.
 * @param status    The dialog event status to be initialized.
 *
 * @return          PJ_SUCCESS on success.
 *
 * @see pjsip_dlg_event_parse_dialog_info()
 */
PJ_DECL(pj_status_t)
pjsip_dlg_event_parse_dialog_info(pjsip_rx_data *rdata,
                                  pj_pool_t *pool,
                                  pjsip_dlg_event_status *dlg_status);


/**
 * This is a utility function to parse XML body into PJSIP dialog event status.
 *
 * @param body          Text body, with one extra space at the end to place
 *                      NULL character temporarily during parsing.
 * @param body_len      Length of the body, not including the NULL termination
 *                      character.
 * @param pool          Pool to allocate memory to copy the strings into
 *                      the dialog event status structure.
 * @param status        The dialog event status to be initialized.
 *
 * @return              PJ_SUCCESS on success.
 *
 * @see pjsip_dlg_event_parse_dialog_info2()
 */
PJ_DECL(pj_status_t)
pjsip_dlg_event_parse_dialog_info2(char *body, unsigned body_len,
                                   pj_pool_t *pool,
                                   pjsip_dlg_event_status *dlg_status);


/**
 * @}
 */

PJ_END_DECL


#endif  /* __PJSIP_SIMPLE_DLG_EVENT_H__ */
