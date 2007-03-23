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
#ifndef __PJNATH_ICE_STREAM_TRANSPORT_H__
#define __PJNATH_ICE_STREAM_TRANSPORT_H__


/**
 * @file ice_mt.h
 * @brief ICE Media Transport.
 */
#include <pjnath/ice.h>
#include <pjlib-util/resolver.h>
#include <pj/ioqueue.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJNATH_ICE_STREAM_TRANSPORT ICE Stream Transport
 * @brief Transport for media stream using ICE
 * @ingroup PJNATH_ICE
 * @{
 */

typedef struct pj_ice_st pj_ice_st;

typedef struct pj_ice_st_cb
{
    void    (*on_rx_data)(pj_ice_st *ice_st,
			  unsigned comp_id, unsigned cand_id,
			  void *pkt, pj_size_t size,
			  const pj_sockaddr_t *src_addr,
			  unsigned src_addr_len);
    void    (*on_ice_complete)(pj_ice_st *ice_st, 
			       pj_status_t status);

} pj_ice_st_cb;


typedef struct pj_ice_st_comp
{
    unsigned		 comp_id;
} pj_ice_st_comp;


typedef struct pj_ice_st_interface
{
    pj_ice_st		*ice_st;
    pj_ice_cand_type	 type;
    pj_status_t		 status;
    unsigned		 comp_id;
    int			 cand_id;
    pj_str_t		 foundation;
    pj_uint16_t		 local_pref;
    pj_sock_t		 sock;
    pj_sockaddr		 addr;
    pj_sockaddr		 base_addr;
    pj_ioqueue_key_t	*key;
    pj_uint8_t		 pkt[1500];
    pj_ioqueue_op_key_t	 read_op;
    pj_ioqueue_op_key_t	 write_op;
    pj_sockaddr		 src_addr;
    int			 src_addr_len;
    pj_stun_session	*stun_sess;
} pj_ice_st_interface;


struct pj_ice_st
{
    char		     obj_name[PJ_MAX_OBJ_NAME];
    pj_pool_t		    *pool;
    void		    *user_data;
    pj_stun_config	     stun_cfg;
    pj_ice_st_cb	     cb;

    pj_ice		    *ice;

    unsigned		     comp_cnt;
    unsigned		     comps[PJ_ICE_MAX_COMP];

    unsigned		     itf_cnt;
    pj_ice_st_interface	    *itfs[PJ_ICE_MAX_CAND];

    pj_dns_resolver	    *resolver;
    pj_bool_t		     relay_enabled;
    pj_str_t		     stun_domain;
    pj_sockaddr_in	     stun_srv;
};


PJ_DECL(pj_status_t) pj_ice_st_create(pj_stun_config *stun_cfg,
				      const char *name,
				      void *user_data,
				      const pj_ice_st_cb *cb,
				      pj_ice_st **p_ice_st);
PJ_DECL(pj_status_t) pj_ice_st_destroy(pj_ice_st *ice_st);

PJ_DECL(pj_status_t) pj_ice_st_set_stun(pj_ice_st *ice_st,
					pj_dns_resolver *resolver,
					pj_bool_t enable_relay,
					const pj_str_t *domain);
PJ_DECL(pj_status_t) pj_ice_st_set_stun_addr(pj_ice_st *ice_st,
					     pj_bool_t enable_relay,
					     const pj_sockaddr_in *srv_addr);

PJ_DECL(pj_status_t) pj_ice_st_add_comp(pj_ice_st *ice_st,
					unsigned comp_id);

PJ_DECL(pj_status_t) pj_ice_st_add_host_interface(pj_ice_st *ice_st,
						  unsigned comp_id,
						  pj_uint16_t local_pref,
					          const pj_sockaddr_in *addr,
				    		  unsigned *p_itf_id);
PJ_DECL(pj_status_t) pj_ice_st_add_all_host_interfaces(pj_ice_st *ice_st,
						       unsigned comp_id,
						       unsigned port);
PJ_DECL(pj_status_t) pj_ice_st_add_stun_interface(pj_ice_st *ice_st,
						  unsigned comp_id,
						  unsigned local_port,
						  unsigned *p_itf_id);
PJ_DECL(pj_status_t) pj_ice_st_add_relay_interface(pj_ice_st *ice_st,
						   unsigned comp_id,
						   unsigned local_port,
						   pj_bool_t notify,
						   void *notify_data);
PJ_DECL(pj_status_t) pj_ice_st_get_interfaces_status(pj_ice_st *ice_st);

PJ_DECL(pj_status_t) pj_ice_st_init_ice(pj_ice_st *ice_st,
					pj_ice_role role,
					const pj_str_t *local_ufrag,
					const pj_str_t *local_passwd);
PJ_DECL(pj_status_t) pj_ice_st_enum_cands(pj_ice_st *ice_st,
					  unsigned *count,
					  pj_ice_cand cand[]);
PJ_DECL(pj_status_t) pj_ice_st_start_ice(pj_ice_st *ice_st,
					 const pj_str_t *rem_ufrag,
					 const pj_str_t *rem_passwd,
					 unsigned rem_cand_cnt,
					 const pj_ice_cand rem_cand[]);
PJ_DECL(pj_status_t) pj_ice_st_stop_ice(pj_ice_st *ice_st);

PJ_DECL(pj_status_t) pj_ice_st_send_data(pj_ice_st *ice_st,
					 unsigned comp_id,
					 const void *data,
					 pj_size_t data_len);
PJ_DECL(pj_status_t) pj_ice_st_sendto(pj_ice_st *ice_st,
				      unsigned comp_id,
				      unsigned itf_id,
				      const void *data,
				      pj_size_t data_len,
				      const pj_sockaddr_t *dst_addr,
				      int dst_addr_len);


/**
 * @}
 */


PJ_END_DECL



#endif	/* __PJNATH_ICE_STREAM_TRANSPORT_H__ */

