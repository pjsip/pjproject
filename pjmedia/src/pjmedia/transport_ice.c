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
#include <pjmedia/transport_ice.h>
#include <pjnath/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>

#define THIS_FILE   "transport_ice.c"
#if 0
#   define TRACE__(expr)    PJ_LOG(5,expr)
#else
#   define TRACE__(expr)
#endif

enum oa_role
{
    ROLE_NONE,
    ROLE_OFFERER,
    ROLE_ANSWERER
};

struct sdp_state
{
    unsigned            match_comp_cnt; /* Matching number of components    */
    pj_bool_t           ice_mismatch;   /* Address doesn't match candidates */
    pj_bool_t           ice_restart;    /* Offer to restart ICE             */
    pj_ice_sess_role    local_role;     /* Our role                         */
    pj_bool_t           has_trickle;    /* Has trickle ICE attribute        */
};

/* ICE listener */
typedef struct ice_listener
{
    PJ_DECL_LIST_MEMBER(struct ice_listener);
    pjmedia_ice_cb       cb;
    void                *user_data;
} ice_listener;

struct transport_ice
{
    pjmedia_transport    base;
    pj_pool_t           *pool;
    unsigned             options;       /**< Transport options.             */

    unsigned             comp_cnt;
    pj_ice_strans       *ice_st;

    pjmedia_ice_cb       cb;
    ice_listener         listener;
    ice_listener         listener_empty;
    unsigned             media_option;

    pj_bool_t            initial_sdp;
    enum oa_role         oa_role;       /**< Last role in SDP offer/answer  */
    struct sdp_state     rem_offer_state;/**< Describes the remote offer    */

    void                *stream;
    pj_sockaddr          remote_rtp;
    pj_sockaddr          remote_rtcp;
    unsigned             addr_len;      /**< Length of addresses.           */

    pj_bool_t            use_ice;
    pj_sockaddr          rtp_src_addr;  /**< Actual source RTP address.     */
    unsigned             rtp_src_cnt;   /**< How many pkt from this addr.   */
    pj_sockaddr          rtcp_src_addr; /**< Actual source RTCP address.    */
    unsigned             rtcp_src_cnt;  /**< How many pkt from this addr.   */
    pj_bool_t            enable_rtcp_mux;/**< Enable RTP& RTCP multiplexing?*/
    pj_bool_t            use_rtcp_mux;  /**< Use RTP & RTCP multiplexing?   */

    unsigned             tx_drop_pct;   /**< Percent of tx pkts to drop.    */
    unsigned             rx_drop_pct;   /**< Percent of rx pkts to drop.    */

    pj_ice_sess_trickle  trickle_ice;   /**< Trickle ICE mode.              */
    unsigned             last_send_cand_cnt[PJ_ICE_MAX_COMP];
                                        /**< Last local candidate count.    */
    pj_bool_t            end_of_cand;   /**< Local cand gathering done?     */
    pj_str_t             sdp_mid;       /**< SDP "a=mid" attribute.         */

    void               (*rtp_cb)(void*,
                                 void*,
                                 pj_ssize_t);
    void               (*rtp_cb2)(pjmedia_tp_cb_param*);
    void               (*rtcp_cb)(void*,
                                  void*,
                                  pj_ssize_t);
};


/*
 * These are media transport operations.
 */
static pj_status_t transport_get_info (pjmedia_transport *tp,
                                       pjmedia_transport_info *info);
static pj_status_t transport_attach   (pjmedia_transport *tp,
                                       void *user_data,
                                       const pj_sockaddr_t *rem_addr,
                                       const pj_sockaddr_t *rem_rtcp,
                                       unsigned addr_len,
                                       void (*rtp_cb)(void*,
                                                      void*,
                                                      pj_ssize_t),
                                       void (*rtcp_cb)(void*,
                                                       void*,
                                                       pj_ssize_t));
static pj_status_t transport_attach2  (pjmedia_transport *tp,
                                       pjmedia_transport_attach_param
                                           *att_param);
static void        transport_detach   (pjmedia_transport *tp,
                                       void *strm);
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
                                       const void *pkt,
                                       pj_size_t size);
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
                                       const void *pkt,
                                       pj_size_t size);
static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
                                       const pj_sockaddr_t *addr,
                                       unsigned addr_len,
                                       const void *pkt,
                                       pj_size_t size);
static pj_status_t transport_media_create(pjmedia_transport *tp,
                                       pj_pool_t *pool,
                                       unsigned options,
                                       const pjmedia_sdp_session *rem_sdp,
                                       unsigned media_index);
static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
                                        pj_pool_t *tmp_pool,
                                        pjmedia_sdp_session *sdp_local,
                                        const pjmedia_sdp_session *rem_sdp,
                                        unsigned media_index);
static pj_status_t transport_media_start(pjmedia_transport *tp,
                                       pj_pool_t *pool,
                                       const pjmedia_sdp_session *sdp_local,
                                       const pjmedia_sdp_session *rem_sdp,
                                       unsigned media_index);
static pj_status_t transport_media_stop(pjmedia_transport *tp);
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
                                       pjmedia_dir dir,
                                       unsigned pct_lost);
static pj_status_t transport_destroy  (pjmedia_transport *tp);

/*
 * And these are ICE callbacks.
 */
static void ice_on_rx_data(pj_ice_strans *ice_st, 
                           unsigned comp_id, 
                           void *pkt, pj_size_t size,
                           const pj_sockaddr_t *src_addr,
                           unsigned src_addr_len);
static void ice_on_ice_complete(pj_ice_strans *ice_st, 
                                pj_ice_strans_op op,
                                pj_status_t status);
static void ice_on_new_candidate(pj_ice_strans *ice_st,
                                 const pj_ice_sess_cand *cand,
                                 pj_bool_t last);

/*
 * Clean up ICE resources.
 */
static void tp_ice_on_destroy(void *arg);


static pjmedia_transport_op transport_ice_op = 
{
    &transport_get_info,
    &transport_attach,
    &transport_detach,
    &transport_send_rtp,
    &transport_send_rtcp,
    &transport_send_rtcp2,
    &transport_media_create,
    &transport_encode_sdp,
    &transport_media_start,
    &transport_media_stop,
    &transport_simulate_lost,
    &transport_destroy,
    &transport_attach2
};

static const pj_str_t STR_CANDIDATE     = { "candidate", 9};
static const pj_str_t STR_REM_CAND      = { "remote-candidates", 17 };
static const pj_str_t STR_ICE_LITE      = { "ice-lite", 8};
static const pj_str_t STR_ICE_MISMATCH  = { "ice-mismatch", 12};
static const pj_str_t STR_ICE_UFRAG     = { "ice-ufrag", 9 };
static const pj_str_t STR_ICE_PWD       = { "ice-pwd", 7 };
static const pj_str_t STR_IP4           = { "IP4", 3 };
static const pj_str_t STR_IP6           = { "IP6", 3 };
static const pj_str_t STR_RTCP          = { "rtcp", 4 };
static const pj_str_t STR_RTCP_MUX      = { "rtcp-mux", 8 };
static const pj_str_t STR_BANDW_RR      = { "RR", 2 };
static const pj_str_t STR_BANDW_RS      = { "RS", 2 };
static const pj_str_t STR_ICE_OPTIONS   = { "ice-options", 11 };
static const pj_str_t STR_TRICKLE       = { "trickle", 7 };
static const pj_str_t STR_END_OF_CAND   = { "end-of-candidates", 17 };

enum {
    COMP_RTP = 1,
    COMP_RTCP = 2
};


/* Forward declaration of internal functions */

static int print_sdp_cand_attr(char *buffer, int max_len,
                               const pj_ice_sess_cand *cand);
static void get_ice_attr(const pjmedia_sdp_session *rem_sdp,
                         const pjmedia_sdp_media *rem_m,
                         const pjmedia_sdp_attr **p_ice_ufrag,
                         const pjmedia_sdp_attr **p_ice_pwd);
static pj_status_t parse_cand(const char *obj_name,
                              pj_pool_t *pool,
                              const pj_str_t *orig_input,
                              pj_ice_sess_cand *cand);


/*
 * Create ICE media transport.
 */
PJ_DEF(pj_status_t) pjmedia_ice_create(pjmedia_endpt *endpt,
                                       const char *name,
                                       unsigned comp_cnt,
                                       const pj_ice_strans_cfg *cfg,
                                       const pjmedia_ice_cb *cb,
                                       pjmedia_transport **p_tp)
{
    return pjmedia_ice_create2(endpt, name, comp_cnt, cfg, cb, 0, p_tp);
}

/*
 * Create ICE media transport.
 */
PJ_DEF(pj_status_t) pjmedia_ice_create2(pjmedia_endpt *endpt,
                                        const char *name,
                                        unsigned comp_cnt,
                                        const pj_ice_strans_cfg *cfg,
                                        const pjmedia_ice_cb *cb,
                                        unsigned options,
                                        pjmedia_transport **p_tp)
{
    return pjmedia_ice_create3(endpt, name, comp_cnt, cfg, cb,
                               options, NULL, p_tp);
}

/*
 * Create ICE media transport.
 */
PJ_DEF(pj_status_t) pjmedia_ice_create3(pjmedia_endpt *endpt,
                                        const char *name,
                                        unsigned comp_cnt,
                                        const pj_ice_strans_cfg *cfg,
                                        const pjmedia_ice_cb *cb,
                                        unsigned options,
                                        void *user_data,
                                        pjmedia_transport **p_tp)
{
    pj_pool_t *pool;
    pj_ice_strans_cb ice_st_cb;
    pj_ice_strans_cfg ice_st_cfg;
    struct transport_ice *tp_ice;
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt && comp_cnt && cfg && p_tp, PJ_EINVAL);

    /* Create transport instance */
    pool = pjmedia_endpt_create_pool(endpt, name, 512, 512);
    tp_ice = PJ_POOL_ZALLOC_T(pool, struct transport_ice);
    tp_ice->pool = pool;
    tp_ice->options = options;
    tp_ice->comp_cnt = comp_cnt;
    pj_ansi_strxcpy(tp_ice->base.name, pool->obj_name, 
                    sizeof(tp_ice->base.name));
    tp_ice->base.op = &transport_ice_op;
    tp_ice->base.type = PJMEDIA_TRANSPORT_TYPE_ICE;
    tp_ice->base.user_data = user_data;
    tp_ice->initial_sdp = PJ_TRUE;
    tp_ice->oa_role = ROLE_NONE;
    tp_ice->use_ice = PJ_FALSE;
    tp_ice->trickle_ice = cfg->opt.trickle;
    pj_list_init(&tp_ice->listener);
    pj_list_init(&tp_ice->listener_empty);

    pj_memcpy(&ice_st_cfg, cfg, sizeof(pj_ice_strans_cfg));
    if (cb)
        pj_memcpy(&tp_ice->cb, cb, sizeof(pjmedia_ice_cb));

    /* Assign return value first because ICE might call callback
     * in create()
     */
    *p_tp = &tp_ice->base;

    /* Configure ICE callbacks */
    pj_bzero(&ice_st_cb, sizeof(ice_st_cb));
    ice_st_cb.on_ice_complete = &ice_on_ice_complete;
    ice_st_cb.on_rx_data = &ice_on_rx_data;
    ice_st_cb.on_new_candidate = &ice_on_new_candidate;

    /* Configure RTP socket buffer settings, if not set */
    if (ice_st_cfg.comp[COMP_RTP-1].so_rcvbuf_size == 0) {
        ice_st_cfg.comp[COMP_RTP-1].so_rcvbuf_size = 
                            PJMEDIA_TRANSPORT_SO_RCVBUF_SIZE;
    }
    if (ice_st_cfg.comp[COMP_RTP-1].so_sndbuf_size == 0) {
        ice_st_cfg.comp[COMP_RTP-1].so_sndbuf_size = 
                            PJMEDIA_TRANSPORT_SO_SNDBUF_SIZE;
    }
    if (ice_st_cfg.send_buf_size == 0)
        ice_st_cfg.send_buf_size = PJMEDIA_MAX_MTU;

    /* Create ICE */
    status = pj_ice_strans_create(name, &ice_st_cfg, comp_cnt, tp_ice, 
                                  &ice_st_cb, &tp_ice->ice_st);
    if (status != PJ_SUCCESS) {
        pj_pool_release(pool);
        *p_tp = NULL;
        return status;
    }

    /* Sync to ICE */
    {
        pj_grp_lock_t *grp_lock = pj_ice_strans_get_grp_lock(tp_ice->ice_st);
        pj_grp_lock_add_ref(grp_lock);
        pj_grp_lock_add_handler(grp_lock, pool, tp_ice, &tp_ice_on_destroy);
        tp_ice->base.grp_lock = grp_lock;
    }

    /* Done */
    return PJ_SUCCESS;
}

PJ_DEF(pj_grp_lock_t *) pjmedia_ice_get_grp_lock(pjmedia_transport *tp)
{
    PJ_ASSERT_RETURN(tp, NULL);
    return pj_ice_strans_get_grp_lock(((struct transport_ice *)tp)->ice_st);
}


/*
 * Add application to receive ICE notifications from the specified ICE media
 * transport.
 */
PJ_DEF(pj_status_t) pjmedia_ice_add_ice_cb( pjmedia_transport *tp,
                                            const pjmedia_ice_cb *cb,
                                            void *user_data)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    ice_listener *il;
    pj_grp_lock_t *grp_lock;

    PJ_ASSERT_RETURN(tp && cb, PJ_EINVAL);
    grp_lock = pjmedia_ice_get_grp_lock(tp);
    PJ_ASSERT_RETURN(grp_lock, PJ_EINVAL);

    pj_grp_lock_acquire(grp_lock);

    if (!pj_list_empty(&tp_ice->listener_empty)) {
        il = tp_ice->listener_empty.next;
        pj_list_erase(il);
        il->cb = *cb;
        il->user_data = user_data;
        pj_list_push_back(&tp_ice->listener, il);
    } else {
        il = PJ_POOL_ZALLOC_T(tp_ice->pool, ice_listener);
        pj_list_init(il);
        il->cb = *cb;
        il->user_data = user_data;
        pj_list_push_back(&tp_ice->listener, il);
    }

    pj_grp_lock_release(grp_lock);

    return PJ_SUCCESS;
}


/*
 * Remove application to stop receiving ICE notifications the specified
 * ICE media transport.
 */
PJ_DEF(pj_status_t) pjmedia_ice_remove_ice_cb( pjmedia_transport *tp,
                                               const pjmedia_ice_cb *cb,
                                               void *user_data)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    ice_listener *il;
    pj_grp_lock_t *grp_lock;

    PJ_ASSERT_RETURN(tp && cb, PJ_EINVAL);
    grp_lock = pjmedia_ice_get_grp_lock(tp);
    PJ_ASSERT_RETURN(grp_lock, PJ_EINVAL);

    pj_grp_lock_acquire(grp_lock);

    for (il=tp_ice->listener.next; il!=&tp_ice->listener; il=il->next) {
        if (pj_memcmp(&il->cb, cb, sizeof(*cb))==0 && il->user_data==user_data)
            break;
    }
    if (il != &tp_ice->listener) {
        pj_list_erase(il);
        pj_list_push_back(&tp_ice->listener_empty, il);
    }

    pj_grp_lock_release(grp_lock);

    return (il != &tp_ice->listener? PJ_SUCCESS : PJ_ENOTFOUND);
}


/* Check if trickle support is signalled in the specified SDP. */
PJ_DEF(pj_bool_t) pjmedia_ice_sdp_has_trickle( const pjmedia_sdp_session *sdp,
                                               unsigned med_idx)
{
    const pjmedia_sdp_media *m;
    const pjmedia_sdp_attr *a;

    PJ_ASSERT_RETURN(sdp && med_idx < sdp->media_count, PJ_EINVAL);

    /* Find in media level */
    m = sdp->media[med_idx];
    a = pjmedia_sdp_attr_find(m->attr_count, m->attr, &STR_ICE_OPTIONS, NULL);
    if (a && pj_strstr(&a->value, &STR_TRICKLE))
        return PJ_TRUE;

    /* Find in session level */
    a = pjmedia_sdp_attr_find(sdp->attr_count, sdp->attr, &STR_ICE_OPTIONS,
                              NULL);
    if (a && pj_strstr(&a->value, &STR_TRICKLE))
        return PJ_TRUE;

    return PJ_FALSE;
}


/* Update check list after discovering and conveying new local ICE candidate,
 * or receiving update of remote ICE candidates in trickle ICE.
 */
PJ_DEF(pj_status_t) pjmedia_ice_trickle_update(
                                             pjmedia_transport *tp,
                                             const pj_str_t *rem_ufrag,
                                             const pj_str_t *rem_passwd,
                                             unsigned rcand_cnt,
                                             const pj_ice_sess_cand rcand[],
                                             pj_bool_t rcand_end)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp_ice && tp_ice->ice_st, PJ_EINVAL);
    PJ_ASSERT_RETURN(tp_ice->trickle_ice != PJ_ICE_SESS_TRICKLE_DISABLED,
                     PJ_EINVALIDOP);


    /* Update the checklist */
    status = pj_ice_strans_update_check_list(tp_ice->ice_st,
                                             rem_ufrag, rem_passwd,
                                             rcand_cnt, rcand, rcand_end);
    if (status != PJ_SUCCESS)
        return status;

    /* Start ICE if both sides have sent their (initial) SDPs */
    if (!pj_ice_strans_sess_is_running(tp_ice->ice_st)) {
        unsigned i, comp_cnt;

        comp_cnt = pj_ice_strans_get_running_comp_cnt(tp_ice->ice_st);
        for (i = 0; i < comp_cnt; ++i) {
            if (tp_ice->last_send_cand_cnt[i] > 0)
                break;
        }
        if (i != comp_cnt) {
            pj_str_t rufrag;
            pj_ice_strans_get_ufrag_pwd(tp_ice->ice_st, NULL, NULL,
                                        &rufrag, NULL);
            if (rufrag.slen > 0) {
                PJ_LOG(3,(THIS_FILE,"Trickle ICE starts connectivity check"));
                status = pj_ice_strans_start_ice(tp_ice->ice_st, NULL, NULL,
                                                 0, NULL);
            }
        }
    }

    return status;
}


/* Fetch trickle ICE info from the specified SDP. */
PJ_DEF(pj_status_t) pjmedia_ice_trickle_decode_sdp(
                                            const pjmedia_sdp_session *sdp,
                                            unsigned media_index,
                                            pj_str_t *mid,
                                            pj_str_t *ufrag,
                                            pj_str_t *passwd,
                                            unsigned *cand_cnt,
                                            pj_ice_sess_cand cand[],
                                            pj_bool_t *end_of_cand)
{
    const pjmedia_sdp_media *m;
    const pjmedia_sdp_attr *a;

    PJ_ASSERT_RETURN(sdp && media_index < sdp->media_count, PJ_EINVAL);

    m = sdp->media[media_index];

    if (mid) {
        a = pjmedia_sdp_attr_find2(m->attr_count, m->attr, "mid", NULL);
        if (a) {
            *mid = a->value;
        } else {
            pj_bzero(mid, sizeof(*mid));
        }
    }

    if (ufrag && passwd) {
        const pjmedia_sdp_attr *a_ufrag, *a_pwd;
        get_ice_attr(sdp, m, &a_ufrag, &a_pwd);
        if (a_ufrag && a_pwd) {
            *ufrag = a_ufrag->value;
            *passwd = a_pwd->value;
        } else {
            pj_bzero(ufrag, sizeof(*ufrag));
            pj_bzero(passwd, sizeof(*passwd));
        }
    }

    if (cand_cnt && cand && *cand_cnt > 0) {
        pj_status_t status;
        unsigned i, cnt = 0;

        for (i=0; i<m->attr_count && cnt<*cand_cnt; ++i) {
            a = m->attr[i];
            if (pj_strcmp(&a->name, &STR_CANDIDATE)!=0)
                continue;

            /* Parse candidate */
            status = parse_cand("trickle-ice", NULL, &a->value, &cand[cnt]);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(4,("trickle-ice", status,
                             "Error in parsing SDP candidate attribute '%.*s', "
                             "candidate is ignored",
                             (int)a->value.slen, a->value.ptr));
                continue;
            }
            ++cnt;
        }
        *cand_cnt = cnt;
    }

    if (end_of_cand) {
        a = pjmedia_sdp_attr_find(m->attr_count, m->attr, &STR_END_OF_CAND,
                                  NULL);
        if (!a) {
            a = pjmedia_sdp_attr_find(sdp->attr_count, sdp->attr,
                                      &STR_END_OF_CAND, NULL);
        }
        *end_of_cand = (a != NULL);
    }
    return PJ_SUCCESS;
}


/* Generate SDP attributes for trickle ICE in the specified SDP. */
PJ_DEF(pj_status_t) pjmedia_ice_trickle_encode_sdp(
                                            pj_pool_t *sdp_pool,
                                            pjmedia_sdp_session *sdp,
                                            const pj_str_t *mid,
                                            const pj_str_t *ufrag,
                                            const pj_str_t *passwd,
                                            unsigned cand_cnt,
                                            const pj_ice_sess_cand cand[],
                                            pj_bool_t end_of_cand)
{
    pjmedia_sdp_media *m = NULL;
    pjmedia_sdp_attr *a;
    unsigned i;

    PJ_ASSERT_RETURN(sdp_pool && sdp, PJ_EINVAL);

    /* Find media by checking "a=mid"*/
    for (i = 0; i < sdp->media_count; ++i) {
        m = sdp->media[i];
        a = pjmedia_sdp_media_find_attr2(m, "mid", NULL);
        if (a && pj_strcmp(&a->value, mid)==0)
            break;
    }

    /* Media not exist, try to add it */
    if (i == sdp->media_count) {
        if (sdp->media_count >= PJMEDIA_MAX_SDP_MEDIA) {
            PJ_LOG(3,(THIS_FILE,"Trickle ICE failed to encode candidates, "
                                "the specified SDP has too many media"));
            return PJ_ETOOMANY;
        }

        /* Add a new media to the SDP */
        m = PJ_POOL_ZALLOC_T(sdp_pool, pjmedia_sdp_media);
        m->desc.media = pj_str("audio");
        m->desc.fmt_count = 1;
        m->desc.fmt[0] = pj_str("0");
        m->desc.transport = pj_str("RTP/AVP");
        sdp->media[sdp->media_count++] = m;

        /* Add media ID attribute "a=mid" */
        a = pjmedia_sdp_attr_create(sdp_pool, "mid", mid);
        pjmedia_sdp_attr_add(&m->attr_count, m->attr, a);
    }

    /* Add "a=ice-options:trickle" in session level */
    a = pjmedia_sdp_attr_find(sdp->attr_count, sdp->attr, &STR_ICE_OPTIONS,
                              NULL);
    if (!a || !pj_strstr(&a->value, &STR_TRICKLE)) {
        a = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_OPTIONS.ptr,
                                    &STR_TRICKLE);
        pjmedia_sdp_attr_add(&sdp->attr_count, sdp->attr, a);
    }

    /* Add "a=ice-options:trickle" in media level */
    /*
    a = pjmedia_sdp_attr_find(m->attr_count, m->attr, &STR_ICE_OPTIONS,
                              NULL);
    if (!a || !pj_strstr(&a->value, &STR_TRICKLE)) {
        a = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_OPTIONS.ptr,
                                    &STR_TRICKLE);
        pjmedia_sdp_attr_add(&m->attr_count, m->attr, a);
    }
    */

    /* Add ice-ufrag & ice-pwd attributes */
    if (ufrag && passwd &&
        !pjmedia_sdp_attr_find(m->attr_count, m->attr, &STR_ICE_UFRAG, NULL))
    {
        a = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_UFRAG.ptr, ufrag);
        pjmedia_sdp_attr_add(&m->attr_count, m->attr, a);

        a = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_PWD.ptr, passwd);
        pjmedia_sdp_attr_add(&m->attr_count, m->attr, a);
    }

    /* Add candidates */
    for (i=0; i<cand_cnt; ++i) {
        enum {
            ATTR_BUF_LEN = 160, /* Max len of a=candidate attr */
            RATTR_BUF_LEN= 160  /* Max len of a=remote-candidates attr */
        };
        char attr_buf[ATTR_BUF_LEN];
        pj_str_t value;

        value.slen = print_sdp_cand_attr(attr_buf, ATTR_BUF_LEN, &cand[i]);
        if (value.slen < 0) {
            pj_assert(!"Not enough attr_buf to print candidate");
            return PJ_EBUG;
        }

        value.ptr = attr_buf;
        a = pjmedia_sdp_attr_create(sdp_pool, STR_CANDIDATE.ptr, &value);
        pjmedia_sdp_attr_add(&m->attr_count, m->attr, a);
    }

    /* Add "a=end-of-candidates" */
    if (end_of_cand) {
        a = pjmedia_sdp_attr_find(m->attr_count, m->attr, &STR_END_OF_CAND,
                                  NULL);
        if (!a) {
            a = pjmedia_sdp_attr_create(sdp_pool, STR_END_OF_CAND.ptr, NULL);
            pjmedia_sdp_attr_add(&m->attr_count, m->attr, a);
        }
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_bool_t) pjmedia_ice_trickle_has_new_cand(pjmedia_transport *tp)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    unsigned i, comp_cnt;

    /* Make sure ICE transport has session already */
    if (!tp_ice->ice_st || !pj_ice_strans_has_sess(tp_ice->ice_st))
        return PJ_FALSE;

    /* Count all local candidates */
    comp_cnt = pj_ice_strans_get_running_comp_cnt(tp_ice->ice_st);
    for (i = 0; i < comp_cnt; ++i) {
        if (tp_ice->last_send_cand_cnt[i] <
            pj_ice_strans_get_cands_count(tp_ice->ice_st, i+1))
        {
            return PJ_TRUE;
        }
    }
    return PJ_FALSE;
}


/* Add any new local candidates to the specified SDP to be conveyed to
 * remote (e.g: via SIP INFO).
 */
PJ_DEF(pj_status_t) pjmedia_ice_trickle_send_local_cand(
                                            pjmedia_transport *tp,
                                            pj_pool_t *sdp_pool,
                                            pjmedia_sdp_session *sdp,
                                            pj_bool_t *p_end_of_cand)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_str_t ufrag, pwd;
    pj_ice_sess_cand cand[PJ_ICE_MAX_CAND];
    unsigned cand_cnt, i, comp_cnt;
    pj_bool_t end_of_cand;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp && sdp_pool && sdp, PJ_EINVAL);

    /* Make sure ICE transport has session already */
    if (!tp_ice->ice_st || !pj_ice_strans_has_sess(tp_ice->ice_st))
        return PJ_EINVALIDOP;

    end_of_cand = tp_ice->end_of_cand;

    /* Get ufrag and pwd from current session */
    pj_ice_strans_get_ufrag_pwd(tp_ice->ice_st, &ufrag, &pwd, NULL, NULL);

    cand_cnt = 0;
    comp_cnt = pj_ice_strans_get_running_comp_cnt(tp_ice->ice_st);
    for (i = 0; i < comp_cnt; ++i) {
        unsigned cnt = PJ_ICE_MAX_CAND - cand_cnt;

        /* Get all local candidates for this comp */
        status = pj_ice_strans_enum_cands(tp_ice->ice_st, i+1,
                                          &cnt, &cand[cand_cnt]);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(3,(tp_ice->base.name, status,
                         "Failed enumerating local candidates for comp-id=%d",
                         i+1));
            continue;
        }
        cand_cnt += cnt;

        tp_ice->last_send_cand_cnt[i] = cnt;
    }

    /* Update the SDP with all local candidates (not just the new ones).
     * https://tools.ietf.org/html/draft-ietf-mmusic-trickle-ice-sip-18:
     * 4.4. Delivering Candidates in INFO Requests: the agent MUST
     * repeat in the INFO request body all candidates that were previously
     * sent under the same combination of "a=ice-pwd:" and "a=ice-ufrag:"
     * in the same order as they were sent before.
     */
    status = pjmedia_ice_trickle_encode_sdp(sdp_pool, sdp, &tp_ice->sdp_mid,
                                            &ufrag, &pwd, cand_cnt, cand,
                                            end_of_cand);
    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(tp_ice->base.name, status,
                     "Failed encoding local candidates to SDP"));
    }

    if (p_end_of_cand)
        *p_end_of_cand = end_of_cand;

    return PJ_SUCCESS;
}


/* Disable ICE when SDP from remote doesn't contain a=candidate line */
static void set_no_ice(struct transport_ice *tp_ice, const char *reason,
                       pj_status_t err)
{
    if (err != PJ_SUCCESS) {
        PJ_PERROR(4,(tp_ice->base.name, err,
                     "Stopping ICE, reason=%s", reason));
    } else {
        PJ_LOG(4,(tp_ice->base.name, 
                  "Stopping ICE, reason=%s", reason));
    }

    if (tp_ice->ice_st) {
        pj_ice_strans_stop_ice(tp_ice->ice_st);
    }

    tp_ice->use_ice = PJ_FALSE;
}


/* Create SDP candidate attribute */
static int print_sdp_cand_attr(char *buffer, int max_len,
                               const pj_ice_sess_cand *cand)
{
    char ipaddr[PJ_INET6_ADDRSTRLEN+2];
    int len, len2;

    len = pj_ansi_snprintf( buffer, max_len,
                            "%.*s %u UDP %u %s %u typ ",
                            (int)cand->foundation.slen,
                            cand->foundation.ptr,
                            (unsigned)cand->comp_id,
                            cand->prio,
                            pj_sockaddr_print(&cand->addr, ipaddr, 
                                              sizeof(ipaddr), 0),
                            (unsigned)pj_sockaddr_get_port(&cand->addr));
    if (len < 1 || len >= max_len)
        return -1;

    switch (cand->type) {
    case PJ_ICE_CAND_TYPE_HOST:
        len2 = pj_ansi_snprintf(buffer+len, max_len-len, "host");
        break;
    case PJ_ICE_CAND_TYPE_SRFLX:
    case PJ_ICE_CAND_TYPE_RELAYED:
    case PJ_ICE_CAND_TYPE_PRFLX:
        len2 = pj_ansi_snprintf(buffer+len, max_len-len,
                                "%s raddr %s rport %d",
                                pj_ice_get_cand_type_name(cand->type),
                                pj_sockaddr_print(&cand->rel_addr, ipaddr,
                                                  sizeof(ipaddr), 0),
                                (int)pj_sockaddr_get_port(&cand->rel_addr));
        break;
    default:
        pj_assert(!"Invalid candidate type");
        len2 = -1;
        break;
    }
    if (len2 < 1 || len2 >= max_len-len)
        return -1;

    return len+len2;
}


/* Get ice-ufrag and ice-pwd attribute */
static void get_ice_attr(const pjmedia_sdp_session *rem_sdp,
                         const pjmedia_sdp_media *rem_m,
                         const pjmedia_sdp_attr **p_ice_ufrag,
                         const pjmedia_sdp_attr **p_ice_pwd)
{
    pjmedia_sdp_attr *attr;

    /* Find ice-ufrag attribute in media descriptor */
    attr = pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr,
                                 &STR_ICE_UFRAG, NULL);
    if (attr == NULL) {
        /* Find ice-ufrag attribute in session descriptor */
        attr = pjmedia_sdp_attr_find(rem_sdp->attr_count, rem_sdp->attr,
                                     &STR_ICE_UFRAG, NULL);
    }
    *p_ice_ufrag = attr;

    /* Find ice-pwd attribute in media descriptor */
    attr = pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr,
                                 &STR_ICE_PWD, NULL);
    if (attr == NULL) {
        /* Find ice-pwd attribute in session descriptor */
        attr = pjmedia_sdp_attr_find(rem_sdp->attr_count, rem_sdp->attr,
                                     &STR_ICE_PWD, NULL);
    }
    *p_ice_pwd = attr;
}


/* Encode and add "a=ice-mismatch" attribute in the SDP */
static void encode_ice_mismatch(pj_pool_t *sdp_pool,
                                pjmedia_sdp_session *sdp_local,
                                unsigned media_index)
{
    pjmedia_sdp_attr *attr;
    pjmedia_sdp_media *m = sdp_local->media[media_index];

    attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);
    attr->name = STR_ICE_MISMATCH;
    attr->value.slen = 0;
    pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
}


/* Encode ICE information in SDP */
static pj_status_t encode_session_in_sdp(struct transport_ice *tp_ice,
                                         pj_pool_t *sdp_pool,
                                         pjmedia_sdp_session *sdp_local,
                                         unsigned media_index,
                                         unsigned comp_cnt,
                                         pj_bool_t restart_session,
                                         pj_bool_t rtcp_mux,
                                         pj_bool_t trickle)
{
    enum { 
        ATTR_BUF_LEN = 160,     /* Max len of a=candidate attr */
        RATTR_BUF_LEN= 160      /* Max len of a=remote-candidates attr */
    };
    pjmedia_sdp_media *m = sdp_local->media[media_index];
    pj_str_t local_ufrag, local_pwd;
    pjmedia_sdp_attr *attr;
    pj_status_t status;

    /* Must have a session */
    PJ_ASSERT_RETURN(pj_ice_strans_has_sess(tp_ice->ice_st), PJ_EBUG);

    /* Get ufrag and pwd from current session */
    pj_ice_strans_get_ufrag_pwd(tp_ice->ice_st, &local_ufrag, &local_pwd,
                                NULL, NULL);

    /* The listing of candidates depends on whether ICE has completed
     * or not. When ICE has completed:
     *
     * 9.1.2.2: Existing Media Streams with ICE Completed
     *   The agent MUST include a candidate attributes for candidates
     *   matching the default destination for each component of the 
     *   media stream, and MUST NOT include any other candidates.
     *
     * When ICE has not completed, we shall include all candidates.
     *
     * Except when we have detected that remote is offering to restart
     * the session, in this case we will answer with full ICE SDP and
     * new ufrag/pwd pair.
     */
    if (!restart_session && pj_ice_strans_sess_is_complete(tp_ice->ice_st) &&
        pj_ice_strans_get_state(tp_ice->ice_st) != PJ_ICE_STRANS_STATE_FAILED)
    {
        const pj_ice_sess_check *check;
        char *attr_buf;
        pjmedia_sdp_conn *conn;
        pjmedia_sdp_attr *a_rtcp;
        pj_str_t rem_cand;
        unsigned comp;

        /* Encode ice-ufrag attribute */
        attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_UFRAG.ptr,
                                       &local_ufrag);
        pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

        /* Encode ice-pwd attribute */
        attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_PWD.ptr, 
                                       &local_pwd);
        pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

        /* Prepare buffer */
        attr_buf = (char*) pj_pool_alloc(sdp_pool, ATTR_BUF_LEN);
        rem_cand.ptr = (char*) pj_pool_alloc(sdp_pool, RATTR_BUF_LEN);
        rem_cand.slen = 0;

        /* 9.1.2.2: Existing Media Streams with ICE Completed
         *   The default destination for media (i.e., the values of 
         *   the IP addresses and ports in the m and c line used for
         *   that media stream) MUST be the local candidate from the
         *   highest priority nominated pair in the valid list for each
         *   component.
         */
        check = pj_ice_strans_get_valid_pair(tp_ice->ice_st, 1);
        if (check == NULL) {
            pj_assert(!"Shouldn't happen");
            return PJ_EBUG;
        }

        /* Override connection line address and media port number */
        conn = m->conn;
        if (conn == NULL)
            conn = sdp_local->conn;

        conn->addr.ptr = (char*) pj_pool_alloc(sdp_pool, 
                                               PJ_INET6_ADDRSTRLEN);
        pj_sockaddr_print(&check->lcand->addr, conn->addr.ptr, 
                          PJ_INET6_ADDRSTRLEN, 0);
        conn->addr.slen = pj_ansi_strlen(conn->addr.ptr);
        m->desc.port = pj_sockaddr_get_port(&check->lcand->addr);

        /* Override address RTCP attribute if it's present */
        if (comp_cnt == 2 &&
            (check = pj_ice_strans_get_valid_pair(tp_ice->ice_st, 
                                                  COMP_RTCP)) != NULL &&
            (a_rtcp = pjmedia_sdp_attr_find(m->attr_count, m->attr, 
                                            &STR_RTCP, 0)) != NULL) 
        {
            pjmedia_sdp_attr_remove(&m->attr_count, m->attr, a_rtcp);

            a_rtcp = pjmedia_sdp_attr_create_rtcp(sdp_pool, 
                                                  &check->lcand->addr);
            if (a_rtcp)
                pjmedia_sdp_attr_add(&m->attr_count, m->attr, a_rtcp);
        }

        /* Encode only candidates matching the default destination 
         * for each component 
         */
        for (comp=0; comp < comp_cnt; ++comp) {
            int len;
            pj_str_t value;

            /* Get valid pair for this component */
            check = pj_ice_strans_get_valid_pair(tp_ice->ice_st, comp+1);
            if (check == NULL)
                continue;

            /* Print and add local candidate in the pair */
            value.ptr = attr_buf;
            value.slen = print_sdp_cand_attr(attr_buf, ATTR_BUF_LEN, 
                                             check->lcand);
            if (value.slen < 0) {
                pj_assert(!"Not enough attr_buf to print candidate");
                return PJ_EBUG;
            }

            attr = pjmedia_sdp_attr_create(sdp_pool, STR_CANDIDATE.ptr,
                                           &value);
            pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

            /* Append to a=remote-candidates attribute */
            if (pj_ice_strans_get_role(tp_ice->ice_st) == 
                                    PJ_ICE_SESS_ROLE_CONTROLLING) 
            {
                char rem_addr[PJ_INET6_ADDRSTRLEN];

                pj_sockaddr_print(&check->rcand->addr, rem_addr, 
                                  sizeof(rem_addr), 0);
                len = pj_ansi_snprintf(
                           rem_cand.ptr + rem_cand.slen,
                           RATTR_BUF_LEN - rem_cand.slen,
                           "%s%u %s %u", 
                           (rem_cand.slen==0? "" : " "),
                           comp+1, rem_addr,
                           pj_sockaddr_get_port(&check->rcand->addr)
                           );
                if (len < 1 || len >= RATTR_BUF_LEN - rem_cand.slen) {
                    pj_assert(!"Not enough buffer to print "
                               "remote-candidates");
                    return PJ_EBUG;
                }

                rem_cand.slen += len;
            }
        }

        /* 9.1.2.2: Existing Media Streams with ICE Completed
         *   In addition, if the agent is controlling, it MUST include
         *   the a=remote-candidates attribute for each media stream 
         *   whose check list is in the Completed state.  The attribute
         *   contains the remote candidates from the highest priority 
         *   nominated pair in the valid list for each component of that
         *   media stream.
         */
        if (pj_ice_strans_get_role(tp_ice->ice_st) == 
                                    PJ_ICE_SESS_ROLE_CONTROLLING) 
        {
            attr = pjmedia_sdp_attr_create(sdp_pool, STR_REM_CAND.ptr, 
                                           &rem_cand);
            pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
        }

    } else if (pj_ice_strans_has_sess(tp_ice->ice_st) &&
               (restart_session || pj_ice_strans_get_state(tp_ice->ice_st) !=
                PJ_ICE_STRANS_STATE_FAILED))
    {
        /* Encode all candidates to SDP media */
        char *attr_buf;
        unsigned comp;

        /* If ICE is not restarted, encode current ICE ufrag/pwd.
         * Otherwise generate new one.
         */
        if (!restart_session) {
            attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_UFRAG.ptr,
                                           &local_ufrag);
            pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

            attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_PWD.ptr, 
                                           &local_pwd);
            pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

        } else {
            pj_str_t str;

            str.slen = PJ_ICE_UFRAG_LEN;
            str.ptr = (char*) pj_pool_alloc(sdp_pool, str.slen);
            pj_create_random_string(str.ptr, str.slen);
            attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_UFRAG.ptr, &str);
            pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

            str.slen = PJ_ICE_PWD_LEN;
            str.ptr = (char*) pj_pool_alloc(sdp_pool, str.slen);
            pj_create_random_string(str.ptr, str.slen);
            attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_PWD.ptr, &str);
            pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
        }

        /* Create buffer to encode candidates as SDP attribute */
        attr_buf = (char*) pj_pool_alloc(sdp_pool, ATTR_BUF_LEN);

        for (comp=0; comp < comp_cnt; ++comp) {
            unsigned cand_cnt;
            pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
            unsigned i;

            cand_cnt = PJ_ARRAY_SIZE(cand);
            status = pj_ice_strans_enum_cands(tp_ice->ice_st, comp+1,
                                              &cand_cnt, cand);
            if (status != PJ_SUCCESS)
                return status;

            for (i=0; i<cand_cnt; ++i) {
                pj_str_t value;

                value.slen = print_sdp_cand_attr(attr_buf, ATTR_BUF_LEN, 
                                                 &cand[i]);
                if (value.slen < 0) {
                    pj_assert(!"Not enough attr_buf to print candidate");
                    return PJ_EBUG;
                }

                value.ptr = attr_buf;
                attr = pjmedia_sdp_attr_create(sdp_pool, 
                                               STR_CANDIDATE.ptr,
                                               &value);
                pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
            }
        }
    } else {
        /* ICE has failed, application should have terminated this call */
    }

    /* Removing a=rtcp line when there is only one component. */
    if (comp_cnt == 1) {
        attr = pjmedia_sdp_attr_find(m->attr_count, m->attr, &STR_RTCP, NULL);
        if (attr)
            pjmedia_sdp_attr_remove(&m->attr_count, m->attr, attr);
        /* If RTCP is not in use, we MUST send b=RS:0 and b=RR:0. */
        pj_assert(m->bandw_count + 2 <= PJ_ARRAY_SIZE(m->bandw));
        if (m->bandw_count + 2 <= PJ_ARRAY_SIZE(m->bandw)) {
            m->bandw[m->bandw_count] = PJ_POOL_ZALLOC_T(sdp_pool,
                                                        pjmedia_sdp_bandw);
            pj_memcpy(&m->bandw[m->bandw_count]->modifier, &STR_BANDW_RS,
                      sizeof(pj_str_t));
            m->bandw_count++;
            m->bandw[m->bandw_count] = PJ_POOL_ZALLOC_T(sdp_pool,
                                                        pjmedia_sdp_bandw);
            pj_memcpy(&m->bandw[m->bandw_count]->modifier, &STR_BANDW_RR,
                      sizeof(pj_str_t));
            m->bandw_count++;
        }
    }

    /* Add a=rtcp-mux attribute */
    if (rtcp_mux) {
        pjmedia_sdp_attr *add_attr;

        add_attr = PJ_POOL_ZALLOC_T(sdp_pool, pjmedia_sdp_attr);
        add_attr->name = STR_RTCP_MUX;
        m->attr[m->attr_count++] = add_attr;
    }

    /* Add trickle ICE attributes */
    if (trickle) {
        pj_bool_t end_of_cand;

        /* Add media ID attribute "a=mid" */
        attr = pjmedia_sdp_attr_find2(m->attr_count, m->attr, "mid", NULL);
        if (!attr) {
            attr = pjmedia_sdp_attr_create(sdp_pool, "mid", &tp_ice->sdp_mid);
            pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
        }

        end_of_cand = tp_ice->end_of_cand;
        status = pjmedia_ice_trickle_encode_sdp(sdp_pool, sdp_local,
                                                &tp_ice->sdp_mid, NULL, NULL,
                                                0, NULL, end_of_cand);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(3,(tp_ice->base.name, status,
                         "Failed in adding trickle ICE attributes"));
            return status;
        }
    }

    return PJ_SUCCESS;
}


/* Parse a=candidate line */
static pj_status_t parse_cand(const char *obj_name,
                              pj_pool_t *pool,
                              const pj_str_t *orig_input,
                              pj_ice_sess_cand *cand)
{
    pj_str_t token, delim, host;
    int af;
    pj_ssize_t found_idx;
    pj_status_t status = PJNATH_EICEINCANDSDP;

    pj_bzero(cand, sizeof(*cand));

    PJ_UNUSED_ARG(obj_name);

    /* Foundation */
    delim = pj_str(" ");
    found_idx = pj_strtok(orig_input, &delim, &token, 0);
    if (found_idx == orig_input->slen) {
        TRACE__((obj_name, "Expecting ICE foundation in candidate"));
        goto on_return;
    }
    if (pool) {
        pj_strdup(pool, &cand->foundation, &token);
    } else {
        cand->foundation = token;
    }

    /* Component ID */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        TRACE__((obj_name, "Expecting ICE component ID in candidate"));
        goto on_return;
    }
    cand->comp_id = (pj_uint8_t)pj_strtoul(&token);

    /* Transport */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        TRACE__((obj_name, "Expecting ICE transport in candidate"));
        goto on_return;
    }
    if (pj_stricmp2(&token, "UDP") != 0) {
        TRACE__((obj_name, 
                 "Expecting ICE UDP transport only in candidate"));
        goto on_return;
    }

    /* Priority */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        TRACE__((obj_name, "Expecting ICE priority in candidate"));
        goto on_return;
    }
    cand->prio = pj_strtoul(&token);

    /* Host */
    found_idx = pj_strtok(orig_input, &delim, &host, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        TRACE__((obj_name, "Expecting ICE host in candidate"));
        goto on_return;
    }
    /* Detect address family */
    if (pj_strchr(&host, ':'))
        af = pj_AF_INET6();
    else
        af = pj_AF_INET();
    /* Assign address */
    if (pj_sockaddr_init(af, &cand->addr, &host, 0)) {
        TRACE__((obj_name, "Invalid ICE candidate address"));
        goto on_return;
    }

    /* Port */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + host.slen);
    if (found_idx == orig_input->slen) {
        TRACE__((obj_name, "Expecting ICE port number in candidate"));
        goto on_return;
    }
    pj_sockaddr_set_port(&cand->addr, (pj_uint16_t)pj_strtoul(&token));

    /* typ */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        TRACE__((obj_name, "Expecting ICE \"typ\" in candidate"));
        goto on_return;
    }
    if (pj_stricmp2(&token, "typ") != 0) {
        TRACE__((obj_name, "Expecting ICE \"typ\" in candidate"));
        goto on_return;
    }

    /* candidate type */
    found_idx = pj_strtok(orig_input, &delim, &token, found_idx + token.slen);
    if (found_idx == orig_input->slen) {
        TRACE__((obj_name, "Expecting ICE candidate type in candidate"));
        goto on_return;
    }

    if (pj_stricmp2(&token, "host") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_HOST;

    } else if (pj_stricmp2(&token, "srflx") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_SRFLX;

    } else if (pj_stricmp2(&token, "relay") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_RELAYED;

    } else if (pj_stricmp2(&token, "prflx") == 0) {
        cand->type = PJ_ICE_CAND_TYPE_PRFLX;

    } else {
        PJ_LOG(5,(obj_name, "Invalid ICE candidate type %.*s in candidate", 
                  (int)token.slen, token.ptr));
        goto on_return;
    }

    status = PJ_SUCCESS;

on_return:
    return status;
}


/* Create initial SDP offer */
static pj_status_t create_initial_offer(struct transport_ice *tp_ice,
                                        pj_pool_t *sdp_pool,
                                        pjmedia_sdp_session *loc_sdp,
                                        unsigned media_index)
{
    pj_status_t status;

    /* Encode ICE in SDP */
    status = encode_session_in_sdp(tp_ice, sdp_pool, loc_sdp, media_index, 
                                   tp_ice->comp_cnt, PJ_FALSE,
                                   tp_ice->enable_rtcp_mux,
                                   tp_ice->trickle_ice !=
                                        PJ_ICE_SESS_TRICKLE_DISABLED);
    if (status != PJ_SUCCESS) {
        set_no_ice(tp_ice, "Error encoding SDP answer", status);
        return status;
    }

    return PJ_SUCCESS;
}


/* Verify incoming offer */
static pj_status_t verify_ice_sdp(struct transport_ice *tp_ice,
                                  pj_pool_t *tmp_pool,
                                  const pjmedia_sdp_session *rem_sdp,
                                  unsigned media_index,
                                  pj_ice_sess_role current_ice_role,
                                  struct sdp_state *sdp_state)
{
    const pjmedia_sdp_media *rem_m;
    const pjmedia_sdp_attr *ufrag_attr, *pwd_attr;
    const pjmedia_sdp_conn *rem_conn;
    pj_bool_t comp1_found=PJ_FALSE, comp2_found=PJ_FALSE, has_rtcp=PJ_FALSE;
    pj_sockaddr rem_conn_addr, rtcp_addr;
    unsigned i;
    int rem_af = 0;
    pj_status_t status;

    rem_m = rem_sdp->media[media_index];

    /* Check if remote wants RTCP mux */
    if (tp_ice->enable_rtcp_mux) {
        pjmedia_sdp_attr *attr;

        attr = pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr, 
                                     &STR_RTCP_MUX, NULL);
        tp_ice->use_rtcp_mux = (attr? PJ_TRUE: PJ_FALSE);
    }

    /* Get the "ice-ufrag" and "ice-pwd" attributes */
    get_ice_attr(rem_sdp, rem_m, &ufrag_attr, &pwd_attr);

    /* If "ice-ufrag" or "ice-pwd" are not found, disable ICE */
    if (ufrag_attr==NULL || pwd_attr==NULL) {
        sdp_state->match_comp_cnt = 0;
        return PJ_SUCCESS;
    }

    /* Verify that default target for each component matches one of the 
     * candidate for the component. Otherwise stop ICE with ICE ice_mismatch 
     * error.
     */

    /* Component 1 is the c= line */
    rem_conn = rem_m->conn;
    if (rem_conn == NULL)
        rem_conn = rem_sdp->conn;
    if (!rem_conn)
        return PJMEDIA_SDP_EMISSINGCONN;

    /* Verify address family matches */
    /*
    if ((tp_ice->af==pj_AF_INET() && 
         pj_strcmp(&rem_conn->addr_type, &STR_IP4)!=0) ||
        (tp_ice->af==pj_AF_INET6() && 
         pj_strcmp(&rem_conn->addr_type, &STR_IP6)!=0))
    {
        return PJMEDIA_SDP_ETPORTNOTEQUAL;
    }
    */

    /* Get remote address family */
    if (pj_strcmp(&rem_conn->addr_type, &STR_IP4)==0)
        rem_af = pj_AF_INET();
    else if (pj_strcmp(&rem_conn->addr_type, &STR_IP6)==0)
        rem_af = pj_AF_INET6();
    else
        pj_assert(!"Unsupported address family");

    /* Assign remote connection address */
    status = pj_sockaddr_init(rem_af, &rem_conn_addr, &rem_conn->addr,
                              (pj_uint16_t)rem_m->desc.port);
    if (status != PJ_SUCCESS)
        return status;

    if (tp_ice->comp_cnt > 1) {
        const pjmedia_sdp_attr *attr;

        /* Get default RTCP candidate from a=rtcp line, if present, otherwise
         * calculate default RTCP candidate from default RTP target.
         */
        attr = pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr, 
                                     &STR_RTCP, NULL);
        has_rtcp = (attr != NULL);

        if (attr) {
            pjmedia_sdp_rtcp_attr rtcp_attr;

            status = pjmedia_sdp_attr_get_rtcp(attr, &rtcp_attr);
            if (status != PJ_SUCCESS) {
                /* Error parsing a=rtcp attribute */
                return status;
            }
        
            if (rtcp_attr.addr.slen) {
                /* Verify address family matches */
                /*
                if ((tp_ice->af==pj_AF_INET() && 
                     pj_strcmp(&rtcp_attr.addr_type, &STR_IP4)!=0) ||
                    (tp_ice->af==pj_AF_INET6() && 
                     pj_strcmp(&rtcp_attr.addr_type, &STR_IP6)!=0))
                {
                    return PJMEDIA_SDP_ETPORTNOTEQUAL;
                }
                */

                /* Assign RTCP address */
                status = pj_sockaddr_init(rem_af, &rtcp_addr,
                                          &rtcp_attr.addr,
                                          (pj_uint16_t)rtcp_attr.port);
                if (status != PJ_SUCCESS) {
                    return PJMEDIA_SDP_EINRTCP;
                }
            } else {
                /* Assign RTCP address */
                status = pj_sockaddr_init(rem_af, &rtcp_addr, 
                                          NULL, 
                                          (pj_uint16_t)rtcp_attr.port);
                if (status != PJ_SUCCESS) {
                    return PJMEDIA_SDP_EINRTCP;
                }
                pj_sockaddr_copy_addr(&rtcp_addr, &rem_conn_addr);
            }
        } else {
            unsigned rtcp_port;
        
            rtcp_port = pj_sockaddr_get_port(&rem_conn_addr) + 1;
            pj_sockaddr_cp(&rtcp_addr, &rem_conn_addr);
            pj_sockaddr_set_port(&rtcp_addr, (pj_uint16_t)rtcp_port);
        }
    }

    /* Find the default addresses in a=candidate attributes. 
     */
    for (i=0; i<rem_m->attr_count; ++i) {
        pj_ice_sess_cand cand;
        unsigned disable_ice_mismatch = tp_ice->options &
                                        PJMEDIA_ICE_DISABLE_ICE_MISMATCH;

        if (pj_strcmp(&rem_m->attr[i]->name, &STR_CANDIDATE)!=0)
            continue;

        status = parse_cand(tp_ice->base.name, tmp_pool, 
                            &rem_m->attr[i]->value, &cand);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(tp_ice->base.name, status,
                         "Error in parsing SDP candidate attribute '%.*s', "
                         "candidate is ignored",
                         (int)rem_m->attr[i]->value.slen, 
                         rem_m->attr[i]->value.ptr));
            continue;
        }

        if (!comp1_found && cand.comp_id==COMP_RTP)
        {
            if ((disable_ice_mismatch) ||
                (pj_sockaddr_cmp(&rem_conn_addr, &cand.addr) == 0))
            {
                comp1_found = PJ_TRUE;
            }
        } else if (!comp2_found && cand.comp_id==COMP_RTCP)
        {
            if ((disable_ice_mismatch) ||
                (pj_sockaddr_cmp(&rtcp_addr, &cand.addr) == 0))
            {
                comp2_found = PJ_TRUE;
            }
        }

        if (cand.comp_id == COMP_RTCP)
            has_rtcp = PJ_TRUE;

        if (comp1_found && (comp2_found || tp_ice->comp_cnt==1))
            break;
    }

    /* Check matched component count and ice_mismatch */
    if (comp1_found &&
        (tp_ice->comp_cnt==1 || !has_rtcp || tp_ice->use_rtcp_mux))
    {
        sdp_state->match_comp_cnt = 1;
        sdp_state->ice_mismatch = PJ_FALSE;
    } else if (comp1_found && comp2_found) {
        sdp_state->match_comp_cnt = 2;
        sdp_state->ice_mismatch = PJ_FALSE;
    } else {
        sdp_state->match_comp_cnt = (tp_ice->comp_cnt > 1 && has_rtcp)? 2 : 1;
        sdp_state->ice_mismatch = PJ_TRUE;
    }


    /* Detect remote restarting session */
    if (pj_ice_strans_has_sess(tp_ice->ice_st) &&
        (pj_ice_strans_sess_is_running(tp_ice->ice_st) ||
         pj_ice_strans_sess_is_complete(tp_ice->ice_st))) 
    {
        pj_str_t rem_run_ufrag, rem_run_pwd;
        pj_ice_strans_get_ufrag_pwd(tp_ice->ice_st, NULL, NULL,
                                    &rem_run_ufrag, &rem_run_pwd);
        if (pj_strcmp(&ufrag_attr->value, &rem_run_ufrag) ||
            pj_strcmp(&pwd_attr->value, &rem_run_pwd))
        {
            /* Remote offers to restart ICE */
            sdp_state->ice_restart = PJ_TRUE;
        } else {
            sdp_state->ice_restart = PJ_FALSE;
        }
    } else {
        sdp_state->ice_restart = PJ_FALSE;
    }

    /* Detect our role */
    if (pjmedia_sdp_attr_find(rem_sdp->attr_count, rem_sdp->attr,
                              &STR_ICE_LITE, NULL) != NULL)
    {
        /* Remote is ICE lite, set our role as controlling */
        sdp_state->local_role = PJ_ICE_SESS_ROLE_CONTROLLING;
    } else {
        if (current_ice_role==PJ_ICE_SESS_ROLE_CONTROLLING) {
            sdp_state->local_role = PJ_ICE_SESS_ROLE_CONTROLLING;
        } else {
            sdp_state->local_role = PJ_ICE_SESS_ROLE_CONTROLLED;
        }
    }

    /* Check trickle ICE indication */
    if (tp_ice->trickle_ice != PJ_ICE_SESS_TRICKLE_DISABLED) {
        sdp_state->has_trickle = pjmedia_ice_sdp_has_trickle(rem_sdp,
                                                             media_index);

        /* Reset ICE mismatch flag if conn addr is default address */
        if (sdp_state->ice_mismatch && sdp_state->has_trickle) {
            pj_sockaddr def_addr;
            pj_sockaddr_init(rem_af, &def_addr, NULL, 9);
            if (pj_sockaddr_cmp(&rem_conn_addr, &def_addr)==0)
                sdp_state->ice_mismatch = PJ_FALSE;
        }
    } else {
        sdp_state->has_trickle = PJ_FALSE;
    }

    PJ_LOG(4,(tp_ice->base.name, 
              "Processing SDP: support ICE=%u, common comp_cnt=%u, "
              "ice_mismatch=%u, ice_restart=%u, local_role=%s, trickle=%u",
              (sdp_state->match_comp_cnt != 0), 
              sdp_state->match_comp_cnt, 
              sdp_state->ice_mismatch, 
              sdp_state->ice_restart,
              pj_ice_sess_role_name(sdp_state->local_role),
              sdp_state->has_trickle));

    return PJ_SUCCESS;

}

/* Encode information in SDP if ICE is not used */
static pj_status_t encode_no_ice_in_sdp( struct transport_ice *tp_ice,
                                         pj_pool_t *pool,
                                         pjmedia_sdp_session *sdp_local,
                                         const pjmedia_sdp_session *rem_sdp,
                                         unsigned media_index)
{
    if (tp_ice->enable_rtcp_mux) {
        pjmedia_sdp_media *m = sdp_local->media[media_index];
        pjmedia_sdp_attr *attr;
        pj_bool_t add_rtcp_mux = PJ_TRUE;

        if (rem_sdp)
            add_rtcp_mux = tp_ice->use_rtcp_mux;
        else {
            /* For subsequent offer, set it to false first since
             * we are still waiting for remote answer.
             */
            tp_ice->use_rtcp_mux = PJ_FALSE;
        }

        /* Remove RTCP attribute because for subsequent offers/answers,
         * the address (obtained from transport_get_info() ) may be
         * incorrect if we are not yet confirmed to use RTCP mux
         * (because we are still waiting for remote answer) or
         * if remote rejects it.
         */
        pjmedia_sdp_attr_remove_all(&m->attr_count, m->attr, "rtcp");
        
        if (!tp_ice->use_rtcp_mux && tp_ice->comp_cnt > 1) {
            pj_ice_sess_cand cand;
            pj_status_t status;
            
            status = pj_ice_strans_get_def_cand(tp_ice->ice_st, 2, &cand);
            if (status == PJ_SUCCESS) {
                /* Add RTCP attribute if the remote doesn't offer or
                 * rejects it.
                 */
                attr = pjmedia_sdp_attr_create_rtcp(pool, &cand.addr);  
                if (attr)
                    pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
            }
        }

        /* Add a=rtcp-mux attribute. */
        if (add_rtcp_mux) {
            attr = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_attr);
            attr->name = STR_RTCP_MUX;
            m->attr[m->attr_count++] = attr;
        }
    }
    return PJ_SUCCESS;
}


/* Verify incoming offer and create initial answer */
static pj_status_t create_initial_answer(struct transport_ice *tp_ice,
                                         pj_pool_t *sdp_pool,
                                         pjmedia_sdp_session *loc_sdp,
                                         const pjmedia_sdp_session *rem_sdp,
                                         unsigned media_index)
{
    const pjmedia_sdp_media *rem_m = rem_sdp->media[media_index];
    pj_bool_t with_trickle;
    pj_status_t status;

    /* Check if media is removed (just in case) */
    if (rem_m->desc.port == 0) {
        return PJ_SUCCESS;
    }

    /* Verify the offer */
    status = verify_ice_sdp(tp_ice, sdp_pool, rem_sdp, media_index, 
                            PJ_ICE_SESS_ROLE_CONTROLLED, 
                            &tp_ice->rem_offer_state);
    if (status != PJ_SUCCESS) {
        set_no_ice(tp_ice, "Invalid SDP offer", status);
        return status;
    }

    /* Does remote support ICE? */
    if (tp_ice->rem_offer_state.match_comp_cnt==0) {
        set_no_ice(tp_ice, "No ICE found in SDP offer", PJ_SUCCESS);
        encode_no_ice_in_sdp(tp_ice, sdp_pool, loc_sdp, rem_sdp,
                             media_index);
        return PJ_SUCCESS;
    }

    /* ICE ice_mismatch? */
    if (tp_ice->rem_offer_state.ice_mismatch) {
        set_no_ice(tp_ice, "ICE ice_mismatch in remote offer", PJ_SUCCESS);
        encode_ice_mismatch(sdp_pool, loc_sdp, media_index);
        return PJ_SUCCESS;
    }

    /* Encode ICE in SDP */
    with_trickle = tp_ice->rem_offer_state.has_trickle &&
                   tp_ice->trickle_ice != PJ_ICE_SESS_TRICKLE_DISABLED;
    status = encode_session_in_sdp(tp_ice, sdp_pool, loc_sdp, media_index, 
                                   tp_ice->rem_offer_state.match_comp_cnt,
                                   PJ_FALSE, tp_ice->use_rtcp_mux,
                                   with_trickle);
    if (status != PJ_SUCCESS) {
        set_no_ice(tp_ice, "Error encoding SDP answer", status);
        return status;
    }

    return PJ_SUCCESS;
}


/* Create subsequent SDP offer */
static pj_status_t create_subsequent_offer(struct transport_ice *tp_ice,
                                           pj_pool_t *sdp_pool,
                                           pjmedia_sdp_session *loc_sdp,
                                           unsigned media_index)
{
    unsigned comp_cnt;

    if (pj_ice_strans_has_sess(tp_ice->ice_st) == PJ_FALSE) {
        /* We don't have ICE */
        encode_no_ice_in_sdp(tp_ice, sdp_pool, loc_sdp, NULL, media_index);
        return PJ_SUCCESS;
    }

    comp_cnt = pj_ice_strans_get_running_comp_cnt(tp_ice->ice_st);
    return encode_session_in_sdp(tp_ice, sdp_pool, loc_sdp, media_index,
                                 comp_cnt, PJ_FALSE, tp_ice->enable_rtcp_mux,
                                 PJ_FALSE);
}


/* Create subsequent SDP answer */
static pj_status_t create_subsequent_answer(struct transport_ice *tp_ice,
                                            pj_pool_t *sdp_pool,
                                            pjmedia_sdp_session *loc_sdp,
                                            const pjmedia_sdp_session *rem_sdp,
                                            unsigned media_index)
{
    pj_status_t status;

    /* We have a session */
    status = verify_ice_sdp(tp_ice, sdp_pool, rem_sdp, media_index, 
                            PJ_ICE_SESS_ROLE_CONTROLLED, 
                            &tp_ice->rem_offer_state);
    if (status != PJ_SUCCESS) {
        /* Something wrong with the offer */
        return status;
    }

    if (pj_ice_strans_has_sess(tp_ice->ice_st)) {
        /*
         * Received subsequent offer while we have ICE active.
         */

        if (tp_ice->rem_offer_state.match_comp_cnt == 0) {
            /* Remote no longer offers ICE */
            encode_no_ice_in_sdp(tp_ice, sdp_pool, loc_sdp, rem_sdp,
                                 media_index);
            return PJ_SUCCESS;
        }

        if (tp_ice->rem_offer_state.ice_mismatch) {
            encode_ice_mismatch(sdp_pool, loc_sdp, media_index);
            return PJ_SUCCESS;
        }

        status = encode_session_in_sdp(tp_ice, sdp_pool, loc_sdp, media_index,
                                       tp_ice->rem_offer_state.match_comp_cnt,
                                       tp_ice->rem_offer_state.ice_restart,
                                       tp_ice->use_rtcp_mux, PJ_FALSE);
        if (status != PJ_SUCCESS)
            return status;

        /* Done */

    } else {
        pj_bool_t with_trickle;

        /*
         * Received subsequent offer while we DON'T have ICE active.
         */

        if (tp_ice->rem_offer_state.match_comp_cnt == 0) {
            /* Remote does not support ICE */
            encode_no_ice_in_sdp(tp_ice, sdp_pool, loc_sdp, rem_sdp,
                                 media_index);
            return PJ_SUCCESS;
        }

        if (tp_ice->rem_offer_state.ice_mismatch) {
            encode_ice_mismatch(sdp_pool, loc_sdp, media_index);
            return PJ_SUCCESS;
        }

        /* Looks like now remote is offering ICE, so we need to create
         * ICE session now.
         */
        status = pj_ice_strans_init_ice(tp_ice->ice_st, 
                                        PJ_ICE_SESS_ROLE_CONTROLLED,
                                        NULL, NULL);
        if (status != PJ_SUCCESS) {
            /* Fail to create new ICE session */
            return status;
        }

        with_trickle = tp_ice->rem_offer_state.has_trickle &&
                       tp_ice->trickle_ice != PJ_ICE_SESS_TRICKLE_DISABLED;
        status = encode_session_in_sdp(tp_ice, sdp_pool, loc_sdp, media_index,
                                       tp_ice->rem_offer_state.match_comp_cnt,
                                       tp_ice->rem_offer_state.ice_restart,
                                       tp_ice->use_rtcp_mux,
                                       with_trickle);
        if (status != PJ_SUCCESS)
            return status;

        /* Done */
    }

    return PJ_SUCCESS;
}


/*
 * For both UAC and UAS, pass in the SDP before sending it to remote.
 * This will add ICE attributes to the SDP.
 */
static pj_status_t transport_media_create(pjmedia_transport *tp,
                                          pj_pool_t *sdp_pool,
                                          unsigned options,
                                          const pjmedia_sdp_session *rem_sdp,
                                          unsigned media_index)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_ice_sess_role ice_role;
    pj_status_t status;

    PJ_UNUSED_ARG(media_index);
    PJ_UNUSED_ARG(sdp_pool);

    tp_ice->media_option = options;
    tp_ice->enable_rtcp_mux = ((options & PJMEDIA_TPMED_RTCP_MUX) != 0);
    tp_ice->oa_role = ROLE_NONE;
    tp_ice->initial_sdp = PJ_TRUE;

    /* Init SDP "a=mid" attribute. Get from remote SDP for answerer or
     * generate one for offerer (or if remote doesn't specify).
     */
    tp_ice->sdp_mid.slen = 0;
    if (rem_sdp) {
        pjmedia_sdp_media *m = rem_sdp->media[media_index];
        pjmedia_sdp_attr *a;
        a = pjmedia_sdp_attr_find2(m->attr_count, m->attr, "mid", NULL);
        if (a) {
            pj_strdup(tp_ice->pool, &tp_ice->sdp_mid, &a->value);
        }
    }
    if (tp_ice->sdp_mid.slen == 0) {
        char tmp_buf[8];
        pj_ansi_snprintf(tmp_buf, sizeof(tmp_buf), "%d", media_index+1);
        tp_ice->sdp_mid = pj_strdup3(tp_ice->pool, tmp_buf);
    }

    /* If RTCP mux is being used, set component count to 1 */
    if (rem_sdp && tp_ice->enable_rtcp_mux) {
        pjmedia_sdp_media *rem_m = rem_sdp->media[media_index];
        pjmedia_sdp_attr *attr;
        attr = pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr,
                                     &STR_RTCP_MUX, NULL);
        tp_ice->use_rtcp_mux = (attr? PJ_TRUE: PJ_FALSE);
    }
    if (tp_ice->use_rtcp_mux &&
        pj_ice_strans_get_running_comp_cnt(tp_ice->ice_st)>1)
    {
        pj_ice_strans_update_comp_cnt(tp_ice->ice_st, 1);
    }

    /* Init ICE, the initial role is set now based on availability of
     * rem_sdp, but it will be checked again later.
     */
    ice_role = (rem_sdp==NULL ? PJ_ICE_SESS_ROLE_CONTROLLING : 
                                PJ_ICE_SESS_ROLE_CONTROLLED);
    status = pj_ice_strans_init_ice(tp_ice->ice_st, ice_role, NULL, NULL);

    /* For trickle ICE, if remote SDP has been received, process any remote
     * ICE info now (ICE user fragment and/or initial ICE candidate list).
     */
    if (rem_sdp && status == PJ_SUCCESS) {
        if (tp_ice->trickle_ice != PJ_ICE_SESS_TRICKLE_DISABLED &&
            pjmedia_ice_sdp_has_trickle(rem_sdp, media_index))
        {
            pj_str_t ufrag, pwd;
            unsigned cand_cnt = PJ_ICE_ST_MAX_CAND;
            pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
            pj_bool_t end_of_cand;

            status = pjmedia_ice_trickle_decode_sdp(rem_sdp, media_index,
                                                    NULL, &ufrag, &pwd,
                                                    &cand_cnt, cand,
                                                    &end_of_cand);
            if (status == PJ_SUCCESS)
                status = pj_ice_strans_update_check_list(
                                            tp_ice->ice_st, &ufrag, &pwd,
                                            cand_cnt, cand, end_of_cand);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(1,(tp_ice->base.name, status,
                             "Failed create checklist for trickling ICE"));
                return status;
            }
        }
    }

    /* Done */
    return status;
}


static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
                                        pj_pool_t *sdp_pool,
                                        pjmedia_sdp_session *sdp_local,
                                        const pjmedia_sdp_session *rem_sdp,
                                        unsigned media_index)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_status_t status;

    /* Validate media transport */
    /* This transport only support RTP/AVP transport, unless if
     * transport checking is disabled
     */
    if ((tp_ice->media_option & PJMEDIA_TPMED_NO_TRANSPORT_CHECKING) == 0) {
        pjmedia_sdp_media *m_rem, *m_loc;
        pj_uint32_t tp_proto_loc, tp_proto_rem;

        m_rem = rem_sdp? rem_sdp->media[media_index] : NULL;
        m_loc = sdp_local->media[media_index];

        tp_proto_loc = pjmedia_sdp_transport_get_proto(&m_loc->desc.transport);
        tp_proto_rem = m_rem? 
                pjmedia_sdp_transport_get_proto(&m_rem->desc.transport) : 0;
        PJMEDIA_TP_PROTO_TRIM_FLAG(tp_proto_loc, PJMEDIA_TP_PROFILE_RTCP_FB);
        PJMEDIA_TP_PROTO_TRIM_FLAG(tp_proto_rem, PJMEDIA_TP_PROFILE_RTCP_FB);

        if ((tp_proto_loc != PJMEDIA_TP_PROTO_RTP_AVP) ||
            (m_rem && tp_proto_rem != PJMEDIA_TP_PROTO_RTP_AVP))
        {
            pjmedia_sdp_media_deactivate(sdp_pool, m_loc);
            return PJMEDIA_SDP_EINPROTO;
        }
    }

    if (tp_ice->initial_sdp) {
        if (rem_sdp) {
            status = create_initial_answer(tp_ice, sdp_pool, sdp_local, 
                                           rem_sdp, media_index);
        } else {
            status = create_initial_offer(tp_ice, sdp_pool, sdp_local,
                                          media_index);
        }
    } else {
        if (rem_sdp) {
            status = create_subsequent_answer(tp_ice, sdp_pool, sdp_local,
                                              rem_sdp, media_index);
        } else {
            status = create_subsequent_offer(tp_ice, sdp_pool, sdp_local,
                                             media_index);
        }
    }

    if (status==PJ_SUCCESS) {
        if (rem_sdp)
            tp_ice->oa_role = ROLE_ANSWERER;
        else
            tp_ice->oa_role = ROLE_OFFERER;

        if (tp_ice->use_ice) {
            /* Update last local candidate count, so trickle ICE can identify
             * if there is any new local candidate.
             */
            unsigned i, comp_cnt;
            comp_cnt = pj_ice_strans_get_running_comp_cnt(tp_ice->ice_st);
            for (i = 0; i < comp_cnt; ++i) {
                tp_ice->last_send_cand_cnt[i] =
                        pj_ice_strans_get_cands_count(tp_ice->ice_st, i+1);
            }
        }
    }

    return status;
}


/* Start ICE session with the specified remote SDP */
static pj_status_t start_ice(struct transport_ice *tp_ice,
                             pj_pool_t *tmp_pool,
                             const pjmedia_sdp_session *rem_sdp,
                             unsigned media_index)
{
    pjmedia_sdp_media *rem_m = rem_sdp->media[media_index];
    const pjmedia_sdp_attr *ufrag_attr, *pwd_attr;
    pj_ice_sess_cand *cand;
    unsigned i, cand_cnt;
    pj_status_t status;

    get_ice_attr(rem_sdp, rem_m, &ufrag_attr, &pwd_attr);

    /* Allocate candidate array */
    cand = (pj_ice_sess_cand*)
           pj_pool_calloc(tmp_pool, PJ_ICE_MAX_CAND, 
                          sizeof(pj_ice_sess_cand));

    /* Get all candidates in the media */
    cand_cnt = 0;
    for (i=0; i<rem_m->attr_count && cand_cnt < PJ_ICE_MAX_CAND; ++i) {
        pjmedia_sdp_attr *attr;

        attr = rem_m->attr[i];

        if (pj_strcmp(&attr->name, &STR_CANDIDATE)!=0)
            continue;

        /* Parse candidate */
        status = parse_cand(tp_ice->base.name, tmp_pool, &attr->value, 
                            &cand[cand_cnt]);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(tp_ice->base.name, status,
                         "Error in parsing SDP candidate attribute '%.*s', "
                         "candidate is ignored",
                         (int)attr->value.slen, attr->value.ptr));
            continue;
        }

        if (!tp_ice->use_rtcp_mux || cand[cand_cnt].comp_id < 2)
            cand_cnt++;
    }

    /* Start ICE */
    return pj_ice_strans_start_ice(tp_ice->ice_st, &ufrag_attr->value, 
                                   &pwd_attr->value, cand_cnt, cand);
}


/*
 * Start ICE checks when both offer and answer have been negotiated
 * by SDP negotiator.
 */
static pj_status_t transport_media_start(pjmedia_transport *tp,
                                         pj_pool_t *tmp_pool,
                                         const pjmedia_sdp_session *sdp_local,
                                         const pjmedia_sdp_session *rem_sdp,
                                         unsigned media_index)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pjmedia_sdp_media *rem_m;
    enum oa_role current_oa_role;
    pj_bool_t initial_oa;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp && tmp_pool && rem_sdp, PJ_EINVAL);
    PJ_ASSERT_RETURN(media_index < rem_sdp->media_count, PJ_EINVAL);

    rem_m = rem_sdp->media[media_index];

    initial_oa = tp_ice->initial_sdp;
    current_oa_role = tp_ice->oa_role;

    /* SDP has been negotiated */
    tp_ice->initial_sdp = PJ_FALSE;
    tp_ice->oa_role = ROLE_NONE;

    /* Nothing to do if we don't have ICE session */
    if (pj_ice_strans_has_sess(tp_ice->ice_st) == PJ_FALSE) {
        return PJ_SUCCESS;
    }

    /* Special case for Session Timer. The re-INVITE for session refresh
     * doesn't call transport_encode_sdp(), causing current_oa_role to
     * be set to ROLE_NONE. This is a workaround.
     */
    if (current_oa_role == ROLE_NONE) {
        current_oa_role = ROLE_OFFERER;
    }

    /* Processing depends on the offer/answer role */
    if (current_oa_role == ROLE_OFFERER) {
        /*
         * We are offerer. So this will be the first time we see the
         * remote's SDP.
         */
        struct sdp_state answer_state;

        /* Verify the answer */
        status = verify_ice_sdp(tp_ice, tmp_pool, rem_sdp, media_index, 
                                PJ_ICE_SESS_ROLE_CONTROLLING, &answer_state);
        if (status != PJ_SUCCESS) {
            /* Something wrong in the SDP answer */
            set_no_ice(tp_ice, "Invalid remote SDP answer", status);
            return status;
        }

        /* Does it have ICE? */
        if (answer_state.match_comp_cnt == 0) {
            /* Remote doesn't support ICE */
            set_no_ice(tp_ice, "Remote answer doesn't support ICE", 
                       PJ_SUCCESS);
            return PJ_SUCCESS;
        }

        /* Check if remote has reported ice-mismatch */
        if (pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr, 
                                  &STR_ICE_MISMATCH, NULL) != NULL)
        {
            /* Remote has reported ice-mismatch */
            set_no_ice(tp_ice, 
                       "Remote answer contains 'ice-mismatch' attribute", 
                       PJ_SUCCESS);
            return PJ_SUCCESS;
        }

        /* Check if remote has indicated a restart */
        if (answer_state.ice_restart) {
            PJ_LOG(2,(tp_ice->base.name, 
                      "Warning: remote has signalled ICE restart in SDP "
                      "answer which is disallowed. Remote ICE negotiation"
                      " may fail."));
        }

        /* Check if the answer itself is mismatched */
        if (answer_state.ice_mismatch) {
            /* This happens either when a B2BUA modified remote answer but
             * strangely didn't modify our offer, or remote is not capable
             * of detecting mismatch in our offer (it didn't put 
             * 'ice-mismatch' attribute in the answer).
             */
            PJ_LOG(2,(tp_ice->base.name, 
                      "Warning: remote answer mismatch, but it does not "
                      "reject our offer with 'ice-mismatch'. ICE negotiation "
                      "may fail"));
        }

        /* Do nothing if ICE is complete or running */
        if (pj_ice_strans_sess_is_running(tp_ice->ice_st)) {
            PJ_LOG(4,(tp_ice->base.name,
                      "Ignored offer/answer because ICE is running"));
            return PJ_SUCCESS;
        }

        if (pj_ice_strans_sess_is_complete(tp_ice->ice_st)) {
            PJ_LOG(4,(tp_ice->base.name, "ICE session unchanged"));
            return PJ_SUCCESS;
        }

        /* Start ICE */

    } else {
        /*
         * We are answerer. We've seen and negotiated remote's SDP
         * before, and the result is in "rem_offer_state".
         */
        const pjmedia_sdp_attr *ufrag_attr, *pwd_attr;

        /* Check for ICE in remote offer */
        if (tp_ice->rem_offer_state.match_comp_cnt == 0) {
            /* No ICE attribute present */
            set_no_ice(tp_ice, "Remote no longer offers ICE",
                       PJ_SUCCESS);
            return PJ_SUCCESS;
        }

        /* Check for ICE ice_mismatch condition in the offer */
        if (tp_ice->rem_offer_state.ice_mismatch) {
            set_no_ice(tp_ice, "Remote offer mismatch: ", 
                       PJNATH_EICEMISMATCH);
            return PJ_SUCCESS;
        }

        /* If ICE is complete and remote doesn't request restart,
         * then leave the session as is.
         */
        if (!initial_oa && tp_ice->rem_offer_state.ice_restart == PJ_FALSE) {
            /* Remote has not requested ICE restart, so session is
             * unchanged.
             */
            PJ_LOG(4,(tp_ice->base.name, "ICE session unchanged"));
            return PJ_SUCCESS;
        }

        /* Either remote has requested ICE restart or this is our
         * first answer. 
         */

        /* Stop ICE */
        if (!initial_oa) {
            set_no_ice(tp_ice, "restarting by remote request..", PJ_SUCCESS);

            /* We have put new ICE ufrag and pwd in the answer. Now
             * create a new ICE session with that ufrag/pwd pair.
             */
            get_ice_attr(sdp_local, sdp_local->media[media_index], 
                         &ufrag_attr, &pwd_attr);
            status = pj_ice_strans_init_ice(tp_ice->ice_st, 
                                            tp_ice->rem_offer_state.local_role,
                                            &ufrag_attr->value, 
                                            &pwd_attr->value);
            if (status != PJ_SUCCESS) {
                PJ_PERROR(1,(tp_ice->base.name, status,
                             "ICE re-initialization failed!"));
                return status;
            }
        }

        /* Ticket #977: Update role if turns out we're supposed to be the 
         * Controlling agent (e.g. when talking to ice-lite peer). 
         */
        if (tp_ice->rem_offer_state.local_role==PJ_ICE_SESS_ROLE_CONTROLLING &&
            pj_ice_strans_has_sess(tp_ice->ice_st)) 
        {
            pj_ice_strans_change_role(tp_ice->ice_st, 
                                      PJ_ICE_SESS_ROLE_CONTROLLING);
        }

        /* start ICE */
    }

    /* RFC 5245 section 8.1.1:
     * If its peer has a lite implementation, an agent MUST use
     * a regular nomination algorithm.
     */
    if (pjmedia_sdp_attr_find(rem_sdp->attr_count, rem_sdp->attr,
                              &STR_ICE_LITE, NULL) != NULL)
    {
        pj_ice_sess_options opt;
        pj_ice_strans_get_options(tp_ice->ice_st, &opt);
        if (opt.aggressive) {
            opt.aggressive = PJ_FALSE;
            pj_ice_strans_set_options(tp_ice->ice_st, &opt);
            PJ_LOG(4,(tp_ice->base.name, "Forcefully set ICE to use regular "
                      "nomination as remote is lite implementation"));
        }
    }

    /* Now start ICE, if not yet (trickle ICE may have started it earlier) */
    if (!pj_ice_strans_sess_is_running(tp_ice->ice_st) &&
        !pj_ice_strans_sess_is_complete(tp_ice->ice_st))
    {
        status = start_ice(tp_ice, tmp_pool, rem_sdp, media_index);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(1,(tp_ice->base.name, status, "ICE restart failed!"));
            return status;
        }
    }

    /* Done */
    tp_ice->use_ice = PJ_TRUE;

    return PJ_SUCCESS;
}


static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    
    set_no_ice(tp_ice, "media stop requested", PJ_SUCCESS);

    return PJ_SUCCESS;
}


static pj_status_t transport_get_info(pjmedia_transport *tp,
                                      pjmedia_transport_info *info)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_ice_sess_cand cands[PJ_ICE_ST_MAX_CAND];
    pj_ice_sess_cand cand;
    pj_sockaddr_t *addr;
    pj_status_t status;

    pj_bzero(&info->sock_info, sizeof(info->sock_info));
    info->sock_info.rtp_sock = info->sock_info.rtcp_sock = PJ_INVALID_SOCKET;

    /* Get RTP default address */
    status = pj_ice_strans_get_def_cand(tp_ice->ice_st, 1, &cand);
    if (status != PJ_SUCCESS)
        return status;

    /* Address of the default candidate may not be available (e.g:
     * STUN/TURN address is still being resolved), so let's find address
     * of any other candidate, if still not available, the draft RFC
     * seems to allow us using "0.0.0.0:9" in SDP.
     */
    addr = NULL;
    if (pj_sockaddr_has_addr(&cand.addr)) {
        addr = &cand.addr;
    } else if (pj_ice_strans_has_sess(tp_ice->ice_st)) {
        unsigned i, cnt = PJ_ICE_ST_MAX_CAND;
        pj_ice_strans_enum_cands(tp_ice->ice_st, 1, &cnt, cands);
        for (i = 0; i < cnt && !addr; ++i) {
            if (pj_sockaddr_has_addr(&cands[i].addr))
                addr = &cands[i].addr;
        }
    }
    if (addr) {
        pj_sockaddr_cp(&info->sock_info.rtp_addr_name, addr);
    } else {
        pj_sockaddr_init(PJ_AF_INET, &info->sock_info.rtp_addr_name, NULL, 9);
    }

    /* Get RTCP default address */
    if (tp_ice->use_rtcp_mux) {
        pj_sockaddr_cp(&info->sock_info.rtcp_addr_name,
                       &info->sock_info.rtp_addr_name);
    } else if (tp_ice->comp_cnt > 1) {
        status = pj_ice_strans_get_def_cand(tp_ice->ice_st, 2, &cand);
        if (status != PJ_SUCCESS)
            return status;

        /* Address of the default candidate may not be available (e.g:
         * STUN/TURN address is still being resolved), so let's find address
         * of any other candidate. If none is available, SDP must not include
         * "a=rtcp" attribute.
         */
        addr = NULL;
        if (pj_sockaddr_has_addr(&cand.addr)) {
            addr = &cand.addr;
        } else if (pj_ice_strans_has_sess(tp_ice->ice_st)) {
            unsigned i, cnt = PJ_ICE_ST_MAX_CAND;
            pj_ice_strans_enum_cands(tp_ice->ice_st, 2, &cnt, cands);
            for (i = 0; i < cnt && !addr; ++i) {
                if (pj_sockaddr_has_addr(&cands[i].addr))
                    addr = &cands[i].addr;
            }
        }
        if (addr) {
            pj_sockaddr_cp(&info->sock_info.rtcp_addr_name, addr);
        }
    }

    /* Set remote address originating RTP & RTCP if this transport has 
     * ICE activated or received any packets.
     */
    if (tp_ice->use_ice || tp_ice->rtp_src_cnt) {
        info->src_rtp_name = tp_ice->rtp_src_addr;
        if (tp_ice->use_rtcp_mux)
            info->src_rtcp_name = tp_ice->rtp_src_addr;
    }
    if ((!tp_ice->use_rtcp_mux) &&
        (tp_ice->use_ice || tp_ice->rtcp_src_cnt))
    {
        info->src_rtcp_name = tp_ice->rtcp_src_addr;
    }

    /* Fill up transport specific info */
    if (info->specific_info_cnt < PJ_ARRAY_SIZE(info->spc_info)) {
        pjmedia_transport_specific_info *tsi;
        pjmedia_ice_transport_info *ii;
        unsigned i;

        pj_assert(sizeof(*ii) <= sizeof(tsi->buffer));
        tsi = &info->spc_info[info->specific_info_cnt++];
        tsi->type = PJMEDIA_TRANSPORT_TYPE_ICE;
        tsi->tp = tp;
        tsi->cbsize = sizeof(*ii);

        ii = (pjmedia_ice_transport_info*) tsi->buffer;
        pj_bzero(ii, sizeof(*ii));

        ii->active = tp_ice->use_ice;

        if (pj_ice_strans_has_sess(tp_ice->ice_st)) {
            ii->role = pj_ice_strans_get_role(tp_ice->ice_st);
            pj_ice_strans_get_ufrag_pwd(tp_ice->ice_st, &ii->loc_ufrag, NULL,
                                        &ii->rem_ufrag, NULL);
        } else {
            ii->role = PJ_ICE_SESS_ROLE_UNKNOWN;
        }
        ii->sess_state = pj_ice_strans_get_state(tp_ice->ice_st);
        ii->comp_cnt = pj_ice_strans_get_running_comp_cnt(tp_ice->ice_st);
        
        for (i=1; i<=ii->comp_cnt && i<=PJ_ARRAY_SIZE(ii->comp); ++i) {
            const pj_ice_sess_check *chk;

            chk = pj_ice_strans_get_valid_pair(tp_ice->ice_st, i);
            if (chk) {
                ii->comp[i-1].lcand_type = chk->lcand->type;
                pj_sockaddr_cp(&ii->comp[i-1].lcand_addr,
                               &chk->lcand->addr);
                ii->comp[i-1].rcand_type = chk->rcand->type;
                pj_sockaddr_cp(&ii->comp[i-1].rcand_addr,
                               &chk->rcand->addr);
            }
        }
    }

    return PJ_SUCCESS;
}


static pj_status_t transport_attach  (pjmedia_transport *tp,
                                      void *stream,
                                      const pj_sockaddr_t *rem_addr,
                                      const pj_sockaddr_t *rem_rtcp,
                                      unsigned addr_len,
                                      void (*rtp_cb)(void*,
                                                     void*,
                                                     pj_ssize_t),
                                      void (*rtcp_cb)(void*,
                                                      void*,
                                                      pj_ssize_t))
{
    pjmedia_transport_attach_param param;
    
    pj_bzero(&param, sizeof(param));
    param.user_data = stream;
    pj_sockaddr_cp(&param.rem_addr, rem_addr);
    pj_sockaddr_cp(&param.rem_rtcp, rem_rtcp);
    param.addr_len = addr_len;
    param.rtp_cb = rtp_cb;
    param.rtcp_cb = rtcp_cb;
    return transport_attach2(tp, &param);
}


static pj_status_t transport_attach2  (pjmedia_transport *tp,
                                       pjmedia_transport_attach_param
                                           *att_param)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    tp_ice->stream = att_param->user_data;
    tp_ice->rtp_cb = att_param->rtp_cb;
    tp_ice->rtp_cb2 = att_param->rtp_cb2;
    tp_ice->rtcp_cb = att_param->rtcp_cb;

    /* Check again if we are multiplexing RTP & RTCP. */
    tp_ice->use_rtcp_mux = (pj_sockaddr_has_addr(&att_param->rem_addr) &&
                            pj_sockaddr_cmp(&att_param->rem_addr,
                                            &att_param->rem_rtcp) == 0);

    pj_memcpy(&tp_ice->remote_rtp, &att_param->rem_addr, att_param->addr_len);
    pj_memcpy(&tp_ice->remote_rtcp, &att_param->rem_rtcp, att_param->addr_len);
    tp_ice->addr_len = att_param->addr_len;

    /* Init source RTP & RTCP addresses and counter */
    tp_ice->rtp_src_addr = tp_ice->remote_rtp;
    pj_bzero(&tp_ice->rtcp_src_addr, sizeof(tp_ice->rtcp_src_addr));
    tp_ice->rtp_src_cnt = tp_ice->rtcp_src_cnt = 0;

    return PJ_SUCCESS;
}


static void transport_detach(pjmedia_transport *tp,
                             void *strm)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    /* TODO: need to solve ticket #460 here */

    tp_ice->rtp_cb = NULL;
    tp_ice->rtp_cb2 = NULL;
    tp_ice->rtcp_cb = NULL;
    tp_ice->stream = NULL;

    PJ_UNUSED_ARG(strm);
}


static pj_status_t transport_send_rtp(pjmedia_transport *tp,
                                      const void *pkt,
                                      pj_size_t size)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_status_t status;

    /* Simulate packet lost on TX direction */
    if (tp_ice->tx_drop_pct) {
        if ((pj_rand() % 100) <= (int)tp_ice->tx_drop_pct) {
            PJ_LOG(5,(tp_ice->base.name, 
                      "TX RTP packet dropped because of pkt lost "
                      "simulation"));
            return PJ_SUCCESS;
        }
    }

    status = pj_ice_strans_sendto2(tp_ice->ice_st, 1, 
                                   pkt, size, &tp_ice->remote_rtp,
                                   tp_ice->addr_len);
    if (status == PJ_EPENDING)
        status = PJ_SUCCESS;

    return status;
}


static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
                                       const void *pkt,
                                       pj_size_t size)
{
    return transport_send_rtcp2(tp, NULL, 0, pkt, size);
}

static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
                                        const pj_sockaddr_t *addr,
                                        unsigned addr_len,
                                        const void *pkt,
                                        pj_size_t size)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    if (tp_ice->comp_cnt > 1 || tp_ice->use_rtcp_mux) {
        pj_status_t status;
        unsigned comp_id = (tp_ice->use_rtcp_mux? 1: 2);

        if (addr == NULL) {
            addr = &tp_ice->remote_rtcp;
            addr_len = pj_sockaddr_get_len(addr);
        }         

        status = pj_ice_strans_sendto2(tp_ice->ice_st, comp_id, pkt, size,
                                       addr, addr_len);
        if (status == PJ_EPENDING)
            status = PJ_SUCCESS;

        return status;
    } else {
        return PJ_SUCCESS;
    }
}


static void ice_on_rx_data(pj_ice_strans *ice_st, unsigned comp_id, 
                           void *pkt, pj_size_t size,
                           const pj_sockaddr_t *src_addr,
                           unsigned src_addr_len)
{
    struct transport_ice *tp_ice;
    pj_bool_t discard = PJ_FALSE;

    tp_ice = (struct transport_ice*) pj_ice_strans_get_user_data(ice_st);
    if (!tp_ice) {
        /* Destroy on progress */
        return;
    }

    if (comp_id == 1) {
        ++tp_ice->rtp_src_cnt;
        pj_sockaddr_cp(&tp_ice->rtp_src_addr, src_addr);
    } else if (comp_id == 2) {
        pj_sockaddr_cp(&tp_ice->rtcp_src_addr, src_addr);
    }

    if (comp_id==1 && (tp_ice->rtp_cb || tp_ice->rtp_cb2)) {
        pj_bool_t rem_switch = PJ_FALSE;

        /* Simulate packet lost on RX direction */
        if (tp_ice->rx_drop_pct) {
            if ((pj_rand() % 100) <= (int)tp_ice->rx_drop_pct) {
                PJ_LOG(5,(tp_ice->base.name, 
                          "RX RTP packet dropped because of pkt lost "
                          "simulation"));
                return;
            }
        }

        if (!discard) {
            if (tp_ice->rtp_cb2) {
                pjmedia_tp_cb_param param;

                param.user_data = tp_ice->stream;
                param.pkt = pkt;
                param.size = size;
                param.src_addr = (tp_ice->use_ice? NULL:
                                  (pj_sockaddr_t *)src_addr);
                param.rem_switch = PJ_FALSE;
                (*tp_ice->rtp_cb2)(&param);
                rem_switch = param.rem_switch;
            } else {
                (*tp_ice->rtp_cb)(tp_ice->stream, pkt, size);
            }
        }
        
#if defined(PJMEDIA_TRANSPORT_SWITCH_REMOTE_ADDR) && \
    (PJMEDIA_TRANSPORT_SWITCH_REMOTE_ADDR == 1)
        if (rem_switch &&
            (tp_ice->options & PJMEDIA_ICE_NO_SRC_ADDR_CHECKING)==0)
        {
            char addr_text[PJ_INET6_ADDRSTRLEN+10];

            /* Set remote RTP address to source address */
            pj_sockaddr_cp(&tp_ice->rtp_src_addr, src_addr);
            pj_sockaddr_cp(&tp_ice->remote_rtp, src_addr);
            tp_ice->addr_len = pj_sockaddr_get_len(&tp_ice->remote_rtp);

            PJ_LOG(4,(tp_ice->base.name,
                      "Remote RTP address switched to %s",
                      pj_sockaddr_print(&tp_ice->remote_rtp, addr_text,
                                        sizeof(addr_text), 3)));

            if (tp_ice->use_rtcp_mux) {
                pj_sockaddr_cp(&tp_ice->remote_rtcp, &tp_ice->remote_rtp);
            } else if (!pj_sockaddr_has_addr(&tp_ice->rtcp_src_addr)) {
                /* Also update remote RTCP address if actual RTCP source
                 * address is not heard yet.
                 */
                pj_uint16_t port;

                pj_sockaddr_cp(&tp_ice->remote_rtcp, &tp_ice->remote_rtp);

                port = (pj_uint16_t)
                       (pj_sockaddr_get_port(&tp_ice->remote_rtp)+1);
                pj_sockaddr_set_port(&tp_ice->remote_rtcp, port);

                PJ_LOG(4,(tp_ice->base.name,
                          "Remote RTCP address switched to predicted "
                          "address %s",
                          pj_sockaddr_print(&tp_ice->remote_rtcp, 
                                            addr_text,
                                            sizeof(addr_text), 3)));
            }
        }
#else
        PJ_UNUSED_ARG(rem_switch);
#endif

    } else if (comp_id==2 && tp_ice->rtcp_cb) {

#if defined(PJMEDIA_TRANSPORT_SWITCH_REMOTE_ADDR) && \
    (PJMEDIA_TRANSPORT_SWITCH_REMOTE_ADDR == 1)
        /* Check if RTCP source address is the same as the configured
         * remote address, and switch the address when they are
         * different.
         */
        if (!tp_ice->use_ice &&
            (tp_ice->options & PJMEDIA_ICE_NO_SRC_ADDR_CHECKING)==0)
        {
            if (pj_sockaddr_cmp(&tp_ice->remote_rtcp, src_addr) == 0) {
                tp_ice->rtcp_src_cnt = 0;
            } else {
                char addr_text[PJ_INET6_ADDRSTRLEN+10];

                ++tp_ice->rtcp_src_cnt;
                if (tp_ice->rtcp_src_cnt < PJMEDIA_RTCP_NAT_PROBATION_CNT) {
                    discard = PJ_TRUE;
                } else {
                    tp_ice->rtcp_src_cnt = 0;
                    pj_sockaddr_cp(&tp_ice->rtcp_src_addr, src_addr);
                    pj_sockaddr_cp(&tp_ice->remote_rtcp, src_addr);

                    pj_assert(tp_ice->addr_len==pj_sockaddr_get_len(src_addr));

                    PJ_LOG(4,(tp_ice->base.name,
                              "Remote RTCP address switched to %s",
                              pj_sockaddr_print(&tp_ice->remote_rtcp,
                                                addr_text, sizeof(addr_text),
                                                3)));
                }
            }
        }
#endif

        if (!discard)
            (*tp_ice->rtcp_cb)(tp_ice->stream, pkt, size);
    }

    PJ_UNUSED_ARG(src_addr_len);
}


static void ice_on_ice_complete(pj_ice_strans *ice_st, 
                                pj_ice_strans_op op,
                                pj_status_t result)
{
    struct transport_ice *tp_ice;
    ice_listener *il;

    tp_ice = (struct transport_ice*) pj_ice_strans_get_user_data(ice_st);
    if (!tp_ice) {
        /* Destroy on progress */
        return;
    }

    /* Set the flag indicating that local candidate gathering is completed */
    if (op == PJ_ICE_STRANS_OP_INIT && result == PJ_SUCCESS)
        tp_ice->end_of_cand = PJ_TRUE;

    pj_perror(5, tp_ice->base.name, result, "ICE operation complete"
              " (op=%d%s)", op,
              (op==PJ_ICE_STRANS_OP_INIT? "/initialization" :
              (op==PJ_ICE_STRANS_OP_NEGOTIATION? "/negotiation":"")));

    /* Notify application */
    if (tp_ice->cb.on_ice_complete)
        (*tp_ice->cb.on_ice_complete)(&tp_ice->base, op, result);

    for (il=tp_ice->listener.next; il!=&tp_ice->listener; il=il->next) {
        if (il->cb.on_ice_complete2) {
            (*il->cb.on_ice_complete2)(&tp_ice->base, op, result,
                                       il->user_data);
        } else if (il->cb.on_ice_complete) {
            (*il->cb.on_ice_complete)(&tp_ice->base, op, result);
        }
    }
}


static void ice_on_new_candidate(pj_ice_strans *ice_st,
                                 const pj_ice_sess_cand *cand,
                                 pj_bool_t last)
{
    struct transport_ice *tp_ice;
    ice_listener *il;

    tp_ice = (struct transport_ice*) pj_ice_strans_get_user_data(ice_st);
    if (!tp_ice) {
        /* Destroy on progress */
        return;
    }

    /* Notify application */
    if (tp_ice->cb.on_new_candidate)
        (*tp_ice->cb.on_new_candidate)(&tp_ice->base, cand, last);

    for (il=tp_ice->listener.next; il!=&tp_ice->listener; il=il->next) {
        if (il->cb.on_new_candidate) {
            (*il->cb.on_new_candidate)(&tp_ice->base, cand, last);
        }
    }
}


/* Simulate lost */
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
                                           pjmedia_dir dir,
                                           unsigned pct_lost)
{
    struct transport_ice *ice = (struct transport_ice*) tp;

    PJ_ASSERT_RETURN(tp && pct_lost <= 100, PJ_EINVAL);

    if (dir & PJMEDIA_DIR_ENCODING)
        ice->tx_drop_pct = pct_lost;

    if (dir & PJMEDIA_DIR_DECODING)
        ice->rx_drop_pct = pct_lost;

    return PJ_SUCCESS;
}

static void tp_ice_on_destroy(void *arg)
{
    struct transport_ice *tp_ice = (struct transport_ice*)arg;

    PJ_LOG(4, (tp_ice->base.name, "ICE transport destroyed"));
    pj_pool_safe_release(&tp_ice->pool);
}

/*
 * Destroy ICE media transport.
 */
static pj_status_t transport_destroy(pjmedia_transport *tp)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    PJ_LOG(4, (tp_ice->base.name, "Destroying ICE transport"));

    /* Reset callback and user data */
    pj_bzero(&tp_ice->cb, sizeof(tp_ice->cb));
    tp_ice->base.user_data = NULL;
    tp_ice->rtp_cb = NULL;
    tp_ice->rtp_cb2 = NULL;
    tp_ice->rtcp_cb = NULL;

    if (tp_ice->ice_st) {
        pj_grp_lock_t *grp_lock = pj_ice_strans_get_grp_lock(tp_ice->ice_st);
        pj_ice_strans_destroy(tp_ice->ice_st);
        tp_ice->ice_st = NULL;
        pj_grp_lock_dec_ref(grp_lock);
    } else {
        tp_ice_on_destroy(tp);
    }

    return PJ_SUCCESS;
}

