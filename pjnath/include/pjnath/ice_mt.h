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
#ifndef __PJNATH_ICE_MT_H__
#define __PJNATH_ICE_MT_H__


/**
 * @file ice_mt.h
 * @brief ICE Media Transport.
 */
#include <pjnath/ice.h>
#include <pj/ioqueue.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJNATH_ICE_MEDIA_TRANSPORT ICE Media Transport
 * @brief ICE Media Transport
 * @ingroup PJNATH_ICE
 * @{
 */

typedef struct pj_icemt pj_icemt;

typedef struct pj_icemt_cb
{
    void	(*on_ice_complete)(pj_icemt *icemt, 
				   pj_status_t status);
    void	(*on_rx_rtp)(pj_icemt *icemt,
			     void *pkt, pj_size_t size,
			     const pj_sockaddr_t *src_addr,
			     unsigned src_addr_len);
    void	(*on_rx_rtcp)(pj_icemt *icemt,
			      void *pkt, pj_size_t size,
			      const pj_sockaddr_t *src_addr,
			      unsigned src_addr_len);

} pj_icemt_cb;


typedef struct pj_icemt_sock
{
    pj_icemt		*icemt;
    unsigned		 comp_id;
    pj_sock_t		 sock;
    pj_sockaddr		 addr;
    pj_sockaddr		 base_addr;
    pj_ioqueue_key_t	*key;
    pj_uint8_t		 pkt[1500];
    pj_ioqueue_op_key_t	 read_op;
    pj_ioqueue_op_key_t	 write_op;
    pj_sockaddr		 src_addr;
    int			 src_addr_len;
} pj_icemt_sock;


struct pj_icemt
{
    pj_pool_t	    *pool;
    pj_ice	    *ice;
    void	    *user_data;

    pj_icemt_cb	     cb;

    pj_icemt_sock    rtp;
    pj_icemt_sock    rtcp;

    pj_bool_t	     has_turn;
    pj_sockaddr	     stun_srv;
};


PJ_DECL(pj_status_t) pj_icemt_create(pj_stun_config *stun_cfg,
				     const char *name,
				     pj_ice_role role,
				     const pj_icemt_cb *cb,
				     unsigned rtp_port,
				     pj_bool_t has_rtcp,
				     pj_bool_t has_turn,
				     const pj_sockaddr *srv,
				     pj_icemt **p_icemt);
PJ_DECL(pj_status_t) pj_icemt_destroy(pj_icemt *icemt);



/**
 * @}
 */


PJ_END_DECL



#endif	/* __PJNATH_ICE_MT_H__ */

