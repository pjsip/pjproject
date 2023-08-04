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
#include <pjnath/ice_session.h>
#include <pj/addr_resolv.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/guid.h>
#include <pj/hash.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>

/* String names for candidate types */
static const char *cand_type_names[] =
{
    "host",
    "srflx",
    "prflx",
    "relay"

};

/* String names for pj_ice_sess_check_state */
#if PJ_LOG_MAX_LEVEL >= 4
static const char *check_state_name[] = 
{
    "Frozen",
    "Waiting",
    "In Progress",
    "Succeeded",
    "Failed"
};

static const char *clist_state_name[] =
{
    "Idle",
    "Running",
    "Completed"
};
#endif  /* PJ_LOG_MAX_LEVEL >= 4 */

static const char *role_names[] = 
{
    "Unknown",
    "Controlled",
    "Controlling"
};

enum timer_type
{
    TIMER_NONE,                 /**< Timer not active                   */
    TIMER_COMPLETION_CALLBACK,  /**< Call on_ice_complete() callback    */
    TIMER_CONTROLLED_WAIT_NOM,  /**< Controlled agent is waiting for 
                                     controlling agent to send connectivity
                                     check with nominated flag after it has
                                     valid check for every components.  */
    TIMER_START_NOMINATED_CHECK,/**< Controlling agent start connectivity
                                     checks with USE-CANDIDATE flag.    */
    TIMER_KEEP_ALIVE            /**< ICE keep-alive timer.              */

};

/* Candidate type preference */
static pj_uint8_t cand_type_prefs[PJ_ICE_CAND_TYPE_MAX] =
{
#if PJ_ICE_CAND_TYPE_PREF_BITS < 8
    /* Keep it to 2 bits */
    3,      /**< PJ_ICE_HOST_PREF       */
    1,      /**< PJ_ICE_SRFLX_PREF.     */
    2,      /**< PJ_ICE_PRFLX_PREF      */
    0       /**< PJ_ICE_RELAYED_PREF    */
#else
    /* Default ICE session preferences, according to draft-ice */
    126,    /**< PJ_ICE_HOST_PREF       */
    100,    /**< PJ_ICE_SRFLX_PREF.     */
    110,    /**< PJ_ICE_PRFLX_PREF      */
    0       /**< PJ_ICE_RELAYED_PREF    */
#endif
};

#define THIS_FILE               "ice_session.c"
#define CHECK_NAME_LEN          128
#define LOG4(expr)              PJ_LOG(4,expr)
#define LOG5(expr)              PJ_LOG(4,expr)
#define GET_LCAND_ID(cand)      (unsigned)(cand - ice->lcand)
#define GET_CHECK_ID(cl, chk)   (chk - (cl)->checks)


/* The data that will be attached to the STUN session on each
 * component.
 */
typedef struct stun_data
{
    pj_ice_sess         *ice;
    unsigned             comp_id;
    pj_ice_sess_comp    *comp;
} stun_data;


/* The data that will be attached to the timer to perform
 * periodic check.
 */
typedef struct timer_data
{
    pj_ice_sess             *ice;
    pj_ice_sess_checklist   *clist;
} timer_data;


/* This is the data that will be attached as token to outgoing
 * STUN messages.
 */


/* Forward declarations */
static void on_timer(pj_timer_heap_t *th, pj_timer_entry *te);
static void on_ice_complete(pj_ice_sess *ice, pj_status_t status);
static void ice_keep_alive(pj_ice_sess *ice, pj_bool_t send_now);
static void ice_on_destroy(void *obj);
static void destroy_ice(pj_ice_sess *ice,
                        pj_status_t reason);
static pj_status_t start_periodic_check(pj_timer_heap_t *th, 
                                        pj_timer_entry *te);
static void start_nominated_check(pj_ice_sess *ice);
static void periodic_timer(pj_timer_heap_t *th, 
                          pj_timer_entry *te);
static void handle_incoming_check(pj_ice_sess *ice,
                                  const pj_ice_rx_check *rcheck);
static void end_of_cand_ind_timer(pj_timer_heap_t *th,
                                  pj_timer_entry *te);

/* These are the callbacks registered to the STUN sessions */
static pj_status_t on_stun_send_msg(pj_stun_session *sess,
                                    void *token,
                                    const void *pkt,
                                    pj_size_t pkt_size,
                                    const pj_sockaddr_t *dst_addr,
                                    unsigned addr_len);
static pj_status_t on_stun_rx_request(pj_stun_session *sess,
                                      const pj_uint8_t *pkt,
                                      unsigned pkt_len,
                                      const pj_stun_rx_data *rdata,
                                      void *token,
                                      const pj_sockaddr_t *src_addr,
                                      unsigned src_addr_len);
static void on_stun_request_complete(pj_stun_session *stun_sess,
                                     pj_status_t status,
                                     void *token,
                                     pj_stun_tx_data *tdata,
                                     const pj_stun_msg *response,
                                     const pj_sockaddr_t *src_addr,
                                     unsigned src_addr_len);
static pj_status_t on_stun_rx_indication(pj_stun_session *sess,
                                         const pj_uint8_t *pkt,
                                         unsigned pkt_len,
                                         const pj_stun_msg *msg,
                                         void *token,
                                         const pj_sockaddr_t *src_addr,
                                         unsigned src_addr_len);

/* These are the callbacks for performing STUN authentication */
static pj_status_t stun_auth_get_auth(void *user_data,
                                      pj_pool_t *pool,
                                      pj_str_t *realm,
                                      pj_str_t *nonce);
static pj_status_t stun_auth_get_cred(const pj_stun_msg *msg,
                                      void *user_data,
                                      pj_pool_t *pool,
                                      pj_str_t *realm,
                                      pj_str_t *username,
                                      pj_str_t *nonce,
                                      pj_stun_passwd_type *data_type,
                                      pj_str_t *data);
static pj_status_t stun_auth_get_password(const pj_stun_msg *msg,
                                          void *user_data, 
                                          const pj_str_t *realm,
                                          const pj_str_t *username,
                                          pj_pool_t *pool,
                                          pj_stun_passwd_type *data_type,
                                          pj_str_t *data);


PJ_DEF(const char*) pj_ice_get_cand_type_name(pj_ice_cand_type type)
{
    PJ_ASSERT_RETURN(type <= PJ_ICE_CAND_TYPE_RELAYED, "???");
    return cand_type_names[type];
}


PJ_DEF(const char*) pj_ice_sess_role_name(pj_ice_sess_role role)
{
    switch (role) {
    case PJ_ICE_SESS_ROLE_UNKNOWN:
        return "Unknown";
    case PJ_ICE_SESS_ROLE_CONTROLLED:
        return "Controlled";
    case PJ_ICE_SESS_ROLE_CONTROLLING:
        return "Controlling";
    default:
        return "??";
    }
}


/* Get the prefix for the foundation */
static int get_type_prefix(pj_ice_cand_type type)
{
    switch (type) {
    case PJ_ICE_CAND_TYPE_HOST:     return 'H';
    case PJ_ICE_CAND_TYPE_SRFLX:    return 'S';
    case PJ_ICE_CAND_TYPE_PRFLX:    return 'P';
    case PJ_ICE_CAND_TYPE_RELAYED:  return 'R';
    default:
        pj_assert(!"Invalid type");
        return 'U';
    }
}

/* Calculate foundation:
 * Two candidates have the same foundation when they are "similar" - of
 * the same type and obtained from the same host candidate and STUN
 * server using the same protocol.  Otherwise, their foundation is
 * different.
 */
PJ_DEF(void) pj_ice_calc_foundation(pj_pool_t *pool,
                                    pj_str_t *foundation,
                                    pj_ice_cand_type type,
                                    const pj_sockaddr *base_addr)
{
#if PJNATH_ICE_PRIO_STD
    char buf[64];
    pj_uint32_t val;

    if (base_addr->addr.sa_family == pj_AF_INET()) {
        val = pj_ntohl(base_addr->ipv4.sin_addr.s_addr);
    } else {
        val = pj_hash_calc(0, pj_sockaddr_get_addr(base_addr),
                           pj_sockaddr_get_addr_len(base_addr));
    }
    pj_ansi_snprintf(buf, sizeof(buf), "%c%x",
                     get_type_prefix(type), val);
    pj_strdup2(pool, foundation, buf);
#else
    /* Much shorter version, valid for candidates added by
     * pj_ice_strans.
     */
    foundation->ptr = (char*) pj_pool_alloc(pool, 1);
    *foundation->ptr = (char)get_type_prefix(type);
    foundation->slen = 1;

    PJ_UNUSED_ARG(base_addr);
#endif
}


/* Init component */
static pj_status_t init_comp(pj_ice_sess *ice,
                             unsigned comp_id,
                             pj_ice_sess_comp *comp)
{
    pj_stun_session_cb sess_cb;
    pj_stun_auth_cred auth_cred;
    stun_data *sd;
    pj_status_t status;

    /* Init STUN callbacks */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_request_complete = &on_stun_request_complete;
    sess_cb.on_rx_indication = &on_stun_rx_indication;
    sess_cb.on_rx_request = &on_stun_rx_request;
    sess_cb.on_send_msg = &on_stun_send_msg;

    /* Create STUN session for this candidate */
    status = pj_stun_session_create(&ice->stun_cfg, NULL, 
                                    &sess_cb, PJ_TRUE,
                                    ice->grp_lock,
                                    &comp->stun_sess);
    if (status != PJ_SUCCESS)
        return status;

    /* Associate data with this STUN session */
    sd = PJ_POOL_ZALLOC_T(ice->pool, struct stun_data);
    sd->ice = ice;
    sd->comp_id = comp_id;
    sd->comp = comp;
    pj_stun_session_set_user_data(comp->stun_sess, sd);

    /* Init STUN authentication credential */
    pj_bzero(&auth_cred, sizeof(auth_cred));
    auth_cred.type = PJ_STUN_AUTH_CRED_DYNAMIC;
    auth_cred.data.dyn_cred.get_auth = &stun_auth_get_auth;
    auth_cred.data.dyn_cred.get_cred = &stun_auth_get_cred;
    auth_cred.data.dyn_cred.get_password = &stun_auth_get_password;
    auth_cred.data.dyn_cred.user_data = comp->stun_sess;
    pj_stun_session_set_credential(comp->stun_sess, PJ_STUN_AUTH_SHORT_TERM,
                                   &auth_cred);

    return PJ_SUCCESS;
}


/* Init options with default values */
PJ_DEF(void) pj_ice_sess_options_default(pj_ice_sess_options *opt)
{
    opt->aggressive = PJ_TRUE;
    opt->nominated_check_delay = PJ_ICE_NOMINATED_CHECK_DELAY;
    opt->controlled_agent_want_nom_timeout = 
        ICE_CONTROLLED_AGENT_WAIT_NOMINATION_TIMEOUT;
    opt->trickle = PJ_ICE_SESS_TRICKLE_DISABLED;
}

/*
 * Create ICE session.
 */
PJ_DEF(pj_status_t) pj_ice_sess_create(pj_stun_config *stun_cfg,
                                       const char *name,
                                       pj_ice_sess_role role,
                                       unsigned comp_cnt,
                                       const pj_ice_sess_cb *cb,
                                       const pj_str_t *local_ufrag,
                                       const pj_str_t *local_passwd,
                                       pj_grp_lock_t *grp_lock,
                                       pj_ice_sess **p_ice)
{
    pj_pool_t *pool;
    pj_ice_sess *ice;
    unsigned i;
    pj_status_t status;

    PJ_ASSERT_RETURN(stun_cfg && cb && p_ice, PJ_EINVAL);

    if (name == NULL)
        name = "icess%p";

    pool = pj_pool_create(stun_cfg->pf, name, PJNATH_POOL_LEN_ICE_SESS, 
                          PJNATH_POOL_INC_ICE_SESS, NULL);
    ice = PJ_POOL_ZALLOC_T(pool, pj_ice_sess);
    ice->pool = pool;
    ice->role = role;
    ice->tie_breaker.u32.hi = pj_rand();
    ice->tie_breaker.u32.lo = pj_rand();
    ice->prefs = cand_type_prefs;
    pj_ice_sess_options_default(&ice->opt);

    pj_timer_entry_init(&ice->timer, TIMER_NONE, (void*)ice, &on_timer);

    pj_ansi_snprintf(ice->obj_name, sizeof(ice->obj_name),
                     name, ice);

    if (grp_lock) {
        ice->grp_lock = grp_lock;
    } else {
        status = pj_grp_lock_create(pool, NULL, &ice->grp_lock);
        if (status != PJ_SUCCESS) {
            pj_pool_release(pool);
            return status;
        }
    }

    pj_grp_lock_add_ref(ice->grp_lock);
    pj_grp_lock_add_handler(ice->grp_lock, pool, ice,
                            &ice_on_destroy);

    pj_memcpy(&ice->cb, cb, sizeof(*cb));
    pj_memcpy(&ice->stun_cfg, stun_cfg, sizeof(*stun_cfg));

    ice->comp_cnt = comp_cnt;
    for (i=0; i<comp_cnt; ++i) {
        pj_ice_sess_comp *comp;
        comp = &ice->comp[i];
        comp->valid_check = NULL;
        comp->nominated_check = NULL;

        status = init_comp(ice, i+1, comp);
        if (status != PJ_SUCCESS) {
            destroy_ice(ice, status);
            return status;
        }
    }

    /* Initialize transport datas */
    for (i=0; i<PJ_ARRAY_SIZE(ice->tp_data); ++i) {
        ice->tp_data[i].transport_id = 0;
        ice->tp_data[i].has_req_data = PJ_FALSE;
    }

    if (local_ufrag == NULL) {
        ice->rx_ufrag.ptr = (char*) pj_pool_alloc(ice->pool, PJ_ICE_UFRAG_LEN);
        pj_create_random_string(ice->rx_ufrag.ptr, PJ_ICE_UFRAG_LEN);
        ice->rx_ufrag.slen = PJ_ICE_UFRAG_LEN;
    } else {
        pj_strdup(ice->pool, &ice->rx_ufrag, local_ufrag);
    }

    if (local_passwd == NULL) {
        ice->rx_pass.ptr = (char*) pj_pool_alloc(ice->pool, PJ_ICE_PWD_LEN);
        pj_create_random_string(ice->rx_pass.ptr, PJ_ICE_PWD_LEN);
        ice->rx_pass.slen = PJ_ICE_PWD_LEN;
    } else {
        pj_strdup(ice->pool, &ice->rx_pass, local_passwd);
    }

    pj_list_init(&ice->early_check);

    ice->valid_pair_found = PJ_FALSE;

    /* Done */
    *p_ice = ice;

    LOG4((ice->obj_name, 
         "ICE session created, comp_cnt=%d, role is %s agent",
         comp_cnt, role_names[ice->role]));

    return PJ_SUCCESS;
}


/*
 * Get the value of various options of the ICE session.
 */
PJ_DEF(pj_status_t) pj_ice_sess_get_options(pj_ice_sess *ice,
                                            pj_ice_sess_options *opt)
{
    PJ_ASSERT_RETURN(ice, PJ_EINVAL);
    pj_memcpy(opt, &ice->opt, sizeof(*opt));
    return PJ_SUCCESS;
}

/*
 * Specify various options for this ICE session.
 */
PJ_DEF(pj_status_t) pj_ice_sess_set_options(pj_ice_sess *ice,
                                            const pj_ice_sess_options *opt)
{
    PJ_ASSERT_RETURN(ice && opt, PJ_EINVAL);
    pj_memcpy(&ice->opt, opt, sizeof(*opt));
    ice->is_trickling = (ice->opt.trickle != PJ_ICE_SESS_TRICKLE_DISABLED);
    if (ice->is_trickling) {
        LOG5((ice->obj_name, "Trickle ICE is active (%s mode)",
              (ice->opt.trickle==PJ_ICE_SESS_TRICKLE_HALF? "half":"full")));

        if (ice->opt.aggressive) {
            /* Disable aggressive when ICE trickle is active */
            ice->opt.aggressive = PJ_FALSE;
            LOG4((ice->obj_name, "Warning: aggressive nomination is disabled"
                                 " as trickle ICE is active"));
        }
    }

    LOG5((ice->obj_name, "ICE nomination type set to %s",
          (ice->opt.aggressive ? "aggressive" : "regular")));
    return PJ_SUCCESS;
}


/*
 * Callback to really destroy the session
 */
static void ice_on_destroy(void *obj)
{
    pj_ice_sess *ice = (pj_ice_sess*) obj;

    pj_pool_safe_release(&ice->pool);

    LOG4((THIS_FILE, "ICE session %p destroyed", ice));
}

/*
 * Destroy
 */
static void destroy_ice(pj_ice_sess *ice,
                        pj_status_t reason)
{
    unsigned i;

    if (reason == PJ_SUCCESS) {
        LOG4((ice->obj_name, "Destroying ICE session %p", ice));
    }

    pj_grp_lock_acquire(ice->grp_lock);

    if (ice->is_destroying) {
        pj_grp_lock_release(ice->grp_lock);
        return;
    }

    ice->is_destroying = PJ_TRUE;

    pj_timer_heap_cancel_if_active(ice->stun_cfg.timer_heap,
                                   &ice->timer, PJ_FALSE);

    for (i=0; i<ice->comp_cnt; ++i) {
        if (ice->comp[i].stun_sess) {
            pj_stun_session_destroy(ice->comp[i].stun_sess);
            ice->comp[i].stun_sess = NULL;
        }
    }

    pj_timer_heap_cancel_if_active(ice->stun_cfg.timer_heap,
                                   &ice->clist.timer,
                                   PJ_FALSE);

    pj_grp_lock_dec_ref(ice->grp_lock);
    pj_grp_lock_release(ice->grp_lock);
}


/*
 * Destroy
 */
PJ_DEF(pj_status_t) pj_ice_sess_destroy(pj_ice_sess *ice)
{
    PJ_ASSERT_RETURN(ice, PJ_EINVAL);
    destroy_ice(ice, PJ_SUCCESS);
    return PJ_SUCCESS;
}


/*
 * Detach ICE session from group lock.
 */
PJ_DEF(pj_status_t) pj_ice_sess_detach_grp_lock(pj_ice_sess *ice,
                                                pj_grp_lock_handler *handler)
{
    PJ_ASSERT_RETURN(ice && handler, PJ_EINVAL);

    pj_grp_lock_acquire(ice->grp_lock);
    pj_grp_lock_del_handler(ice->grp_lock, ice, &ice_on_destroy);
    *handler = &ice_on_destroy;
    pj_grp_lock_release(ice->grp_lock);
    return PJ_SUCCESS;
}


/*
 * Change session role. 
 */
PJ_DEF(pj_status_t) pj_ice_sess_change_role(pj_ice_sess *ice,
                                            pj_ice_sess_role new_role)
{
    PJ_ASSERT_RETURN(ice, PJ_EINVAL);

    if (new_role != ice->role) {
        ice->role = new_role;
        LOG4((ice->obj_name, "Role changed to %s", role_names[new_role]));
    }

    return PJ_SUCCESS;
}


/*
 * Change type preference
 */
PJ_DEF(pj_status_t) pj_ice_sess_set_prefs(pj_ice_sess *ice,
                                          const pj_uint8_t prefs[4])
{
    unsigned i;
    PJ_ASSERT_RETURN(ice && prefs, PJ_EINVAL);
    ice->prefs = (pj_uint8_t*) pj_pool_calloc(ice->pool, PJ_ICE_CAND_TYPE_MAX,
                                              sizeof(pj_uint8_t));
    for (i=0; i<PJ_ICE_CAND_TYPE_MAX; ++i) {
#if PJ_ICE_CAND_TYPE_PREF_BITS < 8
        pj_assert(prefs[i] < (2 << PJ_ICE_CAND_TYPE_PREF_BITS));
#endif
        ice->prefs[i] = prefs[i];
    }
    return PJ_SUCCESS;
}


/* Find component by ID */
static pj_ice_sess_comp *find_comp(const pj_ice_sess *ice, unsigned comp_id)
{
    /* Ticket #1844: possible wrong assertion when remote has less ICE comp */
    //pj_assert(comp_id > 0 && comp_id <= ice->comp_cnt);
    if (comp_id > ice->comp_cnt)
        return NULL;

    return (pj_ice_sess_comp*) &ice->comp[comp_id-1];
}


/* Callback by STUN authentication when it needs to send 401 */
static pj_status_t stun_auth_get_auth(void *user_data,
                                      pj_pool_t *pool,
                                      pj_str_t *realm,
                                      pj_str_t *nonce)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(pool);

    realm->slen = 0;
    nonce->slen = 0;

    return PJ_SUCCESS;
}


/* Get credential to be sent with outgoing message */
static pj_status_t stun_auth_get_cred(const pj_stun_msg *msg,
                                      void *user_data,
                                      pj_pool_t *pool,
                                      pj_str_t *realm,
                                      pj_str_t *username,
                                      pj_str_t *nonce,
                                      pj_stun_passwd_type *data_type,
                                      pj_str_t *data)
{
    pj_stun_session *sess = (pj_stun_session *)user_data;
    stun_data *sd = (stun_data*) pj_stun_session_get_user_data(sess);
    pj_ice_sess *ice = sd->ice;

    PJ_UNUSED_ARG(pool);
    realm->slen = nonce->slen = 0;

    if (PJ_STUN_IS_RESPONSE(msg->hdr.type)) {
        /* Outgoing responses need to have the same credential as
         * incoming requests.
         */
        *username = ice->rx_uname;
        *data_type = PJ_STUN_PASSWD_PLAIN;
        *data = ice->rx_pass;
    }
    else {
        *username = ice->tx_uname;
        *data_type = PJ_STUN_PASSWD_PLAIN;
        *data = ice->tx_pass;
    }

    return PJ_SUCCESS;
}

/* Get password to be used to authenticate incoming message */
static pj_status_t stun_auth_get_password(const pj_stun_msg *msg,
                                          void *user_data, 
                                          const pj_str_t *realm,
                                          const pj_str_t *username,
                                          pj_pool_t *pool,
                                          pj_stun_passwd_type *data_type,
                                          pj_str_t *data)
{
    pj_stun_session *sess = (pj_stun_session *)user_data;
    stun_data *sd = (stun_data*) pj_stun_session_get_user_data(sess);
    pj_ice_sess *ice = sd->ice;

    PJ_UNUSED_ARG(realm);
    PJ_UNUSED_ARG(pool);

    if (PJ_STUN_IS_SUCCESS_RESPONSE(msg->hdr.type) ||
        PJ_STUN_IS_ERROR_RESPONSE(msg->hdr.type))
    {
        /* Incoming response is authenticated with TX credential */
        /* Verify username */
        if (pj_strcmp(username, &ice->tx_uname) != 0)
            return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_SC_UNAUTHORIZED);
        *data_type = PJ_STUN_PASSWD_PLAIN;
        *data = ice->tx_pass;

    } else {
        /* Incoming request is authenticated with RX credential */
        /* The agent MUST accept a credential if the username consists
         * of two values separated by a colon, where the first value is
         * equal to the username fragment generated by the agent in an offer
         * or answer for a session in-progress, and the MESSAGE-INTEGRITY 
         * is the output of a hash of the password and the STUN packet's 
         * contents.
         */
        const char *pos;
        pj_str_t ufrag;

        pos = (const char*)pj_memchr(username->ptr, ':', username->slen);
        if (pos == NULL)
            return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_SC_UNAUTHORIZED);

        ufrag.ptr = (char*)username->ptr;
        ufrag.slen = (pos - username->ptr);

        if (pj_strcmp(&ufrag, &ice->rx_ufrag) != 0)
            return PJ_STATUS_FROM_STUN_CODE(PJ_STUN_SC_UNAUTHORIZED);

        *data_type = PJ_STUN_PASSWD_PLAIN;
        *data = ice->rx_pass;

    }

    return PJ_SUCCESS;
}


static pj_uint32_t CALC_CAND_PRIO(pj_ice_sess *ice,
                                  pj_ice_cand_type type,
                                  pj_uint32_t local_pref,
                                  pj_uint32_t comp_id)
{
#if PJNATH_ICE_PRIO_STD
    return ((ice->prefs[type] & 0xFF) << 24) + 
           ((local_pref & 0xFFFF)    << 8) +
           (((256 - comp_id) & 0xFF) << 0);
#else
    enum {
        type_mask   = ((1 << PJ_ICE_CAND_TYPE_PREF_BITS) - 1),
        local_mask  = ((1 << PJ_ICE_LOCAL_PREF_BITS) - 1),
        comp_mask   = ((1 << PJ_ICE_COMP_BITS) - 1),

        comp_shift  = 0,
        local_shift = (PJ_ICE_COMP_BITS),
        type_shift  = (comp_shift + local_shift),

        max_comp    = (2<<PJ_ICE_COMP_BITS),
    };

    return ((ice->prefs[type] & type_mask) << type_shift) + 
           ((local_pref & local_mask) << local_shift) +
           (((max_comp - comp_id) & comp_mask) << comp_shift);
#endif
}


/*
 * Add ICE candidate
 */
PJ_DEF(pj_status_t) pj_ice_sess_add_cand(pj_ice_sess *ice,
                                         unsigned comp_id,
                                         unsigned transport_id,
                                         pj_ice_cand_type type,
                                         pj_uint16_t local_pref,
                                         const pj_str_t *foundation,
                                         const pj_sockaddr_t *addr,
                                         const pj_sockaddr_t *base_addr,
                                         const pj_sockaddr_t *rel_addr,
                                         int addr_len,
                                         unsigned *p_cand_id)
{
    pj_ice_sess_cand *lcand;
    pj_status_t status = PJ_SUCCESS;
    char address[PJ_INET6_ADDRSTRLEN];
    unsigned i;

    PJ_ASSERT_RETURN(ice && comp_id && 
                     foundation && addr && base_addr && addr_len,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(comp_id <= ice->comp_cnt, PJ_EINVAL);

    pj_grp_lock_acquire(ice->grp_lock);

    if (ice->lcand_cnt >= PJ_ARRAY_SIZE(ice->lcand)) {
        status = PJ_ETOOMANY;
        goto on_return;
    }

    if (ice->opt.trickle != PJ_ICE_SESS_TRICKLE_DISABLED) {
        /* Trickle ICE:
         * Make sure that candidate has not been added
         */
        for (i=0; i<ice->lcand_cnt; ++i) {
            const pj_ice_sess_cand *c = &ice->lcand[i];
            if (c->comp_id==comp_id && c->type == type &&
                pj_sockaddr_cmp(&c->addr, addr)==0 &&
                pj_sockaddr_cmp(&c->base_addr, base_addr)==0)
            {
                break;
            }
        }

        /* Skip candidate, it has been added */
        if (i < ice->lcand_cnt) {
            if (p_cand_id)
                *p_cand_id = i;
            goto on_return;
        }
    }

    lcand = &ice->lcand[ice->lcand_cnt];
    lcand->id = ice->lcand_cnt;
    lcand->comp_id = (pj_uint8_t)comp_id;
    lcand->transport_id = (pj_uint8_t)transport_id;
    lcand->type = type;
    pj_strdup(ice->pool, &lcand->foundation, foundation);
    lcand->local_pref = local_pref;
    lcand->prio = CALC_CAND_PRIO(ice, type, local_pref, lcand->comp_id);
    pj_sockaddr_cp(&lcand->addr, addr);
    pj_sockaddr_cp(&lcand->base_addr, base_addr);
    if (rel_addr == NULL)
        rel_addr = base_addr;
    pj_memcpy(&lcand->rel_addr, rel_addr, addr_len);

    /* Update transport data */
    for (i = 0; i < PJ_ARRAY_SIZE(ice->tp_data); ++i) {
        /* Check if this transport has been registered */
        if (ice->tp_data[i].transport_id == transport_id)
            break;

        if (ice->tp_data[i].transport_id == 0) {
            /* Found an empty slot, register this transport here */
            ice->tp_data[i].transport_id = transport_id;
            break;
        }
    }
    pj_assert(i < PJ_ARRAY_SIZE(ice->tp_data) &&
              ice->tp_data[i].transport_id == transport_id);

    pj_ansi_strxcpy(ice->tmp.txt, pj_sockaddr_print(&lcand->addr, address,
                                                    sizeof(address), 2),
                    sizeof(ice->tmp.txt));
    LOG4((ice->obj_name, 
         "Candidate %d added: comp_id=%d, type=%s, foundation=%.*s, "
         "addr=%s:%d, base=%s:%d, prio=0x%x (%u)",
         lcand->id,
         lcand->comp_id, 
         cand_type_names[lcand->type],
         (int)lcand->foundation.slen,
         lcand->foundation.ptr,
         ice->tmp.txt, 
          pj_sockaddr_get_port(&lcand->addr),
          pj_sockaddr_print(&lcand->base_addr, address, sizeof(address), 2),
          pj_sockaddr_get_port(&lcand->base_addr),
         lcand->prio, lcand->prio));

    if (p_cand_id)
        *p_cand_id = lcand->id;

    ++ice->lcand_cnt;

on_return:
    pj_grp_lock_release(ice->grp_lock);
    return status;
}


/* Find default candidate ID for the component */
PJ_DEF(pj_status_t) pj_ice_sess_find_default_cand(pj_ice_sess *ice,
                                                  unsigned comp_id,
                                                  int *cand_id)
{
    unsigned i;

    PJ_ASSERT_RETURN(ice && comp_id && cand_id, PJ_EINVAL);
    PJ_ASSERT_RETURN(comp_id <= ice->comp_cnt, PJ_EINVAL);

    *cand_id = -1;

    pj_grp_lock_acquire(ice->grp_lock);

    /* First find in valid list if we have nominated pair */
    for (i=0; i<ice->valid_list.count; ++i) {
        pj_ice_sess_check *check = &ice->valid_list.checks[i];
        
        if (check->lcand->comp_id == comp_id) {
            *cand_id = GET_LCAND_ID(check->lcand);
            pj_grp_lock_release(ice->grp_lock);
            return PJ_SUCCESS;
        }
    }

    /* If there's no nominated pair, find relayed candidate */
    for (i=0; i<ice->lcand_cnt; ++i) {
        pj_ice_sess_cand *lcand = &ice->lcand[i];
        if (lcand->comp_id==comp_id &&
            lcand->type == PJ_ICE_CAND_TYPE_RELAYED) 
        {
            *cand_id = GET_LCAND_ID(lcand);
            pj_grp_lock_release(ice->grp_lock);
            return PJ_SUCCESS;
        }
    }

    /* If there's no relayed candidate, find reflexive candidate */
    for (i=0; i<ice->lcand_cnt; ++i) {
        pj_ice_sess_cand *lcand = &ice->lcand[i];
        if (lcand->comp_id==comp_id &&
            (lcand->type == PJ_ICE_CAND_TYPE_SRFLX ||
             lcand->type == PJ_ICE_CAND_TYPE_PRFLX)) 
        {
            *cand_id = GET_LCAND_ID(lcand);
            pj_grp_lock_release(ice->grp_lock);
            return PJ_SUCCESS;
        }
    }

    /* Otherwise return host candidate */
    for (i=0; i<ice->lcand_cnt; ++i) {
        pj_ice_sess_cand *lcand = &ice->lcand[i];
        if (lcand->comp_id==comp_id &&
            lcand->type == PJ_ICE_CAND_TYPE_HOST) 
        {
            *cand_id = GET_LCAND_ID(lcand);
            pj_grp_lock_release(ice->grp_lock);
            return PJ_SUCCESS;
        }
    }

    /* Still no candidate is found! :( */
    pj_grp_lock_release(ice->grp_lock);

    pj_assert(!"Should have a candidate by now");
    return PJ_EBUG;
}


#ifndef MIN
#   define MIN(a,b) (a < b ? a : b)
#endif

#ifndef MAX
#   define MAX(a,b) (a > b ? a : b)
#endif

static pj_timestamp CALC_CHECK_PRIO(const pj_ice_sess *ice, 
                                    const pj_ice_sess_cand *lcand,
                                    const pj_ice_sess_cand *rcand)
{
    pj_uint32_t O, A;
    pj_timestamp prio;

    /* Original formula:
     *   pair priority = 2^32*MIN(O,A) + 2*MAX(O,A) + (O>A?1:0)
     */

    if (ice->role == PJ_ICE_SESS_ROLE_CONTROLLING) {
        O = lcand->prio; 
        A = rcand->prio;
    } else {
        O = rcand->prio;
        A = lcand->prio;
    }

    /*
    return ((pj_uint64_t)1 << 32) * MIN(O, A) +
           (pj_uint64_t)2 * MAX(O, A) + (O>A ? 1 : 0);
    */

    prio.u32.hi = MIN(O,A);
    prio.u32.lo = (MAX(O, A) << 1) + (O>A ? 1 : 0);

    return prio;
}

PJ_INLINE(int) CMP_CHECK_STATE(const pj_ice_sess_check *c1,
                               const pj_ice_sess_check *c2)
{
    /* SUCCEEDED has higher state than FAILED */
    if (c1->state == PJ_ICE_SESS_CHECK_STATE_SUCCEEDED &&
        c2->state == PJ_ICE_SESS_CHECK_STATE_FAILED)
    {
        return 1;
    }
    if (c2->state == PJ_ICE_SESS_CHECK_STATE_SUCCEEDED &&
        c1->state == PJ_ICE_SESS_CHECK_STATE_FAILED)
    {
        return -1;
    }

    /* Other state, just compare the state value */
    return (c1->state - c2->state);
}


PJ_INLINE(int) CMP_CHECK_PRIO(const pj_ice_sess_check *c1,
                              const pj_ice_sess_check *c2)
{
    return pj_cmp_timestamp(&c1->prio, &c2->prio);
}


#if PJ_LOG_MAX_LEVEL >= 4
static const char *dump_check(char *buffer, unsigned bufsize,
                              const pj_ice_sess_checklist *clist,
                              const pj_ice_sess_check *check)
{
    const pj_ice_sess_cand *lcand = check->lcand;
    const pj_ice_sess_cand *rcand = check->rcand;
    char laddr[PJ_INET6_ADDRSTRLEN], raddr[PJ_INET6_ADDRSTRLEN];
    int len;

    PJ_CHECK_STACK();

    len = pj_ansi_snprintf(buffer, bufsize,
                           "%d: [%d] %s:%d-->%s:%d",
                           (int)GET_CHECK_ID(clist, check),
                           check->lcand->comp_id,
                           pj_sockaddr_print(&lcand->addr, laddr,
                                             sizeof(laddr), 2),
                           pj_sockaddr_get_port(&lcand->addr),
                           pj_sockaddr_print(&rcand->addr, raddr,
                                             sizeof(raddr), 2),
                           pj_sockaddr_get_port(&rcand->addr));

    if (len < 0)
        len = 0;
    else if (len >= (int)bufsize)
        len = bufsize - 1;

    buffer[len] = '\0';
    return buffer;
}

static void dump_checklist(const char *title, pj_ice_sess *ice, 
                           const pj_ice_sess_checklist *clist)
{
    unsigned i;

    LOG4((ice->obj_name, "%s", title));
    for (i=0; i<clist->count; ++i) {
        const pj_ice_sess_check *c = &clist->checks[i];
        LOG4((ice->obj_name, " %s (%s, state=%s)",
             dump_check(ice->tmp.txt, sizeof(ice->tmp.txt), clist, c),
             (c->nominated ? "nominated" : "not nominated"), 
             check_state_name[c->state]));
    }
}

#else
#define dump_checklist(title, ice, clist)
#endif

static void check_set_state(pj_ice_sess *ice, pj_ice_sess_check *check,
                            pj_ice_sess_check_state st, 
                            pj_status_t err_code)
{
    LOG5((ice->obj_name, "Check %s: state changed from %s to %s",
         dump_check(ice->tmp.txt, sizeof(ice->tmp.txt), &ice->clist, check),
         check_state_name[check->state],
         check_state_name[st]));

    /* Put the assert after printing log for debugging purpose */
    // There is corner case, nomination (in non-aggressive ICE mode) may be
    // done using an in-progress pair instead of successful pair, this is
    // possible because host candidates actually share a single STUN transport
    // and pair selection for nomination compares transport instead of
    // candidate. So later the pair will receive double completions.
    //pj_assert(check->state < PJ_ICE_SESS_CHECK_STATE_SUCCEEDED);

    check->state = st;
    check->err_code = err_code;
}

static void clist_set_state(pj_ice_sess *ice, pj_ice_sess_checklist *clist,
                            pj_ice_sess_checklist_state st)
{
    if (clist->state != st) {
        LOG5((ice->obj_name, "Checklist: state changed from %s to %s",
             clist_state_name[clist->state],
             clist_state_name[st]));
        clist->state = st;
    }
}

/* Sort checklist based on state & priority, we need to put Successful pairs
 * on top of the list for pruning.
 */
static void sort_checklist(pj_ice_sess *ice, pj_ice_sess_checklist *clist)
{
    unsigned i;
    pj_ice_sess_check **check_ptr[PJ_ICE_MAX_COMP*2];
    unsigned check_ptr_cnt = 0;

    for (i=0; i<ice->comp_cnt; ++i) {
        if (ice->comp[i].valid_check) {
            check_ptr[check_ptr_cnt++] = &ice->comp[i].valid_check;
        }
        if (ice->comp[i].nominated_check) {
            check_ptr[check_ptr_cnt++] = &ice->comp[i].nominated_check;
        }
    }

    pj_assert(clist->count > 0);
    for (i=0; i<clist->count-1; ++i) {
        unsigned j, highest = i;

        for (j=i+1; j<clist->count; ++j) {
            int cmp_state = CMP_CHECK_STATE(&clist->checks[j],
                                            &clist->checks[highest]);
            if (cmp_state > 0 ||
                (cmp_state==0 && CMP_CHECK_PRIO(&clist->checks[j],
                                                &clist->checks[highest]) > 0))
            {
                highest = j;
            }
        }

        if (highest != i) {
            pj_ice_sess_check tmp;
            unsigned k;

            pj_memcpy(&tmp, &clist->checks[i], sizeof(pj_ice_sess_check));
            pj_memcpy(&clist->checks[i], &clist->checks[highest], 
                      sizeof(pj_ice_sess_check));
            pj_memcpy(&clist->checks[highest], &tmp, 
                      sizeof(pj_ice_sess_check));

            /* Update valid and nominated check pointers, since we're moving
             * around checks
             */
            for (k=0; k<check_ptr_cnt; ++k) {
                if (*check_ptr[k] == &clist->checks[highest])
                    *check_ptr[k] = &clist->checks[i];
                else if (*check_ptr[k] == &clist->checks[i])
                    *check_ptr[k] = &clist->checks[highest];
            }
        }
    }
}

/* Remove a check pair from checklist */
static void remove_check(pj_ice_sess *ice, pj_ice_sess_checklist *clist,
                         unsigned check_idx,
                         const char *reason)
{
    LOG5((ice->obj_name, "Check %s pruned (%s)",
          dump_check(ice->tmp.txt, sizeof(ice->tmp.txt),
                     clist, &clist->checks[check_idx]),
          reason));

    pj_array_erase(clist->checks, sizeof(clist->checks[0]),
                   clist->count, check_idx);
    --clist->count;
}

/* Prune checklist, this must have been done after the checklist
 * is sorted.
 */
static pj_status_t prune_checklist(pj_ice_sess *ice, 
                                   pj_ice_sess_checklist *clist)
{
    unsigned i;

    /* Since an agent cannot send requests directly from a reflexive
     * candidate, but only from its base, the agent next goes through the
     * sorted list of candidate pairs.  For each pair where the local
     * candidate is server reflexive, the server reflexive candidate MUST be
     * replaced by its base.  Once this has been done, the agent MUST prune
     * the list.  This is done by removing a pair if its local and remote
     * candidates are identical to the local and remote candidates of a pair
     * higher up on the priority list.  The result is a sequence of ordered
     * candidate pairs, called the check list for that media stream.    
     */
    /* First replace SRFLX candidates with their base */
    for (i=0; i<clist->count; ++i) {
        pj_ice_sess_cand *srflx = clist->checks[i].lcand;

        if (srflx->type == PJ_ICE_CAND_TYPE_SRFLX ||
            srflx->type == PJ_ICE_CAND_TYPE_PRFLX)
        {
            /* Find the base for this candidate */
            unsigned j;
            for (j=0; j<ice->lcand_cnt; ++j) {
                pj_ice_sess_cand *host = &ice->lcand[j];

                if (host->type != PJ_ICE_CAND_TYPE_HOST)
                    continue;

                if (pj_sockaddr_cmp(&srflx->base_addr, &host->addr) == 0) {
                    /* Replace this SRFLX/PRFLX with its BASE */
                    clist->checks[i].lcand = host;
                    break;
                }
            }

            if (j==ice->lcand_cnt) {
                char baddr[PJ_INET6_ADDRSTRLEN];
                /* Host candidate not found this this srflx! */
                LOG4((ice->obj_name, 
                      "Base candidate %s:%d not found for srflx candidate %d",
                      pj_sockaddr_print(&srflx->base_addr, baddr,
                                        sizeof(baddr), 2),
                      pj_sockaddr_get_port(&srflx->base_addr),
                      GET_LCAND_ID(srflx)));
                return PJNATH_EICENOHOSTCAND;
            }
        }
    }

    /* Next remove a pair if its local and remote candidates are identical
     * to the local and remote candidates of a pair higher up on the priority
     * list
     */
    /*
     * Not in ICE!
     * Remove host candidates if their base are the the same!
     */
    for (i=0; i<clist->count; ++i) {
        pj_ice_sess_cand *licand = clist->checks[i].lcand;
        pj_ice_sess_cand *ricand = clist->checks[i].rcand;
        unsigned j;

        for (j=i+1; j<clist->count;) {
            pj_ice_sess_cand *ljcand = clist->checks[j].lcand;
            pj_ice_sess_cand *rjcand = clist->checks[j].rcand;
            const char *reason = NULL;

            /* Only discard Frozen/Waiting checks */
            if (clist->checks[j].state != PJ_ICE_SESS_CHECK_STATE_FROZEN &&
                clist->checks[j].state != PJ_ICE_SESS_CHECK_STATE_WAITING)
            {
                ++j;
                continue;
            }

            if ((licand == ljcand) && (ricand == rjcand)) {
                reason = "duplicate found";
            } else if ((rjcand == ricand) &&
                       (pj_sockaddr_cmp(&ljcand->base_addr, 
                                     &licand->base_addr)==0)) 
            {
                reason = "equal base";
            }

            if (reason != NULL) {
                /* Found duplicate, remove it */
                remove_check(ice, clist, j, reason);
            } else {
                ++j;
            }
        }
    }

    return PJ_SUCCESS;
}

/* Timer callback */
static void on_timer(pj_timer_heap_t *th, pj_timer_entry *te)
{
    pj_ice_sess *ice = (pj_ice_sess*) te->user_data;
    enum timer_type type = (enum timer_type)te->id;

    PJ_UNUSED_ARG(th);

    pj_grp_lock_acquire(ice->grp_lock);

    te->id = TIMER_NONE;

    if (ice->is_destroying) {
        /* Stray timer, could happen when destroy is invoked while callback
         * is pending. */
        pj_grp_lock_release(ice->grp_lock);
        return;
    }

    switch (type) {
    case TIMER_CONTROLLED_WAIT_NOM:
        LOG4((ice->obj_name, 
              "Controlled agent timed-out in waiting for the controlling "
              "agent to send nominated check. Setting state to fail now.."));
        on_ice_complete(ice, PJNATH_EICENOMTIMEOUT);
        break;
    case TIMER_COMPLETION_CALLBACK:
        {
            void (*on_ice_complete)(pj_ice_sess *ice, pj_status_t status);
            pj_status_t ice_status;

            /* Start keep-alive timer but don't send any packets yet.
             * Need to do it here just in case app destroy the session
             * in the callback.
             */
            if (ice->ice_status == PJ_SUCCESS)
                ice_keep_alive(ice, PJ_FALSE);

            /* Release mutex in case app destroy us in the callback */
            ice_status = ice->ice_status;
            on_ice_complete = ice->cb.on_ice_complete;

            /* Notify app about ICE completion*/
            if (on_ice_complete)
                (*on_ice_complete)(ice, ice_status);
        }
        break;
    case TIMER_START_NOMINATED_CHECK:
        start_nominated_check(ice);
        break;
    case TIMER_KEEP_ALIVE:
        ice_keep_alive(ice, PJ_TRUE);
        break;
    case TIMER_NONE:
        /* Nothing to do, just to get rid of gcc warning */
        break;
    }

    pj_grp_lock_release(ice->grp_lock);
}

/* Send keep-alive */
static void ice_keep_alive(pj_ice_sess *ice, pj_bool_t send_now)
{
    if (send_now) {
        /* Send Binding Indication for the component */
        pj_ice_sess_comp *comp = &ice->comp[ice->comp_ka];
        pj_stun_tx_data *tdata;
        pj_ice_sess_check *the_check;
        pj_ice_msg_data *msg_data;
        int addr_len;
        pj_bool_t saved;
        pj_status_t status;

        /* Must have nominated check by now */
        pj_assert(comp->nominated_check != NULL);
        the_check = comp->nominated_check;

        /* Create the Binding Indication */
        status = pj_stun_session_create_ind(comp->stun_sess, 
                                            PJ_STUN_BINDING_INDICATION,
                                            &tdata);
        if (status != PJ_SUCCESS)
            goto done;

        /* Need the transport_id */
        msg_data = PJ_POOL_ZALLOC_T(tdata->pool, pj_ice_msg_data);
        msg_data->transport_id = the_check->lcand->transport_id;

        /* RFC 5245 Section 10:
         * The Binding Indication SHOULD contain the FINGERPRINT attribute
         * to aid in demultiplexing, but SHOULD NOT contain any other
         * attributes.
         */
        saved = pj_stun_session_use_fingerprint(comp->stun_sess, PJ_TRUE);

        /* Send to session */
        addr_len = pj_sockaddr_get_len(&the_check->rcand->addr);
        status = pj_stun_session_send_msg(comp->stun_sess, msg_data,
                                          PJ_FALSE, PJ_FALSE, 
                                          &the_check->rcand->addr, 
                                          addr_len, tdata);

        /* Restore FINGERPRINT usage */
        pj_stun_session_use_fingerprint(comp->stun_sess, saved);

done:
        ice->comp_ka = (ice->comp_ka + 1) % ice->comp_cnt;
    }

    if (ice->timer.id == TIMER_NONE) {
        pj_time_val delay = { 0, 0 };

        delay.msec = (PJ_ICE_SESS_KEEP_ALIVE_MIN + 
                      (pj_rand() % PJ_ICE_SESS_KEEP_ALIVE_MAX_RAND)) * 1000 / 
                     ice->comp_cnt;
        pj_time_val_normalize(&delay);

        pj_timer_heap_schedule_w_grp_lock(ice->stun_cfg.timer_heap,
                                          &ice->timer, &delay,
                                          TIMER_KEEP_ALIVE,
                                          ice->grp_lock);

    } else {
        pj_assert(!"Not expected any timer active");
    }
}

/* This function is called when ICE processing completes */
static void on_ice_complete(pj_ice_sess *ice, pj_status_t status)
{
    if (!ice->is_complete) {
        ice->is_complete = PJ_TRUE;
        ice->ice_status = status;
    
        pj_timer_heap_cancel_if_active(ice->stun_cfg.timer_heap, &ice->timer,
                                       TIMER_NONE);

        /* Log message */
        LOG4((ice->obj_name, "ICE process complete, status=%s", 
             pj_strerror(status, ice->tmp.errmsg, 
                         sizeof(ice->tmp.errmsg)).ptr));

        dump_checklist("Valid list", ice, &ice->valid_list);

        /* Call callback */
        if (ice->cb.on_ice_complete) {
            pj_time_val delay = {0, 0};

            pj_timer_heap_schedule_w_grp_lock(ice->stun_cfg.timer_heap,
                                              &ice->timer, &delay,
                                              TIMER_COMPLETION_CALLBACK,
                                              ice->grp_lock);
        }
    }
}

/* Update valid check and nominated check for the candidate */
static void update_comp_check(pj_ice_sess *ice, unsigned comp_id, 
                              pj_ice_sess_check *check)
{
    pj_ice_sess_comp *comp;

    pj_assert(!ice->is_complete);

    comp = find_comp(ice, comp_id);
    if (comp->valid_check == NULL) {
        comp->valid_check = check;
    } else {
        pj_bool_t update = PJ_FALSE;

        /* Update component's valid check with conditions:
         * - it is the first nominated check, or
         * - it has higher prio, as long as nomination status is NOT degraded
         *   (existing is nominated -> new is not-nominated).
         */
        if (!comp->nominated_check && check->nominated)
        {
            update = PJ_TRUE;
        } else if (CMP_CHECK_PRIO(comp->valid_check, check) < 0 &&
                   (!comp->nominated_check || check->nominated))
        {
            update = PJ_TRUE;
        }

        if (update)
            comp->valid_check = check;
    }

    if (check->nominated) {
        /* Update the nominated check for the component */
        if (comp->nominated_check == NULL) {
            comp->nominated_check = check;
        } else {
            if (CMP_CHECK_PRIO(comp->nominated_check, check) < 0)
                comp->nominated_check = check;
        }
    }
}

/* Check if ICE nego completed */
static pj_bool_t check_ice_complete(pj_ice_sess *ice)
{
    unsigned i;
    pj_bool_t no_pending_check = PJ_FALSE;

    /* Still in 8.2.  Updating States
     * 
     * o  Once there is at least one nominated pair in the valid list for
     *    every component of at least one media stream and the state of the
     *    check list is Running:
     *    
     *    *  The agent MUST change the state of processing for its check
     *       list for that media stream to Completed.
     *    
     *    *  The agent MUST continue to respond to any checks it may still
     *       receive for that media stream, and MUST perform triggered
     *       checks if required by the processing of Section 7.2.
     *    
     *    *  The agent MAY begin transmitting media for this media stream as
     *       described in Section 11.1
     */

    /* See if all components have nominated pair. If they do, then mark
     * ICE processing as success, otherwise wait.
     */
    for (i=0; i<ice->comp_cnt; ++i) {
        if (ice->comp[i].nominated_check == NULL)
            break;
    }
    if (i == ice->comp_cnt) {
        /* All components have nominated pair */
        on_ice_complete(ice, PJ_SUCCESS);
        return PJ_TRUE;
    }

    /* Note: this is the stuffs that we don't do in 7.1.2.2.2, since our
     *       ICE session only supports one media stream for now:
     * 
     * 7.1.2.2.2.  Updating Pair States
     *
     * 2.  If there is a pair in the valid list for every component of this
     *     media stream (where this is the actual number of components being
     *     used, in cases where the number of components signaled in the SDP
     *     differs from offerer to answerer), the success of this check may
     *     unfreeze checks for other media streams. 
     */

    /* 7.1.2.3.  Check List and Timer State Updates
     * Regardless of whether the check was successful or failed, the
     * completion of the transaction may require updating of check list and
     * timer states.
     * 
     * If all of the pairs in the check list are now either in the Failed or
     * Succeeded state, and there is not a pair in the valid list for each
     * component of the media stream, the state of the check list is set to
     * Failed.  
     */

    /* 
     * See if all checks in the checklist have completed. If we do,
     * then mark ICE processing as failed.
     */
    if (!ice->is_trickling) {
        for (i=0; i<ice->clist.count; ++i) {
            pj_ice_sess_check *c = &ice->clist.checks[i];
            if (c->state < PJ_ICE_SESS_CHECK_STATE_SUCCEEDED) {
                break;
            }
        }
        no_pending_check = (i == ice->clist.count);
    }

    if (no_pending_check) {
        /* All checks have completed, but we don't have nominated pair.
         * If agent's role is controlled, check if all components have
         * valid pair. If it does, this means the controlled agent has
         * finished the check list and it's waiting for controlling
         * agent to send checks with USE-CANDIDATE flag set.
         */
        if (ice->role == PJ_ICE_SESS_ROLE_CONTROLLED) {
            for (i=0; i < ice->comp_cnt; ++i) {
                if (ice->comp[i].valid_check == NULL)
                    break;
            }

            if (i < ice->comp_cnt) {
                /* This component ID doesn't have valid pair.
                 * Mark ICE as failed. 
                 */
                on_ice_complete(ice, PJNATH_EICEFAILED);
                return PJ_TRUE;
            } else {
                /* All components have a valid pair.
                 * We should wait until we receive nominated checks.
                 */
                if (ice->timer.id == TIMER_NONE &&
                    ice->opt.controlled_agent_want_nom_timeout >= 0) 
                {
                    pj_time_val delay;

                    delay.sec = 0;
                    delay.msec = ice->opt.controlled_agent_want_nom_timeout;
                    pj_time_val_normalize(&delay);

                    pj_timer_heap_schedule_w_grp_lock(
                                        ice->stun_cfg.timer_heap,
                                        &ice->timer, &delay,
                                        TIMER_CONTROLLED_WAIT_NOM,
                                        ice->grp_lock);

                    LOG5((ice->obj_name, 
                          "All checks have completed. Controlled agent now "
                          "waits for nomination from controlling agent "
                          "(timeout=%d msec)",
                          ice->opt.controlled_agent_want_nom_timeout));
                }
                return PJ_FALSE;
            }

            /* Unreached */

        } else if (ice->is_nominating) {
            /* We are controlling agent and all checks have completed but
             * there's at least one component without nominated pair (or
             * more likely we don't have any nominated pairs at all).
             */
            on_ice_complete(ice, PJNATH_EICEFAILED);
            return PJ_TRUE;

        } else {
            /* We are controlling agent and all checks have completed. If
             * we have valid list for every component, then move on to
             * sending nominated check, otherwise we have failed.
             */
            for (i=0; i<ice->comp_cnt; ++i) {
                if (ice->comp[i].valid_check == NULL)
                    break;
            }

            if (i < ice->comp_cnt) {
                /* At least one component doesn't have a valid check. Mark
                 * ICE as failed.
                 */
                on_ice_complete(ice, PJNATH_EICEFAILED);
                return PJ_TRUE;
            }

            /* Now it's time to send connectivity check with nomination 
             * flag set.
             */
            LOG4((ice->obj_name, 
                  "All checks have completed, starting nominated checks now"));
            start_nominated_check(ice);
            return PJ_FALSE;
        }
    }

    /* If this connectivity check has been successful, scan all components
     * and see if they have a valid pair, if we are controlling and we haven't
     * started our nominated check yet.
     */
    /* Always scan regardless the last connectivity check result */
    if (/*check->err_code == PJ_SUCCESS && */
        ice->role==PJ_ICE_SESS_ROLE_CONTROLLING &&
        !ice->is_nominating &&
        ice->timer.id == TIMER_NONE) 
    {
        pj_time_val delay;

        for (i=0; i<ice->comp_cnt; ++i) {
            if (ice->comp[i].valid_check == NULL)
                break;
        }

        if (i < ice->comp_cnt) {
            /* Some components still don't have valid pair, continue
             * processing.
             */
            return PJ_FALSE;
        }

        LOG4((ice->obj_name, 
              "Scheduling nominated check in %d ms",
              ice->opt.nominated_check_delay));

        pj_timer_heap_cancel_if_active(ice->stun_cfg.timer_heap, &ice->timer,
                                       TIMER_NONE);

        /* All components have valid pair. Let connectivity checks run for
         * a little bit more time, then start our nominated check.
         */
        delay.sec = 0;
        delay.msec = ice->opt.nominated_check_delay;
        pj_time_val_normalize(&delay);

        pj_timer_heap_schedule_w_grp_lock(ice->stun_cfg.timer_heap,
                                          &ice->timer, &delay,
                                          TIMER_START_NOMINATED_CHECK,
                                          ice->grp_lock);
        return PJ_FALSE;
    }

    /* We still have checks to perform */
    return PJ_FALSE;
}

/* This function is called when one check completes */
static pj_bool_t on_check_complete(pj_ice_sess *ice,
                                   pj_ice_sess_check *check)
{
    pj_ice_sess_comp *comp;
    unsigned i;

    pj_assert(check->state >= PJ_ICE_SESS_CHECK_STATE_SUCCEEDED);

    comp = find_comp(ice, check->lcand->comp_id);

    /* 7.1.2.2.2.  Updating Pair States
     * 
     * The agent sets the state of the pair that generated the check to
     * Succeeded.  The success of this check might also cause the state of
     * other checks to change as well.  The agent MUST perform the following
     * two steps:
     * 
     * 1.  The agent changes the states for all other Frozen pairs for the
     *     same media stream and same foundation to Waiting.  Typically
     *     these other pairs will have different component IDs but not
     *     always.
     */
    if (check->err_code==PJ_SUCCESS) {

        for (i=0; i<ice->clist.count; ++i) {
            pj_ice_sess_check *c = &ice->clist.checks[i];
            if (c->foundation_idx == check->foundation_idx &&
                c->state == PJ_ICE_SESS_CHECK_STATE_FROZEN)
            {
                check_set_state(ice, c, PJ_ICE_SESS_CHECK_STATE_WAITING, 0);
            }
        }

        LOG5((ice->obj_name, "Check %ld is successful%s",
             GET_CHECK_ID(&ice->clist, check),
             (check->nominated ? " and nominated" : "")));

        /* On the first valid pair, we call the callback, if present */
        if (ice->valid_pair_found == PJ_FALSE) {
            ice->valid_pair_found = PJ_TRUE;

            if (ice->cb.on_valid_pair) {
                (*ice->cb.on_valid_pair)(ice);
            }
        }
    }

    /* 8.2.  Updating States
     * 
     * For both controlling and controlled agents, the state of ICE
     * processing depends on the presence of nominated candidate pairs in
     * the valid list and on the state of the check list:
     *
     * o  If there are no nominated pairs in the valid list for a media
     *    stream and the state of the check list is Running, ICE processing
     *    continues.
     *
     * o  If there is at least one nominated pair in the valid list:
     *
     *    - The agent MUST remove all Waiting and Frozen pairs in the check
     *      list for the same component as the nominated pairs for that
     *      media stream
     *
     *    - If an In-Progress pair in the check list is for the same
     *      component as a nominated pair, the agent SHOULD cease
     *      retransmissions for its check if its pair priority is lower
     *      than the lowest priority nominated pair for that component
     */
    if (check->err_code==PJ_SUCCESS && check->nominated) {

        for (i=0; i<ice->clist.count; ++i) {

            pj_ice_sess_check *c = &ice->clist.checks[i];

            if (c->lcand->comp_id == check->lcand->comp_id) {

                if (c->state < PJ_ICE_SESS_CHECK_STATE_IN_PROGRESS) {

                    /* Just fail Frozen/Waiting check */
                    LOG5((ice->obj_name, 
                         "Check %s to be failed because state is %s",
                         dump_check(ice->tmp.txt, sizeof(ice->tmp.txt), 
                                    &ice->clist, c), 
                         check_state_name[c->state]));
                    check_set_state(ice, c, PJ_ICE_SESS_CHECK_STATE_FAILED,
                                    PJ_ECANCELLED);

                } else if (c->state == PJ_ICE_SESS_CHECK_STATE_IN_PROGRESS
                           && (PJ_ICE_CANCEL_ALL ||
                                CMP_CHECK_PRIO(c, check) < 0)) {

                    /* State is IN_PROGRESS, cancel transaction */
                    if (c->tdata) {
                        LOG5((ice->obj_name, 
                             "Cancelling check %s (In Progress)",
                             dump_check(ice->tmp.txt, sizeof(ice->tmp.txt), 
                                        &ice->clist, c)));
                        pj_stun_session_cancel_req(comp->stun_sess, 
                                                   c->tdata, PJ_FALSE, 0);
                        c->tdata = NULL;
                        check_set_state(ice, c, PJ_ICE_SESS_CHECK_STATE_FAILED,
                                        PJ_ECANCELLED);
                    }
                }
            }
        }
    }

    return check_ice_complete(ice);
}


/* Get foundation index of a check pair. This function can also be used for
 * adding a new foundation (combination of local & remote cands foundations)
 * to checklist.
 */
static int get_check_foundation_idx(pj_ice_sess *ice,
                                    const pj_ice_sess_cand *lcand,
                                    const pj_ice_sess_cand *rcand,
                                    pj_bool_t add_if_not_found)
{
    pj_ice_sess_checklist *clist = &ice->clist;
    char fnd_str[65];
    unsigned i;

    pj_ansi_snprintf(fnd_str, sizeof(fnd_str), "%.*s|%.*s",
                     (int)lcand->foundation.slen, lcand->foundation.ptr,
                     (int)rcand->foundation.slen, rcand->foundation.ptr);
    for (i=0; i<clist->foundation_cnt; ++i) {
        if (pj_strcmp2(&clist->foundation[i], fnd_str) == 0)
            return i;
    }

    if (add_if_not_found && clist->foundation_cnt < PJ_ICE_MAX_CHECKS) {
        pj_strdup2(ice->pool, &clist->foundation[i], fnd_str);
        ++clist->foundation_cnt;
        return i;
    }

    return -1;
}

/* Discard a pair check with Failed state or lowest prio (as long as lower
 * than prio_lower_than.
 */
static int discard_check(pj_ice_sess *ice, pj_ice_sess_checklist *clist,
                         const pj_timestamp *prio_lower_than)
{
    /* Discard any Failed check */
    unsigned k;
    for (k=0; k < clist->count; ++k) {
        if (clist->checks[k].state==PJ_ICE_SESS_CHECK_STATE_FAILED) {
            remove_check(ice, clist, k, "too many, drop Failed");
            return 1;
        }
    }

    /* If none, discard the lowest prio */
    /* Re-sort before discarding the last */
    sort_checklist(ice, clist);
    if (!prio_lower_than ||
        pj_cmp_timestamp(&clist->checks[k-1].prio, prio_lower_than) < 0)
    {
        remove_check(ice, clist, k-1, "too many, drop low-prio");
        return 1;
    }

    return 0;
}


/* Timer callback for end of candidate indication from remote */
static void end_of_cand_ind_timer(pj_timer_heap_t *th,
                                  pj_timer_entry *te)
{
    pj_ice_sess *ice = (pj_ice_sess*)te->user_data;
    PJ_UNUSED_ARG(th);

    pj_grp_lock_acquire(ice->grp_lock);

    if (ice->is_trickling && !ice->is_complete) {
        LOG5((ice->obj_name, "End-of-candidate timer timeout, any future "
                             "remote candidate update will be ignored"));
        ice->is_trickling = PJ_FALSE;

        /* ICE checks may have been completed/failed */
        check_ice_complete(ice);
    }

    pj_grp_lock_release(ice->grp_lock);
}


/* Add remote candidates and create/update checklist */
static pj_status_t add_rcand_and_update_checklist(
                              pj_ice_sess *ice,
                              unsigned rem_cand_cnt,
                              const pj_ice_sess_cand rem_cand[],
                              pj_bool_t trickle_done)
{
    pj_ice_sess_checklist *clist;
    unsigned i, j, new_pair = 0;
    pj_status_t status;

    /* Save remote candidates */
    for (i=0; i<rem_cand_cnt; ++i) {
        pj_ice_sess_cand *cn = &ice->rcand[ice->rcand_cnt];

        /* Check component ID */
        if (rem_cand[i].comp_id==0 || rem_cand[i].comp_id > ice->comp_cnt)
        {
            continue;
        }

        if (ice->opt.trickle != PJ_ICE_SESS_TRICKLE_DISABLED) {
            /* Trickle ICE:
             * Make sure that candidate has not been added
             */
            for (j=0; j<ice->rcand_cnt; ++j) {
                const pj_ice_sess_cand *c1 = &rem_cand[i];
                const pj_ice_sess_cand *c2 = &ice->rcand[j];
                if (c1->comp_id==c2->comp_id && c1->type==c2->type &&
                    pj_sockaddr_cmp(&c1->addr, &c2->addr)==0)
                {
                    break;
                }
            }

            /* Skip candidate, it has been added */
            if (j < ice->rcand_cnt)
                continue;
        }
        
        /* Available cand slot? */
        if (ice->rcand_cnt >= PJ_ICE_MAX_CAND) {
            char tmp[PJ_INET6_ADDRSTRLEN + 10];
            PJ_PERROR(3,(ice->obj_name, PJ_ETOOMANY,
                         "Cannot add remote candidate %s",
                         pj_sockaddr_print(&rem_cand[i].addr,
                                           tmp, sizeof(tmp), 3)));
            continue;
        }

        /* Add this candidate */
        pj_memcpy(cn, &rem_cand[i], sizeof(pj_ice_sess_cand));
        pj_strdup(ice->pool, &cn->foundation, &rem_cand[i].foundation);
        cn->id = ice->rcand_cnt++;
    }

    /* Generate checklist */
    clist = &ice->clist;
    for (i=0; i<ice->lcand_cnt; ++i) {
        /* First index of remote cand to be paired with this local cand */
        unsigned rstart = (i >= ice->lcand_paired)? 0 : ice->rcand_paired;
        for (j=rstart; j<ice->rcand_cnt; ++j) {

            pj_ice_sess_cand *lcand = &ice->lcand[i];
            pj_ice_sess_cand *rcand = &ice->rcand[j];
            pj_ice_sess_check *chk = NULL;

            if (clist->count >= PJ_ICE_MAX_CHECKS) {
                // Instead of returning PJ_ETOOMANY, discard Failed/low-prio.
                // If this check is actually the lowest prio, just skip it.
                //return PJ_ETOOMANY;
                pj_timestamp max_prio = CALC_CHECK_PRIO(ice, lcand, rcand);
                if (discard_check(ice, clist, &max_prio) == 0)
                    continue;
            }
            
            /* A local candidate is paired with a remote candidate if
             * and only if the two candidates have the same component ID 
             * and have the same IP address version. 
             */
            if ((lcand->comp_id != rcand->comp_id) ||
                (lcand->addr.addr.sa_family != rcand->addr.addr.sa_family))
            {
                continue;
            }

#if 0
            /* Trickle ICE:
             * Make sure that pair has not been added to checklist
             */
            // Should not happen, paired cands are already marked using
            // lcand_paired & rcand_paired.
            if (ice->opt.trickle != PJ_ICE_SESS_TRICKLE_DISABLED) {
                unsigned k;
                for (k=0; k<clist->count; ++k) {
                    chk = &clist->checks[k];
                    if (chk->lcand == lcand && chk->rcand == rcand)
                        break;
                }

                /* Pair already exists */
                if (k < clist->count)
                    continue;
            }
#endif


            /* Add the pair */
            chk = &clist->checks[clist->count];
            chk->lcand = lcand;
            chk->rcand = rcand;
            chk->prio = CALC_CHECK_PRIO(ice, lcand, rcand);
            chk->state = PJ_ICE_SESS_CHECK_STATE_FROZEN;
            chk->foundation_idx = get_check_foundation_idx(ice, lcand, rcand,
                                                           PJ_TRUE);

            /* Check if foundation cannot be added (e.g: list is full) */
            if (chk->foundation_idx < 0)
                continue;

            /* Check if the check can be unfrozen */
            if (ice->is_trickling) {
                unsigned k;

                /* For this foundation, unfreeze if this pair has the lowest
                 * comp ID, or the highest priority among existing pairs with
                 * same comp ID, or any other checks in Succeeded.
                 */
                for (k=0; k<clist->count; ++k) {
                    if (clist->checks[k].foundation_idx != chk->foundation_idx)
                        continue;

                    /* Unfreeze if there is already check in Succeeded */
                    if (clist->checks[k].state==PJ_ICE_SESS_CHECK_STATE_SUCCEEDED)
                    {
                        k = clist->count;
                        break;
                    }

                    /* Don't unfreeze if there is already check in Waiting or
                     * In Progress.
                     */
                    if (clist->checks[k].state==PJ_ICE_SESS_CHECK_STATE_WAITING ||
                        clist->checks[k].state==PJ_ICE_SESS_CHECK_STATE_IN_PROGRESS)
                    {
                        break;
                    }

                    /* Don't unfreeze if this pair does not have the lowest
                     * comp ID.
                     */
                    if (clist->checks[k].lcand->comp_id < lcand->comp_id)
                        break;

                    /* Don't unfreeze if this pair has the lowest comp ID, but
                     * does not have the highest prio.
                     */
                    if (clist->checks[k].lcand->comp_id == lcand->comp_id &&
                        pj_cmp_timestamp(&clist->checks[k].prio, &chk->prio) > 0)
                    {
                        break;
                    }
                }

                /* Unfreeze */
                if (k == clist->count)
                     check_set_state(ice, chk, PJ_ICE_SESS_CHECK_STATE_WAITING, 0);
            }

            clist->count++;
            new_pair++;
        }
    }

    /* This could happen if candidates have no matching address families */
    if (clist->count==0 && trickle_done) {
        LOG4((ice->obj_name,  "Error: no checklist can be created"));
        return PJ_ENOTFOUND;
    }

    /* Update paired candidate counts */
    ice->lcand_paired = ice->lcand_cnt;
    ice->rcand_paired = ice->rcand_cnt;

    if (new_pair) {
        /* Sort checklist based on priority */
        //dump_checklist("Checklist before sort:", ice, &ice->clist);
        sort_checklist(ice, clist);

        /* Prune the checklist */
        //dump_checklist("Checklist before prune:", ice, &ice->clist);
        status = prune_checklist(ice, clist);
        if (status != PJ_SUCCESS)
            return status;
    }

    /* Regular ICE or trickle ICE after end-of-candidates indication:
     * Disable our components which don't have matching component
     */
    if (trickle_done) {
        unsigned highest_comp = 0;

        for (i=0; i<ice->rcand_cnt; ++i) {
            if (ice->rcand[i].comp_id > highest_comp)
                highest_comp = ice->rcand[i].comp_id;
        }

        for (i=highest_comp; i<ice->comp_cnt; ++i) {
            if (ice->comp[i].stun_sess) {
                pj_stun_session_destroy(ice->comp[i].stun_sess);
                pj_bzero(&ice->comp[i], sizeof(ice->comp[i]));
            }
        }
        ice->comp_cnt = highest_comp;

        /* If using trickle ICE and end-of-candidate has been signalled,
         * check for ICE nego completion.
         */
        if (ice->opt.trickle != PJ_ICE_SESS_TRICKLE_DISABLED)
            check_ice_complete(ice);
    }

    /* For trickle ICE: resume the periodic check, it may be halted when
     * there is no available check pair.
     */
    if (ice->opt.trickle != PJ_ICE_SESS_TRICKLE_DISABLED &&
        clist->count > 0 && !ice->is_complete &&
        clist->state == PJ_ICE_SESS_CHECKLIST_ST_RUNNING)
    {
        if (!pj_timer_entry_running(&clist->timer)) {
            pj_time_val delay = {0, 0};
            status = pj_timer_heap_schedule_w_grp_lock(
                                                    ice->stun_cfg.timer_heap,
                                                    &clist->timer, &delay,
                                                    PJ_TRUE,
                                                    ice->grp_lock);
            if (status == PJ_SUCCESS) {
                LOG5((ice->obj_name,
                      "Trickle ICE resumes periodic check because "
                      "check pair is available"));
            }
        }
    }

    /* Stop the end-of-candidates indication timer if trickling is done */
    if (trickle_done && pj_timer_entry_running(&ice->timer_end_of_cand)) {
        pj_timer_heap_cancel_if_active(ice->stun_cfg.timer_heap,
                                       &ice->timer_end_of_cand, 0);
    }

    return PJ_SUCCESS;
}


/* Create checklist by pairing local candidates with remote candidates */
PJ_DEF(pj_status_t) pj_ice_sess_create_check_list(
                              pj_ice_sess *ice,
                              const pj_str_t *rem_ufrag,
                              const pj_str_t *rem_passwd,
                              unsigned rem_cand_cnt,
                              const pj_ice_sess_cand rem_cand[])
{
    pj_ice_sess_checklist *clist;
    char buf[128];
    pj_str_t username;
    timer_data *td;
    pj_status_t status;

    PJ_ASSERT_RETURN(ice && rem_ufrag && rem_passwd, PJ_EINVAL);

    pj_grp_lock_acquire(ice->grp_lock);

    if (ice->tx_ufrag.slen) {
        /* Checklist has been created */
        pj_grp_lock_release(ice->grp_lock);
        return PJ_SUCCESS;
    }

    /* Save credentials */
    username.ptr = buf;

    pj_strcpy(&username, rem_ufrag);
    pj_strcat2(&username, ":");
    pj_strcat(&username, &ice->rx_ufrag);
    pj_strdup(ice->pool, &ice->tx_uname, &username);

    pj_strdup(ice->pool, &ice->tx_ufrag, rem_ufrag);
    pj_strdup(ice->pool, &ice->tx_pass, rem_passwd);

    pj_strcpy(&username, &ice->rx_ufrag);
    pj_strcat2(&username, ":");
    pj_strcat(&username, rem_ufrag);
    pj_strdup(ice->pool, &ice->rx_uname, &username);

    /* Init timer entry in the checklist. Initially the timer ID is FALSE
     * because timer is not running.
     */
    clist = &ice->clist;
    clist->timer.id = PJ_FALSE;
    td = PJ_POOL_ZALLOC_T(ice->pool, timer_data);
    td->ice = ice;
    td->clist = clist;
    clist->timer.user_data = (void*)td;
    clist->timer.cb = &periodic_timer;

    ice->clist.count = 0;
    ice->lcand_paired = ice->rcand_paired = 0;

    /* Build checklist only if both sides have candidates already */
    if (ice->lcand_cnt > 0 && rem_cand_cnt > 0) {
        status = add_rcand_and_update_checklist(ice, rem_cand_cnt, rem_cand,
                                                !ice->is_trickling);
        if (status != PJ_SUCCESS) {
            pj_grp_lock_release(ice->grp_lock);
            return status;
        }

        /* Log checklist */
        dump_checklist("Checklist created:", ice, clist);
    }

    pj_grp_lock_release(ice->grp_lock);

    return PJ_SUCCESS;
}


/* Update checklist by pairing local candidates with remote candidates */
PJ_DEF(pj_status_t) pj_ice_sess_update_check_list(
                              pj_ice_sess *ice,
                              const pj_str_t *rem_ufrag,
                              const pj_str_t *rem_passwd,
                              unsigned rem_cand_cnt,
                              const pj_ice_sess_cand rem_cand[],
                              pj_bool_t trickle_done)
{
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(ice && ((rem_cand_cnt==0) ||
                             (rem_ufrag && rem_passwd && rem_cand)),
                     PJ_EINVAL);

    pj_grp_lock_acquire(ice->grp_lock);

    /* Ignore if remote ufrag has not known yet */
    if (ice->tx_ufrag.slen == 0) {
        LOG5((ice->obj_name,
              "Cannot update ICE checklist when remote ufrag is unknown"));
        pj_grp_lock_release(ice->grp_lock);
        return PJ_EINVALIDOP;
    }

    /* Ignore if trickle has been stopped (e.g: received end-of-candidate) */
    if (!ice->is_trickling && rem_cand_cnt) {
        LOG5((ice->obj_name,
              "Ignored remote candidate update as ICE trickling has ended"));
        pj_grp_lock_release(ice->grp_lock);
        return PJ_SUCCESS;
    }
    
    /* Verify remote ufrag & passwd, if remote candidate specified */
    if (rem_cand_cnt && (pj_strcmp(&ice->tx_ufrag, rem_ufrag) ||
                         pj_strcmp(&ice->tx_pass, rem_passwd)))
    {
        LOG5((ice->obj_name, "Ignored remote candidate update due to remote "
                             "ufrag/pwd mismatch"));
        rem_cand_cnt = 0;
    }

    if (status == PJ_SUCCESS) {
        status = add_rcand_and_update_checklist(ice, rem_cand_cnt, rem_cand,
                                                trickle_done);
    }

    /* Log checklist */
    if (status == PJ_SUCCESS)
        dump_checklist("Checklist updated:", ice, &ice->clist);

    if (trickle_done && ice->is_trickling) {
        LOG5((ice->obj_name, "Remote signalled end-of-candidates "
                             "and local candidates gathering completed, "
                             "will ignore any candidate update"));
        ice->is_trickling = PJ_FALSE;
    }

    pj_grp_lock_release(ice->grp_lock);

    return status;
}

/* Perform check on the specified candidate pair. */
static pj_status_t perform_check(pj_ice_sess *ice, 
                                 pj_ice_sess_checklist *clist,
                                 unsigned check_id,
                                 pj_bool_t nominate)
{
    pj_ice_sess_comp *comp;
    pj_ice_msg_data *msg_data;
    pj_ice_sess_check *check;
    const pj_ice_sess_cand *lcand;
    const pj_ice_sess_cand *rcand;
    pj_uint32_t prio;
    pj_status_t status;

    check = &clist->checks[check_id];
    lcand = check->lcand;
    rcand = check->rcand;
    comp = find_comp(ice, lcand->comp_id);

    LOG5((ice->obj_name, 
         "Sending connectivity check for check %s", 
         dump_check(ice->tmp.txt, sizeof(ice->tmp.txt), clist, check)));
    pj_log_push_indent();

    /* Create request */
    status = pj_stun_session_create_req(comp->stun_sess, 
                                        PJ_STUN_BINDING_REQUEST, PJ_STUN_MAGIC,
                                        NULL, &check->tdata);
    if (status != PJ_SUCCESS) {
        pjnath_perror(ice->obj_name, "Error creating STUN request", status);
        pj_log_pop_indent();
        return status;
    }

    /* Attach data to be retrieved later when STUN request transaction
     * completes and on_stun_request_complete() callback is called.
     */
    msg_data = PJ_POOL_ZALLOC_T(check->tdata->pool, pj_ice_msg_data);
    msg_data->transport_id = lcand->transport_id;
    msg_data->has_req_data = PJ_TRUE;
    msg_data->data.req.ice = ice;
    msg_data->data.req.clist = clist;
    msg_data->data.req.ckid = check_id;
    msg_data->data.req.lcand = check->lcand;
    msg_data->data.req.rcand = check->rcand;

    /* Add PRIORITY */
#if PJNATH_ICE_PRIO_STD
    prio = CALC_CAND_PRIO(ice, PJ_ICE_CAND_TYPE_PRFLX, 65535 - lcand->id,
                          lcand->comp_id);
#else
    prio = CALC_CAND_PRIO(ice, PJ_ICE_CAND_TYPE_PRFLX,
                          ((1 << PJ_ICE_LOCAL_PREF_BITS) - 1) - lcand->id,
                          lcand->comp_id);
#endif
    pj_stun_msg_add_uint_attr(check->tdata->pool, check->tdata->msg, 
                              PJ_STUN_ATTR_PRIORITY, prio);

    /* Add USE-CANDIDATE and set this check to nominated.
     * Also add ICE-CONTROLLING or ICE-CONTROLLED
     */
    if (ice->role == PJ_ICE_SESS_ROLE_CONTROLLING) {
        if (nominate) {
            pj_stun_msg_add_empty_attr(check->tdata->pool, check->tdata->msg,
                                       PJ_STUN_ATTR_USE_CANDIDATE);
            check->nominated = PJ_TRUE;
        }

        pj_stun_msg_add_uint64_attr(check->tdata->pool, check->tdata->msg, 
                                    PJ_STUN_ATTR_ICE_CONTROLLING,
                                    &ice->tie_breaker);

    } else {
        pj_stun_msg_add_uint64_attr(check->tdata->pool, check->tdata->msg, 
                                    PJ_STUN_ATTR_ICE_CONTROLLED,
                                    &ice->tie_breaker);
    }


    /* Note that USERNAME and MESSAGE-INTEGRITY will be added by the 
     * STUN session.
     */

    /* Initiate STUN transaction to send the request */
    status = pj_stun_session_send_msg(comp->stun_sess, msg_data, PJ_FALSE, 
                                      PJ_TRUE, &rcand->addr, 
                                      pj_sockaddr_get_len(&rcand->addr),
                                      check->tdata);
    if (status != PJ_SUCCESS) {
        check->tdata = NULL;
        pjnath_perror(ice->obj_name, "Error sending STUN request", status);
        pj_log_pop_indent();
        return status;
    }

    check_set_state(ice, check, PJ_ICE_SESS_CHECK_STATE_IN_PROGRESS, 
                    PJ_SUCCESS);
    pj_log_pop_indent();
    return PJ_SUCCESS;
}


/* Start periodic check for the specified checklist.
 * This callback is called by timer on every Ta (20msec by default)
 */
static pj_status_t start_periodic_check(pj_timer_heap_t *th, 
                                        pj_timer_entry *te)
{
    timer_data *td;
    pj_ice_sess *ice;
    pj_ice_sess_checklist *clist;
    pj_ice_sess_check *check = NULL;
    unsigned i, check_idx = 0;
    pj_status_t status;

    td = (struct timer_data*) te->user_data;
    ice = td->ice;
    clist = td->clist;

    pj_grp_lock_acquire(ice->grp_lock);

    if (ice->is_destroying) {
        pj_grp_lock_release(ice->grp_lock);
        return PJ_SUCCESS;
    }

    /* Set timer ID to FALSE first */
    te->id = PJ_FALSE;

    /* Set checklist state to Running */
    clist_set_state(ice, clist, PJ_ICE_SESS_CHECKLIST_ST_RUNNING);

    LOG5((ice->obj_name, "Starting checklist periodic check"));
    pj_log_push_indent();

    /* Find a pair to check (using STUN Binding request).
     * - If we are nominating in regular nomination, only check the valid pair
     *   of each component.
     * - Otherwise, check any first/highest-prio pair in Waiting, or Frozen
     *   if no pair is in Waiting.
     */
    if (ice->is_nominating && !ice->opt.aggressive) {
        /* ICE is nominating in regular nomination, find any first valid pair,
         * the pair should already be in Waiting state.
         */
        for (i=0; i<ice->comp_cnt && !check; ++i) {
            unsigned j;
            const pj_ice_sess_check *vc = ice->comp[i].valid_check;
            for (j=0; j<ice->clist.count; ++j) {
                pj_ice_sess_check *c = &ice->clist.checks[j];
                if (c->state == PJ_ICE_SESS_CHECK_STATE_WAITING &&
                    c->lcand->transport_id == vc->lcand->transport_id &&
                    c->rcand == vc->rcand)
                {
                    check = c;
                    check_idx = j;
                    break;
                }
            }
        }

    } else {
        /* Not nominating or in aggressive-nomination mode */

        /* Find any pair with highest priority on Waiting state. */
        for (i=0; i<clist->count; ++i) {
            pj_ice_sess_check *c = &clist->checks[i];
            if (c->state == PJ_ICE_SESS_CHECK_STATE_WAITING) {
                check = c;
                check_idx = i;
                break;
            }
        }

        /* If we don't have anything in Waiting state, find any pair with
         * highest priority in Frozen state.
         */
        if (!check) {
            for (i=0; i<clist->count; ++i) {
                pj_ice_sess_check *c = &clist->checks[i];
                if (c->state == PJ_ICE_SESS_CHECK_STATE_FROZEN) {
                    check = c;
                    check_idx = i;
                    break;
                }
            }
        }
    }

    /* Perform check & schedule next check for next candidate pair,
     * unless there is no suitable candidate pair (all pairs have been checked
     * or empty checklist).
     */
    if (check) {
        pj_time_val timeout = {0, PJ_ICE_TA_VAL};

        status = perform_check(ice, clist, check_idx, ice->is_nominating);
        if (status != PJ_SUCCESS) {
            check_set_state(ice, check,
                            PJ_ICE_SESS_CHECK_STATE_FAILED, status);
            on_check_complete(ice, check);
        }

        /* Schedule next check */
        pj_time_val_normalize(&timeout);
        pj_timer_heap_schedule_w_grp_lock(th, te, &timeout, PJ_TRUE,
                                          ice->grp_lock);
    }

    pj_grp_lock_release(ice->grp_lock);
    pj_log_pop_indent();
    return PJ_SUCCESS;
}


/* Start sending connectivity check with USE-CANDIDATE */
static void start_nominated_check(pj_ice_sess *ice)
{
    pj_time_val delay;
    unsigned i;
    pj_status_t status;

    LOG4((ice->obj_name, "Starting nominated check.."));
    pj_log_push_indent();

    pj_assert(ice->is_nominating == PJ_FALSE);

    /* Stop trickling if not yet */
    if (ice->is_trickling) {
        ice->is_trickling = PJ_FALSE;
        LOG5((ice->obj_name, "Trickling stopped as nomination started."));
    }

    /* Stop our timer if it's active */
    if (ice->timer.id == TIMER_START_NOMINATED_CHECK) {
        pj_timer_heap_cancel_if_active(ice->stun_cfg.timer_heap, &ice->timer,
                                       TIMER_NONE);
    }

    /* For each component, set the check state of valid check with
     * highest priority to Waiting (it should have Success state now).
     */
    for (i=0; i<ice->comp_cnt; ++i) {
        unsigned j;
        const pj_ice_sess_check *vc = ice->comp[i].valid_check;

        pj_assert(ice->comp[i].nominated_check == NULL);
        pj_assert(vc->err_code == PJ_SUCCESS);

        for (j=0; j<ice->clist.count; ++j) {
            pj_ice_sess_check *c = &ice->clist.checks[j];
            if (c->lcand->transport_id == vc->lcand->transport_id &&
                c->rcand == vc->rcand)
            {
                pj_assert(c->err_code == PJ_SUCCESS);
                c->state = PJ_ICE_SESS_CHECK_STATE_FROZEN;
                check_set_state(ice, c, PJ_ICE_SESS_CHECK_STATE_WAITING, 
                                PJ_SUCCESS);
                break;
            }
        }

        /* Make sure the valid pair is found the checklist */
        pj_assert(j < ice->clist.count);
    }

    /* And (re)start the periodic check */
    pj_timer_heap_cancel_if_active(ice->stun_cfg.timer_heap,
                                   &ice->clist.timer, PJ_FALSE);

    delay.sec = delay.msec = 0;
    status = pj_timer_heap_schedule_w_grp_lock(ice->stun_cfg.timer_heap,
                                               &ice->clist.timer, &delay,
                                               PJ_TRUE,
                                               ice->grp_lock);
    if (status == PJ_SUCCESS) {
        LOG5((ice->obj_name, "Periodic timer rescheduled.."));
    }

    ice->is_nominating = PJ_TRUE;
    pj_log_pop_indent();
}

/* Timer callback to perform periodic check */
static void periodic_timer(pj_timer_heap_t *th, 
                           pj_timer_entry *te)
{
    start_periodic_check(th, te);
}


/*
 * Start ICE periodic check. This function will return immediately, and
 * application will be notified about the connectivity check status in
 * #pj_ice_sess_cb callback.
 */
PJ_DEF(pj_status_t) pj_ice_sess_start_check(pj_ice_sess *ice)
{
    pj_ice_sess_checklist *clist;
    pj_ice_rx_check *rcheck;
    unsigned i;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(ice, PJ_EINVAL);

    /* Checklist must have been created */
    if (ice->clist.count == 0 && !ice->is_trickling)
        return PJ_EINVALIDOP;

    /* Lock session */
    pj_grp_lock_acquire(ice->grp_lock);

    LOG4((ice->obj_name, "Starting ICE check.."));
    pj_log_push_indent();

    /* If we are using aggressive nomination, set the is_nominating state */
    if (ice->opt.aggressive)
        ice->is_nominating = PJ_TRUE;

    /* The agent examines the check list for the first media stream (a
     * media stream is the first media stream when it is described by
     * the first m-line in the SDP offer and answer).  For that media
     * stream, it:
     * 
     * -  Groups together all of the pairs with the same foundation,
     * 
     * -  For each group, sets the state of the pair with the lowest
     *    component ID to Waiting.  If there is more than one such pair,
     *    the one with the highest priority is used.
     */

    clist = &ice->clist;
    for (i=0; i < clist->foundation_cnt; ++i) {
        unsigned k;
        pj_ice_sess_check *chk = NULL;

        for (k=0; k < clist->count; ++k) {
            pj_ice_sess_check *c = &clist->checks[k];
            if (c->foundation_idx != (int)i ||
                c->state != PJ_ICE_SESS_CHECK_STATE_FROZEN)
            {
                continue;
            }

            /* First pair of this foundation */
            if (chk == NULL) {
                chk = c;
                continue;
            }

            /* Found the lowest comp ID so far */
            if (c->lcand->comp_id < chk->lcand->comp_id) {
                chk = c;
                continue;
            }

            /* Found the lowest comp ID and the highest prio so far */
            if (c->lcand->comp_id == chk->lcand->comp_id &&
                pj_cmp_timestamp(&c->prio, &chk->prio) > 0)
            {
                chk = c;
                continue;
            }
        }

        /* Unfreeze */
        if (chk)
            check_set_state(ice, chk, PJ_ICE_SESS_CHECK_STATE_WAITING, 0);
    }

    /* First, perform all pending triggered checks, simultaneously. */
    rcheck = ice->early_check.next;
    while (rcheck != &ice->early_check) {
        LOG4((ice->obj_name, 
              "Performing delayed triggerred check for component %d",
              rcheck->comp_id));
        pj_log_push_indent();
        handle_incoming_check(ice, rcheck);
        rcheck = rcheck->next;
        pj_log_pop_indent();
    }
    pj_list_init(&ice->early_check);

    /* Start periodic check */
    /* We could start it immediately like below, but lets schedule timer 
     * instead to reduce stack usage:
     * return start_periodic_check(ice->stun_cfg.timer_heap, &clist->timer);
     */
    if (!pj_timer_entry_running(&clist->timer)) {
        pj_time_val delay = {0, 0};
        status = pj_timer_heap_schedule_w_grp_lock(ice->stun_cfg.timer_heap,
                                                   &clist->timer, &delay,
                                                   PJ_TRUE, ice->grp_lock);
    }

    /* For trickle ICE, start timer for end-of-candidates indication from
     * remote.
     */
    if (ice->is_trickling && !pj_timer_entry_running(&ice->timer_end_of_cand))
    {
        pj_time_val delay = {PJ_TRICKLE_ICE_END_OF_CAND_TIMEOUT, 0};
        pj_timer_entry_init(&ice->timer_end_of_cand, 0, ice,
                            &end_of_cand_ind_timer);
        status = pj_timer_heap_schedule_w_grp_lock(
                                                ice->stun_cfg.timer_heap,
                                                &ice->timer_end_of_cand,
                                                &delay, PJ_TRUE,
                                                ice->grp_lock);
        if (status != PJ_SUCCESS) {
            LOG4((ice->obj_name,
                  "Failed to schedule end-of-candidate indication timer"));
        }
    }

    pj_grp_lock_release(ice->grp_lock);
    pj_log_pop_indent();
    return status;
}


//////////////////////////////////////////////////////////////////////////////

/* Callback called by STUN session to send the STUN message.
 * STUN session also doesn't have a transport, remember?!
 */
static pj_status_t on_stun_send_msg(pj_stun_session *sess,
                                    void *token,
                                    const void *pkt,
                                    pj_size_t pkt_size,
                                    const pj_sockaddr_t *dst_addr,
                                    unsigned addr_len)
{
    stun_data *sd = (stun_data*) pj_stun_session_get_user_data(sess);
    pj_ice_sess *ice = sd->ice;
    pj_ice_msg_data *msg_data = (pj_ice_msg_data*) token;
    pj_status_t status;
    
    pj_grp_lock_acquire(ice->grp_lock);

    if (ice->is_destroying) {
        /* Stray retransmit timer that could happen while
         * we're being destroyed */
        pj_grp_lock_release(ice->grp_lock);
        return PJ_EINVALIDOP;
    }

    status = (*ice->cb.on_tx_pkt)(ice, sd->comp_id, msg_data->transport_id,
                                  pkt, pkt_size, dst_addr, addr_len);

    pj_grp_lock_release(ice->grp_lock);
    return status;
}


/* This callback is called when outgoing STUN request completed */
static void on_stun_request_complete(pj_stun_session *stun_sess,
                                     pj_status_t status,
                                     void *token,
                                     pj_stun_tx_data *tdata,
                                     const pj_stun_msg *response,
                                     const pj_sockaddr_t *src_addr,
                                     unsigned src_addr_len)
{
    pj_ice_msg_data *msg_data = (pj_ice_msg_data*) token;
    pj_ice_sess *ice;
    pj_ice_sess_check *check, *new_check;
    pj_ice_sess_cand *lcand;
    pj_ice_sess_checklist *clist;
    pj_stun_xor_mapped_addr_attr *xaddr;
    const pj_sockaddr_t *source_addr = src_addr;
    unsigned i, ckid;

    PJ_UNUSED_ARG(stun_sess);
    PJ_UNUSED_ARG(src_addr_len);

    pj_assert(msg_data->has_req_data);

    ice = msg_data->data.req.ice;
    clist = msg_data->data.req.clist;
    ckid = msg_data->data.req.ckid;
    check = &clist->checks[ckid];

    pj_grp_lock_acquire(ice->grp_lock);

    if (ice->is_destroying) {
        /* Not sure if this is possible but just in case */
        pj_grp_lock_release(ice->grp_lock);
        return;
    }

    /* Check if ICE has been completed */
    if (ice->is_complete) {
        LOG4((ice->obj_name,
              "Ignored completed STUN request after ICE nego has been "
              "completed!"));
        pj_grp_lock_release(ice->grp_lock);
        return;
    }

    /* Verify check (check ID may change as trickle ICE re-sort the list */
    if (tdata != check->tdata) {
        /* Okay, it was re-sorted, lookup using lcand & rcand */
        for (i = 0; i < clist->count; ++i) {
            if (clist->checks[i].lcand == msg_data->data.req.lcand &&
                clist->checks[i].rcand == msg_data->data.req.rcand)
            {
                check = &clist->checks[i];
                ckid = i;
                break;
            }
        }
        if (i == clist->count) {
            /* The check may have been pruned (due to low prio) */
            check->tdata = NULL;
            pj_grp_lock_release(ice->grp_lock);
            return;
        }
    }

    /* Mark STUN transaction as complete */
    // Find 'corner case ...'.
    //pj_assert(tdata == check->tdata);
    check->tdata = NULL;

    /* Init lcand to NULL. lcand will be found from the mapped address
     * found in the response.
     */
    lcand = NULL;

    if (status != PJ_SUCCESS) {
        char errmsg[PJ_ERR_MSG_SIZE];

        if (status==PJ_STATUS_FROM_STUN_CODE(PJ_STUN_SC_ROLE_CONFLICT)) {

            /* Role conclict response.
             *
             * 7.1.2.1.  Failure Cases:
             *
             * If the request had contained the ICE-CONTROLLED attribute, 
             * the agent MUST switch to the controlling role if it has not
             * already done so.  If the request had contained the 
             * ICE-CONTROLLING attribute, the agent MUST switch to the 
             * controlled role if it has not already done so.  Once it has
             * switched, the agent MUST immediately retry the request with
             * the ICE-CONTROLLING or ICE-CONTROLLED attribute reflecting 
             * its new role.
             */
            pj_ice_sess_role new_role = PJ_ICE_SESS_ROLE_UNKNOWN;
            pj_stun_msg *req = tdata->msg;

            if (pj_stun_msg_find_attr(req, PJ_STUN_ATTR_ICE_CONTROLLING, 0)) {
                new_role = PJ_ICE_SESS_ROLE_CONTROLLED;
            } else if (pj_stun_msg_find_attr(req, PJ_STUN_ATTR_ICE_CONTROLLED, 
                                             0)) {
                new_role = PJ_ICE_SESS_ROLE_CONTROLLING;
            } else {
                pj_assert(!"We should have put CONTROLLING/CONTROLLED attr!");
                new_role = PJ_ICE_SESS_ROLE_CONTROLLED;
            }

            if (new_role != ice->role) {
                LOG4((ice->obj_name, 
                      "Changing role because of role conflict response"));
                pj_ice_sess_change_role(ice, new_role);
            }

            /* Resend request */
            LOG4((ice->obj_name, "Resending check because of role conflict"));
            pj_log_push_indent();
            check_set_state(ice, check, PJ_ICE_SESS_CHECK_STATE_WAITING, 0);
            perform_check(ice, clist, ckid,
                          check->nominated || ice->is_nominating);
            pj_log_pop_indent();
            pj_grp_lock_release(ice->grp_lock);
            return;
        }

        pj_strerror(status, errmsg, sizeof(errmsg));
        LOG4((ice->obj_name, 
             "Check %s%s: connectivity check FAILED: %s",
             dump_check(ice->tmp.txt, sizeof(ice->tmp.txt), 
                        &ice->clist, check),
             (check->nominated ? " (nominated)" : " (not nominated)"),
             errmsg));
        pj_log_push_indent();
        check_set_state(ice, check, PJ_ICE_SESS_CHECK_STATE_FAILED, status);
        on_check_complete(ice, check);
        pj_log_pop_indent();
        pj_grp_lock_release(ice->grp_lock);
        return;
    }


    /* 7.1.2.1.  Failure Cases
     *
     * The agent MUST check that the source IP address and port of the
     * response equals the destination IP address and port that the Binding
     * Request was sent to, and that the destination IP address and port of
     * the response match the source IP address and port that the Binding
     * Request was sent from.
     */
    if (check->rcand->addr.addr.sa_family == pj_AF_INET() &&
        ((pj_sockaddr *)src_addr)->addr.sa_family == pj_AF_INET6())
    {
        /* If the address family is different, we need to check
         * whether the two addresses are equivalent (i.e. the IPv6
         * is synthesized from IPv4).
         */
        pj_sockaddr synth_addr;
        
        status = pj_sockaddr_synthesize(pj_AF_INET6(), &synth_addr,
                                        &check->rcand->addr);
        if (status == PJ_SUCCESS &&
            pj_sockaddr_cmp(&synth_addr, src_addr) == 0)
        {
            source_addr = &check->rcand->addr;
        }
    }

    if (pj_sockaddr_cmp(&check->rcand->addr, source_addr) != 0) {
        status = PJNATH_EICEINSRCADDR;
        LOG4((ice->obj_name, 
             "Check %s%s: connectivity check FAILED: source address mismatch",
             dump_check(ice->tmp.txt, sizeof(ice->tmp.txt), 
                        &ice->clist, check),
             (check->nominated ? " (nominated)" : " (not nominated)")));
        pj_log_push_indent();
        check_set_state(ice, check, PJ_ICE_SESS_CHECK_STATE_FAILED, status);
        on_check_complete(ice, check);
        pj_log_pop_indent();
        pj_grp_lock_release(ice->grp_lock);
        return;
    }

    /* 7.1.2.2.  Success Cases
     * 
     * A check is considered to be a success if all of the following are
     * true:
     * 
     * o  the STUN transaction generated a success response
     * 
     * o  the source IP address and port of the response equals the
     *    destination IP address and port that the Binding Request was sent
     *    to
     * 
     * o  the destination IP address and port of the response match the
     *    source IP address and port that the Binding Request was sent from
     */


    LOG4((ice->obj_name, 
         "Check %s%s: connectivity check SUCCESS",
         dump_check(ice->tmp.txt, sizeof(ice->tmp.txt), 
                    &ice->clist, check),
         (check->nominated ? " (nominated)" : " (not nominated)")));

    /* Get the STUN XOR-MAPPED-ADDRESS attribute. */
    xaddr = (pj_stun_xor_mapped_addr_attr*)
            pj_stun_msg_find_attr(response, PJ_STUN_ATTR_XOR_MAPPED_ADDR,0);
    if (!xaddr) {
        check_set_state(ice, check, PJ_ICE_SESS_CHECK_STATE_FAILED, 
                        PJNATH_ESTUNNOMAPPEDADDR);
        on_check_complete(ice, check);
        pj_grp_lock_release(ice->grp_lock);
        return;
    }

    /* Find local candidate that matches the XOR-MAPPED-ADDRESS */
    pj_assert(lcand == NULL);
    for (i=0; i<ice->lcand_cnt; ++i) {
        /* Ticket #1891: apply additional check as there may be a shared
         * mapped address for different base/local addresses.
         */
        if (pj_sockaddr_cmp(&xaddr->sockaddr, &ice->lcand[i].addr) == 0 &&
            pj_sockaddr_cmp(&check->lcand->base_addr,
                            &ice->lcand[i].base_addr) == 0)
        {
            /* Match */
            lcand = &ice->lcand[i];

#if 0
            // The following code tries to verify if the STUN request belongs
            // to the correct ICE check (so if it doesn't, it will set current
            // ICE check state to FAILED (why?) and try to find the correct
            // check). However, ICE check verification has been added in
            // the beginning of this function, so the following block should
            // not be needed anymore.

            /* Verify lcand==check->lcand, this may happen when a STUN socket
             * corresponds to multiple host candidates.
             */
            if (lcand != check->lcand) {
                unsigned j;

                pj_log_push_indent();
                LOG4((ice->obj_name,
                     "Check %s%s: local candidate mismatch",
                     dump_check(ice->tmp.txt, sizeof(ice->tmp.txt),
                                &ice->clist, check),
                     (check->nominated ? " (nominated)" : " (not nominated)")));


                /* Local candidate does not belong to this check! Set current
                 * check state to Failed.
                 */
                check_set_state(ice, check, PJ_ICE_SESS_CHECK_STATE_FAILED,
                                PJNATH_ESTUNNOMAPPEDADDR);

                /* Find the matching check */
                for (j = 0; j < clist->count; ++j) {
                    if (clist->checks[j].lcand == lcand &&
                        clist->checks[j].rcand == check->rcand)
                    {
                        check = &clist->checks[j];
                        break;
                    }
                }
                if (j == clist->count) {
                    on_check_complete(ice, check);
                    pj_log_pop_indent();
                    pj_grp_lock_release(ice->grp_lock);
                    return;
                }

                pj_log_pop_indent();
            }
#endif
            break;
        }
    }

    /* 7.1.2.2.1.  Discovering Peer Reflexive Candidates
     * If the transport address returned in XOR-MAPPED-ADDRESS does not match
     * any of the local candidates that the agent knows about, the mapped 
     * address represents a new candidate - a peer reflexive candidate.
     */
    if (lcand == NULL) {
        unsigned cand_id = ice->lcand_cnt;
        pj_str_t foundation;

        pj_ice_calc_foundation(ice->pool, &foundation, PJ_ICE_CAND_TYPE_PRFLX,
                               &check->lcand->base_addr);

        /* Still in 7.1.2.2.1.  Discovering Peer Reflexive Candidates
         * Its priority is set equal to the value of the PRIORITY attribute
         * in the Binding Request.
         *
         * I think the priority calculated by add_cand() should be the same
         * as the one calculated in perform_check(), so there's no need to
         * get the priority from the PRIORITY attribute.
         */

        /* Add new peer reflexive candidate */
        status = pj_ice_sess_add_cand(ice, check->lcand->comp_id,
                                      msg_data->transport_id,
                                      PJ_ICE_CAND_TYPE_PRFLX,
#if PJNATH_ICE_PRIO_STD
                                      65535 - (pj_uint16_t)ice->lcand_cnt,
#else
                                      ((1 << PJ_ICE_LOCAL_PREF_BITS) - 1) -
                                      ice->lcand_cnt,
#endif
                                      &foundation,
                                      &xaddr->sockaddr,
                                      &check->lcand->base_addr,
                                      &check->lcand->base_addr,
                                      pj_sockaddr_get_len(&xaddr->sockaddr),
                                      &cand_id);
        // Note: for IPv6, pj_ice_sess_add_cand can return SUCCESS
        // without adding any candidates if the candidate is
        // deprecated (because the ICE MUST NOT fail)
        // In this case, cand_id == ice->lcand_cnt will be true.
        if (status != PJ_SUCCESS || cand_id == ice->lcand_cnt) {
            if (cand_id == ice->lcand_cnt) {
                LOG4((ice->obj_name,
                  "Cannot add any candidate, all IPv6 seems deprecated"));
            }
            check_set_state(ice, check, PJ_ICE_SESS_CHECK_STATE_FAILED,
                            status);
            on_check_complete(ice, check);
            pj_grp_lock_release(ice->grp_lock);
            return;
        }

        /* Update local candidate */
        lcand = &ice->lcand[cand_id];
    }

    /* 7.1.2.2.3.  Constructing a Valid Pair
     * Next, the agent constructs a candidate pair whose local candidate
     * equals the mapped address of the response, and whose remote candidate
     * equals the destination address to which the request was sent.    
     */

    /* Add pair to valid list, if it's not there, otherwise just update
     * nominated flag
     */
    for (i=0; i<ice->valid_list.count; ++i) {
        if (ice->valid_list.checks[i].lcand == lcand &&
            ice->valid_list.checks[i].rcand == check->rcand)
            break;
    }

    if (i==ice->valid_list.count) {
        pj_assert(ice->valid_list.count < PJ_ICE_MAX_CHECKS);
        new_check = &ice->valid_list.checks[ice->valid_list.count++];
        new_check->lcand = lcand;
        new_check->rcand = check->rcand;
        new_check->prio = CALC_CHECK_PRIO(ice, lcand, check->rcand);
        new_check->state = PJ_ICE_SESS_CHECK_STATE_SUCCEEDED;
        new_check->nominated = check->nominated;
        new_check->err_code = PJ_SUCCESS;
    } else {
        new_check = &ice->valid_list.checks[i];
        ice->valid_list.checks[i].nominated = check->nominated;
    }

    /* Update valid check and nominated check for the component */
    update_comp_check(ice, new_check->lcand->comp_id, new_check);

    /* Sort valid_list (must do so after update_comp_check(), otherwise
     * new_check will point to something else (#953)
     */
    sort_checklist(ice, &ice->valid_list);

    /* 7.1.2.2.2.  Updating Pair States
     * 
     * The agent sets the state of the pair that generated the check to
     * Succeeded.  The success of this check might also cause the state of
     * other checks to change as well.
     */
    check_set_state(ice, check, PJ_ICE_SESS_CHECK_STATE_SUCCEEDED, 
                    PJ_SUCCESS);

    /* Perform 7.1.2.2.2.  Updating Pair States.
     * This may terminate ICE processing.
     */
    if (on_check_complete(ice, check)) {
        /* ICE complete! */
        pj_grp_lock_release(ice->grp_lock);
        return;
    }

    pj_grp_lock_release(ice->grp_lock);
}


/* This callback is called by the STUN session associated with a candidate
 * when it receives incoming request.
 */
static pj_status_t on_stun_rx_request(pj_stun_session *sess,
                                      const pj_uint8_t *pkt,
                                      unsigned pkt_len,
                                      const pj_stun_rx_data *rdata,
                                      void *token,
                                      const pj_sockaddr_t *src_addr,
                                      unsigned src_addr_len)
{
    stun_data *sd;
    const pj_stun_msg *msg = rdata->msg;
    pj_ice_msg_data *msg_data;
    pj_ice_sess *ice;
    pj_stun_priority_attr *prio_attr;
    pj_stun_use_candidate_attr *uc_attr;
    pj_stun_uint64_attr *role_attr;
    pj_stun_tx_data *tdata;
    pj_ice_rx_check *rcheck, tmp_rcheck;
    const pj_sockaddr_t *source_addr = src_addr;
    unsigned source_addr_len = src_addr_len;
    pj_status_t status;

    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(pkt_len);
    
    /* Reject any requests except Binding request */
    if (msg->hdr.type != PJ_STUN_BINDING_REQUEST) {
        pj_stun_session_respond(sess, rdata, PJ_STUN_SC_BAD_REQUEST, 
                                NULL, token, PJ_TRUE, 
                                src_addr, src_addr_len);
        return PJ_SUCCESS;
    }


    sd = (stun_data*) pj_stun_session_get_user_data(sess);
    ice = sd->ice;

    pj_grp_lock_acquire(ice->grp_lock);

    if (ice->is_destroying) {
        pj_grp_lock_release(ice->grp_lock);
        return PJ_EINVALIDOP;
    }

    /*
     * Note:
     *  Be aware that when STUN request is received, we might not get
     *  SDP answer yet, so we might not have remote candidates and
     *  checklist yet. This case will be handled after we send
     *  a response.
     */

    /* Get PRIORITY attribute */
    prio_attr = (pj_stun_priority_attr*)
                pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_PRIORITY, 0);
    if (prio_attr == NULL) {
        LOG5((ice->obj_name, "Received Binding request with no PRIORITY"));
        pj_grp_lock_release(ice->grp_lock);
        return PJ_SUCCESS;
    }

    /* Get USE-CANDIDATE attribute */
    uc_attr = (pj_stun_use_candidate_attr*)
              pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_USE_CANDIDATE, 0);


    /* Get ICE-CONTROLLING or ICE-CONTROLLED */
    role_attr = (pj_stun_uint64_attr*)
                pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_ICE_CONTROLLING, 0);
    if (role_attr == NULL) {
        role_attr = (pj_stun_uint64_attr*)
                    pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_ICE_CONTROLLED, 0);
    }

    /* Handle the case when request comes before SDP answer is received.
     * We need to put credential in the response, and since we haven't
     * got the SDP answer, copy the username from the request.
     */
    if (ice->tx_ufrag.slen == 0) {
        pj_stun_string_attr *uname_attr;

        uname_attr = (pj_stun_string_attr*)
                     pj_stun_msg_find_attr(msg, PJ_STUN_ATTR_USERNAME, 0);
        pj_assert(uname_attr != NULL);
        pj_strdup(ice->pool, &ice->rx_uname, &uname_attr->value);
    }

    /* 7.2.1.1.  Detecting and Repairing Role Conflicts
     */
    if (ice->role == PJ_ICE_SESS_ROLE_CONTROLLING &&
        role_attr && role_attr->hdr.type == PJ_STUN_ATTR_ICE_CONTROLLING)
    {
        if (pj_cmp_timestamp(&ice->tie_breaker, &role_attr->value) < 0) {
            /* Switch role to controlled */
            LOG4((ice->obj_name, 
                  "Changing role because of ICE-CONTROLLING attribute"));
            pj_ice_sess_change_role(ice, PJ_ICE_SESS_ROLE_CONTROLLED);
        } else {
            /* Generate 487 response */
            pj_stun_session_respond(sess, rdata, PJ_STUN_SC_ROLE_CONFLICT, 
                                    NULL, token, PJ_TRUE, 
                                    src_addr, src_addr_len);
            pj_grp_lock_release(ice->grp_lock);
            return PJ_SUCCESS;
        }

    } else if (ice->role == PJ_ICE_SESS_ROLE_CONTROLLED &&
               role_attr && role_attr->hdr.type == PJ_STUN_ATTR_ICE_CONTROLLED)
    {
        if (pj_cmp_timestamp(&ice->tie_breaker, &role_attr->value) < 0) {
            /* Generate 487 response */
            pj_stun_session_respond(sess, rdata, PJ_STUN_SC_ROLE_CONFLICT, 
                                    NULL, token, PJ_TRUE, 
                                    src_addr, src_addr_len);
            pj_grp_lock_release(ice->grp_lock);
            return PJ_SUCCESS;
        } else {
            /* Switch role to controlled */
            LOG4((ice->obj_name, 
                  "Changing role because of ICE-CONTROLLED attribute"));
            pj_ice_sess_change_role(ice, PJ_ICE_SESS_ROLE_CONTROLLING);
        }
    }

    /* 
     * First send response to this request 
     */
    status = pj_stun_session_create_res(sess, rdata, 0, NULL, &tdata);
    if (status != PJ_SUCCESS) {
        pj_grp_lock_release(ice->grp_lock);
        return status;
    }

    if (((pj_sockaddr *)src_addr)->addr.sa_family == pj_AF_INET6()) {
        unsigned i;
        unsigned transport_id = ((pj_ice_msg_data*)token)->transport_id;
        pj_ice_sess_cand *lcand = NULL;

        for (i = 0; i < ice->clist.count; ++i) {
            pj_ice_sess_check *c = &ice->clist.checks[i];
            if (c->lcand->comp_id == sd->comp_id &&
                c->lcand->transport_id == transport_id) 
            {
                lcand = c->lcand;
                break;
            }
        }

        if (lcand != NULL && lcand->addr.addr.sa_family == pj_AF_INET()) {
            /* We are behind NAT64, so src_addr is a synthesized IPv6
             * address. Instead of putting this synth IPv6 address as
             * the XOR-MAPPED-ADDRESS, we need to find its original
             * IPv4 address.
             */
            for (i = 0; i < ice->rcand_cnt; ++i) {
                pj_sockaddr synth_addr;
            
                if (ice->rcand[i].addr.addr.sa_family != pj_AF_INET())
                    continue;

                status = pj_sockaddr_synthesize(pj_AF_INET6(), &synth_addr,
                                                &ice->rcand[i].addr);
                if (status == PJ_SUCCESS &&
                    pj_sockaddr_cmp(src_addr, &synth_addr) == 0)
                {
                    /* We find the original IPv4 address. */
                    source_addr = &ice->rcand[i].addr;
                    source_addr_len = pj_sockaddr_get_len(source_addr);
                    break;
                }
            }
        }
    }


    /* Add XOR-MAPPED-ADDRESS attribute */
    status = pj_stun_msg_add_sockaddr_attr(tdata->pool, tdata->msg, 
                                           PJ_STUN_ATTR_XOR_MAPPED_ADDR,
                                           PJ_TRUE, source_addr,
                                           source_addr_len);

    /* Create a msg_data to be associated with this response */
    msg_data = PJ_POOL_ZALLOC_T(tdata->pool, pj_ice_msg_data);
    msg_data->transport_id = ((pj_ice_msg_data*)token)->transport_id;
    msg_data->has_req_data = PJ_FALSE;

    /* Send the response */
    status = pj_stun_session_send_msg(sess, msg_data, PJ_TRUE, PJ_TRUE,
                                      src_addr, src_addr_len, tdata);


    /* 
     * Handling early check.
     *
     * It's possible that we receive this request before we receive SDP
     * answer. In this case, we can't perform trigger check since we
     * don't have checklist yet, so just save this check in a pending
     * triggered check array to be acted upon later.
     */
    if (ice->tx_ufrag.slen == 0) {
        rcheck = PJ_POOL_ZALLOC_T(ice->pool, pj_ice_rx_check);
    } else {
        rcheck = &tmp_rcheck;
    }

    /* Init rcheck */
    rcheck->comp_id = sd->comp_id;
    rcheck->transport_id = ((pj_ice_msg_data*)token)->transport_id;
    rcheck->src_addr_len = source_addr_len;
    pj_sockaddr_cp(&rcheck->src_addr, source_addr);
    rcheck->use_candidate = (uc_attr != NULL);
    rcheck->priority = prio_attr->value;
    rcheck->role_attr = role_attr;

    if (ice->tx_ufrag.slen == 0) {
        /* We don't have answer yet, so keep this request for later */
        LOG4((ice->obj_name, "Received an early check for comp %d",
              rcheck->comp_id));
        pj_list_push_back(&ice->early_check, rcheck);
    } else {
        /* Handle this check */
        handle_incoming_check(ice, rcheck);
    }

    pj_grp_lock_release(ice->grp_lock);
    return PJ_SUCCESS;
}


/* Handle incoming Binding request and perform triggered check.
 * This function may be called by on_stun_rx_request(), or when
 * SDP answer is received and we have received early checks.
 */
static void handle_incoming_check(pj_ice_sess *ice,
                                  const pj_ice_rx_check *rcheck)
{
    pj_ice_sess_comp *comp;
    pj_ice_sess_cand *lcand = NULL;
    pj_ice_sess_cand *rcand;
    unsigned i;

    /* Check if ICE has been completed */
    if (ice->is_complete) {
        LOG4((ice->obj_name,
              "Ignored incoming check after ICE nego has been completed!"));
        return;
    }

    comp = find_comp(ice, rcheck->comp_id);

    /* Find remote candidate based on the source transport address of 
     * the request.
     */
    for (i=0; i<ice->rcand_cnt; ++i) {
        if (pj_sockaddr_cmp(&rcheck->src_addr, &ice->rcand[i].addr)==0)
            break;
    }

    /* 7.2.1.3.  Learning Peer Reflexive Candidates
     * If the source transport address of the request does not match any
     * existing remote candidates, it represents a new peer reflexive remote
     * candidate.
     */
    if (i == ice->rcand_cnt) {
        char raddr[PJ_INET6_ADDRSTRLEN];
        void *p;

        if (ice->rcand_cnt >= PJ_ICE_MAX_CAND) {
            LOG4((ice->obj_name, 
                  "Unable to add new peer reflexive candidate: too many "
                  "candidates already (%d)", PJ_ICE_MAX_CAND));
            return;
        }

        rcand = &ice->rcand[ice->rcand_cnt++];
        rcand->comp_id = (pj_uint8_t)rcheck->comp_id;
        rcand->type = PJ_ICE_CAND_TYPE_PRFLX;
        rcand->prio = rcheck->priority;
        pj_sockaddr_cp(&rcand->addr, &rcheck->src_addr);

        /* Foundation is random, unique from other foundation */
        rcand->foundation.ptr = p = (char*) pj_pool_alloc(ice->pool, 36);
        rcand->foundation.slen = pj_ansi_snprintf(rcand->foundation.ptr, 36,
                                                  "f%p", p);

        LOG4((ice->obj_name, 
              "Added new remote candidate from the request: %s:%d",
              pj_sockaddr_print(&rcand->addr, raddr, sizeof(raddr), 2),
              pj_sockaddr_get_port(&rcand->addr)));

    } else {
        /* Remote candidate found */
        rcand = &ice->rcand[i];
    }

#if 0
    /* Find again the local candidate by matching the base address
     * with the local candidates in the checklist. Checks may have
     * been pruned before, so it's possible that if we use the lcand
     * as it is, we wouldn't be able to find the check in the checklist
     * and we will end up creating a new check unnecessarily.
     */
    for (i=0; i<ice->clist.count; ++i) {
        pj_ice_sess_check *c = &ice->clist.checks[i];
        if (/*c->lcand == lcand ||*/
            pj_sockaddr_cmp(&c->lcand->base_addr, &lcand->base_addr)==0)
        {
            lcand = c->lcand;
            break;
        }
    }
#else
    /* Just get candidate with the highest priority and same transport ID
     * for the specified  component ID in the checklist.
     */
    for (i=0; i<ice->clist.count; ++i) {
        pj_ice_sess_check *c = &ice->clist.checks[i];
        if (c->lcand->comp_id == rcheck->comp_id &&
            c->lcand->transport_id == rcheck->transport_id) 
        {
            lcand = c->lcand;
            break;
        }
    }
    if (lcand == NULL) {
        /* Should not happen, but just in case remote is sending a
         * Binding request for a component which it doesn't have.
         */
        LOG4((ice->obj_name, 
             "Received Binding request but no local candidate is found!"));
        return;
    }
#endif

    /* 
     * Create candidate pair for this request. 
     */

    /* 
     * 7.2.1.4.  Triggered Checks
     *
     * Now that we have local and remote candidate, check if we already
     * have this pair in our checklist.
     */
    for (i=0; i<ice->clist.count; ++i) {
        pj_ice_sess_check *c = &ice->clist.checks[i];
        if (c->lcand == lcand && c->rcand == rcand)
            break;
    }

    /* If the pair is already on the check list:
     * - If the state of that pair is Waiting or Frozen, its state is
     *   changed to In-Progress and a check for that pair is performed
     *   immediately.  This is called a triggered check.
     *
     * - If the state of that pair is In-Progress, the agent SHOULD
     *   generate an immediate retransmit of the Binding Request for the
     *   check in progress.  This is to facilitate rapid completion of
     *   ICE when both agents are behind NAT.
     * 
     * - If the state of that pair is Failed or Succeeded, no triggered
     *   check is sent.
     */
    if (i != ice->clist.count) {
        pj_ice_sess_check *c = &ice->clist.checks[i];

        /* If USE-CANDIDATE is present, set nominated flag 
         * Note: DO NOT overwrite nominated flag if one is already set.
         */
        c->nominated = ((rcheck->use_candidate) || c->nominated);

        if (c->state == PJ_ICE_SESS_CHECK_STATE_FROZEN ||
            c->state == PJ_ICE_SESS_CHECK_STATE_WAITING)
        {
            /* If we are nominating in regular nomination, don't nominate this
             * triggered check immediately, just wait for its scheduled check.
             */
            if (ice->is_nominating && !ice->opt.aggressive) {
                LOG5((ice->obj_name, "Triggered check for check %d not "
                      "performed because nomination is in progress", i));
            } else {
                /* See if we shall nominate this check */
                pj_bool_t nominate = (c->nominated || ice->is_nominating);

                LOG5((ice->obj_name, "Performing triggered check for "
                      "check %d",i));
                pj_log_push_indent();
                perform_check(ice, &ice->clist, i, nominate);
                pj_log_pop_indent();
            }
        } else if (c->state == PJ_ICE_SESS_CHECK_STATE_IN_PROGRESS) {
            /* Should retransmit immediately
             */
            LOG5((ice->obj_name, "Triggered check for check %d not performed "
                  "because it's in progress. Retransmitting", i));
            pj_log_push_indent();
            pj_stun_session_retransmit_req(comp->stun_sess, c->tdata, PJ_FALSE);
            pj_log_pop_indent();

        } else if (c->state == PJ_ICE_SESS_CHECK_STATE_SUCCEEDED) {
            /* Check complete for this component.
             * Note this may end ICE process.
             */
            pj_bool_t complete;
            unsigned j;

            /* If this check is nominated, scan the valid_list for the
             * same check and update the nominated flag. A controlled 
             * agent might have finished the check earlier.
             */
            if (rcheck->use_candidate) {
                for (j=0; j<ice->valid_list.count; ++j) {
                    pj_ice_sess_check *vc = &ice->valid_list.checks[j];
                    if (vc->lcand->transport_id == c->lcand->transport_id && 
                        vc->rcand == c->rcand) 
                    {
                        /* Set nominated flag */
                        vc->nominated = PJ_TRUE;

                        /* Update valid check and nominated check for the component */
                        update_comp_check(ice, vc->lcand->comp_id, vc);

                        LOG5((ice->obj_name, "Valid check %s is nominated", 
                              dump_check(ice->tmp.txt, sizeof(ice->tmp.txt), 
                                         &ice->valid_list, vc)));
                    }
                }
            }

            LOG5((ice->obj_name, "Triggered check for check %d not performed "
                                "because it's completed", i));
            pj_log_push_indent();
            complete = on_check_complete(ice, c);
            pj_log_pop_indent();
            if (complete) {
                return;
            }
        }

    }
    /* If the pair is not already on the check list:
     * - The pair is inserted into the check list based on its priority.
     * - Its state is set to In-Progress
     * - A triggered check for that pair is performed immediately.
     */
    /* Note: only do this if we don't have too many checks in checklist */
    else if (ice->clist.count < PJ_ICE_MAX_CHECKS) {

        pj_ice_sess_check *c = &ice->clist.checks[ice->clist.count];
        unsigned check_id = ice->clist.count;

        c->lcand = lcand;
        c->rcand = rcand;
        c->prio = CALC_CHECK_PRIO(ice, lcand, rcand);
        c->state = PJ_ICE_SESS_CHECK_STATE_WAITING;
        c->nominated = rcheck->use_candidate;
        c->err_code = PJ_SUCCESS;
        ++ice->clist.count;

        LOG4((ice->obj_name, "New triggered check added: %d", check_id));

        /* If we are nominating in regular nomination, don't nominate this
         * newly found pair.
         */
        if (ice->is_nominating && !ice->opt.aggressive) {
            LOG5((ice->obj_name, "Triggered check for check %d not "
                  "performed because nomination is in progress", check_id));

            /* Just in case the periodic check has been stopped (due to no more
             * pair to check), let's restart it for this pair.
             */
            if (!pj_timer_entry_running(&ice->clist.timer)) {
                pj_time_val delay = {0, 0};
                pj_timer_heap_schedule_w_grp_lock(ice->stun_cfg.timer_heap,
                                                  &ice->clist.timer, &delay,
                                                  PJ_TRUE, ice->grp_lock);
            }
        } else {
            pj_bool_t nominate;
            nominate = (c->nominated || ice->is_nominating);

            pj_log_push_indent();
            perform_check(ice, &ice->clist, check_id, nominate);
            pj_log_pop_indent();
        }

        /* Re-sort the list because of the newly added pair. */
        sort_checklist(ice, &ice->clist);

    } else {
        LOG4((ice->obj_name, "Error: unable to perform triggered check: "
             "TOO MANY CHECKS IN CHECKLIST!"));
    }
}


static pj_status_t on_stun_rx_indication(pj_stun_session *sess,
                                         const pj_uint8_t *pkt,
                                         unsigned pkt_len,
                                         const pj_stun_msg *msg,
                                         void *token,
                                         const pj_sockaddr_t *src_addr,
                                         unsigned src_addr_len)
{
    struct stun_data *sd;

    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(pkt_len);
    PJ_UNUSED_ARG(msg);
    PJ_UNUSED_ARG(token);
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    sd = (struct stun_data*) pj_stun_session_get_user_data(sess);

    pj_log_push_indent();

    if (msg->hdr.type == PJ_STUN_BINDING_INDICATION) {
        LOG5((sd->ice->obj_name, "Received Binding Indication keep-alive "
              "for component %d", sd->comp_id));
    } else {
        LOG4((sd->ice->obj_name, "Received unexpected %s indication "
              "for component %d", pj_stun_get_method_name(msg->hdr.type), 
              sd->comp_id));
    }

    pj_log_pop_indent();

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_ice_sess_send_data(pj_ice_sess *ice,
                                          unsigned comp_id,
                                          const void *data,
                                          pj_size_t data_len)
{
    pj_status_t status = PJ_SUCCESS;
    pj_ice_sess_comp *comp;
    pj_ice_sess_cand *cand;
    pj_uint8_t transport_id;
    pj_sockaddr addr;

    PJ_ASSERT_RETURN(ice && comp_id, PJ_EINVAL);
    
    /* It is possible that comp_cnt is less than comp_id, when remote
     * doesn't support all the components that we have.
     */
    if (comp_id > ice->comp_cnt) {
        return PJNATH_EICEINCOMPID;
    }

    pj_grp_lock_acquire(ice->grp_lock);

    if (ice->is_destroying) {
        pj_grp_lock_release(ice->grp_lock);
        return PJ_EINVALIDOP;
    }

    comp = find_comp(ice, comp_id);
    if (comp == NULL) {
        status = PJNATH_EICEINCOMPID;
        pj_grp_lock_release(ice->grp_lock);
        goto on_return;
    }

    if (comp->valid_check == NULL) {
        status = PJNATH_EICEINPROGRESS;
        pj_grp_lock_release(ice->grp_lock);
        goto on_return;
    }

    cand = comp->valid_check->lcand;
    transport_id = cand->transport_id;
    pj_sockaddr_cp(&addr, &comp->valid_check->rcand->addr);

    /* Release the mutex now to avoid deadlock (see ticket #1451). */
    pj_grp_lock_release(ice->grp_lock);

    PJ_RACE_ME(5);

    status = (*ice->cb.on_tx_pkt)(ice, comp_id, transport_id, 
                                  data, data_len, 
                                  &addr, 
                                  pj_sockaddr_get_len(&addr));

on_return:
    return status;
}


PJ_DEF(pj_status_t) pj_ice_sess_on_rx_pkt(pj_ice_sess *ice,
                                          unsigned comp_id,
                                          unsigned transport_id,
                                          void *pkt,
                                          pj_size_t pkt_size,
                                          const pj_sockaddr_t *src_addr,
                                          int src_addr_len)
{
    pj_status_t status = PJ_SUCCESS;
    pj_ice_sess_comp *comp;
    pj_ice_msg_data *msg_data = NULL;
    unsigned i;

    PJ_ASSERT_RETURN(ice, PJ_EINVAL);

    pj_grp_lock_acquire(ice->grp_lock);

    if (ice->is_destroying) {
        pj_grp_lock_release(ice->grp_lock);
        return PJ_EINVALIDOP;
    }

    comp = find_comp(ice, comp_id);
    if (comp == NULL) {
        pj_grp_lock_release(ice->grp_lock);
        return PJNATH_EICEINCOMPID;
    }

    /* Find transport */
    for (i=0; i<PJ_ARRAY_SIZE(ice->tp_data); ++i) {
        if (ice->tp_data[i].transport_id == transport_id) {
            msg_data = &ice->tp_data[i];
            break;
        }
    }
    if (msg_data == NULL) {
        pj_assert(!"Invalid transport ID");
        pj_grp_lock_release(ice->grp_lock);
        return PJ_EINVAL;
    }

    /* Don't check fingerprint. We only need to distinguish STUN and non-STUN
     * packets. We don't need to verify the STUN packet too rigorously, that
     * will be done by the user.
     */
    status = pj_stun_msg_check((const pj_uint8_t*)pkt, pkt_size, 
                               PJ_STUN_IS_DATAGRAM |
                                 PJ_STUN_NO_FINGERPRINT_CHECK);
    if (status == PJ_SUCCESS) {
        status = pj_stun_session_on_rx_pkt(comp->stun_sess, pkt, pkt_size,
                                           PJ_STUN_IS_DATAGRAM, msg_data,
                                           NULL, src_addr, src_addr_len);
        if (status != PJ_SUCCESS) {
            pj_strerror(status, ice->tmp.errmsg, sizeof(ice->tmp.errmsg));
            LOG4((ice->obj_name, "Error processing incoming message: %s",
                  ice->tmp.errmsg));
        }
        pj_grp_lock_release(ice->grp_lock);
    } else {
        /* Not a STUN packet. Call application's callback instead, but release
         * the mutex now or otherwise we may get deadlock.
         */
        pj_grp_lock_release(ice->grp_lock);

        PJ_RACE_ME(5);

        (*ice->cb.on_rx_data)(ice, comp_id, transport_id, pkt, pkt_size, 
                              src_addr, src_addr_len);
        status = PJ_SUCCESS;
    }

    return status;
}


