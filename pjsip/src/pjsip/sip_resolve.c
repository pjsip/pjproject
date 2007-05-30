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
#include <pjsip/sip_resolve.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_errno.h>
#include <pjlib-util/errno.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>


#define THIS_FILE   "sip_resolve.c"

#define ADDR_MAX_COUNT	    8

struct naptr_target
{
    pj_str_t		    target_name;    /**< NAPTR target name. */
    pjsip_transport_type_e  type;	    /**< Transport type.    */
    unsigned		    order;	    /**< Order		    */
    unsigned		    pref;	    /**< Preference.	    */
};

struct srv_target
{
    pjsip_transport_type_e  type;
    pj_str_t		    target_name;
    char		    target_buf[PJ_MAX_HOSTNAME];
    unsigned		    port;
    unsigned		    priority;
    unsigned		    weight;
    unsigned		    sum;
    unsigned		    addr_cnt;
    pj_in_addr		    addr[ADDR_MAX_COUNT];
};

struct query
{
    char		     objname[PJ_MAX_OBJ_NAME];

    pjsip_resolver_t	    *resolver;	    /**< Resolver SIP instance.	    */
    pj_dns_type		     dns_state;	    /**< DNS type being resolved.   */
    void		    *token;
    pjsip_resolver_callback *cb;
    pj_dns_async_query	    *object;
    pj_status_t		     last_error;

    /* Original request: */
    struct {
	pjsip_host_info	     target;
    } req;

    /* NAPTR records: */
    unsigned		     naptr_cnt;
    struct naptr_target	     naptr[8];

    /* SRV records and their resolved IP addresses: */
    unsigned		     srv_cnt;
    struct srv_target	     srv[PJSIP_MAX_RESOLVED_ADDRESSES];

    /* Number of hosts in SRV records that the IP address has been resolved */
    unsigned		     host_resolved;
};


struct pjsip_resolver_t
{
    pj_dns_resolver *res;
    unsigned	     job_id;
};

static void dns_callback(void *user_data,
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
 *  determine if an address is a valid IP address.
 */
static int is_str_ip(const pj_str_t *host)
{
    const char *p = host->ptr;
    const char *end = ((const char*)host->ptr) + host->slen;

    while (p != end) {
	if (pj_isdigit(*p) || *p=='.') {
	    ++p;
	} else {
	    return 0;
	}
    }
    return 1;
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
    int is_ip_addr;
    struct query *query;
    pj_str_t srv_name;
    pjsip_transport_type_e type = target->type;

    /* Is it IP address or hostname?. */
    is_ip_addr = is_str_ip(&target->addr.host);

    /* Set the transport type if not explicitly specified. 
     * RFC 3263 section 4.1 specify rules to set up this.
     */
    if (type == PJSIP_TRANSPORT_UNSPECIFIED) {
	if (is_ip_addr || (target->addr.port != 0)) {
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
	     * In this case, full resolution must be performed.
	     * But we don't support it (yet).
	     */
	    type = PJSIP_TRANSPORT_UDP;
	}
    }


    /* If target is an IP address, or if resolver is not configured, 
     * we can just finish the resolution now using pj_gethostbyname()
     */
    if (is_ip_addr || resolver->res == NULL) {

	pj_in_addr ip_addr;
	pj_uint16_t srv_port;

	if (!is_ip_addr) {
	    PJ_LOG(5,(THIS_FILE, 
		      "DNS resolver not available, target '%.*s:%d' type=%s "
		      "will be resolved with gethostbyname()",
		      target->addr.host.slen,
		      target->addr.host.ptr,
		      target->addr.port,
		      pjsip_transport_get_type_name(target->type)));
	}

	/* Set the port number if not specified. */
	if (target->addr.port == 0) {
	   srv_port = (pj_uint16_t)
		      pjsip_transport_get_default_port_for_type(type);
	} else {
	   srv_port = (pj_uint16_t)target->addr.port;
	}

	/* This will eventually call pj_gethostbyname() if the host
	 * is not an IP address.
	 */
	status = pj_sockaddr_in_init((pj_sockaddr_in*)&svr_addr.entry[0].addr,
				      &target->addr.host, srv_port);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Call the callback. */
	ip_addr = ((pj_sockaddr_in*)&svr_addr.entry[0].addr)->sin_addr;
	PJ_LOG(5,(THIS_FILE, 
		  "Target '%.*s:%d' type=%s resolved to "
		  "'%s:%d' type=%s",
		  (int)target->addr.host.slen,
		  target->addr.host.ptr,
		  target->addr.port,
		  pjsip_transport_get_type_name(target->type),
		  pj_inet_ntoa(ip_addr),
		  srv_port,
		  pjsip_transport_get_type_name(type)));
	svr_addr.count = 1;
	svr_addr.entry[0].priority = 0;
	svr_addr.entry[0].weight = 0;
	svr_addr.entry[0].type = type;
	svr_addr.entry[0].addr_len = sizeof(pj_sockaddr_in);
	(*cb)(status, token, &svr_addr);

	/* Done. */
	return;
    }

    /* Target is not an IP address so we need to resolve it. */
#if PJSIP_HAS_RESOLVER

    /* Build the query state */
    query = PJ_POOL_ZALLOC_T(pool, struct query);
    pj_ansi_snprintf(query->objname, sizeof(query->objname), "rsvjob%X",
		     resolver->job_id++);
    query->resolver = resolver;
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
    query->naptr[0].target_name.ptr = (char*)
	pj_pool_alloc(pool, target->addr.host.slen + 12);

    if (type == PJSIP_TRANSPORT_TLS)
	pj_strcpy2(&query->naptr[0].target_name, "_sips._tcp.");
    else if (type == PJSIP_TRANSPORT_TCP)
	pj_strcpy2(&query->naptr[0].target_name, "_sip._tcp.");
    else if (type == PJSIP_TRANSPORT_UDP)
	pj_strcpy2(&query->naptr[0].target_name, "_sip._udp.");
    else {
	pj_assert(!"Unknown transport type");
	pj_strcpy2(&query->naptr[0].target_name, "_sip._udp.");
    }
    pj_strcat(&query->naptr[0].target_name, &target->addr.host);


    /* Start DNS SRV or A resolution, depending on whether port is specified */
    if (target->addr.port == 0) {
	query->dns_state = PJ_DNS_TYPE_SRV;
	srv_name = query->naptr[0].target_name;

    } else {
	/* Otherwise if port is specified, start with A (or AAAA) host 
	 * resolution 
	 */
	query->dns_state = PJ_DNS_TYPE_A;

	/* Since we don't perform SRV resolution, pretend that we'ee already
	 * done so by inserting a dummy SRV record.
	 */

	query->srv_cnt = 1;
	pj_bzero(&query->srv[0], sizeof(query->srv[0]));
	query->srv[0].target_name = query->req.target.addr.host;
	query->srv[0].type = type;
	query->srv[0].port = query->req.target.addr.port;
	query->srv[0].priority = 0;
	query->srv[0].weight = 0;

	srv_name = query->srv[0].target_name;
    }

    /* Start the asynchronous query */
    PJ_LOG(5, (query->objname, 
	       "Starting async DNS %s query: target=%.*s, transport=%s, "
	       "port=%d",
	       pj_dns_get_type_name(query->dns_state),
	       (int)srv_name.slen, srv_name.ptr,
	       pjsip_transport_get_type_name(target->type),
	       target->addr.port));

    status = pj_dns_resolver_start_query(resolver->res, &srv_name, 
				         query->dns_state, 0, &dns_callback,
    					 query, &query->object);
    if (status != PJ_SUCCESS)
	goto on_error;

    return;

#else /* PJSIP_HAS_RESOLVER */
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(query);
    PJ_UNUSED_ARG(srv_name);
#endif /* PJSIP_HAS_RESOLVER */

on_error:
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	PJ_LOG(4,(THIS_FILE, "Failed to resolve '%.*s'. Err=%d (%s)",
			     (int)target->addr.host.slen,
			     target->addr.host.ptr,
			     status,
			     pj_strerror(status,errmsg,sizeof(errmsg)).ptr));
	(*cb)(status, token, NULL);
	return;
    }
}

/*
 * The rest of the code should only get compiled when resolver is enabled
 */
#if PJSIP_HAS_RESOLVER

#define SWAP(type,ptr1,ptr2)	if (ptr1 != ptr2) { \
				  type tmp; \
				  pj_memcpy(&tmp, ptr1, sizeof(type)); \
				  pj_memcpy(ptr1, ptr2, sizeof(type)); \
				  (ptr1)->target_name.ptr = (ptr1)->target_buf; \
				  pj_memcpy(ptr2, &tmp, sizeof(type)); \
				  (ptr2)->target_name.ptr = (ptr2)->target_buf; \
				} else {}

/* Build server entries in the query based on received SRV response */
static void build_server_entries(struct query *query, 
				 pj_dns_parsed_packet *response)
{
    unsigned i;
    unsigned naptr_id;

    /* Find NAPTR target which corresponds to this SRV target */
    for (naptr_id=0; naptr_id < query->naptr_cnt; ++naptr_id) {
	if (pj_stricmp(&query->naptr[naptr_id].target_name,
		       &response->ans[0].name)==0)
	    break;
    }
    if (naptr_id == query->naptr_cnt) {
	PJ_LOG(4,(query->objname, 
		  "Unable to find NAPTR record for SRV name %.*s!",
		  (int)response->ans[0].name.slen, 
		  response->ans[0].name.ptr));
	return;
    }


    /* Save the Resource Records in DNS answer into SRV targets. */
    query->srv_cnt = 0;
    for (i=0; i<response->hdr.anscount && 
	      query->srv_cnt < PJSIP_MAX_RESOLVED_ADDRESSES; ++i) 
    {
	pj_dns_parsed_rr *rr = &response->ans[i];
	struct srv_target *srv = &query->srv[query->srv_cnt];

	if (rr->type != PJ_DNS_TYPE_SRV) {
	    PJ_LOG(4,(query->objname, 
		      "Received non SRV answer for SRV query!"));
	    continue;
	}

	if (rr->rdata.srv.target.slen > PJ_MAX_HOSTNAME) {
	    PJ_LOG(4,(query->objname, "Hostname is too long!"));
	    continue;
	}

	/* Build the SRV entry for RR */
	pj_bzero(srv, sizeof(*srv));
	pj_memcpy(srv->target_buf, rr->rdata.srv.target.ptr, 
		  rr->rdata.srv.target.slen);
	srv->target_name.ptr = srv->target_buf;
	srv->target_name.slen = rr->rdata.srv.target.slen;
	srv->type = query->naptr[naptr_id].type;
	srv->port = rr->rdata.srv.port;
	srv->priority = rr->rdata.srv.prio;
	srv->weight = rr->rdata.srv.weight;
	
	++query->srv_cnt;
    }

    /* First pass: 
     *	order the entries based on priority.
     */
    for (i=0; i<query->srv_cnt-1; ++i) {
	unsigned min = i, j;
	for (j=i+1; j<query->srv_cnt; ++j) {
	    if (query->srv[j].priority < query->srv[min].priority)
		min = j;
	}
	SWAP(struct srv_target, &query->srv[i], &query->srv[min]);
    }

    /* Second pass:
     *	pick one host among hosts with the same priority, according
     *	to its weight. The idea is when one server fails, client should
     *	contact the next server with higher priority rather than contacting
     *	server with the same priority as the failed one.
     *
     *  The algorithm for selecting server among servers with the same
     *  priority is described in RFC 2782.
     */
    for (i=0; i<query->srv_cnt; ++i) {
	unsigned j, count=1, sum;

	/* Calculate running sum for servers with the same priority */
	sum = query->srv[i].sum = query->srv[i].weight;
	for (j=i+1; j<query->srv_cnt && 
		    query->srv[j].priority == query->srv[i].priority; ++j)
	{
	    sum += query->srv[j].weight;
	    query->srv[j].sum = sum;
	    ++count;
	}

	if (count > 1) {
	    unsigned r;

	    /* Elect one random number between zero and the total sum of
	     * weight (inclusive).
	     */
	    r = pj_rand() % (sum + 1);

	    /* Select the first server which running sum is greater than or
	     * equal to the random number.
	     */
	    for (j=i; j<i+count; ++j) {
		if (query->srv[j].sum >= r)
		    break;
	    }

	    /* Must have selected one! */
	    pj_assert(j != i+count);

	    /* Put this entry in front (of entries with same priority) */
	    SWAP(struct srv_target, &query->srv[i], &query->srv[j]);

	    /* Remove all other entries (of the same priority) */
	    while (count > 1) {
		pj_array_erase(query->srv, sizeof(struct srv_target), 
			       query->srv_cnt, i+1);
		--count;
		--query->srv_cnt;
	    }
	}
    }

    /* Since we've been moving around SRV entries, update the pointers
     * in target_name.
     */
    for (i=0; i<query->srv_cnt; ++i) {
	query->srv[i].target_name.ptr = query->srv[i].target_buf;
    }

    /* Check for Additional Info section if A records are available, and
     * fill in the IP address (so that we won't need to resolve the A 
     * record with another DNS query). 
     */
    for (i=0; i<response->hdr.arcount; ++i) {
	pj_dns_parsed_rr *rr = &response->arr[i];
	unsigned j;

	if (rr->type != PJ_DNS_TYPE_A)
	    continue;

	/* Yippeaiyee!! There is an "A" record! 
	 * Update the IP address of the corresponding SRV record.
	 */
	for (j=0; j<query->srv_cnt; ++j) {
	    if (pj_stricmp(&rr->name, &query->srv[j].target_name)==0) {
		unsigned cnt = query->srv[j].addr_cnt;
		query->srv[j].addr[cnt].s_addr = rr->rdata.a.ip_addr.s_addr;
		++query->srv[j].addr_cnt;
		++query->host_resolved;
		break;
	    }
	}

	/* Not valid message; SRV entry might have been deleted in
	 * server selection process.
	 */
	/*
	if (j == query->srv_cnt) {
	    PJ_LOG(4,(query->objname, 
		      "Received DNS SRV answer with A record, but "
		      "couldn't find matching name (name=%.*s)",
		      (int)rr->name.slen,
		      rr->name.ptr));
	}
	*/
    }

    /* Rescan again the name specified in the SRV record to see if IP
     * address is specified as the target name (unlikely, but well, who 
     * knows..).
     */
    for (i=0; i<query->srv_cnt; ++i) {
	pj_in_addr addr;

	if (query->srv[i].addr_cnt != 0) {
	    /* IP address already resolved */
	    continue;
	}

	if (pj_inet_aton(&query->srv[i].target_name, &addr) != 0) {
	    query->srv[i].addr[query->srv[i].addr_cnt++] = addr;
	    ++query->host_resolved;
	}
    }

    /* Print resolved entries to the log */
    PJ_LOG(5,(query->objname, 
	      "SRV query for %.*s completed, "
	      "%d of %d total entries selected%c",
	      (int)query->naptr[naptr_id].target_name.slen,
	      query->naptr[naptr_id].target_name.ptr,
	      query->srv_cnt,
	      response->hdr.anscount,
	      (query->srv_cnt ? ':' : ' ')));

    for (i=0; i<query->srv_cnt; ++i) {
	const char *addr;

	if (query->srv[i].addr_cnt != 0)
	    addr = pj_inet_ntoa(query->srv[i].addr[0]);
	else
	    addr = "-";

	PJ_LOG(5,(query->objname, 
		  " %d: SRV %d %d %d %.*s (%s)",
		  i, query->srv[i].priority, 
		  query->srv[i].weight, 
		  query->srv[i].port, 
		  (int)query->srv[i].target_name.slen, 
		  query->srv[i].target_name.ptr,
		  addr));
    }
}


/* Start DNS A record queries for all SRV records in the query structure */
static pj_status_t resolve_hostnames(struct query *query)
{
    unsigned i;
    pj_status_t err=PJ_SUCCESS, status;

    query->dns_state = PJ_DNS_TYPE_A;
    for (i=0; i<query->srv_cnt; ++i) {
	PJ_LOG(5, (query->objname, 
		   "Starting async DNS A query for %.*s",
		   (int)query->srv[i].target_name.slen, 
		   query->srv[i].target_name.ptr));

	status = pj_dns_resolver_start_query(query->resolver->res,
					     &query->srv[i].target_name,
					     PJ_DNS_TYPE_A, 0,
					     &dns_callback,
					     query, NULL);
	if (status != PJ_SUCCESS) {
	    query->host_resolved++;
	    err = status;
	}
    }
    
    return (query->host_resolved == query->srv_cnt) ? err : PJ_SUCCESS;
}

/* 
 * This callback is called by PJLIB-UTIL DNS resolver when asynchronous
 * query has completed (successfully or with error).
 */
static void dns_callback(void *user_data,
			 pj_status_t status,
			 pj_dns_parsed_packet *pkt)
{
    struct query *query = (struct query*) user_data;
    unsigned i;

    /* Proceed to next stage */

    if (query->dns_state == PJ_DNS_TYPE_SRV) {

	/* We are getting SRV response */

	if (status == PJ_SUCCESS && pkt->hdr.anscount != 0) {
	    /* Got SRV response, build server entry. If A records are available
	     * in additional records section of the DNS response, save them too.
	     */
	    build_server_entries(query, pkt);

	} else if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    unsigned naptr_id;

	    /* Update query last error */
	    query->last_error = status;

	    /* Find which NAPTR target has not got SRV records */
	    for (naptr_id=0; naptr_id < query->naptr_cnt; ++naptr_id) {
		for (i=0; i<query->srv_cnt; ++i) {
		    if (query->srv[i].type == query->naptr[naptr_id].type)
			break;
		}
		if (i == query->srv_cnt)
		    break;
	    }
	    if (naptr_id == query->naptr_cnt) {
		/* Strangely all NAPTR records seem to already have SRV
		 * records! This is quite unexpected, by anyway lets set
		 * the naptr_id to zero just in case.
		 */
		pj_assert(!"Strange");
		naptr_id = 0;

	    }

	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(query->objname, 
		      "DNS SRV resolution failed for %.*s: %s", 
		      (int)query->naptr[naptr_id].target_name.slen, 
		      query->naptr[naptr_id].target_name.ptr,
		      errmsg));
	}

	/* If we can't build SRV record, assume the original target is
	 * an A record.
	 */
	if (query->srv_cnt == 0) {
	    /* Looks like we aren't getting any SRV responses.
	     * Resolve the original target as A record by creating a 
	     * single "dummy" srv record and start the hostname resolution.
	     */
	    unsigned naptr_id;

	    /* Find which NAPTR target has not got SRV records */
	    for (naptr_id=0; naptr_id < query->naptr_cnt; ++naptr_id) {
		for (i=0; i<query->srv_cnt; ++i) {
		    if (query->srv[i].type == query->naptr[naptr_id].type)
			break;
		}
		if (i == query->srv_cnt)
		    break;
	    }
	    if (naptr_id == query->naptr_cnt) {
		/* Strangely all NAPTR records seem to already have SRV
		 * records! This is quite unexpected, by anyway lets set
		 * the naptr_id to zero just in case.
		 */
		pj_assert(!"Strange");
		naptr_id = 0;

	    }

	    PJ_LOG(4, (query->objname, 
		       "DNS SRV resolution failed for %.*s, trying "
		       "resolving A record for %.*s",
		       (int)query->naptr[naptr_id].target_name.slen, 
		       query->naptr[naptr_id].target_name.ptr,
		       (int)query->req.target.addr.host.slen,
		       query->req.target.addr.host.ptr));

	    /* Create a "dummy" srv record using the original target */
	    i = query->srv_cnt++;
	    pj_bzero(&query->srv[i], sizeof(query->srv[i]));
	    query->srv[i].target_name = query->req.target.addr.host;
	    query->srv[i].type = query->naptr[naptr_id].type;
	    query->srv[i].priority = 0;
	    query->srv[i].weight = 0;

	    query->srv[i].port = query->req.target.addr.port;
	    if (query->srv[i].port == 0) {
		query->srv[i].port = (pj_uint16_t)
		 pjsip_transport_get_default_port_for_type(query->srv[i].type);
	    }
	} 
	

	/* Resolve server hostnames (DNS A record) for hosts which don't have
	 * A record yet.
	 */
	if (query->host_resolved != query->srv_cnt) {
	    status = resolve_hostnames(query);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    /* Must return now. Callback may have been called and query
	     * may have been destroyed.
	     */
	    return;
	}

    } else if (query->dns_state == PJ_DNS_TYPE_A) {

	/* Check that we really have answer */
	if (status==PJ_SUCCESS && pkt->hdr.anscount != 0) {

	    unsigned srv_idx;

	    /* Update IP address of the corresponding hostname */
	    for (srv_idx=0; srv_idx<query->srv_cnt; ++srv_idx) {
		if (pj_stricmp(&pkt->ans[0].name, 
			       &query->srv[srv_idx].target_name)==0) 
		{
		    break;
		}
	    }

	    if (srv_idx == query->srv_cnt) {
		PJ_LOG(4,(query->objname, 
			  "Received answer to DNS A request with no matching "
			  "SRV record! The unknown name is %.*s",
			  (int)pkt->ans[0].name.slen, pkt->ans[0].name.ptr));
	    } else {
		int ans_idx = -1;
		unsigned k, j;
		pj_str_t cname = { NULL, 0 };

		/* Find the first DNS A record in the answer while processing
		 * the CNAME info found in the response.
		 */
		for (k=0; k < pkt->hdr.anscount; ++k) {

		    pj_dns_parsed_rr *rr = &pkt->ans[k];

		    if (rr->type == PJ_DNS_TYPE_A &&
			(cname.slen == 0 || pj_stricmp(&rr->name, &cname)==0))
		    {
			if (ans_idx == -1)
			    ans_idx = k;

		    } else if (rr->type == PJ_DNS_TYPE_CNAME &&
			       pj_stricmp(&query->srv[srv_idx].target_name, 
				          &rr->name)==0) 
		    {
			cname = rr->rdata.cname.name;
		    }
		}

		if (ans_idx == -1) {
		    /* There's no DNS A answer! */
		    PJ_LOG(5,(query->objname, 
			      "No DNS A record in response!"));
		    status = PJLIB_UTIL_EDNSNOANSWERREC;
		    goto on_error;
		}

		query->srv[srv_idx].addr[query->srv[srv_idx].addr_cnt++].s_addr =
		    pkt->ans[ans_idx].rdata.a.ip_addr.s_addr;

		PJ_LOG(5,(query->objname, 
			  "DNS A for %.*s: %s",
			  (int)query->srv[srv_idx].target_name.slen, 
			  query->srv[srv_idx].target_name.ptr,
			  pj_inet_ntoa(pkt->ans[ans_idx].rdata.a.ip_addr)));

		/* Check for multiple IP addresses */
		for (j=ans_idx+1; j<pkt->hdr.anscount && 
			    query->srv[srv_idx].addr_cnt < ADDR_MAX_COUNT; ++j)
		{
		    query->srv[srv_idx].addr[query->srv[srv_idx].addr_cnt++].s_addr = 
			pkt->ans[j].rdata.a.ip_addr.s_addr;

		    PJ_LOG(5,(query->objname, 
			      "Additional DNS A for %.*s: %s",
			      (int)query->srv[srv_idx].target_name.slen, 
			      query->srv[srv_idx].target_name.ptr,
			      pj_inet_ntoa(pkt->ans[j].rdata.a.ip_addr)));
		}
	    }

	} else if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    /* Update last error */
	    query->last_error = status;

	    /* Log error */
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(query->objname, "DNS A record resolution failed: %s", 
		      errmsg));
	}

	++query->host_resolved;

    } else {
	pj_assert(!"Unexpected state!");
	query->last_error = status = PJ_EINVALIDOP;
	goto on_error;
    }

    /* Check if all hosts have been resolved */
    if (query->host_resolved == query->srv_cnt) {
	/* Got all answers, build server addresses */
	pjsip_server_addresses svr_addr;

	svr_addr.count = 0;
	for (i=0; i<query->srv_cnt; ++i) {
	    unsigned j;

	    /* Do we have IP address for this server? */
	    /* This log is redundant really.
	    if (query->srv[i].addr_cnt == 0) {
		PJ_LOG(5,(query->objname, 
			  " SRV target %.*s:%d does not have IP address!",
			  (int)query->srv[i].target_name.slen,
			  query->srv[i].target_name.ptr,
			  query->srv[i].port));
		continue;
	    }
	    */

	    for (j=0; j<query->srv[i].addr_cnt; ++j) {
		unsigned idx = svr_addr.count;
		pj_sockaddr_in *addr;

		svr_addr.entry[idx].type = query->srv[i].type;
		svr_addr.entry[idx].priority = query->srv[i].priority;
		svr_addr.entry[idx].weight = query->srv[i].weight;
		svr_addr.entry[idx].addr_len = sizeof(pj_sockaddr_in);
	     
		addr = (pj_sockaddr_in*)&svr_addr.entry[idx].addr;
		pj_bzero(addr, sizeof(pj_sockaddr_in));
		addr->sin_family = PJ_AF_INET;
		addr->sin_addr = query->srv[i].addr[j];
		addr->sin_port = pj_htons((pj_uint16_t)query->srv[i].port);

		++svr_addr.count;
	    }
	}

	PJ_LOG(5,(query->objname, 
		  "Server resolution complete, %d server entry(s) found",
		  svr_addr.count));


	if (svr_addr.count > 0)
	    status = PJ_SUCCESS;
	else {
	    status = query->last_error;
	    if (status == PJ_SUCCESS)
		status = PJLIB_UTIL_EDNSNOANSWERREC;
	}

	/* Call the callback */
	(*query->cb)(status, query->token, &svr_addr);
    }


    return;

on_error:
    /* Check for failure */
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	PJ_LOG(4,(query->objname, 
		  "DNS %s record resolution error for '%.*s'."
		  " Err=%d (%s)",
		  pj_dns_get_type_name(query->dns_state),
		  (int)query->req.target.addr.host.slen,
		  query->req.target.addr.host.ptr,
		  status,
		  pj_strerror(status,errmsg,sizeof(errmsg)).ptr));
	(*query->cb)(status, query->token, NULL);
	return;
    }
}

#endif	/* PJSIP_HAS_RESOLVER */



