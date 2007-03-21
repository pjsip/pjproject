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
#ifndef __PJNATH_ICE_SOCK_H__
#define __PJNATH_ICE_SOCK_H__

/**
 * @file ice_sock.h
 * @brief ICE socket.
 */
#include <pjnath/types.h>
#include <pjnath/stun_session.h>
#include <pjlib-util/resolver.h>
#include <pj/sock.h>
#include <pj/timer.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJNATH_ICE_SOCK ICE Socket
 * @brief High level ICE socket abstraction.
 * @ingroup PJNATH
 * @{
 */

/**
 * This enumeration describes the type of an ICE candidate.
 */
typedef enum pj_ice_cand_type
{
    PJ_ICE_CAND_TYPE_HOST,
    PJ_ICE_CAND_TYPE_MAPPED,
    PJ_ICE_CAND_TYPE_PEER_MAPPED,
    PJ_ICE_CAND_TYPE_RELAYED
} pj_ice_cand_type;

/**
 *
 */
enum pj_ice_type_pref
{
    PJ_ICE_HOST_PREF	    = 126,
    PJ_ICE_MAPPED_PREF	    = 100,
    PJ_ICE_PEER_MAPPED_PREF = 110,
    PJ_ICE_RELAYED_PREF	    = 0
};

typedef struct pj_ice pj_ice;


#define PJ_ICE_MAX_CAND	    16
#define PJ_ICE_MAX_COMP	    8
#define PJ_ICE_MAX_CHECKS   32
#define PJ_ICE_TA_VAL	    20

/**
 * ICE component
 */
typedef struct pj_ice_comp
{
    unsigned	     comp_id;
    pj_sock_t	     sock;
    pj_stun_session *stun_sess;
    pj_sockaddr	     local_addr;
} pj_ice_comp;


/**
 * This structure describes an ICE candidate.
 */
typedef struct pj_ice_cand
{
    pj_uint32_t		 comp_id;
    pj_ice_cand_type	 type;
    pj_str_t		 foundation;
    pj_uint32_t		 prio;
    pj_sockaddr		 addr;
    pj_sockaddr		 base_addr;
    pj_sockaddr		 srv_addr;
} pj_ice_cand;

typedef enum pj_ice_check_state
{
    PJ_ICE_CHECK_STATE_FROZEN,
    PJ_ICE_CHECK_STATE_WAITING,
    PJ_ICE_CHECK_STATE_IN_PROGRESS,
    PJ_ICE_CHECK_STATE_SUCCEEDED,
    PJ_ICE_CHECK_STATE_FAILED
} pj_ice_check_state;


typedef struct pj_ice_check
{
    pj_ice_cand		*lcand;
    pj_ice_cand		*rcand;

    pj_uint64_t		 prio;
    pj_ice_check_state	 state;
    pj_bool_t		 nominated;
    pj_status_t		 err_code;
} pj_ice_check;


typedef enum pj_ice_checklist_state
{
    PJ_ICE_CHECKLIST_ST_IDLE,
    PJ_ICE_CHECKLIST_ST_RUNNING,
    PJ_ICE_CHECKLIST_ST_COMPLETED
} pj_ice_checklist_state;

typedef struct pj_ice_checklist
{
    pj_ice_checklist_state   state;
    unsigned		     count;
    pj_ice_check	     checks[PJ_ICE_MAX_CHECKS];
    pj_timer_entry	     timer;
} pj_ice_checklist;


/**
 * ICE sock callback.
 */
typedef struct pj_ice_cb
{
    pj_status_t (*on_send_pkt)(pj_ice *ice, 
			       const void *pkt, pj_size_t size,
			       const pj_sockaddr_t *dst_addr,
			       unsigned addr_len);
} pj_ice_cb;


typedef enum pj_ice_role
{
    PJ_ICE_ROLE_CONTROLLED,
    PJ_ICE_ROLE_CONTROLLING
} pj_ice_role;

/**
 * ICE structure.
 */
struct pj_ice
{
    char		obj_name[PJ_MAX_OBJ_NAME];

    pj_pool_t		*pool;
    pj_mutex_t		*mutex;
    int			 af;
    int			 sock_type;
    pj_ice_role		 role;
    pj_ice_cb		 cb;

    pj_stun_config	 stun_cfg;

    /* STUN credentials */
    pj_str_t		 tx_uname;
    pj_str_t		 tx_pass;
    pj_str_t		 rx_uname;
    pj_str_t		 rx_pass;

    /* Components */
    unsigned		 comp_cnt;
    pj_ice_comp		 comp[PJ_ICE_MAX_COMP];

    /* Local candidates */
    unsigned		 lcand_cnt;
    pj_ice_cand		 lcand[PJ_ICE_MAX_CAND];

    /* Remote candidates */
    unsigned		 rcand_cnt;
    pj_ice_cand		 rcand[PJ_ICE_MAX_CAND];

    /* Checklist */
    pj_ice_checklist	 clist;
    
    /* Valid list */
    unsigned		 valid_cnt;
    unsigned		 valid_list[PJ_ICE_MAX_CHECKS];

    /* STUN servers */
    pj_dns_resolver	*resv;
    pj_dns_async_query	*resv_q;
    pj_bool_t		 relay_enabled;
    pj_sockaddr		 stun_srv;
};


PJ_DECL(pj_status_t) pj_ice_create(pj_stun_config *stun_cfg,
				   const char *name,
				   pj_ice_role role,
				   const pj_ice_cb *cb,
				   int af,
				   int sock_type,
				   pj_ice **p_ice);
PJ_DECL(pj_status_t) pj_ice_destroy(pj_ice *ice);
PJ_DECL(pj_status_t) pj_ice_set_srv(pj_ice *ice,
				    pj_bool_t enable_relay,
				    pj_dns_resolver *resolver,
				    const pj_str_t *domain);
PJ_DECL(pj_status_t) pj_ice_set_srv_addr(pj_ice *ice,
					 pj_bool_t enable_relay,
					 const pj_sockaddr_t *srv_addr,
					 unsigned addr_len);
PJ_DECL(pj_status_t) pj_ice_add_comp(pj_ice *ice,
				     unsigned comp_id,
				     const pj_sockaddr_t *local_addr,
				     unsigned addr_len);
PJ_DECL(pj_status_t) pj_ice_add_sock_comp(pj_ice *ice,
					  unsigned comp_id,
					  pj_sock_t sock);
PJ_DECL(pj_status_t) pj_ice_set_credentials(pj_ice *ice,
					    const pj_str_t *local_ufrag,
					    const pj_str_t *local_pass,
					    const pj_str_t *remote_ufrag,
					    const pj_str_t *remote_pass);
PJ_DECL(pj_status_t) pj_ice_start_gather(pj_ice *ice,
					 unsigned flags);

PJ_DECL(pj_status_t) pj_ice_add_cand(pj_ice *ice,
				     unsigned comp_id,
				     pj_ice_cand_type type,
				     pj_uint16_t local_pref,
				     const pj_str_t *foundation,
				     const pj_sockaddr_t *addr,
				     const pj_sockaddr_t *base_addr,
				     const pj_sockaddr_t *srv_addr,
				     int addr_len,
				     unsigned *cand_id);

PJ_DECL(unsigned) pj_ice_get_cand_cnt(pj_ice *ice);
PJ_DECL(pj_status_t) pj_ice_enum_cands(pj_ice *ice,
				       unsigned *p_count,
				       unsigned cand_ids[]);
PJ_DECL(pj_status_t) pj_ice_get_default_cand(pj_ice *ice,
					     unsigned comp_id,
					     int *cand_id);
PJ_DECL(pj_status_t) pj_ice_get_cand(pj_ice *ice,
				     unsigned cand_id,
				     pj_ice_cand **p_cand);

PJ_DECL(pj_status_t) pj_ice_create_check_list(pj_ice *ice,
					      unsigned rem_cand_cnt,
					      const pj_ice_cand rem_cand[]);

PJ_DECL(pj_status_t) pj_ice_start_check(pj_ice *ice);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_ICE_SOCK_H__ */

