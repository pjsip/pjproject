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
#include <pjmedia/transport_loop.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>


#define THIS_FILE   "transport_loop.c"

/* Upper bound on the number of simultaneously attached users, so the receive
 * fan-out can take a stable stack snapshot of the recipient list. This is a
 * loopback transport used only by tests, which attach a small handful of
 * streams; max_attach_cnt is clamped to this at create time. */
#define LOOP_MAX_USERS  32


struct tp_user
{
    pj_bool_t           rx_disabled;    /**< Doesn't want to receive pkt?   */
    void               *user_data;      /**< Only valid when attached       */
    void  (*rtp_cb)(    void*,          /**< To report incoming RTP.        */
                        void*,
                        pj_ssize_t);
    void  (*rtp_cb2)(   pjmedia_tp_cb_param*);
    void  (*rtcp_cb)(   void*,          /**< To report incoming RTCP.       */
                        void*,
                        pj_ssize_t);
    pj_grp_lock_t      *cb_grp_lock;    /**< Callback owner's group lock,
                                             ref'd across each rx callback. */
};

struct transport_loop
{
    pjmedia_transport   base;           /**< Base transport.                */

    pj_pool_t          *pool;           /**< Memory pool                    */
    unsigned            max_attach_cnt; /**< Max number of attachments      */
    unsigned            user_cnt;       /**< Number of attachments          */
    struct tp_user     *users;          /**< Array of users.                */
    pj_bool_t           disable_rx;     /**< Disable RX.                    */

    pjmedia_loop_tp_setting setting;    /**< Setting.                       */

    unsigned            tx_drop_pct;    /**< Percent of tx pkts to drop.    */
    unsigned            rx_drop_pct;    /**< Percent of rx pkts to drop.    */

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
                                       const pjmedia_sdp_session *sdp_remote,
                                       unsigned media_index);
static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
                                        pj_pool_t *pool,
                                        pjmedia_sdp_session *sdp_local,
                                        const pjmedia_sdp_session *rem_sdp,
                                        unsigned media_index);
static pj_status_t transport_media_start (pjmedia_transport *tp,
                                       pj_pool_t *pool,
                                       const pjmedia_sdp_session *sdp_local,
                                       const pjmedia_sdp_session *sdp_remote,
                                       unsigned media_index);
static pj_status_t transport_media_stop(pjmedia_transport *tp);
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
                                       pjmedia_dir dir,
                                       unsigned pct_lost);
static pj_status_t transport_destroy  (pjmedia_transport *tp);


static pjmedia_transport_op transport_udp_op = 
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

static void tp_loop_on_destroy(void *arg);

/**
 * Initialize loopback media transport setting with its default values.
 */
PJ_DEF(void) pjmedia_loop_tp_setting_default(pjmedia_loop_tp_setting *opt)
{
    pj_bzero(opt, sizeof(pjmedia_loop_tp_setting));
    
    opt->af = pj_AF_INET();
    opt->max_attach_cnt = 4;
}


/**
 * Create loopback transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_loop_create(pjmedia_endpt *endpt,
                                                  pjmedia_transport **p_tp)
{
    pjmedia_loop_tp_setting opt;

    pjmedia_loop_tp_setting_default(&opt);

    return pjmedia_transport_loop_create2(endpt, &opt, p_tp);
}


PJ_DEF(pj_status_t)
pjmedia_transport_loop_create2(pjmedia_endpt *endpt,
                               const pjmedia_loop_tp_setting *opt,
                               pjmedia_transport **p_tp)
{
    struct transport_loop *tp;
    pj_pool_t *pool;
    pj_grp_lock_t *grp_lock;
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && p_tp, PJ_EINVAL);

    /* Create transport structure */
    pool = pjmedia_endpt_create_pool(endpt, "tploop", 512, 512);
    if (!pool)
        return PJ_ENOMEM;

    tp = PJ_POOL_ZALLOC_T(pool, struct transport_loop);
    tp->pool = pool;
    pj_memcpy(tp->base.name, tp->pool->obj_name, PJ_MAX_OBJ_NAME);
    tp->base.op = &transport_udp_op;
    tp->base.type = PJMEDIA_TRANSPORT_TYPE_UDP;

    /* Create group lock */
    status = pj_grp_lock_create(pool, NULL, &grp_lock);
    if (status != PJ_SUCCESS)
        return status;

    tp->base.grp_lock = grp_lock;
    pj_grp_lock_add_ref(grp_lock);
    pj_grp_lock_add_handler(grp_lock, pool, tp, &tp_loop_on_destroy);

    if (opt) {
        tp->setting = *opt;
    } else {
        pjmedia_loop_tp_setting_default(&tp->setting);
    }
    if (tp->setting.addr.slen) {
        pj_strdup(pool, &tp->setting.addr, &opt->addr);
    } else {
        pj_strset2(&tp->setting.addr, (tp->setting.af == pj_AF_INET())?
                                       "127.0.0.1": "::1");
    }
    if (tp->setting.port == 0)
        tp->setting.port = 4000;

    /* alloc users array */
    tp->max_attach_cnt = tp->setting.max_attach_cnt;
    if (tp->max_attach_cnt == 0)
        tp->max_attach_cnt = 1;
    if (tp->max_attach_cnt > LOOP_MAX_USERS) {
        PJ_LOG(3,(THIS_FILE, "Loop transport max_attach_cnt %u clamped to %u",
                  tp->max_attach_cnt, (unsigned)LOOP_MAX_USERS));
        tp->max_attach_cnt = LOOP_MAX_USERS;
    }
    tp->users = (struct tp_user *)pj_pool_calloc(pool, tp->max_attach_cnt, sizeof(struct tp_user));

    /* Done */
    *p_tp = &tp->base;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_transport_loop_disable_rx( pjmedia_transport *tp,
                                                       void *user,
                                                       pj_bool_t disabled)
{
    struct transport_loop *loop = (struct transport_loop*) tp;
    unsigned i;
    pj_status_t status = PJ_ENOTFOUND;

    /* Serialize the users[] search/update with attach/detach and the
     * send-side snapshot via the transport group lock.
     */
    pj_grp_lock_acquire(tp->grp_lock);
    for (i=0; i<loop->user_cnt; ++i) {
        if (loop->users[i].user_data == user) {
            loop->users[i].rx_disabled = disabled;
            status = PJ_SUCCESS;
            break;
        }
    }
    pj_grp_lock_release(tp->grp_lock);

    if (status != PJ_SUCCESS)
        pj_assert(!"Invalid stream user");
    return status;
}


static void tp_loop_on_destroy(void *arg)
{
    struct transport_loop *loop = (struct transport_loop*) arg;

    PJ_LOG(4, (loop->base.name, "Loop transport destroyed"));
    pj_pool_release(loop->pool);
}


/**
 * Close loopback transport.
 */
static pj_status_t transport_destroy(pjmedia_transport *tp)
{
    /* Sanity check */
    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    pj_grp_lock_dec_ref(tp->grp_lock);

    return PJ_SUCCESS;
}


/* Called to get the transport info */
static pj_status_t transport_get_info(pjmedia_transport *tp,
                                      pjmedia_transport_info *info)
{
    struct transport_loop *loop = (struct transport_loop*) tp;

    info->sock_info.rtp_sock = 1;
    pj_sockaddr_init(loop->setting.af, &info->sock_info.rtp_addr_name, 
                     &loop->setting.addr, (pj_uint16_t)loop->setting.port);
    info->sock_info.rtcp_sock = 2;
    pj_sockaddr_init(loop->setting.af, &info->sock_info.rtcp_addr_name,
                     &loop->setting.addr, (pj_uint16_t)loop->setting.port + 1);

    return PJ_SUCCESS;
}


/* Called by application to initialize the transport */
static pj_status_t tp_attach(   pjmedia_transport *tp,
                                       void *user_data,
                                       const pj_sockaddr_t *rem_addr,
                                       const pj_sockaddr_t *rem_rtcp,
                                       unsigned addr_len,
                                       void (*rtp_cb)(void*,
                                                      void*,
                                                      pj_ssize_t),
                                       void (*rtp_cb2)(pjmedia_tp_cb_param*),
                                       void (*rtcp_cb)(void*,
                                                       void*,
                                                       pj_ssize_t),
                                       pj_grp_lock_t *cb_grp_lock)
{
    struct transport_loop *loop = (struct transport_loop*) tp;
    unsigned i;
    const pj_sockaddr *rtcp_addr;

    /* Validate arguments */
    PJ_ASSERT_RETURN(tp && rem_addr && addr_len, PJ_EINVAL);

    PJ_UNUSED_ARG(rem_rtcp);
    PJ_UNUSED_ARG(rtcp_addr);

    /* "Attach" the application under the transport group lock. The
     * duplicate-user and capacity checks run under the same lock as the
     * append (and detach's erase / the send-side snapshot), so two
     * concurrent attaches sharing the last free slot cannot both pass the
     * capacity check and overrun users[].
     */
    pj_grp_lock_acquire(tp->grp_lock);
    for (i=0; i<loop->user_cnt; ++i) {
        if (loop->users[i].user_data == user_data) {
            pj_grp_lock_release(tp->grp_lock);
            return PJ_EINVALIDOP;
        }
    }
    if (loop->user_cnt >= loop->max_attach_cnt) {
        pj_grp_lock_release(tp->grp_lock);
        return PJ_ETOOMANY;
    }
    loop->users[loop->user_cnt].rtp_cb = rtp_cb;
    loop->users[loop->user_cnt].rtp_cb2 = rtp_cb2;
    loop->users[loop->user_cnt].rtcp_cb = rtcp_cb;
    loop->users[loop->user_cnt].user_data = user_data;
    loop->users[loop->user_cnt].cb_grp_lock = cb_grp_lock;
    loop->users[loop->user_cnt].rx_disabled = loop->disable_rx;
    ++loop->user_cnt;
    pj_grp_lock_release(tp->grp_lock);

    return PJ_SUCCESS;
}

static pj_status_t transport_attach(   pjmedia_transport *tp,
                                       void *user_data,
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
    return tp_attach(tp, user_data, rem_addr, rem_rtcp, addr_len,
                     rtp_cb, NULL, rtcp_cb, NULL);
}

static pj_status_t transport_attach2(pjmedia_transport *tp,
                                     pjmedia_transport_attach_param *att_param)
{
    return tp_attach(tp, att_param->user_data,
                            (pj_sockaddr_t*)&att_param->rem_addr,
                            (pj_sockaddr_t*)&att_param->rem_rtcp,
                            att_param->addr_len, att_param->rtp_cb,
                            att_param->rtp_cb2,
                            att_param->rtcp_cb,
                            att_param->grp_lock);
}


/* Called by application when it no longer needs the transport */
static void transport_detach( pjmedia_transport *tp,
                              void *user_data)
{
    struct transport_loop *loop = (struct transport_loop*) tp;
    unsigned i;

    pj_assert(tp);

    /* Serialize the users[] mutation with the send-side snapshot below via
     * the transport group lock, so a concurrent send cannot read/ref an
     * entry (and its owner group lock) while it is being erased.
     */
    pj_grp_lock_acquire(tp->grp_lock);

    for (i=0; i<loop->user_cnt; ++i) {
        if (loop->users[i].user_data == user_data)
            break;
    }

    /* Remove this user */
    if (i != loop->user_cnt) {
        pj_array_erase(loop->users, sizeof(loop->users[0]),
                       loop->user_cnt, i);
        --loop->user_cnt;
    }

    pj_grp_lock_release(tp->grp_lock);
}


/* A stable snapshot of one receiving user, taken under the transport group
 * lock so the fan-out below can invoke callbacks without holding the lock and
 * without a concurrent detach shifting users[] under the iteration. */
struct loop_rcpt {
    void  (*rtp_cb)(void*, void*, pj_ssize_t);
    void  (*rtp_cb2)(pjmedia_tp_cb_param*);
    void  (*rtcp_cb)(void*, void*, pj_ssize_t);
    void   *user_data;
    pj_grp_lock_t *cb_grp_lock;
};

/* Snapshot all rx-enabled users into rcpt[] (capacity LOOP_MAX_USERS, which
 * bounds loop->max_attach_cnt) under the group lock, add_ref'ing each user's
 * owner group lock. Returns the count. Each returned entry's cb_grp_lock (if
 * any) must be dec_ref'd by the caller after the callback. Because the whole
 * recipient set is captured under a single lock hold, a detach that erases
 * (and left-shifts) an entry during the subsequent callbacks cannot cause a
 * still-attached user to be skipped, and the owner ref keeps each callback
 * target alive across its up-call.
 */
static unsigned loop_snapshot_users(struct transport_loop *loop,
                                    struct loop_rcpt *rcpt)
{
    unsigned i, n = 0;

    pj_grp_lock_acquire(loop->base.grp_lock);
    for (i=0; i<loop->user_cnt && n<LOOP_MAX_USERS; ++i) {
        if (loop->users[i].rx_disabled)
            continue;
        rcpt[n].rtp_cb = loop->users[i].rtp_cb;
        rcpt[n].rtp_cb2 = loop->users[i].rtp_cb2;
        rcpt[n].rtcp_cb = loop->users[i].rtcp_cb;
        rcpt[n].user_data = loop->users[i].user_data;
        rcpt[n].cb_grp_lock = loop->users[i].cb_grp_lock;
        if (rcpt[n].cb_grp_lock)
            pj_grp_lock_add_ref(rcpt[n].cb_grp_lock);
        ++n;
    }
    pj_grp_lock_release(loop->base.grp_lock);
    return n;
}


/* Called by application to send RTP packet */
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
                                       const void *pkt,
                                       pj_size_t size)
{
    struct transport_loop *loop = (struct transport_loop*)tp;
    struct loop_rcpt rcpt[LOOP_MAX_USERS];
    unsigned i, n;

    /* Simulate packet lost on TX direction */
    if (loop->tx_drop_pct) {
        if ((pj_rand() % 100) <= (int)loop->tx_drop_pct) {
            PJ_LOG(5,(loop->base.name,
                      "TX RTP packet dropped because of pkt lost "
                      "simulation"));
            return PJ_SUCCESS;
        }
    }

    /* Simulate packet lost on RX direction */
    if (loop->rx_drop_pct) {
        if ((pj_rand() % 100) <= (int)loop->rx_drop_pct) {
            PJ_LOG(5,(loop->base.name,
                      "RX RTP packet dropped because of pkt lost "
                      "simulation"));
            return PJ_SUCCESS;
        }
    }

    pj_grp_lock_add_ref(tp->grp_lock);

    /* Distribute to a stable snapshot of the recipients (see
     * loop_snapshot_users()); the group lock is not held across the up-calls.
     */
    n = loop_snapshot_users(loop, rcpt);
    for (i=0; i<n; ++i) {
        if (rcpt[i].rtp_cb2) {
            pjmedia_tp_cb_param param;

            pj_bzero(&param, sizeof(param));
            param.user_data = rcpt[i].user_data;
            param.pkt = (void *)pkt;
            param.size = size;
            (*rcpt[i].rtp_cb2)(&param);
        } else if (rcpt[i].rtp_cb) {
            (*rcpt[i].rtp_cb)(rcpt[i].user_data, (void*)pkt, size);
        }

        if (rcpt[i].cb_grp_lock)
            pj_grp_lock_dec_ref(rcpt[i].cb_grp_lock);
    }

    pj_grp_lock_dec_ref(tp->grp_lock);

    return PJ_SUCCESS;
}

/* Called by application to send RTCP packet */
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
                                       const void *pkt,
                                       pj_size_t size)
{
    return transport_send_rtcp2(tp, NULL, 0, pkt, size);
}


/* Called by application to send RTCP packet */
static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
                                        const pj_sockaddr_t *addr,
                                        unsigned addr_len,
                                        const void *pkt,
                                        pj_size_t size)
{
    struct transport_loop *loop = (struct transport_loop*)tp;
    struct loop_rcpt rcpt[LOOP_MAX_USERS];
    unsigned i, n;

    PJ_UNUSED_ARG(addr_len);
    PJ_UNUSED_ARG(addr);

    pj_grp_lock_add_ref(tp->grp_lock);

    /* Distribute to a stable snapshot of the recipients (see
     * loop_snapshot_users()); the group lock is not held across the up-calls.
     */
    n = loop_snapshot_users(loop, rcpt);
    for (i=0; i<n; ++i) {
        if (rcpt[i].rtcp_cb)
            (*rcpt[i].rtcp_cb)(rcpt[i].user_data, (void*)pkt, size);

        if (rcpt[i].cb_grp_lock)
            pj_grp_lock_dec_ref(rcpt[i].cb_grp_lock);
    }

    pj_grp_lock_dec_ref(tp->grp_lock);

    return PJ_SUCCESS;
}


static pj_status_t transport_media_create(pjmedia_transport *tp,
                                  pj_pool_t *pool,
                                  unsigned options,
                                  const pjmedia_sdp_session *sdp_remote,
                                  unsigned media_index)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(sdp_remote);
    PJ_UNUSED_ARG(media_index);
    return PJ_SUCCESS;
}

static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
                                        pj_pool_t *pool,
                                        pjmedia_sdp_session *sdp_local,
                                        const pjmedia_sdp_session *rem_sdp,
                                        unsigned media_index)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(sdp_local);
    PJ_UNUSED_ARG(rem_sdp);
    PJ_UNUSED_ARG(media_index);
    return PJ_SUCCESS;
}

static pj_status_t transport_media_start(pjmedia_transport *tp,
                                  pj_pool_t *pool,
                                  const pjmedia_sdp_session *sdp_local,
                                  const pjmedia_sdp_session *sdp_remote,
                                  unsigned media_index)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(sdp_local);
    PJ_UNUSED_ARG(sdp_remote);
    PJ_UNUSED_ARG(media_index);
    return PJ_SUCCESS;
}

static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    PJ_UNUSED_ARG(tp);
    return PJ_SUCCESS;
}

static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
                                           pjmedia_dir dir,
                                           unsigned pct_lost)
{
    struct transport_loop *loop = (struct transport_loop*)tp;

    PJ_ASSERT_RETURN(tp && pct_lost <= 100, PJ_EINVAL);

    if (dir & PJMEDIA_DIR_ENCODING)
        loop->tx_drop_pct = pct_lost;
    
    if (dir & PJMEDIA_DIR_DECODING)
        loop->rx_drop_pct = pct_lost;

    return PJ_SUCCESS;
}

