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
#include <pjsip-simple/blf.h>
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


#define THIS_FILE           "blf.c"
#define BLF_DEFAULT_EXPIRES        PJSIP_BLF_DEFAULT_EXPIRES

#if PJSIP_BLF_BAD_CONTENT_RESPONSE < 200 || \
    PJSIP_BLF_BAD_CONTENT_RESPONSE > 699 || \
    PJSIP_BLF_BAD_CONTENT_RESPONSE/100 == 3
# error Invalid PJSIP_BLF_BAD_CONTENT_RESPONSE value
#endif

/*
 * "busy lamp field" (blf)  module (mod-blf)
 */
static struct pjsip_module mod_blf =
{
    NULL, NULL,             /* prev, next.          */
    { "mod-blf", 12 },     /* Name.                */
    -1,                 /* Id               */
    PJSIP_MOD_PRIORITY_DIALOG_USAGE,/* Priority             */
    NULL,               /* load()               */
    NULL,               /* start()              */
    NULL,               /* stop()               */
    NULL,               /* unload()             */
    NULL,               /* on_rx_request()          */
    NULL,               /* on_rx_response()         */
    NULL,               /* on_tx_request.           */
    NULL,               /* on_tx_response()         */
    NULL,               /* on_tsx_state()           */
};


/*
 * "busy lamp field" message body type.
 */
typedef enum content_type_e
{
    CONTENT_TYPE_NONE,
    CONTENT_TYPE_DIALOG_INFO,
} content_type_e;

/*
 * This structure describe a presentity, for both subscriber and notifier.
 */
struct pjsip_blf
{
    pjsip_evsub     *sub;           /**< Event subscription record.         */
    pjsip_dialog    *dlg;           /**< The dialog.                        */
    content_type_e   content_type;  /**< Content-Type.                      */
    pj_pool_t       *status_pool;   /**< Pool for pres_status               */
    pjsip_blf_status    status;     /**< "busy lamp field" status.          */
    pj_pool_t       *tmp_pool;      /**< Pool for tmp_status                */
    pjsip_blf_status    tmp_status; /**< Temp, before NOTIFY is answered.   */
    pjsip_evsub_user     user_cb;   /**< The user callback.                 */
};


typedef struct pjsip_blf pjsip_blf;


/*
 * Forward decl for evsub callback.
 */
static void blf_on_evsub_state(pjsip_evsub *sub, pjsip_event *event);
static void blf_on_evsub_tsx_state(pjsip_evsub *sub, pjsip_transaction *tsx,
                                   pjsip_event *event);
static void blf_on_evsub_rx_notify(pjsip_evsub *sub,
                                   pjsip_rx_data *rdata,
                                   int *p_st_code,
                                   pj_str_t **p_st_text,
                                   pjsip_hdr *res_hdr,
                                   pjsip_msg_body **p_body);
static void blf_on_evsub_client_refresh(pjsip_evsub *sub);


/*
 * Event subscription callback for "busy lamp field".
 */
static pjsip_evsub_blf_user blf_user =
{
    &blf_on_evsub_state,
    &blf_on_evsub_tsx_state,
    NULL,
    &blf_on_evsub_rx_notify,
    &blf_on_evsub_client_refresh,
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
 * Init presence module.
 */
PJ_DEF(pj_status_t) pjsip_blf_init_module(pjsip_endpoint *endpt,
                                          pjsip_module *mod_evsub)
{
    pj_status_t status;
    pj_str_t accept[1];

    /* Check arguments. */
    PJ_ASSERT_RETURN(endpt && mod_evsub, PJ_EINVAL);

    /* Must have not been registered */
    PJ_ASSERT_RETURN(mod_blf.id == -1, PJ_EINVALIDOP);

    /* Register to endpoint */
    status = pjsip_endpt_register_module(endpt, &mod_blf);
    if (status != PJ_SUCCESS)
    return status;

    accept[0] = STR_APP_DIALOG_INFO_XML;

    /* Register event package to event module. */
    status = pjsip_evsub_register_pkg( &mod_blf, &STR_DIALOG,
                       BLF_DEFAULT_EXPIRES,
                       PJ_ARRAY_SIZE(accept), accept);
    if (status != PJ_SUCCESS) {
    pjsip_endpt_unregister_module(endpt, &mod_blf);
    return status;
    }

    return PJ_SUCCESS;
}


/*
 * Get presence module instance.
 */
PJ_DEF(pjsip_module*) pjsip_blf_instance(void)
{
    return &mod_blf;
}


/*
 * Create client subscription.
 */
PJ_DEF(pj_status_t) pjsip_blf_create_uac(pjsip_dialog *dlg,
                                         const pjsip_evsub_blf_user *user_cb,
                                         unsigned options,
                                         pjsip_evsub **p_evsub)
{
    pj_status_t status;
    pjsip_blf *pres;
    char obj_name[PJ_MAX_OBJ_NAME];
    pjsip_evsub *sub;

    PJ_ASSERT_RETURN(dlg && p_evsub, PJ_EINVAL);

    pjsip_dlg_inc_lock(dlg);

    /* Create event subscription */
    status = pjsip_evsub_create_uac( dlg,  &blf_user, &STR_DIALOG,
                     options, &sub);
    if (status != PJ_SUCCESS)
    goto on_return;

    /* Create presence */
    pres = PJ_POOL_ZALLOC_T(dlg->pool, pjsip_blf);
    pres->dlg = dlg;
    pres->sub = sub;
    if (user_cb)
        pj_memcpy(&pres->user_cb, user_cb, sizeof(pjsip_evsub_user));

    pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "pres%p", dlg->pool);
    pres->status_pool = pj_pool_create(dlg->pool->factory, obj_name,
                       512, 512, NULL);
    pj_ansi_snprintf(obj_name, PJ_MAX_OBJ_NAME, "tmpres%p", dlg->pool);
    pres->tmp_pool = pj_pool_create(dlg->pool->factory, obj_name,
                    512, 512, NULL);

    /* Attach to evsub */
    pjsip_evsub_set_mod_data(sub, mod_blf.id, pres);

    *p_evsub = sub;

on_return:
    pjsip_dlg_dec_lock(dlg);
    return status;
}


/*
 * Forcefully terminate presence.
 */
PJ_DEF(pj_status_t) pjsip_blf_terminate(pjsip_evsub *sub,
                                        pj_bool_t notify)
{
    return pjsip_evsub_terminate(sub, notify);
}

/*
 * Create SUBSCRIBE
 */
PJ_DEF(pj_status_t) pjsip_blf_initiate(pjsip_evsub *sub,
                                       pj_int32_t expires,
                                       pjsip_tx_data **p_tdata)
{
    return pjsip_evsub_initiate(sub, &pjsip_subscribe_method, expires,
                                p_tdata);
}


/*
 * Add custom headers.
 */
PJ_DEF(pj_status_t) pjsip_blf_add_header(pjsip_evsub *sub,
                                         const pjsip_hdr *hdr_list )
{
    return pjsip_evsub_add_header( sub, hdr_list );
}


/*
 * Accept incoming subscription.
 */
PJ_DEF(pj_status_t) pjsip_blf_accept(pjsip_evsub *sub,
                                     pjsip_rx_data *rdata,
                                     int st_code,
                                     const pjsip_hdr *hdr_list)
{
    return pjsip_evsub_accept( sub, rdata, st_code, hdr_list );
}


/*
 * Get "busy lamp field" status.
 */
PJ_DEF(pj_status_t) pjsip_blf_get_status(pjsip_evsub *sub,
                                         pjsip_blf_status *status )
{
    pjsip_blf *blf;

    PJ_ASSERT_RETURN(sub && status, PJ_EINVAL);

    blf = (pjsip_blf*) pjsip_evsub_get_mod_data(sub, mod_blf.id);
    PJ_ASSERT_RETURN(blf!=NULL, PJSIP_SIMPLE_ENOPRESENCE);

    if (blf->tmp_status._is_valid) {
        PJ_ASSERT_RETURN(blf->tmp_pool!=NULL, PJSIP_SIMPLE_ENOPRESENCE);
        pj_memcpy(status, &blf->tmp_status, sizeof(pjsip_blf_status));
    } else {
        PJ_ASSERT_RETURN(blf->status_pool!=NULL, PJSIP_SIMPLE_ENOPRESENCE);
        pj_memcpy(status, &blf->status, sizeof(pjsip_blf_status));
    }

    return PJ_SUCCESS;
}


/*
 * Send request.
 */
PJ_DEF(pj_status_t) pjsip_blf_send_request(pjsip_evsub *sub,
                                           pjsip_tx_data *tdata )
{
    return pjsip_evsub_send_request(sub, tdata);
}


/*
 * This callback is called by event subscription when subscription
 * state has changed.
 */
static void blf_on_evsub_state( pjsip_evsub *sub, pjsip_event *event)
{
    pjsip_blf *pres;

    pres = (pjsip_blf*) pjsip_evsub_get_mod_data(sub, mod_blf.id);
    PJ_ASSERT_ON_FAIL(pres!=NULL, {return;});

    if (pres->user_cb.on_evsub_state)
        (*pres->user_cb.on_evsub_state)(sub, event);

    if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED) {
        if (pres->status_pool) {
            pj_pool_release(pres->status_pool);
            pres->status_pool = NULL;
        }
        if (pres->tmp_pool) {
            pj_pool_release(pres->tmp_pool);
            pres->tmp_pool = NULL;
        }
    }
}

/*
 * Called when transaction state has changed.
 */
static void blf_on_evsub_tsx_state(pjsip_evsub *sub,
                                   pjsip_transaction *tsx,
                                   pjsip_event *event)
{
    pjsip_blf *pres;

    pres = (pjsip_blf*) pjsip_evsub_get_mod_data(sub, mod_blf.id);
    PJ_ASSERT_ON_FAIL(pres!=NULL, {return;});

    if (pres->user_cb.on_tsx_state)
        (*pres->user_cb.on_tsx_state)(sub, tsx, event);
}


/*
 * Process the content of incoming NOTIFY request and update temporary
 * status.
 *
 * return PJ_SUCCESS if incoming request is acceptable. If return value
 *    is not PJ_SUCCESS, res_hdr may be added with Warning header.
 */
static pj_status_t blf_process_rx_notify(pjsip_blf *blf,
                                         pjsip_rx_data *rdata,
                                         int *p_st_code,
                                         pj_str_t **p_st_text,
                                         pjsip_hdr *res_hdr)
{
    pjsip_ctype_hdr *ctype_hdr;
    pj_status_t status = PJ_SUCCESS;

    *p_st_text = NULL;

    /* Check Content-Type and msg body are present. */
    ctype_hdr = rdata->msg_info.ctype;

    if (ctype_hdr==NULL || rdata->msg_info.msg->body==NULL)
    {
        pjsip_warning_hdr *warn_hdr;
        pj_str_t warn_text;

        *p_st_code = PJSIP_SC_BAD_REQUEST;

        warn_text = pj_str("Message body is not present");
        warn_hdr = pjsip_warning_hdr_create(rdata->tp_info.pool, 399,
                            pjsip_endpt_name(blf->dlg->endpt),
                            &warn_text);
        pj_list_push_back(res_hdr, warn_hdr);

        return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_BAD_REQUEST);
    }

    /* Parse content. */
    if (pj_stricmp(&ctype_hdr->media.type, &STR_DIALOG_APPLICATION)==0 &&
        pj_stricmp(&ctype_hdr->media.subtype, &STR_DIALOG_INFO_XML)==0)
    {
        status = pjsip_blf_parse_dialog_info( rdata, blf->tmp_pool,
                &blf->tmp_status);
    }
    else
    {
        status = PJSIP_SIMPLE_EBADCONTENT;
    }

    if (status != PJ_SUCCESS) {
    /* Unsupported or bad Content-Type */
        if (PJSIP_BLF_BAD_CONTENT_RESPONSE >= 300) {
            pjsip_accept_hdr *accept_hdr;
            pjsip_warning_hdr *warn_hdr;

            *p_st_code = PJSIP_BLF_BAD_CONTENT_RESPONSE;

            /* Add Accept header */
            accept_hdr = pjsip_accept_hdr_create(rdata->tp_info.pool);
            accept_hdr->values[accept_hdr->count++] = STR_APP_DIALOG_INFO_XML;
            pj_list_push_back(res_hdr, accept_hdr);

            /* Add Warning header */
            warn_hdr = pjsip_warning_hdr_create_from_status(
                        rdata->tp_info.pool,
                        pjsip_endpt_name(blf->dlg->endpt),
                        status);
            pj_list_push_back(res_hdr, warn_hdr);

            return status;
        } else {
            pj_assert(PJSIP_BLF_BAD_CONTENT_RESPONSE/100 == 2);
            PJ_PERROR(4,(THIS_FILE, status,
                 "Ignoring blf error due to "
                     "PJSIP_BLF_BAD_CONTENT_RESPONSE setting [%d]",
                     PJSIP_BLF_BAD_CONTENT_RESPONSE));
            *p_st_code = PJSIP_BLF_BAD_CONTENT_RESPONSE;
            status = PJ_SUCCESS;
        }
    }

    /* If application calls pres_get_status(), redirect the call to
     * retrieve the temporary status.
     */
    blf->tmp_status._is_valid = PJ_TRUE;

    return PJ_SUCCESS;
}


/*
 * Called when NOTIFY is received.
 */
static void blf_on_evsub_rx_notify(pjsip_evsub *sub,
                                   pjsip_rx_data *rdata,
                                   int *p_st_code,
                                   pj_str_t **p_st_text,
                                   pjsip_hdr *res_hdr,
                                   pjsip_msg_body **p_body)
{
    pjsip_blf *blf;
    pj_status_t status;

    blf = (pjsip_blf*) pjsip_evsub_get_mod_data(sub, mod_blf.id);
    PJ_ASSERT_ON_FAIL(blf!=NULL, {return;});

    if (rdata->msg_info.msg->body) {
        status = blf_process_rx_notify( blf, rdata, p_st_code, p_st_text,
                     res_hdr );
    if (status != PJ_SUCCESS)
        return;

    } else {
        unsigned i;
        for (i=0; i<blf->status.info_cnt; ++i) {
            blf->status.info[i].dialog_node = NULL;
        }

    }

    /* Notify application. */
    if (blf->user_cb.on_rx_notify) {
        (*blf->user_cb.on_rx_notify)(sub, rdata, p_st_code, p_st_text,
                      res_hdr, p_body);
    }


    /* If application responded NOTIFY with 2xx, copy temporary status
     * to main status, and mark the temporary status as invalid.
     */
    if ((*p_st_code)/100 == 2) {
        pj_pool_t *tmp;

        pj_memcpy(&blf->status, &blf->tmp_status, sizeof(pjsip_blf_status));

        /* Swap the pool */
        tmp = blf->tmp_pool;
        blf->tmp_pool = blf->status_pool;
        blf->status_pool = tmp;
    }

    blf->tmp_status._is_valid = PJ_FALSE;
    pj_pool_reset(blf->tmp_pool);

    /* Done */
}

/*
 * Called when it's time to send SUBSCRIBE.
 */
static void blf_on_evsub_client_refresh(pjsip_evsub *sub)
{
    pjsip_blf *pres;

    pres = (pjsip_blf*) pjsip_evsub_get_mod_data(sub, mod_blf.id);
    PJ_ASSERT_ON_FAIL(pres!=NULL, {return;});

    if (pres->user_cb.on_client_refresh) {
    (*pres->user_cb.on_client_refresh)(sub);
    } else {
    pj_status_t status;
    pjsip_tx_data *tdata;

    status = pjsip_blf_initiate(sub, -1, &tdata);
    if (status == PJ_SUCCESS)
        pjsip_blf_send_request(sub, tdata);
    }
}


