/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include <pjnath/nat_detect.h>
#include <pjnath/errno.h>
#include <pj/assert.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/timer.h>
#include <pj/compat/socket.h>


static const char *nat_type_names[] =
{
    "Unknown",
    "Open Internet",
    "Blocked",
    "Symmetric UDP",
    "Full Cone",
    "Symmetric",
    "Restricted",
    "Port Restricted"
};


#define CHANGE_ADDR		4
#define CHANGE_PORT		2
#define CHANGE_ADDR_PORT	(CHANGE_ADDR | CHANGE_PORT)


enum state
{
    ST_TEST_1,
    ST_TEST_2,
    ST_TEST_1B,
    ST_TEST_3
};

static const char *test_names[] =
{
    "Test I: Binding request",
    "Test II: Binding request with change address and port request",
    "Test IB: Binding request to alternate address",
    "Test III: Binding request with change port request"
};

typedef struct nat_detect_session
{
    pj_pool_t		    *pool;
    pj_mutex_t		    *mutex;

    pj_timer_heap_t	    *timer_heap;
    pj_timer_entry	     destroy_timer;

    void		    *user_data;
    pj_stun_nat_detect_cb   *cb;
    pj_sock_t		     sock;
    pj_sockaddr_in	     local_addr;
    pj_ioqueue_key_t	    *key;
    pj_sockaddr_in	     server;
    pj_sockaddr_in	    *cur_server;
    pj_stun_session	    *stun_sess;
    enum state		     state;

    pj_ioqueue_op_key_t	     read_op, write_op;
    pj_uint8_t		     rx_pkt[PJ_STUN_MAX_PKT_LEN];
    pj_ssize_t		     rx_pkt_len;
    pj_sockaddr_in	     src_addr;
    int			     src_addr_len;

    pj_bool_t		     test1_same_ip;
    pj_sockaddr_in	     test1_ma;	    /* MAPPED-ADDRESS */
    pj_sockaddr_in	     test1_ca;	    /* CHANGED-ADDRESS */

} nat_detect_session;


static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);
static void on_request_complete(pj_stun_session *sess,
			        pj_status_t status,
			        pj_stun_tx_data *tdata,
			        const pj_stun_msg *response,
				const pj_sockaddr_t *src_addr,
				unsigned src_addr_len);
static pj_status_t on_send_msg(pj_stun_session *sess,
			       const void *pkt,
			       pj_size_t pkt_size,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len);

static pj_status_t start_test(nat_detect_session *sess,
			      enum state state,
			      const pj_sockaddr_in *alt_addr,
			      pj_uint32_t change_flag);
static void on_timer_destroy(pj_timer_heap_t *th,
			     pj_timer_entry *te);
static void sess_destroy(nat_detect_session *sess);

static pj_status_t get_local_interface(const pj_sockaddr_in *server,
				       pj_in_addr *local_addr)
{
    pj_sock_t sock;
    pj_sockaddr_in tmp;
    int addr_len;
    pj_status_t status;

    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_sock_bind_in(sock, 0, 0);
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    status = pj_sock_connect(sock, server, sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    addr_len = sizeof(pj_sockaddr_in);
    status = pj_sock_getsockname(sock, &tmp, &addr_len);
    if (status != PJ_SUCCESS) {
	pj_sock_close(sock);
	return status;
    }

    local_addr->s_addr = tmp.sin_addr.s_addr;
    
    pj_sock_close(sock);
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_stun_detect_nat_type(const pj_sockaddr_in *server,
					    pj_stun_config *stun_cfg,
					    void *user_data,
					    pj_stun_nat_detect_cb *cb)
{
    pj_pool_t *pool;
    nat_detect_session *sess;
    pj_stun_session_cb sess_cb;
    pj_ioqueue_callback ioqueue_cb;
    int addr_len;
    pj_status_t status;

    PJ_ASSERT_RETURN(server && stun_cfg, PJ_EINVAL);
    PJ_ASSERT_RETURN(stun_cfg->pf && stun_cfg->ioqueue && stun_cfg->timer_heap,
		     PJ_EINVAL);

    /*
     * Init NAT detection session.
     */
    pool = pj_pool_create(stun_cfg->pf, "natck%p", 512, 512, NULL);
    if (!pool)
	return PJ_ENOMEM;

    sess = PJ_POOL_ZALLOC_T(pool, nat_detect_session);
    sess->pool = pool;
    sess->user_data = user_data;
    sess->cb = cb;

    status = pj_mutex_create_recursive(pool, pool->obj_name, &sess->mutex);
    if (status != PJ_SUCCESS)
	goto on_error;
    
    pj_memcpy(&sess->server, server, sizeof(pj_sockaddr_in));

    /*
     * Init timer to self-destroy.
     */
    sess->timer_heap = stun_cfg->timer_heap;
    sess->destroy_timer.cb = &on_timer_destroy;
    sess->destroy_timer.user_data = sess;


    /*
     * Initialize socket.
     */
    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &sess->sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /*
     * Bind to any.
     */
    pj_bzero(&sess->local_addr, sizeof(pj_sockaddr_in));
    sess->local_addr.sin_family = PJ_AF_INET;
    status = pj_sock_bind(sess->sock, &sess->local_addr, 
			  sizeof(pj_sockaddr_in));
    if (status != PJ_SUCCESS)
	goto on_error;

    /*
     * Get local/bound address.
     */
    addr_len = sizeof(sess->local_addr);
    status = pj_sock_getsockname(sess->sock, &sess->local_addr, &addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;

    /*
     * Find out which interface is used to send to the server.
     */
    status = get_local_interface(server, &sess->local_addr.sin_addr);
    if (status != PJ_SUCCESS)
	goto on_error;

    PJ_LOG(5,(sess->pool->obj_name, "Local address is %s:%d",
	      pj_inet_ntoa(sess->local_addr.sin_addr), 
	      pj_ntohs(sess->local_addr.sin_port)));

    /*
     * Register socket to ioqueue to receive asynchronous input
     * notification.
     */
    pj_bzero(&ioqueue_cb, sizeof(ioqueue_cb));
    ioqueue_cb.on_read_complete = &on_read_complete;

    status = pj_ioqueue_register_sock(sess->pool, stun_cfg->ioqueue, 
				      sess->sock, sess, &ioqueue_cb,
				      &sess->key);
    if (status != PJ_SUCCESS)
	goto on_error;

    /*
     * Create STUN session.
     */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_request_complete = &on_request_complete;
    sess_cb.on_send_msg = &on_send_msg;
    status = pj_stun_session_create(stun_cfg, pool->obj_name, &sess_cb,
				    PJ_FALSE, &sess->stun_sess);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_stun_session_set_user_data(sess->stun_sess, sess);

    /*
     * Kick-off ioqueue reading.
     */
    pj_ioqueue_op_key_init(&sess->read_op, sizeof(sess->read_op));
    pj_ioqueue_op_key_init(&sess->write_op, sizeof(sess->write_op));
    on_read_complete(sess->key, &sess->read_op, 0);

    /*
     * Start TEST_1
     */
    PJ_LOG(5,(sess->pool->obj_name, "Server set to %s:%d",
	      pj_inet_ntoa(server->sin_addr), 
	      pj_ntohs(server->sin_port)));

    status = start_test(sess, ST_TEST_1, NULL, 0);
    if (status != PJ_SUCCESS)
	goto on_error;

    return PJ_SUCCESS;

on_error:
    sess_destroy(sess);
    return status;
}


static void sess_destroy(nat_detect_session *sess)
{
    if (sess->stun_sess) { 
	pj_stun_session_destroy(sess->stun_sess);
    }

    if (sess->key) {
	pj_ioqueue_unregister(sess->key);
    } else if (sess->sock && sess->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(sess->sock);
    }

    if (sess->mutex) {
	pj_mutex_destroy(sess->mutex);
    }

    if (sess->pool) {
	pj_pool_release(sess->pool);
    }
}


static pj_status_t start_test(nat_detect_session *sess,
			      enum state state,
			      const pj_sockaddr_in *alt_addr,
			      pj_uint32_t change_flag)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    /* Create BIND request */
    status = pj_stun_session_create_req(sess->stun_sess, 
					PJ_STUN_BINDING_REQUEST, 0x83224,
					NULL, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Add CHANGE-REQUEST attribute */
    status = pj_stun_msg_add_uint_attr(sess->pool, tdata->msg,
				       PJ_STUN_ATTR_CHANGE_REQUEST,
				       change_flag);
    if (status != PJ_SUCCESS)
	return status;

    /* Configure alternate address */
    if (alt_addr)
	sess->cur_server = (pj_sockaddr_in*) alt_addr;
    else
	sess->cur_server = &sess->server;

    PJ_LOG(5,(sess->pool->obj_name, 
              "Performing %s to %s:%d", 
	      test_names[state],
	      pj_inet_ntoa(sess->cur_server->sin_addr),
	      pj_ntohs(sess->cur_server->sin_port)));

    /* Send the request */
    status = pj_stun_session_send_msg(sess->stun_sess, PJ_TRUE, 
				      sess->cur_server, 
				      sizeof(pj_sockaddr_in),
				      tdata);
    if (status != PJ_SUCCESS)
	return status;

    sess->state = state;

    return PJ_SUCCESS;
}


static void end_session(nat_detect_session *sess,
			pj_status_t status,
			pj_stun_nat_type nat_type)
{
    pj_stun_nat_detect_result result;
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_time_val delay;

    pj_bzero(&result, sizeof(result));
    errmsg[0] = '\0';
    result.status_text = errmsg;

    result.status = status;
    pj_strerror(status, errmsg, sizeof(errmsg));
    result.nat_type = nat_type;
    result.nat_type_name = nat_type_names[result.nat_type];

    if (sess->cb)
	(*sess->cb)(sess->user_data, &result);

    delay.sec = 0;
    delay.msec = 0;

    pj_timer_heap_schedule(sess->timer_heap, &sess->destroy_timer, &delay);
}


/*
 * Callback upon receiving packet from network.
 */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    nat_detect_session *sess;
    pj_status_t status;

    sess = (nat_detect_session *) pj_ioqueue_get_user_data(key);
    pj_assert(sess != NULL);

    pj_mutex_lock(sess->mutex);

    if (bytes_read < 0) {
	if (-bytes_read != PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK) &&
	    -bytes_read != PJ_STATUS_FROM_OS(OSERR_EINPROGRESS) && 
	    -bytes_read != PJ_STATUS_FROM_OS(OSERR_ECONNRESET)) 
	{
	    /* Permanent error */
	    end_session(sess, -bytes_read, PJ_STUN_NAT_TYPE_UNKNOWN);
	    goto on_return;
	}

    } else if (bytes_read > 0) {
	pj_stun_session_on_rx_pkt(sess->stun_sess, sess->rx_pkt, bytes_read,
				  PJ_STUN_IS_DATAGRAM|PJ_STUN_CHECK_PACKET, 
				  NULL, &sess->src_addr, sess->src_addr_len);
    }


    sess->rx_pkt_len = sizeof(sess->rx_pkt);
    sess->src_addr_len = sizeof(sess->src_addr);
    status = pj_ioqueue_recvfrom(key, op_key, sess->rx_pkt, &sess->rx_pkt_len,
				 PJ_IOQUEUE_ALWAYS_ASYNC, 
				 &sess->src_addr, &sess->src_addr_len);

    if (status != PJ_EPENDING) {
	pj_assert(status != PJ_SUCCESS);
	end_session(sess, status, PJ_STUN_NAT_TYPE_UNKNOWN);
    }

on_return:
    pj_mutex_unlock(sess->mutex);
}


/*
 * Callback to send outgoing packet from STUN session.
 */
static pj_status_t on_send_msg(pj_stun_session *stun_sess,
			       const void *pkt,
			       pj_size_t pkt_size,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len)
{
    nat_detect_session *sess;
    pj_ssize_t pkt_len;

    sess = (nat_detect_session*) pj_stun_session_get_user_data(stun_sess);

    pkt_len = pkt_size;
    return pj_ioqueue_sendto(sess->key, &sess->write_op, pkt, &pkt_len, 0,
			     dst_addr, addr_len);

}

/*
 * Callback upon request completion.
 */
static void on_request_complete(pj_stun_session *stun_sess,
			        pj_status_t status,
			        pj_stun_tx_data *tdata,
			        const pj_stun_msg *response,
				const pj_sockaddr_t *src_addr,
				unsigned src_addr_len)
{
    nat_detect_session *sess;
    pj_stun_sockaddr_attr *mattr = NULL;

    PJ_UNUSED_ARG(tdata);
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    sess = (nat_detect_session*) pj_stun_session_get_user_data(stun_sess);

    pj_mutex_lock(sess->mutex);

    /* Find errors in the response */
    if (status == PJ_SUCCESS) {

	/* Check error message */
	if (PJ_STUN_IS_ERROR_RESPONSE(response->hdr.type)) {
	    pj_stun_errcode_attr *eattr;
	    int err_code;

	    eattr = (pj_stun_errcode_attr*)
		    pj_stun_msg_find_attr(response, PJ_STUN_ATTR_ERROR_CODE, 0);

	    if (eattr != NULL)
		err_code = eattr->err_code;
	    else
		err_code = PJ_STUN_SC_SERVER_ERROR;

	    status = PJ_STATUS_FROM_STUN_CODE(err_code);


	} else {

	    /* Get MAPPED-ADDRESS or XOR-MAPPED-ADDRESS */
	    mattr = (pj_stun_sockaddr_attr*)
		    pj_stun_msg_find_attr(response, PJ_STUN_ATTR_XOR_MAPPED_ADDR, 0);
	    if (mattr == NULL) {
		mattr = (pj_stun_sockaddr_attr*)
			pj_stun_msg_find_attr(response, PJ_STUN_ATTR_MAPPED_ADDR, 0);
	    }

	    if (mattr == NULL) {
		status = PJNATH_ESTUNNOMAPPEDADDR;
	    }
	}
    }

    /* Handle the test result according to RFC 3489 page 22:


                        +--------+
                        |  Test  |
                        |   1    |
                        +--------+
                             |
                             |
                             V
                            /\              /\
                         N /  \ Y          /  \ Y             +--------+
          UDP     <-------/Resp\--------->/ IP \------------->|  Test  |
          Blocked         \ ?  /          \Same/              |   2    |
                           \  /            \? /               +--------+
                            \/              \/                    |
                                             | N                  |
                                             |                    V
                                             V                    /\
                                         +--------+  Sym.      N /  \
                                         |  Test  |  UDP    <---/Resp\
                                         |   2    |  Firewall   \ ?  /
                                         +--------+              \  /
                                             |                    \/
                                             V                     |Y
                  /\                         /\                    |
   Symmetric  N  /  \       +--------+   N  /  \                   V
      NAT  <--- / IP \<-----|  Test  |<--- /Resp\               Open
                \Same/      |   1B   |     \ ?  /               Internet
                 \? /       +--------+      \  /
                  \/                         \/
                  |                           |Y
                  |                           |
                  |                           V
                  |                           Full
                  |                           Cone
                  V              /\
              +--------+        /  \ Y
              |  Test  |------>/Resp\---->Restricted
              |   3    |       \ ?  /
              +--------+        \  /
                                 \/
                                  |N
                                  |       Port
                                  +------>Restricted

                 Figure 2: Flow for type discovery process
     */

    switch (sess->state) {
    case ST_TEST_1:
	if (status == PJ_SUCCESS) {
	    pj_stun_changed_addr_attr *ca;

	    /* Get CHANGED-ADDRESS attribute */
	    ca = (pj_stun_changed_addr_attr*)
		 pj_stun_msg_find_attr(response, PJ_STUN_ATTR_CHANGED_ADDR, 0);

	    if (ca) {
		pj_memcpy(&sess->test1_ca, &ca->sockaddr, 
			  sizeof(pj_sockaddr_in));
	    }

	    /* Save mapped address */
	    pj_memcpy(&sess->test1_ma, &mattr->sockaddr,
		      sizeof(pj_sockaddr_in));

	    /* Compare mapped address with local address */
	    sess->test1_same_ip=(pj_memcmp(&sess->local_addr, &mattr->sockaddr,
					   sizeof(pj_sockaddr_in))==0);
	    
	    /* Execute test 2:
	     * Send BINDING_REQUEST with both the "change IP" and "change port" 
	     * flags from the CHANGE-REQUEST attribute set
	     */
	    start_test(sess, ST_TEST_2, NULL, CHANGE_ADDR_PORT);

	} else {
	    /* Test 1 has completed with error.
	     * Terminate our test session.
	     */
	    if (status == PJNATH_ESTUNTIMEDOUT)
		end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_BLOCKED);
	    else
		end_session(sess, status, PJ_STUN_NAT_TYPE_UNKNOWN);
	}
	break;

    case ST_TEST_2:
	if (sess->test1_same_ip) {
	    if (status == PJ_SUCCESS) {
		end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_OPEN);
	    } else if (status == PJNATH_ESTUNTIMEDOUT) {
		end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_SYMMETRIC_UDP);
	    } else {
		end_session(sess, status, PJ_STUN_NAT_TYPE_UNKNOWN);
	    }
	} else {
	    if (status == PJ_SUCCESS) {
		end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_FULL_CONE);
	    } else if (status == PJNATH_ESTUNTIMEDOUT) {
		if (sess->test1_ca.sin_family == 0) {
		    PJ_LOG(4,(sess->pool->obj_name, 
			      "CHANGED-ADDRESS attribute not present in "
			      "Binding response, unable to continue test"));
		    end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_UNKNOWN);
		} else {
		    /* Execute TEST_1B */
		    start_test(sess, ST_TEST_1B, &sess->test1_ca, 0);
		}
	    } else {
		end_session(sess, status, PJ_STUN_NAT_TYPE_UNKNOWN);
	    }
	}
	break;

    case ST_TEST_1B:
	if (status == PJ_SUCCESS) {
	    int cmp;

	    /* Compare MAPPED-ADDRESS with the one from TEST_1 */
	    cmp = pj_memcmp(&mattr->sockaddr, &sess->test1_ma, 
			    sizeof(pj_sockaddr_in));

	    if (cmp!=0) {
		/* Different address, this is symmetric NAT */
		end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_SYMMETRIC);
	    } else {
		/* Same address. Check if this is port restricted.
		 * Execute TEST_3
		 */
		start_test(sess, ST_TEST_3, NULL, CHANGE_PORT);
	    }
	} else {
	    end_session(sess, status, PJ_STUN_NAT_TYPE_UNKNOWN);
	}
	break;

    case ST_TEST_3:
	if (status == PJ_SUCCESS) {
	    end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_RESTRICTED);
	} else if (status == PJNATH_ESTUNTIMEDOUT) {
	    end_session(sess, PJ_SUCCESS, PJ_STUN_NAT_TYPE_PORT_RESTRICTED);
	} else {
	    end_session(sess, status, PJ_STUN_NAT_TYPE_UNKNOWN);
	}
	break;
    }

    pj_mutex_unlock(sess->mutex);
}


static void on_timer_destroy(pj_timer_heap_t *th,
			     pj_timer_entry *te)
{
    nat_detect_session *sess;

    PJ_UNUSED_ARG(th);

    sess = (nat_detect_session*) te->user_data;

    pj_mutex_lock(sess->mutex);
    pj_ioqueue_unregister(sess->key);
    sess->key = NULL;
    pj_mutex_unlock(sess->mutex);

    sess_destroy(sess);
}

