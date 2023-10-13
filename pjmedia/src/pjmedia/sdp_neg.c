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
#include <pjmedia/sdp_neg.h>
#include <pjmedia/sdp.h>
#include <pjmedia/codec.h>
#include <pjmedia/vid_codec.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/ctype.h>
#include <pj/array.h>
#include <pj/log.h>

#define THIS_FILE           "sdp_neg.c"

#define START_DYNAMIC_PT    PJMEDIA_RTP_PT_DYNAMIC
#define DYNAMIC_PT_SIZE     127 - START_DYNAMIC_PT + 1
#define UNKNOWN_CODEC       99

/* Array for mapping PT number to codec ID. */
typedef pj_int8_t pt_to_codec_map[DYNAMIC_PT_SIZE];

/* Array for mapping codec ID to PT number. */
typedef pj_int8_t codec_to_pt_map[PJMEDIA_CODEC_MGR_MAX_CODECS];

/**
 * This structure describes SDP media negotiator.
 */
struct pjmedia_sdp_neg
{
    pjmedia_sdp_neg_state state;            /**< Negotiator state.           */
    pj_bool_t             prefer_remote_codec_order;
    pj_bool_t             answer_with_multiple_codecs;
    pj_bool_t             has_remote_answer;
    pj_bool_t             answer_was_remote;

    pt_to_codec_map       pt_to_codec[PJMEDIA_MAX_SDP_MEDIA];
    codec_to_pt_map       codec_to_pt[PJMEDIA_MAX_SDP_MEDIA];

    pj_int8_t             aud_dyn_codecs_cnt;
    pj_str_t              aud_dyn_codecs[PJMEDIA_CODEC_MGR_MAX_CODECS];
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    pj_int8_t             vid_dyn_codecs_cnt;
    pj_str_t              vid_dyn_codecs[PJMEDIA_CODEC_MGR_MAX_CODECS];
#endif

    pjmedia_sdp_session *initial_sdp,       /**< Initial local SDP           */
                        *initial_sdp_tmp,   /**< Temporary initial local SDP */
                        *active_local_sdp,  /**< Currently active local SDP. */
                        *active_remote_sdp, /**< Currently active remote's.  */
                        *neg_local_sdp,     /**< Temporary local SDP.        */
                        *neg_remote_sdp,    /**< Temporary remote SDP.       */
                        *last_sent;         /**< Last sent local SDP.
                                                 Note that application might
                                                 change the actual SDP sent
                                                 using PJSIP module.         */
    pj_pool_t *pool_active;                 /**< Pool used by active SDPs, used
                                                 for retaining last_sent.    */
};

static const char *state_str[] = 
{
    "STATE_NULL",
    "STATE_LOCAL_OFFER",
    "STATE_REMOTE_OFFER",
    "STATE_WAIT_NEGO",
    "STATE_DONE",
};

/* Definition of customized SDP format negotiation callback */
struct fmt_match_cb_t
{
    pj_str_t                        fmt_name;
    pjmedia_sdp_neg_fmt_match_cb    cb;
};

/* Number of registered customized SDP format negotiation callbacks */
static unsigned fmt_match_cb_cnt;

/* The registered customized SDP format negotiation callbacks */
static struct fmt_match_cb_t 
              fmt_match_cb[PJMEDIA_SDP_NEG_MAX_CUSTOM_FMT_NEG_CB];

/* Redefining a very long identifier name, just for convenience */
#define ALLOW_MODIFY_ANSWER PJMEDIA_SDP_NEG_FMT_MATCH_ALLOW_MODIFY_ANSWER

static pj_status_t custom_fmt_match( pj_pool_t *pool,
                                   const pj_str_t *fmt_name,
                                   pjmedia_sdp_media *offer,
                                   unsigned o_fmt_idx,
                                   pjmedia_sdp_media *answer,
                                   unsigned a_fmt_idx,
                                   unsigned option);

static pj_status_t assign_pt_and_update_map(pj_pool_t *pool,
                                            pjmedia_sdp_neg* neg,
                                            pjmedia_sdp_session* sdp_sess,
                                            pj_bool_t is_offer,
                                            pj_bool_t update_only);

/* Init PT number mapping variables. */
static void init_mapping(pjmedia_sdp_neg *neg)
{
    /* Get the audio & video codecs with dynamic PT */
    neg->aud_dyn_codecs_cnt = PJ_ARRAY_SIZE(neg->aud_dyn_codecs);
    pjmedia_codec_mgr_get_dyn_codecs(NULL, &neg->aud_dyn_codecs_cnt,
                                     neg->aud_dyn_codecs);
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
    neg->vid_dyn_codecs_cnt = PJ_ARRAY_SIZE(neg->vid_dyn_codecs);
    pjmedia_vid_codec_mgr_get_dyn_codecs(NULL, &neg->vid_dyn_codecs_cnt,
                                         neg->vid_dyn_codecs);
#endif

    pj_memset(neg->pt_to_codec, -1, PJ_ARRAY_SIZE(neg->pt_to_codec) *
              PJ_ARRAY_SIZE(neg->pt_to_codec[0]));
    pj_bzero(neg->codec_to_pt, PJ_ARRAY_SIZE(neg->codec_to_pt) *
             PJ_ARRAY_SIZE(neg->codec_to_pt[0]));
}

/*
 * Get string representation of negotiator state.
 */
PJ_DEF(const char*) pjmedia_sdp_neg_state_str(pjmedia_sdp_neg_state state)
{
    if ((int)state >=0 && state < (pjmedia_sdp_neg_state)PJ_ARRAY_SIZE(state_str))
        return state_str[state];

    return "<?UNKNOWN?>";
}


/*
 * Create with local offer.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_create_w_local_offer( pj_pool_t *pool,
                                      const pjmedia_sdp_session *local,
                                      pjmedia_sdp_neg **p_neg)
{
    pjmedia_sdp_neg *neg;
    pj_status_t status;

    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && local && p_neg, PJ_EINVAL);

    *p_neg = NULL;

    /* Validate local offer. */
    status = pjmedia_sdp_validate(local);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

    /* Create and initialize negotiator. */
    neg = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_neg);
    PJ_ASSERT_RETURN(neg != NULL, PJ_ENOMEM);

    neg->state = PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER;
    neg->prefer_remote_codec_order = PJMEDIA_SDP_NEG_PREFER_REMOTE_CODEC_ORDER;
    neg->answer_with_multiple_codecs = PJMEDIA_SDP_NEG_ANSWER_MULTIPLE_CODECS;
    neg->initial_sdp = pjmedia_sdp_session_clone(pool, local);
    neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, local);
    neg->last_sent = neg->initial_sdp;

    /* Init last_sent's pool to app's pool, will be updated to active
     * SDPs pool after a successful SDP nego.
     */
    neg->pool_active = pool;

    /* Init PT number mapping variables. */
    init_mapping(neg);

    *p_neg = neg;
    return PJ_SUCCESS;
}

/*
 * Create with remote offer and initial local offer/answer.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_create_w_remote_offer(pj_pool_t *pool,
                                      const pjmedia_sdp_session *initial,
                                      const pjmedia_sdp_session *remote,
                                      pjmedia_sdp_neg **p_neg)
{
    pjmedia_sdp_neg *neg;
    pj_status_t status;

    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && remote && p_neg, PJ_EINVAL);

    *p_neg = NULL;

    /* Validate remote offer and initial answer */
    status = pjmedia_sdp_validate2(remote, PJ_FALSE);
    if (status != PJ_SUCCESS)
        return status;

    /* Create and initialize negotiator. */
    neg = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_neg);
    PJ_ASSERT_RETURN(neg != NULL, PJ_ENOMEM);

    neg->prefer_remote_codec_order = PJMEDIA_SDP_NEG_PREFER_REMOTE_CODEC_ORDER;
    neg->answer_with_multiple_codecs = PJMEDIA_SDP_NEG_ANSWER_MULTIPLE_CODECS;
    neg->neg_remote_sdp = pjmedia_sdp_session_clone(pool, remote);

    if (initial) {
        status = pjmedia_sdp_validate(initial);
        PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

        neg->initial_sdp = pjmedia_sdp_session_clone(pool, initial);
        neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, initial);

        neg->state = PJMEDIA_SDP_NEG_STATE_WAIT_NEGO;

    } else {
        
        neg->state = PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER;

    }

    /* Init last_sent's pool to app's pool, will be updated to active
     * SDPs pool after a successful SDP nego.
     */
    neg->pool_active = pool;

    /* Init PT number mapping variables. */
    init_mapping(neg);

    *p_neg = neg;
    return PJ_SUCCESS;
}


/*
 * Set codec order preference.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_set_prefer_remote_codec_order(
                                                pjmedia_sdp_neg *neg,
                                                pj_bool_t prefer_remote)
{
    PJ_ASSERT_RETURN(neg, PJ_EINVAL);
    neg->prefer_remote_codec_order = prefer_remote;
    return PJ_SUCCESS;
}


/*
 * Set multiple codec answering.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_set_answer_multiple_codecs(
                        pjmedia_sdp_neg *neg,
                        pj_bool_t answer_multiple)
{
    PJ_ASSERT_RETURN(neg, PJ_EINVAL);
    neg->answer_with_multiple_codecs = answer_multiple;
    return PJ_SUCCESS;
}


/*
 * Get SDP negotiator state.
 */
PJ_DEF(pjmedia_sdp_neg_state) pjmedia_sdp_neg_get_state( pjmedia_sdp_neg *neg )
{
    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(neg != NULL, PJMEDIA_SDP_NEG_STATE_NULL);
    return neg->state;
}


PJ_DEF(pj_status_t) pjmedia_sdp_neg_get_active_local( pjmedia_sdp_neg *neg,
                                        const pjmedia_sdp_session **local)
{
    PJ_ASSERT_RETURN(neg && local, PJ_EINVAL);
    PJ_ASSERT_RETURN(neg->active_local_sdp, PJMEDIA_SDPNEG_ENOACTIVE);

    *local = neg->active_local_sdp;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_sdp_neg_get_active_remote( pjmedia_sdp_neg *neg,
                                   const pjmedia_sdp_session **remote)
{
    PJ_ASSERT_RETURN(neg && remote, PJ_EINVAL);
    PJ_ASSERT_RETURN(neg->active_remote_sdp, PJMEDIA_SDPNEG_ENOACTIVE);

    *remote = neg->active_remote_sdp;
    return PJ_SUCCESS;
}


PJ_DEF(pj_bool_t) pjmedia_sdp_neg_was_answer_remote(pjmedia_sdp_neg *neg)
{
    PJ_ASSERT_RETURN(neg, PJ_FALSE);

    return neg->answer_was_remote;
}


PJ_DEF(pj_status_t) pjmedia_sdp_neg_get_neg_remote( pjmedia_sdp_neg *neg,
                                const pjmedia_sdp_session **remote)
{
    PJ_ASSERT_RETURN(neg && remote, PJ_EINVAL);
    PJ_ASSERT_RETURN(neg->neg_remote_sdp, PJMEDIA_SDPNEG_ENONEG);

    *remote = neg->neg_remote_sdp;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_sdp_neg_get_neg_local( pjmedia_sdp_neg *neg,
                               const pjmedia_sdp_session **local)
{
    PJ_ASSERT_RETURN(neg && local, PJ_EINVAL);
    PJ_ASSERT_RETURN(neg->neg_local_sdp, PJMEDIA_SDPNEG_ENONEG);

    *local = neg->neg_local_sdp;
    return PJ_SUCCESS;
}

static pjmedia_sdp_media *sdp_media_clone_deactivate(
                                    pj_pool_t *pool,
                                    const pjmedia_sdp_media *rem_med,
                                    const pjmedia_sdp_media *local_med,
                                    const pjmedia_sdp_session *local_sess)
{
    pjmedia_sdp_media *res;

    res = pjmedia_sdp_media_clone_deactivate(pool, rem_med);
    if (!res)
        return NULL;

    if (!res->conn && (!local_sess || !local_sess->conn)) {
        if (local_med && local_med->conn)
            res->conn = pjmedia_sdp_conn_clone(pool, local_med->conn);
        else {
            res->conn = PJ_POOL_ZALLOC_T(pool, pjmedia_sdp_conn);
            res->conn->net_type = pj_str("IN");
            res->conn->addr_type = pj_str("IP4");
            res->conn->addr = pj_str("127.0.0.1");
        }
    }

    return res;
}

/*
 * Modify local SDP and wait for remote answer.
 */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_modify_local_offer( pj_pool_t *pool,
                                    pjmedia_sdp_neg *neg,
                                    const pjmedia_sdp_session *local)
{
    return pjmedia_sdp_neg_modify_local_offer2(pool, neg, 0, local);
}

PJ_DEF(pj_status_t) pjmedia_sdp_neg_modify_local_offer2(
                                    pj_pool_t *pool,
                                    pjmedia_sdp_neg *neg,
                                    unsigned flags,
                                    const pjmedia_sdp_session *local)
{
    pjmedia_sdp_session *new_offer;
    pjmedia_sdp_session *old_offer;
    unsigned oi; /* old offer media index */
    pj_status_t status;

    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg && local, PJ_EINVAL);

    /* Can only do this in STATE_DONE. */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_DONE, 
                     PJMEDIA_SDPNEG_EINSTATE);

    /* Validate the new offer */
    status = pjmedia_sdp_validate(local);
    if (status != PJ_SUCCESS)
        return status;

    /* Change state to STATE_LOCAL_OFFER */
    neg->state = PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER;

    /* When there is no active local SDP in state PJMEDIA_SDP_NEG_STATE_DONE,
     * it means that the previous initial SDP nego must have been failed,
     * so we'll just set the local SDP offer here.
     */
    if (!neg->active_local_sdp) {
        neg->initial_sdp_tmp = NULL;
        neg->initial_sdp = pjmedia_sdp_session_clone(pool, local);

        /* Assign PT numbers for our offer and update the mapping. */
        assign_pt_and_update_map(pool, neg, neg->initial_sdp,
                                 PJ_TRUE, PJ_FALSE);
        neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, neg->initial_sdp);

        if (pjmedia_sdp_session_cmp(neg->last_sent, neg->neg_local_sdp, 0) !=
            PJ_SUCCESS)
        {
            ++neg->neg_local_sdp->origin.version;
        }
        neg->last_sent = neg->neg_local_sdp;

        return PJ_SUCCESS;
    }

    /* Init vars */
    old_offer = neg->active_local_sdp;
    new_offer = pjmedia_sdp_session_clone(pool, local);

    /* RFC 3264 Section 8: When issuing an offer that modifies the session,
     * the "o=" line of the new SDP MUST be identical to that in the
     * previous SDP, except that the version in the origin field MUST
     * increment by one from the previous SDP.
     */
    pj_strdup(pool, &new_offer->origin.user, &old_offer->origin.user);
    new_offer->origin.id = old_offer->origin.id;

    pj_strdup(pool, &new_offer->origin.net_type, &old_offer->origin.net_type);
    pj_strdup(pool, &new_offer->origin.addr_type,&old_offer->origin.addr_type);
    pj_strdup(pool, &new_offer->origin.addr, &old_offer->origin.addr);

    if ((flags & PJMEDIA_SDP_NEG_ALLOW_MEDIA_CHANGE) == 0) {
       /* Generating the new offer, in the case media lines doesn't match the
        * active SDP (e.g. current/active SDP's have m=audio and m=video lines,
        * and the new offer only has m=audio line), the negotiator will fix 
        * the new offer by reordering and adding the missing media line with 
        * port number set to zero.
        */
        for (oi = 0; oi < old_offer->media_count; ++oi) {
            pjmedia_sdp_media *om;
            pjmedia_sdp_media *nm;
            unsigned ni; /* new offer media index */
            pj_bool_t found = PJ_FALSE;

            om = old_offer->media[oi];
            for (ni = oi; ni < new_offer->media_count; ++ni) {
                nm = new_offer->media[ni];
                if (pj_strcmp(&nm->desc.media, &om->desc.media) == 0) {
                    if (ni != oi) {
                        /* The same media found but the position unmatched to
                         * the old offer, so let's put this media in the right
                         * place, and keep the order of the rest.
                         */
                        pj_array_insert(
                            new_offer->media,            /* array    */
                            sizeof(new_offer->media[0]), /* elmt size*/
                            ni,                          /* count    */
                            oi,                          /* pos      */
                            &nm);                        /* new elmt */
                    }
                    found = PJ_TRUE;
                    break;
                }
            }
            if (!found) {
                pjmedia_sdp_media *m;

                m = sdp_media_clone_deactivate(pool, om, om, local);

                pj_array_insert(new_offer->media, sizeof(new_offer->media[0]),
                                new_offer->media_count++, oi, &m);
            }
        }
    } else {
        /* If media type change is allowed, the negotiator only needs to fix 
         * the new offer by adding the missing media line(s) with port number
         * set to zero.
         */
        for (oi = new_offer->media_count; oi < old_offer->media_count; ++oi) {
            pjmedia_sdp_media *m;

            m = sdp_media_clone_deactivate(pool, old_offer->media[oi],
                                           old_offer->media[oi], local);

            pj_array_insert(new_offer->media, sizeof(new_offer->media[0]),
                            new_offer->media_count++, oi, &m);

        }
    }

    /* New_offer fixed */
    new_offer->origin.version = old_offer->origin.version;

    /* Assign PT numbers for our offer and update the mapping. */
    assign_pt_and_update_map(pool, neg, new_offer, PJ_TRUE, PJ_FALSE);

    if (pjmedia_sdp_session_cmp(neg->last_sent, new_offer, 0) != PJ_SUCCESS) {
        ++new_offer->origin.version;
    }
    neg->initial_sdp_tmp = neg->initial_sdp;
    neg->initial_sdp = new_offer;
    neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, new_offer);
    neg->last_sent = neg->neg_local_sdp;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_sdp_neg_send_local_offer( pj_pool_t *pool,
                                  pjmedia_sdp_neg *neg,
                                  const pjmedia_sdp_session **offer)
{
    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(neg && offer, PJ_EINVAL);

    *offer = NULL;

    /* Can only do this in STATE_DONE or STATE_LOCAL_OFFER. */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_DONE ||
                     neg->state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER, 
                     PJMEDIA_SDPNEG_EINSTATE);

    if (neg->state == PJMEDIA_SDP_NEG_STATE_DONE) {
        pjmedia_sdp_session *new_offer;

        /* If in STATE_DONE, set the active SDP as the offer. */
        PJ_ASSERT_RETURN(neg->active_local_sdp && neg->last_sent,
                         PJMEDIA_SDPNEG_ENOACTIVE);

        /* Retain initial SDP */
        if (neg->initial_sdp) {
            neg->initial_sdp_tmp = neg->initial_sdp;
            neg->initial_sdp = pjmedia_sdp_session_clone(pool,
                                                         neg->initial_sdp);
        }

        neg->state = PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER;
        new_offer = pjmedia_sdp_session_clone(pool, neg->active_local_sdp);

        if (pjmedia_sdp_session_cmp(neg->last_sent, new_offer, 0) !=
            PJ_SUCCESS)
        {
            ++new_offer->origin.version;
        }

        /* Update local SDP states */
        neg->neg_local_sdp = new_offer;
        neg->last_sent = new_offer;

        /* Return the new offer */
        *offer = new_offer;

    } else {
        /* We assume that we're in STATE_LOCAL_OFFER.
         * In this case set the neg_local_sdp as the offer.
         */
        *offer = neg->neg_local_sdp;
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_sdp_neg_set_remote_answer( pj_pool_t *pool,
                                   pjmedia_sdp_neg *neg,
                                   const pjmedia_sdp_session *remote)
{
    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg && remote, PJ_EINVAL);

    /* Can only do this in STATE_LOCAL_OFFER.
     * If we haven't provided local offer, then rx_remote_offer() should
     * be called instead of this function.
     */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER, 
                     PJMEDIA_SDPNEG_EINSTATE);

    /* We're ready to negotiate. */
    neg->state = PJMEDIA_SDP_NEG_STATE_WAIT_NEGO;
    neg->has_remote_answer = PJ_TRUE;
    neg->neg_remote_sdp = pjmedia_sdp_session_clone(pool, remote);
 
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_sdp_neg_set_remote_offer( pj_pool_t *pool,
                                  pjmedia_sdp_neg *neg,
                                  const pjmedia_sdp_session *remote)
{
    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg && remote, PJ_EINVAL);

    /* Can only do this in STATE_DONE.
     * If we already provide local offer, then rx_remote_answer() should
     * be called instead of this function.
     */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_DONE, 
                     PJMEDIA_SDPNEG_EINSTATE);

    /* State now is STATE_REMOTE_OFFER. */
    neg->state = PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER;
    neg->neg_remote_sdp = pjmedia_sdp_session_clone(pool, remote);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_sdp_neg_set_local_answer( pj_pool_t *pool,
                                  pjmedia_sdp_neg *neg,
                                  const pjmedia_sdp_session *local)
{
    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg && local, PJ_EINVAL);

    /* Can only do this in STATE_REMOTE_OFFER or WAIT_NEGO.
     * If we already provide local offer, then set_remote_answer() should
     * be called instead of this function.
     */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER ||
                     neg->state == PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, 
                     PJMEDIA_SDPNEG_EINSTATE);

    /* State now is STATE_WAIT_NEGO. */
    neg->state = PJMEDIA_SDP_NEG_STATE_WAIT_NEGO;
    if (local) {
        neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, local);
        if (neg->initial_sdp) {
            /* Retain initial_sdp value. */
            neg->initial_sdp_tmp = neg->initial_sdp;
            neg->initial_sdp = pjmedia_sdp_session_clone(pool,
                                                         neg->initial_sdp);
        
            /* I don't think there is anything in RFC 3264 that mandates
             * answerer to place the same origin (and increment version)
             * in the answer, but probably it won't hurt either.
             * Note that the version will be incremented in 
             * pjmedia_sdp_neg_negotiate()
             */
            neg->neg_local_sdp->origin.id = neg->initial_sdp->origin.id;
        } else {
            neg->initial_sdp = pjmedia_sdp_session_clone(pool, local);
        }
    } else {
        PJ_ASSERT_RETURN(neg->initial_sdp, PJMEDIA_SDPNEG_ENOINITIAL);
        neg->initial_sdp_tmp = neg->initial_sdp;
        neg->initial_sdp = pjmedia_sdp_session_clone(pool, neg->initial_sdp);
        neg->neg_local_sdp = pjmedia_sdp_session_clone(pool, neg->initial_sdp);
    }

    return PJ_SUCCESS;
}

PJ_DEF(pj_bool_t) pjmedia_sdp_neg_has_local_answer(pjmedia_sdp_neg *neg)
{
    pj_assert(neg && neg->state==PJMEDIA_SDP_NEG_STATE_WAIT_NEGO);
    return !neg->has_remote_answer;
}


/* Swap string. */
static void str_swap(pj_str_t *str1, pj_str_t *str2)
{
    pj_str_t tmp = *str1;
    *str1 = *str2;
    *str2 = tmp;
}

static void remove_all_media_directions(pjmedia_sdp_media *m)
{
    pjmedia_sdp_media_remove_all_attr(m, "inactive");
    pjmedia_sdp_media_remove_all_attr(m, "sendrecv");
    pjmedia_sdp_media_remove_all_attr(m, "sendonly");
    pjmedia_sdp_media_remove_all_attr(m, "recvonly");
}

/* Update media direction based on peer's media direction */
static void update_media_direction(pj_pool_t *pool,
                                   const pjmedia_sdp_media *remote,
                                   pjmedia_sdp_media *local)
{
    pjmedia_dir old_dir = PJMEDIA_DIR_ENCODING_DECODING,
                new_dir;

    /* Get the media direction of local SDP */
    if (pjmedia_sdp_media_find_attr2(local, "sendonly", NULL))
        old_dir = PJMEDIA_DIR_ENCODING;
    else if (pjmedia_sdp_media_find_attr2(local, "recvonly", NULL))
        old_dir = PJMEDIA_DIR_DECODING;
    else if (pjmedia_sdp_media_find_attr2(local, "inactive", NULL))
        old_dir = PJMEDIA_DIR_NONE;

    new_dir = old_dir;

    /* Adjust local media direction based on remote media direction */
    if (pjmedia_sdp_media_find_attr2(remote, "inactive", NULL) != NULL) {
        /* If remote has "a=inactive", then local is inactive too */

        new_dir = PJMEDIA_DIR_NONE;

    } else if(pjmedia_sdp_media_find_attr2(remote, "sendonly", NULL) != NULL) {
        /* If remote has "a=sendonly", then set local to "recvonly" if
         * it is currently "sendrecv". Otherwise if local is NOT "recvonly",
         * then set local direction to "inactive".
         */
        switch (old_dir) {
        case PJMEDIA_DIR_ENCODING_DECODING:
            new_dir = PJMEDIA_DIR_DECODING;
            break;
        case PJMEDIA_DIR_DECODING:
            /* No change */
            break;
        default:
            new_dir = PJMEDIA_DIR_NONE;
            break;
        }

    } else if(pjmedia_sdp_media_find_attr2(remote, "recvonly", NULL) != NULL) {
        /* If remote has "a=recvonly", then set local to "sendonly" if
         * it is currently "sendrecv". Otherwise if local is NOT "sendonly",
         * then set local direction to "inactive"
         */
    
        switch (old_dir) {
        case PJMEDIA_DIR_ENCODING_DECODING:
            new_dir = PJMEDIA_DIR_ENCODING;
            break;
        case PJMEDIA_DIR_ENCODING:
            /* No change */
            break;
        default:
            new_dir = PJMEDIA_DIR_NONE;
            break;
        }

    } else {
        /* Remote indicates "sendrecv" capability. No change to local 
         * direction 
         */
    }

    if (new_dir != old_dir) {
        pjmedia_sdp_attr *a = NULL;

        remove_all_media_directions(local);

        switch (new_dir) {
        case PJMEDIA_DIR_NONE:
            a = pjmedia_sdp_attr_create(pool, "inactive", NULL);
            break;
        case PJMEDIA_DIR_ENCODING:
            a = pjmedia_sdp_attr_create(pool, "sendonly", NULL);
            break;
        case PJMEDIA_DIR_DECODING:
            a = pjmedia_sdp_attr_create(pool, "recvonly", NULL);
            break;
        default:
            /* sendrecv */
            break;
        }
        
        if (a) {
            pjmedia_sdp_media_add_attr(local, a);
        }
    }
}


/* Update single local media description to after receiving answer
 * from remote.
 */
static pj_status_t process_m_answer( pj_pool_t *pool,
                                     pjmedia_sdp_media *offer,
                                     pjmedia_sdp_media *answer,
                                     pj_bool_t allow_asym)
{
    unsigned i;

    /* Check that the media type match our offer. */

    if (pj_strcmp(&answer->desc.media, &offer->desc.media)!=0) {
        /* The media type in the answer is different than the offer! */
        return PJMEDIA_SDPNEG_EINVANSMEDIA;
    }

    /* Check if remote has rejected our offer */
    if (answer->desc.port == 0) {
        
        /* Remote has rejected our offer. 
         * Deactivate our media too.
         */
        pjmedia_sdp_media_deactivate(pool, offer);

        /* Don't need to proceed */
        return PJ_SUCCESS;
    }

    /* Check that transport in the answer match our offer. */

    /* At this point, transport type must be compatible, 
     * the transport instance will do more validation later.
     */
    if (pjmedia_sdp_transport_cmp(&answer->desc.transport, 
                                  &offer->desc.transport) 
        != PJ_SUCCESS)
    {
        return PJMEDIA_SDPNEG_EINVANSTP;
    }


    /* Ticket #1148: check if remote answer does not set port to zero when
     * offered with port zero. Let's just tolerate it.
     */
    if (offer->desc.port == 0) {
        /* Don't need to proceed */
        return PJ_SUCCESS;
    }

    /* Process direction attributes */
    update_media_direction(pool, answer, offer);
 
    /* If asymetric media is allowed, then just check that remote answer has 
     * codecs that are within the offer. 
     *
     * Otherwise if asymetric media is not allowed, then we will choose only
     * one codec in our initial offer to match the answer.
     */
    if (allow_asym) {
        for (i=0; i<answer->desc.fmt_count; ++i) {
            unsigned j;
            pj_str_t *rem_fmt = &answer->desc.fmt[i];

            for (j=0; j<offer->desc.fmt_count; ++j) {
                if (pj_strcmp(rem_fmt, &answer->desc.fmt[j])==0)
                    break;
            }

            if (j != offer->desc.fmt_count) {
                /* Found at least one common codec. */
                break;
            }
        }

        if (i == answer->desc.fmt_count) {
            /* No common codec in the answer! */
            return PJMEDIA_SDPNEG_EANSNOMEDIA;
        }

        PJ_TODO(CHECK_SDP_NEGOTIATION_WHEN_ASYMETRIC_MEDIA_IS_ALLOWED);

    } else {
        /* Offer format priority based on answer format index/priority */
        unsigned offer_fmt_prior[PJMEDIA_MAX_SDP_FMT];

        /* Remove all format in the offer that has no matching answer */
        for (i=0; i<offer->desc.fmt_count;) {
            unsigned pt;
            pj_uint32_t j;
            pj_str_t *fmt = &offer->desc.fmt[i];
            

            /* Find matching answer */
            pt = pj_strtoul(fmt);

            if (pt < 96) {
                for (j=0; j<answer->desc.fmt_count; ++j) {
                    if (pj_strcmp(fmt, &answer->desc.fmt[j])==0)
                        break;
                }
            } else {
                /* This is dynamic payload type.
                 * For dynamic payload type, we must look the rtpmap and
                 * compare the encoding name.
                 */
                const pjmedia_sdp_attr *a;
                pjmedia_sdp_rtpmap or_;

                /* Get the rtpmap for the payload type in the offer. */
                a = pjmedia_sdp_media_find_attr2(offer, "rtpmap", fmt);
                if (!a) {
                    pj_assert(!"Bug! Offer should have been validated");
                    return PJ_EBUG;
                }
                pjmedia_sdp_attr_get_rtpmap(a, &or_);

                /* Find paylaod in answer SDP with matching 
                 * encoding name and clock rate.
                 */
                for (j=0; j<answer->desc.fmt_count; ++j) {
                    a = pjmedia_sdp_media_find_attr2(answer, "rtpmap", 
                                                     &answer->desc.fmt[j]);
                    if (a) {
                        pjmedia_sdp_rtpmap ar;
                        pjmedia_sdp_attr_get_rtpmap(a, &ar);

                        /* See if encoding name, clock rate, and channel
                         * count match 
                         */
                        if (!pj_stricmp(&or_.enc_name, &ar.enc_name) &&
                            or_.clock_rate == ar.clock_rate &&
                            (pj_stricmp(&or_.param, &ar.param)==0 ||
                             (ar.param.slen==1 && *ar.param.ptr=='1')))
                        {
                            /* Call custom format matching callbacks */
                            if (custom_fmt_match(pool, &or_.enc_name,
                                                 offer, i, answer, j, 0) ==
                                PJ_SUCCESS)
                            {
                                /* Match! */
                                break;
                            }
                        }
                    }
                }
            }

            if (j == answer->desc.fmt_count) {
                /* This format has no matching answer.
                 * Remove it from our offer.
                 */
                pjmedia_sdp_attr *a;

                /* Remove rtpmap associated with this format */
                a = pjmedia_sdp_media_find_attr2(offer, "rtpmap", fmt);
                if (a)
                    pjmedia_sdp_media_remove_attr(offer, a);

                /* Remove fmtp associated with this format */
                a = pjmedia_sdp_media_find_attr2(offer, "fmtp", fmt);
                if (a)
                    pjmedia_sdp_media_remove_attr(offer, a);

                /* Remove this format from offer's array */
                pj_array_erase(offer->desc.fmt, sizeof(offer->desc.fmt[0]),
                               offer->desc.fmt_count, i);
                --offer->desc.fmt_count;

            } else {
                offer_fmt_prior[i] = j;
                ++i;
            }
        }

        if (0 == offer->desc.fmt_count) {
            /* No common codec in the answer! */
            return PJMEDIA_SDPNEG_EANSNOMEDIA;
        }

        /* Post process:
         * - Resort offer formats so the order match to the answer.
         * - Remove answer formats that unmatches to the offer.
         */
        
        /* Resort offer formats */
        for (i=0; i<offer->desc.fmt_count; ++i) {
            unsigned j;
            for (j=i+1; j<offer->desc.fmt_count; ++j) {
                if (offer_fmt_prior[i] > offer_fmt_prior[j]) {
                    unsigned tmp = offer_fmt_prior[i];
                    offer_fmt_prior[i] = offer_fmt_prior[j];
                    offer_fmt_prior[j] = tmp;
                    str_swap(&offer->desc.fmt[i], &offer->desc.fmt[j]);
                }
            }
        }

        /* Remove unmatched answer formats */
        {
            unsigned del_cnt = 0;
            for (i=0; i<answer->desc.fmt_count;) {
                /* The offer is ordered now, also the offer_fmt_prior */
                if (i >= offer->desc.fmt_count || 
                    offer_fmt_prior[i]-del_cnt != i)
                {
                    pj_str_t *fmt = &answer->desc.fmt[i];
                    pjmedia_sdp_attr *a;

                    /* Remove rtpmap associated with this format */
                    a = pjmedia_sdp_media_find_attr2(answer, "rtpmap", fmt);
                    if (a)
                        pjmedia_sdp_media_remove_attr(answer, a);

                    /* Remove fmtp associated with this format */
                    a = pjmedia_sdp_media_find_attr2(answer, "fmtp", fmt);
                    if (a)
                        pjmedia_sdp_media_remove_attr(answer, a);

                    /* Remove this format from answer's array */
                    pj_array_erase(answer->desc.fmt, 
                                   sizeof(answer->desc.fmt[0]),
                                   answer->desc.fmt_count, i);
                    --answer->desc.fmt_count;

                    ++del_cnt;
                } else {
                    ++i;
                }
            }
        }
    }

    /* Looks okay */
    return PJ_SUCCESS;
}


/* Update local media session (offer) to create active local session
 * after receiving remote answer.
 */
static pj_status_t process_answer(pj_pool_t *pool,
                                  pjmedia_sdp_session *local_offer,
                                  pjmedia_sdp_session *answer,
                                  pj_bool_t allow_asym,
                                  pjmedia_sdp_session **p_active)
{
    unsigned omi = 0; /* Offer media index */
    unsigned ami = 0; /* Answer media index */
    pj_bool_t has_active = PJ_FALSE;
    pjmedia_sdp_session *offer;
    pj_status_t status;

    /* Check arguments. */
    PJ_ASSERT_RETURN(pool && local_offer && answer && p_active, PJ_EINVAL);

    /* Duplicate local offer SDP. */
    offer = pjmedia_sdp_session_clone(pool, local_offer);

    /* Check that media count match between offer and answer */
    // Ticket #527, different media count is allowed for more interoperability,
    // however, the media order must be same between offer and answer.
    // if (offer->media_count != answer->media_count)
    //     return PJMEDIA_SDPNEG_EMISMEDIA;

    /* Now update each media line in the offer with the answer. */
    for (; omi<offer->media_count; ++omi) {
        if (ami == answer->media_count) {
            /* The answer has less media than the offer */
            pjmedia_sdp_media *am;

            /* Generate matching-but-disabled-media for the answer */
            am = sdp_media_clone_deactivate(pool, offer->media[omi],
                                            offer->media[omi], offer);
            answer->media[answer->media_count++] = am;
            ++ami;

            /* Deactivate our media offer too */
            pjmedia_sdp_media_deactivate(pool, offer->media[omi]);

            /* No answer media to be negotiated */
            continue;
        }

        status = process_m_answer(pool, offer->media[omi], answer->media[ami],
                                  allow_asym);

        /* If media type is mismatched, just disable the media. */
        if (status == PJMEDIA_SDPNEG_EINVANSMEDIA) {
            pjmedia_sdp_media_deactivate(pool, offer->media[omi]);
            continue;
        }
        /* No common format in the answer media. */
        else if (status == PJMEDIA_SDPNEG_EANSNOMEDIA) {
            pjmedia_sdp_media_deactivate(pool, offer->media[omi]);
            pjmedia_sdp_media_deactivate(pool, answer->media[ami]);
        } 
        /* Return the error code, for other errors. */
        else if (status != PJ_SUCCESS) {
            return status;
        }

        if (offer->media[omi]->desc.port != 0)
            has_active = PJ_TRUE;

        ++ami;
    }

    *p_active = offer;

    return has_active ? PJ_SUCCESS : PJMEDIA_SDPNEG_ENOMEDIA;
}


/* Internal function to rewrite the format string in SDP attribute rtpmap
 * and fmtp.
 */
PJ_INLINE(void) rewrite_pt(pj_pool_t *pool, pj_str_t *attr_val,
                           const pj_str_t *old_pt, const pj_str_t *new_pt)
{
    int len_diff = (int)(new_pt->slen - old_pt->slen);

    /* Note that attribute value should be null-terminated. */
    if (len_diff > 0) {
        pj_str_t new_val;
        new_val.ptr = (char*)pj_pool_alloc(pool, attr_val->slen+len_diff+1);
        new_val.slen = attr_val->slen + len_diff;
        pj_memcpy(new_val.ptr + len_diff, attr_val->ptr, attr_val->slen + 1);
        *attr_val = new_val;
    } else if (len_diff < 0) {
        attr_val->slen += len_diff;
        pj_memmove(attr_val->ptr, attr_val->ptr - len_diff,
                   attr_val->slen + 1);
    }
    pj_memcpy(attr_val->ptr, new_pt->ptr, new_pt->slen);
}

/* Internal function to rewrite the format string in SDP attribute rtpmap
 * and fmtp.
 */
PJ_INLINE(void) rewrite_pt2(pj_pool_t *pool, pj_str_t *attr_val,
                            unsigned pt, unsigned new_pt)
{
    pj_str_t new_pt_str, old_pt_str = {0};
    char buf[4];

    /* Rewrite the PT with the new one. */
    pj_utoa(new_pt, buf);
    new_pt_str = pj_str(buf);

    /* This is intentional, rewrite_pt() doesn't need the string content,
     * only the length.
     */
    old_pt_str.slen = pt >= 100? 3: 2;
    rewrite_pt(pool, attr_val, &old_pt_str, &new_pt_str);
}


/* Internal function to apply symmetric PT for the local answer. */
static void apply_answer_symmetric_pt(pj_pool_t *pool,
                                      pjmedia_sdp_media *answer,
                                      unsigned pt_cnt,
                                      const pj_str_t pt_offer[],
                                      const pj_str_t pt_answer[])
{
    pjmedia_sdp_attr *a_tmp[PJMEDIA_MAX_SDP_ATTR];
    unsigned i, a_tmp_cnt = 0;

    /* Rewrite the payload types in the answer if different to
     * the ones in the offer.
     */
    for (i = 0; i < pt_cnt; ++i) {
        pjmedia_sdp_attr *a;

        /* Skip if the PTs are the same already, e.g: static PT. */
        if (pj_strcmp(&pt_answer[i], &pt_offer[i]) == 0)
            continue;

        /* Rewrite payload type in the answer to match to the offer */
        pj_strdup(pool, &answer->desc.fmt[i], &pt_offer[i]);

        /* Also update payload type in rtpmap */
        a = pjmedia_sdp_media_find_attr2(answer, "rtpmap", &pt_answer[i]);
        if (a) {
            rewrite_pt(pool, &a->value, &pt_answer[i], &pt_offer[i]);
            /* Temporarily remove the attribute in case the new payload
             * type is being used by another format in the media.
             */
            pjmedia_sdp_media_remove_attr(answer, a);
            a_tmp[a_tmp_cnt++] = a;
        }

        /* Also update payload type in fmtp */
        a = pjmedia_sdp_media_find_attr2(answer, "fmtp", &pt_answer[i]);
        if (a) {
            rewrite_pt(pool, &a->value, &pt_answer[i], &pt_offer[i]);
            /* Temporarily remove the attribute in case the new payload
             * type is being used by another format in the media.
             */
            pjmedia_sdp_media_remove_attr(answer, a);
            a_tmp[a_tmp_cnt++] = a;
        }
    }

    /* Return back 'rtpmap' and 'fmtp' attributes */
    for (i = 0; i < a_tmp_cnt; ++i)
        pjmedia_sdp_media_add_attr(answer, a_tmp[i]);
}


/* Try to match offer with answer. */
static pj_status_t match_offer(pj_pool_t *pool,
                               pj_bool_t prefer_remote_codec_order,
                               pj_bool_t answer_with_multiple_codecs,
                               const pjmedia_sdp_media *offer,
                               const pjmedia_sdp_media *preanswer,
                               const pjmedia_sdp_session *preanswer_sdp,
                               pjmedia_sdp_media **p_answer)
{
    unsigned i;
    pj_bool_t master_has_codec = 0,
              master_has_other = 0,
              found_matching_codec = 0,
              found_matching_telephone_event = 0,
              found_matching_other = 0;
    unsigned pt_answer_count = 0;
    pj_str_t pt_answer[PJMEDIA_MAX_SDP_FMT];
    pj_str_t pt_offer[PJMEDIA_MAX_SDP_FMT];
    pjmedia_sdp_media *answer;
    const pjmedia_sdp_media *master, *slave;
    unsigned nclockrate = 0, clockrate[PJMEDIA_MAX_SDP_FMT];
    unsigned ntel_clockrate = 0, tel_clockrate[PJMEDIA_MAX_SDP_FMT];

    /* If offer has zero port, just clone the offer */
    if (offer->desc.port == 0) {
        answer = sdp_media_clone_deactivate(pool, offer, preanswer,
                                            preanswer_sdp);
        *p_answer = answer;
        return PJ_SUCCESS;
    }

    /* If the preanswer define zero port, this media is being rejected,
     * just clone the preanswer.
     */
    if (preanswer->desc.port == 0) {
        answer = pjmedia_sdp_media_clone(pool, preanswer);
        *p_answer = answer;
        return PJ_SUCCESS;
    }

    /* Set master/slave negotiator based on prefer_remote_codec_order. */
    if (prefer_remote_codec_order) {
        master = offer;
        slave  = preanswer;
    } else {
        master = preanswer;
        slave  = offer;
    }
    
    /* With the addition of telephone-event and dodgy MS RTC SDP, 
     * the answer generation algorithm looks really shitty...
     */
    for (i=0; i<master->desc.fmt_count; ++i) {
        unsigned j;
        
        if (pj_isdigit(*master->desc.fmt[i].ptr)) {
            /* This is normal/standard payload type, where it's identified
             * by payload number.
             */
            unsigned pt;

            pt = pj_strtoul(&master->desc.fmt[i]);
            
            if (pt < 96) {
                /* For static payload type, it's enough to compare just
                 * the payload number.
                 */

                master_has_codec = 1;

                /* We just need to select one codec if not allowing multiple.
                 * Continue if we have selected matching codec for previous 
                 * payload.
                 */
                if (!answer_with_multiple_codecs && found_matching_codec)
                    continue;

                /* Find matching codec in local descriptor. */
                for (j=0; j<slave->desc.fmt_count; ++j) {
                    unsigned p;
                    p = pj_strtoul(&slave->desc.fmt[j]);
                    if (p == pt && pj_isdigit(*slave->desc.fmt[j].ptr)) {
                        unsigned k;

                        found_matching_codec = 1;
                        pt_offer[pt_answer_count] = slave->desc.fmt[j];
                        pt_answer[pt_answer_count++] = slave->desc.fmt[j];

                        /* Take note of clock rate for tel-event. Note: for
                         * static PT, we assume the clock rate is 8000.
                         */
                        for (k=0; k<nclockrate; ++k)
                            if (clockrate[k] == 8000)
                                break;
                        if (k == nclockrate)
                            clockrate[nclockrate++] = 8000;
                        break;
                    }
                }

            } else {
                /* This is dynamic payload type.
                 * For dynamic payload type, we must look the rtpmap and
                 * compare the encoding name.
                 */
                const pjmedia_sdp_attr *a;
                pjmedia_sdp_rtpmap or_;
                pj_bool_t is_codec = 0;

                /* Get the rtpmap for the payload type in the master. */
                a = pjmedia_sdp_media_find_attr2(master, "rtpmap", 
                                                 &master->desc.fmt[i]);
                if (!a) {
                    pj_assert(!"Bug! Offer should have been validated");
                    return PJMEDIA_SDP_EMISSINGRTPMAP;
                }
                pjmedia_sdp_attr_get_rtpmap(a, &or_);

                if (pj_stricmp2(&or_.enc_name, "telephone-event")) {
                    master_has_codec = 1;
                    if (!answer_with_multiple_codecs && found_matching_codec)
                        continue;
                    is_codec = 1;
                }
                
                /* Find paylaod in our initial SDP with matching 
                 * encoding name and clock rate.
                 */
                for (j=0; j<slave->desc.fmt_count; ++j) {
                    a = pjmedia_sdp_media_find_attr2(slave, "rtpmap", 
                                                     &slave->desc.fmt[j]);
                    if (a) {
                        pjmedia_sdp_rtpmap lr;
                        pjmedia_sdp_attr_get_rtpmap(a, &lr);

                        /* See if encoding name, clock rate, and
                         * channel count  match 
                         */
                        if (!pj_stricmp(&or_.enc_name, &lr.enc_name) &&
                            or_.clock_rate == lr.clock_rate &&
                            (pj_stricmp(&or_.param, &lr.param)==0 ||
                             (lr.param.slen==0 && or_.param.slen==1 && 
                                                 *or_.param.ptr=='1') || 
                             (or_.param.slen==0 && lr.param.slen==1 && 
                                                  *lr.param.ptr=='1'))) 
                        {
                            /* Match! */
                            if (is_codec) {
                                pjmedia_sdp_media *o_med, *a_med;
                                unsigned o_fmt_idx, a_fmt_idx;
                                unsigned k;

                                o_med = (pjmedia_sdp_media*)offer;
                                a_med = (pjmedia_sdp_media*)preanswer;
                                o_fmt_idx = prefer_remote_codec_order? i:j;
                                a_fmt_idx = prefer_remote_codec_order? j:i;

                                /* Call custom format matching callbacks */
                                if (custom_fmt_match(pool, &or_.enc_name,
                                                     o_med, o_fmt_idx,
                                                     a_med, a_fmt_idx,
                                                     ALLOW_MODIFY_ANSWER) !=
                                    PJ_SUCCESS)
                                {
                                    continue;
                                }
                                found_matching_codec = 1;

                                /* Take note of clock rate for tel-event */
                                for (k=0; k<nclockrate; ++k)
                                    if (clockrate[k] == or_.clock_rate)
                                        break;
                                if (k == nclockrate)
                                    clockrate[nclockrate++] = or_.clock_rate;
                            } else {
                                unsigned k;

                                /* Keep track of tel-event clock rate,
                                 * to prevent duplicate.
                                 */
                                for (k=0; k<ntel_clockrate; ++k)
                                    if (tel_clockrate[k] == or_.clock_rate)
                                        break;
                                if (k < ntel_clockrate)
                                    continue;
                                
                                tel_clockrate[ntel_clockrate++] = or_.clock_rate;
                                found_matching_telephone_event = 1;
                            }

                            pt_offer[pt_answer_count] = 
                                                prefer_remote_codec_order?
                                                offer->desc.fmt[i]:
                                                offer->desc.fmt[j];
                            pt_answer[pt_answer_count++] = 
                                                prefer_remote_codec_order? 
                                                preanswer->desc.fmt[j]:
                                                preanswer->desc.fmt[i];
                            break;
                        }
                    }
                }
            }

        } else {
            /* This is a non-standard, brain damaged SDP where the payload
             * type is non-numeric. It exists e.g. in Microsoft RTC based
             * UA, to indicate instant messaging capability.
             * Example:
             *  - m=x-ms-message 5060 sip null
             */
            master_has_other = 1;
            if (found_matching_other)
                continue;

            for (j=0; j<slave->desc.fmt_count; ++j) {
                if (!pj_strcmp(&master->desc.fmt[i], &slave->desc.fmt[j])) {
                    /* Match */
                    found_matching_other = 1;
                    pt_offer[pt_answer_count] = prefer_remote_codec_order?
                                                offer->desc.fmt[i]:
                                                offer->desc.fmt[j];
                    pt_answer[pt_answer_count++] = prefer_remote_codec_order? 
                                                   preanswer->desc.fmt[j]:
                                                   preanswer->desc.fmt[i];
                    break;
                }
            }
        }
    }

    /* See if all types of master can be matched. */
    if (master_has_codec && !found_matching_codec) {
        return PJMEDIA_SDPNEG_NOANSCODEC;
    }

    /* If this comment is removed, negotiation will fail if remote has offered
       telephone-event and local is not configured with telephone-event

    if (offer_has_telephone_event && !found_matching_telephone_event) {
        return PJMEDIA_SDPNEG_NOANSTELEVENT;
    }
    */

    if (master_has_other && !found_matching_other) {
        return PJMEDIA_SDPNEG_NOANSUNKNOWN;
    }

    /* Seems like everything is in order. */

    /* Remove unwanted telephone-event formats. */
    if (found_matching_telephone_event) {
        pj_str_t first_televent_offer = {0};
        pj_str_t first_televent_answer = {0};
        unsigned matched_cnt = 0;

        for (i=0; i<pt_answer_count; ) {
            const pjmedia_sdp_attr *a;
            pjmedia_sdp_rtpmap r;
            unsigned j;

            /* Skip static PT, as telephone-event uses dynamic PT */
            if (!pj_isdigit(*pt_answer[i].ptr) || pj_strtol(&pt_answer[i])<96)
            {
                ++i;
                continue;
            }

            /* Get the rtpmap for format. */
            a = pjmedia_sdp_media_find_attr2(preanswer, "rtpmap",
                                             &pt_answer[i]);
            pj_assert(a);
            pjmedia_sdp_attr_get_rtpmap(a, &r);

            /* Only care for telephone-event format */
            if (pj_stricmp2(&r.enc_name, "telephone-event")) {
                ++i;
                continue;
            }

            if (first_televent_offer.slen == 0) {
                first_televent_offer = pt_offer[i];
                first_televent_answer = pt_answer[i];
            }

            for (j=0; j<nclockrate; ++j) {
                if (r.clock_rate==clockrate[j])
                    break;
            }

            /* This tel-event's clockrate is unwanted, remove the tel-event */
            if (j==nclockrate) {
                pj_array_erase(pt_answer, sizeof(pt_answer[0]),
                               pt_answer_count, i);
                pj_array_erase(pt_offer, sizeof(pt_offer[0]),
                               pt_answer_count, i);
                pt_answer_count--;
            } else {
                ++matched_cnt;
                ++i;
            }
        }

        /* Tel-event is wanted, but no matched clock rate (to the selected
         * audio codec), just put back any first matched tel-event formats.
         */
        if (!matched_cnt) {
            pt_offer[pt_answer_count] = first_televent_offer;
            pt_answer[pt_answer_count++] = first_televent_answer;
        }
    }

    /* Build the answer by cloning from preanswer, and reorder the payload
     * to suit the offer.
     */
    answer = pjmedia_sdp_media_clone(pool, preanswer);
    for (i=0; i<pt_answer_count; ++i) {
        unsigned j;
        for (j=i; j<answer->desc.fmt_count; ++j) {
            if (!pj_strcmp(&answer->desc.fmt[j], &pt_answer[i]))
                break;
        }
        pj_assert(j != answer->desc.fmt_count);
        str_swap(&answer->desc.fmt[i], &answer->desc.fmt[j]);
    }
    
    /* Remove unwanted local formats. */
    for (i=pt_answer_count; i<answer->desc.fmt_count; ++i) {
        pjmedia_sdp_attr *a;

        /* Remove rtpmap for this format */
        a = pjmedia_sdp_media_find_attr2(answer, "rtpmap", 
                                         &answer->desc.fmt[i]);
        if (a) {
            pjmedia_sdp_media_remove_attr(answer, a);
        }

        /* Remove fmtp for this format */
        a = pjmedia_sdp_media_find_attr2(answer, "fmtp", 
                                         &answer->desc.fmt[i]);
        if (a) {
            pjmedia_sdp_media_remove_attr(answer, a);
        }
    }
    answer->desc.fmt_count = pt_answer_count;

#if PJMEDIA_SDP_NEG_ANSWER_SYMMETRIC_PT
    apply_answer_symmetric_pt(pool, answer, pt_answer_count,
                              pt_offer, pt_answer);
#endif

    /* Update media direction. */
    update_media_direction(pool, offer, answer);

    *p_answer = answer;
    return PJ_SUCCESS;
}

/* Create complete answer for remote's offer. */
static pj_status_t create_answer( pj_pool_t *pool,
                                  pj_bool_t prefer_remote_codec_order,
                                  pj_bool_t answer_with_multiple_codecs,
                                  const pjmedia_sdp_session *initial,
                                  const pjmedia_sdp_session *offer,
                                  pjmedia_sdp_session **p_answer)
{
    pj_status_t status = PJMEDIA_SDPNEG_ENOMEDIA;
    pj_bool_t has_active = PJ_FALSE;
    pjmedia_sdp_session *answer;
    char media_used[PJMEDIA_MAX_SDP_MEDIA];
    unsigned i;

    /* Validate remote offer. 
     * This should have been validated before.
     */
    status = pjmedia_sdp_validate(offer);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, status);

    /* Create initial answer by duplicating initial SDP,
     * but clear all media lines. The media lines will be filled up later.
     */
    answer = pjmedia_sdp_session_clone(pool, initial);
    PJ_ASSERT_RETURN(answer != NULL, PJ_ENOMEM);

    answer->media_count = 0;

    pj_bzero(media_used, sizeof(media_used));

    /* For each media line, create our answer based on our initial
     * capability.
     */
    for (i=0; i<offer->media_count; ++i) {
        const pjmedia_sdp_media *om;    /* offer */
        const pjmedia_sdp_media *im;    /* initial media */
        pjmedia_sdp_media *am = NULL;   /* answer/result */
        pj_uint32_t om_tp;
        unsigned j;

        om = offer->media[i];

        om_tp = pjmedia_sdp_transport_get_proto(&om->desc.transport);
        PJMEDIA_TP_PROTO_TRIM_FLAG(om_tp, PJMEDIA_TP_PROFILE_RTCP_FB);

        /* Find media description in our initial capability that matches
         * the media type and transport type of offer's media, has
         * matching codec, and has not been used to answer other offer.
         */
        for (im=NULL, j=0; j<initial->media_count; ++j) {
            pj_uint32_t im_tp;

            im = initial->media[j];

            im_tp = pjmedia_sdp_transport_get_proto(&im->desc.transport);
            PJMEDIA_TP_PROTO_TRIM_FLAG(im_tp, PJMEDIA_TP_PROFILE_RTCP_FB);

            if (pj_strcmp(&om->desc.media, &im->desc.media)==0 &&
                om_tp == im_tp &&
                media_used[j] == 0)
            {
                pj_status_t status2;

                /* See if it has matching codec. */
                status2 = match_offer(pool, prefer_remote_codec_order,
                                      answer_with_multiple_codecs,
                                      om, im, initial, &am);
                if (status2 == PJ_SUCCESS) {
                    /* Mark media as used. */
                    media_used[j] = 1;
                    break;
                } else {
                    status = status2;
                }
            }
        }

        if (j==initial->media_count) {
            /* No matching media.
             * Reject the offer by setting the port to zero in the answer.
             */
            /* For simplicity in the construction of the answer, we'll
             * just clone the media from the offer. Anyway receiver will
             * ignore anything in the media once it sees that the port
             * number is zero.
             */
            am = sdp_media_clone_deactivate(pool, om, om, answer);
        } else {
            /* The answer is in am */
            pj_assert(am != NULL);
        }

        /* Add the media answer */
        answer->media[answer->media_count++] = am;

        /* Check if this media is active.*/
        if (am->desc.port != 0)
            has_active = PJ_TRUE;
    }

    *p_answer = answer;

    return has_active ? PJ_SUCCESS : status;
}

/* Find an unused PT number to be assigned to a codec. */
static int find_new_pt(const pt_to_codec_map *pt_to_codec,
                       const pj_bool_t used[],
                       const pj_str_t *codec,
                       pj_int8_t codec_idx)
{
    int idx, start = 0, result = -1;
    const pj_str_t telephone_event = pj_str("telephone-event");

    /* If the codec is a telephone-event, start searching from
     * PJMEDIA_RTP_PT_TELEPHONE_EVENTS.
     */
    if (pj_strnicmp(codec, &telephone_event, telephone_event.slen) == 0)
        start = PJMEDIA_RTP_PT_TELEPHONE_EVENTS - START_DYNAMIC_PT;

    /* Find an unused PT number.
     * First priority, find PT number that has been mapped to that codec.
     * Second priority, find PT number that has never been mapped.
     * Last resort, any number that's unused.
     */
    for (idx = start; idx < DYNAMIC_PT_SIZE; idx++) {
        if (used[idx]) continue;
        if ((*pt_to_codec)[idx] == codec_idx) {
            return START_DYNAMIC_PT + idx;
        } else if ((*pt_to_codec)[idx] == -1 &&
                   (result == -1 || (*pt_to_codec)[result] != -1))
        {
            result = idx;
        } else if (result == -1) {
            result = idx;
        }
    }

    /* Not found, start from the beginning. */
    for (idx = 0; idx < start; idx++) {
        if (used[idx]) continue;
        if ((*pt_to_codec)[idx] == codec_idx) {
            return START_DYNAMIC_PT + idx;
        } else if ((*pt_to_codec)[idx] == -1 &&
                   (result == -1 || (*pt_to_codec)[result] != -1))
        {
            result = idx;
        } else if (result == -1) {
            result = idx;
        }
    }

    /* Since SDP has been validated and the number of codecs with dynamic
     * PTs won't exceed the available slots, this should never happen.
     */
    pj_assert(result != -1);

    if ((*pt_to_codec)[result] != -1) {
        PJ_LOG(3, (THIS_FILE, "Unable to assign PT number for codec %.*s "
                               "that conforms to PT mapping requirement, "
                               "will use PT no %d", (int)codec->slen,
                               codec->ptr, START_DYNAMIC_PT + result));
    }

    return START_DYNAMIC_PT + result;
}

/*
 * This method will assign PT numbers for the codecs based on the mapping
 * that we have recorded, and update the mapping.
 */
static pj_status_t assign_pt_and_update_map(pj_pool_t *pool,
                                            pjmedia_sdp_neg *neg,
                                            pjmedia_sdp_session *sess,
                                            pj_bool_t is_offer,
                                            pj_bool_t update_only)
{
    unsigned i, j;

    PJ_UNUSED_ARG(pool);

    for (i = 0; i < sess->media_count; ++i) {
        pjmedia_type med_type;
        unsigned count;
        pj_str_t *dyn_codecs;
        pj_bool_t pt_used[DYNAMIC_PT_SIZE];
        pj_int8_t pt_change[DYNAMIC_PT_SIZE];
        pjmedia_sdp_media *sdp_m = sess->media[i];

        if (sdp_m->desc.port == 0) {
            /* Reset the mapping. */
            pj_memset(neg->pt_to_codec[i], -1,
                      PJ_ARRAY_SIZE(neg->pt_to_codec[i]));
            pj_bzero(neg->codec_to_pt[i], PJ_ARRAY_SIZE(neg->codec_to_pt[i]));
            continue;
        }

        /* Get the codec string list based on media type */
        med_type = pjmedia_get_type(&sdp_m->desc.media);
        if (med_type == PJMEDIA_TYPE_AUDIO) {
            dyn_codecs = neg->aud_dyn_codecs;
            count = neg->aud_dyn_codecs_cnt;
#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)
        } else if (med_type == PJMEDIA_TYPE_VIDEO) {
            dyn_codecs = neg->vid_dyn_codecs;
            count = neg->vid_dyn_codecs_cnt;
#endif
        } else {
            continue;
        }

        /* Initialize arrays to keep track of:
         * - which PT numbers have been used, to avoid duplicates
         * - the change of PT numbers
         */
        pj_bzero(pt_used, PJ_ARRAY_SIZE(pt_used) * sizeof(pj_bool_t));
        pj_bzero(pt_change, PJ_ARRAY_SIZE(pt_change));

        for (j = 0; j < sdp_m->attr_count; ++j) {
            const pjmedia_sdp_attr *attr;
            pjmedia_sdp_rtpmap rtpmap;
            unsigned pt;
            pjmedia_codec_id codec_id;
            pj_str_t codec;
            pj_int8_t codec_idx;
            pj_int8_t pt_to_codec;
            pj_int8_t codec_to_pt = 0;
            pj_int8_t new_pt = 0;
            pj_bool_t need_new_pt = PJ_FALSE;

            attr = sdp_m->attr[j];
            if (pj_strcmp2(&attr->name, "rtpmap") != 0)
                continue;
            if (pjmedia_sdp_attr_get_rtpmap(attr, &rtpmap) != PJ_SUCCESS)
                continue;

            /* We only need to handle mapping for dynamic PT */
            pt = pj_strtoul(&rtpmap.pt);
            if (pt < START_DYNAMIC_PT)
                continue;

            if (med_type == PJMEDIA_TYPE_AUDIO) {
                pjmedia_codec_info info;

                /* Build codec format info */
                info.encoding_name = rtpmap.enc_name;
                info.clock_rate = rtpmap.clock_rate;
                if (rtpmap.param.slen) {
                    info.channel_cnt = (unsigned) pj_strtoul(&rtpmap.param);
                } else {
                    info.channel_cnt = 1;
                }

                /* Normalize codec info to get codec id. */
                pjmedia_codec_info_to_id(&info, codec_id, sizeof(codec_id));
                codec = pj_str(codec_id);
            } else {
                /* For video, we just use the encoding name */
                codec = rtpmap.enc_name;
            }

            codec_idx = (pj_int8_t)pjmedia_codec_mgr_find_codec(dyn_codecs,
                                                     count, &codec, NULL);
            if (codec_idx < 0) {
                /* This typically happens when remote offers unknown
                 * codec.
                 */
                codec_idx = UNKNOWN_CODEC;
            }

            if (update_only) {
                /* Update the mapping. */
                neg->pt_to_codec[i][pt - START_DYNAMIC_PT] = codec_idx;
                if (codec_idx != UNKNOWN_CODEC)
                    neg->codec_to_pt[i][codec_idx] = (pj_int8_t)pt;
                continue;
            }

            pt_to_codec = neg->pt_to_codec[i][pt - START_DYNAMIC_PT];
            if (codec_idx != UNKNOWN_CODEC)
                codec_to_pt = neg->codec_to_pt[i][codec_idx];

            /* If the PT number has been mapped to another codec,
             * we need to find another PT number, as per the RFC 3264
             * section 8.3.2:
             * the mapping from a particular dynamic payload type number
             * to a particular codec within that media stream MUST NOT change
             * for the duration of a session.
             */
            if (pt_to_codec != -1 && pt_to_codec != codec_idx)
                need_new_pt = PJ_TRUE;

            /* We also need to find a new PT number if this number has
             * been assigned to another codec.
             */
            if (pt_used[pt - START_DYNAMIC_PT])
                need_new_pt = PJ_TRUE;

            /* If the codec has previously been mapped to a PT number,
             * we use that number, provided that:
             * - that PT number is unused
             * - this is not an answer with symmetric PT (answer with
             *   symmetric PT has been matched to the offer so we'd better
             *   leave it unchanged)
             */
            if (codec_to_pt != 0 &&
                !pt_used[codec_to_pt - START_DYNAMIC_PT] &&
                (need_new_pt || is_offer ||
                 !PJMEDIA_SDP_NEG_ANSWER_SYMMETRIC_PT))
            {
                new_pt = codec_to_pt;
            }

            /* We need a new PT number and haven't got one,
             * find the first unused one.
             */
            if (need_new_pt && new_pt == 0) {
                new_pt = (pj_int8_t)find_new_pt(&neg->pt_to_codec[i], pt_used,
                                                &codec, codec_idx);
            }

            if (new_pt != 0 && new_pt != (pj_int8_t)pt) {
                rewrite_pt2(neg->pool_active, (pj_str_t *)&attr->value,
                            pt, new_pt);
            } else {
                new_pt = (pj_int8_t)pt;
            }

            /* Mark the PT number as used and keep track of the change
             * from old to new PT number.
             */
            pt_used[new_pt - START_DYNAMIC_PT] = PJ_TRUE;
            pt_change[pt - START_DYNAMIC_PT] = new_pt;

            /* Update the mapping. */
            neg->pt_to_codec[i][new_pt - START_DYNAMIC_PT] = codec_idx;
            if (codec_idx != UNKNOWN_CODEC)
                neg->codec_to_pt[i][codec_idx] = new_pt;
        }

        /* We don't need to modify SDP if we only want to update
         * the mapping.
         */
        if (update_only) continue;

        /* Modify fmtp */
        for (j = 0; j < sdp_m->attr_count; ++j) {
            const pjmedia_sdp_attr *attr;
            pjmedia_sdp_fmtp fmtp;
            unsigned pt, new_pt = 0;

            attr = sdp_m->attr[j];
            if (pj_strcmp2(&attr->name, "fmtp") != 0)
                continue;
            if (pjmedia_sdp_attr_get_fmtp(attr, &fmtp) != PJ_SUCCESS)
                continue;

            /* We only need to handle mapping for dynamic PT */
            pt = pj_strtoul(&fmtp.fmt);
            if (pt < START_DYNAMIC_PT)
                continue;

            new_pt = pt_change[pt - START_DYNAMIC_PT];
            /* No PT change */
            if (new_pt == 0 || new_pt == pt)
                continue;

            rewrite_pt2(neg->pool_active, (pj_str_t *)&attr->value,
                        pt, new_pt);
        }

        /* Modify format list */
        for (j = 0; j < sdp_m->desc.fmt_count; ++j) {
            unsigned pt, new_pt = 0;

            pt = pj_strtoul(&sdp_m->desc.fmt[j]);
            if (pt < START_DYNAMIC_PT)
                 continue;

            new_pt = pt_change[pt - START_DYNAMIC_PT];
            /* No PT change */
            if (new_pt == 0 || new_pt == pt)
                continue;

            rewrite_pt2(neg->pool_active, &sdp_m->desc.fmt[j],
                        pt, new_pt);
        }
    }

    return PJ_SUCCESS;
}



/* Cancel offer */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_cancel_offer(pjmedia_sdp_neg *neg)
{
    PJ_ASSERT_RETURN(neg, PJ_EINVAL);

    /* Must be in LOCAL_OFFER state. */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER ||
                     neg->state == PJMEDIA_SDP_NEG_STATE_REMOTE_OFFER,
                     PJMEDIA_SDPNEG_EINSTATE);

    // No longer needed after #3322.
    if (0 && neg->state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER &&
        neg->active_local_sdp) 
    {
        /* Increment next version number. This happens if for example
         * the reinvite offer is rejected by 488. If we don't increment
         * the version here, the next offer will have the same version.
         */
        neg->active_local_sdp->origin.version++;
    }

    /* Revert back initial SDP */
    if (neg->state == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER)
        neg->initial_sdp = neg->initial_sdp_tmp;

    /* Clear temporary SDP */
    neg->initial_sdp_tmp = NULL;
    neg->neg_local_sdp = neg->neg_remote_sdp = NULL;
    neg->has_remote_answer = PJ_FALSE;

    /* Reset state to done */
    neg->state = PJMEDIA_SDP_NEG_STATE_DONE;

    return PJ_SUCCESS;
}


/* The best bit: SDP negotiation function! */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_negotiate( pj_pool_t *pool,
                                               pjmedia_sdp_neg *neg,
                                               pj_bool_t allow_asym)
{
    pj_status_t status;

    /* Check arguments are valid. */
    PJ_ASSERT_RETURN(pool && neg, PJ_EINVAL);

    /* Must be in STATE_WAIT_NEGO state. */
    PJ_ASSERT_RETURN(neg->state == PJMEDIA_SDP_NEG_STATE_WAIT_NEGO, 
                     PJMEDIA_SDPNEG_EINSTATE);

    /* Must have remote offer. */
    PJ_ASSERT_RETURN(neg->neg_remote_sdp, PJ_EBUG);

#if PJMEDIA_SDP_NEG_MAINTAIN_REMOTE_PT_MAP
    /* Update PT mapping based on remote SDP as well. */
    assign_pt_and_update_map(pool, neg, neg->neg_remote_sdp,
                             !neg->has_remote_answer, PJ_TRUE);
#endif

    if (neg->has_remote_answer) {
        pjmedia_sdp_session *active;
        status = process_answer(pool, neg->neg_local_sdp, neg->neg_remote_sdp,
                                allow_asym, &active);
        if (status == PJ_SUCCESS) {
            /* Only update active SDPs when negotiation is successfull */
            neg->active_local_sdp = active;
            neg->active_remote_sdp = neg->neg_remote_sdp;

            /* Keep the pool used for allocating the active SDPs */
            neg->pool_active = pool;
        } else {
            /* SDP nego failed, retain the last_sdp. */
            neg->last_sent = pjmedia_sdp_session_clone(neg->pool_active,
                                                       neg->last_sent);
        }
    } else {
        pjmedia_sdp_session *answer = NULL;

        status = create_answer(pool, neg->prefer_remote_codec_order,
                               neg->answer_with_multiple_codecs,
                               neg->neg_local_sdp, neg->neg_remote_sdp,
                               &answer);
        if (status == PJ_SUCCESS) {
            /* Assign PT numbers for our answer and update the mapping. */
            assign_pt_and_update_map(pool, neg, answer, PJ_FALSE, PJ_FALSE);

            if (neg->last_sent)
                answer->origin.version = neg->last_sent->origin.version;

            if (!neg->last_sent ||
                pjmedia_sdp_session_cmp(neg->last_sent, answer, 0) !=
                PJ_SUCCESS)
            {
                ++answer->origin.version;
            }

            /* Only update active SDPs when negotiation is successfull */
            neg->active_local_sdp = answer;
            neg->active_remote_sdp = neg->neg_remote_sdp;

            /* This answer will be sent, so update the last sent SDP */
            neg->last_sent = answer;

            /* Keep the pool used for allocating the active SDPs */
            neg->pool_active = pool;
        }
    }

    /* State is DONE regardless */
    neg->state = PJMEDIA_SDP_NEG_STATE_DONE;

    /* Save state */
    neg->answer_was_remote = neg->has_remote_answer;

    /* Revert back initial SDP if nego fails */
    if (status != PJ_SUCCESS)
        neg->initial_sdp = neg->initial_sdp_tmp;

    /* Clear temporary SDP */
    neg->initial_sdp_tmp = NULL;
    neg->neg_local_sdp = neg->neg_remote_sdp = NULL;
    neg->has_remote_answer = PJ_FALSE;

    return status;
}


static pj_status_t custom_fmt_match(pj_pool_t *pool,
                                    const pj_str_t *fmt_name,
                                    pjmedia_sdp_media *offer,
                                    unsigned o_fmt_idx,
                                    pjmedia_sdp_media *answer,
                                    unsigned a_fmt_idx,
                                    unsigned option)
{
    unsigned i;

    for (i = 0; i < fmt_match_cb_cnt; ++i) {
        if (pj_stricmp(fmt_name, &fmt_match_cb[i].fmt_name) == 0) {
            pj_assert(fmt_match_cb[i].cb);
            return (*fmt_match_cb[i].cb)(pool, offer, o_fmt_idx,
                                         answer, a_fmt_idx,
                                         option);
        }
    }

    /* Not customized format matching found, should be matched */
    return PJ_SUCCESS;
}

/* Register customized SDP format negotiation callback function. */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_register_fmt_match_cb(
                                        const pj_str_t *fmt_name,
                                        pjmedia_sdp_neg_fmt_match_cb cb)
{
    struct fmt_match_cb_t *f = NULL;
    unsigned i;

    PJ_ASSERT_RETURN(fmt_name, PJ_EINVAL);

    /* Check if the callback for the format name has been registered */
    for (i = 0; i < fmt_match_cb_cnt; ++i) {
        if (pj_stricmp(fmt_name, &fmt_match_cb[i].fmt_name) == 0)
            break;
    }

    /* Unregistration */
    
    if (cb == NULL) {
        if (i == fmt_match_cb_cnt)
            return PJ_ENOTFOUND;

        pj_array_erase(fmt_match_cb, sizeof(fmt_match_cb[0]),
                       fmt_match_cb_cnt, i);
        fmt_match_cb_cnt--;

        return PJ_SUCCESS;
    }

    /* Registration */

    if (i < fmt_match_cb_cnt) {
        /* The same format name has been registered before */
        if (cb != fmt_match_cb[i].cb)
            return PJ_EEXISTS;
        else
            return PJ_SUCCESS;
    }

    if (fmt_match_cb_cnt >= PJ_ARRAY_SIZE(fmt_match_cb))
        return PJ_ETOOMANY;

    f = &fmt_match_cb[fmt_match_cb_cnt++];
    f->fmt_name = *fmt_name;
    f->cb = cb;

    return PJ_SUCCESS;
}


/* Match format in the SDP media offer and answer. */
PJ_DEF(pj_status_t) pjmedia_sdp_neg_fmt_match(pj_pool_t *pool,
                                              pjmedia_sdp_media *offer,
                                              unsigned o_fmt_idx,
                                              pjmedia_sdp_media *answer,
                                              unsigned a_fmt_idx,
                                              unsigned option)
{
    const pjmedia_sdp_attr *attr;
    pjmedia_sdp_rtpmap o_rtpmap, a_rtpmap;
    unsigned o_pt;
    unsigned a_pt;

    o_pt = pj_strtoul(&offer->desc.fmt[o_fmt_idx]);
    a_pt = pj_strtoul(&answer->desc.fmt[a_fmt_idx]);

    if (o_pt < 96 || a_pt < 96) {
        if (o_pt == a_pt)
            return PJ_SUCCESS;
        else
            return PJMEDIA_SDP_EFORMATNOTEQUAL;
    }

    /* Get the format rtpmap from the offer. */
    attr = pjmedia_sdp_media_find_attr2(offer, "rtpmap", 
                                        &offer->desc.fmt[o_fmt_idx]);
    if (!attr) {
        pj_assert(!"Bug! Offer haven't been validated");
        return PJ_EBUG;
    }
    pjmedia_sdp_attr_get_rtpmap(attr, &o_rtpmap);

    /* Get the format rtpmap from the answer. */
    attr = pjmedia_sdp_media_find_attr2(answer, "rtpmap", 
                                        &answer->desc.fmt[a_fmt_idx]);
    if (!attr) {
        pj_assert(!"Bug! Answer haven't been validated");
        return PJ_EBUG;
    }
    pjmedia_sdp_attr_get_rtpmap(attr, &a_rtpmap);

    if (pj_stricmp(&o_rtpmap.enc_name, &a_rtpmap.enc_name) != 0 ||
        (o_rtpmap.clock_rate != a_rtpmap.clock_rate) ||
        (!(pj_stricmp(&o_rtpmap.param, &a_rtpmap.param) == 0 ||
           (a_rtpmap.param.slen == 0 && o_rtpmap.param.slen == 1 &&
            *o_rtpmap.param.ptr == '1') ||
           (o_rtpmap.param.slen == 0 && a_rtpmap.param.slen == 1 &&
            *a_rtpmap.param.ptr=='1'))))
    {
        return PJMEDIA_SDP_EFORMATNOTEQUAL;
    }

    return custom_fmt_match(pool, &o_rtpmap.enc_name,
                            offer, o_fmt_idx, answer, a_fmt_idx, option);
}

