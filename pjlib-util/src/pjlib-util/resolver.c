/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pjlib-util/resolver.h>
#include <pjlib-util/errno.h>
#include <pj/compat/socket.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/except.h>
#include <pj/hash.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/pool_buf.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/sock.h>
#include <pj/timer.h>


#define THIS_FILE           "resolver.c"


/* Check that maximum DNS nameservers is not too large. 
 * This has got todo with the datatype to index the nameserver in the query.
 */
#if PJ_DNS_RESOLVER_MAX_NS > 256
#   error "PJ_DNS_RESOLVER_MAX_NS is too large (max=256)"
#endif


#define RES_HASH_TABLE_SIZE 127         /**< Hash table size (must be 2^n-1 */
#define PORT                53          /**< Default NS port.               */
#define Q_HASH_TABLE_SIZE   127         /**< Query hash table size          */
#define TIMER_SIZE          127         /**< Initial number of timers.      */
#define MAX_FD              3           /**< Maximum internal sockets.      */

#define RES_BUF_SZ          PJ_DNS_RESOLVER_RES_BUF_SIZE
#define UDPSZ               PJ_DNS_RESOLVER_MAX_UDP_SIZE
#define TMP_SZ              PJ_DNS_RESOLVER_TMP_BUF_SIZE


/* Nameserver state */
enum ns_state
{
    STATE_PROBING,
    STATE_ACTIVE,
    STATE_BAD,
};

static const char *state_names[3] =
{
    "Probing",
    "Active",
    "Bad"
};


/* 
 * Each nameserver entry.
 * A name server is identified by its socket address (IP and port).
 * Each NS will have a flag to indicate whether it's properly functioning.
 */
struct nameserver
{
    pj_sockaddr     addr;               /**< Server address.                */

    enum ns_state   state;              /**< Nameserver state.              */
    pj_time_val     state_expiry;       /**< Time set next state.           */
    pj_time_val     rt_delay;           /**< Response time.                 */
    

    /* For calculating rt_delay: */
    pj_uint16_t     q_id;               /**< Query ID.                      */
    pj_time_val     sent_time;          /**< Time this query is sent.       */
};


/* Child query list head 
 * See comments on pj_dns_async_query below.
 */
struct query_head
{
    PJ_DECL_LIST_MEMBER(pj_dns_async_query);
};


/* Key to look for outstanding query and/or cached response */
struct res_key
{
    pj_uint16_t              qtype;                 /**< Query type.        */
    char                     name[PJ_MAX_HOSTNAME]; /**< Name being queried */
};


/* 
 * This represents each asynchronous query entry.
 * This entry will be put in two hash tables, the first one keyed on the DNS 
 * transaction ID to match response with the query, and the second one keyed
 * on "res_key" structure above to match a new request against outstanding 
 * requests.
 *
 * An asynchronous entry may have child entries; child entries are subsequent
 * queries to the same resource while there is pending query on the same
 * DNS resource name and type. When a query has child entries, once the
 * response is received (or error occurs), the response will trigger callback
 * invocations for all childs entries.
 *
 * Note: when application cancels the query, the callback member will be
 *       set to NULL, but for simplicity, the query will be let running.
 */
struct pj_dns_async_query
{
    PJ_DECL_LIST_MEMBER(pj_dns_async_query);    /**< List member.           */

    pj_dns_resolver     *resolver;      /**< The resolver instance.         */
    pj_uint16_t          id;            /**< Transaction ID.                */

    unsigned             transmit_cnt;  /**< Number of transmissions.       */

    struct res_key       key;           /**< Key to index this query.       */
    pj_hash_entry_buf    hbufid;        /**< Hash buffer 1                  */
    pj_hash_entry_buf    hbufkey;       /**< Hash buffer 2                  */
    pj_timer_entry       timer_entry;   /**< Timer to manage timeouts       */
    unsigned             options;       /**< Query options.                 */
    void                *user_data;     /**< Application data.              */
    pj_dns_callback     *cb;            /**< Callback to be called.         */
    struct query_head    child_head;    /**< Child queries list head.       */
};


/* This structure is used to keep cached response entry.
 * The cache is a hash table keyed on "res_key" structure above.
 */
struct cached_res
{
    PJ_DECL_LIST_MEMBER(struct cached_res);

    pj_pool_t               *pool;          /**< Cache's pool.              */
    struct res_key           key;           /**< Resource key.              */
    pj_hash_entry_buf        hbuf;          /**< Hash buffer                */
    pj_time_val              expiry_time;   /**< Expiration time.           */
    pj_dns_parsed_packet    *pkt;           /**< The response packet.       */
    unsigned                 ref_cnt;       /**< Reference counter.         */
};


/* Resolver entry */
struct pj_dns_resolver
{
    pj_str_t             name;          /**< Resolver instance name for id. */

    /* Internals */
    pj_pool_t           *pool;          /**< Internal pool.                 */
    pj_grp_lock_t       *grp_lock;      /**< Group lock protection.         */
    pj_bool_t            own_timer;     /**< Do we own timer?               */
    pj_timer_heap_t     *timer;         /**< Timer instance.                */
    pj_bool_t            own_ioqueue;   /**< Do we own ioqueue?             */
    pj_ioqueue_t        *ioqueue;       /**< Ioqueue instance.              */
    char                 tmp_pool[TMP_SZ];/**< Temporary pool buffer.       */

    /* Socket */
    pj_sock_t            udp_sock;      /**< UDP socket.                    */
    pj_ioqueue_key_t    *udp_key;       /**< UDP socket ioqueue key.        */
    unsigned char        udp_rx_pkt[UDPSZ];/**< UDP receive buffer.         */
    unsigned char        udp_tx_pkt[UDPSZ];/**< UDP transmit buffer.        */
    pj_ioqueue_op_key_t  udp_op_rx_key; /**< UDP read operation key.        */
    pj_ioqueue_op_key_t  udp_op_tx_key; /**< UDP write operation key.       */
    pj_sockaddr          udp_src_addr;  /**< Source address of packet       */
    int                  udp_addr_len;  /**< Source address length.         */

#if PJ_HAS_IPV6
    /* IPv6 socket */
    pj_sock_t            udp6_sock;     /**< UDP socket.                    */
    pj_ioqueue_key_t    *udp6_key;      /**< UDP socket ioqueue key.        */
    unsigned char        udp6_rx_pkt[UDPSZ];/**< UDP receive buffer.        */
    //unsigned char      udp6_tx_pkt[UDPSZ];/**< UDP transmit buffer.       */
    pj_ioqueue_op_key_t  udp6_op_rx_key;/**< UDP read operation key.        */
    pj_ioqueue_op_key_t  udp6_op_tx_key;/**< UDP write operation key.       */
    pj_sockaddr          udp6_src_addr; /**< Source address of packet       */
    int                  udp6_addr_len; /**< Source address length.         */
#endif

    /* Settings */
    pj_dns_settings      settings;      /**< Resolver settings.             */

    /* Nameservers */
    unsigned             ns_count;      /**< Number of name servers.        */
    struct nameserver    ns[PJ_DNS_RESOLVER_MAX_NS];    /**< Array of NS.   */

    /* Last DNS transaction ID used. */
    pj_uint16_t          last_id;

    /* Hash table for cached response */
    pj_hash_table_t     *hrescache;     /**< Cached response in hash table  */

    /* Pending asynchronous query, hashed by transaction ID. */
    pj_hash_table_t     *hquerybyid;

    /* Pending asynchronous query, hashed by "res_key" */
    pj_hash_table_t     *hquerybyres;

    /* Query entries free list */
    struct query_head    query_free_nodes;
};


/* Callback from ioqueue when packet is received */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);

/* Callback to be called when query has timed out */
static void on_timeout( pj_timer_heap_t *timer_heap,
                        struct pj_timer_entry *entry);

/* Select which nameserver to use */
static pj_status_t select_nameservers(pj_dns_resolver *resolver,
                                      unsigned *count,
                                      unsigned servers[]);

/* Destructor */
static void dns_resolver_on_destroy(void *member);

/* Close UDP socket */
static void close_sock(pj_dns_resolver *resv)
{
    /* Close existing socket */
    if (resv->udp_key != NULL) {
        pj_ioqueue_unregister(resv->udp_key);
        resv->udp_key = NULL;
        resv->udp_sock = PJ_INVALID_SOCKET;
    } else if (resv->udp_sock != PJ_INVALID_SOCKET) {
        pj_sock_close(resv->udp_sock);
        resv->udp_sock = PJ_INVALID_SOCKET;
    }

#if PJ_HAS_IPV6
    if (resv->udp6_key != NULL) {
        pj_ioqueue_unregister(resv->udp6_key);
        resv->udp6_key = NULL;
        resv->udp6_sock = PJ_INVALID_SOCKET;
    } else if (resv->udp6_sock != PJ_INVALID_SOCKET) {
        pj_sock_close(resv->udp6_sock);
        resv->udp6_sock = PJ_INVALID_SOCKET;
    }
#endif
}


/* Initialize UDP socket */
static pj_status_t init_sock(pj_dns_resolver *resv)
{
    pj_ioqueue_callback socket_cb;
    pj_sockaddr bound_addr;
    pj_ssize_t rx_pkt_size;
    pj_status_t status;

    /* Create the UDP socket */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &resv->udp_sock);
    if (status != PJ_SUCCESS)
        return status;

    /* Bind to any address/port */
    status = pj_sock_bind_in(resv->udp_sock, 0, 0);
    if (status != PJ_SUCCESS)
        return status;

    /* Register to ioqueue */
    pj_bzero(&socket_cb, sizeof(socket_cb));
    socket_cb.on_read_complete = &on_read_complete;
    status = pj_ioqueue_register_sock2(resv->pool, resv->ioqueue,
                                       resv->udp_sock, resv->grp_lock,
                                       resv, &socket_cb, &resv->udp_key);
    if (status != PJ_SUCCESS)
        return status;

    pj_ioqueue_op_key_init(&resv->udp_op_rx_key, sizeof(resv->udp_op_rx_key));
    pj_ioqueue_op_key_init(&resv->udp_op_tx_key, sizeof(resv->udp_op_tx_key));

    /* Start asynchronous read to the UDP socket */
    rx_pkt_size = sizeof(resv->udp_rx_pkt);
    resv->udp_addr_len = sizeof(resv->udp_src_addr);
    status = pj_ioqueue_recvfrom(resv->udp_key, &resv->udp_op_rx_key,
                                 resv->udp_rx_pkt, &rx_pkt_size,
                                 PJ_IOQUEUE_ALWAYS_ASYNC,
                                 &resv->udp_src_addr, &resv->udp_addr_len);
    if (status != PJ_EPENDING)
        return status;


#if PJ_HAS_IPV6
    /* Also setup IPv6 socket */

    /* Create the UDP socket */
    status = pj_sock_socket(pj_AF_INET6(), pj_SOCK_DGRAM(), 0,
                            &resv->udp6_sock);
    if (status != PJ_SUCCESS) {
        /* Skip IPv6 socket on system without IPv6 (see ticket #1953) */
        if (status == PJ_STATUS_FROM_OS(OSERR_EAFNOSUPPORT)) {
            PJ_LOG(3,(resv->name.ptr,
                      "System does not support IPv6, resolver will "
                      "ignore any IPv6 nameservers"));
            return PJ_SUCCESS;
        }
        return status;
    }

    /* Bind to any address/port */
    pj_sockaddr_init(pj_AF_INET6(), &bound_addr, NULL, 0);
    status = pj_sock_bind(resv->udp6_sock, &bound_addr,
                          pj_sockaddr_get_len(&bound_addr));
    if (status != PJ_SUCCESS)
        return status;

    /* Register to ioqueue */
    pj_bzero(&socket_cb, sizeof(socket_cb));
    socket_cb.on_read_complete = &on_read_complete;
    status = pj_ioqueue_register_sock2(resv->pool, resv->ioqueue,
                                       resv->udp6_sock, resv->grp_lock,
                                       resv, &socket_cb, &resv->udp6_key);
    if (status != PJ_SUCCESS)
        return status;

    pj_ioqueue_op_key_init(&resv->udp6_op_rx_key,
                           sizeof(resv->udp6_op_rx_key));
    pj_ioqueue_op_key_init(&resv->udp6_op_tx_key,
                           sizeof(resv->udp6_op_tx_key));

    /* Start asynchronous read to the UDP socket */
    rx_pkt_size = sizeof(resv->udp6_rx_pkt);
    resv->udp6_addr_len = sizeof(resv->udp6_src_addr);
    status = pj_ioqueue_recvfrom(resv->udp6_key, &resv->udp6_op_rx_key,
                                 resv->udp6_rx_pkt, &rx_pkt_size,
                                 PJ_IOQUEUE_ALWAYS_ASYNC,
                                 &resv->udp6_src_addr, &resv->udp6_addr_len);
    if (status != PJ_EPENDING)
        return status;
#else
    PJ_UNUSED_ARG(bound_addr);
#endif

    return PJ_SUCCESS;
}


/* Initialize DNS settings with default values */
PJ_DEF(void) pj_dns_settings_default(pj_dns_settings *s)
{
    pj_bzero(s, sizeof(pj_dns_settings));
    s->qretr_delay = PJ_DNS_RESOLVER_QUERY_RETRANSMIT_DELAY;
    s->qretr_count = PJ_DNS_RESOLVER_QUERY_RETRANSMIT_COUNT;
    s->cache_max_ttl = PJ_DNS_RESOLVER_MAX_TTL;
    s->good_ns_ttl = PJ_DNS_RESOLVER_GOOD_NS_TTL;
    s->bad_ns_ttl = PJ_DNS_RESOLVER_BAD_NS_TTL;
}


/*
 * Create the resolver.
 */
PJ_DEF(pj_status_t) pj_dns_resolver_create( pj_pool_factory *pf,
                                            const char *name,
                                            unsigned options,
                                            pj_timer_heap_t *timer,
                                            pj_ioqueue_t *ioqueue,
                                            pj_dns_resolver **p_resolver)
{
    pj_pool_t *pool;
    pj_dns_resolver *resv;
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(pf && p_resolver, PJ_EINVAL);

    if (name == NULL)
        name = THIS_FILE;

    /* Create and initialize resolver instance */
    pool = pj_pool_create(pf, name, 4000, 4000, NULL);
    if (!pool)
        return PJ_ENOMEM;

    /* Create pool and name */
    resv = PJ_POOL_ZALLOC_T(pool, struct pj_dns_resolver);
    resv->pool = pool;
    resv->udp_sock = PJ_INVALID_SOCKET;
    pj_strdup2_with_null(pool, &resv->name, name);
    
    /* Create group lock */
    status = pj_grp_lock_create_w_handler(pool, NULL, resv,
                                          &dns_resolver_on_destroy,
                                          &resv->grp_lock); 
    if (status != PJ_SUCCESS)
        goto on_error;

    pj_grp_lock_add_ref(resv->grp_lock);

    /* Timer, ioqueue, and settings */
    resv->timer = timer;
    resv->ioqueue = ioqueue;
    resv->last_id = 1;

    pj_dns_settings_default(&resv->settings);
    resv->settings.options = options;

    /* Create the timer heap if one is not specified */
    if (resv->timer == NULL) {
        resv->own_timer = PJ_TRUE;
        status = pj_timer_heap_create(pool, TIMER_SIZE, &resv->timer);
        if (status != PJ_SUCCESS)
            goto on_error;
    }

    /* Create the ioqueue if one is not specified */
    if (resv->ioqueue == NULL) {
        resv->own_ioqueue = PJ_TRUE;
        status = pj_ioqueue_create(pool, MAX_FD, &resv->ioqueue);
        if (status != PJ_SUCCESS)
            goto on_error;
    }

    /* Response cache hash table */
    resv->hrescache = pj_hash_create(pool, RES_HASH_TABLE_SIZE);

    /* Query hash table and free list. */
    resv->hquerybyid = pj_hash_create(pool, Q_HASH_TABLE_SIZE);
    resv->hquerybyres = pj_hash_create(pool, Q_HASH_TABLE_SIZE);
    pj_list_init(&resv->query_free_nodes);

    /* Initialize the UDP socket */
    status = init_sock(resv);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Looks like everything is okay */
    *p_resolver = resv;
    return PJ_SUCCESS;

on_error:
    pj_dns_resolver_destroy(resv, PJ_FALSE);
    return status;
}


void dns_resolver_on_destroy(void *member)
{
    pj_dns_resolver *resolver = (pj_dns_resolver*)member;
    pj_pool_safe_release(&resolver->pool);
}


/*
 * Destroy DNS resolver instance.
 */
PJ_DEF(pj_status_t) pj_dns_resolver_destroy( pj_dns_resolver *resolver,
                                             pj_bool_t notify)
{
    pj_hash_iterator_t it_buf, *it;
    PJ_ASSERT_RETURN(resolver, PJ_EINVAL);

    if (notify) {
        /*
         * Notify pending queries if requested.
         */
        it = pj_hash_first(resolver->hquerybyid, &it_buf);
        while (it) {
            pj_dns_async_query *q = (pj_dns_async_query *)
                                    pj_hash_this(resolver->hquerybyid, it);
            pj_dns_async_query *cq;
            if (q->cb)
                (*q->cb)(q->user_data, PJ_ECANCELLED, NULL);

            cq = q->child_head.next;
            while (cq != (pj_dns_async_query*)&q->child_head) {
                if (cq->cb)
                    (*cq->cb)(cq->user_data, PJ_ECANCELLED, NULL);
                cq = cq->next;
            }
            it = pj_hash_next(resolver->hquerybyid, it);
        }
    }

    /* Destroy cached entries */
    it = pj_hash_first(resolver->hrescache, &it_buf);
    while (it) {
        struct cached_res *cache;

        cache = (struct cached_res*) pj_hash_this(resolver->hrescache, it);
        pj_hash_set(NULL, resolver->hrescache, &cache->key, 
                    sizeof(cache->key), 0, NULL);
        pj_pool_release(cache->pool);

        it = pj_hash_first(resolver->hrescache, &it_buf);
    }

    if (resolver->own_timer && resolver->timer) {
        pj_timer_heap_destroy(resolver->timer);
        resolver->timer = NULL;
    }

    close_sock(resolver);

    if (resolver->own_ioqueue && resolver->ioqueue) {
        pj_ioqueue_destroy(resolver->ioqueue);
        resolver->ioqueue = NULL;
    }

    pj_grp_lock_dec_ref(resolver->grp_lock);

    return PJ_SUCCESS;
}



/*
 * Configure name servers for the DNS resolver. 
 */
PJ_DEF(pj_status_t) pj_dns_resolver_set_ns( pj_dns_resolver *resolver,
                                            unsigned count,
                                            const pj_str_t servers[],
                                            const pj_uint16_t ports[])
{
    unsigned i;
    pj_time_val now;
    pj_status_t status;

    PJ_ASSERT_RETURN(resolver && count && servers, PJ_EINVAL);
    PJ_ASSERT_RETURN(count <= PJ_DNS_RESOLVER_MAX_NS, PJ_ETOOMANY);

    pj_grp_lock_acquire(resolver->grp_lock);

    resolver->ns_count = 0;
    pj_bzero(resolver->ns, sizeof(resolver->ns));

    pj_gettimeofday(&now);

    for (i=0; i<count; ++i) {
        struct nameserver *ns = &resolver->ns[i];

        status = pj_sockaddr_init(pj_AF_INET(), &ns->addr, &servers[i], 
                                  (pj_uint16_t)(ports ? ports[i] : PORT));
        if (status != PJ_SUCCESS)
            status = pj_sockaddr_init(pj_AF_INET6(), &ns->addr, &servers[i], 
                                      (pj_uint16_t)(ports ? ports[i] : PORT));
        if (status != PJ_SUCCESS) {
            pj_grp_lock_release(resolver->grp_lock);
            return PJLIB_UTIL_EDNSINNSADDR;
        }

        ns->state = STATE_ACTIVE;
        ns->state_expiry = now;
        ns->rt_delay.sec = 10;
    }
    
    resolver->ns_count = count;

    pj_grp_lock_release(resolver->grp_lock);
    return PJ_SUCCESS;
}



/*
 * Modify the resolver settings.
 */
PJ_DEF(pj_status_t) pj_dns_resolver_set_settings(pj_dns_resolver *resolver,
                                                 const pj_dns_settings *st)
{
    PJ_ASSERT_RETURN(resolver && st, PJ_EINVAL);

    pj_grp_lock_acquire(resolver->grp_lock);
    pj_memcpy(&resolver->settings, st, sizeof(*st));
    pj_grp_lock_release(resolver->grp_lock);
    return PJ_SUCCESS;
}


/*
 * Get the resolver current settings.
 */
PJ_DEF(pj_status_t) pj_dns_resolver_get_settings( pj_dns_resolver *resolver,
                                                  pj_dns_settings *st)
{
    PJ_ASSERT_RETURN(resolver && st, PJ_EINVAL);

    pj_grp_lock_acquire(resolver->grp_lock);
    pj_memcpy(st, &resolver->settings, sizeof(*st));
    pj_grp_lock_release(resolver->grp_lock);
    return PJ_SUCCESS;
}


/*
 * Poll for events from the resolver. 
 */
PJ_DEF(void) pj_dns_resolver_handle_events(pj_dns_resolver *resolver,
                                           const pj_time_val *timeout)
{
    PJ_ASSERT_ON_FAIL(resolver, return);

    pj_grp_lock_acquire(resolver->grp_lock);
    pj_timer_heap_poll(resolver->timer, NULL);
    pj_grp_lock_release(resolver->grp_lock);

    pj_ioqueue_poll(resolver->ioqueue, timeout);
}


/* Get one query node from the free node, if any, or allocate 
 * a new one.
 */
static pj_dns_async_query *alloc_qnode(pj_dns_resolver *resolver,
                                       unsigned options,
                                       void *user_data,
                                       pj_dns_callback *cb)
{
    pj_dns_async_query *q;

    /* Merge query options with resolver options */
    options |= resolver->settings.options;

    if (!pj_list_empty(&resolver->query_free_nodes)) {
        q = resolver->query_free_nodes.next;
        pj_list_erase(q);
        pj_bzero(q, sizeof(*q));
    } else {
        q = PJ_POOL_ZALLOC_T(resolver->pool, pj_dns_async_query);
    }

    /* Init query */
    q->resolver = resolver;
    q->options = options;
    q->user_data = user_data;
    q->cb = cb;
    pj_list_init(&q->child_head);

    return q;
}


/*
 * Transmit query.
 */
static pj_status_t transmit_query(pj_dns_resolver *resolver,
                                  pj_dns_async_query *q)
{
    unsigned pkt_size;
    unsigned i, server_cnt, send_cnt;
    unsigned servers[PJ_DNS_RESOLVER_MAX_NS];
    pj_time_val now;
    pj_str_t name;
    pj_time_val delay;
    pj_status_t status;

    /* Select which nameserver(s) to send requests to. */
    server_cnt = PJ_ARRAY_SIZE(servers);
    status = select_nameservers(resolver, &server_cnt, servers);
    if (status != PJ_SUCCESS) {
        return status;
    }

    if (server_cnt == 0) {
        return PJLIB_UTIL_EDNSNOWORKINGNS;
    }

    /* Start retransmit/timeout timer for the query */
    pj_assert(q->timer_entry.id == 0);
    q->timer_entry.id = 1;
    q->timer_entry.user_data = q;
    q->timer_entry.cb = &on_timeout;

    delay.sec = 0;
    delay.msec = resolver->settings.qretr_delay;
    pj_time_val_normalize(&delay);
    status = pj_timer_heap_schedule_w_grp_lock(resolver->timer,
                                               &q->timer_entry,
                                               &delay, 1,
                                               resolver->grp_lock);
    if (status != PJ_SUCCESS) {
        return status;
    }

    /* Check if the socket is available for sending */
    if (pj_ioqueue_is_pending(resolver->udp_key, &resolver->udp_op_tx_key)
#if PJ_HAS_IPV6
        || (resolver->udp6_key &&
            pj_ioqueue_is_pending(resolver->udp6_key,
                                  &resolver->udp6_op_tx_key))
#endif
        )
    {
        ++q->transmit_cnt;
        PJ_LOG(4,(resolver->name.ptr,
                  "Socket busy in transmitting DNS %s query for %s%s",
                  pj_dns_get_type_name(q->key.qtype),
                  q->key.name,
                  (q->transmit_cnt < resolver->settings.qretr_count?
                   ", will try again later":"")));
        return PJ_SUCCESS;
    }

    /* Create DNS query packet */
    pkt_size = sizeof(resolver->udp_tx_pkt);
    name = pj_str(q->key.name);
    status = pj_dns_make_query(resolver->udp_tx_pkt, &pkt_size,
                               q->id, q->key.qtype, &name);
    if (status != PJ_SUCCESS) {
        pj_timer_heap_cancel(resolver->timer, &q->timer_entry);
        return status;
    }

    /* Get current time. */
    pj_gettimeofday(&now);

    /* Send the packet to name servers */
    send_cnt = 0;
    for (i=0; i<server_cnt; ++i) {
        char addr[PJ_INET6_ADDRSTRLEN];
        pj_ssize_t sent  = (pj_ssize_t) pkt_size;
        struct nameserver *ns = &resolver->ns[servers[i]];

        if (ns->addr.addr.sa_family == pj_AF_INET()) {
            status = pj_ioqueue_sendto(resolver->udp_key,
                                       &resolver->udp_op_tx_key,
                                       resolver->udp_tx_pkt, &sent, 0,
                                       &ns->addr,
                                       pj_sockaddr_get_len(&ns->addr));
            if (status == PJ_SUCCESS || status == PJ_EPENDING)
                send_cnt++;
        }
#if PJ_HAS_IPV6
        else if (resolver->udp6_key) {
            status = pj_ioqueue_sendto(resolver->udp6_key,
                                       &resolver->udp6_op_tx_key,
                                       resolver->udp_tx_pkt, &sent, 0,
                                       &ns->addr,
                                       pj_sockaddr_get_len(&ns->addr));
            if (status == PJ_SUCCESS || status == PJ_EPENDING)
                send_cnt++;
        }
#endif
        else {
            continue;
        }

        PJ_PERROR(4,(resolver->name.ptr, status,
                  "%s %d bytes to NS %d (%s:%d): DNS %s query for %s",
                  (q->transmit_cnt==0? "Transmitting":"Re-transmitting"),
                  (int)pkt_size, servers[i],
                  pj_sockaddr_print(&ns->addr, addr, sizeof(addr), 2),
                  pj_sockaddr_get_port(&ns->addr),
                  pj_dns_get_type_name(q->key.qtype), 
                  q->key.name));

        if (ns->q_id == 0) {
            ns->q_id = q->id;
            ns->sent_time = now;
        }
    }

    if (send_cnt == 0) {
        pj_timer_heap_cancel(resolver->timer, &q->timer_entry);
        return PJLIB_UTIL_EDNSNOWORKINGNS;
    }

    ++q->transmit_cnt;

    return PJ_SUCCESS;
}


/*
 * Initialize resource key for hash table lookup.
 */
static void init_res_key(struct res_key *key, int type, const pj_str_t *name)
{
    unsigned i;
    pj_size_t len;
    char *dst = key->name;
    const char *src = name->ptr;

    pj_bzero(key, sizeof(struct res_key));
    key->qtype = (pj_uint16_t)type;

    len = name->slen;
    if (len >= PJ_MAX_HOSTNAME) len = PJ_MAX_HOSTNAME - 1;

    /* Copy key, in lowercase */
    for (i=0; i<len; ++i) {
        *dst++ = (char)pj_tolower(*src++);
    }
}


/* Allocate new cache entry */
static struct cached_res *alloc_entry(pj_dns_resolver *resolver)
{
    pj_pool_t *pool;
    struct cached_res *cache;

    pool = pj_pool_create(resolver->pool->factory, "dnscache",
                          RES_BUF_SZ, 256, NULL);
    cache = PJ_POOL_ZALLOC_T(pool, struct cached_res);
    cache->pool = pool;
    cache->ref_cnt = 1;

    return cache;
}

/* Re-allocate cache entry, to free cached packet */
static void reset_entry(struct cached_res **p_cached)
{
    pj_pool_t *pool;
    struct cached_res *cache = *p_cached;
    unsigned ref_cnt;

    pool = cache->pool;
    ref_cnt = cache->ref_cnt;

    pj_pool_reset(pool);

    cache = PJ_POOL_ZALLOC_T(pool, struct cached_res);
    cache->pool = pool;
    cache->ref_cnt = ref_cnt;
    *p_cached = cache;
}

/* Put unused/expired cached entry to the free list */
static void free_entry(pj_dns_resolver *resolver, struct cached_res *cache)
{
    PJ_UNUSED_ARG(resolver);
    pj_pool_release(cache->pool);
}


/*
 * Create and start asynchronous DNS query for a single resource.
 */
PJ_DEF(pj_status_t) pj_dns_resolver_start_query( pj_dns_resolver *resolver,
                                                 const pj_str_t *name,
                                                 int type,
                                                 unsigned options,
                                                 pj_dns_callback *cb,
                                                 void *user_data,
                                                 pj_dns_async_query **p_query)
{
    pj_time_val now;
    struct res_key key;
    struct cached_res *cache;
    pj_dns_async_query *q, *p_q = NULL;
    pj_uint32_t hval;
    pj_status_t status = PJ_SUCCESS;

    /* Validate arguments */
    PJ_ASSERT_RETURN(resolver && name && type, PJ_EINVAL);

    /* Check name is not too long. */
    PJ_ASSERT_RETURN(name->slen>0 && name->slen < PJ_MAX_HOSTNAME,
                     PJ_ENAMETOOLONG);

    /* Check type */
    PJ_ASSERT_RETURN(type > 0 && type < 0xFFFF, PJ_EINVAL);

    /* Build resource key for looking up hash tables */
    init_res_key(&key, type, name);

    /* Start working with the resolver */
    pj_grp_lock_acquire(resolver->grp_lock);

    /* Get current time. */
    pj_gettimeofday(&now);

    /* First, check if we have cached response for the specified name/type,
     * and the cached entry has not expired.
     */
    hval = 0;
    cache = (struct cached_res *) pj_hash_get(resolver->hrescache, &key, 
                                              sizeof(key), &hval);
    if (cache) {
        /* We've found a cached entry. */

        /* Check for expiration */
        if (PJ_TIME_VAL_GT(cache->expiry_time, now)) {

            /* Log */
            PJ_LOG(5,(resolver->name.ptr, 
                      "Picked up DNS %s record for %.*s from cache, ttl=%d",
                      pj_dns_get_type_name(type),
                      (int)name->slen, name->ptr,
                      (int)(cache->expiry_time.sec - now.sec)));

            /* Map DNS Rcode in the response into PJLIB status name space */
            status = PJ_DNS_GET_RCODE(cache->pkt->hdr.flags);
            status = PJ_STATUS_FROM_DNS_RCODE(status);

            /* Workaround for deadlock problem. Need to increment the cache's
             * ref counter first before releasing mutex, so the cache won't be
             * destroyed by other thread while in callback.
             */
            cache->ref_cnt++;
            pj_grp_lock_release(resolver->grp_lock);

            /* This cached response is still valid. Just return this
             * response to caller.
             */
            if (cb) {
                (*cb)(user_data, status, cache->pkt);
            }

            /* Done. No host resolution is necessary */
            pj_grp_lock_acquire(resolver->grp_lock);

            /* Decrement the ref counter. Also check if it is time to free
             * the cache (as it has been expired).
             */
            cache->ref_cnt--;
            if (cache->ref_cnt <= 0)
                free_entry(resolver, cache);

            /* Must return PJ_SUCCESS */
            status = PJ_SUCCESS;

            /*
             * We cannot write to *p_query after calling cb because what
             * p_query points to may have been freed by cb.
             * Refer to ticket #1974.
             */
            pj_grp_lock_release(resolver->grp_lock);
            return status;
        }

        /* At this point, we have a cached entry, but this entry has expired.
         * Remove this entry from the cached list.
         */
        pj_hash_set(NULL, resolver->hrescache, &key, sizeof(key), 0, NULL);

        /* Also free the cache, if it is not being used (by callback). */
        cache->ref_cnt--;
        if (cache->ref_cnt <= 0)
            free_entry(resolver, cache);

        /* Must continue with creating a query now */
    }

    /* Next, check if we have pending query on the same resource */
    q = (pj_dns_async_query *) pj_hash_get(resolver->hquerybyres, &key, 
                                           sizeof(key), NULL);
    if (q) {
        /* Yes, there's another pending query to the same key.
         * Just create a new child query and add this query to
         * pending query's child queries.
         */
        pj_dns_async_query *nq;

        nq = alloc_qnode(resolver, options, user_data, cb);
        pj_list_push_back(&q->child_head, nq);

        /* Done. This child query will be notified once the "parent"
         * query completes.
         */
        p_q = nq;
        status = PJ_SUCCESS;
        goto on_return;
    } 

    /* There's no pending query to the same key, initiate a new one. */
    q = alloc_qnode(resolver, options, user_data, cb);

    /* Save the ID and key */
    /* TODO: dnsext-forgery-resilient: randomize id for security */
    q->id = resolver->last_id++;
    if (resolver->last_id == 0)
        resolver->last_id = 1;
    pj_memcpy(&q->key, &key, sizeof(struct res_key));

    /* Send the query */
    status = transmit_query(resolver, q);
    if (status != PJ_SUCCESS) {
        pj_list_push_back(&resolver->query_free_nodes, q);
        goto on_return;
    }

    /* Add query entry to the hash tables */
    pj_hash_set_np(resolver->hquerybyid, &q->id, sizeof(q->id), 
                   0, q->hbufid, q);
    pj_hash_set_np(resolver->hquerybyres, &q->key, sizeof(q->key),
                   0, q->hbufkey, q);

    p_q = q;

on_return:
    if (p_query)
        *p_query = p_q;

    pj_grp_lock_release(resolver->grp_lock);
    return status;
}


/*
 * Cancel a pending query.
 */
PJ_DEF(pj_status_t) pj_dns_resolver_cancel_query(pj_dns_async_query *query,
                                                 pj_bool_t notify)
{
    pj_dns_callback *cb;

    PJ_ASSERT_RETURN(query, PJ_EINVAL);

    pj_grp_lock_acquire(query->resolver->grp_lock);

    if (query->timer_entry.id == 1) {
        pj_timer_heap_cancel_if_active(query->resolver->timer,
                                       &query->timer_entry, 0);
    }

    cb = query->cb;
    query->cb = NULL;

    if (notify)
        (*cb)(query->user_data, PJ_ECANCELLED, NULL);

    pj_grp_lock_release(query->resolver->grp_lock);
    return PJ_SUCCESS;
}


/* 
 * DNS response containing A packet. 
 */
PJ_DEF(pj_status_t) pj_dns_parse_a_response(const pj_dns_parsed_packet *pkt,
                                            pj_dns_a_record *rec)
{
    enum { MAX_SEARCH = 20 };
    pj_str_t hostname, alias = {NULL, 0}, *resname;
    pj_size_t bufstart = 0;
    pj_size_t bufleft = sizeof(rec->buf_);
    unsigned i, ansidx, search_cnt=0;

    PJ_ASSERT_RETURN(pkt && rec, PJ_EINVAL);

    /* Init the record */
    pj_bzero(rec, sizeof(pj_dns_a_record));

    /* Return error if there's error in the packet. */
    if (PJ_DNS_GET_RCODE(pkt->hdr.flags))
        return PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_GET_RCODE(pkt->hdr.flags));

    /* Return error if there's no query section */
    if (pkt->hdr.qdcount == 0)
        return PJLIB_UTIL_EDNSINANSWER;

    /* Return error if there's no answer */
    if (pkt->hdr.anscount == 0)
        return PJLIB_UTIL_EDNSNOANSWERREC;

    /* Get the hostname from the query. */
    hostname = pkt->q[0].name;

    /* Copy hostname to the record */
    if (hostname.slen > (int)bufleft) {
        return PJ_ENAMETOOLONG;
    }

    pj_memcpy(&rec->buf_[bufstart], hostname.ptr, hostname.slen);
    rec->name.ptr = &rec->buf_[bufstart];
    rec->name.slen = hostname.slen;

    bufstart += hostname.slen;
    bufleft -= hostname.slen;

    /* Find the first RR which name matches the hostname */
    for (ansidx=0; ansidx < pkt->hdr.anscount; ++ansidx) {
        if (pj_stricmp(&pkt->ans[ansidx].name, &hostname)==0)
            break;
    }

    if (ansidx == pkt->hdr.anscount)
        return PJLIB_UTIL_EDNSNOANSWERREC;

    resname = &hostname;

    /* Keep following CNAME records. */
    while (pkt->ans[ansidx].type == PJ_DNS_TYPE_CNAME &&
           search_cnt++ < MAX_SEARCH)
    {
        resname = &pkt->ans[ansidx].rdata.cname.name;

        if (!alias.slen)
            alias = *resname;

        for (i=0; i < pkt->hdr.anscount; ++i) {
            if (pj_stricmp(resname, &pkt->ans[i].name)==0) {
                break;
            }
        }

        if (i==pkt->hdr.anscount)
            return PJLIB_UTIL_EDNSNOANSWERREC;

        ansidx = i;
    }

    if (search_cnt >= MAX_SEARCH)
        return PJLIB_UTIL_EDNSINANSWER;

    if (pkt->ans[ansidx].type != PJ_DNS_TYPE_A)
        return PJLIB_UTIL_EDNSINANSWER;

    /* Copy alias to the record, if present. */
    if (alias.slen) {
        if (alias.slen > (int)bufleft)
            return PJ_ENAMETOOLONG;

        pj_memcpy(&rec->buf_[bufstart], alias.ptr, alias.slen);
        rec->alias.ptr = &rec->buf_[bufstart];
        rec->alias.slen = alias.slen;

        // bufstart += alias.slen;
        // bufleft -= alias.slen;
    }

    /* Get the IP addresses. */
    for (i=0; i < pkt->hdr.anscount; ++i) {
        if (pkt->ans[i].type == PJ_DNS_TYPE_A &&
            pj_stricmp(&pkt->ans[i].name, resname)==0 &&
            rec->addr_count < PJ_DNS_MAX_IP_IN_A_REC)
        {
            rec->addr[rec->addr_count++].s_addr =
                pkt->ans[i].rdata.a.ip_addr.s_addr;
        }
    }

    if (rec->addr_count == 0)
        return PJLIB_UTIL_EDNSNOANSWERREC;

    return PJ_SUCCESS;
}


/* 
 * DNS response containing A and/or AAAA packet. 
 */
PJ_DEF(pj_status_t) pj_dns_parse_addr_response(
                                            const pj_dns_parsed_packet *pkt,
                                            pj_dns_addr_record *rec)
{
    enum { MAX_SEARCH = 20 };
    pj_str_t hostname, alias = {NULL, 0}, *resname;
    pj_size_t bufstart = 0;
    pj_size_t bufleft;
    unsigned i, ansidx, cnt=0;

    PJ_ASSERT_RETURN(pkt && rec, PJ_EINVAL);

    /* Init the record */
    pj_bzero(rec, sizeof(*rec));

    bufleft = sizeof(rec->buf_);

    /* Return error if there's error in the packet. */
    if (PJ_DNS_GET_RCODE(pkt->hdr.flags))
        return PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_GET_RCODE(pkt->hdr.flags));

    /* Return error if there's no query section */
    if (pkt->hdr.qdcount == 0)
        return PJLIB_UTIL_EDNSINANSWER;

    /* Return error if there's no answer */
    if (pkt->hdr.anscount == 0)
        return PJLIB_UTIL_EDNSNOANSWERREC;

    /* Get the hostname from the query. */
    hostname = pkt->q[0].name;

    /* Copy hostname to the record */
    if (hostname.slen > (int)bufleft) {
        return PJ_ENAMETOOLONG;
    }

    pj_memcpy(&rec->buf_[bufstart], hostname.ptr, hostname.slen);
    rec->name.ptr = &rec->buf_[bufstart];
    rec->name.slen = hostname.slen;

    bufstart += hostname.slen;
    bufleft -= hostname.slen;

    /* Find the first RR which name matches the hostname. */
    for (ansidx=0; ansidx < pkt->hdr.anscount; ++ansidx) {
        if (pj_stricmp(&pkt->ans[ansidx].name, &hostname)==0)
            break;
    }

    if (ansidx == pkt->hdr.anscount)
        return PJLIB_UTIL_EDNSNOANSWERREC;

    resname = &hostname;

    /* Keep following CNAME records. */
    while (pkt->ans[ansidx].type == PJ_DNS_TYPE_CNAME &&
           cnt++ < MAX_SEARCH)
    {
        resname = &pkt->ans[ansidx].rdata.cname.name;

        if (!alias.slen)
            alias = *resname;

        for (i=0; i < pkt->hdr.anscount; ++i) {
            if (pj_stricmp(resname, &pkt->ans[i].name)==0)
                break;
        }

        if (i==pkt->hdr.anscount)
            return PJLIB_UTIL_EDNSNOANSWERREC;

        ansidx = i;
    }

    if (cnt >= MAX_SEARCH)
        return PJLIB_UTIL_EDNSINANSWER;

    if (pkt->ans[ansidx].type != PJ_DNS_TYPE_A &&
        pkt->ans[ansidx].type != PJ_DNS_TYPE_AAAA)
    {
        return PJLIB_UTIL_EDNSINANSWER;
    }

    /* Copy alias to the record, if present. */
    if (alias.slen) {
        if (alias.slen > (int)bufleft)
            return PJ_ENAMETOOLONG;

        pj_memcpy(&rec->buf_[bufstart], alias.ptr, alias.slen);
        rec->alias.ptr = &rec->buf_[bufstart];
        rec->alias.slen = alias.slen;

        // bufstart += alias.slen;
        // bufleft -= alias.slen;
    }

    /* Get the IP addresses. */
    cnt = 0;
    for (i=0; i < pkt->hdr.anscount && cnt < PJ_DNS_MAX_IP_IN_A_REC ; ++i) {
        if ((pkt->ans[i].type == PJ_DNS_TYPE_A ||
             pkt->ans[i].type == PJ_DNS_TYPE_AAAA) &&
            pj_stricmp(&pkt->ans[i].name, resname)==0)
        {
            if (pkt->ans[i].type == PJ_DNS_TYPE_A) {
                rec->addr[cnt].af = pj_AF_INET();
                rec->addr[cnt].ip.v4 = pkt->ans[i].rdata.a.ip_addr;
            } else {
                rec->addr[cnt].af = pj_AF_INET6();
                rec->addr[cnt].ip.v6 = pkt->ans[i].rdata.aaaa.ip_addr;
            }
            ++cnt;
        }
    }
    rec->addr_count = cnt;

    if (cnt == 0)
        return PJLIB_UTIL_EDNSNOANSWERREC;

    return PJ_SUCCESS;
}


/* Set nameserver state */
static void set_nameserver_state(pj_dns_resolver *resolver,
                                 unsigned index,
                                 enum ns_state state,
                                 const pj_time_val *now)
{
    struct nameserver *ns = &resolver->ns[index];
    enum ns_state old_state = ns->state;
    char addr[PJ_INET6_ADDRSTRLEN];

    ns->state = state;
    ns->state_expiry = *now;

    if (state == STATE_PROBING)
        ns->state_expiry.sec += ((resolver->settings.qretr_count + 2) *
                                 resolver->settings.qretr_delay) / 1000;
    else if (state == STATE_ACTIVE)
        ns->state_expiry.sec += resolver->settings.good_ns_ttl;
    else
        ns->state_expiry.sec += resolver->settings.bad_ns_ttl;

    PJ_LOG(5, (resolver->name.ptr, "Nameserver %s:%d state changed %s --> %s",
               pj_sockaddr_print(&ns->addr, addr, sizeof(addr), 2),
               pj_sockaddr_get_port(&ns->addr),
               state_names[old_state], state_names[state]));
}


/* Select which nameserver(s) to use. Note this may return multiple
 * name servers. The algorithm to select which nameservers to be
 * sent the request to is as follows:
 *  - select the first nameserver that is known to be good for the
 *    last PJ_DNS_RESOLVER_GOOD_NS_TTL interval.
 *  - for all NSes, if last_known_good >= PJ_DNS_RESOLVER_GOOD_NS_TTL, 
 *    include the NS to re-check again that the server is still good,
 *    unless the NS is known to be bad in the last PJ_DNS_RESOLVER_BAD_NS_TTL
 *    interval.
 *  - for all NSes, if last_known_bad >= PJ_DNS_RESOLVER_BAD_NS_TTL, 
 *    also include the NS to re-check again that the server is still bad.
 */
static pj_status_t select_nameservers(pj_dns_resolver *resolver,
                                      unsigned *count,
                                      unsigned servers[])
{
    unsigned i, max_count=*count;
    int min;
    pj_time_val now;

    pj_assert(max_count > 0);

    *count = 0;
    servers[0] = 0xFFFF;

    /* Check that nameservers are configured. */
    if (resolver->ns_count == 0)
        return PJLIB_UTIL_EDNSNONS;

    pj_gettimeofday(&now);

    /* Select one Active nameserver with best response time. */
    for (min=-1, i=0; i<resolver->ns_count; ++i) {
        struct nameserver *ns = &resolver->ns[i];

        if (ns->state != STATE_ACTIVE)
            continue;

        if (min == -1)
            min = i;
        else if (PJ_TIME_VAL_LT(ns->rt_delay, resolver->ns[min].rt_delay))
            min = i;
    }
    if (min != -1) {
        servers[0] = min;
        ++(*count);
    }

    /* Scan nameservers. */
    for (i=0; i<resolver->ns_count && *count < max_count; ++i) {
        struct nameserver *ns = &resolver->ns[i];

        if (PJ_TIME_VAL_LTE(ns->state_expiry, now)) {
            if (ns->state == STATE_PROBING) {
                set_nameserver_state(resolver, i, STATE_BAD, &now);
            } else {
                set_nameserver_state(resolver, i, STATE_PROBING, &now);
                if ((int)i != min) {
                    servers[*count] = i;
                    ++(*count);
                }
            }
        } else if (ns->state == STATE_PROBING && (int)i != min) {
            servers[*count] = i;
            ++(*count);
        }
    }

    return PJ_SUCCESS;
}


/* Update name server status */
static void report_nameserver_status(pj_dns_resolver *resolver,
                                     const pj_sockaddr *ns_addr,
                                     const pj_dns_parsed_packet *pkt)
{
    unsigned i;
    int rcode;
    pj_uint32_t q_id;
    pj_time_val now;
    pj_bool_t is_good;

    /* Only mark nameserver as "bad" if it returned non-parseable response or
     * it returned the following status codes
     */
    if (pkt) {
        rcode = PJ_DNS_GET_RCODE(pkt->hdr.flags);
        q_id = pkt->hdr.id;
    } else {
        rcode = 0;
        q_id = (pj_uint32_t)-1;
    }

    /* Some nameserver is reported to respond with PJ_DNS_RCODE_SERVFAIL for
     * missing AAAA record, and the standard doesn't seem to specify that
     * SERVFAIL should prevent the server to be contacted again for other
     * queries. So let's not mark nameserver as bad for SERVFAIL response.
     */
    if (!pkt || /* rcode == PJ_DNS_RCODE_SERVFAIL || */
                rcode == PJ_DNS_RCODE_REFUSED ||
                rcode == PJ_DNS_RCODE_NOTAUTH) 
    {
        is_good = PJ_FALSE;
    } else {
        is_good = PJ_TRUE;
    }


    /* Mark time */
    pj_gettimeofday(&now);

    /* Recheck all nameservers. */
    for (i=0; i<resolver->ns_count; ++i) {
        struct nameserver *ns = &resolver->ns[i];

        if (pj_sockaddr_cmp(&ns->addr, ns_addr) == 0) {
            if (q_id == ns->q_id) {
                /* Calculate response time */
                pj_time_val rt = now;
                PJ_TIME_VAL_SUB(rt, ns->sent_time);
                ns->rt_delay = rt;
                ns->q_id = 0;
            }
            set_nameserver_state(resolver, i, 
                                 (is_good ? STATE_ACTIVE : STATE_BAD), &now);
            break;
        }
    }
}


/* Update response cache */
static void update_res_cache(pj_dns_resolver *resolver,
                             const struct res_key *key,
                             pj_status_t status,
                             pj_bool_t set_expiry,
                             const pj_dns_parsed_packet *pkt)
{
    struct cached_res *cache;
    pj_uint32_t hval=0, ttl;

    /* If status is unsuccessful, clear the same entry from the cache */
    if (status != PJ_SUCCESS) {
        cache = (struct cached_res *) pj_hash_get(resolver->hrescache, key, 
                                                  sizeof(*key), &hval);
        /* Remove the entry before releasing its pool (see ticket #1710) */
        pj_hash_set(NULL, resolver->hrescache, key, sizeof(*key), hval, NULL);
        
        /* Free the entry */
        if (cache && --cache->ref_cnt <= 0)
            free_entry(resolver, cache);
    }


    /* Calculate expiration time. */
    if (set_expiry) {
        if (pkt->hdr.anscount == 0 || status != PJ_SUCCESS) {
            /* If we don't have answers for the name, then give a different
             * ttl value (note: PJ_DNS_RESOLVER_INVALID_TTL may be zero, 
             * which means that invalid names won't be kept in the cache)
             */
            ttl = PJ_DNS_RESOLVER_INVALID_TTL;

        } else {
            /* Otherwise get the minimum TTL from the answers */
            unsigned i;
            ttl = 0xFFFFFFFF;
            for (i=0; i<pkt->hdr.anscount; ++i) {
                if (pkt->ans[i].ttl < ttl)
                    ttl = pkt->ans[i].ttl;
            }
        }
    } else {
        ttl = 0xFFFFFFFF;
    }

    /* Apply maximum TTL */
    if (ttl > resolver->settings.cache_max_ttl)
        ttl = resolver->settings.cache_max_ttl;

    /* Get a cache response entry */
    cache = (struct cached_res *) pj_hash_get(resolver->hrescache, key,
                                              sizeof(*key), &hval);

    /* If TTL is zero, clear the same entry in the hash table */
    if (ttl == 0) {
        /* Remove the entry before releasing its pool (see ticket #1710) */
        pj_hash_set(NULL, resolver->hrescache, key, sizeof(*key), hval, NULL);

        /* Free the entry */
        if (cache && --cache->ref_cnt <= 0)
            free_entry(resolver, cache);
        return;
    }

    if (cache == NULL) {
        cache = alloc_entry(resolver);
    } else {
        /* Remove the entry before resetting its pool (see ticket #1710) */
        pj_hash_set(NULL, resolver->hrescache, key, sizeof(*key), hval, NULL);

        if (cache->ref_cnt > 1) {
            /* When cache entry is being used by callback (to app),
             * just decrement ref_cnt so it will be freed after
             * the callback returns and allocate new entry.
             */
            cache->ref_cnt--;
            cache = alloc_entry(resolver);
        } else {
            /* Reset cache to avoid bloated cache pool */
            reset_entry(&cache);
        }
    }

    /* Duplicate the packet.
     * We don't need to keep the NS and AR sections from the packet,
     * so exclude from duplication. We do need to keep the Query
     * section since DNS A parser needs the query section to know
     * the name being requested.
     */
    pj_dns_packet_dup(cache->pool, pkt, 
                      PJ_DNS_NO_NS | PJ_DNS_NO_AR,
                      &cache->pkt);

    /* Calculate expiration time */
    if (set_expiry) {
        pj_gettimeofday(&cache->expiry_time);
        cache->expiry_time.sec += ttl;
    } else {
        cache->expiry_time.sec = 0x7FFFFFFFL;
        cache->expiry_time.msec = 0;
    }

    /* Copy key to the cached response */
    pj_memcpy(&cache->key, key, sizeof(*key));

    /* Update the hash table */
    pj_hash_set_np(resolver->hrescache, &cache->key, sizeof(*key), hval,
                   cache->hbuf, cache);

}


/* Callback to be called when query has timed out */
static void on_timeout( pj_timer_heap_t *timer_heap,
                        struct pj_timer_entry *entry)
{
    pj_dns_resolver *resolver;
    pj_dns_async_query *q, *cq;
    pj_status_t status;

    PJ_UNUSED_ARG(timer_heap);

    q = (pj_dns_async_query *) entry->user_data;
    resolver = q->resolver;

    pj_grp_lock_acquire(resolver->grp_lock);

    /* Recheck that this query is still pending, since there is a slight
     * possibility of race condition (timer elapsed while at the same time
     * response arrives)
     */
    if (pj_hash_get(resolver->hquerybyid, &q->id, sizeof(q->id), NULL)==NULL) {
        /* Yeah, this query is done. */
        pj_grp_lock_release(resolver->grp_lock);
        return;
    }

    /* Invalidate id. */
    q->timer_entry.id = 0;

    /* Check to see if we should retransmit instead of time out */
    if (q->transmit_cnt < resolver->settings.qretr_count) {
        status = transmit_query(resolver, q);
        if (status == PJ_SUCCESS) {
            pj_grp_lock_release(resolver->grp_lock);
            return;
        } else {
            /* Error occurs */
            PJ_PERROR(4,(resolver->name.ptr, status,
                         "Error transmitting request"));

            /* Let it fallback to timeout section below */
        }
    }

    /* Clear hash table entries */
    pj_hash_set(NULL, resolver->hquerybyid, &q->id, sizeof(q->id), 0, NULL);
    pj_hash_set(NULL, resolver->hquerybyres, &q->key, sizeof(q->key), 0, NULL);

    /* Workaround for deadlock problem in #1565 (similar to #1108) */
    pj_grp_lock_release(resolver->grp_lock);

    /* Call application callback, if any. */
    if (q->cb)
        (*q->cb)(q->user_data, PJ_ETIMEDOUT, NULL);

    /* Call application callback for child queries. */
    cq = q->child_head.next;
    while (cq != (void*)&q->child_head) {
        if (cq->cb)
            (*cq->cb)(cq->user_data, PJ_ETIMEDOUT, NULL);
        cq = cq->next;
    }

    /* Workaround for deadlock problem in #1565 (similar to #1108) */
    pj_grp_lock_acquire(resolver->grp_lock);

    /* Clear data */
    q->timer_entry.id = 0;
    q->user_data = NULL;

    /* Put child entries into recycle list */
    cq = q->child_head.next;
    while (cq != (void*)&q->child_head) {
        pj_dns_async_query *next = cq->next;
        pj_list_push_back(&resolver->query_free_nodes, cq);
        cq = next;
    }

    /* Put query entry into recycle list */
    pj_list_push_back(&resolver->query_free_nodes, q);

    pj_grp_lock_release(resolver->grp_lock);
}


/* Callback from ioqueue when packet is received */
static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    pj_dns_resolver *resolver;
    pj_pool_t *pool = NULL;
    pj_dns_parsed_packet *dns_pkt;
    pj_dns_async_query *q;
    char addr[PJ_INET6_ADDRSTRLEN];
    pj_sockaddr *src_addr;
    int *src_addr_len;
    unsigned char *rx_pkt;
    pj_ssize_t rx_pkt_size;
    pj_status_t status;
    PJ_USE_EXCEPTION;


    resolver = (pj_dns_resolver *) pj_ioqueue_get_user_data(key);
    pj_assert(resolver);

#if PJ_HAS_IPV6
    if (key == resolver->udp6_key) {
        src_addr = &resolver->udp6_src_addr;
        src_addr_len = &resolver->udp6_addr_len;
        rx_pkt = resolver->udp6_rx_pkt;
        rx_pkt_size = sizeof(resolver->udp6_rx_pkt);
    } else 
#endif
    {
        src_addr = &resolver->udp_src_addr;
        src_addr_len = &resolver->udp_addr_len;
        rx_pkt = resolver->udp_rx_pkt;
        rx_pkt_size = sizeof(resolver->udp_rx_pkt);
    }

    pj_grp_lock_acquire(resolver->grp_lock);


    /* Check for errors */
    if (bytes_read < 0) {
        status = (pj_status_t)-bytes_read;
        PJ_PERROR(4,(resolver->name.ptr, status, "DNS resolver read error"));

        goto read_next_packet;
    }

    PJ_LOG(5,(resolver->name.ptr, 
              "Received %d bytes DNS response from %s:%d",
              (int)bytes_read, 
              pj_sockaddr_print(src_addr, addr, sizeof(addr), 2),
              pj_sockaddr_get_port(src_addr)));


    /* Check for zero packet */
    if (bytes_read == 0)
        goto read_next_packet;

    /* Create temporary pool from a fixed buffer */
    pool = pj_pool_create_on_buf("restmp", resolver->tmp_pool, 
                                 sizeof(resolver->tmp_pool));

    /* Parse DNS response */
    dns_pkt = NULL;
    PJ_TRY {
        status = pj_dns_parse_packet(pool, rx_pkt, 
                                     (unsigned)bytes_read, &dns_pkt);
    }
    PJ_CATCH_ANY {
        status = PJ_ENOMEM;
    }
    PJ_END;

    /* Update nameserver status */
    report_nameserver_status(resolver, src_addr, dns_pkt);

    /* Handle parse error */
    if (status != PJ_SUCCESS) {
        PJ_PERROR(3,(resolver->name.ptr, status,
                     "Error parsing DNS response from %s:%d", 
                     pj_sockaddr_print(src_addr, addr, sizeof(addr), 2),
                     pj_sockaddr_get_port(src_addr)));
        goto read_next_packet;
    }

    /* Find the query based on the transaction ID */
    q = (pj_dns_async_query*) 
        pj_hash_get(resolver->hquerybyid, &dns_pkt->hdr.id,
                    sizeof(dns_pkt->hdr.id), NULL);
    if (!q) {
        PJ_LOG(5,(resolver->name.ptr, 
                  "DNS response from %s:%d id=%d discarded",
                  pj_sockaddr_print(src_addr, addr, sizeof(addr), 2),
                  pj_sockaddr_get_port(src_addr),
                  (unsigned)dns_pkt->hdr.id));
        goto read_next_packet;
    }

    /* Map DNS Rcode in the response into PJLIB status name space */
    status = PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_GET_RCODE(dns_pkt->hdr.flags));

    /* Cancel query timeout timer. */
    pj_assert(q->timer_entry.id != 0);
    pj_timer_heap_cancel(resolver->timer, &q->timer_entry);
    q->timer_entry.id = 0;

    /* Clear hash table entries */
    pj_hash_set(NULL, resolver->hquerybyid, &q->id, sizeof(q->id), 0, NULL);
    pj_hash_set(NULL, resolver->hquerybyres, &q->key, sizeof(q->key), 0, NULL);

    /* Workaround for deadlock problem in #1108 */
    pj_grp_lock_release(resolver->grp_lock);

    /* Notify applications first, to allow application to modify the 
     * record before it is saved to the hash table.
     */
    if (q->cb)
        (*q->cb)(q->user_data, status, dns_pkt);

    /* If query has subqueries, notify subqueries's application callback */
    if (!pj_list_empty(&q->child_head)) {
        pj_dns_async_query *child_q;

        child_q = q->child_head.next;
        while (child_q != (pj_dns_async_query*)&q->child_head) {
            if (child_q->cb)
                (*child_q->cb)(child_q->user_data, status, dns_pkt);
            child_q = child_q->next;
        }
    }

    /* Workaround for deadlock problem in #1108 */
    pj_grp_lock_acquire(resolver->grp_lock);

    /* Truncated responses MUST NOT be saved (cached). */
    if (PJ_DNS_GET_TC(dns_pkt->hdr.flags) == 0) {
        /* Save/update response cache. */
        update_res_cache(resolver, &q->key, status, PJ_TRUE, dns_pkt);
    }

    /* Recycle query objects, starting with the child queries */
    if (!pj_list_empty(&q->child_head)) {
        pj_dns_async_query *child_q;

        child_q = q->child_head.next;
        while (child_q != (pj_dns_async_query*)&q->child_head) {
            pj_dns_async_query *next = child_q->next;
            pj_list_erase(child_q);
            pj_list_push_back(&resolver->query_free_nodes, child_q);
            child_q = next;
        }
    }
    pj_list_push_back(&resolver->query_free_nodes, q);

read_next_packet:
    if (pool) {
        /* needed just in case PJ_HAS_POOL_ALT_API is set */
        pj_pool_release(pool);
    }

    status = pj_ioqueue_recvfrom(key, op_key, rx_pkt, &rx_pkt_size,
                                 PJ_IOQUEUE_ALWAYS_ASYNC,
                                 src_addr, src_addr_len);

    if (status != PJ_EPENDING && status != PJ_ECANCELLED) {
        PJ_PERROR(4,(resolver->name.ptr, status,
                     "DNS resolver ioqueue read error"));

        pj_assert(!"Unhandled error");
    }

    pj_grp_lock_release(resolver->grp_lock);
}


/*
 * Put the specified DNS packet into DNS cache. This function is mainly used
 * for testing the resolver, however it can also be used to inject entries
 * into the resolver.
 */
PJ_DEF(pj_status_t) pj_dns_resolver_add_entry( pj_dns_resolver *resolver,
                                               const pj_dns_parsed_packet *pkt,
                                               pj_bool_t set_ttl)
{
    struct res_key key;

    /* Sanity check */
    PJ_ASSERT_RETURN(resolver && pkt, PJ_EINVAL);

    /* Packet must be a DNS response */
    PJ_ASSERT_RETURN(PJ_DNS_GET_QR(pkt->hdr.flags) & 1, PJ_EINVAL);

    /* Make sure there are answers in the packet */
    PJ_ASSERT_RETURN((pkt->hdr.anscount && pkt->ans) ||
                      (pkt->hdr.qdcount && pkt->q && !pkt->hdr.anscount),
                     PJLIB_UTIL_EDNSNOANSWERREC);

    pj_grp_lock_acquire(resolver->grp_lock);

    /* Build resource key for looking up hash tables */
    pj_bzero(&key, sizeof(struct res_key));
    if (pkt->hdr.anscount) {
        /* Make sure name is not too long. */
        PJ_ASSERT_RETURN(pkt->ans[0].name.slen < PJ_MAX_HOSTNAME, 
                         PJ_ENAMETOOLONG);

        init_res_key(&key, pkt->ans[0].type, &pkt->ans[0].name);

    } else {
        /* Make sure name is not too long. */
        PJ_ASSERT_RETURN(pkt->q[0].name.slen < PJ_MAX_HOSTNAME, 
                         PJ_ENAMETOOLONG);

        init_res_key(&key, pkt->q[0].type, &pkt->q[0].name);
    }

    /* Insert entry. */
    update_res_cache(resolver, &key, PJ_SUCCESS, set_ttl, pkt);

    pj_grp_lock_release(resolver->grp_lock);

    return PJ_SUCCESS;
}


/*
 * Get the total number of response in the response cache.
 */
PJ_DEF(unsigned) pj_dns_resolver_get_cached_count(pj_dns_resolver *resolver)
{
    unsigned count;

    PJ_ASSERT_RETURN(resolver, 0);

    pj_grp_lock_acquire(resolver->grp_lock);
    count = pj_hash_count(resolver->hrescache);
    pj_grp_lock_release(resolver->grp_lock);

    return count;
}


/*
 * Dump resolver state to the log.
 */
PJ_DEF(void) pj_dns_resolver_dump(pj_dns_resolver *resolver,
                                  pj_bool_t detail)
{
#if PJ_LOG_MAX_LEVEL >= 3
    unsigned i;
    pj_time_val now;

    pj_grp_lock_acquire(resolver->grp_lock);

    pj_gettimeofday(&now);

    PJ_LOG(3,(resolver->name.ptr, " Dumping resolver state:"));

    PJ_LOG(3,(resolver->name.ptr, "  Name servers:"));
    for (i=0; i<resolver->ns_count; ++i) {
        char addr[PJ_INET6_ADDRSTRLEN];
        struct nameserver *ns = &resolver->ns[i];

        PJ_LOG(3,(resolver->name.ptr,
                  "   NS %d: %s:%d (state=%s until %lds, rtt=%ld ms)",
                  i,
                  pj_sockaddr_print(&ns->addr, addr, sizeof(addr), 2),
                  pj_sockaddr_get_port(&ns->addr),
                  state_names[ns->state],
                  ns->state_expiry.sec - now.sec,
                  PJ_TIME_VAL_MSEC(ns->rt_delay)));
    }

    PJ_LOG(3,(resolver->name.ptr, "  Nb. of cached responses: %u",
              pj_hash_count(resolver->hrescache)));
    if (detail) {
        pj_hash_iterator_t itbuf, *it;
        it = pj_hash_first(resolver->hrescache, &itbuf);
        while (it) {
            struct cached_res *cache;
            cache = (struct cached_res*)pj_hash_this(resolver->hrescache, it);
            PJ_LOG(3,(resolver->name.ptr, 
                      "   Type %s: %s",
                      pj_dns_get_type_name(cache->key.qtype), 
                      cache->key.name));
            it = pj_hash_next(resolver->hrescache, it);
        }
    }
    PJ_LOG(3,(resolver->name.ptr, "  Nb. of pending queries: %u (%u)",
              pj_hash_count(resolver->hquerybyid),
              pj_hash_count(resolver->hquerybyres)));
    if (detail) {
        pj_hash_iterator_t itbuf, *it;
        it = pj_hash_first(resolver->hquerybyid, &itbuf);
        while (it) {
            struct pj_dns_async_query *q;
            q = (pj_dns_async_query*) pj_hash_this(resolver->hquerybyid, it);
            PJ_LOG(3,(resolver->name.ptr, 
                      "   Type %s: %s",
                      pj_dns_get_type_name(q->key.qtype), 
                      q->key.name));
            it = pj_hash_next(resolver->hquerybyid, it);
        }
    }
    PJ_LOG(3,(resolver->name.ptr, "  Nb. of pending query free nodes: %lu",
              pj_list_size(&resolver->query_free_nodes)));
    PJ_LOG(3,(resolver->name.ptr, "  Nb. of timer entries: %lu",
              pj_timer_heap_count(resolver->timer)));
    PJ_LOG(3,(resolver->name.ptr, "  Pool capacity: %lu, used size: %lu",
              pj_pool_get_capacity(resolver->pool),
              pj_pool_get_used_size(resolver->pool)));

    pj_grp_lock_release(resolver->grp_lock);
#endif
}

