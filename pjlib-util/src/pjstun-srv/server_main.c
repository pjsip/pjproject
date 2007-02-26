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
#include "server.h"

#include <stdio.h>
#include <conio.h>


#define THIS_FILE	"server_main.c"
#define MAX_THREADS	8

struct stun_server_tag server;


pj_status_t server_perror(const char *sender, const char *title, 
			  pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(3,(sender, "%s: %s", title, errmsg));

    return status;
}


static pj_status_t create_response(pj_pool_t *pool,
				   const pj_stun_msg *req_msg,
				   unsigned err_code,
				   unsigned uattr_cnt,
				   pj_uint16_t uattr_types[],
				   pj_stun_msg **p_response)
{
    pj_uint32_t msg_type = req_msg->hdr.type;
    pj_stun_msg *response;
    pj_status_t status;

    status = pj_stun_msg_create_response(pool, req_msg, err_code, NULL,
					 &response);
    if (status != PJ_SUCCESS)
	return status;

    /* Add unknown_attribute attributes if err_code is 420 */
    if (err_code == PJ_STUN_STATUS_UNKNOWN_ATTRIBUTE) {
	pj_stun_unknown_attr *uattr;

	status = pj_stun_unknown_attr_create(pool, uattr_cnt, uattr_types,
					     &uattr);
	if (status != PJ_SUCCESS)
	    return status;

	pj_stun_msg_add_attr(response, &uattr->hdr);
    }

    *p_response = response;
    return PJ_SUCCESS;
}


static pj_status_t send_msg(struct service *svc, const pj_stun_msg *msg)
{
    unsigned tx_pkt_len;
    pj_ssize_t length;
    pj_status_t status;

    /* Print to log */
    PJ_LOG(4,(THIS_FILE, "TX STUN message: \n"
			 "--- begin STUN message ---\n"
			 "%s"
			 "--- end of STUN message ---\n", 
	      pj_stun_msg_dump(msg, svc->tx_pkt, sizeof(svc->tx_pkt), NULL)));

    /* Encode packet */
    tx_pkt_len = sizeof(svc->tx_pkt);
    status = pj_stun_msg_encode(msg, svc->tx_pkt, tx_pkt_len, 0,
				NULL, &tx_pkt_len);
    if (status != PJ_SUCCESS)
	return status;

    length = tx_pkt_len;

    /* Send packet */
    if (svc->is_stream) {
	status = pj_ioqueue_send(svc->key, &svc->send_opkey, svc->tx_pkt,
				 &length, 0);
    } else {
	status = pj_ioqueue_sendto(svc->key, &svc->send_opkey, svc->tx_pkt,
				   &length, 0, &svc->src_addr, 
				   svc->src_addr_len);
    }

    PJ_LOG(4,(THIS_FILE, "Sending STUN %s %s",
	      pj_stun_get_method_name(msg->hdr.type),
	      pj_stun_get_class_name(msg->hdr.type)));

    return (status == PJ_SUCCESS || status == PJ_EPENDING) ? 
	    PJ_SUCCESS : status;
}


static pj_status_t err_respond(struct service *svc,
			       pj_pool_t *pool,
			       const pj_stun_msg *req_msg,
			       unsigned err_code,
			       unsigned uattr_cnt,
			       pj_uint16_t uattr_types[])
{
    pj_stun_msg *response;
    pj_status_t status;

    /* Create the error response */
    status = create_response(pool, req_msg, err_code,
			     uattr_cnt, uattr_types, &response);
    if (status != PJ_SUCCESS) {
	server_perror(THIS_FILE, "Error creating response", status);
	return status;
    }

    /* Send response */
    status = send_msg(svc, response);
    if (status != PJ_SUCCESS) {
	server_perror(THIS_FILE, "Error sending response", status);
	return status;
    }

    return PJ_SUCCESS;
}


static void handle_binding_request(struct service *svc, pj_pool_t *pool, 
				   const pj_stun_msg *rx_msg)
{
    pj_stun_msg *response;
    pj_stun_generic_ip_addr_attr *m_attr;
    pj_status_t status;

    status = create_response(pool, rx_msg, 0, 0, NULL, &response);
    if (status != PJ_SUCCESS) {
	server_perror(THIS_FILE, "Error creating response", status);
	return;
    }

    /* Create MAPPED-ADDRESS attribute */
    status = pj_stun_generic_ip_addr_attr_create(pool,
						 PJ_STUN_ATTR_MAPPED_ADDR,
					         PJ_FALSE,
					         svc->src_addr_len,
					         &svc->src_addr, &m_attr);
    if (status != PJ_SUCCESS) {
	server_perror(THIS_FILE, "Error creating response", status);
	return;
    }
    pj_stun_msg_add_attr(response, &m_attr->hdr);

    /* On the presence of magic, create XOR-MAPPED-ADDRESS attribute */
    if (rx_msg->hdr.magic == PJ_STUN_MAGIC) {
	status = 
	    pj_stun_generic_ip_addr_attr_create(pool, 
						PJ_STUN_ATTR_XOR_MAPPED_ADDRESS,
						PJ_TRUE,
						svc->src_addr_len,
						&svc->src_addr, &m_attr);
	if (status != PJ_SUCCESS) {
	    server_perror(THIS_FILE, "Error creating response", status);
	    return;
	}
    }

    /* Send */
    status = send_msg(svc, response);
    if (status != PJ_SUCCESS)
	server_perror(THIS_FILE, "Error sending response", status);
}


static void handle_unknown_request(struct service *svc, pj_pool_t *pool, 
				   pj_stun_msg *rx_msg)
{
    err_respond(svc, pool, rx_msg, PJ_STUN_STATUS_BAD_REQUEST, 0, NULL);
}


static void on_read_complete(pj_ioqueue_key_t *key, 
			     pj_ioqueue_op_key_t *op_key, 
			     pj_ssize_t bytes_read)
{
    struct service *svc = (struct service *) pj_ioqueue_get_user_data(key);
    pj_pool_t *pool = NULL;
    pj_stun_msg *rx_msg, *response;
    char dump[512];
    pj_status_t status;

    if (bytes_read <= 0)
	goto next_read;

    pool = pj_pool_create(&server.cp.factory, "service", 4000, 4000, NULL);

    rx_msg = NULL;
    status = pj_stun_msg_decode(pool, svc->rx_pkt, bytes_read, 0, &rx_msg,
				NULL, &response);
    if (status != PJ_SUCCESS) {
	server_perror(THIS_FILE, "STUN msg_decode() error", status);
	if (response) {
	    send_msg(svc, response);
	}
	goto next_read;
    }

    PJ_LOG(4,(THIS_FILE, "RX STUN message: \n"
			 "--- begin STUN message ---\n"
			 "%s"
			 "--- end of STUN message ---\n", 
	      pj_stun_msg_dump(rx_msg, dump, sizeof(dump), NULL)));

    if (PJ_STUN_IS_REQUEST(rx_msg->hdr.type)) {
	switch (rx_msg->hdr.type) {
	case PJ_STUN_BINDING_REQUEST:
	    handle_binding_request(svc, pool, rx_msg);
	    break;
	default:
	    handle_unknown_request(svc, pool, rx_msg);
	}
	
    }

next_read:
    if (pool != NULL)
	pj_pool_release(pool);

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
#if 1
    for (;;) {
	pj_time_val timeout = { 0, 50 };
	pj_ioqueue_poll(server.ioqueue, &timeout);

	if (kbhit() && _getch()==27)
	    break;
    }
#else
    pj_status_t status;
    char line[10];

    status = pj_thread_create(server.pool, "stun_server", &worker_thread, NULL,
			      0, 0, &server.threads[0]);
    if (status != PJ_SUCCESS)
	return server_perror(THIS_FILE, "create_thread() error", status);

    puts("Press ENTER to quit");
    fgets(line, sizeof(line), stdin);

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

    pj_ioqueue_destroy(server.ioqueue);
    pj_pool_release(server.pool);
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
