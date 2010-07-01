/* $Id$ */
/*
 * Copyright (C) 2010 Teluu Inc. (http://www.teluu.com)
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

#include <pjlib-util/cli_imp.h>
#include <pjlib-util/cli_telnet.h>
#include <pj/activesock.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pjlib-util/errno.h>

#define CLI_TELNET_BUF_SIZE 256

#define CUT_MSG "<..data truncated..>\r\n"
#define MAX_CUT_MSG_LEN 25

#if 1
    /* Enable some tracing */
    #define THIS_FILE   "cli_telnet.c"
    #define TRACE_(arg)	PJ_LOG(3,arg)
#else
    #define TRACE_(arg)
#endif

struct cli_telnet_sess
{
    pj_cli_sess         base;
    pj_pool_t          *pool;
    pj_activesock_t    *asock;
    pj_bool_t           authorized;
    pj_ioqueue_op_key_t op_key;
    pj_mutex_t         *smutex;

    char                rcmd[PJ_CLI_MAX_CMDBUF];
    int                 len;
    char                buf[CLI_TELNET_BUF_SIZE + MAX_CUT_MSG_LEN];
    unsigned            buf_len;
};

struct cli_telnet_fe
{
    pj_cli_front_end        base;
    pj_pool_t              *pool;
    pj_cli_telnet_cfg       cfg;
    pj_bool_t               own_ioqueue;
    pj_cli_sess             sess_head;

    pj_activesock_t	   *asock;
    pj_thread_t            *worker_thread;
    pj_bool_t               is_quitting;
    pj_mutex_t             *mutex;
};

PJ_DEF(void) pj_cli_telnet_cfg_default(pj_cli_telnet_cfg *param)
{
    pj_assert(param);

    pj_bzero(param, sizeof(*param));
    param->port = PJ_CLI_TELNET_PORT;
    param->log_level = PJ_CLI_TELNET_LOG_LEVEL;
}


/* Send a message to a telnet session */
static pj_status_t telnet_sess_send(struct cli_telnet_sess *sess,
                                    const pj_str_t *str)
{
    pj_ssize_t sz;
    pj_status_t status = PJ_SUCCESS;

    sz = str->slen;
    if (!sz)
        return PJ_SUCCESS;

    pj_mutex_lock(sess->smutex);

    if (sess->buf_len == 0)
        status = pj_activesock_send(sess->asock, &sess->op_key, 
                                    str->ptr, &sz, 0);
    /* If we cannot send now, append it at the end of the buffer 
     * to be sent later.
     */
    if (sess->buf_len > 0 || 
        (status != PJ_SUCCESS && status != PJ_EPENDING))
    {
        int clen = (int)sz;

        if (sess->buf_len + clen > CLI_TELNET_BUF_SIZE)
            clen = CLI_TELNET_BUF_SIZE - sess->buf_len;
        if (clen > 0)
            pj_memmove(sess->buf + sess->buf_len, str->ptr, clen);
        if (clen < sz) {
            pj_ansi_snprintf(sess->buf + CLI_TELNET_BUF_SIZE,
                             MAX_CUT_MSG_LEN, CUT_MSG);
            sess->buf_len = CLI_TELNET_BUF_SIZE +
                            pj_ansi_strlen(sess->buf + CLI_TELNET_BUF_SIZE);
        } else
            sess->buf_len += clen;
    } else if (status == PJ_SUCCESS && sz < str->slen) {
        pj_mutex_unlock(sess->smutex);
        return PJ_CLI_ETELNETLOST;
    }

    pj_mutex_unlock(sess->smutex);

    return PJ_SUCCESS;
}

static pj_status_t telnet_sess_send2(struct cli_telnet_sess *sess,
                                     const char *str, int len)
{
    pj_str_t s;

    pj_strset(&s, (char *)str, len);
    return telnet_sess_send(sess, &s);
}

static void cli_telnet_sess_destroy(pj_cli_sess *sess)
{
    struct cli_telnet_sess *tsess = (struct cli_telnet_sess *)sess;
    pj_mutex_t *mutex = ((struct cli_telnet_fe *)sess->fe)->mutex;

    pj_mutex_lock(mutex);
    pj_list_erase(sess);
    pj_mutex_unlock(mutex);

    pj_mutex_lock(tsess->smutex);
    pj_mutex_unlock(tsess->smutex);
    pj_mutex_destroy(tsess->smutex);
    pj_activesock_close(tsess->asock);
    pj_pool_release(tsess->pool);
}

static void cli_telnet_fe_write_log(pj_cli_front_end *fe, int level,
		                    const char *data, int len)
{
    struct cli_telnet_fe * tfe = (struct cli_telnet_fe *)fe;
    pj_cli_sess *sess;

    pj_mutex_lock(tfe->mutex);

    sess = tfe->sess_head.next;
    while (sess != &tfe->sess_head) {
        struct cli_telnet_sess *tsess = (struct cli_telnet_sess *)sess;

        sess = sess->next;
        if (tsess->base.log_level > level && tsess->authorized)
            telnet_sess_send2(tsess, data, len);
    }
    
    pj_mutex_unlock(tfe->mutex);
}

static void cli_telnet_fe_destroy(pj_cli_front_end *fe)
{
    struct cli_telnet_fe *tfe = (struct cli_telnet_fe *)fe;
    pj_cli_sess *sess;

    tfe->is_quitting = PJ_TRUE;
    if (tfe->worker_thread)
        pj_thread_join(tfe->worker_thread);

    pj_mutex_lock(tfe->mutex);

    /* Destroy all the sessions */
    sess = tfe->sess_head.next;
    while (sess != &tfe->sess_head) {
        (*sess->op->destroy)(sess);
        sess = tfe->sess_head.next;
    }

    pj_mutex_unlock(tfe->mutex);

    pj_activesock_close(tfe->asock);
    if (tfe->own_ioqueue)
        pj_ioqueue_destroy(tfe->cfg.ioqueue);
    pj_mutex_destroy(tfe->mutex);
    pj_pool_release(tfe->pool);
}

static int poll_worker_thread(void *p)
{
    struct cli_telnet_fe *fe = (struct cli_telnet_fe *)p;

    while (!fe->is_quitting) {
	pj_time_val delay = {0, 50};
        pj_ioqueue_poll(fe->cfg.ioqueue, &delay);
    }

    return 0;
}

static pj_bool_t telnet_sess_on_data_sent(pj_activesock_t *asock,
 				          pj_ioqueue_op_key_t *op_key,
				          pj_ssize_t sent)
{
    struct cli_telnet_sess *sess = (struct cli_telnet_sess *)
                                    pj_activesock_get_user_data(asock);

    PJ_UNUSED_ARG(op_key);

    if (sent <= 0) {
        pj_cli_end_session(&sess->base);
        return PJ_FALSE;
    }

    pj_mutex_lock(sess->smutex);

    if (sess->buf_len) {
        int len = sess->buf_len;

        sess->buf_len = 0;
        if (telnet_sess_send2(sess, sess->buf, len) != PJ_SUCCESS) {
            pj_mutex_unlock(sess->smutex);
            pj_cli_end_session(&sess->base);
            return PJ_FALSE;
        }
    }

    pj_mutex_unlock(sess->smutex);

    return PJ_TRUE;
}

static pj_bool_t telnet_sess_on_data_read(pj_activesock_t *asock,
		                          void *data,
			                  pj_size_t size,
			                  pj_status_t status,
			                  pj_size_t *remainder)
{
    struct cli_telnet_sess *sess = (struct cli_telnet_sess *)
                                    pj_activesock_get_user_data(asock);
    struct cli_telnet_fe *tfe = (struct cli_telnet_fe *)sess->base.fe;
    char *cdata = (char*)data;

    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(remainder);

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
        pj_cli_end_session(&sess->base);
        return PJ_FALSE;
    }

    if (tfe->is_quitting)
        return PJ_FALSE;

    pj_mutex_lock(sess->smutex);

    if (*cdata == 8) {                          // Backspace
        if (sess->len > 0)
            sess->len--;
        if (telnet_sess_send2(sess, " \b", 2) != PJ_SUCCESS)
            goto on_exit;
    } else {
        if (sess->len < PJ_CLI_MAX_CMDBUF - 1)
            sess->rcmd[sess->len++] = *cdata;
        if (*cdata == '\n') {
            /* Trim trailing newlines */
            for (; sess->len > 0;) {
                if (sess->rcmd[sess->len - 1] == '\n' ||
                    sess->rcmd[sess->len - 1] == '\r')
                    sess->rcmd[--sess->len] = 0;
                else
                    break;
            }
            if (sess->len > 0 && sess->rcmd[sess->len - 1] != 0)
                sess->rcmd[sess->len++] = 0;

            /* If a password is necessary, check whether the supplied password
             * is correct.
             */
            if (!sess->authorized && 
                ((struct cli_telnet_fe *)sess->base.fe)->cfg.passwd.slen > 0)
            {
                if (pj_strcmp2(&tfe->cfg.passwd, sess->rcmd)) {
                    pj_str_t str = pj_str("Wrong password!\r\n");

                    telnet_sess_send(sess, &str);
                    goto on_exit;
                } else {
                    /* Password is correct, send welcome message */
                    if (telnet_sess_send(sess, &tfe->cfg.welcome_msg)
                        != PJ_SUCCESS)
                    {
                        goto on_exit;
                    }
                    sess->authorized = PJ_TRUE;
                }
            } else {
                pj_status_t status;
                
                pj_mutex_unlock(sess->smutex);
                status = pj_cli_exec(&sess->base, sess->rcmd, NULL);
                if (status == PJ_CLI_EEXIT)
                    return PJ_FALSE;
                pj_mutex_lock(sess->smutex);
            }
            sess->len = 0;
        } else if (!sess->authorized && 
            ((struct cli_telnet_fe *)sess->base.fe)->cfg.passwd.slen > 0 &&
            *cdata != '\r')
        {
            if (telnet_sess_send2(sess, "\b ", 2) != PJ_SUCCESS)
                goto on_exit;
        }

    }

    pj_mutex_unlock(sess->smutex);

    return PJ_TRUE;

on_exit:
    pj_mutex_unlock(sess->smutex);
    pj_cli_end_session(&sess->base);
    return PJ_FALSE;
}

static pj_bool_t telnet_fe_on_accept(pj_activesock_t *asock,
				     pj_sock_t newsock,
				     const pj_sockaddr_t *src_addr,
				     int src_addr_len)
{
    struct cli_telnet_fe *fe = (struct cli_telnet_fe *)
                                pj_activesock_get_user_data(asock);
    pj_status_t sstatus;
    pj_pool_t *pool;
    struct cli_telnet_sess *sess;
    pj_activesock_cb asock_cb;

    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    if (fe->is_quitting)
        return PJ_FALSE;

    /* An incoming connection is accepted, create a new session */
    pool = pj_pool_create(fe->pool->factory, "telnet_sess",
                          PJ_CLI_TELNET_POOL_SIZE, PJ_CLI_TELNET_POOL_INC,
                          NULL);
    if (!pool) {
        TRACE_((THIS_FILE, 
                "Not enough memory to create a new telnet session"));
        return PJ_TRUE;
    }

    sess = PJ_POOL_ZALLOC_T(pool, struct cli_telnet_sess);
    sess->pool = pool;
    sess->base.fe = &fe->base;
    sess->base.log_level = fe->cfg.log_level;
    sess->base.op = PJ_POOL_ZALLOC_T(pool, struct pj_cli_sess_op);
    sess->base.op->destroy = &cli_telnet_sess_destroy;
    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_data_read = &telnet_sess_on_data_read;
    asock_cb.on_data_sent = &telnet_sess_on_data_sent;

    sstatus = pj_mutex_create_recursive(pool, "mutex_telnet_sess",
                                        &sess->smutex);
    if (sstatus != PJ_SUCCESS)
        goto on_exit;

    sstatus = pj_activesock_create(pool, newsock, pj_SOCK_STREAM(),
                                   NULL, fe->cfg.ioqueue,
			           &asock_cb, sess, &sess->asock);
    if (sstatus != PJ_SUCCESS) {
        TRACE_((THIS_FILE, "Failure creating active socket"));
        goto on_exit;
    }

    /* Prompt for password if required, otherwise directly send
     * a welcome message.
     */
    if (fe->cfg.passwd.slen) {
        pj_str_t pwd = pj_str("Password: ");
        if (telnet_sess_send(sess, &pwd) != PJ_SUCCESS)
            goto on_exit;
    } else {
        if (pj_strlen(&fe->cfg.welcome_msg)) {
            if (telnet_sess_send(sess, &fe->cfg.welcome_msg) != PJ_SUCCESS)
                goto on_exit;
        } else {
            if (telnet_sess_send2(sess, " \b", 2) != PJ_SUCCESS)
                goto on_exit;
        }
        sess->authorized = PJ_TRUE;
    }

    /* Start reading for input from the new telnet session */
    sstatus = pj_activesock_start_read(sess->asock, pool, 1, 0);
    if (sstatus != PJ_SUCCESS) {
        TRACE_((THIS_FILE, "Failure reading active socket"));
        goto on_exit;
    }

    pj_ioqueue_op_key_init(&sess->op_key, sizeof(sess->op_key));
    pj_mutex_lock(fe->mutex);
    pj_list_push_back(&fe->sess_head, &sess->base);
    pj_mutex_unlock(fe->mutex);

    return PJ_TRUE;

on_exit:
    if (sess->asock)
        pj_activesock_close(sess->asock);
    else
        pj_sock_close(newsock);
    
    if (sess->smutex)
        pj_mutex_destroy(sess->smutex);

    pj_pool_release(pool);

    return PJ_TRUE;
}

PJ_DEF(pj_status_t) pj_cli_telnet_create(pj_cli_t *cli,
					 const pj_cli_telnet_cfg *param,
					 pj_cli_front_end **p_fe)
{
    struct cli_telnet_fe *fe;
    pj_pool_t *pool;
    pj_sock_t sock = PJ_INVALID_SOCKET;
    pj_activesock_cb asock_cb;
    pj_sockaddr_in addr;
    pj_status_t sstatus;

    PJ_ASSERT_RETURN(cli, PJ_EINVAL);

    pool = pj_pool_create(pj_cli_get_param(cli)->pf, "telnet_fe",
                          PJ_CLI_TELNET_POOL_SIZE, PJ_CLI_TELNET_POOL_INC,
                          NULL);
    fe = PJ_POOL_ZALLOC_T(pool, struct cli_telnet_fe);
    if (!fe)
        return PJ_ENOMEM;
    fe->base.op = PJ_POOL_ZALLOC_T(pool, struct pj_cli_front_end_op);

    if (!param)
        pj_cli_telnet_cfg_default(&fe->cfg);
    else
        pj_memcpy(&fe->cfg, param, sizeof(*param));
    pj_list_init(&fe->sess_head);
    fe->base.cli = cli;
    fe->base.type = PJ_CLI_TELNET_FRONT_END;
    fe->base.op->on_write_log = &cli_telnet_fe_write_log;
//    fe->base.op->on_quit = &cli_telnet_fe_quit;
    fe->base.op->on_destroy = &cli_telnet_fe_destroy;
    fe->pool = pool;

    if (!fe->cfg.ioqueue) {
        /* Create own ioqueue if application doesn't supply one */
        sstatus = pj_ioqueue_create(pool, 8, &fe->cfg.ioqueue);
        if (sstatus != PJ_SUCCESS)
            goto on_exit;
        fe->own_ioqueue = PJ_TRUE;
    }

    sstatus = pj_mutex_create_recursive(pool, "mutex_telnet_fe", &fe->mutex);
    if (sstatus != PJ_SUCCESS)
        goto on_exit;

    /* Start telnet daemon */
    sstatus = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, 
                             &sock);
    if (sstatus != PJ_SUCCESS)
        goto on_exit;

    pj_sockaddr_in_init(&addr, NULL, fe->cfg.port);

    sstatus = pj_sock_bind(sock, &addr, sizeof(addr));
    if (sstatus == PJ_SUCCESS) {
	pj_sockaddr_in addr;
	int addr_len = sizeof(addr);

	sstatus = pj_sock_getsockname(sock, &addr, &addr_len);
	if (sstatus != PJ_SUCCESS)
	    goto on_exit;
        fe->cfg.port = pj_sockaddr_in_get_port(&addr);
        PJ_LOG(3, (THIS_FILE, "CLI telnet daemon listening at port %d",
               fe->cfg.port));
    } else {
        PJ_LOG(3, (THIS_FILE, "Failed binding the socket"));
        goto on_exit;
    }

    sstatus = pj_sock_listen(sock, 4);
    if (sstatus != PJ_SUCCESS)
        goto on_exit;

    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_accept_complete = &telnet_fe_on_accept;
    sstatus = pj_activesock_create(pool, sock, pj_SOCK_STREAM(),
                                   NULL, fe->cfg.ioqueue,
		                   &asock_cb, fe, &fe->asock);
    if (sstatus != PJ_SUCCESS)
        goto on_exit;

    sstatus = pj_activesock_start_accept(fe->asock, pool);
    if (sstatus != PJ_SUCCESS)
        goto on_exit;

    if (fe->own_ioqueue) {
        /* Create our own worker thread */
        sstatus = pj_thread_create(pool, "worker_telnet_fe",
                                   &poll_worker_thread, fe, 0, 0,
                                   &fe->worker_thread);
        if (sstatus != PJ_SUCCESS)
            goto on_exit;
    }

    pj_cli_register_front_end(cli, &fe->base);

    if (p_fe)
        *p_fe = &fe->base;

    return PJ_SUCCESS;

on_exit:
    if (fe->asock)
        pj_activesock_close(fe->asock);
    else if (sock != PJ_INVALID_SOCKET)
        pj_sock_close(sock);

    if (fe->own_ioqueue)
        pj_ioqueue_destroy(fe->cfg.ioqueue);

    if (fe->mutex)
        pj_mutex_destroy(fe->mutex);

    pj_pool_release(pool);
    return sstatus;
}
