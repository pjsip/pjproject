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
#ifndef __PJTURN_SRV_TURN_H__
#define __PJTURN_SRV_TURN_H__

#include <pjlib.h>
#include <pjnath.h>

typedef struct pjturn_relay_res	    pjturn_relay_res;
typedef struct pjturn_listener	    pjturn_listener;
typedef struct pjturn_permission    pjturn_permission;
typedef struct pjturn_allocation    pjturn_allocation;
typedef struct pjturn_srv	    pjturn_srv;
typedef struct pjturn_pkt	    pjturn_pkt;


#define PJTURN_INVALID_CHANNEL	    0xFFFF
#define PJTURN_NO_TIMEOUT	    ((long)0x7FFFFFFF)
#define PJTURN_MAX_PKT_LEN	    3000

/** Transport types */
enum {
    PJTURN_TP_UDP = 16,	    /**< UDP.	*/
    PJTURN_TP_TCP = 6	    /**< TCP.	*/
};


/**
 * This structure describes TURN relay resource. An allocation allocates
 * one relay resource, and optionally it may reserve another resource.
 */
struct pjturn_relay_res
{
    /** Hash table key */
    struct {
	/** Transport type. */
	int		    tp_type;

	/** Transport/relay address */
	pj_sockaddr	    addr;
    } key;

    /** Pool for this resource. */
    pj_pool_t       *pool;

    /** Mutex */
    pj_lock_t	    *lock;

    /** Allocation who requested or reserved this resource. */
    pjturn_allocation *allocation;

    /** Time when this resource times out */
    pj_time_val	    timeout;

    /** Username used in credential */
    pj_str_t	    user;

    /** Realm used in credential. */
    pj_str_t	    realm;

    /** Transport/relay socket */
    pj_sock_t	    sock;
};


/****************************************************************************/
/*
 * TURN Allocation API
 */

/**
 * This structure describes key to lookup TURN allocations in the
 * allocation hash table.
 */
typedef struct pjturn_allocation_key
{
    int		    tp_type;	/**< Transport type.	    */
    pj_sockaddr	    clt_addr;	/**< Client's address.	    */
} pjturn_allocation_key;


/**
 * Allocation request.
 */
typedef struct pjturn_allocation_req
{
    /** Requested transport */
    unsigned		tp_type;

    /** Requested IP */
    pj_sockaddr		addr;

    /** Requested bandwidth */
    unsigned		bandwidth;

    /** Lifetime. */
    unsigned		lifetime;

    /** A bits */
    unsigned		rpp_bits;

    /** Requested port */
    unsigned		rpp_port;

} pjturn_allocation_req;


/**
 * This structure describes TURN pjturn_allocation session.
 */
struct pjturn_allocation
{
    /** Hash table key to identify client. */
    pjturn_allocation_key key;

    /** Pool for this allocation. */
    pj_pool_t		*pool;

    /** Mutex */
    pj_lock_t		*lock;

    /** TURN listener. */
    pjturn_listener	*listener;

    /** Client socket, if connection to client is using TCP. */
    pj_sock_t		clt_sock;

    /** The relay resource for this allocation. */
    pjturn_relay_res	relay;

    /** Relay resource reserved by this allocation, if any */
    pjturn_relay_res	*resv;

};


/**
 * This structure describes TURN pjturn_permission or channel.
 */
struct pjturn_permission
{
    /** Hash table key */
    struct {
	/** Transport type. */
	pj_uint16_t		tp_type;

	/** Transport socket. If TCP is used, the value will be the actual
	 *  TCP socket. If UDP is used, the value will be the relay address
	 */
	pj_sock_t		sock;

	/** Peer address. */
	pj_sockaddr		peer_addr;
    } key;

    /** Pool for this permission. */
    pj_pool_t	        *pool;

    /** Mutex */
    pj_lock_t		*lock;

    /** TURN allocation that owns this permission/channel */
    pjturn_allocation	*allocation;

    /** Optional channel number, or PJTURN_INVALID_CHANNEL if channel number
     *  is not requested for this permission.
     */
    pj_uint16_t		channel;

    /** Permission timeout. */
    pj_time_val		timeout;
};

/**
 * Handle incoming packet.
 */
PJ_DECL(void) pjturn_allocation_on_rx_pkt(pjturn_allocation *alloc,
					  pjturn_pkt *pkt);


/****************************************************************************/
/*
 * TURN Listener API
 */

/**
 * This structure describes TURN listener socket. A TURN listener socket
 * listens for incoming connections from clients.
 */
struct pjturn_listener
{
    /** TURN server instance. */
    pjturn_srv		*server;

    /** Listener index in the server */
    unsigned		id;

    /** Pool for this listener. */
    pj_pool_t	       *pool;

    /** Transport type. */
    int			tp_type;

    /** Bound address of this listener. */
    pj_sockaddr		addr;

    /** Socket. */
    pj_sock_t		sock;

    /** Flags. */
    unsigned		flags;

    /** Sendto handler */
    pj_status_t		(*sendto)(pjturn_listener *listener,
				  const void *packet,
				  pj_size_t size,
				  unsigned flag,
				  const pj_sockaddr_t *addr,
				  int addr_len);

    /** Destroy handler */
    pj_status_t		(*destroy)(pjturn_listener*);
};


/**
 * An incoming packet.
 */
struct pjturn_pkt
{
    /** Pool for this packet */
    pj_pool_t		    *pool;

    /** Listener that owns this. */
    pjturn_listener	    *listener;

    /** Packet buffer. */
    pj_uint8_t		    pkt[PJTURN_MAX_PKT_LEN];

    /** Size of the packet */
    pj_size_t		    len;

    /** Arrival time. */
    pj_time_val		    rx_time;

    /** Source transport type and source address. */
    pjturn_allocation_key   src;

    /** Source address length. */
    int			    src_addr_len;
};


/**
 * Create a new listener on the specified port.
 */
PJ_DECL(pj_status_t) pjturn_listener_create_udp(pjturn_srv *srv,
						int af,
					        const pj_str_t *bound_addr,
					        unsigned port,
						unsigned concurrency_cnt,
						unsigned flags,
						pjturn_listener **p_listener);

/**
 * Send packet with this listener.
 */
PJ_DECL(pj_status_t) pjturn_listener_sendto(pjturn_listener *listener,
					    const void *packet,
					    pj_size_t size,
					    unsigned flag,
					    const pj_sockaddr_t *addr,
					    int addr_len);

/**
 * Destroy listener.
 */
PJ_DECL(pj_status_t) pjturn_listener_destroy(pjturn_listener *listener);


/****************************************************************************/
/*
 * TURN Server API
 */
/**
 * This structure describes TURN pjturn_srv instance.
 */
struct pjturn_srv
{
    /** Core settings */
    struct {
	/** Object name */
	char		*obj_name;

	/** Pool factory */
	pj_pool_factory *pf;

	/** Pool for this server instance. */
	pj_pool_t       *pool;

	/** Global Ioqueue */
	pj_ioqueue_t    *ioqueue;

	/** Mutex */
	pj_lock_t	*lock;

	/** Global timer heap instance. */
	pj_timer_heap_t *timer_heap;

	/** Number of listeners */
	unsigned         lis_cnt;

	/** Array of listeners. */
	pjturn_listener **listener;

	/** Array of STUN sessions, one for each listeners. */
	pj_stun_session	**stun_sess;

	/** Number of worker threads. */
	unsigned        thread_cnt;

	/** Array of worker threads. */
	pj_thread_t     **thread;

	/** STUN config. */
	pj_stun_config	 stun_cfg;


    } core;

    
    /** Hash tables */
    struct {
	/** Allocations hash table, indexed by transport type and
	 *  client address. 
	 */
	pj_hash_table_t *alloc;

	/** Relay resource hash table, indexed by transport type and
	 *  relay address. 
	 */
	pj_hash_table_t *res;

	/** Permission hash table, indexed by transport type, socket handle,
	 *  and peer address.
	 */
	pj_hash_table_t *peer;

    } tables;

    /** Ports settings */
    struct {
	/** Minimum UDP port number. */
	pj_uint16_t	    min_udp;

	/** Maximum UDP port number. */
	pj_uint16_t	    max_udp;

	/** Next UDP port number. */
	pj_uint16_t	    next_udp;


	/** Minimum TCP port number. */
	pj_uint16_t	    min_tcp;

	/** Maximum TCP port number. */
	pj_uint16_t	    max_tcp;

	/** Next TCP port number. */
	pj_uint16_t	    next_tcp;

    } ports;
};


/** 
 * Create server.
 */
PJ_DECL(pj_status_t) pjturn_srv_create(pj_pool_factory *pf,
				       pjturn_srv **p_srv);

/** 
 * Destroy server.
 */
PJ_DECL(pj_status_t) pjturn_srv_destroy(pjturn_srv *srv);

/** 
 * Add listener.
 */
PJ_DECL(pj_status_t) pjturn_srv_add_listener(pjturn_srv *srv,
					     pjturn_listener *lis);

/**
 * This callback is called by UDP listener on incoming packet.
 */
PJ_DECL(void) pjturn_srv_on_rx_pkt(pjturn_srv *srv, 
				   pjturn_pkt *pkt);


#endif	/* __PJTURN_SRV_TURN_H__ */

