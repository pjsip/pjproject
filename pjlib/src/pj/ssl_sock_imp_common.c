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
#include <pj/pool.h>
#include <pj/string.h>

#include "ssl_sock_imp_common.h"

/* Workaround for ticket #985 and #1930 */
#ifndef PJ_SSL_SOCK_DELAYED_CLOSE_TIMEOUT
#   define PJ_SSL_SOCK_DELAYED_CLOSE_TIMEOUT	500
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

static pj_status_t circ_init(pj_pool_factory *factory,
                             circ_buf_t *cb, pj_size_t cap)
{
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
        pj_pool_release(cb->pool);
        return PJ_ENOMEM;
    }

    return PJ_SUCCESS;
}

static void circ_deinit(circ_buf_t *cb)
{
    if (cb->pool) {
        pj_pool_release(cb->pool);
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

    pj_memcpy(dst, cb->buf + cb->readp, tbc);
    pj_memcpy(dst + tbc, cb->buf, rem);

    cb->readp += len;
    cb->readp &= (cb->cap - 1);

    cb->size -= len;
}

static pj_status_t circ_write(circ_buf_t *cb,
                              const pj_uint8_t *src, pj_size_t len)
{
    /* Overflow condition: resize */
    if (len > circ_avail(cb)) {
        /* Minimum required capacity */
        pj_size_t min_cap = len + cb->size;

        /* Next 32-bit power of two */
        min_cap--;
        min_cap |= min_cap >> 1;
        min_cap |= min_cap >> 2;
        min_cap |= min_cap >> 4;
        min_cap |= min_cap >> 8;
        min_cap |= min_cap >> 16;
        min_cap++;

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
        pj_pool_release(cb->pool);

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

	    char buf[PJ_INET6_ADDRSTRLEN+10];

            if (pj_sockaddr_has_addr(&ssock->rem_addr)) {
                PJ_PERROR(3,(ssock->pool->obj_name, status,
			  "Handshake failed in accepting %s",
			  pj_sockaddr_print(&ssock->rem_addr, buf,
					    sizeof(buf), 3)));
            }

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

static write_data_t* alloc_send_data(pj_ssl_sock_t *ssock, pj_size_t len)
{
    send_buf_t *send_buf = &ssock->send_buf;
    pj_size_t avail_len, skipped_len = 0;
    char *reg1, *reg2;
    pj_size_t reg1_len, reg2_len;
    write_data_t *p;

    /* Check buffer availability */
    avail_len = send_buf->max_len - send_buf->len;
    if (avail_len < len)
	return NULL;

    /* If buffer empty, reset start pointer and return it */
    if (send_buf->len == 0) {
	send_buf->start = send_buf->buf;
	send_buf->len   = len;
	p = (write_data_t*)send_buf->start;
	goto init_send_data;
    }

    /* Free space may be wrapped/splitted into two regions, so let's
     * analyze them if any region can hold the write data.
     */
    reg1 = send_buf->start + send_buf->len;
    if (reg1 >= send_buf->buf + send_buf->max_len)
	reg1 -= send_buf->max_len;
    reg1_len = send_buf->max_len - send_buf->len;
    if (reg1 + reg1_len > send_buf->buf + send_buf->max_len) {
	reg1_len = send_buf->buf + send_buf->max_len - reg1;
	reg2 = send_buf->buf;
	reg2_len = send_buf->start - send_buf->buf;
    } else {
	reg2 = NULL;
	reg2_len = 0;
    }

    /* More buffer availability check, note that the write data must be in
     * a contigue buffer.
     */
    avail_len = PJ_MAX(reg1_len, reg2_len);
    if (avail_len < len)
	return NULL;

    /* Get the data slot */
    if (reg1_len >= len) {
	p = (write_data_t*)reg1;
    } else {
	p = (write_data_t*)reg2;
	skipped_len = reg1_len;
    }

    /* Update buffer length */
    send_buf->len += len + skipped_len;

init_send_data:
    /* Init the new send data */
    pj_bzero(p, sizeof(*p));
    pj_list_init(p);
    pj_list_push_back(&ssock->send_pending, p);

    return p;
}

static void free_send_data(pj_ssl_sock_t *ssock, write_data_t *wdata)
{
    send_buf_t *buf = &ssock->send_buf;
    write_data_t *spl = &ssock->send_pending;

    pj_assert(!pj_list_empty(&ssock->send_pending));
    
    /* Free slot from the buffer */
    if (spl->next == wdata && spl->prev == wdata) {
	/* This is the only data, reset the buffer */
	buf->start = buf->buf;
	buf->len = 0;
    } else if (spl->next == wdata) {
	/* This is the first data, shift start pointer of the buffer and
	 * adjust the buffer length.
	 */
	buf->start = (char*)wdata->next;
	if (wdata->next > wdata) {
	    buf->len -= ((char*)wdata->next - buf->start);
	} else {
	    /* Overlapped */
	    pj_size_t right_len, left_len;
	    right_len = buf->buf + buf->max_len - (char*)wdata;
	    left_len  = (char*)wdata->next - buf->buf;
	    buf->len -= (right_len + left_len);
	}
    } else if (spl->prev == wdata) {
	/* This is the last data, just adjust the buffer length */
	if (wdata->prev < wdata) {
	    pj_size_t jump_len;
	    jump_len = (char*)wdata -
		       ((char*)wdata->prev + wdata->prev->record_len);
	    buf->len -= (wdata->record_len + jump_len);
	} else {
	    /* Overlapped */
	    pj_size_t right_len, left_len;
	    right_len = buf->buf + buf->max_len -
			((char*)wdata->prev + wdata->prev->record_len);
	    left_len  = (char*)wdata + wdata->record_len - buf->buf;
	    buf->len -= (right_len + left_len);
	}
    }
    /* For data in the middle buffer, just do nothing on the buffer. The slot
     * will be freed later when freeing the first/last data.
     */
    
    /* Remove the data from send pending list */
    pj_list_erase(wdata);
}

/* Flush write circular buffer to network socket. */
static pj_status_t flush_circ_buf_output(pj_ssl_sock_t *ssock,
                                         pj_ioqueue_op_key_t *send_key,
                                         pj_size_t orig_len, unsigned flags)
{
    pj_ssize_t len;
    write_data_t *wdata;
    pj_size_t needed_len;
    pj_status_t status;

    pj_lock_acquire(ssock->write_mutex);

    /* Check if there is data in the circular buffer, flush it if any */
    if (io_empty(ssock, &ssock->circ_buf_output)) {
	pj_lock_release(ssock->write_mutex);
	return PJ_SUCCESS;
    }

    /* Get data and its length */
    len = io_size(ssock, &ssock->circ_buf_output);
    if (len == 0) {
	pj_lock_release(ssock->write_mutex);
	return PJ_SUCCESS;
    }

    /* Calculate buffer size needed, and align it to 8 */
    needed_len = len + sizeof(write_data_t);
    needed_len = ((needed_len + 7) >> 3) << 3;

    /* Allocate buffer for send data */
    wdata = alloc_send_data(ssock, needed_len);
    if (wdata == NULL) {
	/* Oops, the send buffer is full, let's just
	 * queue it for sending and return PJ_EPENDING.
	 */
	ssock->send_buf_pending.data_len = needed_len;
	ssock->send_buf_pending.app_key = send_key;
	ssock->send_buf_pending.flags = flags;
	ssock->send_buf_pending.plain_data_len = orig_len;
	pj_lock_release(ssock->write_mutex);
	return PJ_EPENDING;
    }

    /* Copy the data and set its properties into the send data */
    pj_ioqueue_op_key_init(&wdata->key, sizeof(pj_ioqueue_op_key_t));
    wdata->key.user_data = wdata;
    wdata->app_key = send_key;
    wdata->record_len = needed_len;
    wdata->data_len = len;
    wdata->plain_data_len = orig_len;
    wdata->flags = flags;
    io_read(ssock, &ssock->circ_buf_output, (pj_uint8_t *)&wdata->data, len);

    /* Ticket #1573: Don't hold mutex while calling PJLIB socket send(). */
    pj_lock_release(ssock->write_mutex);

    /* Send it */
#ifdef SSL_SOCK_IMP_USE_OWN_NETWORK
    status = network_send(ssock, &wdata->key, wdata->data.content, &len,
			  flags);
#else
    if (ssock->param.sock_type == pj_SOCK_STREAM()) {
	status = pj_activesock_send(ssock->asock, &wdata->key, 
				    wdata->data.content, &len,
				    flags);
    } else {
	status = pj_activesock_sendto(ssock->asock, &wdata->key, 
				      wdata->data.content, &len,
				      flags,
				      (pj_sockaddr_t*)&ssock->rem_addr,
				      ssock->addr_len);
    }
#endif

    if (status != PJ_EPENDING) {
	/* When the sending is not pending, remove the wdata from send
	 * pending list.
	 */
	pj_lock_acquire(ssock->write_mutex);
	free_send_data(ssock, wdata);
	pj_lock_release(ssock->write_mutex);
    }

    return status;
}

#if 0
/* Just for testing send buffer alloc/free */
#include <pj/rand.h>
pj_status_t pj_ssl_sock_ossl_test_send_buf(pj_pool_t *pool)
{
    enum { MAX_CHUNK_NUM = 20 };
    unsigned chunk_size, chunk_cnt, i;
    write_data_t *wdata[MAX_CHUNK_NUM] = {0};
    pj_time_val now;
    pj_ssl_sock_t *ssock = NULL;
    pj_ssl_sock_param param;
    pj_status_t status;

    pj_gettimeofday(&now);
    pj_srand((unsigned)now.sec);

    pj_ssl_sock_param_default(&param);
    status = pj_ssl_sock_create(pool, &param, &ssock);
    if (status != PJ_SUCCESS) {
	return status;
    }

    if (ssock->send_buf.max_len == 0) {
	ssock->send_buf.buf = (char*)
			      pj_pool_alloc(ssock->pool, 
					    ssock->param.send_buffer_size);
	ssock->send_buf.max_len = ssock->param.send_buffer_size;
	ssock->send_buf.start = ssock->send_buf.buf;
	ssock->send_buf.len = 0;
    }

    chunk_size = ssock->param.send_buffer_size / MAX_CHUNK_NUM / 2;
    chunk_cnt = 0;
    for (i = 0; i < MAX_CHUNK_NUM; i++) {
	wdata[i] = alloc_send_data(ssock, pj_rand() % chunk_size + 321);
	if (wdata[i])
	    chunk_cnt++;
	else
	    break;
    }

    while (chunk_cnt) {
	i = pj_rand() % MAX_CHUNK_NUM;
	if (wdata[i]) {
	    free_send_data(ssock, wdata[i]);
	    wdata[i] = NULL;
	    chunk_cnt--;
	}
    }

    if (ssock->send_buf.len != 0)
	status = PJ_EBUG;

    pj_ssl_sock_close(ssock);
    return status;
}
#endif

static void on_timer(pj_timer_heap_t *th, struct pj_timer_entry *te)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)te->user_data;
    int timer_id = te->id;

    te->id = TIMER_NONE;

    PJ_UNUSED_ARG(th);

    switch (timer_id) {
    case TIMER_HANDSHAKE_TIMEOUT:
	PJ_LOG(1,(ssock->pool->obj_name, "SSL timeout after %d.%ds",
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

static void ssl_on_destroy(void *arg)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)arg;

    ssl_destroy(ssock);

    if (ssock->circ_buf_input_mutex) {
        pj_lock_destroy(ssock->circ_buf_input_mutex);
	ssock->circ_buf_input_mutex = NULL;
    }

    if (ssock->circ_buf_output_mutex) {
        pj_lock_destroy(ssock->circ_buf_output_mutex);
	ssock->circ_buf_output_mutex = NULL;
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
        if (ssock->circ_buf_input_mutex)
	    pj_lock_acquire(ssock->circ_buf_input_mutex);
        status_ = io_write(ssock,&ssock->circ_buf_input, data, size);
        if (ssock->circ_buf_input_mutex)
            pj_lock_release(ssock->circ_buf_input_mutex);
        if (status_ != PJ_SUCCESS) {
            status = status_;
	    goto on_error;
	}
    }

    /* Check if SSL handshake hasn't finished yet */
    if (ssock->ssl_state == SSL_STATE_HANDSHAKING) {
	pj_bool_t ret = PJ_TRUE;

	if (status == PJ_SUCCESS)
	    status = ssl_do_handshake(ssock);

	/* Not pending is either success or failed */
	if (status != PJ_EPENDING)
	    ret = on_handshake_complete(ssock, status);

	return ret;
    }

    /* See if there is any decrypted data for the application */
    if (ssock->read_started) {
	do {
	    read_data_t *buf = *(OFFSET_OF_READ_DATA_PTR(ssock, data));
	    void *data_ = (pj_int8_t*)buf->data + buf->len;
	    int size_ = (int)(ssock->read_size - buf->len);
	    pj_status_t status_;

	    status_ = ssl_read(ssock, data_, &size_);

	    if (size_ > 0 || status != PJ_SUCCESS) {
		if (ssock->param.cb.on_data_read) {
		    pj_bool_t ret;
		    pj_size_t remainder_ = 0;

		    if (size_ > 0)
			buf->len += size_;
    		
                    if (status != PJ_SUCCESS) {
                        ssock->ssl_state = SSL_STATE_ERROR;
                    }

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
		if (status != PJ_SUCCESS) {
		    ssl_reset_sock_state(ssock);
		    return PJ_FALSE;
		}

	    } else if (status_ == PJ_SUCCESS) {
	    	break;
	    } else if (status_ == PJ_EEOF) {
		status = ssl_do_handshake(ssock);
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
    write_data_t *wdata = (write_data_t*)send_key->user_data;
    pj_ioqueue_op_key_t *app_key = wdata->app_key;
    pj_ssize_t sent_len;

    sent_len = (sent > 0)? wdata->plain_data_len : sent;
    
    /* Update write buffer state */
    pj_lock_acquire(ssock->write_mutex);
    free_send_data(ssock, wdata);
    pj_lock_release(ssock->write_mutex);
    wdata = NULL;

    if (ssock->ssl_state == SSL_STATE_HANDSHAKING) {
	/* Initial handshaking */
	pj_status_t status;
	
	status = ssl_do_handshake(ssock);
	/* Not pending is either success or failed */
	if (status != PJ_EPENDING)
	    return on_handshake_complete(ssock, status);

    } else if (send_key != &ssock->handshake_op_key) {
	/* Some data has been sent, notify application */
	if (ssock->param.cb.on_data_sent) {
	    pj_bool_t ret;
	    ret = (*ssock->param.cb.on_data_sent)(ssock, app_key, 
						  sent_len);
	    if (!ret) {
		/* We've been destroyed */
		return PJ_FALSE;
	    }
	}
    } else {
	/* SSL re-negotiation is on-progress, just do nothing */
    }

    /* Send buffer has been updated, let's try to send any pending data */
    if (ssock->send_buf_pending.data_len) {
	pj_status_t status;
	status = flush_circ_buf_output(ssock, ssock->send_buf_pending.app_key,
				 ssock->send_buf_pending.plain_data_len,
				 ssock->send_buf_pending.flags);
	if (status == PJ_SUCCESS || status == PJ_EPENDING) {
	    ssock->send_buf_pending.data_len = 0;
	}
    }

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
    pj_ssl_sock_t *ssock;
#ifndef SSL_SOCK_IMP_USE_OWN_NETWORK
    pj_activesock_cb asock_cb;
    pj_activesock_cfg asock_cfg;
#endif
    unsigned i;
    pj_status_t status;

#ifndef SSL_SOCK_IMP_USE_OWN_NETWORK
    PJ_UNUSED_ARG(newconn);
#endif

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

    /* Prepare write/send state */
    pj_assert(ssock->send_buf.max_len == 0);
    ssock->send_buf.buf = (char*)
			  pj_pool_alloc(ssock->pool, 
					ssock->param.send_buffer_size);
    if (!ssock->send_buf.buf)
        return PJ_ENOMEM;

    ssock->send_buf.max_len = ssock->param.send_buffer_size;
    ssock->send_buf.start = ssock->send_buf.buf;
    ssock->send_buf.len = 0;

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
	    status = PJ_SUCCESS;
	}
    }

    /* Start SSL handshake */
    ssock->ssl_state = SSL_STATE_HANDSHAKING;
    ssl_set_state(ssock, PJ_TRUE);
    status = ssl_do_handshake(ssock);

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
    if (!ssock->asock_rbuf)
        return PJ_ENOMEM;

    for (i = 0; i<ssock->param.async_cnt; ++i) {
	ssock->asock_rbuf[i] = (void*) pj_pool_alloc(
					    ssock->pool, 
					    ssock->param.read_buffer_size + 
					    sizeof(read_data_t*));
        if (!ssock->asock_rbuf[i])
            return PJ_ENOMEM;
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

    /* Prepare write/send state */
    pj_assert(ssock->send_buf.max_len == 0);
    ssock->send_buf.buf = (char*)
			     pj_pool_alloc(ssock->pool, 
					   ssock->param.send_buffer_size);
    if (!ssock->send_buf.buf)
        return PJ_ENOMEM;

    ssock->send_buf.max_len = ssock->param.send_buffer_size;
    ssock->send_buf.start = ssock->send_buf.buf;
    ssock->send_buf.len = 0;

    /* Set peer name */
    ssl_set_peer_name(ssock);

    /* Start SSL handshake */
    ssock->ssl_state = SSL_STATE_HANDSHAKING;
    ssl_set_state(ssock, PJ_FALSE);

    status = ssl_do_handshake(ssock);
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
    PJ_ASSERT_RETURN(param->sock_type == pj_SOCK_STREAM(), PJ_ENOTSUP);

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
    ssock->circ_buf_input.owner = ssock;
    ssock->circ_buf_output.owner = ssock;
    pj_list_init(&ssock->write_pending);
    pj_list_init(&ssock->write_pending_empty);
    pj_list_init(&ssock->send_pending);
    pj_timer_entry_init(&ssock->timer, 0, ssock, &on_timer);
    pj_ioqueue_op_key_init(&ssock->handshake_op_key,
			   sizeof(pj_ioqueue_op_key_t));
    pj_ioqueue_op_key_init(&ssock->shutdown_op_key,
			   sizeof(pj_ioqueue_op_key_t));

    /* Create secure socket mutex */
    status = pj_lock_create_recursive_mutex(pool, pool->obj_name,
                                            &ssock->circ_buf_output_mutex);
    ssock->write_mutex = ssock->circ_buf_output_mutex;
    if (status != PJ_SUCCESS)
        return status;

    /* Create input circular buffer mutex */
    status = pj_lock_create_simple_mutex(pool, pool->obj_name,
                                         &ssock->circ_buf_input_mutex);
    if (status != PJ_SUCCESS)
        return status;

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

    /* Wipe out cert & key buffer. */
    if (ssock->cert) {
	pj_ssl_cert_wipe_keys(ssock->cert);
	ssock->cert = NULL;
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

	/* Verification status */
	info->verify_status = ssock->verify_status;
    }

    /* Last known SSL error code */
    info->last_native_err = ssock->last_err;

    /* Group lock */
    info->grp_lock = ssock->param.grp_lock;

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
    int nwritten;

    /* Write the plain data to SSL, after SSL encrypts it, the buffer will
     * contain the secured data to be sent via socket. Note that re-
     * negotitation may be on progress, so sending data should be delayed
     * until re-negotiation is completed.
     */
    pj_lock_acquire(ssock->write_mutex);
    /* Don't write to SSL if send buffer is full and some data is in
     * write buffer already, just return PJ_ENOMEM.
     */
    if (ssock->send_buf_pending.data_len) {
	pj_lock_release(ssock->write_mutex);
	return PJ_ENOMEM;
    }
    status = ssl_write(ssock, data, size, &nwritten);
    pj_lock_release(ssock->write_mutex);
    
    if (status == PJ_SUCCESS && nwritten == size) {
	/* All data written, flush write buffer to network socket */
	status = flush_circ_buf_output(ssock, send_key, size, flags);
    } else if (status == PJ_EEOF) {
        /* Re-negotiation is on progress, flush re-negotiation data */
	status = flush_circ_buf_output(ssock, &ssock->handshake_op_key, 0, 0);
	if (status == PJ_SUCCESS || status == PJ_EPENDING) {
	    /* Just return PJ_EBUSY when re-negotiation is on progress */
	    status = PJ_EBUSY;
	}
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
	pj_status_t status;

	wp = ssock->write_pending.next;

	/* Ticket #1573: Don't hold mutex while calling socket send. */
	pj_lock_release(ssock->write_mutex);

	status = ssl_send (ssock, &wp->key, wp->data.ptr, 
			   wp->plain_data_len, wp->flags);
	if (status != PJ_SUCCESS) {
	    /* Reset ongoing flush flag first. */
	    ssock->flushing_write_pend = PJ_FALSE;
	    return status;
	}

	pj_lock_acquire(ssock->write_mutex);
	pj_list_erase(wp);
	pj_list_push_back(&ssock->write_pending_empty, wp);
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
    if (status == PJ_EBUSY) {
	/* Re-negotiation or flushing is on progress, delay sending */
	status = delay_send(ssock, send_key, data, *size, flags);
	goto on_return;
    } else if (status != PJ_SUCCESS) {
	goto on_return;
    }

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

    if (status == PJ_SUCCESS)
	asock_on_connect_complete(ssock->asock, PJ_SUCCESS);
    else if (status != PJ_EPENDING)
	goto on_error;

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
	status = ssl_do_handshake(ssock);
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
	wipe_buf(&cert->CA_file);
	wipe_buf(&cert->CA_path);
	wipe_buf(&cert->cert_file);
	wipe_buf(&cert->privkey_file);
	wipe_buf(&cert->privkey_pass);
	wipe_buf(&cert->CA_buf);
	wipe_buf(&cert->cert_buf);
	wipe_buf(&cert->privkey_buf);
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
    pj_ssl_cert_t *cert;

    PJ_ASSERT_RETURN(pool && (CA_file || CA_path) && cert_file &&
		     privkey_file,
		     PJ_EINVAL);

    cert = PJ_POOL_ZALLOC_T(pool, pj_ssl_cert_t);
    if (CA_file) {
    	pj_strdup_with_null(pool, &cert->CA_file, CA_file);
    }
    if (CA_path) {
    	pj_strdup_with_null(pool, &cert->CA_path, CA_path);
    }
    pj_strdup_with_null(pool, &cert->cert_file, cert_file);
    pj_strdup_with_null(pool, &cert->privkey_file, privkey_file);
    pj_strdup_with_null(pool, &cert->privkey_pass, privkey_pass);

    *p_cert = cert;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ssl_cert_load_from_buffer(pj_pool_t *pool,
					const pj_ssl_cert_buffer *CA_buf,
					const pj_ssl_cert_buffer *cert_buf,
					const pj_ssl_cert_buffer *privkey_buf,
					const pj_str_t *privkey_pass,
					pj_ssl_cert_t **p_cert)
{
    pj_ssl_cert_t *cert;

    PJ_ASSERT_RETURN(pool && CA_buf && cert_buf && privkey_buf, PJ_EINVAL);

    cert = PJ_POOL_ZALLOC_T(pool, pj_ssl_cert_t);
    pj_strdup(pool, &cert->CA_buf, CA_buf);
    pj_strdup(pool, &cert->cert_buf, cert_buf);
    pj_strdup(pool, &cert->privkey_buf, privkey_buf);
    pj_strdup_with_null(pool, &cert->privkey_pass, privkey_pass);

    *p_cert = cert;

    return PJ_SUCCESS;
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
    pj_strdup_with_null(pool, &cert_->CA_file, &cert->CA_file);
    pj_strdup_with_null(pool, &cert_->CA_path, &cert->CA_path);
    pj_strdup_with_null(pool, &cert_->cert_file, &cert->cert_file);
    pj_strdup_with_null(pool, &cert_->privkey_file, &cert->privkey_file);
    pj_strdup_with_null(pool, &cert_->privkey_pass, &cert->privkey_pass);

    pj_strdup(pool, &cert_->CA_buf, &cert->CA_buf);
    pj_strdup(pool, &cert_->cert_buf, &cert->cert_buf);
    pj_strdup(pool, &cert_->privkey_buf, &cert->privkey_buf);

    ssock->cert = cert_;

    return PJ_SUCCESS;
}

