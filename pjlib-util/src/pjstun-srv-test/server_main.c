/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#include <pjlib-util.h>
#include <pjlib.h>

#include <stdio.h>
#include <conio.h>


#define THIS_FILE	"server_main.c"
#define MAX_THREADS	8
#define MAX_SERVICE	16
#define MAX_PKT_LEN	512

struct service
{
    unsigned		 index;
    pj_uint16_t		 port;
    pj_bool_t		 is_stream;
    pj_sock_t		 sock;
    pj_ioqueue_key_t	*key;
    pj_ioqueue_op_key_t  recv_opkey,
			 send_opkey;

    pj_stun_session	*sess;

    int		         src_addr_len;
    pj_sockaddr_in	 src_addr;
    pj_ssize_t		 rx_pkt_len;
    pj_uint8_t		 rx_pkt[MAX_PKT_LEN];
    pj_uint8_t		 tx_pkt[MAX_PKT_LEN];
};

static struct stun_server
{
    pj_caching_pool	 cp;
    pj_pool_t		*pool;
    pj_stun_endpoint	*endpt;
    pj_ioqueue_t	*ioqueue;
    pj_timer_heap_t	*timer_heap;
    unsigned		 service_cnt;
    struct service	 services[MAX_SERVICE];

    pj_bool_t		 thread_quit_flag;
    unsigned		 thread_cnt;
    pj_thread_t		*threads[16];

} server;


static pj_status_t server_perror(const char *sender, const char *title, 
				 pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(3,(sender, "%s: %s", title, errmsg));

    return status;
}


/* Callback to be called to send outgoing message */
static pj_status_t on_send_msg(pj_stun_session *sess,
			       const void *pkt,
			       pj_size_t pkt_size,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len)
{
    struct service *svc;
    pj_ssize_t length;
    pj_status_t status;

    svc = (struct service*) pj_stun_session_get_user_data(sess);

    /* Send packet */
    length = pkt_size;
    if (svc->is_stream) {
	status = pj_ioqueue_send(svc->key, &svc->send_opkey, pkt, &length, 0);
    } else {
#if 0
	pj_pool_t *pool;
	char *buf;
	pj_stun_msg *msg;

	pool = pj_pool_create(&server.cp.factory, "", 4000, 4000, NULL);
	status = pj_stun_msg_decode(pool, pkt, pkt_size, PJ_STUN_CHECK_PACKET, &msg, NULL, NULL);
	buf = pj_pool_alloc(pool, 512);
	PJ_LOG(3,("", "%s", pj_stun_msg_dump(msg, buf, 512, NULL)));
#endif
	status = pj_ioqueue_sendto(svc->key, &svc->send_opkey, pkt, &length, 
				   0, dst_addr, addr_len);
    }

    return (status == PJ_SUCCESS || status == PJ_EPENDING) ? 
	    PJ_SUCCESS : status;
}


/* Handle STUN binding request */
static pj_status_t on_rx_binding_request(pj_stun_session *sess,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len)
{
    struct service *svc = (struct service *) pj_stun_session_get_user_data(sess);
    pj_stun_tx_data *tdata;
    pj_status_t status;

    /* Create response */
    status = pj_stun_session_create_response(sess, msg, 0, NULL, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Create MAPPED-ADDRESS attribute */
    status = pj_stun_msg_add_generic_ip_addr_attr(tdata->pool, tdata->msg,
						  PJ_STUN_ATTR_MAPPED_ADDR,
						  PJ_FALSE,
					          src_addr, src_addr_len);
    if (status != PJ_SUCCESS) {
	server_perror(THIS_FILE, "Error creating response", status);
	pj_stun_msg_destroy_tdata(sess, tdata);
	return status;
    }

    /* On the presence of magic, create XOR-MAPPED-ADDRESS attribute */
    if (msg->hdr.magic == PJ_STUN_MAGIC) {
	status = 
	    pj_stun_msg_add_generic_ip_addr_attr(tdata->pool, tdata->msg,
						 PJ_STUN_ATTR_XOR_MAPPED_ADDRESS,
						 PJ_TRUE,
						 src_addr, src_addr_len);
	if (status != PJ_SUCCESS) {
	    server_perror(THIS_FILE, "Error creating response", status);
	    pj_stun_msg_destroy_tdata(sess, tdata);
	    return status;
	}
    }

    /* Send */
    status = pj_stun_session_send_msg(sess, PJ_STUN_CACHE_RESPONSE, 
				      src_addr, src_addr_len, tdata);
    return status;
}


/* Handle unknown request */
static pj_status_t on_rx_unknown_request(pj_stun_session *sess,
					 const pj_uint8_t *pkt,
					 unsigned pkt_len,
					 const pj_stun_msg *msg,
					 const pj_sockaddr_t *src_addr,
					 unsigned src_addr_len)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    /* Create response */
    status = pj_stun_session_create_response(sess, msg, 
					     PJ_STUN_STATUS_BAD_REQUEST,
					     NULL, &tdata);
    if (status != PJ_SUCCESS)
	return status;

    /* Send */
    status = pj_stun_session_send_msg(sess, 0, src_addr, src_addr_len, tdata);
    return status;
}

/* Callback to be called by STUN session on incoming STUN requests */
static pj_status_t on_rx_request(pj_stun_session *sess,
				 const pj_uint8_t *pkt,
				 unsigned pkt_len,
				 const pj_stun_msg *msg,
				 const pj_sockaddr_t *src_addr,
				 unsigned src_addr_len)
{
    switch (PJ_STUN_GET_METHOD(msg->hdr.type)) {
    case PJ_STUN_BINDING_METHOD:
	return on_rx_binding_request(sess, pkt, pkt_len, msg, 
				     src_addr, src_addr_len);
    default:
	return on_rx_unknown_request(sess, pkt, pkt_len, msg,
				     src_addr, src_addr_len);
    }
}


/* Callback on ioqueue read completion */
static void on_read_complete(pj_ioqueue_key_t *key, 
			     pj_ioqueue_op_key_t *op_key, 
			     pj_ssize_t bytes_read)
{
    struct service *svc = (struct service *) pj_ioqueue_get_user_data(key);
    pj_status_t status;

    if (bytes_read <= 0)
	goto next_read;

    /* Handle packet to session */
    status = pj_stun_session_on_rx_pkt(svc->sess, svc->rx_pkt, bytes_read,
				       PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET,
				       NULL, &svc->src_addr, svc->src_addr_len);
    if (status != PJ_SUCCESS) {
	server_perror(THIS_FILE, "Error processing incoming packet", status);
    }

next_read:
    if (bytes_read < 0) {
	server_perror(THIS_FILE, "on_read_complete()", -bytes_read);
    }

    svc->rx_pkt_len = sizeof(svc->rx_pkt);
    svc->src_addr_len = sizeof(svc->src_addr);

    status = pj_ioqueue_recvfrom(svc->key, &svc->recv_opkey, 
				 svc->rx_pkt, &svc->rx_pkt_len, 
				 PJ_IOQUEUE_ALWAYS_ASYNC,
				 &svc->src_addr, &svc->src_addr_len);
    if (status != PJ_EPENDING)
	server_perror(THIS_FILE, "error starting async read", status);
}


static pj_status_t init_service(struct service *svc)
{
    pj_status_t status;
    pj_ioqueue_callback service_callback;
    pj_stun_session_cb sess_cb;
    pj_sockaddr_in addr;

    status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &svc->sock);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_sockaddr_in_init(&addr, NULL, svc->port);
    if (status != PJ_SUCCESS)
	goto on_error;
    
    status = pj_sock_bind(svc->sock, &addr, sizeof(addr));
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_send_msg = &on_send_msg;
    sess_cb.on_rx_request = &on_rx_request;
    status = pj_stun_session_create(server.endpt, "session", 
				    &sess_cb, &svc->sess);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_stun_session_set_user_data(svc->sess, (void*)svc);

    pj_bzero(&service_callback, sizeof(service_callback));
    service_callback.on_read_complete = &on_read_complete;

    status = pj_ioqueue_register_sock(server.pool, server.ioqueue, svc->sock,
				      svc, &service_callback, &svc->key);
    if (status != PJ_SUCCESS)
	goto on_error;


    pj_ioqueue_op_key_init(&svc->recv_opkey, sizeof(svc->recv_opkey));
    pj_ioqueue_op_key_init(&svc->send_opkey, sizeof(svc->send_opkey));

    on_read_complete(svc->key, &svc->recv_opkey, 0);

    PJ_LOG(4,(THIS_FILE, "Service started on port %d", svc->port));
    return PJ_SUCCESS;

on_error:
    if (svc->key != NULL) {
	pj_ioqueue_unregister(svc->key);
	svc->key = NULL;
	svc->sock = PJ_INVALID_SOCKET;
    } else if (svc->sock != 0 && svc->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(svc->sock);
	svc->sock = PJ_INVALID_SOCKET;
    }

    return status;
}


static int worker_thread(void *p)
{
    PJ_UNUSED_ARG(p);

    while (!server.thread_quit_flag) {
	pj_time_val timeout = { 0, 50 };
	pj_timer_heap_poll(server.timer_heap, NULL);
	pj_ioqueue_poll(server.ioqueue, &timeout);
    }

    return 0;
}


pj_status_t server_init(void)
{
    pj_status_t status;

    status = pj_init();
    if (status != PJ_SUCCESS)
	return server_perror(THIS_FILE, "pj_init() error", status);

    status = pjlib_util_init();
    if (status != PJ_SUCCESS)
	return server_perror(THIS_FILE, "pjlib_util_init() error", status);

    pj_caching_pool_init(&server.cp, 
			 &pj_pool_factory_default_policy, 0);


    server.pool = pj_pool_create(&server.cp.factory, "server", 4000, 4000,
				 NULL);

    status = pj_ioqueue_create(server.pool, PJ_IOQUEUE_MAX_HANDLES, 
			       &server.ioqueue);
    if (status != PJ_SUCCESS)
	return server_perror(THIS_FILE, "pj_ioqueue_create()", status);

    status = pj_timer_heap_create(server.pool, 1024, &server.timer_heap);
    if (status != PJ_SUCCESS)
	return server_perror(THIS_FILE, "Error creating timer heap", status);

    status = pj_stun_endpoint_create(&server.cp.factory, 0, 
				     server.ioqueue, server.timer_heap, 
				     &server.endpt);
    if (status != PJ_SUCCESS)
	return server_perror(THIS_FILE, "Error creating endpoint", status);

    server.service_cnt = 1;
    server.services[0].index = 0;
    server.services[0].port = PJ_STUN_PORT;

    status = init_service(&server.services[0]);
    if (status != PJ_SUCCESS)
	return server_perror(THIS_FILE, "init_service() error", status);

    return PJ_SUCCESS;
}


pj_status_t server_main(void)
{
#if 0
    for (;;) {
	pj_time_val timeout = { 0, 50 };
	pj_timer_heap_poll(server.timer_heap, NULL);
	pj_ioqueue_poll(server.ioqueue, &timeout);

	if (kbhit() && _getch()==27)
	    break;
    }
#else
    pj_status_t status;

    status = pj_thread_create(server.pool, "stun_server", &worker_thread, NULL,
			      0, 0, &server.threads[0]);
    if (status != PJ_SUCCESS)
	return server_perror(THIS_FILE, "create_thread() error", status);

    while (!server.thread_quit_flag) {
	char line[10];

	printf("Menu:\n"
	       "  q     Quit\n"
	       "Choice:");

	fgets(line, sizeof(line), stdin);
	if (line[0] == 'q')
	    server.thread_quit_flag = 1;
    }

#endif

    return PJ_SUCCESS;
}


pj_status_t server_destroy(void)
{
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(server.services); ++i) {
	struct service *svc = &server.services[i];

	if (svc->key != NULL) {
	    pj_ioqueue_unregister(svc->key);
	    svc->key = NULL;
	    svc->sock = PJ_INVALID_SOCKET;
	} else if (svc->sock != 0 && svc->sock != PJ_INVALID_SOCKET) {
	    pj_sock_close(svc->sock);
	    svc->sock = PJ_INVALID_SOCKET;
	}
    }

    server.thread_quit_flag = PJ_TRUE;
    for (i=0; i<PJ_ARRAY_SIZE(server.threads); ++i) {
	if (server.threads[i]) {
	    pj_thread_join(server.threads[i]);
	    pj_thread_destroy(server.threads[i]);
	    server.threads[i] = NULL;
	}
    }

    pj_stun_session_destroy(server.services[0].sess);
    pj_stun_endpoint_destroy(server.endpt);
    pj_ioqueue_destroy(server.ioqueue);
    pj_pool_release(server.pool);

    pj_pool_factory_dump(&server.cp.factory, PJ_TRUE);
    pj_caching_pool_destroy(&server.cp);

    pj_shutdown();

    return PJ_SUCCESS;
}


int main()
{
    if (server_init()) {
	server_destroy();
	return 1;
    }

    server_main();
    server_destroy();

    return 0;
}
