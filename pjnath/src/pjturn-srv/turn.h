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
#define PJTURN_PERM_TIMEOUT	    300
#define PJTURN_CHANNEL_TIMEOUT	    600

/** Transport types */
enum {
    PJTURN_TP_UDP = 16,	    /**< UDP.	*/
    PJTURN_TP_TCP = 6	    /**< TCP.	*/
};

/** 
 * Get transport type name string.
 */
PJ_DECL(const char*) pjturn_tp_type_name(int tp_type);

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
    } hkey;

    /** Allocation who requested or reserved this resource. */
    pjturn_allocation *allocation;

    /** Username used in credential */
    pj_str_t	    user;

    /** Realm used in credential. */
    pj_str_t	    realm;

    /** Lifetime, in seconds. */
    unsigned	    lifetime;

    /** Relay/allocation expiration time */
    pj_time_val	    expiry;

    /** Timeout timer entry */
    pj_timer_entry  timer;

    /** Transport. */
    struct {
	/** Transport/relay socket */
	pj_sock_t	    sock;

	/** Transport/relay ioqueue */
	pj_ioqueue_key_t    *key;

	/** Read operation key. */
	pj_ioqueue_op_key_t read_key;

	/** The incoming packet buffer */
	char		    rx_pkt[PJTURN_MAX_PKT_LEN];

	/** Source address of the packet. */
	pj_sockaddr	    src_addr;

	/** Source address length */
	int		    src_addr_len;

	/** The outgoing packet buffer. This must be 3wbit aligned. */
	char		    tx_pkt[PJTURN_MAX_PKT_LEN+4];
    } tp;
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
    char		addr[PJ_INET6_ADDRSTRLEN];

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
    pjturn_allocation_key hkey;

    /** Pool for this allocation. */
    pj_pool_t		*pool;

    /** Object name for logging identification */
    char		*obj_name;

    /** Client info (IP address and port) */
    char		info[80];

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

    /** Requested bandwidth */
    unsigned		bandwidth;

    /** STUN session for this client */
    pj_stun_session	*sess;

    /** Peer hash table (keyed by peer address) */
    pj_hash_table_t	*peer_table;

    /** Channel hash table (keyed by channel number) */
    pj_hash_table_t	*ch_table;
};


/**
 * This structure describes the hash table key to lookup TURN
 * permission.
 */
typedef struct pjturn_permission_key
{
    /** Peer address. */
    pj_sockaddr		peer_addr;

} pjturn_permission_key;


/**
 * This structure describes TURN pjturn_permission or channel.
 */
struct pjturn_permission
{
    /** Hash table key */
    pjturn_permission_key hkey;

    /** Transport socket. If TCP is used, the value will be the actual
     *  TCP socket. If UDP is used, the value will be the relay address
     */
    pj_sock_t		sock;

    /** TURN allocation that owns this permission/channel */
    pjturn_allocation	*allocation;

    /** Optional channel number, or PJTURN_INVALID_CHANNEL if channel number
     *  is not requested for this permission.
     */
    pj_uint16_t		channel;

    /** Permission expiration time. */
    pj_time_val		expiry;
};

/**
 * Create new allocation.
 */
PJ_DECL(pj_status_t) pjturn_allocation_create(pjturn_listener *listener,
					      const pj_sockaddr_t *src_addr,
					      unsigned src_addr_len,
					      const pj_stun_msg *msg,
					      const pjturn_allocation_req *req,
					      pjturn_allocation **p_alloc);
/**
 * Destroy allocation.
 */
PJ_DECL(void) pjturn_allocation_destroy(pjturn_allocation *alloc);

/**
 * Create relay.
 */
PJ_DECL(pj_status_t) pjturn_allocation_create_relay(pjturn_srv *srv,
					            pjturn_allocation *alloc,
						    const pj_stun_msg *msg,
						    const pjturn_allocation_req *req,
						    pjturn_relay_res *relay);

/**
 * Handle incoming packet from client.
 */
PJ_DECL(void) pjturn_allocation_on_rx_client_pkt(pjturn_allocation *alloc,
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

    /** Packet buffer (must be 32bit aligned). */
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
 * Register an allocation.
 */
PJ_DECL(pj_status_t) pjturn_srv_register_allocation(pjturn_srv *srv,
						    pjturn_allocation *alloc);

/**
 * Unregister an allocation.
 */
PJ_DECL(pj_status_t) pjturn_srv_unregister_allocation(pjturn_srv *srv,
						      pjturn_allocation *alloc);

/**
 * This callback is called by UDP listener on incoming packet.
 */
PJ_DECL(void) pjturn_srv_on_rx_pkt(pjturn_srv *srv, 
				   pjturn_pkt *pkt);


#endif	/* __PJTURN_SRV_TURN_H__ */

