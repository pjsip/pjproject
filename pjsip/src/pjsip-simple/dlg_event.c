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
#include <pjsip-simple/dlg_event.h>
#include <pjsip-simple/errno.h>
#include <pjsip-simple/evsub_msg.h>
#include <pjsip/sip_module.h>
#include <pjsip/sip_multipart.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_dialog.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>


#define THIS_FILE                   "dlg_event.c"
#define DLG_EVENT_DEFAULT_EXPIRES   PJSIP_DLG_EVENT_DEFAULT_EXPIRES

#if PJSIP_DLG_EVENT_BAD_CONTENT_RESPONSE < 200 || \
    PJSIP_DLG_EVENT_BAD_CONTENT_RESPONSE > 699 || \
    PJSIP_DLG_EVENT_BAD_CONTENT_RESPONSE/100 == 3
# error Invalid PJSIP_DLG_EVENT_BAD_CONTENT_RESPONSE value
#endif

/*
 * Dialog event  module (mod-dlg_event)
 */
static struct pjsip_module mod_dlg_event =
{
    NULL, NULL,         /* prev, next.              */
    { "mod-dlg_event", 12 }, /* Name.               */
    -1,                 /* Id                       */
    PJSIP_MOD_PRIORITY_DIALOG_USAGE,/* Priority     */
    NULL,               /* load()                   */
    NULL,               /* start()                  */
    NULL,               /* stop()                   */
    NULL,               /* unload()                 */
    NULL,               /* on_rx_request()          */
    NULL,               /* on_rx_response()         */
    NULL,               /* on_tx_request.           */
    NULL,               /* on_tx_response()         */
    NULL,               /* on_tsx_state()           */
};


/*
 * Dialog event message body type.
 */
typedef enum content_type_e
{
    CONTENT_TYPE_NONE,
    CONTENT_TYPE_DIALOG_INFO,
} content_type_e;

/*
 * This structure describe an entity, for both subscriber and notifier.
 */
struct pjsip_dlg_event
{
    pjsip_evsub     *sub;           /**< Event subscription record.         */
    pjsip_dialog    *dlg;           /**< The dialog.                        */
    content_type_e   content_type;  /**< Content-Type.                      */
    pj_pool_t       *status_pool;   /**< Pool for dlgev_status              */
    pjsip_dlg_event_status status;  /**< Dialog event status.               */
    pj_pool_t       *tmp_pool;      /**< Pool for tmp_status                */
    pjsip_dlg_event_status tmp_status; /**< Temp, before NOTIFY is answered */
    pj_bool_t        is_ts_valid;   /**< Is tmp_status valid?               */
    pjsip_evsub_user user_cb;       /**< The user callback.                 */
    pj_mutex_t      *mutex;         /**< Mutex                              */
};

typedef struct pjsip_dlg_event pjsip_dlg_event;

/*
 * Forward decl for evsub callback.
 */
static void dlg_event_on_evsub_state(pjsip_evsub *sub, pjsip_event *event);
static void dlg_event_on_evsub_tsx_state(pjsip_evsub *sub, pjsip_transaction *tsx,
                                         pjsip_event *event);
static void dlg_event_on_evsub_rx_notify(pjsip_evsub *sub,
                                         pjsip_rx_data *rdata,
                                         int *p_st_code,
                                         pj_str_t **p_st_text,
                                         pjsip_hdr *res_hdr,
                                         pjsip_msg_body **p_body);
static void dlg_event_on_evsub_client_refresh(pjsip_evsub *sub);


/*
 * Event subscription callback for dialog event.
 */
static pjsip_evsub_user dlg_event_user =
{
    &dlg_event_on_evsub_state,
    &dlg_event_on_evsub_tsx_state,
    NULL,
    &dlg_event_on_evsub_rx_notify,
    &dlg_event_on_evsub_client_refresh,
    NULL,
};


/*
 * Some static constants.
 */
const pj_str_t STR_DIALOG_EVENT        = { "Event", 5 };
const pj_str_t STR_DIALOG              = { "dialog", 6 };
const pj_str_t STR_DIALOG_APPLICATION  = { "application", 11 };
const pj_str_t STR_DIALOG_INFO_XML     = { "dialog-info+xml", 15 };
const pj_str_t STR_APP_DIALOG_INFO_XML = { "application/dialog-info+xml", 27 };


/*
 * Init dialog event module.
 */
PJ_DEF(pj_status_t) pjsip_dlg_event_init_module(pjsip_endpoint *endpt,
                                                pjsip_module *mod_evsub)
{
    pj_status_t status;
    pj_str_t accept[1];

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt && mod_evsub, PJ_EINVAL);

    /* Must have not been registered */
    PJ_ASSERT_RETURN(mod_dlg_event.id == -1, PJ_EINVALIDOP);

    /* Register to endpoint */
    status = pjsip_endpt_register_module(endpt, &mod_dlg_event);
    if (status != PJ_SUCCESS)
        return status;

    accept[0] = STR_APP_DIALOG_INFO_XML;

    /* Register event package to event module. */
    status = pjsip_evsub_register_pkg(&mod_dlg_event, &STR_DIALOG,
                                      DLG_EVENT_DEFAULT_EXPIRES,
                                      PJ_ARRAY_SIZE(accept), accept);
    if (status != PJ_SUCCESS) {
        pjsip_endpt_unregister_module(endpt, &mod_dlg_event);
        return status;
    }

    return PJ_SUCCESS;
}


/*
 * Get dialog event module instance.
 */
PJ_DEF(pjsip_module*) pjsip_dlg_event_instance(void)
{
    return &mod_dlg_event;
}


/*
 * Create client subscription.
 */
PJ_DEF(pj_status_t)
pjsip_dlg_event_create_uac(pjsip_dialog *dlg,
                           const pjsip_evsub_user *user_cb,
                           unsigned options,
                           pjsip_evsub **p_evsub)
{
    pj_status_t status;
    pjsip_dlg_event *dlgev;
    char obj_name[PJ_MAX_OBJ_NAME];
    pjsip_evsub *sub;

    PJ_ASSERT_RETURN(dlg && p_evsub, PJ_EINVAL);

    pjsip_dlg_inc_lock(dlg);

    /* Create event subscription */
    status = pjsip_evsub_create_uac(dlg,  &dlg_event_user, &STR_DIALOG,
                                    options, &sub);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Create dialog event */
    dlgev = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_dlg_event);
    dlgev->dlg = dlg;
    dlgev->sub = sub;
    if (user_cb)
        pj_memcpy(&dlgev->user_cb, user_cb, sizeof(pjsip_evsub_user));

    status = pj_mutex_create_recursive(dlg->pool, "dlgev_mutex",
                                       &dlgev->mutex);
    if (status != PJ_SUCCESS)
        goto on_return;

    pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "dlgev%p", dlg->pool);
    dlgev->status_pool = pj_pool_create(dlg->pool->factory, obj_name,
                                        512, 512, NULL);
    pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "tmdlgev%p", dlg->pool);
    dlgev->tmp_pool = pj_pool_create(dlg->pool->factory, obj_name,
                                     512, 512, NULL);

    /* Attach to evsub */
    pjsip_evsub_set_mod_data(sub, mod_dlg_event.id, dlgev);

    *p_evsub = sub;

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Forcefully terminate dialog event.
 */
PJ_DEF(pj_status_t) pjsip_dlg_event_terminate(pjsip_evsub *sub,
                                              pj_bool_t notify)
{
    return pjsip_evsub_terminate(sub, notify);
}

/*
 * Create SUBSCRIBE
 */
PJ_DEF(pj_status_t) pjsip_dlg_event_initiate(pjsip_evsub *sub,
                                             pj_int32_t expires,
                                             pjsip_tx_data **p_tdata)
{
    return pjsip_evsub_initiate(sub, &pjsip_subscribe_method, expires,
                                p_tdata);
}


/*
 * Add custom headers.
 */
PJ_DEF(pj_status_t) pjsip_dlg_event_add_header(pjsip_evsub *sub,
                                               const pjsip_hdr *hdr_list )
{
    return pjsip_evsub_add_header( sub, hdr_list );
}


/*
 * Accept incoming subscription.
 */
PJ_DEF(pj_status_t) pjsip_dlg_event_accept(pjsip_evsub *sub,
                                           pjsip_rx_data *rdata,
                                           int st_code,
                                           const pjsip_hdr *hdr_list)
{
    return pjsip_evsub_accept( sub, rdata, st_code, hdr_list );
}


/*
 * Get dialog event status.
 */
PJ_DEF(pj_status_t) pjsip_dlg_event_get_status(pjsip_evsub *sub,
                                               pjsip_dlg_event_status *status)
{
    pjsip_dlg_event *dlgev;

    PJ_ASSERT_RETURN(sub && status, PJ_EINVAL);

    dlgev = (pjsip_dlg_event*) pjsip_evsub_get_mod_data(sub, mod_dlg_event.id);
    PJ_ASSERT_RETURN(dlgev!=NULL, PJSIP_SIMPLE_ENOPRESENCE);

    pj_mutex_lock(dlgev->mutex);

    if (dlgev->is_ts_valid) {
        PJ_ASSERT_ON_FAIL(dlgev->tmp_pool!=NULL,
                          {pj_mutex_unlock(dlgev->mutex);
                           return PJSIP_SIMPLE_ENOPRESENCE;});
        pj_memcpy(status, &dlgev->tmp_status, sizeof(pjsip_dlg_event_status));
    } else {
        PJ_ASSERT_ON_FAIL(dlgev->status_pool!=NULL,
                          {pj_mutex_unlock(dlgev->mutex);
                           return PJSIP_SIMPLE_ENOPRESENCE;});
        pj_memcpy(status, &dlgev->status, sizeof(pjsip_dlg_event_status));
    }

    pj_mutex_unlock(dlgev->mutex);

    return PJ_SUCCESS;
}


/*
 * Send request.
 */
PJ_DEF(pj_status_t) pjsip_dlg_event_send_request(pjsip_evsub *sub,
                                                 pjsip_tx_data *tdata )
{
    return pjsip_evsub_send_request(sub, tdata);
}



PJ_DEF(pj_status_t)
pjsip_dlg_event_parse_dialog_info(pjsip_rx_data *rdata,
                                  pj_pool_t *pool,
                                  pjsip_dlg_event_status *dlgev_st)
{
    return pjsip_dlg_event_parse_dialog_info2(
               (char*)rdata->msg_info.msg->body->data,
               rdata->msg_info.msg->body->len,
               pool, dlgev_st);
}

PJ_DEF(pj_status_t)
pjsip_dlg_event_parse_dialog_info2(char *body, unsigned body_len,
                                   pj_pool_t *pool,
                                   pjsip_dlg_event_status *dlgev_st)
{
    pjsip_dlg_info_dialog_info *dialog_info;
    pjsip_dlg_info_dialog *dialog;

    dialog_info = pjsip_dlg_info_parse(pool, body, body_len);
    if (dialog_info == NULL)
        return PJSIP_SIMPLE_EBADPIDF;

    dlgev_st->info_cnt = 0;

    dialog = pjsip_dlg_info_dialog_info_get_dialog(dialog_info);
    pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].dialog_info_entity,
                    pjsip_dlg_info_dialog_info_get_entity(dialog_info));
    pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].dialog_info_state,
                    pjsip_dlg_info_dialog_info_get_state(dialog_info));

    if (dialog) {
        pjsip_dlg_info_local * local;
        pjsip_dlg_info_remote * remote;

        dlgev_st->info[dlgev_st->info_cnt].dialog_node =
            pj_xml_clone(pool, dialog);

        pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].dialog_id,
                        pjsip_dlg_info_dialog_get_id(dialog));
        pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].dialog_call_id,
                        pjsip_dlg_info_dialog_get_call_id(dialog));
        pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].dialog_remote_tag,
                        pjsip_dlg_info_dialog_get_remote_tag(dialog));
        pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].dialog_local_tag,
                        pjsip_dlg_info_dialog_get_local_tag(dialog));
        pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].dialog_direction,
                        pjsip_dlg_info_dialog_get_direction(dialog));
        pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].dialog_state,
                        pjsip_dlg_info_dialog_get_state(dialog));
        pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].dialog_duration,
                        pjsip_dlg_info_dialog_get_duration(dialog));

        local =pjsip_dlg_info_dialog_get_local(dialog);
        if (local) {
            pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].local_identity,
                            pjsip_dlg_info_local_get_identity(local));
            pj_strdup(pool,
                &dlgev_st->info[dlgev_st->info_cnt].local_identity_display,
                pjsip_dlg_info_local_get_identity_display(local));
            pj_strdup(pool,
                &dlgev_st->info[dlgev_st->info_cnt].local_target_uri,
                pjsip_dlg_info_local_get_target_uri(local));
        } else {
            dlgev_st->info[dlgev_st->info_cnt].local_identity.ptr = NULL;
            dlgev_st->info[dlgev_st->info_cnt].local_identity_display.ptr =
                NULL;
            dlgev_st->info[dlgev_st->info_cnt].local_target_uri.ptr = NULL;
        }

        remote = pjsip_dlg_info_dialog_get_remote(dialog);
        if (remote) {
            pj_strdup(pool, &dlgev_st->info[dlgev_st->info_cnt].remote_identity,
                            pjsip_dlg_info_remote_get_identity(remote));
            pj_strdup(pool,
                &dlgev_st->info[dlgev_st->info_cnt].remote_identity_display,
                pjsip_dlg_info_remote_get_identity_display(remote));
            pj_strdup(pool,
                &dlgev_st->info[dlgev_st->info_cnt].remote_target_uri,
                pjsip_dlg_info_remote_get_target_uri(remote));
        } else {
            dlgev_st->info[dlgev_st->info_cnt].remote_identity.ptr = NULL;
            dlgev_st->info[dlgev_st->info_cnt].remote_identity_display.ptr =
                NULL;
            dlgev_st->info[dlgev_st->info_cnt].remote_target_uri.ptr = NULL;
        }
    } else {
        dlgev_st->info[dlgev_st->info_cnt].dialog_node = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * This callback is called by event subscription when subscription
 * state has changed.
 */
static void dlg_event_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
    pjsip_dlg_event *dlgev;

    dlgev = (pjsip_dlg_event*) pjsip_evsub_get_mod_data(sub, mod_dlg_event.id);
    PJ_ASSERT_ON_FAIL(dlgev!=NULL, {return;});

    if (dlgev->user_cb.on_evsub_state)
        (*dlgev->user_cb.on_evsub_state)(sub, event);

    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
        if (dlgev->status_pool) {
            pj_pool_release(dlgev->status_pool);
            dlgev->status_pool = NULL;
        }
        if (dlgev->tmp_pool) {
            pj_pool_release(dlgev->tmp_pool);
            dlgev->tmp_pool = NULL;
        }
        if (dlgev->mutex) {
            pj_mutex_destroy(dlgev->mutex);
            dlgev->mutex = NULL;
        }
    }
}

/*
 * Called when transaction state has changed.
 */
static void dlg_event_on_evsub_tsx_state(pjsip_evsub *sub,
                                         pjsip_transaction *tsx,
                                         pjsip_event *event)
{
    pjsip_dlg_event *dlgev;

    dlgev = (pjsip_dlg_event*) pjsip_evsub_get_mod_data(sub, mod_dlg_event.id);
    PJ_ASSERT_ON_FAIL(dlgev!=NULL, {return;});

    if (dlgev->user_cb.on_tsx_state)
        (*dlgev->user_cb.on_tsx_state)(sub, tsx, event);
}


/*
 * Process the content of incoming NOTIFY request and update temporary
 * status.
 *
 * return PJ_SUCCESS if incoming request is acceptable. If return value
 *        is not PJ_SUCCESS, res_hdr may be added with Warning header.
 */
static pj_status_t
dlg_event_process_rx_notify(pjsip_dlg_event *dlgev,
                            pjsip_rx_data *rdata,
                            int *p_st_code,
                            pj_str_t **p_st_text,
                            pjsip_hdr *res_hdr)
{
    pjsip_ctype_hdr *ctype_hdr;
    pj_status_t status = PJ_SUCCESS;

    *p_st_text = NULL;

    /* Check Content-Type and msg body are dlgevent. */
    ctype_hdr = rdata->msg_info.ctype;

    if (ctype_hdr==NULL || rdata->msg_info.msg->body==NULL)
    {
        pjsip_warning_hdr *warn_hdr;
        pj_str_t warn_text;

        *p_st_code = PJSIP_SC_BAD_REQUEST;

        warn_text = pj_str("Message body is not dlgevent");
        warn_hdr = pjsip_warning_hdr_create(rdata->tp_info.pool, 399,
                            pjsip_endpt_name(dlgev->dlg->endpt),
                            &warn_text);
        pj_list_push_back(res_hdr, warn_hdr);

        return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
    }

    /* Parse content. */
    if (pj_stricmp(&ctype_hdr->media.type, &STR_DIALOG_APPLICATION)==0 &&
        pj_stricmp(&ctype_hdr->media.subtype, &STR_DIALOG_INFO_XML)==0)
    {
        status = pjsip_dlg_event_parse_dialog_info(rdata, dlgev->tmp_pool,
                                                   &dlgev->tmp_status);
    }
    else {
        status = PJSIP_SIMPLE_EBADCONTENT;
    }

    /* If application calls dlgev_get_status(), redirect the call to
     * retrieve the temporary status.
     */
    dlgev->is_ts_valid = (status == PJ_SUCCESS? PJ_TRUE: PJ_FALSE);

    if (status != PJ_SUCCESS) {
    /* Unsupported or bad Content-Type */
        if (PJSIP_DLG_EVENT_BAD_CONTENT_RESPONSE >= 300) {
            pjsip_accept_hdr *accept_hdr;
            pjsip_warning_hdr *warn_hdr;

            *p_st_code = PJSIP_DLG_EVENT_BAD_CONTENT_RESPONSE;

            /* Add Accept header */
            accept_hdr = pjsip_accept_hdr_create(rdata->tp_info.pool);
            accept_hdr->values[accept_hdr->count++] = STR_APP_DIALOG_INFO_XML;
            pj_list_push_back(res_hdr, accept_hdr);

            /* Add Warning header */
            warn_hdr = pjsip_warning_hdr_create_from_status(
                        rdata->tp_info.pool,
                        pjsip_endpt_name(dlgev->dlg->endpt),
                        status);
            pj_list_push_back(res_hdr, warn_hdr);

            return status;
        } else {
            pj_assert(PJSIP_DLG_EVENT_BAD_CONTENT_RESPONSE/100 == 2);
            PJ_PERROR(4,(THIS_FILE, status,
                      "Ignoring dlgev error due to "
                      "PJSIP_DLG_EVENT_BAD_CONTENT_RESPONSE setting [%d]",
                      PJSIP_DLG_EVENT_BAD_CONTENT_RESPONSE));
            *p_st_code = PJSIP_DLG_EVENT_BAD_CONTENT_RESPONSE;
            status = PJ_SUCCESS;
        }
    }

    return PJ_SUCCESS;
}


/*
 * Called when NOTIFY is received.
 */
static void dlg_event_on_evsub_rx_notify(pjsip_evsub *sub,
                                         pjsip_rx_data *rdata,
                                         int *p_st_code,
                                         pj_str_t **p_st_text,
                                         pjsip_hdr *res_hdr,
                                         pjsip_msg_body **p_body)
{
    pjsip_dlg_event *dlgev;
    pj_status_t status;

    dlgev = (pjsip_dlg_event*) pjsip_evsub_get_mod_data(sub, mod_dlg_event.id);
    PJ_ASSERT_ON_FAIL(dlgev!=NULL, {return;});

    if (rdata->msg_info.msg->body) {
        status = dlg_event_process_rx_notify( dlgev, rdata, p_st_code, p_st_text,
                                              res_hdr );
        if (status != PJ_SUCCESS)
            return;

    } else {
        unsigned i;
        pj_mutex_lock(dlgev->mutex);
        for (i=0; i<dlgev->status.info_cnt; ++i) {
            dlgev->status.info[i].dialog_node = NULL;
        }
        pj_mutex_unlock(dlgev->mutex);
    }

    /* Notify application. */
    if (dlgev->user_cb.on_rx_notify) {
        (*dlgev->user_cb.on_rx_notify)(sub, rdata, p_st_code, p_st_text,
                                       res_hdr, p_body);
    }


    /* If application responded NOTIFY with 2xx, copy temporary status
     * to main status, and mark the temporary status as invalid.
     */
    pj_mutex_lock(dlgev->mutex);

    if ((*p_st_code)/100 == 2) {
        pj_pool_t *tmp;

        pj_memcpy(&dlgev->status, &dlgev->tmp_status,
                  sizeof(pjsip_dlg_event_status));

        /* Swap the pool */
        tmp = dlgev->tmp_pool;
        dlgev->tmp_pool = dlgev->status_pool;
        dlgev->status_pool = tmp;
    }

    dlgev->is_ts_valid = PJ_FALSE;
    pj_pool_reset(dlgev->tmp_pool);

    pj_mutex_unlock(dlgev->mutex);

    /* Done */
}

/*
 * Called when it's time to send SUBSCRIBE.
 */
static void dlg_event_on_evsub_client_refresh(pjsip_evsub *sub)
{
    pjsip_dlg_event *dlgev;

    dlgev = (pjsip_dlg_event*) pjsip_evsub_get_mod_data(sub, mod_dlg_event.id);
    PJ_ASSERT_ON_FAIL(dlgev!=NULL, {return;});

    if (dlgev->user_cb.on_client_refresh) {
        (*dlgev->user_cb.on_client_refresh)(sub);
    } else {
        pj_status_t status;
        pjsip_tx_data *tdata;
    
        status = pjsip_dlg_event_initiate(sub, -1, &tdata);
        if (status == PJ_SUCCESS)
            pjsip_dlg_event_send_request(sub, tdata);
    }
}
