/* 
 * Copyright (C) 2019-2019 Teluu Inc. (http://www.teluu.com)
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
#include <pj/ssl_sock.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>

#include "ssl_sock_imp_common.h"

/* Workaround for ticket #985 and #1930 */
#ifndef PJ_SSL_SOCK_DELAYED_CLOSE_TIMEOUT
#   define PJ_SSL_SOCK_DELAYED_CLOSE_TIMEOUT    500
#endif

enum { MAX_BIND_RETRY = 100 };

#ifndef SSL_SOCK_IMP_USE_OWN_NETWORK
static pj_bool_t asock_on_data_read (pj_activesock_t *asock,
                                     void *data,
                                     pj_size_t size,
                                     pj_status_t status,
                                     pj_size_t *remainder);

static pj_bool_t asock_on_data_sent (pj_activesock_t *asock,
                                     pj_ioqueue_op_key_t *send_key,
                                     pj_ssize_t sent);
#endif

#ifdef SSL_SOCK_IMP_USE_CIRC_BUF
/*
 *******************************************************************
 * Circular buffer functions.
 *******************************************************************
 */

static pj_size_t next_pow2(pj_size_t n)
{
    /* Next 32-bit power of two */
    pj_size_t power = 1;
    while (power < n && power < 0x8000000) {
        power <<= 1;
    }
    return power;
}

static pj_status_t circ_init(pj_pool_factory *factory,
                             circ_buf_t *cb, pj_size_t cap)
{
    /* Round-up cap */
    cap = next_pow2(cap);

    cb->cap    = cap;
    cb->readp  = 0;
    cb->writep = 0;
    cb->size   = 0;

    /* Initial pool holding the buffer elements */
    cb->pool = pj_pool_create(factory, "tls-circ%p", cap, cap, NULL);
    if (!cb->pool)
        return PJ_ENOMEM;

    /* Allocate circular buffer */
    cb->buf = pj_pool_alloc(cb->pool, cap);
    if (!cb->buf) {
        pj_pool_secure_release(&cb->pool);
        return PJ_ENOMEM;
    }

    return PJ_SUCCESS;
}

static void circ_reset(circ_buf_t* cb)
{
    cb->readp = 0;
    cb->writep = 0;
    cb->size = 0;
}

static void circ_deinit(circ_buf_t *cb)
{
    if (cb->pool) {
        pj_pool_secure_release(&cb->pool);
        cb->pool = NULL;
    }
}

static pj_bool_t circ_empty(const circ_buf_t *cb)
{
    return cb->size == 0;
}

static pj_size_t circ_size(const circ_buf_t *cb)
{
    return cb->size;
}

static pj_size_t circ_avail(const circ_buf_t *cb)
{
    return cb->cap - cb->size;
}

static void circ_read(circ_buf_t *cb, pj_uint8_t *dst, pj_size_t len)
{
    pj_size_t size_after = cb->cap - cb->readp;
    pj_size_t tbc = PJ_MIN(size_after, len);
    pj_size_t rem = len - tbc;

    pj_assert(cb->size >= len);

    pj_memcpy(dst, cb->buf + cb->readp, tbc);
    pj_memcpy(dst + tbc, cb->buf, rem);

    cb->readp += len;
    cb->readp &= (cb->cap - 1);

    cb->size -= len;
}

/* Cancel previous read, partially or fully.
 * Should be called in the same mutex block as circ_read().
 */
static void circ_read_cancel(circ_buf_t* cb, pj_size_t len)
{
    pj_assert(cb->cap - cb->size >= len);

    if (cb->readp < len)
        cb->readp = cb->cap - (len - cb->readp);
    else
        cb->readp -= len;

    cb->size += len;
}

static pj_status_t circ_write(circ_buf_t *cb,
                              const pj_uint8_t *src, pj_size_t len)
{
    /* Overflow condition: resize */
    if (len > circ_avail(cb)) {
        /* Minimum required capacity */
        pj_size_t min_cap = len + cb->size;

        /* Round-up minimum capacity */
        min_cap = next_pow2(min_cap);

        /* Create a new pool to hold a bigger buffer, using the same factory */
        pj_pool_t *pool = pj_pool_create(cb->pool->factory, "tls-circ%p",
                                         min_cap, min_cap, NULL);
        if (!pool)
            return PJ_ENOMEM;

        /* Allocate our new buffer */
        pj_uint8_t *buf = pj_pool_alloc(pool, min_cap);
        if (!buf) {
            pj_pool_release(pool);
            return PJ_ENOMEM;
        }

        /* Save old size, which we shall restore after the next read */
        pj_size_t old_size = cb->size;

        /* Copy old data into beginning of new buffer */
        circ_read(cb, buf, cb->size);

        /* Restore old size now */
        cb->size = old_size;

        /* Release the previous pool */
        pj_pool_secure_release(&cb->pool);

        /* Update circular buffer members */
        cb->pool = pool;
        cb->buf = buf;
        cb->readp = 0;
        cb->writep = cb->size;
        cb->cap = min_cap;
    }

    pj_size_t size_after = cb->cap - cb->writep;
    pj_size_t tbc = PJ_MIN(size_after, len);
    pj_size_t rem = len - tbc;

    pj_memcpy(cb->buf + cb->writep, src, tbc);
    pj_memcpy(cb->buf, src + tbc, rem);

    cb->writep += len;
    cb->writep &= (cb->cap - 1);

    cb->size += len;

    return PJ_SUCCESS;
}
#endif

/*
 *******************************************************************
 * Helper functions.
 *******************************************************************
 */

#ifndef SSL_SOCK_IMP_USE_OWN_NETWORK

/* Check IP address version. */
static int get_ip_addr_ver(const pj_str_t *host)
{
    pj_in_addr dummy;
    pj_in6_addr dummy6;

    /* Check for empty address */
    if (host->slen == 0)
        return 0;

    /* First check if this is an IPv4 address */
    if (pj_inet_pton(pj_AF_INET(), host, &dummy) == PJ_SUCCESS)
        return 4;

    /* Then check if this is an IPv6 address */
    if (pj_inet_pton(pj_AF_INET6(), host, &dummy6) == PJ_SUCCESS)
        return 6;

    /* Not an IP address */
    return 0;
}

/* Close sockets */
static void ssl_close_sockets(pj_ssl_sock_t *ssock)
{
    pj_activesock_t *asock;
    pj_sock_t sock;

    /* This can happen when pj_ssl_sock_create() fails. */
    if (!ssock->write_mutex)
        return;

    pj_lock_acquire(ssock->write_mutex);
    asock = ssock->asock;
    if (asock) {
        // Don't set ssock->asock to NULL, as it may trigger assertion in
        // send operation. This should be safe as active socket will simply
        // return PJ_EINVALIDOP on any operation if it is already closed.
        //ssock->asock = NULL;
        ssock->sock = PJ_INVALID_SOCKET;
    }
    sock = ssock->sock;
    if (sock != PJ_INVALID_SOCKET)
        ssock->sock = PJ_INVALID_SOCKET;
    pj_lock_release(ssock->write_mutex);

    if (asock)
        pj_activesock_close(asock);

    if (sock != PJ_INVALID_SOCKET)
        pj_sock_close(sock);
}
#endif

/* When handshake completed:
 * - notify application
 * - if handshake failed, reset SSL state
 * - return PJ_FALSE when SSL socket instance is destroyed by application.
 */
static pj_bool_t on_handshake_complete(pj_ssl_sock_t *ssock, 
                                       pj_status_t status)
{
    pj_lock_acquire(ssock->write_mutex);
    if (ssock->handshake_status != PJ_EUNKNOWN) {
        pj_lock_release(ssock->write_mutex);
        return (ssock->handshake_status == PJ_SUCCESS)? PJ_TRUE: PJ_FALSE;
    }
    ssock->handshake_status = status;
    pj_lock_release(ssock->write_mutex);

    /* Cancel handshake timer */
    if (ssock->timer.id == TIMER_HANDSHAKE_TIMEOUT) {
        pj_timer_heap_cancel(ssock->param.timer_heap, &ssock->timer);
        ssock->timer.id = TIMER_NONE;
    }

    /* Update certificates info on successful handshake */
    if (status == PJ_SUCCESS)
        ssl_update_certs_info(ssock);

    /* Accepting */
    if (ssock->is_server) {
        pj_bool_t ret = PJ_TRUE;

        if (status != PJ_SUCCESS) {
            /* Handshake failed in accepting, destroy our self silently. */

            char buf1[PJ_INET6_ADDRSTRLEN+10];
            char buf2[PJ_INET6_ADDRSTRLEN+10];

            if (pj_sockaddr_has_addr(&ssock->local_addr))
                pj_sockaddr_print(&ssock->local_addr, buf1, sizeof(buf1), 3);
            else
                pj_ansi_snprintf(buf1, sizeof(buf1), "(unknown)");

            if (pj_sockaddr_has_addr(&ssock->rem_addr))
                pj_sockaddr_print(&ssock->rem_addr, buf2, sizeof(buf2), 3);
            else
                pj_ansi_snprintf(buf2, sizeof(buf2), "(unknown)");

            PJ_PERROR(3,(ssock->pool->obj_name, status,
                      "Handshake failed on %s in accepting %s", buf1, buf2));

            if (ssock->param.cb.on_accept_complete2) {
                (*ssock->param.cb.on_accept_complete2) 
                      (ssock->parent, ssock, (pj_sockaddr_t*)&ssock->rem_addr, 
                      pj_sockaddr_get_len((pj_sockaddr_t*)&ssock->rem_addr), 
                      status);
            }

            /* Decrement ref count of parent */
            if (ssock->parent->param.grp_lock) {
                pj_grp_lock_dec_ref(ssock->parent->param.grp_lock);
                ssock->parent = NULL;
            }

            /* Originally, this is a workaround for ticket #985. However,
             * a race condition may occur in multiple worker threads
             * environment when we are destroying SSL objects while other
             * threads are still accessing them.
             * Please see ticket #1930 for more info.
             */
#if 1 //(defined(PJ_WIN32) && PJ_WIN32!=0)||(defined(PJ_WIN64) && PJ_WIN64!=0)
            if (ssock->param.timer_heap) {
                pj_time_val interval = {0, PJ_SSL_SOCK_DELAYED_CLOSE_TIMEOUT};
                pj_status_t status1;

                ssock->ssl_state = SSL_STATE_NULL;
                ssl_close_sockets(ssock);

                if (ssock->timer.id != TIMER_NONE) {
                    pj_timer_heap_cancel(ssock->param.timer_heap,
                                         &ssock->timer);
                }
                pj_time_val_normalize(&interval);
                status1 = pj_timer_heap_schedule_w_grp_lock(
                                                 ssock->param.timer_heap, 
                                                 &ssock->timer,
                                                 &interval,
                                                 TIMER_CLOSE,
                                                 ssock->param.grp_lock);
                if (status1 != PJ_SUCCESS) {
                    PJ_PERROR(3,(ssock->pool->obj_name, status,
                                 "Failed to schedule a delayed close. "
                                 "Race condition may occur."));
                    ssock->timer.id = TIMER_NONE;
                    pj_ssl_sock_close(ssock);
                }
            } else {
                pj_ssl_sock_close(ssock);
            }
#else
            {
                pj_ssl_sock_close(ssock);
            }
#endif

            return PJ_FALSE;
        }

        /* Notify application the newly accepted SSL socket */
        if (ssock->param.cb.on_accept_complete2) {
            ret = (*ssock->param.cb.on_accept_complete2) 
                    (ssock->parent, ssock, (pj_sockaddr_t*)&ssock->rem_addr, 
                    pj_sockaddr_get_len((pj_sockaddr_t*)&ssock->rem_addr), 
                    status);
        } else if (ssock->param.cb.on_accept_complete) {
            ret = (*ssock->param.cb.on_accept_complete)
                      (ssock->parent, ssock, (pj_sockaddr_t*)&ssock->rem_addr,
                       pj_sockaddr_get_len((pj_sockaddr_t*)&ssock->rem_addr));
        }

        /* Decrement ref count of parent and reset parent (we don't need it
         * anymore, right?).
         */
        if (ssock->parent->param.grp_lock) {
            pj_grp_lock_dec_ref(ssock->parent->param.grp_lock);
            ssock->parent = NULL;
        }

        if (ret == PJ_FALSE)
            return PJ_FALSE;
    }

    /* Connecting */
    else {
        /* On failure, reset SSL socket state first, as app may try to 
         * reconnect in the callback.
         */
        if (status != PJ_SUCCESS) {
            char buf1[PJ_INET6_ADDRSTRLEN+10];
            char buf2[PJ_INET6_ADDRSTRLEN+10];

            if (pj_sockaddr_has_addr(&ssock->local_addr))
                pj_sockaddr_print(&ssock->local_addr, buf1, sizeof(buf1), 3);
            else
                pj_ansi_snprintf(buf1, sizeof(buf1), "(unknown)");

            if (pj_sockaddr_has_addr(&ssock->rem_addr))
                pj_sockaddr_print(&ssock->rem_addr, buf2, sizeof(buf2), 3);
            else
                pj_ansi_snprintf(buf2, sizeof(buf2), "(unknown)");

            PJ_PERROR(3,(ssock->pool->obj_name, status,
                      "Handshake failed on %s in connecting to %s",
                      buf1, buf2));

            /* Server disconnected us, possibly due to SSL nego failure */
            ssl_reset_sock_state(ssock);
        }
        if (ssock->param.cb.on_connect_complete) {
            pj_bool_t ret;
            ret = (*ssock->param.cb.on_connect_complete)(ssock, status);
            if (ret == PJ_FALSE)
                return PJ_FALSE;
        }
    }

    return PJ_TRUE;
}

static ssl_send_op_t* alloc_send_op(pj_ssl_sock_t *ssock, pj_size_t enc_len)
{
    ssl_send_op_t *op;
    pj_pool_t *op_pool;
    pj_size_t alloc_len;

    /* Apply minimum buffer size for better reuse */
    if (enc_len < PJ_SSL_SEND_OP_MIN_BUF_SIZE)
        enc_len = PJ_SSL_SEND_OP_MIN_BUF_SIZE;

    /* Scan free list for first op with sufficient capacity */
    if (!pj_list_empty(&ssock->send_op_free)) {
        ssl_send_op_t *p = ssock->send_op_free.next;
        while (p != &ssock->send_op_free) {
            if (p->enc_buf_cap >= enc_len) {
                pj_list_erase(p);
                ssock->send_op_free_cnt--;
                return p;
            }
            p = p->next;
        }
        /* No suitable op found, allocate new below */
    }

    /* Allocate new op + embedded buffer from its own pool.
     * Each op has its own pool so it can be truly freed when discarded
     * from the free list (cap exceeded).
     */
    alloc_len = sizeof(ssl_send_op_t) - 1 + enc_len;
    op_pool = pj_pool_create(ssock->pool->factory, "ssl_sop",
                             alloc_len + 256, 0, NULL);
    if (!op_pool)
        return NULL;

    op = (ssl_send_op_t *)pj_pool_alloc(op_pool, alloc_len);
    if (!op) {
        pj_pool_release(op_pool);
        return NULL;
    }
    pj_bzero(op, sizeof(ssl_send_op_t) - 1);
    op->pool = op_pool;
    op->enc_buf_cap = enc_len;
    return op;
}

static void free_send_op(pj_ssl_sock_t *ssock, ssl_send_op_t *op)
{
    pj_list_erase(op);
    if (ssock->send_op_active_cnt > 0)
        ssock->send_op_active_cnt--;

    if (ssock->send_op_free_cnt < PJ_SSL_SEND_OP_FREE_LIST_MAX) {
        pj_list_push_back(&ssock->send_op_free, op);
        ssock->send_op_free_cnt++;
    } else {
        /* Free list full, release the pool to truly free memory */
        pj_pool_secure_release(&op->pool);
    }
}

/* Forward declarations */
static pj_status_t ssl_do_handshake_and_flush(pj_ssl_sock_t *ssock);
static pj_bool_t on_handshake_complete(pj_ssl_sock_t *ssock,
                                       pj_status_t status);

/* Enqueue encrypted data from ssl_write_buf into a send op.
 * MUST be called with write_mutex held.
 * On return, *p_drain is PJ_TRUE if caller must call drain_send_queue().
 */
static pj_status_t enqueue_ssl_write_buf(pj_ssl_sock_t *ssock,
                                         pj_ioqueue_op_key_t *send_key,
                                         pj_size_t orig_len,
                                         unsigned flags,
                                         pj_bool_t *p_drain)
{
    pj_ssize_t len;
    ssl_send_op_t *op;

    *p_drain = PJ_FALSE;

    if (io_empty(ssock, &ssock->ssl_write_buf))
        return PJ_SUCCESS;

    len = io_size(ssock, &ssock->ssl_write_buf);
    if (len == 0)
        return PJ_SUCCESS;

    op = alloc_send_op(ssock, len);
    if (!op)
        return PJ_ENOMEM;

    pj_ioqueue_op_key_init(&op->key, sizeof(pj_ioqueue_op_key_t));
    op->key.user_data = op;
    op->app_key = send_key;
    op->plain_data_len = orig_len;
    op->enc_len = len;
    op->flags = flags;
    io_read(ssock, &ssock->ssl_write_buf, (pj_uint8_t *)op->enc_data, len);

    pj_list_push_back(&ssock->send_op_active, op);
    ssock->send_op_active_cnt++;

    if (ssock->sending) {
        /* Another thread/callback is draining. Just queue. */
        return PJ_EPENDING;
    }

    ssock->sending = PJ_TRUE;
    *p_drain = PJ_TRUE;
    return PJ_EPENDING;
}

/* Drain the send_op_active queue. Called WITHOUT holding write_mutex.
 * Caller must have set ssock->sending = PJ_TRUE.
 *
 * suppress_first_cb: when PJ_TRUE, the first synchronously completed
 * op does NOT get its callback fired — the caller (flush_ssl_write_buf)
 * returns the status to the app directly. When PJ_FALSE (called from
 * ssock_on_data_sent), all sync completions fire callbacks because
 * their callers already received PJ_EPENDING.
 */
static pj_status_t drain_send_queue(pj_ssl_sock_t *ssock,
                                    pj_bool_t suppress_first_cb)
{
    pj_status_t first_status = PJ_SUCCESS;
    pj_bool_t is_first = suppress_first_cb;

    for (;;) {
        ssl_send_op_t *op;
        pj_ioqueue_op_key_t *app_key;
        pj_ssize_t plain_data_len;
        pj_ssize_t len;
        pj_status_t status;

        /* Pop the front of the queue — remove BEFORE sending so that
         * ssock_on_data_sent → drain_send_queue won't double-send.
         * Set send_op_inflight BEFORE releasing the lock so that
         * ssl_on_destroy can find it if the socket closes between
         * the send and the async callback.
         */
        pj_lock_acquire(ssock->write_mutex);
        if (pj_list_empty(&ssock->send_op_active)) {
            ssock->sending = PJ_FALSE;
            pj_lock_release(ssock->write_mutex);
            break;
        }
        op = ssock->send_op_active.next;
        pj_list_erase(op);
        pj_assert(ssock->send_op_inflight == NULL);
        ssock->send_op_inflight = op;
        pj_lock_release(ssock->write_mutex);

        /* Send WITHOUT holding write_mutex — no deadlock possible */
        len = op->enc_len;
#ifdef SSL_SOCK_IMP_USE_OWN_NETWORK
        status = network_send(ssock, &op->key, op->enc_data, &len,
                              op->flags);
#else
        if (ssock->param.sock_type == pj_SOCK_STREAM()) {
            status = pj_activesock_send(ssock->asock, &op->key,
                                        op->enc_data, &len, op->flags);
        } else {
            status = pj_activesock_sendto(ssock->asock, &op->key,
                                          op->enc_data, &len, op->flags,
                                          (pj_sockaddr_t*)&ssock->rem_addr,
                                          ssock->addr_len);
        }
#endif

        if (status == PJ_EPENDING) {
            /* Async — ssock_on_data_sent will resume draining.
             * send_op_inflight is already set above under write_mutex.
             */
            if (is_first)
                first_status = PJ_EPENDING;
            break;
        }

        /* Completed synchronously — clear inflight, free op */
        app_key = op->app_key;
        plain_data_len = (pj_ssize_t)op->plain_data_len;

        pj_lock_acquire(ssock->write_mutex);
        ssock->send_op_inflight = NULL;
        free_send_op(ssock, op);
        pj_lock_release(ssock->write_mutex);

        if (status != PJ_SUCCESS) {
            /* Send error — stop draining */
            pj_lock_acquire(ssock->write_mutex);
            ssock->sending = PJ_FALSE;
            pj_lock_release(ssock->write_mutex);

            if (is_first) {
                first_status = status;
            } else if (app_key != &ssock->handshake_op_key &&
                       app_key != &ssock->shutdown_op_key &&
                       ssock->param.cb.on_data_sent)
            {
                /* Non-first op's caller got PJ_EPENDING — must fire
                 * an error callback since the op was already freed.
                 */
                pj_bool_t ret;
                ret = (*ssock->param.cb.on_data_sent)(ssock, app_key,
                                                      -(pj_ssize_t)status);
                if (!ret) {
                    return first_status;  /* destroyed */
                }
            }
            break;
        }

        if (is_first) {
            first_status = PJ_SUCCESS;
            is_first = PJ_FALSE;
            continue;  /* first op: caller returns status directly */
        }

        /* Non-first op completed sync: its caller got PJ_EPENDING,
         * so we must fire the callback here.
         */
        if (ssock->ssl_state == SSL_STATE_HANDSHAKING) {
            pj_status_t hs;
            hs = ssl_do_handshake_and_flush(ssock);
            if (hs != PJ_EPENDING) {
                if (!on_handshake_complete(ssock, hs))
                    return first_status;
            }
        } else if (app_key != &ssock->handshake_op_key &&
                   app_key != &ssock->shutdown_op_key)
        {
            if (ssock->param.cb.on_data_sent) {
                pj_bool_t ret;
                ret = (*ssock->param.cb.on_data_sent)(ssock, app_key,
                                                       plain_data_len);
                if (!ret)
                    return first_status;  /* destroyed */
            }
        }

        /* After processing a handshake op, try to flush delayed sends.
         * During renegotiation, sends are delayed in write_pending.
         * When the last handshake op completes (renego done), the
         * delayed sends can now proceed.
         */
        if (!pj_list_empty(&ssock->write_pending)) {
            flush_delayed_send(ssock);
        }
    }

    return first_status;
}

/* Flush SSL write buffer to network socket. */
static pj_status_t flush_ssl_write_buf(pj_ssl_sock_t *ssock,
                                       pj_ioqueue_op_key_t *send_key,
                                       pj_size_t orig_len, unsigned flags)
{
    pj_bool_t should_drain;
    pj_status_t status;

    pj_lock_acquire(ssock->write_mutex);
    status = enqueue_ssl_write_buf(ssock, send_key, orig_len, flags,
                                   &should_drain);
    pj_lock_release(ssock->write_mutex);

    if (should_drain)
        status = drain_send_queue(ssock, PJ_TRUE);

    return status;
}

/* ssl_do_handshake_and_flush: common wrapper that calls the backend's
 * ssl_do_handshake() then flushes any handshake data from ssl_write_buf.
 * This unifies error handling across all SSL backends.
 */
static pj_status_t ssl_do_handshake_and_flush(pj_ssl_sock_t *ssock)
{
    pj_status_t status;
    pj_status_t flush_status;

    status = ssl_do_handshake(ssock);

    /* Flush any handshake data produced by the SSL library.
     * Must flush unconditionally — even on handshake failure, the SSL
     * library may have queued error alert records that should be sent
     * to the peer.
     */
    flush_status = flush_ssl_write_buf(ssock,
                                       &ssock->handshake_op_key, 0, 0);
    if (status == PJ_SUCCESS || status == PJ_EPENDING) {
        /* Only override status with flush error when handshake itself
         * was still in progress. Don't mask handshake failure.
         */
        if (flush_status != PJ_SUCCESS && flush_status != PJ_EPENDING) {
            status = flush_status;
        }
    }

    return status;
}

static void on_timer(pj_timer_heap_t *th, struct pj_timer_entry *te)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)te->user_data;
    int timer_id = te->id;

    te->id = TIMER_NONE;

    PJ_UNUSED_ARG(th);

    switch (timer_id) {
    case TIMER_HANDSHAKE_TIMEOUT:
        PJ_LOG(1,(ssock->pool->obj_name, "SSL timeout after %ld.%lds",
                  ssock->param.timeout.sec, ssock->param.timeout.msec));

        on_handshake_complete(ssock, PJ_ETIMEDOUT);
        break;
    case TIMER_CLOSE:
        pj_ssl_sock_close(ssock);
        break;
    default:
        pj_assert(!"Unknown timer");
        break;
    }
}

/* Fire error callbacks for pending sends that will never complete.
 * Called from pj_ssl_sock_close() so callbacks fire synchronously
 * while the app's resources are still valid. Must be called AFTER
 * ssl_reset_sock_state (sockets closed, no more async completions).
 */
static void cancel_pending_sends(pj_ssl_sock_t *ssock)
{
    /* In-flight send_op — removed from send_op_active but pending in
     * ioqueue. Socket close cancelled it without callback.
     */
    if (ssock->send_op_inflight) {
        ssl_send_op_t *op = ssock->send_op_inflight;
        pj_ioqueue_op_key_t *app_key = op->app_key;

        ssock->send_op_inflight = NULL;

        if (app_key != &ssock->handshake_op_key &&
            app_key != &ssock->shutdown_op_key &&
            ssock->param.cb.on_data_sent)
        {
            (*ssock->param.cb.on_data_sent)(ssock, app_key,
                                            -PJ_ECANCELLED);
        }

        pj_lock_acquire(ssock->write_mutex);
        free_send_op(ssock, op);
        pj_lock_release(ssock->write_mutex);
    }

    /* Active send_ops — encrypted data queued but not yet sent */
    while (!pj_list_empty(&ssock->send_op_active)) {
        ssl_send_op_t *op = ssock->send_op_active.next;
        pj_ioqueue_op_key_t *app_key = op->app_key;

        pj_list_erase(op);

        if (app_key != &ssock->handshake_op_key &&
            app_key != &ssock->shutdown_op_key &&
            ssock->param.cb.on_data_sent)
        {
            (*ssock->param.cb.on_data_sent)(ssock, app_key,
                                            -PJ_ECANCELLED);
        }

        pj_lock_acquire(ssock->write_mutex);
        free_send_op(ssock, op);
        pj_lock_release(ssock->write_mutex);
    }

    /* Delayed sends from renegotiation — never encrypted */
    while (!pj_list_empty(&ssock->write_pending)) {
        write_data_t *wp = ssock->write_pending.next;
        pj_ioqueue_op_key_t *app_key = wp->app_key;

        pj_list_erase(wp);

        if (app_key != &ssock->handshake_op_key &&
            app_key != &ssock->shutdown_op_key &&
            ssock->param.cb.on_data_sent)
        {
            (*ssock->param.cb.on_data_sent)(ssock, app_key,
                                            -PJ_ECANCELLED);
        }
    }
}

static void ssl_on_destroy(void *arg)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)arg;

    ssl_destroy(ssock);

    /* Defensive: free any remaining ops without callbacks.
     * Normally cancel_pending_sends() in pj_ssl_sock_close() already
     * drained these, but handle the case where destroy fires without
     * a prior close (e.g., grp_lock ref dropped externally).
     */
    if (ssock->send_op_inflight) {
        pj_pool_secure_release(&ssock->send_op_inflight->pool);
        ssock->send_op_inflight = NULL;
    }
    while (!pj_list_empty(&ssock->send_op_active)) {
        ssl_send_op_t *op = ssock->send_op_active.next;
        pj_list_erase(op);
        pj_pool_secure_release(&op->pool);
    }

    /* Release all send op pools (each op has its own pool) */
    while (!pj_list_empty(&ssock->send_op_free)) {
        ssl_send_op_t *op = ssock->send_op_free.next;
        pj_list_erase(op);
        pj_pool_secure_release(&op->pool);
    }
    ssock->send_op_free_cnt = 0;

    if (ssock->ssl_read_buf_mutex) {
        pj_lock_destroy(ssock->ssl_read_buf_mutex);
        ssock->ssl_read_buf_mutex = NULL;
    }

    if (ssock->ssl_write_buf_mutex) {
        pj_lock_destroy(ssock->ssl_write_buf_mutex);
        ssock->ssl_write_buf_mutex = NULL;
        ssock->write_mutex = NULL;
    }

    /* Secure release pool, i.e: all memory blocks will be zeroed first */
    pj_pool_secure_release(&ssock->info_pool);
    pj_pool_secure_release(&ssock->pool);
}


/*
 *******************************************************************
 * Network callbacks.
 *******************************************************************
 */

/*
 * Get the offset of pointer to read-buffer of SSL socket from read-buffer
 * of active socket. Note that both SSL socket and active socket employ 
 * different but correlated read-buffers (as much as async_cnt for each),
 * and to make it easier/faster to find corresponding SSL socket's read-buffer
 * from known active socket's read-buffer, the pointer of corresponding 
 * SSL socket's read-buffer is stored right after the end of active socket's
 * read-buffer.
 */
#define OFFSET_OF_READ_DATA_PTR(ssock, asock_rbuf) \
                                        (read_data_t**) \
                                        ((pj_int8_t*)(asock_rbuf) + \
                                        ssock->param.read_buffer_size)

static pj_bool_t ssock_on_data_read (pj_ssl_sock_t *ssock,
                                     void *data,
                                     pj_size_t size,
                                     pj_status_t status,
                                     pj_size_t *remainder)
{
    if (status != PJ_SUCCESS)
        goto on_error;

    if (data && size > 0) {
        pj_status_t status_;

        /* Consume the whole data */
        if (ssock->ssl_read_buf_mutex)
            pj_lock_acquire(ssock->ssl_read_buf_mutex);
        status_ = io_write(ssock,&ssock->ssl_read_buf, data, size);
        if (ssock->ssl_read_buf_mutex)
            pj_lock_release(ssock->ssl_read_buf_mutex);
        if (status_ != PJ_SUCCESS) {
            status = status_;
            goto on_error;
        }
    }

    /* Check if SSL handshake hasn't finished yet */
    if (ssock->ssl_state == SSL_STATE_HANDSHAKING) {
        pj_bool_t ret = PJ_TRUE;

        if (status == PJ_SUCCESS)
            status = ssl_do_handshake_and_flush(ssock);

        /* Not pending is either success or failed */
        if (status != PJ_EPENDING)
            ret = on_handshake_complete(ssock, status);

        return ret;
    }

    /* See if there is any decrypted data for the application */
    if (data && ssock->read_started) {
        do {
            read_data_t *buf = *(OFFSET_OF_READ_DATA_PTR(ssock, data));
            void *data_ = (pj_int8_t*)buf->data + buf->len;
            int size_ = (int)(ssock->read_size - buf->len);
            pj_status_t status_;

            status_ = ssl_read(ssock, data_, &size_);

            if (size_ > 0) {
                if (ssock->param.cb.on_data_read) {
                    pj_bool_t ret;
                    pj_size_t remainder_ = 0;

                    buf->len += size_;

                    ret = (*ssock->param.cb.on_data_read)(ssock, buf->data,
                                                          buf->len, status,
                                                          &remainder_);
                    if (!ret) {
                        /* We've been destroyed */
                        return PJ_FALSE;
                    }

                    /* Application may have left some data to be consumed 
                     * later.
                     */
                    buf->len = remainder_;
                }

                /* Active socket signalled connection closed/error, this has
                 * been signalled to the application along with any remaining
                 * buffer. So, let's just reset SSL socket now.
                 */
                /*
                // This has been handled in on_error
                if (status != PJ_SUCCESS) {
                    ssl_reset_sock_state(ssock);
                    return PJ_FALSE;
                }
                */

            } else if (status_ == PJ_SUCCESS) {
                break;
            } else if (status_ == PJ_ETRYAGAIN) {
                /* SSL_read may have produced handshake data (e.g.
                 * ServerHello during renegotiation). Flush it before
                 * calling ssl_do_handshake which may not produce more.
                 */
                flush_ssl_write_buf(ssock, &ssock->handshake_op_key,
                                    0, 0);
                status = ssl_do_handshake_and_flush(ssock);
                if (status == PJ_SUCCESS) {
                    /* Renegotiation completed */

                    /* Update certificates */
                    ssl_update_certs_info(ssock);

                    // Ticket #1573: Don't hold mutex while calling
                    //               PJLIB socket send().
                    //pj_lock_acquire(ssock->write_mutex);
                    status = flush_delayed_send(ssock);
                    //pj_lock_release(ssock->write_mutex);

                    /* If flushing is ongoing, treat it as success */
                    if (status == PJ_EBUSY)
                        status = PJ_SUCCESS;

                    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
                        PJ_PERROR(1,(ssock->pool->obj_name, status, 
                                     "Failed to flush delayed send"));
                        goto on_error;
                    }

                    /* If renego has been completed, continue reading data */
                    if (status == PJ_SUCCESS)
                        continue;

                } else if (status != PJ_EPENDING) {
                    PJ_PERROR(1,(ssock->pool->obj_name, status, 
                                 "Renegotiation failed"));
                    goto on_error;
                }

                break;
            } else {
                /* Error */
                status = status_;
                goto on_error;
            }

        } while (1);
    }

    /* SSL_read may have generated protocol-level responses in the
     * write buffer (e.g. renegotiation rejection alert, session
     * ticket, etc.) that must be sent to the peer. Flush them.
     */
    flush_ssl_write_buf(ssock, &ssock->handshake_op_key, 0, 0);

    /* Flush delayed sends that may have been queued during
     * renegotiation. OpenSSL handles renegotiation transparently
     * inside SSL_read, so by the time ssl_read returns application
     * data again, the renegotiation has completed and delayed sends
     * can proceed.
     */
    if (!pj_list_empty(&ssock->write_pending)) {
        flush_delayed_send(ssock);
    }

    return PJ_TRUE;

on_error:
    if (ssock->ssl_state == SSL_STATE_HANDSHAKING)
        return on_handshake_complete(ssock, status);

    if (ssock->read_started && ssock->param.cb.on_data_read) {
        pj_bool_t ret;
        ret = (*ssock->param.cb.on_data_read)(ssock, NULL, 0, status,
                                              remainder);
        if (!ret) {
            /* We've been destroyed */
            return PJ_FALSE;
        }
    }

    ssl_reset_sock_state(ssock);
    return PJ_FALSE;
}

static pj_bool_t ssock_on_data_sent (pj_ssl_sock_t *ssock,
                                     pj_ioqueue_op_key_t *send_key,
                                     pj_ssize_t sent)
{
    ssl_send_op_t *op;
    pj_ioqueue_op_key_t *app_key;
    pj_ssize_t sent_len;

    /* Skip late callbacks arriving after close (e.g., IOCP cancelled
     * completions, or ioqueue drain on epoll/select). All pending ops
     * are handled by cancel_pending_sends() in pj_ssl_sock_close().
     */
    if (ssock->is_closing)
        return PJ_FALSE;

    op = (ssl_send_op_t *)send_key->user_data;
    app_key = op->app_key;
    sent_len = (sent > 0) ? (pj_ssize_t)op->plain_data_len : sent;

    /* Clear in-flight tracking and free the completed send op */
    pj_lock_acquire(ssock->write_mutex);
    ssock->send_op_inflight = NULL;
    free_send_op(ssock, op);
    pj_lock_release(ssock->write_mutex);
    op = NULL;

    if (ssock->ssl_state == SSL_STATE_HANDSHAKING) {
        /* Handshaking — continue handshake */
        pj_status_t status;

        status = ssl_do_handshake_and_flush(ssock);
        if (status != PJ_EPENDING) {
            pj_bool_t ret = on_handshake_complete(ssock, status);
            if (!ret)
                return PJ_FALSE;
            /* Fall through to drain remaining queue */
        }

    } else if (app_key != &ssock->handshake_op_key &&
               app_key != &ssock->shutdown_op_key)
    {
        /* Application data — notify application */
        if (ssock->param.cb.on_data_sent) {
            pj_bool_t ret;
            ret = (*ssock->param.cb.on_data_sent)(ssock, app_key,
                                                  sent_len);
            if (!ret) {
                /* We've been destroyed */
                return PJ_FALSE;
            }
        }
    }

    /* Drain write_pending (delayed sends from renegotiation) */
    if (!pj_list_empty(&ssock->write_pending)) {
        flush_delayed_send(ssock);
    }

    /* Continue draining the send queue — ssock->sending is still
     * PJ_TRUE from the original drain_send_queue call. Don't
     * suppress callbacks: all remaining ops' callers got PJ_EPENDING.
     */
    drain_send_queue(ssock, PJ_FALSE);

    return PJ_TRUE;
}

static pj_status_t get_localaddr(pj_ssl_sock_t *ssock,
                                 pj_sockaddr_t *addr,
                                 int *namelen)
{
    PJ_UNUSED_ARG(addr);
    PJ_UNUSED_ARG(namelen);

#ifdef SSL_SOCK_IMP_USE_OWN_NETWORK
    return network_get_localaddr(ssock, &ssock->local_addr, 
                                 &ssock->addr_len);
#else
    return pj_sock_getsockname(ssock->sock, &ssock->local_addr, 
                               &ssock->addr_len);
#endif
}


static pj_bool_t ssock_on_accept_complete (pj_ssl_sock_t *ssock_parent,
                                           pj_sock_t newsock,
                                           void *newconn,
                                           const pj_sockaddr_t *src_addr,
                                           int src_addr_len,
                                           pj_status_t accept_status)
{
    pj_ssl_sock_t *ssock = NULL;
#ifndef SSL_SOCK_IMP_USE_OWN_NETWORK
    pj_activesock_cb asock_cb;
    pj_activesock_cfg asock_cfg;
#endif
    unsigned i;
    pj_status_t status;

#ifndef SSL_SOCK_IMP_USE_OWN_NETWORK
    PJ_UNUSED_ARG(newconn);
#endif

    /* Set close-on-exec flag */
    if (ssock_parent->newsock_param.sock_cloexec)
        pj_set_cloexec_flag((int)newsock);

    if (accept_status != PJ_SUCCESS) {
        if (ssock_parent->param.cb.on_accept_complete2) {
            (*ssock_parent->param.cb.on_accept_complete2)(ssock_parent, NULL,
                                                          src_addr,
                                                          src_addr_len,
                                                          accept_status);
        }
        return PJ_TRUE;
    }

    /* Create new SSL socket instance */
    status = pj_ssl_sock_create(ssock_parent->pool,
                                &ssock_parent->newsock_param, &ssock);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Set parent and add ref count (avoid parent destroy during handshake) */
    ssock->parent = ssock_parent;
    if (ssock->parent->param.grp_lock)
        pj_grp_lock_add_ref(ssock->parent->param.grp_lock);

    /* Update new SSL socket attributes */
    ssock->sock = newsock;
    ssock->is_server = PJ_TRUE;
    if (ssock_parent->cert) {
        status = pj_ssl_sock_set_certificate(ssock, ssock->pool, 
                                             ssock_parent->cert);
        if (status != PJ_SUCCESS)
            goto on_return;
    }

    /* Set local address */
    ssock->addr_len = src_addr_len;
    pj_sockaddr_cp(&ssock->local_addr, &ssock_parent->local_addr);

    /* Set remote address */
    pj_sockaddr_cp(&ssock->rem_addr, src_addr);

    /* Create SSL context */
    status = ssl_create(ssock);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Set peer name */
    ssl_set_peer_name(ssock);

    /* Prepare read buffer */
    ssock->asock_rbuf = (void**)pj_pool_calloc(ssock->pool, 
                                               ssock->param.async_cnt,
                                               sizeof(void*));
    if (!ssock->asock_rbuf) {
        status = PJ_ENOMEM;
        goto on_return;
    }

    for (i = 0; i<ssock->param.async_cnt; ++i) {
        ssock->asock_rbuf[i] = (void*) pj_pool_alloc(
                                            ssock->pool, 
                                            ssock->param.read_buffer_size + 
                                            sizeof(read_data_t*));
        if (!ssock->asock_rbuf[i]) {
            status = PJ_ENOMEM;
            goto on_return;
        }
    }

    /* If listener socket has group lock, automatically create group lock
     * for the new socket.
     */
    if (ssock_parent->param.grp_lock) {
        pj_grp_lock_t *glock;

        status = pj_grp_lock_create(ssock->pool, NULL, &glock);
        if (status != PJ_SUCCESS)
            goto on_return;

        pj_grp_lock_add_ref(glock);
        ssock->param.grp_lock = glock;
        pj_grp_lock_add_handler(ssock->param.grp_lock, ssock->pool, ssock,
                                ssl_on_destroy);
    }

#ifdef SSL_SOCK_IMP_USE_OWN_NETWORK
    status = network_setup_connection(ssock, newconn);
    if (status != PJ_SUCCESS)
        goto on_return;

#else
    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(ssock->sock, ssock->param.qos_type,
                                &ssock->param.qos_params, 1, 
                                ssock->pool->obj_name, NULL);
    if (status != PJ_SUCCESS && !ssock->param.qos_ignore_error)
        goto on_return;

    /* Apply socket options, if specified */
    if (ssock->param.sockopt_params.cnt) {
        status = pj_sock_setsockopt_params(ssock->sock, 
                                           &ssock->param.sockopt_params);
        if (status != PJ_SUCCESS && !ssock->param.sockopt_ignore_error)
            goto on_return;
    }

    /* Create active socket */
    pj_activesock_cfg_default(&asock_cfg);
    asock_cfg.grp_lock = ssock->param.grp_lock;
    asock_cfg.async_cnt = ssock->param.async_cnt;
    asock_cfg.concurrency = ssock->param.concurrency;
    asock_cfg.whole_data = PJ_TRUE;

    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_data_read = asock_on_data_read;
    asock_cb.on_data_sent = asock_on_data_sent;

    status = pj_activesock_create(ssock->pool,
                                  ssock->sock, 
                                  ssock->param.sock_type,
                                  &asock_cfg,
                                  ssock->param.ioqueue, 
                                  &asock_cb,
                                  ssock,
                                  &ssock->asock);

    if (status != PJ_SUCCESS)
        goto on_return;

    /* Start read */
    status = pj_activesock_start_read2(ssock->asock, ssock->pool, 
                                       (unsigned)ssock->param.read_buffer_size,
                                       ssock->asock_rbuf,
                                       PJ_IOQUEUE_ALWAYS_ASYNC);
    if (status != PJ_SUCCESS)
        goto on_return;
#endif

    /* Update local address */
    status = get_localaddr(ssock, &ssock->local_addr, &ssock->addr_len);
    if (status != PJ_SUCCESS) {
        /* This fails on few envs, e.g: win IOCP, just tolerate this and
         * use parent local address instead.
         */
        pj_sockaddr_cp(&ssock->local_addr, &ssock_parent->local_addr);
    }

    /* Start handshake timer */
    if (ssock->param.timer_heap && (ssock->param.timeout.sec != 0 ||
        ssock->param.timeout.msec != 0))
    {
        pj_assert(ssock->timer.id == TIMER_NONE);
        status = pj_timer_heap_schedule_w_grp_lock(ssock->param.timer_heap, 
                                                   &ssock->timer,
                                                   &ssock->param.timeout,
                                                   TIMER_HANDSHAKE_TIMEOUT,
                                                   ssock->param.grp_lock);
        if (status != PJ_SUCCESS) {
            ssock->timer.id = TIMER_NONE;
        }
    }

    /* Start SSL handshake */
    /* Use write_mutex (not ssl_read_buf_mutex) to serialise the
     * ssl_state transition and the first ssl_do_handshake() call
     * against a concurrent ssock_on_data_read() that could fire on
     * the newly-registered ioqueue key.  write_mutex is the correct
     * outer lock here because backends that touch shared SSL session
     * state acquire write_mutex internally, and the consistent lock
     * order everywhere is: write_mutex -> ssl_read_buf_mutex.
     */
    pj_lock_acquire(ssock->write_mutex);
    ssock->ssl_state = SSL_STATE_HANDSHAKING;
    ssl_set_state(ssock, PJ_TRUE);
    status = ssl_do_handshake_and_flush(ssock);
    pj_lock_release(ssock->write_mutex);

on_return:
    if (ssock && status != PJ_EPENDING) {
        on_handshake_complete(ssock, status);
    }

    /* Must return PJ_TRUE whatever happened, as we must continue listening */
    return PJ_TRUE;
}

static pj_bool_t ssock_on_connect_complete (pj_ssl_sock_t *ssock,
                                            pj_status_t status)
{
    unsigned i;

    if (status != PJ_SUCCESS)
        goto on_return;

    /* Update local address */
    ssock->addr_len = sizeof(pj_sockaddr);
    status = get_localaddr(ssock, &ssock->local_addr, &ssock->addr_len);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Create SSL context */
    status = ssl_create(ssock);
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Prepare read buffer */
    ssock->asock_rbuf = (void**)pj_pool_calloc(ssock->pool,
                                               ssock->param.async_cnt,
                                               sizeof(void*));
    if (!ssock->asock_rbuf) {
        status = PJ_ENOMEM;
        goto on_return;
    }

    for (i = 0; i<ssock->param.async_cnt; ++i) {
        ssock->asock_rbuf[i] = (void*) pj_pool_alloc(
                                            ssock->pool,
                                            ssock->param.read_buffer_size +
                                            sizeof(read_data_t*));
        if (!ssock->asock_rbuf[i]) {
            status = PJ_ENOMEM;
            goto on_return;
        }
    }

    /* Start read */
#ifdef SSL_SOCK_IMP_USE_OWN_NETWORK
    status = network_start_read(ssock, ssock->param.async_cnt,
                                (unsigned)ssock->param.read_buffer_size,
                                ssock->asock_rbuf, 0);
#else
    status = pj_activesock_start_read2(ssock->asock, ssock->pool, 
                                       (unsigned)ssock->param.read_buffer_size,
                                       ssock->asock_rbuf,
                                       PJ_IOQUEUE_ALWAYS_ASYNC);
#endif
    if (status != PJ_SUCCESS)
        goto on_return;

    /* Set peer name */
    ssl_set_peer_name(ssock);

    /* Start SSL handshake */
    ssock->ssl_state = SSL_STATE_HANDSHAKING;
    ssl_set_state(ssock, PJ_FALSE);

    status = ssl_do_handshake_and_flush(ssock);
    if (status != PJ_EPENDING)
        goto on_return;

    return PJ_TRUE;

on_return:
    return on_handshake_complete(ssock, status);
}

#ifndef SSL_SOCK_IMP_USE_OWN_NETWORK
static pj_bool_t asock_on_data_read (pj_activesock_t *asock,
                                     void *data,
                                     pj_size_t size,
                                     pj_status_t status,
                                     pj_size_t *remainder)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)
                           pj_activesock_get_user_data(asock);

    return ssock_on_data_read(ssock, data, size, status, remainder);
}

static pj_bool_t asock_on_data_sent (pj_activesock_t *asock,
                                     pj_ioqueue_op_key_t *send_key,
                                     pj_ssize_t sent)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)
                           pj_activesock_get_user_data(asock);

    return ssock_on_data_sent(ssock, send_key, sent);
}

static pj_bool_t asock_on_accept_complete2(pj_activesock_t *asock,
                                           pj_sock_t newsock,
                                           const pj_sockaddr_t *src_addr,
                                           int src_addr_len,
                                           pj_status_t status)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)
                           pj_activesock_get_user_data(asock);

    return ssock_on_accept_complete(ssock, newsock, NULL,
                                    src_addr, src_addr_len, status);
}

static pj_bool_t asock_on_connect_complete (pj_activesock_t *asock,
                                            pj_status_t status)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)
                           pj_activesock_get_user_data(asock);

    return ssock_on_connect_complete(ssock, status);
}
#endif

/*
 *******************************************************************
 * API
 *******************************************************************
 */

/* Get available ciphers. */
PJ_DEF(pj_status_t) pj_ssl_cipher_get_availables(pj_ssl_cipher ciphers[],
                                                 unsigned *cipher_num)
{
    unsigned i;

    PJ_ASSERT_RETURN(ciphers && cipher_num, PJ_EINVAL);

    ssl_ciphers_populate();

    if (ssl_cipher_num == 0) {
        *cipher_num = 0;
        return PJ_ENOTFOUND;
    }

    *cipher_num = PJ_MIN(*cipher_num, ssl_cipher_num);

    for (i = 0; i < *cipher_num; ++i)
        ciphers[i] = ssl_ciphers[i].id;

    return PJ_SUCCESS;
}

/* Get cipher name string */
PJ_DEF(const char*) pj_ssl_cipher_name(pj_ssl_cipher cipher)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_cipher_num; ++i) {
        if (cipher == ssl_ciphers[i].id)
            return ssl_ciphers[i].name;
    }

    return NULL;
}

/* Get cipher identifier */
PJ_DEF(pj_ssl_cipher) pj_ssl_cipher_id(const char *cipher_name)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_cipher_num; ++i) {
        if (!pj_ansi_stricmp(ssl_ciphers[i].name, cipher_name))
            return ssl_ciphers[i].id;
    }

    return PJ_TLS_UNKNOWN_CIPHER;
}

/* Check if the specified cipher is supported by SSL/TLS backend. */
PJ_DEF(pj_bool_t) pj_ssl_cipher_is_supported(pj_ssl_cipher cipher)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_cipher_num; ++i) {
        if (cipher == ssl_ciphers[i].id)
            return PJ_TRUE;
    }

    return PJ_FALSE;
}

/* Get available curves. */
PJ_DEF(pj_status_t) pj_ssl_curve_get_availables(pj_ssl_curve curves[],
                                                unsigned *curve_num)
{
    unsigned i;

    PJ_ASSERT_RETURN(curves && curve_num, PJ_EINVAL);

    ssl_ciphers_populate();

    if (ssl_curves_num == 0) {
        *curve_num = 0;
        return PJ_ENOTFOUND;
    }

    *curve_num = PJ_MIN(*curve_num, ssl_curves_num);

    for (i = 0; i < *curve_num; ++i)
        curves[i] = ssl_curves[i].id;

    return PJ_SUCCESS;
}

/* Get curve name string. */
PJ_DEF(const char*) pj_ssl_curve_name(pj_ssl_curve curve)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_curves_num; ++i) {
        if (curve == ssl_curves[i].id)
            return ssl_curves[i].name;
    }

    return NULL;
}

/* Get curve ID from curve name string. */
PJ_DEF(pj_ssl_curve) pj_ssl_curve_id(const char *curve_name)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_curves_num; ++i) {
        if (ssl_curves[i].name &&
                !pj_ansi_stricmp(ssl_curves[i].name, curve_name))
        {
            return ssl_curves[i].id;
        }
    }

    return PJ_TLS_UNKNOWN_CURVE;
}

/* Check if the specified curve is supported by SSL/TLS backend. */
PJ_DEF(pj_bool_t) pj_ssl_curve_is_supported(pj_ssl_curve curve)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_curves_num; ++i) {
        if (curve == ssl_curves[i].id)
            return PJ_TRUE;
    }

    return PJ_FALSE;
}

/*
 * Create SSL socket instance. 
 */
PJ_DEF(pj_status_t) pj_ssl_sock_create (pj_pool_t *pool,
                                        const pj_ssl_sock_param *param,
                                        pj_ssl_sock_t **p_ssock)
{
    pj_ssl_sock_t *ssock;
    pj_status_t status;
    pj_pool_t *info_pool;

    PJ_ASSERT_RETURN(pool && param && p_ssock, PJ_EINVAL);
    PJ_ASSERT_RETURN((param->sock_type & 0xF) == pj_SOCK_STREAM(), PJ_ENOTSUP);

    info_pool = pj_pool_create(pool->factory, "ssl_chain%p", 512, 512, NULL);
    pool = pj_pool_create(pool->factory, "ssl%p", 512, 512, NULL);

    /* Create secure socket */
    ssock = ssl_alloc(pool);
    if (!ssock)
        return PJ_ENOMEM;
    ssock->pool = pool;
    ssock->info_pool = info_pool;
    ssock->sock = PJ_INVALID_SOCKET;
    ssock->ssl_state = SSL_STATE_NULL;
    ssock->ssl_read_buf.owner = ssock;
    ssock->ssl_write_buf.owner = ssock;
    ssock->handshake_status = PJ_EUNKNOWN;
    pj_list_init(&ssock->write_pending);
    pj_list_init(&ssock->write_pending_empty);
    pj_list_init(&ssock->send_op_active);
    pj_list_init(&ssock->send_op_free);
    pj_timer_entry_init(&ssock->timer, 0, ssock, &on_timer);
    pj_ioqueue_op_key_init(&ssock->handshake_op_key,
                           sizeof(pj_ioqueue_op_key_t));
    pj_ioqueue_op_key_init(&ssock->shutdown_op_key,
                           sizeof(pj_ioqueue_op_key_t));

    /* Create secure socket mutex */
    status = pj_lock_create_recursive_mutex(pool, pool->obj_name,
                                            &ssock->ssl_write_buf_mutex);
    ssock->write_mutex = ssock->ssl_write_buf_mutex;
    if (status != PJ_SUCCESS)
        return status;

    /* Create input circular buffer mutex */
    status = pj_lock_create_recursive_mutex(pool, pool->obj_name,
                                            &ssock->ssl_read_buf_mutex);
    if (status != PJ_SUCCESS)
        return status;

    /* Create active socket send mutex */
    ssock->sending = PJ_FALSE;

    /* Init secure socket param */
    pj_ssl_sock_param_copy(pool, &ssock->param, param);

    if (ssock->param.grp_lock) {
        pj_grp_lock_add_ref(ssock->param.grp_lock);
        pj_grp_lock_add_handler(ssock->param.grp_lock, pool, ssock,
                                ssl_on_destroy);
    }

    ssock->param.read_buffer_size = ((ssock->param.read_buffer_size+7)>>3)<<3;
    if (!ssock->param.timer_heap) {
        PJ_LOG(3,(ssock->pool->obj_name, "Warning: timer heap is not "
                  "available. It is recommended to supply one to avoid "
                  "a race condition if more than one worker threads "
                  "are used."));
    }

    /* Finally */
    *p_ssock = ssock;

    return PJ_SUCCESS;
}


/*
 * Close the secure socket. This will unregister the socket from the
 * ioqueue and ultimately close the socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_close(pj_ssl_sock_t *ssock)
{
    PJ_ASSERT_RETURN(ssock, PJ_EINVAL);

    if (!ssock->pool || ssock->is_closing)
        return PJ_SUCCESS;

    ssock->is_closing = PJ_TRUE;

    if (ssock->timer.id != TIMER_NONE) {
        pj_timer_heap_cancel(ssock->param.timer_heap, &ssock->timer);
        ssock->timer.id = TIMER_NONE;
    }

    ssl_reset_sock_state(ssock);

    /* Fire error callbacks for pending sends synchronously, while
     * the app's context is still valid (before we return from close).
     * Must be after ssl_reset_sock_state which closes the socket,
     * ensuring no more async completions can race with us.
     */
    cancel_pending_sends(ssock);

    /* Wipe out cert & key buffer. */
    if (ssock->cert) {
        pj_ssl_cert_wipe_keys(ssock->cert);
        //ssock->cert = NULL;
    }

    if (ssock->param.grp_lock) {
        pj_grp_lock_dec_ref(ssock->param.grp_lock);
    } else {
        ssl_on_destroy(ssock);
    }

    return PJ_SUCCESS;
}


/*
 * Associate arbitrary data with the secure socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_set_user_data(pj_ssl_sock_t *ssock,
                                              void *user_data)
{
    PJ_ASSERT_RETURN(ssock, PJ_EINVAL);

    ssock->param.user_data = user_data;
    return PJ_SUCCESS;
}


/*
 * Retrieve the user data previously associated with this secure
 * socket.
 */
PJ_DEF(void*) pj_ssl_sock_get_user_data(pj_ssl_sock_t *ssock)
{
    PJ_ASSERT_RETURN(ssock, NULL);

    return ssock->param.user_data;
}

/*
 * Retrieve the local address and port used by specified SSL socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_get_info (pj_ssl_sock_t *ssock,
                                          pj_ssl_sock_info *info)
{
    pj_bzero(info, sizeof(*info));

    /* Established flag */
    info->established = (ssock->ssl_state == SSL_STATE_ESTABLISHED);

    /* Protocol */
    info->proto = ssock->param.proto;

    /* Local address */
    pj_sockaddr_cp(&info->local_addr, &ssock->local_addr);

    /* Certificates info */
    info->local_cert_info = &ssock->local_cert_info;
    info->remote_cert_info = &ssock->remote_cert_info;

    /* Remote address */
    if (pj_sockaddr_has_addr(&ssock->rem_addr))
        pj_sockaddr_cp(&info->remote_addr, &ssock->rem_addr);
    
    if (info->established) {
        info->cipher = ssl_get_cipher(ssock);
    }

    /* Verification status */
    info->verify_status = ssock->verify_status;

    /* Last known SSL error code */
    info->last_native_err = ssock->last_err;

    /* Group lock */
    info->grp_lock = ssock->param.grp_lock;

    /* Native SSL object */
#if defined(PJ_HAS_SSL_SOCK) && PJ_HAS_SSL_SOCK != 0 && \
    (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_OPENSSL)
    {
        ossl_sock_t *ossock = (ossl_sock_t *)ssock;
        info->native_ssl = ossock->ossl_ssl;
    }
#endif

    return PJ_SUCCESS;
}


/*
 * Starts read operation on this secure socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_read (pj_ssl_sock_t *ssock,
                                            pj_pool_t *pool,
                                            unsigned buff_size,
                                            pj_uint32_t flags)
{
    void **readbuf;
    unsigned i;

    PJ_ASSERT_RETURN(ssock && pool && buff_size, PJ_EINVAL);

    if (ssock->ssl_state != SSL_STATE_ESTABLISHED) 
        return PJ_EINVALIDOP;

    readbuf = (void**) pj_pool_calloc(pool, ssock->param.async_cnt, 
                                      sizeof(void*));
    if (!readbuf)
        return PJ_ENOMEM;

    for (i=0; i<ssock->param.async_cnt; ++i) {
        readbuf[i] = pj_pool_alloc(pool, buff_size);
        if (!readbuf[i])
            return PJ_ENOMEM;
    }

    return pj_ssl_sock_start_read2(ssock, pool, buff_size, 
                                   readbuf, flags);
}


/*
 * Same as #pj_ssl_sock_start_read(), except that the application
 * supplies the buffers for the read operation so that the acive socket
 * does not have to allocate the buffers.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_read2 (pj_ssl_sock_t *ssock,
                                             pj_pool_t *pool,
                                             unsigned buff_size,
                                             void *readbuf[],
                                             pj_uint32_t flags)
{
    unsigned i;

    PJ_ASSERT_RETURN(ssock && pool && buff_size && readbuf, PJ_EINVAL);

    if (ssock->ssl_state != SSL_STATE_ESTABLISHED) 
        return PJ_EINVALIDOP;

    /* Create SSL socket read buffer */
    ssock->ssock_rbuf = (read_data_t*)pj_pool_calloc(pool, 
                                               ssock->param.async_cnt,
                                               sizeof(read_data_t));
    if (!ssock->ssock_rbuf)
        return PJ_ENOMEM;

    /* Store SSL socket read buffer pointer in the activesock read buffer */
    for (i=0; i<ssock->param.async_cnt; ++i) {
        read_data_t **p_ssock_rbuf = 
                        OFFSET_OF_READ_DATA_PTR(ssock, ssock->asock_rbuf[i]);

        ssock->ssock_rbuf[i].data = readbuf[i];
        ssock->ssock_rbuf[i].len = 0;

        *p_ssock_rbuf = &ssock->ssock_rbuf[i];
    }

    ssock->read_size = buff_size;
    ssock->read_started = PJ_TRUE;
    ssock->read_flags = flags;

    for (i=0; i<ssock->param.async_cnt; ++i) {
        if (ssock->asock_rbuf[i]) {
            pj_size_t remainder = 0;
            ssock_on_data_read(ssock, ssock->asock_rbuf[i], 0,
                               PJ_SUCCESS, &remainder);
        }
    }

    return PJ_SUCCESS;
}


/*
 * Same as pj_ssl_sock_start_read(), except that this function is used
 * only for datagram sockets, and it will trigger \a on_data_recvfrom()
 * callback instead.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_recvfrom (pj_ssl_sock_t *ssock,
                                                pj_pool_t *pool,
                                                unsigned buff_size,
                                                pj_uint32_t flags)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(buff_size);
    PJ_UNUSED_ARG(flags);

    return PJ_ENOTSUP;
}


/*
 * Same as #pj_ssl_sock_start_recvfrom() except that the recvfrom() 
 * operation takes the buffer from the argument rather than creating
 * new ones.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_recvfrom2 (pj_ssl_sock_t *ssock,
                                                 pj_pool_t *pool,
                                                 unsigned buff_size,
                                                 void *readbuf[],
                                                 pj_uint32_t flags)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(buff_size);
    PJ_UNUSED_ARG(readbuf);
    PJ_UNUSED_ARG(flags);

    return PJ_ENOTSUP;
}


/* Write plain data to SSL and flush the buffer. */
static pj_status_t ssl_send (pj_ssl_sock_t *ssock,
                             pj_ioqueue_op_key_t *send_key,
                             const void *data,
                             pj_ssize_t size,
                             unsigned flags)
{
    pj_status_t status;
    int nwritten = 0;
    pj_bool_t should_drain = PJ_FALSE;

    /* Encrypt and enqueue atomically under write_mutex.
     * This eliminates the race between ssl_write and flush that could
     * merge encrypted data from different threads into one send op.
     */
    pj_lock_acquire(ssock->write_mutex);
    status = ssl_write(ssock, data, size, &nwritten);

    if (status == PJ_SUCCESS && nwritten == size) {
        /* All data written — enqueue for sending (still under lock) */
        status = enqueue_ssl_write_buf(ssock, send_key, size, flags,
                                       &should_drain);
        pj_lock_release(ssock->write_mutex);

        if (should_drain)
            status = drain_send_queue(ssock, PJ_TRUE);

    } else if (status == PJ_ETRYAGAIN) {
        /* Re-negotiation in progress — flush handshake data */
        status = enqueue_ssl_write_buf(ssock, &ssock->handshake_op_key,
                                       0, 0, &should_drain);
        pj_lock_release(ssock->write_mutex);

        if (should_drain) {
            pj_status_t ds = drain_send_queue(ssock, PJ_TRUE);
            if (ds == PJ_SUCCESS || ds == PJ_EPENDING)
                status = PJ_EBUSY;
            else
                status = ds;
        } else {
            status = PJ_EBUSY;
        }
    } else {
        pj_lock_release(ssock->write_mutex);
    }

    return status;
}

/* Flush delayed data sending in the write pending list. */
static pj_status_t flush_delayed_send(pj_ssl_sock_t *ssock)
{
    /* Check for another ongoing flush */
    if (ssock->flushing_write_pend)
        return PJ_EBUSY;

    pj_lock_acquire(ssock->write_mutex);

    /* Again, check for another ongoing flush */
    if (ssock->flushing_write_pend) {
        pj_lock_release(ssock->write_mutex);
        return PJ_EBUSY;
    }

    /* Set ongoing flush flag */
    ssock->flushing_write_pend = PJ_TRUE;

    while (!pj_list_empty(&ssock->write_pending)) {
        write_data_t *wp;
        pj_ioqueue_op_key_t *app_key;
        pj_ssize_t plain_data_len;
        pj_status_t status;

        wp = ssock->write_pending.next;
        app_key = wp->app_key;
        plain_data_len = wp->plain_data_len;

        /* Ticket #1573: Don't hold mutex while calling socket send. */
        pj_lock_release(ssock->write_mutex);

        /* Pass the original app_key (not &wp->key) so the correct
         * key propagates through to the final on_data_sent callback.
         */
        status = ssl_send(ssock, app_key, wp->data.ptr,
                          plain_data_len, wp->flags);

        if (status == PJ_EPENDING) {
            /* Data encrypted and queued for async sending.
             * Remove wp to prevent double-processing on next flush.
             */
            pj_lock_acquire(ssock->write_mutex);
            pj_list_erase(wp);
            pj_list_push_back(&ssock->write_pending_empty, wp);
            pj_lock_release(ssock->write_mutex);

            ssock->flushing_write_pend = PJ_FALSE;
            return PJ_EPENDING;
        }

        if (status != PJ_SUCCESS) {
            /* Reset ongoing flush flag first. */
            ssock->flushing_write_pend = PJ_FALSE;
            return status;
        }

        /* PJ_SUCCESS: sent synchronously. Remove wp. */
        pj_lock_acquire(ssock->write_mutex);
        pj_list_erase(wp);
        pj_list_push_back(&ssock->write_pending_empty, wp);
        pj_lock_release(ssock->write_mutex);

        /* Invoke callback — app originally got PJ_EPENDING from
         * pj_ssl_sock_send, so it expects a completion callback.
         */
        if (ssock->param.cb.on_data_sent) {
            pj_bool_t ret;
            ret = (*ssock->param.cb.on_data_sent)(ssock, app_key,
                                                   plain_data_len);
            if (!ret) {
                /* We've been destroyed. Do NOT touch ssock. */
                return PJ_SUCCESS;
            }
        }

        pj_lock_acquire(ssock->write_mutex);
    }

    /* Reset ongoing flush flag */
    ssock->flushing_write_pend = PJ_FALSE;

    pj_lock_release(ssock->write_mutex);

    return PJ_SUCCESS;
}

/* Sending is delayed, push back the sending data into pending list. */
static pj_status_t delay_send (pj_ssl_sock_t *ssock,
                               pj_ioqueue_op_key_t *send_key,
                               const void *data,
                               pj_ssize_t size,
                               unsigned flags)
{
    write_data_t *wp;

    pj_lock_acquire(ssock->write_mutex);

    /* Init write pending instance */
    if (!pj_list_empty(&ssock->write_pending_empty)) {
        wp = ssock->write_pending_empty.next;
        pj_list_erase(wp);
    } else {
        wp = PJ_POOL_ZALLOC_T(ssock->pool, write_data_t);
    }

    wp->app_key = send_key;
    wp->plain_data_len = size;
    wp->data.ptr = data;
    wp->flags = flags;

    pj_list_push_back(&ssock->write_pending, wp);
    
    pj_lock_release(ssock->write_mutex);

    /* Must return PJ_EPENDING */
    return PJ_EPENDING;
}


/**
 * Send data using the socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_send (pj_ssl_sock_t *ssock,
                                      pj_ioqueue_op_key_t *send_key,
                                      const void *data,
                                      pj_ssize_t *size,
                                      unsigned flags)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(ssock && data && size && (*size>0), PJ_EINVAL);

    if (ssock->ssl_state != SSL_STATE_ESTABLISHED) 
        return PJ_EINVALIDOP;

    // Ticket #1573: Don't hold mutex while calling PJLIB socket send().
    //pj_lock_acquire(ssock->write_mutex);

    /* Flush delayed send first. Sending data might be delayed when 
     * re-negotiation is on-progress.
     */
    status = flush_delayed_send(ssock);
    if (status == PJ_EBUSY || status == PJ_EPENDING) {
        /* Re-negotiation or flushing is on progress, delay sending */
        status = delay_send(ssock, send_key, data, *size, flags);
        goto on_return;
    } else if (status != PJ_SUCCESS) {
        goto on_return;
    }

    /* Prevent unbounded queue growth when the network stalls.
     * Check BEFORE ssl_send which encrypts the plaintext — once
     * encrypted, the data cannot be "un-sent" without corruption.
     */
#if PJ_SSL_SEND_OP_ACTIVE_MAX > 0
    if (ssock->send_op_active_cnt >= PJ_SSL_SEND_OP_ACTIVE_MAX) {
        status = PJ_EBUSY;
        goto on_return;
    }
#endif

    /* Write data to SSL */
    status = ssl_send(ssock, send_key, data, *size, flags);
    if (status == PJ_EBUSY) {
        /* Re-negotiation is on progress, delay sending */
        status = delay_send(ssock, send_key, data, *size, flags);
    }

on_return:
    //pj_lock_release(ssock->write_mutex);
    return status;
}


/**
 * Send datagram using the socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_sendto (pj_ssl_sock_t *ssock,
                                        pj_ioqueue_op_key_t *send_key,
                                        const void *data,
                                        pj_ssize_t *size,
                                        unsigned flags,
                                        const pj_sockaddr_t *addr,
                                        int addr_len)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(send_key);
    PJ_UNUSED_ARG(data);
    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(addr);
    PJ_UNUSED_ARG(addr_len);

    return PJ_ENOTSUP;
}


/**
 * Starts asynchronous socket accept() operations on this secure socket. 
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_accept (pj_ssl_sock_t *ssock,
                                              pj_pool_t *pool,
                                              const pj_sockaddr_t *localaddr,
                                              int addr_len)
{
    return pj_ssl_sock_start_accept2(ssock, pool, localaddr, addr_len,
                                     &ssock->param);
}


/**
 * Same as #pj_ssl_sock_start_accept(), but application provides parameter
 * for new accepted secure sockets.
 */
PJ_DEF(pj_status_t)
pj_ssl_sock_start_accept2(pj_ssl_sock_t *ssock,
                          pj_pool_t *pool,
                          const pj_sockaddr_t *localaddr,
                          int addr_len,
                          const pj_ssl_sock_param *newsock_param)
{
    pj_status_t status;
#ifndef SSL_SOCK_IMP_USE_OWN_NETWORK
    pj_activesock_cb asock_cb;
    pj_activesock_cfg asock_cfg;
#endif

    PJ_ASSERT_RETURN(ssock && pool && localaddr && addr_len, PJ_EINVAL);

    /* Verify new socket parameters */
    if (newsock_param->grp_lock != ssock->param.grp_lock ||
        newsock_param->sock_af != ssock->param.sock_af ||
        newsock_param->sock_type != ssock->param.sock_type)
    {
        return PJ_EINVAL;
    }

    ssock->is_server = PJ_TRUE;

#ifdef SSL_SOCK_IMP_USE_OWN_NETWORK
    status = network_start_accept(ssock, pool, localaddr, addr_len,
                                  newsock_param);
    if (status != PJ_SUCCESS)
        goto on_error;
#else
    /* Create socket */
    if (ssock->param.sock_cloexec)
        ssock->param.sock_type |= pj_SOCK_CLOEXEC();
    status = pj_sock_socket(ssock->param.sock_af, ssock->param.sock_type, 0, 
                            &ssock->sock);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Apply SO_REUSEADDR */
    if (ssock->param.reuse_addr) {
        int enabled = 1;
        status = pj_sock_setsockopt(ssock->sock, pj_SOL_SOCKET(),
                                    pj_SO_REUSEADDR(),
                                    &enabled, sizeof(enabled));
        if (status != PJ_SUCCESS) {
            PJ_PERROR(4,(ssock->pool->obj_name, status,
                         "Warning: error applying SO_REUSEADDR"));
        }
    }

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(ssock->sock, ssock->param.qos_type,
                                &ssock->param.qos_params, 2, 
                                ssock->pool->obj_name, NULL);
    if (status != PJ_SUCCESS && !ssock->param.qos_ignore_error)
        goto on_error;

    /* Apply socket options, if specified */
    if (ssock->param.sockopt_params.cnt) {
        status = pj_sock_setsockopt_params(ssock->sock, 
                                           &ssock->param.sockopt_params);

        if (status != PJ_SUCCESS && !ssock->param.sockopt_ignore_error)
            goto on_error;
    }

    /* Bind socket */
    status = pj_sock_bind(ssock->sock, localaddr, addr_len);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Start listening to the address */
    status = pj_sock_listen(ssock->sock, PJ_SOMAXCONN);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Create active socket */
    pj_activesock_cfg_default(&asock_cfg);
    asock_cfg.async_cnt = ssock->param.async_cnt;
    asock_cfg.concurrency = ssock->param.concurrency;
    asock_cfg.whole_data = PJ_FALSE;
    asock_cfg.grp_lock = ssock->param.grp_lock;

    pj_bzero(&asock_cb, sizeof(asock_cb));
    //asock_cb.on_accept_complete = asock_on_accept_complete;
    asock_cb.on_accept_complete2 = asock_on_accept_complete2;

    status = pj_activesock_create(pool,
                                  ssock->sock, 
                                  ssock->param.sock_type,
                                  &asock_cfg,
                                  ssock->param.ioqueue, 
                                  &asock_cb,
                                  ssock,
                                  &ssock->asock);

    if (status != PJ_SUCCESS)
        goto on_error;

    /* Start accepting */
    pj_ssl_sock_param_copy(pool, &ssock->newsock_param, newsock_param);
    ssock->newsock_param.grp_lock = NULL;
    status = pj_activesock_start_accept(ssock->asock, pool);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Update local address */
    ssock->addr_len = addr_len;
    status = pj_sock_getsockname(ssock->sock, &ssock->local_addr, 
                                 &ssock->addr_len);
    if (status != PJ_SUCCESS)
        pj_sockaddr_cp(&ssock->local_addr, localaddr);
#endif

    return PJ_SUCCESS;

on_error:
    ssl_reset_sock_state(ssock);
    return status;
}


/**
 * Starts asynchronous socket connect() operation.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_connect(pj_ssl_sock_t *ssock,
                                              pj_pool_t *pool,
                                              const pj_sockaddr_t *localaddr,
                                              const pj_sockaddr_t *remaddr,
                                              int addr_len)
{
    pj_ssl_start_connect_param param;    
    param.pool = pool;
    param.localaddr = localaddr;
    param.local_port_range = 0;
    param.remaddr = remaddr;
    param.addr_len = addr_len;

    return pj_ssl_sock_start_connect2(ssock, &param);
}

PJ_DEF(pj_status_t) pj_ssl_sock_start_connect2(
                               pj_ssl_sock_t *ssock,
                               pj_ssl_start_connect_param *connect_param)
{
    pj_status_t status;
#ifdef SSL_SOCK_IMP_USE_OWN_NETWORK
    status = network_start_connect(ssock, connect_param);
    if (status != PJ_EPENDING)
        goto on_error;
#else
    pj_activesock_cb asock_cb;
    pj_activesock_cfg asock_cfg;
    
    pj_pool_t *pool = connect_param->pool;
    const pj_sockaddr_t *localaddr = connect_param->localaddr;
    pj_uint16_t port_range = connect_param->local_port_range;
    const pj_sockaddr_t *remaddr = connect_param->remaddr;
    int addr_len = connect_param->addr_len;

    PJ_ASSERT_RETURN(ssock && pool && localaddr && remaddr && addr_len,
                     PJ_EINVAL);

    /* Create socket */
    if (ssock->param.sock_cloexec)
        ssock->param.sock_type |= pj_SOCK_CLOEXEC();
    status = pj_sock_socket(ssock->param.sock_af, ssock->param.sock_type, 0, 
                            &ssock->sock);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(ssock->sock, ssock->param.qos_type,
                                &ssock->param.qos_params, 2, 
                                ssock->pool->obj_name, NULL);
    if (status != PJ_SUCCESS && !ssock->param.qos_ignore_error)
        goto on_error;

    /* Apply socket options, if specified */
    if (ssock->param.sockopt_params.cnt) {
        status = pj_sock_setsockopt_params(ssock->sock, 
                                           &ssock->param.sockopt_params);

        if (status != PJ_SUCCESS && !ssock->param.sockopt_ignore_error)
            goto on_error;
    }

    /* Bind socket */
    if (port_range) {
        pj_uint16_t max_bind_retry = MAX_BIND_RETRY;
        if (port_range && port_range < max_bind_retry)
        {
            max_bind_retry = port_range;
        }
        status = pj_sock_bind_random(ssock->sock, localaddr, port_range,
                                     max_bind_retry);
    } else {
        status = pj_sock_bind(ssock->sock, localaddr, addr_len);
    }

    if (status != PJ_SUCCESS)
        goto on_error;

    /* Create active socket */
    pj_activesock_cfg_default(&asock_cfg);
    asock_cfg.async_cnt = ssock->param.async_cnt;
    asock_cfg.concurrency = ssock->param.concurrency;
    asock_cfg.whole_data = PJ_TRUE;
    asock_cfg.grp_lock = ssock->param.grp_lock;

    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_connect_complete = asock_on_connect_complete;
    asock_cb.on_data_read = asock_on_data_read;
    asock_cb.on_data_sent = asock_on_data_sent;

    status = pj_activesock_create(pool,
                                  ssock->sock, 
                                  ssock->param.sock_type,
                                  &asock_cfg,
                                  ssock->param.ioqueue, 
                                  &asock_cb,
                                  ssock,
                                  &ssock->asock);

    if (status != PJ_SUCCESS)
        goto on_error;

    /* Save remote address */
    pj_sockaddr_cp(&ssock->rem_addr, remaddr);

    status = pj_activesock_start_connect(ssock->asock, pool, remaddr,
                                         addr_len);

    if (status == PJ_SUCCESS) {
        /* Synchronous connect completion. The callback handles
         * local address update, SSL create, handshake, and user
         * notification. Don't access ssock after this — the user
         * callback may have destroyed it.
         */
        asock_on_connect_complete(ssock->asock, PJ_SUCCESS);
        return PJ_EPENDING;
    } else if (status != PJ_EPENDING) {
        goto on_error;
    }

    /* Update local address */
    ssock->addr_len = addr_len;
    status = pj_sock_getsockname(ssock->sock, &ssock->local_addr,
                                 &ssock->addr_len);
    /* Note that we may not get an IP address here. This can
     * happen for example on Windows, where getsockname()
     * would return 0.0.0.0 if socket has just started the
     * async connect. In this case, just leave the local
     * address with 0.0.0.0 for now; it will be updated
     * once the socket is established.
     */

#endif

    /* Start timer */
    if (ssock->param.timer_heap &&
        (ssock->param.timeout.sec != 0 || ssock->param.timeout.msec != 0))
    {
        pj_assert(ssock->timer.id == TIMER_NONE);
        status = pj_timer_heap_schedule_w_grp_lock(ssock->param.timer_heap,
                                                   &ssock->timer,
                                                   &ssock->param.timeout,
                                                   TIMER_HANDSHAKE_TIMEOUT,
                                                   ssock->param.grp_lock);
        if (status != PJ_SUCCESS) {
            ssock->timer.id = TIMER_NONE;
            status = PJ_SUCCESS;
        }
    }

    /* Update SSL state */
    ssock->is_server = PJ_FALSE;

    return PJ_EPENDING;

on_error:
    ssl_reset_sock_state(ssock);
    return status;
}


PJ_DEF(pj_status_t) pj_ssl_sock_renegotiate(pj_ssl_sock_t *ssock)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(ssock, PJ_EINVAL);

    if (ssock->ssl_state != SSL_STATE_ESTABLISHED) 
        return PJ_EINVALIDOP;

    status = ssl_renegotiate(ssock);
    if (status == PJ_SUCCESS) {
        status = ssl_do_handshake_and_flush(ssock);
    }

    return status;
}

static void wipe_buf(pj_str_t *buf)
{
    volatile char *p = buf->ptr;
    pj_ssize_t len = buf->slen;
    while (len--) *p++ = 0;
    buf->slen = 0;
}

PJ_DEF(void) pj_ssl_cert_wipe_keys(pj_ssl_cert_t *cert)
{
    if (cert) {
#if (PJ_SSL_SOCK_IMP != PJ_SSL_SOCK_IMP_SCHANNEL)
        wipe_buf(&cert->CA_file);
        wipe_buf(&cert->CA_path);
        wipe_buf(&cert->cert_file);
        wipe_buf(&cert->privkey_file);
        wipe_buf(&cert->privkey_pass);
        wipe_buf(&cert->CA_buf);
        wipe_buf(&cert->cert_buf);
        wipe_buf(&cert->privkey_buf);
#else
        cert->criteria.type = PJ_SSL_CERT_LOOKUP_NONE;
        wipe_buf(&cert->criteria.keyword);
#endif

#if (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_OPENSSL)
        ssl_free_cert(cert);
#endif
    }
}

/* Load credentials from files. */
PJ_DEF(pj_status_t) pj_ssl_cert_load_from_files (pj_pool_t *pool,
                                                 const pj_str_t *CA_file,
                                                 const pj_str_t *cert_file,
                                                 const pj_str_t *privkey_file,
                                                 const pj_str_t *privkey_pass,
                                                 pj_ssl_cert_t **p_cert)
{
    return pj_ssl_cert_load_from_files2(pool, CA_file, NULL, cert_file,
                                        privkey_file, privkey_pass, p_cert);
}

PJ_DEF(pj_status_t) pj_ssl_cert_load_from_files2(pj_pool_t *pool,
                                                 const pj_str_t *CA_file,
                                                 const pj_str_t *CA_path,
                                                 const pj_str_t *cert_file,
                                                 const pj_str_t *privkey_file,
                                                 const pj_str_t *privkey_pass,
                                                 pj_ssl_cert_t **p_cert)
{
#if (PJ_SSL_SOCK_IMP != PJ_SSL_SOCK_IMP_SCHANNEL)
    pj_ssl_cert_t *cert;

    PJ_ASSERT_RETURN(pool && p_cert &&
                     (CA_file || CA_path || cert_file || privkey_file),
                     PJ_EINVAL);

    cert = *p_cert;
    if (!cert)
        cert = PJ_POOL_ZALLOC_T(pool, pj_ssl_cert_t);
    if (!cert)
        return PJ_ENOMEM;

    if (CA_file) {
        pj_strdup_with_null(pool, &cert->CA_file, CA_file);
    }
    if (CA_path) {
        pj_strdup_with_null(pool, &cert->CA_path, CA_path);
    }
    if (cert_file) {
        pj_strdup_with_null(pool, &cert->cert_file, cert_file);
    }
    if (privkey_file) {
        pj_strdup_with_null(pool, &cert->privkey_file, privkey_file);
    }
    if (privkey_pass) {
        pj_strdup_with_null(pool, &cert->privkey_pass, privkey_pass);
    }

    *p_cert = cert;

    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(CA_file);
    PJ_UNUSED_ARG(CA_path);
    PJ_UNUSED_ARG(cert_file);
    PJ_UNUSED_ARG(privkey_file);
    PJ_UNUSED_ARG(privkey_pass);
    PJ_UNUSED_ARG(p_cert);
    return PJ_ENOTSUP;
#endif
}

PJ_DEF(pj_status_t) pj_ssl_cert_load_from_buffer(pj_pool_t *pool,
                                        const pj_ssl_cert_buffer *CA_buf,
                                        const pj_ssl_cert_buffer *cert_buf,
                                        const pj_ssl_cert_buffer *privkey_buf,
                                        const pj_str_t *privkey_pass,
                                        pj_ssl_cert_t **p_cert)
{
#if (PJ_SSL_SOCK_IMP != PJ_SSL_SOCK_IMP_SCHANNEL)
    pj_ssl_cert_t *cert;

    PJ_ASSERT_RETURN(pool && p_cert &&
                     (CA_buf || cert_buf || privkey_buf),
                     PJ_EINVAL);

    cert = *p_cert;
    if (!cert)
        cert = PJ_POOL_ZALLOC_T(pool, pj_ssl_cert_t);
    if (!cert)
        return PJ_ENOMEM;

    if (CA_buf) {
        pj_strdup(pool, &cert->CA_buf, CA_buf);
    }
    if (cert_buf) {
        pj_strdup(pool, &cert->cert_buf, cert_buf);
    }
    if (privkey_buf) {
        pj_strdup(pool, &cert->privkey_buf, privkey_buf);
    }
    if (privkey_pass) {
        pj_strdup_with_null(pool, &cert->privkey_pass, privkey_pass);
    }

    *p_cert = cert;

    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(CA_buf);
    PJ_UNUSED_ARG(cert_buf);
    PJ_UNUSED_ARG(privkey_buf);
    PJ_UNUSED_ARG(privkey_pass);
    PJ_UNUSED_ARG(p_cert);
    return PJ_ENOTSUP;
#endif
}


PJ_DEF(pj_status_t) pj_ssl_cert_load_from_store(
                                pj_pool_t *pool,
                                const pj_ssl_cert_lookup_criteria *criteria,
                                pj_ssl_cert_t **p_cert)
{
#if (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_SCHANNEL)
    pj_ssl_cert_t *cert;

    PJ_ASSERT_RETURN(pool && p_cert && criteria, PJ_EINVAL);

    cert = *p_cert;
    if (!cert)
        cert = PJ_POOL_ZALLOC_T(pool, pj_ssl_cert_t);
    if (!cert)
        return PJ_ENOMEM;

    pj_memcpy(&cert->criteria, criteria, sizeof(*criteria));
    pj_strdup_with_null(pool, &cert->criteria.keyword, &criteria->keyword);

    *p_cert = cert;

    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(criteria);
    PJ_UNUSED_ARG(p_cert);
    return PJ_ENOTSUP;
#endif
}


PJ_DEF(pj_status_t) pj_ssl_cert_load_direct(
                                pj_pool_t *pool,
                                pj_ssl_cert_direct *cert_direct,
                                pj_ssl_cert_t **p_cert)
{
#if (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_OPENSSL)
    pj_ssl_cert_t *cert;

    PJ_ASSERT_RETURN(pool && p_cert && cert_direct, PJ_EINVAL);

    cert = *p_cert;
    if (!cert)
        cert = PJ_POOL_ZALLOC_T(pool, pj_ssl_cert_t);
    if (!cert)
        return PJ_ENOMEM;

    cert->direct = *cert_direct;

    *p_cert = cert;

    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(cert_direct);
    PJ_UNUSED_ARG(p_cert);
    return PJ_ENOTSUP;
#endif
}


/* Set SSL socket credentials. */
PJ_DEF(pj_status_t) pj_ssl_sock_set_certificate(
                                            pj_ssl_sock_t *ssock,
                                            pj_pool_t *pool,
                                            const pj_ssl_cert_t *cert)
{
    pj_ssl_cert_t *cert_;

    PJ_ASSERT_RETURN(ssock && pool && cert, PJ_EINVAL);

    cert_ = PJ_POOL_ZALLOC_T(pool, pj_ssl_cert_t);
    pj_memcpy(cert_, cert, sizeof(pj_ssl_cert_t));

#if (PJ_SSL_SOCK_IMP != PJ_SSL_SOCK_IMP_SCHANNEL)
    pj_strdup_with_null(pool, &cert_->CA_file, &cert->CA_file);
    pj_strdup_with_null(pool, &cert_->CA_path, &cert->CA_path);
    pj_strdup_with_null(pool, &cert_->cert_file, &cert->cert_file);
    pj_strdup_with_null(pool, &cert_->privkey_file, &cert->privkey_file);
    pj_strdup_with_null(pool, &cert_->privkey_pass, &cert->privkey_pass);

    pj_strdup(pool, &cert_->CA_buf, &cert->CA_buf);
    pj_strdup(pool, &cert_->cert_buf, &cert->cert_buf);
    pj_strdup(pool, &cert_->privkey_buf, &cert->privkey_buf);

    /* For OpenSSL version >= 3.0, add ref EVP_PKEY & X509 */
#   if (PJ_SSL_SOCK_IMP == PJ_SSL_SOCK_IMP_OPENSSL) && \
       (OPENSSL_VERSION_NUMBER >= 0x30000000L)
    if ((cert_->direct.type & PJ_SSL_CERT_DIRECT_OPENSSL_EVP_PKEY) &&
        cert_->direct.privkey)
    {
        EVP_PKEY_up_ref(cert_->direct.privkey);
    }

    if ((cert_->direct.type & PJ_SSL_CERT_DIRECT_OPENSSL_X509_CERT) &&
        cert_->direct.cert)
    {
        X509_up_ref(cert_->direct.cert);
    }
#   endif

#else
    pj_strdup_with_null(pool, &cert_->criteria.keyword,
                        &cert->criteria.keyword);
#endif

    ssock->cert = cert_;

    return PJ_SUCCESS;
}

