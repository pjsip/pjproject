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
#include <pjnath/stun_endpoint.h>
#include <pjlib-util/resolver.h>
#include <pj/sock.h>

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


#define PJ_ICE_MAX_CAND	    32
#define PJ_ICE_MAX_COMP	    8


/**
 * ICE component
 */
typedef struct pj_ice_comp
{
    unsigned	    comp_id;
    pj_sock_t	    sock;
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
    unsigned		local_cand_id;
    pj_uint64_t		check_prio;
    pj_ice_check_state	check_state;

    pj_ice_cand_type	rem_type;
    pj_str_t		rem_foundation;
    pj_uint32_t		rem_prio;
    pj_sockaddr		rem_addr;
    pj_sockaddr		rem_base_addr;
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
    pj_ice_check	    *checks;
} pj_ice_checklist;


/**
 * ICE sock callback.
 */
typedef struct pj_ice_cb
{
    pj_bool_t (*on_found_cand)(pj_ice *sock,
			       pj_ice_cand_type type,
			       const pj_sockaddr_t *addr,
			       int addr_len);
} pj_ice_cb;


typedef enum pj_ice_state
{
    PJ_ICE_STATE_INIT,
    PJ_ICE_STATE_GATHERING,
    PJ_ICE_STATE_CAND_COMPLETE,
    PJ_ICE_STATE_CHECKING,
    PJ_ICE_STATE_COMPLETE,
    PJ_ICE_STATE_RESV_ERROR
} pj_ice_state;

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

    pj_ice_state	 state;

    /* Components */
    unsigned		 comp_cnt;
    pj_ice_comp		 comp[PJ_ICE_MAX_COMP];

    /* Local candidates */
    unsigned		 cand_cnt;
    pj_ice_cand		 cand[PJ_ICE_MAX_CAND];

    /* Checklist */
    pj_ice_checklist	 cklist;

    /* STUN servers */
    pj_dns_resolver	*resv;
    pj_dns_async_query	*resv_q;
    pj_bool_t		 relay_enabled;
    pj_sockaddr		 stun_srv;
};


PJ_DECL(pj_status_t) pj_ice_create(pj_stun_config *cfg,
				   const char *name,
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
				       unsigned sort_by,
				       unsigned *p_count,
				       unsigned cand_ids[]);
PJ_DECL(unsigned) pj_ice_get_default_cand(pj_ice *ice,
					  int *cand_id);
PJ_DECL(pj_status_t) pj_ice_get_cand(pj_ice *ice,
				     unsigned cand_id,
				     pj_ice_cand **p_cand);

PJ_DECL(pj_status_t) pj_ice_create_check_list(pj_ice *ice,
					      pj_bool_t is_remote_offer,
					      unsigned rem_cand_cnt,
					      const pj_ice_cand rem_cand[]);

PJ_DECL(pj_status_t) pj_ice_start_check(pj_ice *ice);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_ICE_SOCK_H__ */

