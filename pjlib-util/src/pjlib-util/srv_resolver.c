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
#include <pjlib-util/srv_resolver.h>
#include <pjlib-util/errno.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>


#define THIS_FILE   "srv_resolver.c"

#define ADDR_MAX_COUNT	    8

struct srv_target
{
    pj_str_t		    target_name;
    char		    target_buf[PJ_MAX_HOSTNAME];
    pj_str_t		    cname;
    char		    cname_buf[PJ_MAX_HOSTNAME];
    unsigned		    port;
    unsigned		    priority;
    unsigned		    weight;
    unsigned		    sum;
    unsigned		    addr_cnt;
    pj_in_addr		    addr[ADDR_MAX_COUNT];
};

typedef struct pj_dns_srv_resolver_job
{
    char		    *objname;

    pj_dns_resolver	    *resolver;	    /**< Resolver SIP instance.	    */
    pj_dns_type		     dns_state;	    /**< DNS type being resolved.   */
    void		    *token;
    pj_dns_srv_resolver_cb  *cb;
    pj_dns_async_query	    *qobject;
    pj_status_t		     last_error;

    /* Original request: */
    pj_bool_t		     fallback_a;
    pj_str_t		     full_name;
    pj_str_t		     domain_part;
    pj_uint16_t		     def_port;

    /* SRV records and their resolved IP addresses: */
    unsigned		     srv_cnt;
    struct srv_target	     srv[PJ_DNS_SRV_MAX_ADDR];

    /* Number of hosts in SRV records that the IP address has been resolved */
    unsigned		     host_resolved;

} pj_dns_srv_resolver_job;


/* Async resolver callback, forward decl. */
static void dns_callback(void *user_data,
			 pj_status_t status,
			 pj_dns_parsed_packet *pkt);



/*
 * The public API to invoke DNS SRV resolution.
 */
PJ_DEF(pj_status_t) pj_dns_srv_resolve( const pj_str_t *domain_name,
				        const pj_str_t *res_name,
					unsigned def_port,
					pj_pool_t *pool,
					pj_dns_resolver *resolver,
					pj_bool_t fallback_a,
					void *token,
					pj_dns_srv_resolver_cb *cb)
{
    int len;
    pj_str_t target_name;
    pj_dns_srv_resolver_job *query_job;
    pj_status_t status;

    PJ_ASSERT_RETURN(domain_name && domain_name->slen &&
		     res_name && res_name->slen &&
		     pool && resolver && cb, PJ_EINVAL);

    /* Build full name */
    len = domain_name->slen + res_name->slen + 2;
    target_name.ptr = (char*) pj_pool_alloc(pool, len);
    pj_strcpy(&target_name, res_name);
    if (res_name->ptr[res_name->slen-1] != '.')
	pj_strcat2(&target_name, ".");
    len = target_name.slen;
    pj_strcat(&target_name, domain_name);
    target_name.ptr[target_name.slen] = '\0';


    /* Build the query_job state */
    query_job = PJ_POOL_ZALLOC_T(pool, pj_dns_srv_resolver_job);
    query_job->objname = target_name.ptr;
    query_job->resolver = resolver;
    query_job->token = token;
    query_job->cb = cb;
    query_job->fallback_a = fallback_a;
    query_job->full_name = target_name;
    query_job->domain_part.ptr = target_name.ptr + len;
    query_job->domain_part.slen = target_name.slen - len;
    query_job->def_port = (pj_uint16_t)def_port;

    /* Start the asynchronous query_job */

    query_job->dns_state = PJ_DNS_TYPE_SRV;

    PJ_LOG(5, (query_job->objname, 
	       "Starting async DNS %s query_job: target=%.*",
	       pj_dns_get_type_name(query_job->dns_state),
	       (int)target_name.slen, target_name.ptr));

    status = pj_dns_resolver_start_query(resolver, &target_name, 
				         query_job->dns_state, 0, 
					 &dns_callback,
    					 query_job, &query_job->qobject);
    return status;
}



#define SWAP(type,ptr1,ptr2) if (ptr1 != ptr2) { \
				type tmp; \
				pj_memcpy(&tmp, ptr1, sizeof(type)); \
				pj_memcpy(ptr1, ptr2, sizeof(type)); \
				(ptr1)->target_name.ptr = (ptr1)->target_buf;\
				pj_memcpy(ptr2, &tmp, sizeof(type)); \
				(ptr2)->target_name.ptr = (ptr2)->target_buf;\
			     } else {}


/* Build server entries in the query_job based on received SRV response */
static void build_server_entries(pj_dns_srv_resolver_job *query_job, 
				 pj_dns_parsed_packet *response)
{
    unsigned i;

    /* Save the Resource Records in DNS answer into SRV targets. */
    query_job->srv_cnt = 0;
    for (i=0; i<response->hdr.anscount && 
	      query_job->srv_cnt < PJ_DNS_SRV_MAX_ADDR; ++i) 
    {
	pj_dns_parsed_rr *rr = &response->ans[i];
	struct srv_target *srv = &query_job->srv[query_job->srv_cnt];

	if (rr->type != PJ_DNS_TYPE_SRV) {
	    PJ_LOG(4,(query_job->objname, 
		      "Received non SRV answer for SRV query_job!"));
	    continue;
	}

	if (rr->rdata.srv.target.slen > PJ_MAX_HOSTNAME) {
	    PJ_LOG(4,(query_job->objname, "Hostname is too long!"));
	    continue;
	}

	/* Build the SRV entry for RR */
	pj_bzero(srv, sizeof(*srv));
	srv->target_name.ptr = srv->target_buf;
	pj_strncpy(&srv->target_name, &rr->rdata.srv.target,
		   sizeof(srv->target_buf));
	srv->port = rr->rdata.srv.port;
	srv->priority = rr->rdata.srv.prio;
	srv->weight = rr->rdata.srv.weight;
	
	++query_job->srv_cnt;
    }

    /* First pass: 
     *	order the entries based on priority.
     */
    for (i=0; i<query_job->srv_cnt-1; ++i) {
	unsigned min = i, j;
	for (j=i+1; j<query_job->srv_cnt; ++j) {
	    if (query_job->srv[j].priority < query_job->srv[min].priority)
		min = j;
	}
	SWAP(struct srv_target, &query_job->srv[i], &query_job->srv[min]);
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
    for (i=0; i<query_job->srv_cnt; ++i) {
	unsigned j, count=1, sum;

	/* Calculate running sum for servers with the same priority */
	sum = query_job->srv[i].sum = query_job->srv[i].weight;
	for (j=i+1; j<query_job->srv_cnt && 
		    query_job->srv[j].priority == query_job->srv[i].priority; ++j)
	{
	    sum += query_job->srv[j].weight;
	    query_job->srv[j].sum = sum;
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
		if (query_job->srv[j].sum >= r)
		    break;
	    }

	    /* Must have selected one! */
	    pj_assert(j != i+count);

	    /* Put this entry in front (of entries with same priority) */
	    SWAP(struct srv_target, &query_job->srv[i], &query_job->srv[j]);

	    /* Remove all other entries (of the same priority) */
	    while (count > 1) {
		pj_array_erase(query_job->srv, sizeof(struct srv_target), 
			       query_job->srv_cnt, i+1);
		--count;
		--query_job->srv_cnt;
	    }
	}
    }

    /* Since we've been moving around SRV entries, update the pointers
     * in target_name.
     */
    for (i=0; i<query_job->srv_cnt; ++i) {
	query_job->srv[i].target_name.ptr = query_job->srv[i].target_buf;
    }

    /* Check for Additional Info section if A records are available, and
     * fill in the IP address (so that we won't need to resolve the A 
     * record with another DNS query_job). 
     */
    for (i=0; i<response->hdr.arcount; ++i) {
	pj_dns_parsed_rr *rr = &response->arr[i];
	unsigned j;

	if (rr->type != PJ_DNS_TYPE_A)
	    continue;

	/* Yippeaiyee!! There is an "A" record! 
	 * Update the IP address of the corresponding SRV record.
	 */
	for (j=0; j<query_job->srv_cnt; ++j) {
	    if (pj_stricmp(&rr->name, &query_job->srv[j].target_name)==0) {
		unsigned cnt = query_job->srv[j].addr_cnt;
		query_job->srv[j].addr[cnt].s_addr = rr->rdata.a.ip_addr.s_addr;
		++query_job->srv[j].addr_cnt;
		++query_job->host_resolved;
		break;
	    }
	}

	/* Not valid message; SRV entry might have been deleted in
	 * server selection process.
	 */
	/*
	if (j == query_job->srv_cnt) {
	    PJ_LOG(4,(query_job->objname, 
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
    for (i=0; i<query_job->srv_cnt; ++i) {
	pj_in_addr addr;

	if (query_job->srv[i].addr_cnt != 0) {
	    /* IP address already resolved */
	    continue;
	}

	if (pj_inet_aton(&query_job->srv[i].target_name, &addr) != 0) {
	    query_job->srv[i].addr[query_job->srv[i].addr_cnt++] = addr;
	    ++query_job->host_resolved;
	}
    }

    /* Print resolved entries to the log */
    PJ_LOG(5,(query_job->objname, 
	      "SRV query_job for %.*s completed, "
	      "%d of %d total entries selected%c",
	      (int)query_job->full_name.slen,
	      query_job->full_name.ptr,
	      query_job->srv_cnt,
	      response->hdr.anscount,
	      (query_job->srv_cnt ? ':' : ' ')));

    for (i=0; i<query_job->srv_cnt; ++i) {
	const char *addr;

	if (query_job->srv[i].addr_cnt != 0)
	    addr = pj_inet_ntoa(query_job->srv[i].addr[0]);
	else
	    addr = "-";

	PJ_LOG(5,(query_job->objname, 
		  " %d: SRV %d %d %d %.*s (%s)",
		  i, query_job->srv[i].priority, 
		  query_job->srv[i].weight, 
		  query_job->srv[i].port, 
		  (int)query_job->srv[i].target_name.slen, 
		  query_job->srv[i].target_name.ptr,
		  addr));
    }
}


/* Start DNS A record queries for all SRV records in the query_job structure */
static pj_status_t resolve_hostnames(pj_dns_srv_resolver_job *query_job)
{
    unsigned i;
    pj_status_t err=PJ_SUCCESS, status;

    query_job->dns_state = PJ_DNS_TYPE_A;
    for (i=0; i<query_job->srv_cnt; ++i) {
	PJ_LOG(5, (query_job->objname, 
		   "Starting async DNS A query_job for %.*s",
		   (int)query_job->srv[i].target_name.slen, 
		   query_job->srv[i].target_name.ptr));

	status = pj_dns_resolver_start_query(query_job->resolver,
					     &query_job->srv[i].target_name,
					     PJ_DNS_TYPE_A, 0,
					     &dns_callback,
					     query_job, NULL);
	if (status != PJ_SUCCESS) {
	    query_job->host_resolved++;
	    err = status;
	}
    }
    
    return (query_job->host_resolved == query_job->srv_cnt) ? err : PJ_SUCCESS;
}

/* 
 * This callback is called by PJLIB-UTIL DNS resolver when asynchronous
 * query_job has completed (successfully or with error).
 */
static void dns_callback(void *user_data,
			 pj_status_t status,
			 pj_dns_parsed_packet *pkt)
{
    pj_dns_srv_resolver_job *query_job = (pj_dns_srv_resolver_job*) user_data;
    unsigned i;

    /* Proceed to next stage */

    if (query_job->dns_state == PJ_DNS_TYPE_SRV) {

	/* We are getting SRV response */

	if (status == PJ_SUCCESS && pkt->hdr.anscount != 0) {
	    /* Got SRV response, build server entry. If A records are available
	     * in additional records section of the DNS response, save them too.
	     */
	    build_server_entries(query_job, pkt);

	} else if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    /* Update query_job last error */
	    query_job->last_error = status;

	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(query_job->objname, 
		      "DNS SRV resolution failed for %.*s: %s", 
		      (int)query_job->full_name.slen, 
		      query_job->full_name.ptr,
		      errmsg));

	    /* Trigger error when fallback is disabled */
	    if (query_job->fallback_a == PJ_FALSE) {
		goto on_error;
	    }
	}

	/* If we can't build SRV record, assume the original target is
	 * an A record and resolve with DNS A resolution.
	 */
	if (query_job->srv_cnt == 0) {
	    /* Looks like we aren't getting any SRV responses.
	     * Resolve the original target as A record by creating a 
	     * single "dummy" srv record and start the hostname resolution.
	     */
	    PJ_LOG(4, (query_job->objname, 
		       "DNS SRV resolution failed for %.*s, trying "
		       "resolving A record for %.*s",
		       (int)query_job->full_name.slen, 
		       query_job->full_name.ptr,
		       (int)query_job->domain_part.slen,
		       query_job->domain_part.ptr));

	    /* Create a "dummy" srv record using the original target */
	    i = query_job->srv_cnt++;
	    pj_bzero(&query_job->srv[i], sizeof(query_job->srv[i]));
	    query_job->srv[i].target_name = query_job->domain_part;
	    query_job->srv[i].priority = 0;
	    query_job->srv[i].weight = 0;
	    query_job->srv[i].port = query_job->def_port;
	} 
	

	/* Resolve server hostnames (DNS A record) for hosts which don't have
	 * A record yet.
	 */
	if (query_job->host_resolved != query_job->srv_cnt) {
	    status = resolve_hostnames(query_job);
	    if (status != PJ_SUCCESS)
		goto on_error;

	    /* Must return now. Callback may have been called and query_job
	     * may have been destroyed.
	     */
	    return;
	}

    } else if (query_job->dns_state == PJ_DNS_TYPE_A) {

	/* Check that we really have answer */
	if (status==PJ_SUCCESS && pkt->hdr.anscount != 0) {
	    int ans_idx = -1;

	    /* Find the first DNS A record in the answer while processing
	     * the CNAME info found in the response.
	     */
	    for (i=0; i < pkt->hdr.anscount; ++i) {

		pj_dns_parsed_rr *rr = &pkt->ans[i];

		if (rr->type == PJ_DNS_TYPE_A) {

		    if (ans_idx == -1)
			ans_idx = i;

		} else if (rr->type == PJ_DNS_TYPE_CNAME) {
		    /* Find which server entry to be updated with
		     * the CNAME information.
		     */
		    unsigned j;
		    pj_str_t cname = rr->rdata.cname.name;

		    for (j=0; j<query_job->srv_cnt; ++j) 
		    {
			struct srv_target *srv = &query_job->srv[j];
			if (pj_stricmp(&rr->name, &srv->target_name)==0)
			{
			    /* Update CNAME info for this server entry */
			    srv->cname.ptr = srv->cname_buf;
			    pj_strncpy(&srv->cname, &cname, 
				       sizeof(srv->cname_buf));
			    break;
			}
		    }
		}
	    }

	    if (ans_idx == -1) {
		/* There's no DNS A answer! */
		PJ_LOG(5,(query_job->objname, 
			  "No DNS A record in response!"));
		status = PJLIB_UTIL_EDNSNOANSWERREC;
		goto on_error;
	    }

	    /* Update IP address of the corresponding hostname or CNAME */
	    for (i=0; i<query_job->srv_cnt; ++i) {
		pj_dns_parsed_rr *rr = &pkt->ans[ans_idx];
		struct srv_target *srv = &query_job->srv[i];

		if (pj_stricmp(&rr->name, &srv->target_name)==0 ||
		    pj_stricmp(&rr->name, &srv->cname)==0) 
		{
		    break;
		}
	    }

	    if (i == query_job->srv_cnt) {
		PJ_LOG(4,(query_job->objname, 
			  "Received answer to DNS A request with no matching "
			  "SRV record! The unknown name is %.*s",
			  (int)pkt->ans[ans_idx].name.slen, 
			  pkt->ans[ans_idx].name.ptr));
	    } else {
		unsigned j;

		query_job->srv[i].addr[query_job->srv[i].addr_cnt++].s_addr =
		    pkt->ans[ans_idx].rdata.a.ip_addr.s_addr;

		PJ_LOG(5,(query_job->objname, 
			  "DNS A for %.*s: %s",
			  (int)query_job->srv[i].target_name.slen, 
			  query_job->srv[i].target_name.ptr,
			  pj_inet_ntoa(pkt->ans[ans_idx].rdata.a.ip_addr)));

		/* Check for multiple IP addresses */
		for (j=ans_idx+1; j<pkt->hdr.anscount && 
			    query_job->srv[i].addr_cnt < ADDR_MAX_COUNT; ++j)
		{
		    query_job->srv[i].addr[query_job->srv[i].addr_cnt++].s_addr = 
			pkt->ans[j].rdata.a.ip_addr.s_addr;

		    PJ_LOG(5,(query_job->objname, 
			      "Additional DNS A for %.*s: %s",
			      (int)query_job->srv[i].target_name.slen, 
			      query_job->srv[i].target_name.ptr,
			      pj_inet_ntoa(pkt->ans[j].rdata.a.ip_addr)));
		}
	    }

	} else if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];

	    /* Update last error */
	    query_job->last_error = status;

	    /* Log error */
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(query_job->objname, "DNS A record resolution failed: %s", 
		      errmsg));
	}

	++query_job->host_resolved;

    } else {
	pj_assert(!"Unexpected state!");
	query_job->last_error = status = PJ_EINVALIDOP;
	goto on_error;
    }

    /* Check if all hosts have been resolved */
    if (query_job->host_resolved == query_job->srv_cnt) {
	/* Got all answers, build server addresses */
	pj_dns_srv_record svr_addr;

	svr_addr.count = 0;
	for (i=0; i<query_job->srv_cnt; ++i) {
	    unsigned j;

	    /* Do we have IP address for this server? */
	    /* This log is redundant really.
	    if (query_job->srv[i].addr_cnt == 0) {
		PJ_LOG(5,(query_job->objname, 
			  " SRV target %.*s:%d does not have IP address!",
			  (int)query_job->srv[i].target_name.slen,
			  query_job->srv[i].target_name.ptr,
			  query_job->srv[i].port));
		continue;
	    }
	    */

	    for (j=0; j<query_job->srv[i].addr_cnt; ++j) {
		unsigned idx = svr_addr.count;
		pj_sockaddr_in *addr;

		svr_addr.entry[idx].priority = query_job->srv[i].priority;
		svr_addr.entry[idx].weight = query_job->srv[i].weight;
		svr_addr.entry[idx].addr_len = sizeof(pj_sockaddr_in);
	     
		addr = (pj_sockaddr_in*)&svr_addr.entry[idx].addr;
		pj_bzero(addr, sizeof(pj_sockaddr_in));
		addr->sin_family = PJ_AF_INET;
		addr->sin_addr = query_job->srv[i].addr[j];
		addr->sin_port = pj_htons((pj_uint16_t)query_job->srv[i].port);

		++svr_addr.count;
	    }
	}

	PJ_LOG(5,(query_job->objname, 
		  "Server resolution complete, %d server entry(s) found",
		  svr_addr.count));


	if (svr_addr.count > 0)
	    status = PJ_SUCCESS;
	else {
	    status = query_job->last_error;
	    if (status == PJ_SUCCESS)
		status = PJLIB_UTIL_EDNSNOANSWERREC;
	}

	/* Call the callback */
	(*query_job->cb)(query_job->token, status, &svr_addr);
    }


    return;

on_error:
    /* Check for failure */
    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	PJ_UNUSED_ARG(errmsg);
	PJ_LOG(4,(query_job->objname, 
		  "DNS %s record resolution error for '%.*s'."
		  " Err=%d (%s)",
		  pj_dns_get_type_name(query_job->dns_state),
		  (int)query_job->domain_part.slen,
		  query_job->domain_part.ptr,
		  status,
		  pj_strerror(status,errmsg,sizeof(errmsg)).ptr));
	(*query_job->cb)(query_job->token, status, NULL);
	return;
    }
}


