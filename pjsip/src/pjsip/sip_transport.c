/* $Header: /pjproject/pjsip/src/pjsip/sip_transport.c 16    6/21/05 12:37a Bennylp $ */
/* 
 * PJSIP - SIP Stack
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_parser.h>
#include <pjsip/sip_msg.h>
#include <pjsip/sip_private.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/ioqueue.h>
#include <pj/hash.h>
#include <pj/string.h>
#include <pj/pool.h>

#define MGR_IDLE_CHECK_INTERVAL	30
#define MGR_HASH_TABLE_SIZE	PJSIP_MAX_DIALOG_COUNT
#define BACKLOG			5
#define DEFAULT_SO_SNDBUF	(8 * 1024 * 1024)
#define DEFAULT_SO_RCVBUF	(8 * 1024 * 1024)

#define LOG_TRANSPORT_MGR	"trmgr"	
#define THIS_FILE		"sip_transport"

static void destroy_transport( pjsip_transport_mgr *mgr, pjsip_transport_t *tr );


/**
 * New TCP socket for accept.
 */
typedef struct incoming_socket_rec
{
    pj_sock_t		sock;
    pj_sockaddr_in	remote;
    pj_sockaddr_in	local;
    int			addrlen;
} incoming_socket_rec;

/**
 * SIP Transport.
 */
struct pjsip_transport_t
{
    /** Standard list members, for chaining the transport in the 
     *  listener list. 
     */
    PJ_DECL_LIST_MEMBER(struct pjsip_transport_t)

    /** Transport's pool. */
    pj_pool_t		*pool;

    /** Mutex */
    pj_mutex_t		*tr_mutex;

    /** Transport name for logging purpose */
    char		 obj_name[PJ_MAX_OBJ_NAME];

    /** Socket handle */
    pj_sock_t		 sock;

    /** Transport type. */
    pjsip_transport_type_e type;

    /** Flags to keep various states (see pjsip_transport_flags_e). */
    pj_uint32_t		 flag;

    /** I/O Queue key */
    pj_ioqueue_key_t	*key;

    /** Receive data buffer */
    pjsip_rx_data	*rdata;

    /** Pointer to transport manager */
    pjsip_transport_mgr *mgr;

    /** Reference counter, to prevent this transport from being closed while
     *  it's being used. 
     */
    pj_atomic_t		 *ref_cnt;

    /** Local address. */
    pj_sockaddr_in	 local_addr;

    /** Address name (what to put in Via address field). */
    pj_sockaddr_in	 addr_name;

    /** Remote address (can be zero for UDP and for listeners). UDP listener
     *  bound to local loopback interface (127.0.0.1) has remote address set
     *  to 127.0.0.1 to prevent client from using it to send to remote hosts,
     *  because remote host then will receive 127.0.0.1 as the packet's 
     *  source address.
     */
    pj_sockaddr_in	 remote_addr;

    /** Struct to save incoming socket information. */
    incoming_socket_rec	 accept_data;

    /** When this transport should be closed. */
    pj_time_val		 close_time;

    /** List of callbacks to be called when client attempt to use this 
     *  transport while it's not connected (i.e. still connecting). 
     */
    pj_list		 cb_list;
};


/*
 * Transport manager.
 */
struct pjsip_transport_mgr 
{
    pj_hash_table_t *transport_table;
    pj_mutex_t	    *mutex;
    pjsip_endpoint  *endpt;
    pj_ioqueue_t    *ioqueue;
    pj_time_val	     next_idle_check;
    void           (*message_callback)(pjsip_endpoint*, pjsip_rx_data *rdata);
};

/*
 * Transport role.
 */
typedef enum transport_role_e
{
    TRANSPORT_ROLE_LISTENER,
    TRANSPORT_ROLE_TRANSPORT,
} transport_role_e;

/*
 * Transport key for indexing in the hash table.
 * WATCH OUT FOR ALIGNMENT PROBLEM HERE!
 */
typedef struct transport_key
{
    pj_uint8_t	type;
    pj_uint8_t	zero;
    pj_uint16_t	port;
    pj_uint32_t	addr;
} transport_key;

/*
 * Transport callback.
 */
struct transport_callback
{
    PJ_DECL_LIST_MEMBER(struct transport_callback)

    /** User defined token to be passed to the callback. */
    void *token;

    /** The callback function. */
    void (*cb)(pjsip_transport_t *tr, void *token, pj_status_t status);

};

/*
 * Transport names.
 */
const struct
{
    pjsip_transport_type_e type;
    pj_uint16_t		   port;
    pj_str_t		   name;
} transport_names[] = 
{
    { PJSIP_TRANSPORT_UNSPECIFIED, 0, {NULL, 0}},
    { PJSIP_TRANSPORT_UDP, 5060, {"UDP", 3}},
#if PJ_HAS_TCP
    { PJSIP_TRANSPORT_TCP, 5060, {"TCP", 3}},
    { PJSIP_TRANSPORT_TLS, 5061, {"TLS", 3}},
    { PJSIP_TRANSPORT_SCTP, 5060, {"SCTP", 4}}
#endif
};

static void on_ioqueue_read(pj_ioqueue_key_t *key, pj_ssize_t bytes_read);
static void on_ioqueue_write(pj_ioqueue_key_t *key, pj_ssize_t bytes_sent);
static void on_ioqueue_accept(pj_ioqueue_key_t *key, int status);
static void on_ioqueue_connect(pj_ioqueue_key_t *key, int status);

static pj_ioqueue_callback ioqueue_transport_callback = 
{
    &on_ioqueue_read,
    &on_ioqueue_write,
    &on_ioqueue_accept,
    &on_ioqueue_connect
};

static void init_key_from_transport(transport_key *key, 
				    const pjsip_transport_t *tr)
{
    /* This is to detect alignment problems. */
    pj_assert(sizeof(transport_key) == 8);

    key->type = (pj_uint8_t)tr->type;
    key->zero = 0;
    key->addr = pj_sockaddr_get_addr(&tr->remote_addr);
    key->port = pj_sockaddr_get_port(&tr->remote_addr);
    /*
    if (key->port == 0) {
	key->port = pj_sockaddr_get_port(&tr->local_addr);
    }
    */
}

#if PJ_HAS_TCP
static void init_tcp_key(transport_key *key, pjsip_transport_type_e type,
			 const pj_sockaddr_in *addr)
{
    /* This is to detect alignment problems. */
    pj_assert(sizeof(transport_key) == 8);

    key->type = (pj_uint8_t)type;
    key->zero = 0;
    key->addr = pj_sockaddr_get_addr(addr);
    key->port = pj_sockaddr_get_port(addr);
}
#endif

static void init_udp_key(transport_key *key, pjsip_transport_type_e type,
			  const pj_sockaddr_in *addr)
{
    PJ_UNUSED_ARG(addr)

    /* This is to detect alignment problems. */
    pj_assert(sizeof(transport_key) == 8);

    pj_memset(key, 0, sizeof(*key));
    key->type = (pj_uint8_t)type;

#if 0	/* Not sure why we need to make 127.0.0.1 a special case */
    if (addr->sin_addr.s_addr == inet_addr("127.0.0.1")) {
	/* This looks more complicated than it is because key->addr uses
	 * the host version of the address (i.e. converted with ntohl()).
	 */
	pj_str_t localaddr = pj_str("127.0.0.1");
	pj_sockaddr_in addr;
	pj_sockaddr_set_str_addr(&addr, &localaddr);
	key->addr = pj_sockaddr_get_addr(&addr);
    }
#endif
}

/*
 * Get type format name (for pool name).
 */
static const char *transport_get_name_format( int type )
{
    switch (type) {
    case PJSIP_TRANSPORT_UDP:
	return " udp%p";
#if PJ_HAS_TCP
    case PJSIP_TRANSPORT_TCP:
	return " tcp%p";
    case PJSIP_TRANSPORT_TLS:
	return " tls%p";
    case PJSIP_TRANSPORT_SCTP:
	return "sctp%p";
#endif
    }
    pj_assert(0);
    return 0;
}

/*
 * Get the default SIP port number for the specified type.
 */
PJ_DEF(int) pjsip_transport_get_default_port_for_type(pjsip_transport_type_e type)
{
    return transport_names[type].port;
}

/*
 * Get transport name.
 */
static const char *get_type_name(int type)
{
    return transport_names[type].name.ptr;
}

/*
 * Get transport type from name.
 */
PJ_DEF(pjsip_transport_type_e) 
pjsip_transport_get_type_from_name(const pj_str_t *name)
{
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(transport_names); ++i) {
	if (pj_stricmp(name, &transport_names[i].name) == 0) {
	    return transport_names[i].type;
	}
    }
    return PJSIP_TRANSPORT_UNSPECIFIED;
}

/*
 * Create new transmit buffer.
 */
pjsip_tx_data* pjsip_tx_data_create( pjsip_transport_mgr *mgr )
{
    pj_pool_t *pool;
    pjsip_tx_data *tdata;

    PJ_LOG(5, ("", "pjsip_tx_data_create"));

    pool = pjsip_endpt_create_pool( mgr->endpt, "ptdt%p",
				    PJSIP_POOL_LEN_TDATA,
				    PJSIP_POOL_INC_TDATA );
    if (!pool) {
	return NULL;
    }
    tdata = pj_pool_calloc(pool, 1, sizeof(pjsip_tx_data));
    tdata->pool = pool;
    tdata->mgr = mgr;
    sprintf(tdata->obj_name,"txd%p", tdata);

    tdata->ref_cnt = pj_atomic_create(tdata->pool, 0);
    if (!tdata->ref_cnt) {
	pjsip_endpt_destroy_pool( mgr->endpt, tdata->pool );
	return NULL;
    }
    
    return tdata;
}

/*
 * Add reference to tx buffer.
 */
PJ_DEF(void) pjsip_tx_data_add_ref( pjsip_tx_data *tdata )
{
    pj_atomic_inc(tdata->ref_cnt);
}

/*
 * Decrease transport data reference, destroy it when the reference count
 * reaches zero.
 */
PJ_DEF(void) pjsip_tx_data_dec_ref( pjsip_tx_data *tdata )
{
    pj_assert( pj_atomic_get(tdata->ref_cnt) > 0);
    if (pj_atomic_dec(tdata->ref_cnt) <= 0) {
	PJ_LOG(6,(tdata->obj_name, "destroying txdata"));
	pj_atomic_destroy( tdata->ref_cnt );
	pjsip_endpt_destroy_pool( tdata->mgr->endpt, tdata->pool );
    }
}

/*
 * Invalidate the content of the print buffer to force the message to be
 * re-printed when sent.
 */
PJ_DEF(void) pjsip_tx_data_invalidate_msg( pjsip_tx_data *tdata )
{
    tdata->buf.cur = tdata->buf.start;
}

/*
 * Get the transport type.
 */
PJ_DEF(pjsip_transport_type_e) pjsip_transport_get_type( const pjsip_transport_t * tr)
{
    return tr->type;
}

/*
 * Get transport type from transport flag.
 */
PJ_DEF(pjsip_transport_type_e) pjsip_get_transport_type_from_flag(unsigned flag)
{
#if PJ_HAS_TCP
    if (flag & PJSIP_TRANSPORT_SECURE) {
	return PJSIP_TRANSPORT_TLS;
    } else if (flag & PJSIP_TRANSPORT_RELIABLE) {
	return PJSIP_TRANSPORT_TCP;
    } else 
#else
    PJ_UNUSED_ARG(flag)
#endif
    {
	return PJSIP_TRANSPORT_UDP;
    }
}

/*
 * Get the transport type name.
 */
PJ_DEF(const char *) pjsip_transport_get_type_name( const pjsip_transport_t * tr)
{
    return get_type_name(tr->type);
}

/*
 * Get the transport's object name.
 */
PJ_DEF(const char*) pjsip_transport_get_obj_name( const pjsip_transport_t *tr )
{
    return tr->obj_name;
}

/*
 * Get the transport's reference counter.
 */
PJ_DEF(int) pjsip_transport_get_ref_cnt( const pjsip_transport_t *tr )
{
    return pj_atomic_get(tr->ref_cnt);
}

/*
 * Get transport local address.
 */
PJ_DEF(const pj_sockaddr_in*) pjsip_transport_get_local_addr( pjsip_transport_t *tr )
{
    return &tr->local_addr;
}

/*
 * Get address name.
 */
PJ_DEF(const pj_sockaddr_in*) pjsip_transport_get_addr_name (pjsip_transport_t *tr)
{
    return &tr->addr_name;
}

/*
 * Get transport remote address.
 */
PJ_DEF(const pj_sockaddr_in*) pjsip_transport_get_remote_addr( const pjsip_transport_t *tr )
{
    return &tr->remote_addr;
}

/*
 * Get transport flag.
 */
PJ_DEF(unsigned) pjsip_transport_get_flag( const pjsip_transport_t * tr )
{
    return tr->flag;
}

/*
 * Add reference to the specified transport.
 */
PJ_DEF(void) pjsip_transport_add_ref( pjsip_transport_t * tr )
{
    pj_atomic_inc(tr->ref_cnt);
}

/*
 * Decrease the reference time of the transport.
 */
PJ_DEF(void) pjsip_transport_dec_ref( pjsip_transport_t *tr )
{
    pj_assert(tr->ref_cnt > 0);
    if (pj_atomic_dec(tr->ref_cnt) == 0) {
	pj_gettimeofday(&tr->close_time);
	tr->close_time.sec += PJSIP_TRANSPORT_CLOSE_TIMEOUT;
    }
}

/*
 * Open the underlying transport.
 */
static pj_sock_t create_socket( pjsip_transport_type_e type,
				pj_sockaddr_in *local )
{
    int sock_family;
    int sock_type;
    int sock_proto;
    int len;
    pj_sock_t sock;

    /* Set socket parameters */
    if (type == PJSIP_TRANSPORT_UDP) {
	sock_family = PJ_AF_INET;
	sock_type = PJ_SOCK_DGRAM;
	sock_proto = 0;

#if PJ_HAS_TCP
    } else if (type == PJSIP_TRANSPORT_TCP) {
	sock_family = PJ_AF_INET;
	sock_type = PJ_SOCK_STREAM;
	sock_proto = 0;
#endif
    } else {
	PJ_LOG(2,("", "create_socket: unsupported transport type %s",
		  get_type_name(type)));
	return PJ_INVALID_SOCKET;
    }

    /* Create socket. */
    sock = pj_sock_socket( sock_family, sock_type, sock_proto, PJ_SOCK_ASYNC);
    if (sock == PJ_INVALID_SOCKET) {
	PJ_PERROR((THIS_FILE, "%s socket()", get_type_name(type)));
	return PJ_INVALID_SOCKET;
    }

    /* Bind the socket to the requested address, or if no address is
     * specified, let the operating system chooses the address.
     */
    if (/*local->sin_addr.s_addr != 0 &&*/ local->sin_port != 0) {
	/* Bind to the requested address. */
	if (pj_sock_bind(sock, local, sizeof(*local)) != 0) {
	    PJ_PERROR((THIS_FILE, "bind() to %s %s:%d",
				  get_type_name(type),
				  pj_sockaddr_get_str_addr(local),
				  pj_sockaddr_get_port(local)));
	    pj_sock_close(sock);
	    return PJ_INVALID_SOCKET;
	}
    } else if (type == PJSIP_TRANSPORT_UDP) {
	/* Only for UDP sockets: bind to any address so that the operating 
	 * system allocates the port for us. For TCP, let the OS implicitly
	 * bind the socket with connect() syscall (if we bind now, then we'll
	 * get 0.0.0.0 as local address).
	 */
	pj_memset(local, 0, sizeof(*local));
	local->sin_family = PJ_AF_INET;
	if (pj_sock_bind(sock, local, sizeof(*local)) != 0) {
	    PJ_PERROR((THIS_FILE, "bind() to %s 0.0.0.0:0", get_type_name(type)));
	    pj_sock_close(sock);
	    return PJ_INVALID_SOCKET;
	}

	/* Get the local address. */
	len = sizeof(pj_sockaddr_in);
	if (pj_sock_getsockname(sock, local, &len)) {
	    PJ_PERROR((THIS_FILE, "getsockname()"));
	    pj_sock_close(sock);
	    return -1;
	}
    }

    return sock;
}

/*
 * Close the transport.
 */
static void destroy_socket( pjsip_transport_t * tr)
{
    pj_assert( pj_atomic_get(tr->ref_cnt) == 0);
    pj_sock_close(tr->sock);
    tr->sock = -1;
}

/*
 * Create a new transport object.
 */
static pjsip_transport_t* create_transport( pjsip_transport_mgr *mgr,
					    pjsip_transport_type_e type,
					    pj_sock_t sock_hnd,
					    const pj_sockaddr_in *local_addr,
					    const pj_sockaddr_in *addr_name)
{
    pj_pool_t *tr_pool=NULL, *rdata_pool=NULL;
    pjsip_transport_t *tr = NULL;

    /* Allocate pool for transport from endpoint. */
    tr_pool = pjsip_endpt_create_pool( mgr->endpt,
				       transport_get_name_format(type),
				       PJSIP_POOL_LEN_TRANSPORT,
				       PJSIP_POOL_INC_TRANSPORT );
    if (!tr_pool) {
	goto on_error;
    }

    /* Allocate pool for rdata from endpoint. */
    rdata_pool = pjsip_endpt_create_pool( mgr->endpt,
					     "prdt%p",
					     PJSIP_POOL_LEN_RDATA,
					     PJSIP_POOL_INC_RDATA );
    if (!rdata_pool) {
	goto on_error;
    }

    /* Allocate and initialize the transport. */
    tr = pj_pool_calloc(tr_pool, 1, sizeof(*tr));
    tr->pool = tr_pool;
    tr->type = type;
    tr->mgr = mgr;
    tr->sock = sock_hnd;
    pj_memcpy(&tr->local_addr, local_addr, sizeof(pj_sockaddr_in));
    pj_list_init(&tr->cb_list);
    sprintf(tr->obj_name, transport_get_name_format(type), tr);

    if (type != PJSIP_TRANSPORT_UDP) {
	tr->flag |= PJSIP_TRANSPORT_RELIABLE;
    }

    /* Address name. */
    if (addr_name == NULL) {
	addr_name = &tr->local_addr;
    } 
    pj_memcpy(&tr->addr_name, addr_name, sizeof(*addr_name));

    /* Create atomic */
    tr->ref_cnt = pj_atomic_create(tr_pool, 0);
    if (!tr->ref_cnt) {
	goto on_error;
    }
    
    /* Init rdata in the transport. */
    tr->rdata = pj_pool_alloc(rdata_pool, sizeof(*tr->rdata));
    tr->rdata->pool = rdata_pool;
    tr->rdata->len = 0;
    tr->rdata->transport = tr;
    
    /* Init transport mutex. */
    tr->tr_mutex = pj_mutex_create(tr_pool, "mtr%p", 0);
    if (!tr->tr_mutex) {
	PJ_PERROR((tr->obj_name, "pj_mutex_create()"));
	goto on_error;
    }

    /* Register to I/O Queue */
    tr->key = pj_ioqueue_register(tr_pool, mgr->ioqueue, 
				  (pj_oshandle_t)tr->sock, tr,
				  &ioqueue_transport_callback);
    if (tr->key == NULL) {
	PJ_PERROR((tr->obj_name, "pj_ioqueue_register()"));
	goto on_error;
    }

    return tr;

on_error:
    if (tr && tr->tr_mutex) {
	pj_mutex_destroy(tr->tr_mutex);
    }
    if (tr_pool) {
	pjsip_endpt_destroy_pool(mgr->endpt, tr_pool);
    }
    if (rdata_pool) {
	pjsip_endpt_destroy_pool(mgr->endpt, rdata_pool);
    }
    return NULL;
}

/*
 * Destroy transport.
 */
static void destroy_transport( pjsip_transport_mgr *mgr, pjsip_transport_t *tr )
{
    transport_key hash_key;

    /* Remove from I/O queue. */
    pj_ioqueue_unregister( mgr->ioqueue, tr->key );

    /* Remove from hash table */
    init_key_from_transport(&hash_key, tr);
    pj_hash_set(NULL, mgr->transport_table, &hash_key, sizeof(hash_key), NULL);

    /* Close transport. */
    destroy_socket(tr);

    /* Destroy the transport mutex. */
    pj_mutex_destroy(tr->tr_mutex);

    /* Destroy atomic */
    pj_atomic_destroy( tr->ref_cnt );

    /* Release the pool associated with the rdata. */
    pjsip_endpt_destroy_pool(mgr->endpt, tr->rdata->pool );

    /* Release the pool associated with the transport. */
    pjsip_endpt_destroy_pool(mgr->endpt, tr->pool );
}


static int transport_send_msg( pjsip_transport_t *tr, pjsip_tx_data *tdata,
			       const pj_sockaddr_in *addr)
{
    const char *buf = tdata->buf.start;
    int sent;
    int len;

    /* Allocate buffer if necessary. */
    if (tdata->buf.start == NULL) {
	tdata->buf.start = pj_pool_alloc( tdata->pool, PJSIP_MAX_PKT_LEN);
	tdata->buf.cur = tdata->buf.start;
	tdata->buf.end = tdata->buf.start + PJSIP_MAX_PKT_LEN;
    }

    /* Print the message if it's not printed */
    if (tdata->buf.cur <= tdata->buf.start) {
	len = pjsip_msg_print(tdata->msg, tdata->buf.start, 
			      tdata->buf.end - tdata->buf.start);
	if (len < 1) {
	    return len;
	}
	tdata->buf.cur += len;
	tdata->buf.cur[len] = '\0';
    }

    /* BUG BUG BUG */
    /* MUST CHECK THAT THE SOCKET IS READY TO SEND (IOQueue)! */
    PJ_TODO(BUG_BUG_BUG___SENDING_DATAGRAM_WHILE_SOCKET_IS_PENDING__)

    /* Send the message. */
    buf = tdata->buf.start;
    len = tdata->buf.cur - tdata->buf.start;

    if (tr->type == PJSIP_TRANSPORT_UDP) {
	PJ_LOG(4,(tr->obj_name, "sendto %s:%d, %d bytes, data:\n"
		  "----------- begin msg ------------\n"
		  "%s"
		  "------------ end msg -------------",
		  pj_sockaddr_get_str_addr(addr), 
		  pj_sockaddr_get_port(addr),
		  len, buf));

	sent = pj_ioqueue_sendto( tr->mgr->ioqueue, tr->key,
				  buf, len, addr, sizeof(*addr));
    } 
#if PJ_HAS_TCP
    else {
	PJ_LOG(4,(tr->obj_name, "sending %d bytes, data:\n"
		  "----------- begin msg ------------\n"
		  "%s"
		  "------------ end msg -------------",
		  len, buf));

	sent = pj_ioqueue_write (tr->mgr->ioqueue, tr->key, buf, len);
    }
#else
    else {
	pj_assert(0);
	sent = -1;
    }
#endif

    if (sent == len || sent == PJ_IOQUEUE_PENDING) {
	return len;
    }

    /* On error, clear the flag. */
    PJ_PERROR((tr->obj_name, tr->type == PJSIP_TRANSPORT_UDP ? "pj_ioqueue_sendto()" : "pj_ioqueue_write()"));
    return -1;
}

/*
 * Send a SIP message using the specified transport, to the address specified
 * in the outgoing data.
 */
PJ_DEF(int) pjsip_transport_send_msg( pjsip_transport_t *tr, 
				      pjsip_tx_data *tdata,
				      const pj_sockaddr_in *addr)
{
    int sent;

    PJ_LOG(5, (tr->obj_name, "pjsip_transport_send_msg(tdata=%s)", tdata->obj_name));

    sent = transport_send_msg(tr, tdata, addr );
    return sent;
}

///////////////////////////////////////////////////////////////////////////////

/*
 * Create a new transport manager.
 */
PJ_DEF(pjsip_transport_mgr*) 
pjsip_transport_mgr_create( pj_pool_t *pool,
			    pjsip_endpoint * endpt,
			    void (*cb)(pjsip_endpoint*,pjsip_rx_data *) )
{
    pjsip_transport_mgr *mgr;

    PJ_LOG(5, (LOG_TRANSPORT_MGR, "pjsip_transport_mgr_create()"));

    mgr = pj_pool_alloc(pool, sizeof(*mgr));
    mgr->endpt = endpt;
    mgr->message_callback = cb;
    
    mgr->transport_table = pj_hash_create(pool, MGR_HASH_TABLE_SIZE);
    if (!mgr->transport_table) {
	PJ_LOG(3, (LOG_TRANSPORT_MGR, "error creating transport manager hash table"));
	return NULL;
    }
    mgr->ioqueue = pj_ioqueue_create(pool, PJSIP_MAX_TRANSPORTS);
    if (!mgr->ioqueue) {
	PJ_LOG(3, (LOG_TRANSPORT_MGR, "error creating IO queue"));
	return NULL;
    }
    mgr->mutex = pj_mutex_create(pool, "tmgr%p", 0);
    if (!mgr->mutex) {
	PJ_LOG(3, (LOG_TRANSPORT_MGR, "error creating mutex"));
	pj_ioqueue_destroy(mgr->ioqueue);
	return NULL;
    }
    pj_gettimeofday(&mgr->next_idle_check);
    mgr->next_idle_check.sec += MGR_IDLE_CHECK_INTERVAL;
    return mgr;
}

/*
 * Destroy transport manager.
 */
PJ_DEF(void) pjsip_transport_mgr_destroy( pjsip_transport_mgr *mgr )
{
    pj_hash_iterator_t itr_val;
    pj_hash_iterator_t *itr;
    
    PJ_LOG(5, (LOG_TRANSPORT_MGR, "pjsip_transport_mgr_destroy()"));

    pj_mutex_lock(mgr->mutex);

    itr = pjsip_transport_first(mgr, &itr_val);
    while (itr != NULL) {
	pj_hash_iterator_t *next;
	pjsip_transport_t *transport;
	
	transport = pjsip_transport_this(mgr, itr);

	next = pjsip_transport_next(mgr, itr);

	pj_atomic_set(transport->ref_cnt, 0);
	destroy_transport( mgr, transport);

	itr = next;
    }
    pj_ioqueue_destroy(mgr->ioqueue);

    pj_mutex_unlock(mgr->mutex);
}

/*
 * Create listener
 */
static pj_status_t create_listener( pjsip_transport_mgr *mgr,
				    pjsip_transport_type_e type,
				    pj_sock_t sock_hnd,
				    pj_sockaddr_in *local_addr,
				    const pj_sockaddr_in *addr_name)
{
    pjsip_transport_t *tr;
    struct transport_key *hash_key;
    int opt_val;

    opt_val = DEFAULT_SO_SNDBUF;
    if (pj_sock_setsockopt( sock_hnd, SOL_SOCKET, SO_SNDBUF, &opt_val, sizeof(opt_val)) != PJ_OK) {
	PJ_LOG(3, (LOG_TRANSPORT_MGR, "create listener: error setting SNDBUF to %d", DEFAULT_SO_SNDBUF));
	// Just ignore the error.
    }

    opt_val = DEFAULT_SO_RCVBUF;
    if (pj_sock_setsockopt( sock_hnd, SOL_SOCKET, SO_RCVBUF, &opt_val, sizeof(opt_val)) != PJ_OK) {
	PJ_LOG(3, (LOG_TRANSPORT_MGR, "create listener: error setting RCVBUF to %d", DEFAULT_SO_SNDBUF));
	// Just ignore the error
    }

    tr = create_transport(mgr, type, sock_hnd, local_addr, addr_name);
    if (!tr) {
	pj_sock_close(sock_hnd);
	return -1;
    }
#if PJ_HAS_TCP
    if (type == PJSIP_TRANSPORT_TCP) {
	pj_status_t status;

	if (pj_sock_listen(tr->sock, BACKLOG) != 0) {
	    PJ_PERROR((tr->obj_name, "listen()"));
	    destroy_transport(mgr, tr);
	    return -1;
	}
	tr->accept_data.addrlen = sizeof(tr->accept_data.local);
	status = pj_ioqueue_accept(mgr->ioqueue, tr->key, 
				   &tr->accept_data.sock, 
				   &tr->accept_data.local, 
				   &tr->accept_data.remote,
				   &tr->accept_data.addrlen);
	if (status != PJ_IOQUEUE_PENDING) {
	    PJ_PERROR((tr->obj_name, "pj_ioqueue_accept()"));
	    destroy_transport(mgr, tr);
	    return -1;
	}

    } else 
#endif
    if (type == PJSIP_TRANSPORT_UDP) {
	pj_status_t status;

	tr->rdata->addr_len = sizeof(tr->rdata->addr);
	status = pj_ioqueue_recvfrom( mgr->ioqueue, tr->key, 
				      tr->rdata->packet, PJSIP_MAX_PKT_LEN,
				      &tr->rdata->addr, 
				      &tr->rdata->addr_len);
	if (status != PJ_IOQUEUE_PENDING) {
	    PJ_PERROR((tr->obj_name, "pj_ioqueue_recvfrom()"));
	    destroy_transport(mgr, tr);
	    return -1;
	}
    }

    pj_atomic_set(tr->ref_cnt, 1);

    /* Listeners normally have no remote address */
    pj_memset(&tr->remote_addr, 0, sizeof(tr->remote_addr));

    /* Set remote address to 127.0.0.1 for UDP socket bound to 127.0.0.1. 
     * See further comments on struct pjsip_transport_t definition.
     */
    if (type == PJSIP_TRANSPORT_UDP && local_addr->sin_addr.s_addr == inet_addr("127.0.0.1")) {
	pj_str_t localaddr = pj_str("127.0.0.1");
	pj_sockaddr_set_str_addr( &tr->remote_addr, &localaddr);
    }
    hash_key = pj_pool_alloc(tr->pool, sizeof(transport_key));
    init_key_from_transport(hash_key, tr);

    pj_mutex_lock(mgr->mutex);
    pj_hash_set(tr->pool, mgr->transport_table, hash_key, sizeof(transport_key), tr);
    pj_mutex_unlock(mgr->mutex);

    PJ_LOG(4,(tr->obj_name, "Listening at %s %s:%d", 
			 get_type_name(tr->type), 
			 pj_sockaddr_get_str_addr(&tr->local_addr),
			 pj_sockaddr_get_port(&tr->local_addr)));
    PJ_LOG(4,(tr->obj_name, "Listener public address is at %s %s:%d", 
			 get_type_name(tr->type), 
			 pj_sockaddr_get_str_addr(&tr->addr_name),
			 pj_sockaddr_get_port(&tr->addr_name)));
    return PJ_OK;
}

/*
 * Create listener.
 */
PJ_DEF(pj_status_t) pjsip_create_listener( pjsip_transport_mgr *mgr,
					   pjsip_transport_type_e type,
					   pj_sockaddr_in *local_addr,
					   const pj_sockaddr_in *addr_name)
{
    pj_sock_t sock_hnd;

    PJ_LOG(5, (LOG_TRANSPORT_MGR, "pjsip_create_listener(type=%d)", type));

    sock_hnd = create_socket(type, local_addr);
    if (sock_hnd == PJ_INVALID_SOCKET) {
	return -1;
    }

    return create_listener(mgr, type, sock_hnd, local_addr, addr_name);
}

/*
 * Create UDP listener.
 */
PJ_DEF(pj_status_t) pjsip_create_udp_listener( pjsip_transport_mgr *mgr,
					       pj_sock_t sock,
					       const pj_sockaddr_in *addr_name)
{
    pj_sockaddr_in local_addr;
    int addrlen = sizeof(local_addr);

    if (pj_sock_getsockname(sock, (pj_sockaddr_t*)&local_addr, &addrlen) != 0)
	return -1;

    return create_listener(mgr, PJSIP_TRANSPORT_UDP, sock, &local_addr, addr_name);
}

/*
 * Find transport to be used to send message to remote destination. If no
 * suitable transport is found, a new one will be created.
 */
PJ_DEF(void) pjsip_transport_get( pjsip_transport_mgr *mgr,
			          pj_pool_t *pool,
				  pjsip_transport_type_e type,
				  const pj_sockaddr_in *remote,
				  void *token,
				  pjsip_transport_completion_callback *cb)
{
    transport_key search_key, *hash_key;
    pjsip_transport_t *tr;
    pj_sockaddr_in local;
    int sock_hnd;
    pj_status_t status;
    struct transport_callback *cb_rec;

    PJ_LOG(5, (LOG_TRANSPORT_MGR, "pjsip_transport_get()"));

    /* Create the callback record.
     */
    cb_rec = pj_pool_calloc(pool, 1, sizeof(*cb_rec));
    cb_rec->token = token;
    cb_rec->cb = cb;

    /* Create key for hash table look-up.
     * The key creation is different for TCP and UDP.
     */
#if PJ_HAS_TCP
    if (type==PJSIP_TRANSPORT_TCP) {
	init_tcp_key(&search_key, type, remote);
    } else 
#endif
    if (type==PJSIP_TRANSPORT_UDP) {
	init_udp_key(&search_key, type, remote);
    }

    /* Start lock the manager. */
    pj_mutex_lock(mgr->mutex);

    /* Lookup the transport in the hash table. */
    tr = pj_hash_get(mgr->transport_table, &search_key, sizeof(transport_key));

    if (tr) {
	/* Transport found. If the transport is still busy (i.e. connecting
	 * is in progress), then just register the callback. Otherwise
	 * report via the callback if callback is specified. 
	 */
	pj_mutex_unlock(mgr->mutex);
	pj_mutex_lock(tr->tr_mutex);

	if (tr->flag & PJSIP_TRANSPORT_IOQUEUE_BUSY) {
	    /* Transport is busy. Just register the callback. */
	    pj_list_insert_before(&tr->cb_list, cb_rec);

	} else {
	    /* Transport is ready. Call callback now.
	     */
	    (*cb_rec->cb)(tr, cb_rec->token, PJ_OK);
	}
	pj_mutex_unlock(tr->tr_mutex);

	return;
    }


    /* Transport not found. Create new one. */
    pj_memset(&local, 0, sizeof(local));
    local.sin_family = PJ_AF_INET;
    sock_hnd = create_socket(type, &local);
    if (sock_hnd == PJ_INVALID_SOCKET) {
	pj_mutex_unlock(mgr->mutex);
	(*cb_rec->cb)(NULL, cb_rec->token, -1);
	return;
    }
    tr = create_transport(mgr, type, sock_hnd, &local, NULL);
    if (!tr) {
	pj_mutex_unlock(mgr->mutex);
	(*cb_rec->cb)(NULL, cb_rec->token, -1);
	return;
    }

#if PJ_HAS_TCP
    if (type == PJSIP_TRANSPORT_TCP) {
	pj_memcpy(&tr->remote_addr, remote, sizeof(pj_sockaddr_in));
	status = pj_ioqueue_connect(mgr->ioqueue, tr->key, 
				    &tr->remote_addr, sizeof(pj_sockaddr_in));
	pj_assert(status != 0);
	if (status != PJ_IOQUEUE_PENDING) {
	    PJ_PERROR((tr->obj_name, "pj_ioqueue_connect()"));
	    destroy_transport(mgr, tr);
	    pj_mutex_unlock(mgr->mutex);
	    (*cb_rec->cb)(NULL, cb_rec->token, -1);
	    return;
	}
    } else 
#endif
    if (type == PJSIP_TRANSPORT_UDP) {
	int len;

	do {
	    tr->rdata->addr_len = sizeof(tr->rdata->addr);
	    len = pj_ioqueue_recvfrom( mgr->ioqueue, tr->key, 
				       tr->rdata->packet, PJSIP_MAX_PKT_LEN,
				       &tr->rdata->addr, 
				       &tr->rdata->addr_len);
	    pj_assert(len < 0);
	    if (len != PJ_IOQUEUE_PENDING) {
		PJ_PERROR((tr->obj_name, "pj_ioqueue_recvfrom()"));
		destroy_transport(mgr, tr);
		pj_mutex_unlock(mgr->mutex);
		(*cb_rec->cb)(NULL, cb_rec->token, -1);
		return;
	    }

	    /* Bug here.
	     * If data is immediately available, although not likely, it will
	     * be dropped because we don't expect to have data right after
	     * the socket is created, do we ?!
	     */
	    PJ_TODO(FIXED_BUG_ON_IMMEDIATE_TRANSPORT_DATA);

	} while (len != PJ_IOQUEUE_PENDING);

	//Bug: cb will never be called!
	//     Must force status to PJ_OK;
	//status = PJ_IOQUEUE_PENDING;

	status = PJ_OK;

    } else {
	pj_mutex_unlock(mgr->mutex);
	(*cb_rec->cb)(NULL, cb_rec->token, -1);
	return;
    }

    pj_assert(status==PJ_IOQUEUE_PENDING || status==PJ_OK);
    pj_mutex_lock(tr->tr_mutex);
    hash_key = pj_pool_alloc(tr->pool, sizeof(transport_key));
    pj_memcpy(hash_key, &search_key, sizeof(transport_key));
    pj_hash_set(tr->pool, mgr->transport_table, hash_key, sizeof(transport_key), tr);
    if (status == PJ_OK) {
	pj_mutex_unlock(tr->tr_mutex);
	pj_mutex_unlock(mgr->mutex);
	(*cb_rec->cb)(tr, cb_rec->token, PJ_OK);
    } else {
	pj_list_insert_before(&tr->cb_list, cb_rec);
	pj_mutex_unlock(tr->tr_mutex);
	pj_mutex_unlock(mgr->mutex);
    }

}

#if PJ_HAS_TCP
/*
 * Handle completion of asynchronous accept() operation.
 * This function is called by handle_events() function.
 */
static void handle_new_connection( pjsip_transport_mgr *mgr,
				   pjsip_transport_t *listener,
				   pj_status_t status )
{
    pjsip_transport_t *tr;
    transport_key *hash_key;

    pj_assert (listener->type == PJSIP_TRANSPORT_TCP);

    if (status != PJ_OK) {
	PJ_PERROR((listener->obj_name, "accept() returned error"));
	return;
    }

    PJ_LOG(4,(listener->obj_name, "incoming tcp connection from %s:%d",
	      pj_sockaddr_get_str_addr(&listener->accept_data.remote),
	      pj_sockaddr_get_port(&listener->accept_data.remote)));

    tr = create_transport(mgr, listener->type, 
			  listener->accept_data.sock,
			  &listener->accept_data.local,
			  NULL);
    if (!tr) {
	goto on_return;
    }

    /*
    tr->rdata->addr_len = sizeof(tr->rdata->addr);
    status = pj_ioqueue_recvfrom( mgr->ioqueue, tr->key, 
				  tr->rdata->packet, PJSIP_MAX_PKT_LEN,
				  &tr->rdata->addr, 
				  &tr->rdata->addr_len);
    */
    tr->rdata->addr = listener->accept_data.remote;
    tr->rdata->addr_len = listener->accept_data.addrlen;

    status = pj_ioqueue_read (mgr->ioqueue, tr->key, tr->rdata->packet, PJSIP_MAX_PKT_LEN);
    if (status != PJ_IOQUEUE_PENDING) {
	PJ_PERROR((tr->obj_name, "pj_ioqueue_read()"));
	destroy_transport(mgr, tr);
	goto on_return;
    }

    pj_memcpy(&tr->remote_addr, &listener->accept_data.remote, listener->accept_data.addrlen);
    hash_key = pj_pool_alloc(tr->pool, sizeof(transport_key));
    init_key_from_transport(hash_key, tr);

    pj_mutex_lock(mgr->mutex);
    pj_hash_set(tr->pool, mgr->transport_table, hash_key, sizeof(transport_key), tr);
    pj_mutex_unlock(mgr->mutex);

on_return:
    /* Re-initiate asynchronous accept() */
    listener->accept_data.addrlen = sizeof(listener->accept_data.local);
    status = pj_ioqueue_accept(mgr->ioqueue, listener->key, 
			       &listener->accept_data.sock, 
			       &listener->accept_data.local, 
			       &listener->accept_data.remote,
			       &listener->accept_data.addrlen);
    if (status != PJ_IOQUEUE_PENDING) {
	PJ_PERROR((listener->obj_name, "pj_ioqueue_accept()"));
	return;
    }
}

/*
 * Handle completion of asynchronous connect() function.
 * This function is called by the handle_events() function.
 */
static void handle_connect_completion( pjsip_transport_mgr *mgr,
				       pjsip_transport_t *tr,
				       pj_status_t status )
{
    struct transport_callback new_list;
    struct transport_callback *cb_rec;

    PJ_UNUSED_ARG(mgr)

    /* On connect completion, we must call all registered callbacks in
     * the transport.
     */

    /* Initialize new list. */
    pj_list_init(&new_list);

    /* Hold transport's mutex. We don't want other thread to register a 
     * callback while we're dealing with it. 
    */
    pj_mutex_lock(tr->tr_mutex);

    /* Copy callback list to new list so that we can call the callbacks
     * without holding the mutex.
     */
    pj_list_merge_last(&new_list, &tr->cb_list);

    /* Clear transport's busy flag. */
    tr->flag &= ~PJSIP_TRANSPORT_IOQUEUE_BUSY;

    /* If success, update local address. 
     * Local address is only available after connect() has returned.
     */
    if (status == PJ_OK) {
	int addrlen = sizeof(tr->local_addr);
	int rc;
	if ((rc=pj_sock_getsockname(tr->sock, (pj_sockaddr_t*)&tr->local_addr, &addrlen)) == 0) {
	    pj_memcpy(&tr->addr_name, &tr->local_addr, sizeof(tr->addr_name));
	} else {
	    PJ_LOG(4,(tr->obj_name, "Unable to get local address (getsockname=%d)", rc));
	}
    }

    /* Unlock mutex. */
    pj_mutex_unlock(tr->tr_mutex);

    /* Call all registered callbacks. */
    cb_rec = new_list.next;
    while (cb_rec != &new_list) {
	struct transport_callback *next;
	next = cb_rec->next;
	(*cb_rec->cb)(tr, cb_rec->token, status);
	cb_rec = next;
    }

    /* Success? */
    if (status != PJ_OK) {
	destroy_transport(mgr, tr);
	return;
    }

    /* Initiate read operation to socket. */
    status = pj_ioqueue_read (mgr->ioqueue, tr->key, tr->rdata->packet, PJSIP_MAX_PKT_LEN);
    if (status != PJ_IOQUEUE_PENDING) {
	PJ_PERROR((tr->obj_name, "pj_ioqueue_read()"));
	destroy_transport(mgr, tr);
	return;
    }
}
#endif /* PJ_HAS_TCP */

/*
 * Handle incoming data.
 * This function is called when the transport manager receives 'notification'
 * from the I/O Queue that the receive operation has completed.
 * This function will then attempt to parse the message, and hands over the
 * message to the endpoint.
 */
static void handle_received_data( pjsip_transport_mgr *mgr,
				  pjsip_transport_t *tr,
				  pj_ssize_t size )
{
    pjsip_msg *msg;
    pjsip_cid_hdr *call_id;
    pjsip_rx_data *rdata = tr->rdata;
    pj_pool_t *rdata_pool;
    pjsip_hdr *hdr;
    pj_str_t s;
    char *src_addr;
    int src_port;
    pj_size_t msg_fragment_size = 0;

    /* Check size. */
    if (size < 1) {
	if (tr->type != PJSIP_TRANSPORT_UDP) {
	    /* zero bytes indicates transport has been closed for TCP.
	     * But alas, we can't destroy it now since transactions may still
	     * have reference to it. In that case, just do nothing, the 
	     * transaction will receive error when it tries to send anything.
	     * But alas!! UAC transactions wont send anything!!.
	     * So this is a bug!
	     */
	    if (pj_atomic_get(tr->ref_cnt)==0) {
		PJ_LOG(4,(tr->obj_name, "connection closed"));
		destroy_transport(mgr, tr);
	    } else {
		PJ_TODO(HANDLE_TCP_TRANSPORT_CLOSED);
		//PJ_TODO(SIGNAL_TRANSACTIONS_ON_TRANSPORT_CLOSED);
	    }
	    return;
	} else {
	    /* On Windows machines, UDP recv() will return zero upon receiving
	     * ICMP port unreachable message.
	     */
	    PJ_LOG(4,(tr->obj_name, "Ignored zero length UDP packet (port unreachable?)"));
	    goto on_return;
	}
    }
    
    /* Save received time. */
    pj_gettimeofday(&rdata->timestamp);
    
    /* Update length. */
    rdata->len += size;

    /* Null terminate packet, this is the requirement of the parser. */
    rdata->packet[rdata->len] = '\0';

    /* Get source address and port for logging purpose. */
    src_addr = pj_sockaddr_get_str_addr(&rdata->addr);
    src_port = pj_sockaddr_get_port(&rdata->addr);

    /* Print the whole data to the log. */
    PJ_LOG(4,(tr->obj_name, "%d bytes recvfrom %s:%d:\n"
		    "----------- begin msg ------------\n"
		    "%s"
		    "------------ end msg -------------", 
	       rdata->len, src_addr, src_port, rdata->packet));


    /* Process all message fragments. */
    while (rdata->len > 0) {

	msg_fragment_size = rdata->len;
#if PJ_HAS_TCP
	/* For TCP transport, check if the whole message has been received. */
	if (tr->type != PJSIP_TRANSPORT_UDP) {
	    pj_bool_t is_complete;
	    is_complete = pjsip_find_msg(rdata->packet, rdata->len, PJ_FALSE, &msg_fragment_size);
	    if (!is_complete) {
		if (rdata->len == PJSIP_MAX_PKT_LEN) {
		    PJ_LOG(1,(tr->obj_name, 
			      "Transport buffer full (%d bytes) for TCP socket %s:%d "
			      "(probably too many invalid fragments received). "
			      "Buffer will be discarded.",
			      PJSIP_MAX_PKT_LEN, src_addr, src_port));
		    goto on_return;
		} else {
		    goto tcp_read_packet;
		}
	    }
	}
#endif

	/* Clear parser error report */
	pj_list_init(&rdata->parse_err);

	/* Parse the message. */
	PJ_LOG(5,(tr->obj_name, "Parsing %d bytes from %s:%d", msg_fragment_size,
		src_addr, src_port));

	msg = pjsip_parse_msg( rdata->pool, rdata->packet, msg_fragment_size, 
			       &rdata->parse_err);
	if (msg == NULL) {
	    PJ_LOG(3,(tr->obj_name, "Bad message (%d bytes from %s:%d)", msg_fragment_size,
		    src_addr, src_port));
	    goto finish_process_fragment;
	}

	/* Attach newly created message to rdata. */
	rdata->msg = msg;

	/* Extract Call-ID, From and To header and tags, topmost Via, and CSeq
	* header from the message.
	*/
	call_id = pjsip_msg_find_hdr( msg, PJSIP_H_CALL_ID, NULL);
	rdata->from = pjsip_msg_find_hdr( msg, PJSIP_H_FROM, NULL);
	rdata->to = pjsip_msg_find_hdr( msg, PJSIP_H_TO, NULL);
	rdata->via = pjsip_msg_find_hdr( msg, PJSIP_H_VIA, NULL);
	rdata->cseq = pjsip_msg_find_hdr( msg, PJSIP_H_CSEQ, NULL);

	if (call_id == NULL || rdata->from == NULL || rdata->to == NULL ||
	    rdata->via == NULL || rdata->cseq == NULL) 
	{
	    PJ_LOG(3,(tr->obj_name, "Bad message from %s:%d: missing some header", 
		    src_addr, src_port));
	    goto finish_process_fragment;
	}
	rdata->call_id = call_id->id;
	rdata->from_tag = rdata->from->tag;
	rdata->to_tag = rdata->to->tag;

	/* If message is received from address that's different from the sent-by,
  	 * MUST add received parameter to the via.
	 * In our case, we add Via receive param for EVERY received message, 
	 * because it saves us from resolving the host HERE in case sent-by is in
	 * FQDN format. And it doesn't hurt either.
	 */
	s = pj_str(src_addr);
	pj_strdup(rdata->pool, &rdata->via->recvd_param, &s);

	/* RFC 3581:
	 * If message contains "rport" param, put the received port there.
	 */
	if (rdata->via->rport_param == 0) {
	    rdata->via->rport_param = pj_sockaddr_get_port(&rdata->addr);
	}

	/* Drop response message if it has more than one Via.
	*/
	if (msg->type == PJSIP_RESPONSE_MSG) {
	    hdr = (pjsip_hdr*)rdata->via->next;
	    if (hdr) {
		hdr = pjsip_msg_find_hdr(msg, PJSIP_H_VIA, hdr);
		if (hdr) {
		    PJ_LOG(3,(tr->obj_name, "Bad message from %s:%d: "
					    "multiple Via in response message",
					    src_addr, src_port));
		    goto finish_process_fragment;
		}
	    }
	}

	/* Call the transport manager's upstream message callback.
	*/
	(*mgr->message_callback)(mgr->endpt, rdata);

finish_process_fragment:
	rdata->len -= msg_fragment_size;
	if (rdata->len > 0) {
	    pj_memmove(rdata->packet, rdata->packet+msg_fragment_size, rdata->len);
	    PJ_LOG(4,(tr->obj_name, "Processing next fragment, size=%d bytes", rdata->len));
	}

    }	/* while (rdata->len > 0) */

on_return:
    /* Reset the pool and rdata */
    rdata_pool = rdata->pool;
    pj_pool_reset(rdata_pool);
    rdata = pj_pool_alloc( rdata_pool, sizeof(*rdata) );
    rdata->len = 0;
    rdata->transport = tr;
    rdata->pool = rdata_pool;
    tr->rdata = rdata;

    /* Read the next packet. */
    rdata->addr_len = sizeof(rdata->addr);
    if (tr->type == PJSIP_TRANSPORT_UDP) {
	pj_ioqueue_recvfrom( tr->mgr->ioqueue, tr->key,
			    tr->rdata->packet, PJSIP_MAX_PKT_LEN,
			    &rdata->addr, &rdata->addr_len);
    }

#if PJ_HAS_TCP
    /* The next 'if' should have been 'else if', but we need to put the
       label inside the '#if PJ_HAS_TCP' block to avoid 'unreferenced label' warning.
     */
tcp_read_packet:
    if (tr->type == PJSIP_TRANSPORT_TCP) {
	pj_ioqueue_read( tr->mgr->ioqueue, tr->key, 
			 tr->rdata->packet + tr->rdata->len,
			 PJSIP_MAX_PKT_LEN - tr->rdata->len);
    }
#endif
}

static void transport_mgr_on_idle( pjsip_transport_mgr *mgr )
{
    pj_time_val now;
    pj_hash_iterator_t itr_val;
    pj_hash_iterator_t *itr;


    /* Get time for comparing transport's close time. */
    pj_gettimeofday(&now);
    if (now.sec < mgr->next_idle_check.sec) {
	return;
    }

    /* Acquire transport manager's lock. */
    pj_mutex_lock(mgr->mutex);

    /* Update next idle check. */
    mgr->next_idle_check.sec += MGR_IDLE_CHECK_INTERVAL;

    /* Iterate all transports, and close transports that are not used for
       some periods.
     */
    itr = pjsip_transport_first(mgr, &itr_val);
    while (itr != NULL) {
	pj_hash_iterator_t *next;
	pjsip_transport_t *transport;
	
	transport = pjsip_transport_this(mgr, itr);

	next = pjsip_transport_next(mgr, itr);

	if (pj_atomic_get(transport->ref_cnt)==0 && 
	    PJ_TIME_VAL_LTE(transport->close_time, now))
	{
	    destroy_transport(mgr, transport);
	}

	itr = next;
    }

    /* Release transport manager's lock. */
    pj_mutex_unlock(mgr->mutex);
}

static void on_ioqueue_read(pj_ioqueue_key_t *key, pj_ssize_t bytes_read)
{
    pjsip_transport_t *t;
    t = pj_ioqueue_get_user_data(key);

    handle_received_data( t->mgr, t, bytes_read );
}

static void on_ioqueue_write(pj_ioqueue_key_t *key, pj_ssize_t bytes_sent)
{
    PJ_UNUSED_ARG(key)
    PJ_UNUSED_ARG(bytes_sent)

    /* Completion of write operation. 
     * Do nothing.
     */
}

static void on_ioqueue_accept(pj_ioqueue_key_t *key, int status)
{
#if PJ_HAS_TCP
    pjsip_transport_t *t;
    t = pj_ioqueue_get_user_data(key);

    handle_new_connection( t->mgr, t, status );
#else
    PJ_UNUSED_ARG(key)
    PJ_UNUSED_ARG(status)
#endif
}

static void on_ioqueue_connect(pj_ioqueue_key_t *key, int status)
{
#if PJ_HAS_TCP
    pjsip_transport_t *t;
    t = pj_ioqueue_get_user_data(key);

    handle_connect_completion( t->mgr, t, status);
#else
    PJ_UNUSED_ARG(key)
    PJ_UNUSED_ARG(status)
#endif
}


/*
 * Poll for events.
 */
PJ_DEF(int) pjsip_transport_mgr_handle_events( pjsip_transport_mgr *mgr,
					       const pj_time_val *req_timeout )
{
    int event_count;
    int break_loop;
    int result;
    pj_time_val timeout;

    PJ_LOG(5, (LOG_TRANSPORT_MGR, "pjsip_transport_mgr_handle_events()"));

    event_count = 0;
    break_loop = 0;
    timeout = *req_timeout;
    do {
	result = pj_ioqueue_poll( mgr->ioqueue, &timeout);
	if (result == 1) {
	    ++event_count;

	    /* Break the loop. */
	    //if (timeout.msec==0 && timeout.sec==0) {
	    	break_loop = 1;
	    //}

	} else {
	    /* On idle, cleanup transport. */
	    transport_mgr_on_idle(mgr);

	    break_loop = 1;
	}
	timeout.sec = timeout.msec = 0;
    } while (!break_loop);

    return event_count;
}


PJ_DEF(pj_hash_iterator_t*) pjsip_transport_first( pjsip_transport_mgr *mgr,
						   pj_hash_iterator_t *it )
{
    return pj_hash_first(mgr->transport_table, it);
}

PJ_DEF(pj_hash_iterator_t*) pjsip_transport_next( pjsip_transport_mgr *mgr,
						  pj_hash_iterator_t *itr )
{
    return pj_hash_next(mgr->transport_table, itr);
}

PJ_DEF(pjsip_transport_t*) pjsip_transport_this( pjsip_transport_mgr *mgr,
						 pj_hash_iterator_t *itr )
{
    return pj_hash_this(mgr->transport_table, itr);
}
