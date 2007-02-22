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
#ifndef __PJLIB_UTIL_STUN_SESSION_H__
#define __PJLIB_UTIL_STUN_SESSION_H__

#include <pjlib-util/stun_msg.h>
#include <pjlib-util/stun_endpoint.h>
#include <pjlib-util/stun_transaction.h>
#include <pj/list.h>


typedef struct pj_stun_tx_data pj_stun_tx_data;
typedef struct pj_stun_session pj_stun_session;

typedef struct pj_stun_session_cb
{
    pj_status_t (*on_send_msg)(pj_stun_tx_data *tdata,
			       unsigned addr_len, 
			       const pj_sockaddr_t *dst_addr);

    void (*on_bind_response)(void *user_data, pj_status_t status, pj_stun_msg *response);
    void (*on_allocate_response)(void *user_data, pj_status_t status, pj_stun_msg *response);
    void (*on_set_active_destination_response)(void *user_data, pj_status_t status, pj_stun_msg *response);
    void (*on_connect_response)(void *user_data, pj_status_t status, pj_stun_msg *response);
} pj_stun_session_cb;


struct pj_stun_tx_data
{
    PJ_DECL_LIST_MEMBER(struct pj_stun_tx_data);

    pj_pool_t		*pool;
    pj_stun_session	*sess;
    pj_stun_msg		*msg;
    void		*user_data;

    pj_stun_client_tsx	*client_tsx;
    pj_uint8_t		 client_key[12];
};


PJ_DECL(pj_status_t) pj_stun_session_create(pj_stun_endpoint *endpt,
					    const char *name,
					    const pj_stun_session_cb *cb,
					    pj_stun_session **p_sess);

PJ_DECL(pj_status_t) pj_stun_session_destroy(pj_stun_session *sess);

PJ_DECL(pj_status_t) pj_stun_session_set_user_data(pj_stun_session *sess,
						   void *user_data);

PJ_DECL(void*) pj_stun_session_get_user_data(pj_stun_session *sess);

PJ_DECL(pj_status_t) pj_stun_session_set_credential(pj_stun_session *sess,
						    const pj_str_t *realm,
						    const pj_str_t *user,
						    const pj_str_t *passwd);

PJ_DECL(pj_status_t) pj_stun_session_enable_fingerprint(pj_stun_session *sess,
							pj_bool_t enabled);

PJ_DECL(pj_status_t) pj_stun_session_create_bind_req(pj_stun_session *sess,
						     pj_stun_tx_data **p_tdata);

PJ_DECL(pj_status_t) pj_stun_session_create_allocate_req(pj_stun_session *sess,
							 pj_stun_tx_data **p_tdata);

PJ_DECL(pj_status_t) 
pj_stun_session_create_set_active_destination_req(pj_stun_session *sess,
						  pj_stun_tx_data **p_tdata);

PJ_DECL(pj_status_t) pj_stun_session_create_connect_req(pj_stun_session *sess,
							pj_stun_tx_data **p_tdata);

PJ_DECL(pj_status_t) 
pj_stun_session_create_connection_status_ind(pj_stun_session *sess,
					     pj_stun_tx_data **p_tdata);

PJ_DECL(pj_status_t) pj_stun_session_create_send_ind(pj_stun_session *sess,
						     pj_stun_tx_data **p_tdata);

PJ_DECL(pj_status_t) pj_stun_session_create_data_ind(pj_stun_session *sess,
						     pj_stun_tx_data **p_tdata);

PJ_DECL(pj_status_t) pj_stun_session_send_msg(pj_stun_session *sess,
					      unsigned addr_len,
					      const pj_sockaddr_t *server,
					      pj_stun_tx_data *tdata);

PJ_DECL(pj_status_t) pj_stun_session_on_rx_pkt(pj_stun_session *sess,
					       const void *packet,
					       pj_size_t pkt_size,
					       unsigned *parsed_len);



#endif	/* __PJLIB_UTIL_STUN_SESSION_H__ */

