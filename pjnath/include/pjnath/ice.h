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
 * @file ice.h
 * @brief ICE.
 */
#include <pjnath/types.h>
#include <pjnath/stun_session.h>
#include <pj/sock.h>
#include <pj/timer.h>

/**
 * @defgroup PJNATH_ICE Interactive Connectivity Establishment (ICE)
 * @brief Interactive Connectivity Establishment (ICE)
 * @ingroup PJNATH
 */


PJ_BEGIN_DECL


/**
 * @defgroup PJNATH_ICE_STREAM Transport Independent ICE Media Stream
 * @brief Transport Independent ICE Media Stream
 * @ingroup PJNATH_ICE
 * @{
 */

/**
 * This enumeration describes the type of an ICE candidate.
 */
typedef enum pj_ice_cand_type
{
    PJ_ICE_CAND_TYPE_HOST,
    PJ_ICE_CAND_TYPE_SRFLX,
    PJ_ICE_CAND_TYPE_PRFLX,
    PJ_ICE_CAND_TYPE_RELAYED
} pj_ice_cand_type;

/**
 *
 */
enum pj_ice_type_pref
{
    PJ_ICE_HOST_PREF	    = 126,
    PJ_ICE_SRFLX_PREF	    = 100,
    PJ_ICE_PRFLX_PREF	    = 110,
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
    int		     nominated_check_id;
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
    pj_sockaddr		 rel_addr;
    pj_stun_session	*stun_sess;
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
    void	(*on_ice_complete)(pj_ice *ice, pj_status_t status);
    pj_status_t (*on_tx_pkt)(pj_ice *ice, unsigned comp_id, 
			     unsigned cand_id,
			     const void *pkt, pj_size_t size,
			     const pj_sockaddr_t *dst_addr,
			     unsigned dst_addr_len);
    void	(*on_rx_data)(pj_ice *ice, unsigned comp_id,
			      void *pkt, pj_size_t size,
			      const pj_sockaddr_t *src_addr,
			      unsigned src_addr_len);
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
    void		*user_data;
    pj_mutex_t		*mutex;
    pj_ice_role		 role;
    pj_bool_t		 is_complete;
    pj_status_t		 ice_status;
    pj_ice_cb		 cb;

    pj_stun_config	 stun_cfg;

    /* STUN credentials */
    pj_str_t		 tx_ufrag;
    pj_str_t		 tx_uname;
    pj_str_t		 tx_pass;
    pj_str_t		 rx_ufrag;
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
    pj_ice_checklist	 valid_list;
};


PJ_DECL(pj_status_t) pj_ice_create(pj_stun_config *stun_cfg,
				   const char *name,
				   pj_ice_role role,
				   unsigned comp_cnt,
				   const pj_ice_cb *cb,
				   const pj_str_t *local_ufrag,
				   const pj_str_t *local_passwd,
				   pj_ice **p_ice);
PJ_DECL(pj_status_t) pj_ice_destroy(pj_ice *ice);
PJ_DECL(pj_status_t) pj_ice_add_cand(pj_ice *ice,
				     unsigned comp_id,
				     pj_ice_cand_type type,
				     pj_uint16_t local_pref,
				     const pj_str_t *foundation,
				     const pj_sockaddr_t *addr,
				     const pj_sockaddr_t *base_addr,
				     const pj_sockaddr_t *rel_addr,
				     int addr_len,
				     unsigned *cand_id);

PJ_DECL(pj_status_t) pj_ice_find_default_cand(pj_ice *ice,
					      unsigned comp_id,
					      int *cand_id);

PJ_DECL(pj_status_t) pj_ice_create_check_list(pj_ice *ice,
					      const pj_str_t *rem_ufrag,
					      const pj_str_t *rem_passwd,
					      unsigned rem_cand_cnt,
					      const pj_ice_cand rem_cand[]);
PJ_DECL(pj_status_t) pj_ice_start_check(pj_ice *ice);

PJ_DECL(pj_status_t) pj_ice_send_data(pj_ice *ice,
				      unsigned comp_id,
				      const void *data,
				      pj_size_t data_len);
PJ_DECL(pj_status_t) pj_ice_on_rx_pkt(pj_ice *ice,
				      unsigned comp_id,
				      unsigned cand_id,
				      void *pkt,
				      pj_size_t pkt_size,
				      const pj_sockaddr_t *src_addr,
				      int src_addr_len);



/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_ICE_SOCK_H__ */

