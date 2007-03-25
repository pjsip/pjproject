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
			  unsigned comp_id, 
			  void *pkt, pj_size_t size,
			  const pj_sockaddr_t *src_addr,
			  unsigned src_addr_len);
    void    (*on_ice_complete)(pj_ice_st *ice_st, 
			       pj_status_t status);

} pj_ice_st_cb;


#ifndef PJ_ICE_ST_MAX_ALIASES
#   define PJ_ICE_ST_MAX_ALIASES	8
#endif

enum pj_ice_st_option
{
    PJ_ICE_ST_OPT_DISABLE_STUN	= 1,
    PJ_ICE_ST_OPT_DISABLE_RELAY	= 2,
    PJ_ICE_ST_OPT_NO_PORT_RETRY	= 4,
};


typedef struct pj_ice_st_cand
{
    pj_ice_cand_type	type;
    pj_status_t		status;
    pj_sockaddr		addr;
    int			cand_id;
    pj_uint16_t		local_pref;
    pj_str_t		foundation;
} pj_ice_st_cand;


typedef struct pj_ice_st_comp
{
    pj_ice_st		*ice_st;
    unsigned		 comp_id;
    pj_uint32_t		 options;
    pj_sock_t		 sock;

    pj_stun_session	*stun_sess;

    pj_sockaddr		 local_addr;

    unsigned		 pending_cnt;
    pj_status_t		 last_status;

    unsigned		 cand_cnt;
    pj_ice_st_cand	 cand_list[PJ_ICE_ST_MAX_ALIASES];
    int			 default_cand;

    pj_ioqueue_key_t	*key;
    pj_uint8_t		 pkt[1500];
    pj_ioqueue_op_key_t	 read_op;
    pj_ioqueue_op_key_t	 write_op;
    pj_sockaddr		 src_addr;
    int			 src_addr_len;

} pj_ice_st_comp;


struct pj_ice_st
{
    char		     obj_name[PJ_MAX_OBJ_NAME];
    pj_pool_t		    *pool;
    void		    *user_data;
    pj_stun_config	     stun_cfg;
    pj_ice_st_cb	     cb;

    pj_ice		    *ice;

    unsigned		     comp_cnt;
    pj_ice_st_comp	   **comp;

    pj_dns_resolver	    *resolver;
    pj_bool_t		     has_resolver_job;
    pj_sockaddr_in	     stun_srv;
    pj_sockaddr_in	     turn_srv;
};


PJ_DECL(pj_status_t) pj_ice_st_create(pj_stun_config *stun_cfg,
				      const char *name,
				      unsigned comp_cnt,
				      void *user_data,
				      const pj_ice_st_cb *cb,
				      pj_ice_st **p_ice_st);
PJ_DECL(pj_status_t) pj_ice_st_destroy(pj_ice_st *ice_st);

PJ_DECL(pj_status_t) pj_ice_st_set_stun_domain(pj_ice_st *ice_st,
					       pj_dns_resolver *resolver,
					       const pj_str_t *domain);
PJ_DECL(pj_status_t) pj_ice_st_set_stun_srv(pj_ice_st *ice_st,
					    const pj_sockaddr_in *stun_srv,
					    const pj_sockaddr_in *turn_srv);

PJ_DECL(pj_status_t) pj_ice_st_create_comp(pj_ice_st *ice_st,
					   unsigned comp_id,
					   pj_uint32_t options,
					   const pj_sockaddr_in *addr,
				    	   unsigned *p_itf_id);

PJ_DECL(pj_status_t) pj_ice_st_get_comps_status(pj_ice_st *ice_st);

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

PJ_DECL(pj_status_t) pj_ice_st_sendto(pj_ice_st *ice_st,
				      unsigned comp_id,
				      const void *data,
				      pj_size_t data_len,
				      const pj_sockaddr_t *dst_addr,
				      int dst_addr_len);


/**
 * @}
 */


PJ_END_DECL



#endif	/* __PJNATH_ICE_STREAM_TRANSPORT_H__ */

