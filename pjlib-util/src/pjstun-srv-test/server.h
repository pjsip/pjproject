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
#ifndef __PJSTUN_SERVER_H__
#define __PJSTUN_SERVER_H__


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

    int		         src_addr_len;
    pj_sockaddr_in	 src_addr;
    pj_ssize_t		 rx_pkt_len;
    pj_uint8_t		 rx_pkt[MAX_PKT_LEN];
    pj_uint8_t		 tx_pkt[MAX_PKT_LEN];
};

struct stun_server_tag
{
    pj_caching_pool	 cp;
    pj_pool_t		*pool;
    pj_ioqueue_t	*ioqueue;
    unsigned		 service_cnt;
    struct service	 services[MAX_SERVICE];

    pj_bool_t		 thread_quit_flag;
    unsigned		 thread_cnt;
    pj_thread_t		*threads[16];

};

extern struct stun_server_tag server;


#endif	/* __PJSTUN_SERVER_H__ */

