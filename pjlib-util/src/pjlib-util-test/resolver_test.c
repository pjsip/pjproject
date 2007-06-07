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
#include <pjlib-util.h>
#include <pjlib.h>
#include "test.h"


#define THIS_FILE   "srv_resolver_test.c"

#define ACTION_REPLY	0
#define ACTION_IGNORE	-1
#define ACTION_CB	-2

static struct server_t
{
    pj_sock_t	     sock;
    pj_uint16_t	     port;
    pj_thread_t	    *thread;

    /* Action:
     *	0:    reply with the response in resp.
     * -1:    ignore query (to simulate timeout).
     * other: reply with that error
     */
    int		    action;

    pj_dns_parsed_packet    resp;
    void		  (*action_cb)(const pj_dns_parsed_packet *pkt,
				       pj_dns_parsed_packet **p_res);

    unsigned	    pkt_count;

} g_server[2];

static pj_pool_t *pool;
static pj_dns_resolver *resolver;
static pj_bool_t thread_quit;
static pj_timer_heap_t *timer_heap;
static pj_ioqueue_t *ioqueue;
static pj_thread_t *poll_thread;
static pj_sem_t *sem;
static pj_dns_settings set;

static int print_label(char *start, const pj_str_t *name)
{
    char *p = (char*) start;
    const char *startlabel, *endlabel;
    char *endname;

    /* Tokenize name */
    startlabel = endlabel = name->ptr;
    endname = name->ptr + name->slen;
    while (endlabel != endname) {
	while (endlabel != endname && *endlabel != '.')
	    ++endlabel;
	*p++ = (char)(endlabel - startlabel);
	pj_memcpy(p, startlabel, endlabel-startlabel);
	p += (endlabel-startlabel);
	if (endlabel != endname && *endlabel == '.')
	    ++endlabel;
	startlabel = endlabel;
    }
    *p++ = '\0';

    return p-start;
}

static int print_packet(const pj_dns_parsed_packet *rec, char *packet)
{
    pj_dns_hdr *hdr;
    char *p;
    int i, len;

    /* Initialize header */
    hdr = (pj_dns_hdr*) packet;
    pj_bzero(hdr, sizeof(pj_dns_hdr));
    hdr->id = pj_htons(rec->hdr.id);
    hdr->flags = pj_htons(rec->hdr.flags);
    hdr->qdcount = pj_htons(rec->hdr.qdcount);
    hdr->anscount = pj_htons(rec->hdr.anscount);
    hdr->nscount = pj_htons(rec->hdr.nscount);
    hdr->arcount = pj_htons(rec->hdr.arcount);

    p = packet + sizeof(pj_dns_hdr);

    /* Print queries */
    for (i=0; i<rec->hdr.qdcount; ++i) {
	pj_uint16_t tmp;

	len = print_label(p, &rec->q[i].name);
	p += len;

	/* Set type */
	tmp = pj_htons((pj_uint16_t)rec->q[i].type);
	pj_memcpy(p, &tmp, 2);
	p += 2;

	/* Set class (IN=1) */
	tmp = pj_htons(rec->q[i].dnsclass);
	pj_memcpy(p, &tmp, 2);
	p += 2;
    }

    /* Print answers */
    for (i=0; i<rec->hdr.anscount; ++i) {
	const pj_dns_parsed_rr *rr = &rec->ans[i];
	pj_uint16_t tmp;
	pj_uint32_t ttl;

	len = print_label(p, &rr->name);
	p += len;

	/* Set type */
	tmp = pj_htons((pj_uint16_t)rr->type);
	pj_memcpy(p, &tmp, 2);
	p += 2;

	/* Set class */
	tmp = pj_htons((pj_uint16_t)rr->dnsclass);
	pj_memcpy(p, &tmp, 2);
	p += 2;

	/* Set TTL */
	ttl = pj_htonl(rr->ttl);
	pj_memcpy(p, &ttl, 4);
	p += 4;

	if (rr->type == PJ_DNS_TYPE_A) {

	    /* RDLEN is 4 */
	    tmp = pj_htons(4);
	    pj_memcpy(p, &tmp, 2);
	    p += 2;

	    /* Address */
	    pj_memcpy(p, &rr->rdata.a.ip_addr, 4);
	    p += 4;

	} else if (rr->type == PJ_DNS_TYPE_CNAME ||
		   rr->type == PJ_DNS_TYPE_NS) {

	    len = print_label(p+2, &rr->rdata.cname.name);

	    tmp = pj_htons((pj_uint16_t)len);
	    pj_memcpy(p, &tmp, 2);

	    p += (len + 2);

	} else if (rr->type == PJ_DNS_TYPE_SRV) {

	    /* Skip RDLEN (will write later) */
	    char *p_rdlen = p;

	    p += 2;

	    /* Priority */
	    tmp = pj_htons(rr->rdata.srv.prio);
	    pj_memcpy(p, &tmp, 2);
	    p += 2;

	    /* Weight */
	    tmp = pj_htons(rr->rdata.srv.weight);
	    pj_memcpy(p, &tmp, 2);
	    p += 2;

	    /* Port */
	    tmp = pj_htons(rr->rdata.srv.port);
	    pj_memcpy(p, &tmp, 2);
	    p += 2;

	    /* Target */
	    len = print_label(p, &rr->rdata.srv.target);

	    /* Now print RDLEN */
	    tmp = pj_htons((pj_uint16_t)(len + 6));
	    pj_memcpy(p_rdlen, &tmp, 2);

	    p += len;

	} else {
	    pj_assert(!"Not supported");
	}
    }

    return p - packet;
}


static int server_thread(void *p)
{
    struct server_t *srv = (struct server_t*)p;

    while (!thread_quit) {
	pj_fd_set_t rset;
	pj_time_val timeout = {0, 500};
	pj_sockaddr_in src_addr;
	pj_dns_parsed_packet *req;
	char pkt[1024];
	pj_ssize_t pkt_len;
	int rc, src_len;

	PJ_FD_ZERO(&rset);
	PJ_FD_SET(srv->sock, &rset);

	rc = pj_sock_select(srv->sock+1, &rset, NULL, NULL, &timeout);
	if (rc != 1)
	    continue;

	src_len = sizeof(src_addr);
	pkt_len = sizeof(pkt);
	rc = pj_sock_recvfrom(srv->sock, pkt, &pkt_len, 0, 
			      &src_addr, &src_len);
	if (rc != 0) {
	    app_perror("Server error receiving packet", rc);
	    continue;
	}

	PJ_LOG(5,(THIS_FILE, "Server %d processing packet", srv - &g_server[0]));
	srv->pkt_count++;

	rc = pj_dns_parse_packet(pool, pkt, pkt_len, &req);
	if (rc != PJ_SUCCESS) {
	    app_perror("server error parsing packet", rc);
	    continue;
	}

	/* Simulate network RTT */
	pj_thread_sleep(50);

	if (srv->action == ACTION_IGNORE) {
	    continue;
	} else if (srv->action == ACTION_REPLY) {
	    srv->resp.hdr.id = req->hdr.id;
	    pkt_len = print_packet(&srv->resp, pkt);
	    pj_sock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
	} else if (srv->action == ACTION_CB) {
	    pj_dns_parsed_packet *resp;
	    (*srv->action_cb)(req, &resp);
	    resp->hdr.id = req->hdr.id;
	    pkt_len = print_packet(resp, pkt);
	    pj_sock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
	} else if (srv->action > 0) {
	    req->hdr.flags |= PJ_DNS_SET_RCODE(srv->action);
	    pkt_len = print_packet(req, pkt);
	    pj_sock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
	}
    }

    return 0;
}

static int poll_worker_thread(void *p)
{
    PJ_UNUSED_ARG(p);

    while (!thread_quit) {
	pj_time_val delay = {0, 100};
	pj_timer_heap_poll(timer_heap, NULL);
	pj_ioqueue_poll(ioqueue, &delay);
    }

    return 0;
}

static void destroy(void);

static int init(void)
{
    pj_status_t status;
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    int i;

    nameservers[0] = pj_str("127.0.0.1");
    ports[0] = 553;
    nameservers[1] = pj_str("127.0.0.1");
    ports[1] = 554;

    g_server[0].port = ports[0];
    g_server[1].port = ports[1];

    pool = pj_pool_create(mem, NULL, 2000, 2000, NULL);

    status = pj_sem_create(pool, NULL, 0, 2, &sem);
    pj_assert(status == PJ_SUCCESS);

    for (i=0; i<2; ++i) {
	pj_sockaddr_in addr;

	status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_DGRAM, 0, &g_server[i].sock);
	if (status != PJ_SUCCESS)
	    return -10;

	pj_sockaddr_in_init(&addr, NULL, (pj_uint16_t)g_server[i].port);

	status = pj_sock_bind(g_server[i].sock, &addr, sizeof(addr));
	if (status != PJ_SUCCESS)
	    return -20;

	status = pj_thread_create(pool, NULL, &server_thread, &g_server[i],
				  0, 0, &g_server[i].thread);
	if (status != PJ_SUCCESS)
	    return -30;
    }

    status = pj_timer_heap_create(pool, 16, &timer_heap);
    pj_assert(status == PJ_SUCCESS);

    status = pj_ioqueue_create(pool, 16, &ioqueue);
    pj_assert(status == PJ_SUCCESS);

    status = pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue, &resolver);
    if (status != PJ_SUCCESS)
	return -40;

    pj_dns_resolver_get_settings(resolver, &set);
    set.good_ns_ttl = 20;
    set.bad_ns_ttl = 20;
    pj_dns_resolver_set_settings(resolver, &set);

    status = pj_dns_resolver_set_ns(resolver, 2, nameservers, ports);
    pj_assert(status == PJ_SUCCESS);

    status = pj_thread_create(pool, NULL, &poll_worker_thread, NULL, 0, 0, &poll_thread);
    pj_assert(status == PJ_SUCCESS);

    return 0;
}


static void destroy(void)
{
    int i;

    thread_quit = PJ_TRUE;

    for (i=0; i<2; ++i) {
	pj_thread_join(g_server[i].thread);
	pj_sock_close(g_server[i].sock);
    }

    pj_thread_join(poll_thread);

    pj_dns_resolver_destroy(resolver, PJ_FALSE);
    pj_ioqueue_destroy(ioqueue);
    pj_timer_heap_destroy(timer_heap);

    pj_sem_destroy(sem);
    pj_pool_release(pool);
}


////////////////////////////////////////////////////////////////////////////
/* Simple DNS test */
#define IP_ADDR0    0x00010203

static void dns_callback(void *user_data,
			 pj_status_t status,
			 pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);

    pj_assert(status == PJ_SUCCESS);
    pj_assert(resp);
    pj_assert(resp->hdr.anscount == 1);
    pj_assert(resp->ans[0].type == PJ_DNS_TYPE_A);
    pj_assert(resp->ans[0].rdata.a.ip_addr.s_addr == IP_ADDR0);

    pj_sem_post(sem);
}


static int simple_test(void)
{
    pj_str_t name = pj_str("helloworld");
    pj_dns_parsed_packet *r;
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "    simple successful test"));

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    g_server[0].action = ACTION_REPLY;
    r = &g_server[0].resp;
    r->hdr.qdcount = 1;
    r->hdr.anscount = 1;
    r->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    r->q[0].type = PJ_DNS_TYPE_A;
    r->q[0].dnsclass = 1;
    r->q[0].name = name;
    r->ans = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_rr);
    r->ans[0].type = PJ_DNS_TYPE_A;
    r->ans[0].dnsclass = 1;
    r->ans[0].name = name;
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;

    g_server[1].action = ACTION_REPLY;
    r = &g_server[1].resp;
    r->hdr.qdcount = 1;
    r->hdr.anscount = 1;
    r->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    r->q[0].type = PJ_DNS_TYPE_A;
    r->q[0].dnsclass = 1;
    r->q[0].name = name;
    r->ans = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_rr);
    r->ans[0].type = PJ_DNS_TYPE_A;
    r->ans[0].dnsclass = 1;
    r->ans[0].name = name;
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;

    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);
    pj_thread_sleep(1000);


    /* Both servers must get packet */
    pj_assert(g_server[0].pkt_count == 1);
    pj_assert(g_server[1].pkt_count == 1);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* DNS nameserver fail-over test */

static void dns_callback_1b(void *user_data,
			    pj_status_t status,
			    pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(resp);

    pj_assert(status == PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_NXDOMAIN));

    pj_sem_post(sem);
}




/* DNS test */
static int dns_test(void)
{
    pj_str_t name = pj_str("name00");
    pj_status_t status;

    PJ_LOG(3,(THIS_FILE, "    simple error response test"));

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* Now only server 0 should get packet, since both servers are
     * in STATE_ACTIVE state
     */
    pj_assert(g_server[0].pkt_count == 1);
    pj_assert(g_server[1].pkt_count == 0);

    /* Wait to allow probing period to complete */
    PJ_LOG(3,(THIS_FILE, "    waiting for active NS to expire (%d sec)",
			 set.good_ns_ttl));
    pj_thread_sleep(set.good_ns_ttl * 1000);

    /* 
     * Fail-over test 
     */
    PJ_LOG(3,(THIS_FILE, "    failing server0"));
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name01");
    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);

    /*
     * Check that both servers still receive requests, since they are
     * in probing state.
     */
    PJ_LOG(3,(THIS_FILE, "    checking both NS during probing period"));
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name02");
    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);
    pj_thread_sleep(set.qretr_delay *  set.qretr_count);

    /* Both servers must get requests */
    pj_assert(g_server[0].pkt_count >= 1);
    pj_assert(g_server[1].pkt_count == 1);

    /* Wait to allow probing period to complete */
    PJ_LOG(3,(THIS_FILE, "    waiting for probing state to end (%d sec)",
			 set.qretr_delay * 
			 (set.qretr_count+2) / 1000));
    pj_thread_sleep(set.qretr_delay * (set.qretr_count + 2));


    /*
     * Now only server 1 should get requests.
     */
    PJ_LOG(3,(THIS_FILE, "    verifying only good NS is used"));
    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name03");
    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* Both servers must get requests */
    pj_assert(g_server[0].pkt_count == 0);
    pj_assert(g_server[1].pkt_count == 1);

    /* Wait to allow probing period to complete */
    PJ_LOG(3,(THIS_FILE, "    waiting for active NS to expire (%d sec)",
			 set.good_ns_ttl));
    pj_thread_sleep(set.good_ns_ttl * 1000);

    /*
     * Now fail server 1 to switch to server 0
     */
    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = ACTION_IGNORE;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name04");
    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);

    /* Wait to allow probing period to complete */
    PJ_LOG(3,(THIS_FILE, "    waiting for probing state (%d sec)",
			 set.qretr_delay * (set.qretr_count+2) / 1000));
    pj_thread_sleep(set.qretr_delay * (set.qretr_count + 2));

    /*
     * Now only server 0 should get requests.
     */
    PJ_LOG(3,(THIS_FILE, "    verifying good NS"));
    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = ACTION_IGNORE;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name05");
    status = pj_dns_resolver_start_query(resolver, &name, PJ_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != PJ_SUCCESS)
	return -1000;

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* Both servers must get requests */
    pj_assert(g_server[0].pkt_count == 1);
    pj_assert(g_server[1].pkt_count == 0);


    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Resolver test, normal, with CNAME */
#define IP_ADDR1    0x02030405

static void action1_1(const pj_dns_parsed_packet *pkt,
		      pj_dns_parsed_packet **p_res)
{
    pj_dns_parsed_packet *res;
    char *target = "sip.somedomain.com";

    res = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);

    if (res->q == NULL) {
	res->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    }
    if (res->ans == NULL) {
	res->ans = (pj_dns_parsed_rr*) 
		  pj_pool_calloc(pool, 4, sizeof(pj_dns_parsed_rr));
    }

    res->hdr.qdcount = 1;
    res->q[0].type = pkt->q[0].type;
    res->q[0].dnsclass = pkt->q[0].dnsclass;
    res->q[0].name = pkt->q[0].name;

    if (pkt->q[0].type == PJ_DNS_TYPE_SRV) {

	pj_assert(pj_strcmp2(&pkt->q[0].name, "_sip._udp.somedomain.com")==0);

	res->hdr.anscount = 1;
	res->ans[0].type = PJ_DNS_TYPE_SRV;
	res->ans[0].dnsclass = 1;
	res->ans[0].name = res->q[0].name;
	res->ans[0].ttl = 1;
	res->ans[0].rdata.srv.prio = 1;
	res->ans[0].rdata.srv.weight = 2;
	res->ans[0].rdata.srv.port = 5061;
	res->ans[0].rdata.srv.target = pj_str(target);

    } else if (pkt->q[0].type == PJ_DNS_TYPE_A) {
	char *alias = "sipalias.somedomain.com";

	pj_assert(pj_strcmp2(&res->q[0].name, target)==0);

	res->hdr.anscount = 2;
	res->ans[0].type = PJ_DNS_TYPE_CNAME;
	res->ans[0].dnsclass = 1;
	res->ans[0].ttl = 1000;	/* resolver should select minimum TTL */
	res->ans[0].name = res->q[0].name;
	res->ans[0].rdata.cname.name = pj_str(alias);

	res->ans[1].type = PJ_DNS_TYPE_A;
	res->ans[1].dnsclass = 1;
	res->ans[1].ttl = 1;
	res->ans[1].name = pj_str(alias);
	res->ans[1].rdata.a.ip_addr.s_addr = IP_ADDR1;
    }

    *p_res = res;
}

static void srv_cb_1(void *user_data,
		     pj_status_t status,
		     const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    pj_assert(status == PJ_SUCCESS);
    pj_assert(rec->count == 1);
    pj_assert(rec->entry[0].priority == 1);
    pj_assert(rec->entry[0].weight == 2);
    pj_assert(rec->entry[0].addr.ipv4.sin_addr.s_addr == IP_ADDR1);
    pj_assert(pj_ntohs(rec->entry[0].addr.ipv4.sin_port) == 5061);

    pj_sem_post(sem);
}

static void srv_cb_1b(void *user_data,
		      pj_status_t status,
		      const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    pj_assert(status == PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_NXDOMAIN));
    pj_assert(rec->count == 0);

    pj_sem_post(sem);
}

static int srv_resolver_test(void)
{
    pj_status_t status;
    pj_str_t domain = pj_str("somedomain.com");
    pj_str_t res_name = pj_str("_sip._udp.");

    /* Successful scenario */
    PJ_LOG(3,(THIS_FILE, "    srv_resolve(): success scenario"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action1_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action1_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = pj_dns_srv_resolve(&domain, &res_name, 5061, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_1);
    pj_assert(status == PJ_SUCCESS);

    pj_sem_wait(sem);

    /* Both servers should receive requests since state should be probing */
    pj_assert(g_server[0].pkt_count == 2);  /* 2 because of SRV and A resolution */
    pj_assert(g_server[1].pkt_count == 0);


    /* Wait until cache expires and nameserver state moves out from STATE_PROBING */
    PJ_LOG(3,(THIS_FILE, "    waiting for cache to expire (~15 secs).."));
    pj_thread_sleep(1000 + 
		    ((set.qretr_count + 2) * set.qretr_delay));

    /* Successful scenario */
    PJ_LOG(3,(THIS_FILE, "    srv_resolve(): parallel queries"));
    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = pj_dns_srv_resolve(&domain, &res_name, 5061, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_1);
    pj_assert(status == PJ_SUCCESS);


    status = pj_dns_srv_resolve(&domain, &res_name, 5061, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_1);
    pj_assert(status == PJ_SUCCESS);

    pj_sem_wait(sem);
    pj_sem_wait(sem);

    /* Only server one should get a query */
    pj_assert(g_server[0].pkt_count == 2);  /* 2 because of SRV and A resolution */
    pj_assert(g_server[1].pkt_count == 0);

    /* Since TTL is one, subsequent queries should fail */
    PJ_LOG(3,(THIS_FILE, "    srv_resolve(): cache expires scenario"));


    pj_thread_sleep(1000);

    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    status = pj_dns_srv_resolve(&domain, &res_name, 5061, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_1b);
    pj_assert(status == PJ_SUCCESS);

    pj_sem_wait(sem);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Fallback because there's no SRV in answer */
#define TARGET	    "domain2.com"
#define IP_ADDR2    0x02030405

static void action2_1(const pj_dns_parsed_packet *pkt,
		      pj_dns_parsed_packet **p_res)
{
    static pj_dns_parsed_packet res;

    if (res.q == NULL) {
	res.q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    }
    if (res.ans == NULL) {
	res.ans = (pj_dns_parsed_rr*) 
		  pj_pool_calloc(pool, 4, sizeof(pj_dns_parsed_rr));
    }

    res.hdr.qdcount = 1;
    res.q[0].type = pkt->q[0].type;
    res.q[0].dnsclass = pkt->q[0].dnsclass;
    res.q[0].name = pkt->q[0].name;

    if (pkt->q[0].type == PJ_DNS_TYPE_SRV) {

	pj_assert(pj_strcmp2(&pkt->q[0].name, "_sip._udp." TARGET)==0);

	res.hdr.anscount = 1;
	res.ans[0].type = PJ_DNS_TYPE_A;    // <-- this will cause the fallback
	res.ans[0].dnsclass = 1;
	res.ans[0].name = res.q[0].name;
	res.ans[0].ttl = 1;
	res.ans[0].rdata.srv.prio = 1;
	res.ans[0].rdata.srv.weight = 2;
	res.ans[0].rdata.srv.port = 5062;
	res.ans[0].rdata.srv.target = pj_str("sip01." TARGET);

    } else if (pkt->q[0].type == PJ_DNS_TYPE_A) {
	char *alias = "sipalias.somedomain.com";

	pj_assert(pj_strcmp2(&res.q[0].name, TARGET)==0);

	res.hdr.anscount = 2;
	res.ans[0].type = PJ_DNS_TYPE_CNAME;
	res.ans[0].dnsclass = 1;
	res.ans[0].name = res.q[0].name;
	res.ans[0].ttl = 1;
	res.ans[0].rdata.cname.name = pj_str(alias);

	res.ans[1].type = PJ_DNS_TYPE_A;
	res.ans[1].dnsclass = 1;
	res.ans[1].name = pj_str(alias);
	res.ans[1].ttl = 1;
	res.ans[1].rdata.a.ip_addr.s_addr = IP_ADDR2;
    }

    *p_res = &res;
}

static void srv_cb_2(void *user_data,
		     pj_status_t status,
		     const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    pj_assert(status == PJ_SUCCESS);
    pj_assert(rec->count == 1);
    pj_assert(rec->entry[0].priority == 0);
    pj_assert(rec->entry[0].weight == 0);
    pj_assert(rec->entry[0].addr.ipv4.sin_addr.s_addr == IP_ADDR2);
    pj_assert(pj_ntohs(rec->entry[0].addr.ipv4.sin_port) == 5062);

    pj_sem_post(sem);
}

static int srv_resolver_fallback_test(void)
{
    pj_status_t status;
    pj_str_t domain = pj_str(TARGET);
    pj_str_t res_name = pj_str("_sip._udp.");

    PJ_LOG(3,(THIS_FILE, "    srv_resolve(): fallback test"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action2_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action2_1;

    status = pj_dns_srv_resolve(&domain, &res_name, 5062, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_2);
    if (status != PJ_SUCCESS) {
	app_perror("     srv_resolve error", status);
	pj_assert(status == PJ_SUCCESS);
    }

    pj_sem_wait(sem);

    /* Subsequent query should just get the response from the cache */
    PJ_LOG(3,(THIS_FILE, "    srv_resolve(): cache test"));
    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = pj_dns_srv_resolve(&domain, &res_name, 5062, pool, resolver, PJ_TRUE,
				NULL, &srv_cb_2);
    if (status != PJ_SUCCESS) {
	app_perror("     srv_resolve error", status);
	pj_assert(status == PJ_SUCCESS);
    }

    pj_sem_wait(sem);

    pj_assert(g_server[0].pkt_count == 0);
    pj_assert(g_server[1].pkt_count == 0);

    return 0;
}


////////////////////////////////////////////////////////////////////////////


int resolver_test(void)
{
    int rc;
    
#ifdef NDEBUG
    PJ_LOG(3,(THIS_FILE, "    error: NDEBUG is declared"));
    return -1;
#endif

    rc = init();

    rc = simple_test();
    if (rc != 0)
	goto on_error;

    rc = dns_test();
    if (rc != 0)
	goto on_error;

    srv_resolver_test();
    srv_resolver_fallback_test();

    destroy();
    return 0;

on_error:
    destroy();
    return rc;
}


