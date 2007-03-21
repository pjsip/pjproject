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
#ifndef __STUN_SERVER_H__
#define __STUN_SERVER_H__

#include <pjnath.h>
#include <pjlib-util.h>
#include <pjlib.h>


/** Opaque declaration for STUN server instance */
typedef struct pj_stun_server pj_stun_server;

/** STUN server info */
typedef struct pj_stun_server_info
{
    pj_pool_factory	*pf;
    pj_stun_config	 stun_cfg;
    pj_ioqueue_t	*ioqueue;
    pj_timer_heap_t	*timer_heap;
    unsigned		 thread_cnt;
} pj_stun_server_info;

/** STUN usage */
typedef struct pj_stun_usage pj_stun_usage;

/** STUN usage callback */
typedef struct pj_stun_usage_cb
{
    void (*on_rx_data)(pj_stun_usage *usage,
		       void *pkt,
		       pj_size_t pkt_size,
		       const pj_sockaddr_t *src_addr,
		       unsigned src_addr_len);
    void (*on_destroy)(pj_stun_usage *usage);
} pj_stun_usage_cb;


PJ_DECL(pj_status_t) pj_stun_perror(const char *sender, 
				    const char *title, 
				    pj_status_t status);

/**
 * Create instance of STUN server.
 */
PJ_DECL(pj_status_t) pj_stun_server_create(pj_pool_factory *pf,
					   unsigned thread_cnt,
					   pj_stun_server **p_srv);

/**
 * Get STUN server info.
 */
PJ_DECL(pj_stun_server_info*) pj_stun_server_get_info(pj_stun_server *srv);


/**
 * Destroy STUN server.
 */
PJ_DECL(pj_status_t) pj_stun_server_destroy(pj_stun_server *srv);


/**
 * Create STUN usage.
 */
PJ_DECL(pj_status_t) pj_stun_usage_create(pj_stun_server *srv,
					  const char *name,
					  const pj_stun_usage_cb *cb,
					  int family,
					  int type,
					  int protocol,
					  const pj_sockaddr_t *local_addr,
					  int addr_len,
					  pj_stun_usage **p_usage);

/**
 * Destroy usage.
 */
PJ_DECL(pj_status_t) pj_stun_usage_destroy(pj_stun_usage *usage);

/**
 * Set user data.
 */
PJ_DECL(pj_status_t) pj_stun_usage_set_user_data(pj_stun_usage *usage,
						 void *user_data);
/**
 * Get user data.
 */
PJ_DECL(void*) pj_stun_usage_get_user_data(pj_stun_usage *usage);

/**
 * Send with the usage.
 */
PJ_DECL(pj_status_t) pj_stun_usage_sendto(pj_stun_usage *usage,
					  const void *pkt,
					  pj_size_t pkt_size,
					  unsigned flags,
					  const pj_sockaddr_t *dst_addr,
					  unsigned addr_len);

PJ_DECL(pj_status_t) pj_stun_bind_usage_create(pj_stun_server *srv,
					       const pj_str_t *ip_addr,
					       unsigned port,
					       pj_stun_usage **p_bu);

PJ_DECL(pj_status_t) pj_stun_turn_usage_create(pj_stun_server *srv,
					       int type,
					       const pj_str_t *ip_addr,
					       unsigned port,
					       pj_stun_usage **p_bu);


pj_status_t pj_stun_server_register_usage(pj_stun_server *srv,
					  pj_stun_usage *usage);
pj_status_t pj_stun_server_unregister_usage(pj_stun_server *srv,
					    pj_stun_usage *usage);


#endif	/* __STUN_SERVER_H__ */


