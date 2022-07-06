/* 
 * Copyright (C) 2022 Teluu Inc. (http://www.teluu.com)
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

/*
 * ioqueue_common_wakeup.c
 *
 * This contains common functionalities to implement the feature wakeup
 * mechanisms
 *
 * This file will be included by the appropriate ioqueue implementation.
 * This file is NOT supposed to be compiled as stand-alone source.
 */

#if PJ_IOQUEUE_HAS_WAKEUP
#if 0
#define TRACE_WAKEUP(expr) PJ_LOG(3, expr)
#else
#define TRACE_WAKEUP(expr)
#endif

static void wakeup_on_read_complete(pj_ioqueue_key_t *key,
				    pj_ioqueue_op_key_t *op_key,
				    pj_ssize_t bytes_read)
{
    pj_status_t status;
    pj_ioqueue_t *ioqueue = (pj_ioqueue_t *)pj_ioqueue_get_user_data(key);

    if (bytes_read > 0) {
	TRACE_WAKEUP((THIS_FILE, "ioqueue:%p, hanle wakeup event", ioqueue));
    } else if (bytes_read != -PJ_EPENDING &&
	       bytes_read != -PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL)) {
	return;
    }
    bytes_read = sizeof(ioqueue->wakeup_buf);
    pj_ioqueue_recv(key, op_key, ioqueue->wakeup_buf, &bytes_read,
		    PJ_IOQUEUE_ALWAYS_ASYNC);
}

static pj_status_t ioqueue_wakeup_init(pj_ioqueue_t *ioqueue)
{
    pj_status_t rc;
    pj_pool_t *pool = ioqueue->pool;
    pj_ioqueue_callback cb;
    pj_ioqueue_key_t *read_key;
    pj_ioqueue_op_key_t *read_op;
    char errmsg[PJ_ERR_MSG_SIZE];

    TRACE_WAKEUP((THIS_FILE, "ioqueue:%p, wakeup init", ioqueue));

    /* create socket pair */
    rc = pj_sock_socketpair(pj_AF_UNIX(), pj_SOCK_DGRAM(), 0,
			    ioqueue->wakeup_fd);
    if (rc != PJ_SUCCESS) {
	pj_strerror(rc, errmsg, sizeof(errmsg));
	PJ_LOG(1, (THIS_FILE, "wakeup init, socketpair: %s(%d)", errmsg, rc));
	ioqueue->wakeup_fd[0] = ioqueue->wakeup_fd[1] = PJ_INVALID_SOCKET;
	return rc;
    }

    /* Add read fd: wakeup_fd[0] to ioqueue */
    pj_bzero(&cb, sizeof(cb));
    cb.on_read_complete = wakeup_on_read_complete;
    rc = pj_ioqueue_register_sock(pool, ioqueue, ioqueue->wakeup_fd[0], ioqueue,
				  &cb, &read_key);
    if (rc != PJ_SUCCESS) {
	pj_strerror(rc, errmsg, sizeof(errmsg));
	PJ_LOG(1, (THIS_FILE, "wakeup init, register: %s(%d)", errmsg, rc));
	pj_sock_close(ioqueue->wakeup_fd[0]);
	pj_sock_close(ioqueue->wakeup_fd[1]);
	ioqueue->wakeup_fd[0] = ioqueue->wakeup_fd[1] = PJ_INVALID_SOCKET;
	return rc;
    }

    /* start read */
    read_op = PJ_POOL_ZALLOC_T(pool, pj_ioqueue_op_key_t);
    wakeup_on_read_complete(read_key, read_op, -PJ_EPENDING);

    return PJ_SUCCESS;
}

static void ioqueue_wakeup_deinit(pj_ioqueue_t *ioqueue)
{
    TRACE_WAKEUP((THIS_FILE, "ioqueue:%p, wakeup deinit", ioqueue));
    if (ioqueue->wakeup_fd[0] != PJ_INVALID_SOCKET) {
	pj_sock_close(ioqueue->wakeup_fd[0]);
	ioqueue->wakeup_fd[0] = PJ_INVALID_SOCKET;
    }

    if (ioqueue->wakeup_fd[1] != PJ_INVALID_SOCKET) {
	pj_sock_close(ioqueue->wakeup_fd[1]);
	ioqueue->wakeup_fd[1] = PJ_INVALID_SOCKET;
    }
}

static pj_status_t ioqueue_wakeup_notify(pj_ioqueue_t *ioqueue)
{
    pj_ssize_t len = 1;
    PJ_ASSERT_RETURN(ioqueue->wakeup_fd[1] != PJ_INVALID_SOCKET, PJ_EINVALIDOP);
    if (ioqueue->wakeup_fd[1] == PJ_INVALID_SOCKET)
	return PJ_EINVALIDOP;

    TRACE_WAKEUP((THIS_FILE, "ioqueue:%p, send wakeup event", ioqueue));
    return pj_sock_send(ioqueue->wakeup_fd[1], "1", &len, 0);
}

/*
 * pj_ioqueue_wakeup()
 *
 * Wakeup ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_wakeup(pj_ioqueue_t *ioqueue)
{
    return ioqueue_wakeup_notify(ioqueue);
}
#endif
