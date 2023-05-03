/* 
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjsip-ua/sip_timer.h>
#include <pjsip/print_util.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_event.h>
#include <pjsip/sip_transaction.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/os.h>
#include <pj/pool.h>

#define THIS_FILE               "sip_timer.c"


/* Constant of Session Timers */
#define ABS_MIN_SE                  90  /* Absolute Min-SE, in seconds      */
#define REFRESHER_EXPIRE_TIMER_ID   2   /* Refresher expire timer id        */ 

/* String definitions */
static const pj_str_t STR_SE            = {"Session-Expires", 15};
static const pj_str_t STR_SHORT_SE      = {"x", 1};
static const pj_str_t STR_MIN_SE        = {"Min-SE", 6};
static const pj_str_t STR_REFRESHER     = {"refresher", 9};
static const pj_str_t STR_UAC           = {"uac", 3};
static const pj_str_t STR_UAS           = {"uas", 3};
static const pj_str_t STR_TIMER         = {"timer", 5};


/* Enumeration of refresher */
enum timer_refresher {
    TR_UNKNOWN,
    TR_UAC,
    TR_UAS
};

/* Structure definition of Session Timers */
struct pjsip_timer 
{
    pj_bool_t                    active;        /**< Active/inactive flag   */
    pjsip_timer_setting          setting;       /**< Session Timers setting */
    enum timer_refresher         refresher;     /**< Session refresher      */
    pj_time_val                  last_refresh;  /**< Timestamp of last
                                                     refresh                */
    pj_timer_entry               timer;         /**< Timer entry            */
    pj_bool_t                    use_update;    /**< Use UPDATE method to
                                                     refresh the session    */
    pj_bool_t                    with_sdp;      /**< SDP in UPDATE?         */
    pjsip_role_e                 role;          /**< Role in last INVITE/
                                                     UPDATE transaction.    */    
    void                        *refresh_tdata; /**< The tdata of refresh 
                                                     request                */
    pj_timer_entry               expire_timer;  /**< Timer entry for expire 
                                                     refresher              */
    pj_int32_t                   last_422_cseq; /**< Last 422 resp CSeq.    */
};

/* Local functions & vars */
static void stop_timer(pjsip_inv_session *inv);
static void start_timer(pjsip_inv_session *inv);
static pj_bool_t is_initialized;
const pjsip_method pjsip_update_method = { PJSIP_OTHER_METHOD, {"UPDATE", 6}};
/*
 * Session-Expires header vptr.
 */
static int se_hdr_print(pjsip_sess_expires_hdr *hdr, 
                        char *buf, pj_size_t size);
static pjsip_sess_expires_hdr* se_hdr_clone(pj_pool_t *pool, 
                                            const pjsip_sess_expires_hdr *hdr);
static pjsip_sess_expires_hdr* se_hdr_shallow_clone( 
                                            pj_pool_t *pool,
                                            const pjsip_sess_expires_hdr* hdr);

static pjsip_hdr_vptr se_hdr_vptr = 
{
    (pjsip_hdr_clone_fptr) &se_hdr_clone,
    (pjsip_hdr_clone_fptr) &se_hdr_shallow_clone,
    (pjsip_hdr_print_fptr) &se_hdr_print,
};

/*
 * Min-SE header vptr.
 */
static int min_se_hdr_print(pjsip_min_se_hdr *hdr, 
                            char *buf, pj_size_t size);
static pjsip_min_se_hdr* min_se_hdr_clone(pj_pool_t *pool, 
                                          const pjsip_min_se_hdr *hdr);
static pjsip_min_se_hdr* min_se_hdr_shallow_clone( 
                                          pj_pool_t *pool,
                                          const pjsip_min_se_hdr* hdr);

static pjsip_hdr_vptr min_se_hdr_vptr = 
{
    (pjsip_hdr_clone_fptr) &min_se_hdr_clone,
    (pjsip_hdr_clone_fptr) &min_se_hdr_shallow_clone,
    (pjsip_hdr_print_fptr) &min_se_hdr_print,
};

/*
 * Session-Expires header vptr.
 */
static int se_hdr_print(pjsip_sess_expires_hdr *hdr, 
                        char *buf, pj_size_t size)
{
    char *p = buf;
    char *endbuf = buf+size;
    pj_ssize_t printed;
    const pjsip_parser_const_t *pc = pjsip_parser_const();
    const pj_str_t *hname = pjsip_cfg()->endpt.use_compact_form? 
                            &hdr->sname : &hdr->name;

    /* Print header name and value */
    if ((endbuf - p) < (hname->slen + 16))
        return -1;

    copy_advance(p, (*hname));
    *p++ = ':';
    *p++ = ' ';

    printed = pj_utoa(hdr->sess_expires, p);
    p += printed;

    /* Print 'refresher' param */
    if (hdr->refresher.slen)
    {
        if  ((endbuf - p) < (STR_REFRESHER.slen + 2 + hdr->refresher.slen))
            return -1;

        *p++ = ';';
        copy_advance(p, STR_REFRESHER);
        *p++ = '=';
        copy_advance(p, hdr->refresher);
    }

    /* Print generic params */
    printed = pjsip_param_print_on(&hdr->other_param, p, endbuf-p,
                                   &pc->pjsip_TOKEN_SPEC, 
                                   &pc->pjsip_TOKEN_SPEC, ';');
    if (printed < 0)
        return (int)printed;

    p += printed;
    return (int)(p - buf);
}

static pjsip_sess_expires_hdr* se_hdr_clone(pj_pool_t *pool, 
                                            const pjsip_sess_expires_hdr *hsrc)
{
    pjsip_sess_expires_hdr *hdr = pjsip_sess_expires_hdr_create(pool);
    hdr->sess_expires = hsrc->sess_expires;
    pj_strdup(pool, &hdr->refresher, &hsrc->refresher);
    pjsip_param_clone(pool, &hdr->other_param, &hsrc->other_param);
    return hdr;
}

static pjsip_sess_expires_hdr* se_hdr_shallow_clone( 
                                            pj_pool_t *pool,
                                            const pjsip_sess_expires_hdr* hsrc)
{
    pjsip_sess_expires_hdr *hdr = PJ_POOL_ALLOC_T(pool,pjsip_sess_expires_hdr);
    pj_memcpy(hdr, hsrc, sizeof(*hdr));
    pjsip_param_shallow_clone(pool, &hdr->other_param, &hsrc->other_param);
    return hdr;
}

/*
 * Min-SE header vptr.
 */
static int min_se_hdr_print(pjsip_min_se_hdr *hdr, 
                            char *buf, pj_size_t size)
{
    char *p = buf;
    char *endbuf = buf+size;
    pj_ssize_t printed;
    const pjsip_parser_const_t *pc = pjsip_parser_const();

    /* Print header name and value */
    if ((endbuf - p) < (hdr->name.slen + 16))
        return -1;

    copy_advance(p, hdr->name);
    *p++ = ':';
    *p++ = ' ';

    printed = pj_utoa(hdr->min_se, p);
    p += printed;

    /* Print generic params */
    printed = pjsip_param_print_on(&hdr->other_param, p, endbuf-p,
                                   &pc->pjsip_TOKEN_SPEC, 
                                   &pc->pjsip_TOKEN_SPEC, ';');
    if (printed < 0)
        return (int)printed;

    p += printed;
    return (int)(p - buf);
}

static pjsip_min_se_hdr* min_se_hdr_clone(pj_pool_t *pool, 
                                          const pjsip_min_se_hdr *hsrc)
{
    pjsip_min_se_hdr *hdr = pjsip_min_se_hdr_create(pool);
    hdr->min_se = hsrc->min_se;
    pjsip_param_clone(pool, &hdr->other_param, &hsrc->other_param);
    return hdr;
}

static pjsip_min_se_hdr* min_se_hdr_shallow_clone( 
                                          pj_pool_t *pool,
                                          const pjsip_min_se_hdr* hsrc)
{
    pjsip_min_se_hdr *hdr = PJ_POOL_ALLOC_T(pool, pjsip_min_se_hdr);
    pj_memcpy(hdr, hsrc, sizeof(*hdr));
    pjsip_param_shallow_clone(pool, &hdr->other_param, &hsrc->other_param);
    return hdr;
}


/*
 * Parse Session-Expires header.
 */
static pjsip_hdr *parse_hdr_se(pjsip_parse_ctx *ctx)
{
    pjsip_sess_expires_hdr *hdr = pjsip_sess_expires_hdr_create(ctx->pool);
    const pjsip_parser_const_t *pc = pjsip_parser_const();
    pj_str_t token;

    pj_scan_get(ctx->scanner, &pc->pjsip_DIGIT_SPEC, &token);
    hdr->sess_expires = pj_strtoul(&token);

    while (*ctx->scanner->curptr == ';') {
        pj_str_t pname, pvalue;

        pj_scan_get_char(ctx->scanner);
        pjsip_parse_param_imp(ctx->scanner, ctx->pool, &pname, &pvalue, 0);

        if (pj_stricmp(&pname, &STR_REFRESHER)==0) {
            hdr->refresher = pvalue;
        } else {
            pjsip_param *param = PJ_POOL_ALLOC_T(ctx->pool, pjsip_param);
            param->name = pname;
            param->value = pvalue;
            pj_list_push_back(&hdr->other_param, param);
        }
    }
    pjsip_parse_end_hdr_imp( ctx->scanner );
    return (pjsip_hdr*)hdr;
}

/*
 * Parse Min-SE header.
 */
static pjsip_hdr *parse_hdr_min_se(pjsip_parse_ctx *ctx)
{
    pjsip_min_se_hdr *hdr = pjsip_min_se_hdr_create(ctx->pool);
    const pjsip_parser_const_t *pc = pjsip_parser_const();
    pj_str_t token;

    pj_scan_get(ctx->scanner, &pc->pjsip_DIGIT_SPEC, &token);
    hdr->min_se = pj_strtoul(&token);

    while (*ctx->scanner->curptr == ';') {
        pj_str_t pname, pvalue;
        pjsip_param *param = PJ_POOL_ALLOC_T(ctx->pool, pjsip_param);

        pj_scan_get_char(ctx->scanner);
        pjsip_parse_param_imp(ctx->scanner, ctx->pool, &pname, &pvalue, 0);

        param->name = pname;
        param->value = pvalue;
        pj_list_push_back(&hdr->other_param, param);
    }
    pjsip_parse_end_hdr_imp( ctx->scanner );
    return (pjsip_hdr*)hdr;
}


/* Add "Session-Expires" and "Min-SE" headers. Note that "Min-SE" header
 * can only be added to INVITE/UPDATE request and 422 response.
 */
static void add_timer_headers(pjsip_inv_session *inv, pjsip_tx_data *tdata,
                              pj_bool_t add_se, pj_bool_t add_min_se)
{
    pjsip_timer *timer = inv->timer;

    /* Add Session-Expires header */
    if (add_se) {
        pjsip_sess_expires_hdr *hdr;

        hdr = pjsip_sess_expires_hdr_create(tdata->pool);
        hdr->sess_expires = timer->setting.sess_expires;
        if (timer->refresher != TR_UNKNOWN)
            hdr->refresher = (timer->refresher == TR_UAC? STR_UAC : STR_UAS);

        pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*) hdr);
    }

    /* Add Min-SE header */
    if (add_min_se) {
        pjsip_min_se_hdr *hdr;

        hdr = pjsip_min_se_hdr_create(tdata->pool);
        hdr->min_se = timer->setting.min_se;

        pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*) hdr);
    }
}

/* Timer callback. When the timer is fired, it can be time to refresh
 * the session if UA is the refresher, otherwise it is time to end 
 * the session.
 */
static void timer_cb(pj_timer_heap_t *timer_heap, struct pj_timer_entry *entry)
{
    pjsip_inv_session *inv = (pjsip_inv_session*) entry->user_data;
    pjsip_tx_data *tdata = NULL;
    pj_status_t status;
    pj_bool_t as_refresher;
    int entry_id;
    char obj_name[PJ_MAX_OBJ_NAME];

    pj_assert(inv);

    PJ_UNUSED_ARG(timer_heap);

    /* Lock dialog. */
    pjsip_dlg_inc_lock(inv->dlg);

    /* Check our role */
    as_refresher =
        (inv->timer->refresher == TR_UAC && inv->timer->role == PJSIP_ROLE_UAC) ||
        (inv->timer->refresher == TR_UAS && inv->timer->role == PJSIP_ROLE_UAS);

    entry_id = entry->id;
    pj_ansi_strxcpy(obj_name, inv->pool->obj_name, PJ_MAX_OBJ_NAME);

    /* Do action based on role(refresher or refreshee). 
     * As refresher:
     * - send refresh, or  
     * - end session if there is no response to the refresh request.
     * As refreshee:
     * - end session if there is no refresh request received.
     */
    if (as_refresher && (entry_id != REFRESHER_EXPIRE_TIMER_ID)) {
        pj_time_val now;

        /* As refresher, reshedule the refresh request on the following:
         *  - must not send re-INVITE if another INVITE or SDP negotiation
         *    is in progress.
         *  - must not send UPDATE with SDP if SDP negotiation is in progress
         */
        pjmedia_sdp_neg_state neg_state = pjmedia_sdp_neg_get_state(inv->neg);

        inv->timer->timer.id = 0;

        if ( (!inv->timer->use_update && (
                        inv->invite_tsx != NULL ||
                        neg_state != PJMEDIA_SDP_NEG_STATE_DONE)
             )
             ||
             (inv->timer->use_update && inv->timer->with_sdp &&
                     neg_state != PJMEDIA_SDP_NEG_STATE_DONE
             )
           )
        {
            pj_time_val delay = {1, 0};

            inv->timer->timer.id = 1;
            pjsip_endpt_schedule_timer(inv->dlg->endpt, &inv->timer->timer,
                                       &delay);
            pjsip_dlg_dec_lock(inv->dlg);

            PJ_LOG(3, (obj_name,
                       "Reschedule refresh request after %ld seconds as "
                       "there is another SDP negotiation in progress",
                       delay.sec));
            return;
        }

        /* Refresher, refresh the session */
        if (inv->timer->use_update) {
            const pjmedia_sdp_session *offer = NULL;

            if (inv->timer->with_sdp) {
                pjmedia_sdp_neg_get_active_local(inv->neg, &offer);
            }
            status = pjsip_inv_update(inv, NULL, offer, &tdata);
        } else {
            /* Create re-INVITE without modifying session */
            pjsip_msg_body *body;
            const pjmedia_sdp_session *offer = NULL;

            pj_assert(pjmedia_sdp_neg_get_state(inv->neg) == 
                      PJMEDIA_SDP_NEG_STATE_DONE);

            status = pjsip_inv_invite(inv, &tdata);
            if (status == PJ_SUCCESS)
                status = pjmedia_sdp_neg_send_local_offer(inv->pool_prov, 
                                                          inv->neg, &offer);
            if (status == PJ_SUCCESS)
                status = pjmedia_sdp_neg_get_neg_local(inv->neg, &offer);
            if (status == PJ_SUCCESS) {
                status = pjsip_create_sdp_body(tdata->pool, 
                                        (pjmedia_sdp_session*)offer, &body);
                tdata->msg->body = body;
            }
        }

        pj_gettimeofday(&now);
        PJ_LOG(4, (obj_name,
                   "Refreshing session after %lds (expiration period=%ds)",
                   (now.sec-inv->timer->last_refresh.sec),
                   inv->timer->setting.sess_expires));
    } else {
        
        pj_time_val now;
        pj_str_t reason = pj_str("No session refresh received.");

        if (as_refresher)
            inv->timer->expire_timer.id = 0;
        else
            inv->timer->timer.id = 0;

        /* Terminate the session */
        status = pjsip_inv_end_session(inv, PJSIP_SC_REQUEST_TIMEOUT, 
                                       &reason, &tdata);

        pj_gettimeofday(&now);
        PJ_LOG(3, (obj_name,
                   "No session %s received after %lds "
                   "(expiration period=%ds), stopping session now!",
                   (as_refresher?"refresh response":"refresh"),
                   (now.sec-inv->timer->last_refresh.sec),
                   inv->timer->setting.sess_expires));

    }

    /* Unlock dialog. */
    pjsip_dlg_dec_lock(inv->dlg);

    /* Send message, if any */
    if (tdata && status == PJ_SUCCESS) {
        inv->timer->refresh_tdata = tdata;

        status = pjsip_inv_send_msg(inv, tdata);        
    }

    /*
     * At this point, dialog might have already been destroyed,
     * including its pool used by the invite session.
     */

    /* Print error message, if any */
    if (status != PJ_SUCCESS) {
        PJ_PERROR(2, (obj_name, status,
                     "Error in %s session timer",
                     ((as_refresher && entry_id != REFRESHER_EXPIRE_TIMER_ID)?
                       "refreshing" : "terminating")));
    }
}

/* Start Session Timers */
static void start_timer(pjsip_inv_session *inv)
{
    const pj_str_t UPDATE = { "UPDATE", 6 };
    pjsip_timer *timer = inv->timer;
    pj_time_val delay = {0};

    pj_assert(inv->timer->active == PJ_TRUE);

    stop_timer(inv);

    inv->timer->use_update =
            (pjsip_dlg_remote_has_cap(inv->dlg, PJSIP_H_ALLOW, NULL,
                                      &UPDATE) == PJSIP_DIALOG_CAP_SUPPORTED);
    if (!inv->timer->use_update) {
        /* INVITE always needs SDP */
        inv->timer->with_sdp = PJ_TRUE;
    }

    pj_timer_entry_init(&timer->timer,
                        1,                  /* id */
                        inv,                /* user data */
                        timer_cb);          /* callback */
    
    /* Set delay based on role, refresher or refreshee */
    if ((timer->refresher == TR_UAC && inv->timer->role == PJSIP_ROLE_UAC) ||
        (timer->refresher == TR_UAS && inv->timer->role == PJSIP_ROLE_UAS))
    {
        /* Add refresher expire timer */
        pj_timer_entry_init(&timer->expire_timer,
                            REFRESHER_EXPIRE_TIMER_ID,      /* id */
                            inv,                            /* user data */
                            timer_cb);                      /* callback */

        delay.sec = timer->setting.sess_expires;
        /* Schedule the timer */
        pjsip_endpt_schedule_timer(inv->dlg->endpt, &timer->expire_timer, 
                                   &delay);

        /* Next refresh, the delay is half of session expire */
        delay.sec = timer->setting.sess_expires / 2;
    } else {
        /* Send BYE if no refresh received until this timer fired, delay
         * is the minimum of 32 seconds and one third of the session interval
         * before session expiration.
         */
        delay.sec = timer->setting.sess_expires - 
                    timer->setting.sess_expires/3;
        delay.sec = PJ_MAX((long)timer->setting.sess_expires-32, delay.sec);
    }

    /* Schedule the timer */
    pjsip_endpt_schedule_timer(inv->dlg->endpt, &timer->timer, &delay);

    /* Update last refresh time */
    pj_gettimeofday(&timer->last_refresh);
}

/* Stop Session Timers */
static void stop_timer(pjsip_inv_session *inv)
{
    if (inv->timer->timer.id != 0) {
        pjsip_endpt_cancel_timer(inv->dlg->endpt, &inv->timer->timer);
        inv->timer->timer.id = 0;       
    }

    if (inv->timer->expire_timer.id != 0) {
        pjsip_endpt_cancel_timer(inv->dlg->endpt, &inv->timer->expire_timer);
        inv->timer->expire_timer.id = 0;
    }
}

/* Deinitialize Session Timers */
static void pjsip_timer_deinit_module(pjsip_endpoint *endpt)
{
    PJ_TODO(provide_initialized_flag_for_each_endpoint);
    PJ_UNUSED_ARG(endpt);
    is_initialized = PJ_FALSE;
}

/*
 * Initialize Session Timers support in PJSIP. 
 */
PJ_DEF(pj_status_t) pjsip_timer_init_module(pjsip_endpoint *endpt)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

    if (is_initialized)
        return PJ_SUCCESS;

    /* Register Session-Expires header parser */
    status = pjsip_register_hdr_parser( STR_SE.ptr, STR_SHORT_SE.ptr, 
                                        &parse_hdr_se);
    if (status != PJ_SUCCESS)
        return status;

    /* Register Min-SE header parser */
    status = pjsip_register_hdr_parser( STR_MIN_SE.ptr, NULL, 
                                        &parse_hdr_min_se);
    if (status != PJ_SUCCESS)
        return status;

    /* Register 'timer' capability to endpoint */
    status = pjsip_endpt_add_capability(endpt, NULL, PJSIP_H_SUPPORTED,
                                        NULL, 1, &STR_TIMER);
    if (status != PJ_SUCCESS)
        return status;

    /* Register deinit module to be executed when PJLIB shutdown */
    if (pjsip_endpt_atexit(endpt, &pjsip_timer_deinit_module) != PJ_SUCCESS) {
        /* Failure to register this function may cause this module won't 
         * work properly when the stack is restarted (without quitting 
         * application).
         */
        pj_assert(!"Failed to register Session Timer deinit.");
        PJ_LOG(1, (THIS_FILE, "Failed to register Session Timer deinit."));
    }

    is_initialized = PJ_TRUE;

    return PJ_SUCCESS;
}


/*
 * Initialize Session Timers setting with default values.
 */
PJ_DEF(pj_status_t) pjsip_timer_setting_default(pjsip_timer_setting *setting)
{
    pj_bzero(setting, sizeof(pjsip_timer_setting));

    setting->sess_expires = PJSIP_SESS_TIMER_DEF_SE;
    setting->min_se = ABS_MIN_SE;

    return PJ_SUCCESS;
}

/*
 * Initialize Session Timers in an INVITE session. 
 */
PJ_DEF(pj_status_t) pjsip_timer_init_session(
                                        pjsip_inv_session *inv,
                                        const pjsip_timer_setting *setting)
{
    pjsip_timer_setting *s;

    pj_assert(is_initialized);
    PJ_ASSERT_RETURN(inv, PJ_EINVAL);

    /* Allocate and/or reset Session Timers structure */
    if (!inv->timer)
        inv->timer = PJ_POOL_ZALLOC_T(inv->pool, pjsip_timer);
    else
        pj_bzero(inv->timer, sizeof(pjsip_timer));

    s = &inv->timer->setting;

    /* Init Session Timers setting */
    if (setting) {
        PJ_ASSERT_RETURN(setting->min_se >= ABS_MIN_SE,
                         PJ_ETOOSMALL);
        PJ_ASSERT_RETURN(setting->sess_expires >= setting->min_se,
                         PJ_EINVAL);

        pj_memcpy(s, setting, sizeof(*s));
    } else {
        pjsip_timer_setting_default(s);
    }

    return PJ_SUCCESS;
}


/*
 * Create Session-Expires header.
 */
PJ_DEF(pjsip_sess_expires_hdr*) pjsip_sess_expires_hdr_create(
                                                        pj_pool_t *pool)
{
    pjsip_sess_expires_hdr *hdr = PJ_POOL_ZALLOC_T(pool,
                                                   pjsip_sess_expires_hdr);

    pj_assert(is_initialized);

    hdr->type = PJSIP_H_OTHER;
    hdr->name = STR_SE;
    hdr->sname = STR_SHORT_SE;
    hdr->vptr = &se_hdr_vptr;
    pj_list_init(hdr);
    pj_list_init(&hdr->other_param);
    return hdr;
}


/*
 * Create Min-SE header.
 */
PJ_DEF(pjsip_min_se_hdr*) pjsip_min_se_hdr_create(pj_pool_t *pool)
{
    pjsip_min_se_hdr *hdr = PJ_POOL_ZALLOC_T(pool, pjsip_min_se_hdr);

    pj_assert(is_initialized);

    hdr->type = PJSIP_H_OTHER;
    hdr->name = STR_MIN_SE;
    hdr->vptr = &min_se_hdr_vptr;
    pj_list_init(hdr);
    pj_list_init(&hdr->other_param);
    return hdr;
}


/* 
 * This function generates headers for Session Timers for intial and
 * refresh INVITE or UPDATE.
 */
PJ_DEF(pj_status_t) pjsip_timer_update_req(pjsip_inv_session *inv,
                                           pjsip_tx_data *tdata)
{
    PJ_ASSERT_RETURN(inv && tdata, PJ_EINVAL);

    /* Check if Session Timers is supported */
    if ((inv->options & PJSIP_INV_SUPPORT_TIMER) == 0)
        return PJ_SUCCESS;

    pj_assert(is_initialized);

    /* Make sure Session Timers is initialized */
    if (inv->timer == NULL)
        pjsip_timer_init_session(inv, NULL);

    /* If refresher role (i.e: ours or peer) has been set/negotiated, 
     * better to keep it.
     */
    if (inv->timer->refresher != TR_UNKNOWN) {
        pj_bool_t as_refresher;

        /* Check our refresher role */
        as_refresher = 
            (inv->timer->refresher==TR_UAC && inv->timer->role==PJSIP_ROLE_UAC) ||
            (inv->timer->refresher==TR_UAS && inv->timer->role==PJSIP_ROLE_UAS);

        /* Update transaction role */
        inv->timer->role = PJSIP_ROLE_UAC;

        /* Update refresher role */
        inv->timer->refresher = as_refresher? TR_UAC : TR_UAS;
    }

    /* Add Session Timers headers */
    add_timer_headers(inv, tdata, PJ_TRUE, PJ_TRUE);

    return PJ_SUCCESS;
}

/* 
 * This function will handle Session Timers part of INVITE/UPDATE 
 * responses with code:
 * - 422 (Session Interval Too Small)
 * - 2xx final response
 */
PJ_DEF(pj_status_t) pjsip_timer_process_resp(pjsip_inv_session *inv,
                                             const pjsip_rx_data *rdata,
                                             pjsip_status_code *st_code)
{
    const pjsip_msg *msg;

    PJ_ASSERT_ON_FAIL(inv && rdata,
        {if(st_code)*st_code=PJSIP_SC_INTERNAL_SERVER_ERROR;return PJ_EINVAL;});

    /* Check if Session Timers is supported */
    if ((inv->options & PJSIP_INV_SUPPORT_TIMER) == 0) {
        if (rdata->msg_info.msg->line.status.code == 
            PJSIP_SC_SESSION_TIMER_TOO_SMALL)
        {
            return PJSIP_EINVALIDSTATUS;
        } else {
            return PJ_SUCCESS;
        }
    }

    pj_assert(is_initialized);

    msg = rdata->msg_info.msg;
    pj_assert(msg->type == PJSIP_RESPONSE_MSG);

    /* Only process response of INVITE or UPDATE */
    if (rdata->msg_info.cseq->method.id != PJSIP_INVITE_METHOD &&
        pjsip_method_cmp(&rdata->msg_info.cseq->method, &pjsip_update_method))
    {
        return PJ_SUCCESS;
    }
    
    if (msg->line.status.code == PJSIP_SC_SESSION_TIMER_TOO_SMALL) {
        /* Our Session-Expires is too small, let's update it based on
         * Min-SE header in the response.
         */
        pjsip_tx_data *tdata;
        pjsip_min_se_hdr *min_se_hdr;
        pjsip_hdr *hdr;
        pjsip_via_hdr *via;

        /* Get Min-SE value from response */
        min_se_hdr = (pjsip_min_se_hdr*) 
                     pjsip_msg_find_hdr_by_name(msg, &STR_MIN_SE, NULL);
        if (min_se_hdr == NULL) {
            /* Response 422 MUST contain Min-SE header */
            PJ_LOG(3, (inv->pool->obj_name, 
                       "Received 422 (Session Interval Too Small) response "
                       "without Min-SE header!"));

            pjsip_timer_end_session(inv);
            return PJSIP_EMISSINGHDR;
        }

        /* Avoid 422 retry loop. Ignore it and stop retrying if dialog is
         * established, otherwise, just return error to disconnect the call.
         */
        if (rdata->msg_info.cseq->cseq == inv->timer->last_422_cseq + 1) {
            if (inv->state == PJSIP_INV_STATE_CONFIRMED) {
                inv->invite_tsx = NULL;
                return PJ_SUCCESS;
            } else {
                return PJSIP_ERRNO_FROM_SIP_STATUS(
                                            PJSIP_SC_SESSION_TIMER_TOO_SMALL);
            }
        }
        inv->timer->last_422_cseq = rdata->msg_info.cseq->cseq;

        /* Session Timers should have been initialized here */
        pj_assert(inv->timer);

        /* Update Min-SE */
        inv->timer->setting.min_se = PJ_MAX(min_se_hdr->min_se, 
                                            inv->timer->setting.min_se);

        /* Update Session Timers setting */
        if (inv->timer->setting.sess_expires < inv->timer->setting.min_se)
            inv->timer->setting.sess_expires = inv->timer->setting.min_se;

        /* Prepare to restart the request */

        /* Get the original INVITE/UPDATE request. */
        tdata = pjsip_rdata_get_tsx((pjsip_rx_data*)rdata)->last_tx;

        /* Remove branch param in Via header. */
        via = (pjsip_via_hdr*) pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL);
        pj_assert(via);
        via->branch_param.slen = 0;

        /* Restore strict route set.
         * See https://github.com/pjsip/pjproject/issues/492
         */
        pjsip_restore_strict_route_set(tdata);

        /* Must invalidate the message! */
        pjsip_tx_data_invalidate_msg(tdata);

        pjsip_tx_data_add_ref(tdata);

        /* Update Session Timers headers */
        hdr = (pjsip_hdr*) pjsip_msg_find_hdr_by_name(tdata->msg, 
                                                      &STR_MIN_SE, NULL);
        if (hdr != NULL) pj_list_erase(hdr);

        hdr = (pjsip_hdr*) pjsip_msg_find_hdr_by_names(tdata->msg, &STR_SE,
                                                       &STR_SHORT_SE, NULL);
        if (hdr != NULL) pj_list_erase(hdr);

        add_timer_headers(inv, tdata, PJ_TRUE, PJ_TRUE);

        /* Restart UAC if this is initial INVITE, or simply reset tsx for
         * subsequent INVITE.
         */
        if (inv->state < PJSIP_INV_STATE_CONFIRMED)
            pjsip_inv_uac_restart(inv, PJ_FALSE);
        else if (tdata->msg->line.req.method.id == PJSIP_INVITE_METHOD)
            inv->invite_tsx = NULL;

        /* Resend the updated request */
        pjsip_inv_send_msg(inv, tdata);

        return PJ_SUCCESS;

    } else if (msg->line.status.code/100 == 2) {

        pjsip_sess_expires_hdr *se_hdr;

        /* Find Session-Expires header */
        se_hdr = (pjsip_sess_expires_hdr*) pjsip_msg_find_hdr_by_names(
                                                msg, &STR_SE, 
                                                &STR_SHORT_SE, NULL);
        if (se_hdr == NULL) {
            /* Remote doesn't support/want Session Timers, check if local 
             * require or force to use Session Timers.
             */
            if (inv->options & PJSIP_INV_REQUIRE_TIMER) {
                if (st_code)
                    *st_code = PJSIP_SC_EXTENSION_REQUIRED;
                pjsip_timer_end_session(inv);
                return PJSIP_ERRNO_FROM_SIP_STATUS(
                                            PJSIP_SC_EXTENSION_REQUIRED);
            }

            if ((inv->options & PJSIP_INV_ALWAYS_USE_TIMER) == 0) {
                /* Session Timers not forced */
                pjsip_timer_end_session(inv);
                return PJ_SUCCESS;
            }
        }
            
        /* Make sure Session Timers is initialized */
        if (inv->timer == NULL)
            pjsip_timer_init_session(inv, NULL);

        /* Session expiration period specified by remote is lower than our
         * Min-SE.
         */
        if (se_hdr && 
            se_hdr->sess_expires < inv->timer->setting.min_se)
        {
            /* See ticket #954, instead of returning non-PJ_SUCCESS (which
             * may cause disconnecting call/dialog), let's just accept the
             * SE and update our local SE, as long as it isn't less than 90s.
             */
            if (se_hdr->sess_expires >= ABS_MIN_SE) {
                PJ_LOG(3, (inv->pool->obj_name, 
                           "Peer responds with bad Session-Expires, %ds, "
                           "which is less than Min-SE specified in request, "
                           "%ds. Well, let's just accept and use it.",
                           se_hdr->sess_expires, inv->timer->setting.min_se));

                inv->timer->setting.sess_expires = se_hdr->sess_expires;
                inv->timer->setting.min_se = se_hdr->sess_expires;
            }

            //if (st_code)
            //  *st_code = PJSIP_SC_SESSION_TIMER_TOO_SMALL;
            //pjsip_timer_end_session(inv);
            //return PJSIP_ERRNO_FROM_SIP_STATUS(
            //                              PJSIP_SC_SESSION_TIMER_TOO_SMALL);
        }

        /* Update SE. Session-Expires in response cannot be lower than Min-SE.
         * Session-Expires in response can only be equal or lower than in 
         * request.
         */
        if (se_hdr && 
            se_hdr->sess_expires <= inv->timer->setting.sess_expires &&
            se_hdr->sess_expires >= inv->timer->setting.min_se)
        {
            /* Good SE from remote, update local SE */
            inv->timer->setting.sess_expires = se_hdr->sess_expires;
        }

        /* Set the refresher */
        if (se_hdr && pj_stricmp(&se_hdr->refresher, &STR_UAC) == 0)
            inv->timer->refresher = TR_UAC;
        else if (se_hdr && pj_stricmp(&se_hdr->refresher, &STR_UAS) == 0)
            inv->timer->refresher = TR_UAS;
        else
            /* UAS should set the refresher, however, there is a case that
             * UAS doesn't support/want Session Timers but the UAC insists
             * to use Session Timers.
             */
            inv->timer->refresher = TR_UAC;

        /* Remember our role in this transaction */
        inv->timer->role = PJSIP_ROLE_UAC;

        /* Finally, set active flag and start the Session Timers */
        inv->timer->active = PJ_TRUE;
        start_timer(inv);

    } else if (pjsip_method_cmp(&rdata->msg_info.cseq->method,
                                &pjsip_update_method) == 0 &&
               msg->line.status.code >= 400 && msg->line.status.code < 600)
    {
        /* This is to handle error response to previous UPDATE that was
         * sent without SDP. In this case, retry sending UPDATE but
         * with SDP this time.
         * Note: the additional expressions are to check that the
         *       UPDATE was really the one sent by us, not by other
         *       call components (e.g. to change codec)
         */
        if (inv->timer->timer.id == 0 && inv->timer->use_update &&
            inv->timer->with_sdp == PJ_FALSE)
        {
            inv->timer->with_sdp = PJ_TRUE;
            timer_cb(NULL, &inv->timer->timer);
        }
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t)  pjsip_timer_handle_refresh_error(
                                            pjsip_inv_session *inv,
                                            pjsip_event *event)
{    
    PJ_ASSERT_RETURN(inv && event, PJ_EINVAL);

    /* Check if Session Timers is supported */
    if ((inv->options & PJSIP_INV_SUPPORT_TIMER) == 0)
        return PJ_SUCCESS;    

    pj_assert(is_initialized);

    if (inv->timer && inv->timer->active) {
        pj_bool_t as_refresher;

        /* Check our role */
        as_refresher = ((inv->timer->refresher == TR_UAC) && 
                        (inv->timer->role == PJSIP_ROLE_UAC)) ||
                       ((inv->timer->refresher == TR_UAS) && 
                        (inv->timer->role == PJSIP_ROLE_UAS));


        if ((as_refresher) && (event->type == PJSIP_EVENT_TSX_STATE) && 
            (inv->timer->refresh_tdata == event->body.tsx_state.tsx->last_tx)) 
        {
            pjsip_status_code st_code;
            pjsip_tx_data *bye;
            pj_status_t status;

            st_code = 
                    (pjsip_status_code)event->body.tsx_state.tsx->status_code;

            PJ_LOG(3, (inv->pool->obj_name, 
                        "Receive error %d for refresh request %.*s/cseq=%d",
                        st_code, (int)event->body.tsx_state.tsx->method.name.slen,
                        event->body.tsx_state.tsx->method.name.ptr,
                        event->body.tsx_state.tsx->cseq));

            if (st_code == 503 && PJSIP_SESS_TIMER_RETRY_DELAY >= 0) {
                /* Retry the refresh after some delay */
                pj_time_val delay = {PJSIP_SESS_TIMER_RETRY_DELAY, 0};

                PJ_LOG(3, (inv->pool->obj_name, "Scheduling to retry refresh "
                           "request after %ld second(s)", delay.sec));

                inv->timer->timer.id = 1;
                pjsip_endpt_schedule_timer(inv->dlg->endpt,
                                           &inv->timer->timer, &delay);
            } else {
                PJ_LOG(3, (inv->pool->obj_name, "Ending session now"));

                status = pjsip_inv_end_session(inv, 
                                    event->body.tsx_state.tsx->status_code, 
                                    pjsip_get_status_text(st_code), 
                                    &bye);

                if (status == PJ_SUCCESS && bye)
                    status = pjsip_inv_send_msg(inv, bye);
            }
        }
    }

    return PJ_SUCCESS;
}

/*
 * Handle incoming INVITE or UPDATE request.
 */
PJ_DEF(pj_status_t) pjsip_timer_process_req(pjsip_inv_session *inv,
                                            const pjsip_rx_data *rdata,
                                            pjsip_status_code *st_code)
{
    pjsip_min_se_hdr *min_se_hdr;
    pjsip_sess_expires_hdr *se_hdr;
    const pjsip_msg *msg;
    unsigned min_se;

    PJ_ASSERT_ON_FAIL(inv && rdata,
        {if(st_code)*st_code=PJSIP_SC_INTERNAL_SERVER_ERROR;return PJ_EINVAL;});

    /* Check if Session Timers is supported */
    if ((inv->options & PJSIP_INV_SUPPORT_TIMER) == 0)
        return PJ_SUCCESS;

    pj_assert(is_initialized);

    msg = rdata->msg_info.msg;
    pj_assert(msg->type == PJSIP_REQUEST_MSG);

    /* Only process INVITE or UPDATE request */
    if (msg->line.req.method.id != PJSIP_INVITE_METHOD &&
        pjsip_method_cmp(&rdata->msg_info.cseq->method, &pjsip_update_method))
    {
        return PJ_SUCCESS;
    }

    /* Make sure Session Timers is initialized */
    if (inv->timer == NULL)
        pjsip_timer_init_session(inv, NULL);

    /* Find Session-Expires header */
    se_hdr = (pjsip_sess_expires_hdr*) pjsip_msg_find_hdr_by_names(
                                            msg, &STR_SE, &STR_SHORT_SE, NULL);
    /* Find Min-SE header */
    min_se_hdr = (pjsip_min_se_hdr*) pjsip_msg_find_hdr_by_name(msg, 
                                                            &STR_MIN_SE, NULL);
    /* Update Min-SE */
    min_se = inv->timer->setting.min_se;
    if (min_se_hdr)
        min_se = PJ_MAX(min_se_hdr->min_se, min_se);

    /* Validate SE. Session-Expires cannot be lower than Min-SE 
     * (or 90 seconds if Min-SE is not set).
     */
    if (se_hdr && se_hdr->sess_expires < min_se) {
        if (st_code)
            *st_code = PJSIP_SC_SESSION_TIMER_TOO_SMALL;
        return PJSIP_ERRNO_FROM_SIP_STATUS(PJSIP_SC_SESSION_TIMER_TOO_SMALL);
    }

    /* Update SE. Note that there is a case that SE is not available in the
     * request (which means remote doesn't want/support it), but local insists
     * to use Session Timers.
     */
    if (se_hdr) {
        /* Update SE as specified by peer. */
        inv->timer->setting.sess_expires = se_hdr->sess_expires;
    } else if (inv->timer->setting.sess_expires < min_se) {
        /* There is no SE in the request (remote support Session Timers but
         * doesn't want to use it, it just specify Min-SE) and local SE is 
         * lower than Min-SE specified by remote.
         */
        inv->timer->setting.sess_expires = min_se;
    }

    /* If incoming request has no SE header, it means remote doesn't
     * support/want Session Timers. So let's check if local require or
     * force to use Session Timers. Note that Supported and Require headers
     * negotiation should have been verified by invite session.
     */
    if (se_hdr == NULL) {
        if ((inv->options & 
            (PJSIP_INV_REQUIRE_TIMER | PJSIP_INV_ALWAYS_USE_TIMER)) == 0)
        {
            /* Session Timers not forced/required */
            pjsip_timer_end_session(inv);
            return PJ_SUCCESS;
        }
    }

    /* Set the refresher */
    if (se_hdr && pj_stricmp(&se_hdr->refresher, &STR_UAC) == 0)
        inv->timer->refresher = TR_UAC;
    else if (se_hdr && pj_stricmp(&se_hdr->refresher, &STR_UAS) == 0)
        inv->timer->refresher = TR_UAS;
    else {
        /* If refresher role (i.e: ours or peer) has been set/negotiated, 
         * better to keep it.
         */
        if (inv->timer->refresher != TR_UNKNOWN) {
            pj_bool_t as_refresher;

            /* Check our refresher role */
            as_refresher = 
                (inv->timer->refresher==TR_UAC && inv->timer->role==PJSIP_ROLE_UAC) ||
                (inv->timer->refresher==TR_UAS && inv->timer->role==PJSIP_ROLE_UAS);

            /* Update refresher role */
            inv->timer->refresher = as_refresher? TR_UAS : TR_UAC;
        } else {
            /* If UAC supports timer and Session-Expires header is present
             * in the request, set UAC as refresher.
             * If UAC doesn't support timer and a proxy inserts a
             * Session-Expires header, then UAS has to be the
             * refresher (according to RFC 4028 Section 9).
             */
            pj_bool_t uac_supports_timer = PJ_FALSE;
            pjsip_supported_hdr *sup_hdr;
            
            sup_hdr = (pjsip_supported_hdr*)
                      pjsip_msg_find_hdr(msg, PJSIP_H_SUPPORTED, NULL);
            if (sup_hdr) {
                unsigned i;
                
                for (i = 0; i < sup_hdr->count; i++) {
                    if (pj_stricmp(&sup_hdr->values[i], &STR_TIMER) == 0) {
                        uac_supports_timer = PJ_TRUE;
                        break;
                    }
                }
            }
            inv->timer->refresher = (uac_supports_timer && se_hdr)? TR_UAC:
                                    TR_UAS;
        }
    }

    /* Remember our role in this transaction */
    inv->timer->role = PJSIP_ROLE_UAS;

    /* Set active flag */
    inv->timer->active = PJ_TRUE;

    return PJ_SUCCESS;
}

/*
 * Handle outgoing response with status code 2xx & 422.
 */
PJ_DEF(pj_status_t) pjsip_timer_update_resp(pjsip_inv_session *inv,
                                            pjsip_tx_data *tdata)
{
    pjsip_msg *msg;

    /* Check if Session Timers is supported */
    if ((inv->options & PJSIP_INV_SUPPORT_TIMER) == 0)
        return PJ_SUCCESS;

    pj_assert(is_initialized);
    PJ_ASSERT_RETURN(inv && tdata, PJ_EINVAL);

    msg = tdata->msg;

    if (msg->line.status.code/100 == 2)
    {
        if (inv->timer && inv->timer->active) {
            /* Add Session-Expires header and start the timer */
            add_timer_headers(inv, tdata, PJ_TRUE, PJ_FALSE);

            /* Add 'timer' to Require header (see ticket #1560). */
            if (inv->timer->refresher == TR_UAC) {
                pjsip_require_hdr *req_hdr;
                pj_bool_t req_hdr_has_timer = PJ_FALSE;

                req_hdr = (pjsip_require_hdr*)
                           pjsip_msg_find_hdr(tdata->msg, PJSIP_H_REQUIRE,
                                              NULL);
                if (req_hdr == NULL) {
                    req_hdr = pjsip_require_hdr_create(tdata->pool);
                    PJ_ASSERT_RETURN(req_hdr, PJ_ENOMEM);
                    pjsip_msg_add_hdr(tdata->msg, (pjsip_hdr*)req_hdr);
                } else {
                    unsigned i;
                    for (i = 0; i < req_hdr->count; ++i) {
                        if (pj_stricmp(&req_hdr->values[i], &STR_TIMER)) {
                            req_hdr_has_timer = PJ_TRUE;
                            break;
                        }
                    }
                }
                if (!req_hdr_has_timer)
                    req_hdr->values[req_hdr->count++] = STR_TIMER;
            }
            
            /* Finally, start timer. */
            start_timer(inv);
        }
    } 
    else if (msg->line.status.code == PJSIP_SC_SESSION_TIMER_TOO_SMALL)
    {
        /* Add Min-SE header */
        add_timer_headers(inv, tdata, PJ_FALSE, PJ_TRUE);
    }

    return PJ_SUCCESS;
}


/*
 * End the Session Timers.
 */
PJ_DEF(pj_status_t) pjsip_timer_end_session(pjsip_inv_session *inv)
{
    PJ_ASSERT_RETURN(inv, PJ_EINVAL);

    if (inv->timer) {
        /* Reset active flag */
        inv->timer->active = PJ_FALSE;

        /* Stop Session Timers */
        stop_timer(inv);
    }

    return PJ_SUCCESS;
}
