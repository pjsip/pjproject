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
#include <pjsip/sip_resolve.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_errno.h>
#include <pjlib-util/errno.h>
#include <pjlib-util/srv_resolver.h>
#include <pj/addr_resolv.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>


#define THIS_FILE   "sip_resolve.c"

struct naptr_target
{
    pj_str_t                res_type;       /**< e.g. "_sip._udp"   */
    pj_str_t                name;           /**< Domain name.       */
    pjsip_transport_type_e  type;           /**< Transport type.    */
    unsigned                order;          /**< Order              */
    unsigned                pref;           /**< Preference.        */
};

struct query
{
    char                    *objname;

    pj_dns_type              query_type;
    void                    *token;
    pjsip_resolver_callback *cb;
    pj_dns_async_query      *object;
    pj_dns_async_query      *object6;
    pj_status_t              last_error;

    /* Original request: */
    struct {
        pjsip_host_info      target;
        unsigned             def_port;
    } req;

    /* NAPTR records: */
    unsigned                 naptr_cnt;
    struct naptr_target      naptr[8];

    /* Query result */
    pjsip_server_addresses   server;
};


struct pjsip_resolver_t
{
    pj_dns_resolver *res;
    pjsip_ext_resolver *ext_res;
};


static void srv_resolver_cb(void *user_data,
                            pj_status_t status,
                            const pj_dns_srv_record *rec);
static void dns_a_callback(void *user_data,
                           pj_status_t status,
                           pj_dns_parsed_packet *response);
static void dns_aaaa_callback(void *user_data,
                              pj_status_t status,
                              pj_dns_parsed_packet *response);


/*
 * Public API to create the resolver.
 */
PJ_DEF(pj_status_t) pjsip_resolver_create( pj_pool_t *pool,
                                           pjsip_resolver_t **p_res)
{
    pjsip_resolver_t *resolver;

    PJ_ASSERT_RETURN(pool && p_res, PJ_EINVAL);
    resolver = PJ_POOL_ZALLOC_T(pool, pjsip_resolver_t);
    *p_res = resolver;

    return PJ_SUCCESS;
}


/*
 * Public API to set the DNS resolver instance for the SIP resolver.
 */
PJ_DEF(pj_status_t) pjsip_resolver_set_resolver(pjsip_resolver_t *res,
                                                pj_dns_resolver *dns_res)
{
#if PJSIP_HAS_RESOLVER
    res->res = dns_res;
    return PJ_SUCCESS;
#else
    PJ_UNUSED_ARG(res);
    PJ_UNUSED_ARG(dns_res);
    pj_assert(!"Resolver is disabled (PJSIP_HAS_RESOLVER==0)");
    return PJ_EINVALIDOP;
#endif
}

/*
 * Public API to set the DNS external resolver implementation for the SIP 
 * resolver.
 */
PJ_DEF(pj_status_t) pjsip_resolver_set_ext_resolver(pjsip_resolver_t *res,
                                                    pjsip_ext_resolver *ext_res)
{
    if (ext_res && !ext_res->resolve)
        return PJ_EINVAL;

    if (ext_res && res->res) {
#if PJSIP_HAS_RESOLVER
        pj_dns_resolver_destroy(res->res, PJ_FALSE);
#endif
        res->res = NULL;
    }
    res->ext_res = ext_res;
    return PJ_SUCCESS;
}


/*
 * Public API to get the internal DNS resolver.
 */
PJ_DEF(pj_dns_resolver*) pjsip_resolver_get_resolver(pjsip_resolver_t *res)
{
    return res->res;
}


/*
 * Public API to create destroy the resolver
 */
PJ_DEF(void) pjsip_resolver_destroy(pjsip_resolver_t *resolver)
{
    if (resolver->res) {
#if PJSIP_HAS_RESOLVER
        pj_dns_resolver_destroy(resolver->res, PJ_FALSE);
#endif
        resolver->res = NULL;
    }
}

/*
 * Internal:
 *  determine if an address is a valid IP address, and if it is,
 *  return the IP version (4 or 6).
 */
static int get_ip_addr_ver(const pj_str_t *host)
{
    pj_in_addr dummy;
    pj_in6_addr dummy6;

    /* First check if this is an IPv4 address */
    if (pj_inet_pton(pj_AF_INET(), host, &dummy) == PJ_SUCCESS)
        return 4;

    /* Then check if this is an IPv6 address */
    if (pj_inet_pton(pj_AF_INET6(), host, &dummy6) == PJ_SUCCESS)
        return 6;

    /* Not an IP address */
    return 0;
}


/*
 * This is the main function for performing server resolution.
 */
PJ_DEF(void) pjsip_resolve( pjsip_resolver_t *resolver,
                            pj_pool_t *pool,
                            const pjsip_host_info *target,
                            void *token,
                            pjsip_resolver_callback *cb)
{
    pjsip_server_addresses svr_addr;
    pj_status_t status = PJ_SUCCESS;
    int ip_addr_ver;
    struct query *query;
    pjsip_transport_type_e type = target->type;
    int af = pj_AF_UNSPEC();

    /* If an external implementation has been provided use it instead */
    if (resolver->ext_res) {
        (*resolver->ext_res->resolve)(resolver, pool, target, token, cb);
        return;
    }

    /* Is it IP address or hostname? And if it's an IP, which version? */
    ip_addr_ver = get_ip_addr_ver(&target->addr.host);

    /* Initialize address family type. Unfortunately, target type doesn't
     * really tell the address family type, except when IPv6 flag is
     * explicitly set.
     */
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6==1
    if ((ip_addr_ver == 6) || (type & PJSIP_TRANSPORT_IPV6))
        af = pj_AF_INET6();
    else if (ip_addr_ver == 4)
        af = pj_AF_INET();
#else
    /* IPv6 is disabled, will resolving IPv6 address be useful? */
    af = pj_AF_INET();
#endif

    /* Set the transport type if not explicitly specified. 
     * RFC 3263 section 4.1 specify rules to set up this.
     */
    if (type == PJSIP_TRANSPORT_UNSPECIFIED) {
        if (ip_addr_ver || (target->addr.port != 0)) {
#if PJ_HAS_TCP
            if (target->flag & PJSIP_TRANSPORT_SECURE) 
            {
                type = PJSIP_TRANSPORT_TLS;
            } else if (target->flag & PJSIP_TRANSPORT_RELIABLE) 
            {
                type = PJSIP_TRANSPORT_TCP;
            } else 
#endif
            {
                type = PJSIP_TRANSPORT_UDP;
            }
        } else {
            /* No type or explicit port is specified, and the address is
             * not IP address.
             * In this case, full NAPTR resolution must be performed.
             * But we don't support it (yet).
             */
#if PJ_HAS_TCP
            if (target->flag & PJSIP_TRANSPORT_SECURE) 
            {
                type = PJSIP_TRANSPORT_TLS;
            } else if (target->flag & PJSIP_TRANSPORT_RELIABLE) 
            {
                type = PJSIP_TRANSPORT_TCP;
            } else 
#endif
            {
                type = PJSIP_TRANSPORT_UDP;
            }
        }
    }


    /* If target is an IP address, or if resolver is not configured, 
     * we can just finish the resolution now using pj_gethostbyname()
     */
    if (ip_addr_ver || resolver->res == NULL) {
        char addr_str[PJ_INET6_ADDRSTRLEN+10];
        pj_uint16_t srv_port;
        unsigned i;

        if (ip_addr_ver != 0) {
            /* Target is an IP address, no need to resolve */
            svr_addr.count = 1;
            if (ip_addr_ver == 4) {
                if (af == pj_AF_INET6()) {
                    /* Generate a synthesized IPv6 address, if possible. */
                    unsigned int count = 1;
                    pj_addrinfo ai[1];
                    pj_status_t status2;

                    status2 = pj_getaddrinfo(pj_AF_INET6(),
                                            &target->addr.host, &count, ai);
                    if (status2 == PJ_SUCCESS && count > 0 &&
                        ai[0].ai_addr.addr.sa_family == pj_AF_INET6())
                    {
                        pj_sockaddr_init(pj_AF_INET6(),
                                         &svr_addr.entry[0].addr,
                                         NULL, 0);
                        svr_addr.entry[0].addr.ipv6.sin6_addr =
                            ai[0].ai_addr.ipv6.sin6_addr;
                    } else {
                        pj_sockaddr_init(pj_AF_INET(),
                                         &svr_addr.entry[0].addr, NULL, 0);
                        pj_inet_pton(pj_AF_INET(), &target->addr.host,
                                     &svr_addr.entry[0].addr.ipv4.sin_addr);
                    }
                } else {
                    pj_sockaddr_init(pj_AF_INET(), &svr_addr.entry[0].addr, 
                                     NULL, 0);
                    pj_inet_pton(pj_AF_INET(), &target->addr.host,
                                 &svr_addr.entry[0].addr.ipv4.sin_addr);
                }
            } else {
                pj_sockaddr_init(pj_AF_INET6(), &svr_addr.entry[0].addr, 
                                 NULL, 0);
                pj_inet_pton(pj_AF_INET6(), &target->addr.host,
                             &svr_addr.entry[0].addr.ipv6.sin6_addr);
            }
        } else {
            pj_addrinfo ai[PJSIP_MAX_RESOLVED_ADDRESSES];
            unsigned count;

            PJ_LOG(5,(THIS_FILE,
                      "DNS resolver not available, target '%.*s:%d' type=%s "
                      "will be resolved with getaddrinfo()",
                      (int)target->addr.host.slen,
                      target->addr.host.ptr,
                      target->addr.port,
                      pjsip_transport_get_type_name(target->type)));

            /* Resolve */
            count = PJSIP_MAX_RESOLVED_ADDRESSES;
            status = pj_getaddrinfo(af, &target->addr.host, &count, ai);
            if (status != PJ_SUCCESS) {
                /* "Normalize" error to PJ_ERESOLVE. This is a special error
                 * because it will be translated to SIP status 502 by
                 * sip_transaction.c
                 */
                status = PJ_ERESOLVE;
                goto on_error;
            }

            svr_addr.count = count;
            for (i = 0; i < count; i++) {
                pj_sockaddr_cp(&svr_addr.entry[i].addr, &ai[i].ai_addr);
            }
        }

        for (i = 0; i < svr_addr.count; i++) {
            /* After address resolution, update IPv6 bitflag in
             * transport type.
             */
            af = svr_addr.entry[i].addr.addr.sa_family;
            if (af == pj_AF_INET6()) {
                type |= PJSIP_TRANSPORT_IPV6;
            } else {
                type &= ~PJSIP_TRANSPORT_IPV6;
            }

            /* Set the port number */
            if (target->addr.port == 0) {
               srv_port = (pj_uint16_t)
                          pjsip_transport_get_default_port_for_type(type);
            } else {
               srv_port = (pj_uint16_t)target->addr.port;
            }
            pj_sockaddr_set_port(&svr_addr.entry[i].addr, srv_port);

            PJ_LOG(5,(THIS_FILE, 
                      "Target '%.*s:%d' type=%s resolved to "
                      "'%s' type=%s (%s)",
                      (int)target->addr.host.slen,
                      target->addr.host.ptr,
                      target->addr.port,
                      pjsip_transport_get_type_name(target->type),
                      pj_sockaddr_print(&svr_addr.entry[i].addr, addr_str,
                                        sizeof(addr_str), 3),
                      pjsip_transport_get_type_name(type),
                      pjsip_transport_get_type_desc(type)));

            svr_addr.entry[i].priority = 0;
            svr_addr.entry[i].weight = 0;
            svr_addr.entry[i].type = type;
            svr_addr.entry[i].addr_len = 
                                pj_sockaddr_get_len(&svr_addr.entry[i].addr);
        }

        /* Call the callback. */
        (*cb)(status, token, &svr_addr);

        /* Done. */
        return;
    }

    /* Target is not an IP address so we need to resolve it. */
#if PJSIP_HAS_RESOLVER

    /* Build the query state */
    query = PJ_POOL_ZALLOC_T(pool, struct query);
    query->objname = THIS_FILE;
    query->token = token;
    query->cb = cb;
    query->req.target = *target;
    pj_strdup(pool, &query->req.target.addr.host, &target->addr.host);

    /* If port is not specified, start with SRV resolution
     * (should be with NAPTR, but we'll do that later)
     */
    PJ_TODO(SUPPORT_DNS_NAPTR);

    /* Build dummy NAPTR entry */
    query->naptr_cnt = 1;
    pj_bzero(&query->naptr[0], sizeof(query->naptr[0]));
    query->naptr[0].order = 0;
    query->naptr[0].pref = 0;
    query->naptr[0].type = type;
    pj_strdup(pool, &query->naptr[0].name, &target->addr.host);


    /* Start DNS SRV or A resolution, depending on whether port is specified */
    if (target->addr.port == 0) {
        query->query_type = PJ_DNS_TYPE_SRV;

        query->req.def_port = 5060;

        if (type == PJSIP_TRANSPORT_TLS || type == PJSIP_TRANSPORT_TLS6) {
            query->naptr[0].res_type = pj_str("_sips._tcp.");
            query->req.def_port = 5061;
        } else if (type == PJSIP_TRANSPORT_TCP || type == PJSIP_TRANSPORT_TCP6)
            query->naptr[0].res_type = pj_str("_sip._tcp.");
        else if (type == PJSIP_TRANSPORT_UDP || type == PJSIP_TRANSPORT_UDP6)
            query->naptr[0].res_type = pj_str("_sip._udp.");
        else {
            pj_assert(!"Unknown transport type");
            query->naptr[0].res_type = pj_str("_sip._udp.");
            
        }

    } else {
        /* Otherwise if port is specified, start with A (or AAAA) host 
         * resolution 
         */
        query->query_type = PJ_DNS_TYPE_A;
        query->naptr[0].res_type.slen = 0;
        query->req.def_port = target->addr.port;
    }

    /* Start the asynchronous query */
    PJ_LOG(5, (query->objname, 
               "Starting async DNS %s query: target=%.*s%.*s, transport=%s, "
               "port=%d",
               pj_dns_get_type_name(query->query_type),
               (int)query->naptr[0].res_type.slen,
               query->naptr[0].res_type.ptr,
               (int)query->naptr[0].name.slen, query->naptr[0].name.ptr,
               pjsip_transport_get_type_name(target->type),
               target->addr.port));

    if (query->query_type == PJ_DNS_TYPE_SRV) {
        int opt = 0;

        if (af == pj_AF_UNSPEC())
            opt = PJ_DNS_SRV_FALLBACK_A | PJ_DNS_SRV_FALLBACK_AAAA |
                  PJ_DNS_SRV_RESOLVE_AAAA;
        else if (af == pj_AF_INET6())
            opt = PJ_DNS_SRV_FALLBACK_AAAA | PJ_DNS_SRV_RESOLVE_AAAA_ONLY;
        else /* af == pj_AF_INET() */
            opt = PJ_DNS_SRV_FALLBACK_A;

        status = pj_dns_srv_resolve(&query->naptr[0].name,
                                    &query->naptr[0].res_type,
                                    query->req.def_port, pool, resolver->res,
                                    opt, query, &srv_resolver_cb, NULL);

    } else if (query->query_type == PJ_DNS_TYPE_A) {

        /* Resolve DNS A record if address family is not fixed to IPv6 */
        if (af != pj_AF_INET6()) {

            /* If there will be DNS AAAA query too, let's setup a dummy one
             * here, otherwise app callback may be called immediately (before
             * DNS AAAA query is sent) when DNS A record is available in the
             * cache.
             */
            if (af == pj_AF_UNSPEC())
                query->object6 = (pj_dns_async_query*)0x1;

            status = pj_dns_resolver_start_query(resolver->res, 
                                                 &query->naptr[0].name,
                                                 PJ_DNS_TYPE_A, 0, 
                                                 &dns_a_callback,
                                                 query, &query->object);
        }

        /* Resolve DNS AAAA record if address family is not fixed to IPv4 */
        if (af != pj_AF_INET() && status == PJ_SUCCESS) {
            status = pj_dns_resolver_start_query(resolver->res, 
                                                 &query->naptr[0].name,
                                                 PJ_DNS_TYPE_AAAA, 0, 
                                                 &dns_aaaa_callback,
                                                 query, &query->object6);
        }

    } else {
        pj_assert(!"Unexpected");
        status = PJ_EBUG;
    }

    if (status != PJ_SUCCESS)
        goto on_error;

    return;

#else /* PJSIP_HAS_RESOLVER */
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(query);
#endif /* PJSIP_HAS_RESOLVER */

on_error:
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4,(THIS_FILE, status,
                     "Failed to resolve '%.*s'",
                     (int)target->addr.host.slen,
                     target->addr.host.ptr));
        (*cb)(status, token, NULL);
        return;
    }
}

#if PJSIP_HAS_RESOLVER

/* 
 * This callback is called when target is resolved with DNS A query.
 */
static void dns_a_callback(void *user_data,
                           pj_status_t status,
                           pj_dns_parsed_packet *pkt)
{
    struct query *query = (struct query*) user_data;
    pjsip_server_addresses *srv = &query->server;

    /* Reset outstanding job */
    query->object = NULL;

    if (status == PJ_SUCCESS) {
        pj_dns_addr_record rec;
        unsigned i;

        /* Parse the response */
        rec.addr_count = 0;
        status = pj_dns_parse_addr_response(pkt, &rec);

        /* Build server addresses and call callback */
        for (i = 0; i < rec.addr_count &&
                    srv->count < PJSIP_MAX_RESOLVED_ADDRESSES; ++i)
        {
            /* Should not happen, just in case */
            if (rec.addr[i].af != pj_AF_INET())
                continue;

            srv->entry[srv->count].type = query->naptr[0].type;
            srv->entry[srv->count].priority = 0;
            srv->entry[srv->count].weight = 0;
            srv->entry[srv->count].addr_len = sizeof(pj_sockaddr_in);
            pj_sockaddr_in_init(&srv->entry[srv->count].addr.ipv4,
                                0, (pj_uint16_t)query->req.def_port);
            srv->entry[srv->count].addr.ipv4.sin_addr = rec.addr[i].ip.v4;

            ++srv->count;
        }
    }
    
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4,(query->objname, status,
                     "DNS A record resolution failed"));

        query->last_error = status;
    }

    /* Call the callback if all DNS queries have been completed */
    if (query->object == NULL && query->object6 == NULL) {
        if (srv->count > 0)
            (*query->cb)(PJ_SUCCESS, query->token, &query->server);
        else
            (*query->cb)(query->last_error, query->token, NULL);
    }
}


/* 
 * This callback is called when target is resolved with DNS AAAA query.
 */
static void dns_aaaa_callback(void *user_data,
                              pj_status_t status,
                              pj_dns_parsed_packet *pkt)
{
    struct query *query = (struct query*) user_data;
    pjsip_server_addresses *srv = &query->server;

    /* Reset outstanding job */
    query->object6 = NULL;

    if (status == PJ_SUCCESS) {
        pj_dns_addr_record rec;
        unsigned i;

        /* Parse the response */
        rec.addr_count = 0;
        status = pj_dns_parse_addr_response(pkt, &rec);

        /* Build server addresses and call callback */
        for (i = 0; i < rec.addr_count &&
                    srv->count < PJSIP_MAX_RESOLVED_ADDRESSES; ++i)
        {
            /* Should not happen, just in case */
            if (rec.addr[i].af != pj_AF_INET6())
                continue;

            srv->entry[srv->count].type = query->naptr[0].type |
                                          PJSIP_TRANSPORT_IPV6;
            srv->entry[srv->count].priority = 0;
            srv->entry[srv->count].weight = 0;
            srv->entry[srv->count].addr_len = sizeof(pj_sockaddr_in6);
            pj_sockaddr_init(pj_AF_INET6(), &srv->entry[srv->count].addr,
                             0, (pj_uint16_t)query->req.def_port);
            srv->entry[srv->count].addr.ipv6.sin6_addr = rec.addr[i].ip.v6;

            ++srv->count;
        }
    }
    
    if (status != PJ_SUCCESS) {
        PJ_PERROR(4,(query->objname, status,
                     "DNS AAAA record resolution failed"));

        query->last_error = status;
    }

    /* Call the callback if all DNS queries have been completed */
    if (query->object == NULL && query->object6 == NULL) {
        if (srv->count > 0)
            (*query->cb)(PJ_SUCCESS, query->token, &query->server);
        else
            (*query->cb)(query->last_error, query->token, NULL);
    }
}


/* Callback to be called by DNS SRV resolution */
static void srv_resolver_cb(void *user_data,
                            pj_status_t status,
                            const pj_dns_srv_record *rec)
{
    struct query *query = (struct query*) user_data;
    pjsip_server_addresses srv;
    unsigned i;

    if (status != PJ_SUCCESS) {
        PJ_PERROR(4,(query->objname, status,
                     "DNS A/AAAA record resolution failed"));

        /* Call the callback */
        (*query->cb)(status, query->token, NULL);
        return;
    }

    /* Build server addresses and call callback */
    srv.count = 0;
    for (i=0; i<rec->count; ++i) {
        const pj_dns_addr_record *s = &rec->entry[i].server;
        unsigned j;

        for (j = 0; j < s->addr_count &&
                    srv.count < PJSIP_MAX_RESOLVED_ADDRESSES; ++j)
        {
            srv.entry[srv.count].type = query->naptr[0].type;
            srv.entry[srv.count].priority = rec->entry[i].priority;
            srv.entry[srv.count].weight = rec->entry[i].weight;
            pj_sockaddr_init(s->addr[j].af,
                             &srv.entry[srv.count].addr,
                             0, (pj_uint16_t)rec->entry[i].port);
            if (s->addr[j].af == pj_AF_INET6())
                srv.entry[srv.count].addr.ipv6.sin6_addr = s->addr[j].ip.v6;
            else
                srv.entry[srv.count].addr.ipv4.sin_addr = s->addr[j].ip.v4;
            srv.entry[srv.count].addr_len =
                            pj_sockaddr_get_len(&srv.entry[srv.count].addr);

            /* Update transport type if this is IPv6 */
            if (s->addr[j].af == pj_AF_INET6())
                srv.entry[srv.count].type |= PJSIP_TRANSPORT_IPV6;

            ++srv.count;
        }
    }

    /* Call the callback */
    (*query->cb)(PJ_SUCCESS, query->token, &srv);
}

#endif  /* PJSIP_HAS_RESOLVER */

