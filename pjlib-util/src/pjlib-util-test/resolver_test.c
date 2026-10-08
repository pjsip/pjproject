/* 
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
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
#include <pjlib-util.h>
#include <pjlib.h>
#include "test.h"


#define THIS_FILE   "resolver_test.c"

////////////////////////////////////////////////////////////////////////////
/*
 * TODO: create various invalid DNS packets.
 */


////////////////////////////////////////////////////////////////////////////


#define ACTION_REPLY    0
#define ACTION_IGNORE   -1
#define ACTION_CB       -2

#undef s6_addr32
#define s6_addr32(addr, idx) *((pj_uint32_t *)(addr.s6_addr + idx*4))

static struct server_t
{
    pj_sock_t        sock;
    pj_uint16_t      port;
    pj_thread_t     *thread;

    /* Action:
     *  0:    reply with the response in resp.
     * -1:    ignore query (to simulate timeout).
     * other: reply with that error
     */
    int             action;

    pj_dns_parsed_packet    resp;
    void                  (*action_cb)(const pj_dns_parsed_packet *pkt,
                                       pj_dns_parsed_packet **p_res);

    unsigned        pkt_count;

} g_server[2];

static pj_pool_t *pool;
static pj_mutex_t *mutex;
static pj_dns_resolver *resolver;
static pj_bool_t thread_quit;
static pj_timer_heap_t *timer_heap;
static pj_ioqueue_t *ioqueue;
static pj_thread_t *poll_thread;
static pj_sem_t *sem;
static pj_dns_settings set;
static volatile pj_bool_t destroy_done;
static volatile pj_bool_t cb_after_destroy;
static pj_dns_resolver *start_during_destroy_res;
static pj_status_t start_during_destroy_status;
static pj_dns_async_query *start_during_destroy_query;
static int cancel_in_cb_count;
static pj_dns_async_query *cancel_in_cb_query;
static pj_bool_t cancel_in_cb_cancelled;
static int cancel_in_tocb_count;
static pj_dns_async_query *cancel_in_tocb_query;
static pj_bool_t cancel_in_tocb_cancelled;
static pj_status_t cancel_in_tocb_status;
static int cancel_child_parent_count;
static int cancel_child_child_count;
static pj_dns_async_query *cancel_child_query;
static pj_bool_t cancel_child_cancelled;
static pj_status_t cancel_child_status;
static pj_dns_resolver *cancel_unlocked_res;
static pj_sem_t *cancel_unlocked_entered;
static pj_sem_t *cancel_unlocked_probed;
static pj_bool_t cancel_unlocked_probe_seen;
static int cancel_unlocked_count;

#define MAX_LABEL   32

struct label_tab
{
    unsigned count;

    struct {
        unsigned pos;
        pj_str_t label;
    } a[MAX_LABEL];
};

static void lock()
{
    pj_mutex_lock(mutex);
}

static void unlock()
{
    pj_mutex_unlock(mutex);
}

static void write16(pj_uint8_t *p, pj_uint16_t val)
{
    p[0] = (pj_uint8_t)(val >> 8);
    p[1] = (pj_uint8_t)(val & 0xFF);
}

static void write32(pj_uint8_t *p, pj_uint32_t val)
{
    val = pj_htonl(val);
    pj_memcpy(p, &val, 4);
}

static int print_name(pj_uint8_t *pkt, int size,
                      pj_uint8_t *pos, const pj_str_t *name,
                      struct label_tab *tab)
{
    pj_uint8_t *p = pos;
    const char *endlabel, *endname;
    unsigned i;
    pj_str_t label;

    /* Check if name is in the table */
    for (i=0; i<tab->count; ++i) {
        if (pj_strcmp(&tab->a[i].label, name)==0)
            break;
    }

    if (i != tab->count) {
        write16(p, (pj_uint16_t)(tab->a[i].pos | (0xc0 << 8)));
        return 2;
    } else {
        if (tab->count < MAX_LABEL) {
            tab->a[tab->count].pos = (unsigned)(p-pkt);
            tab->a[tab->count].label.ptr = (char*)(p+1);
            tab->a[tab->count].label.slen = name->slen;
            ++tab->count;
        }
    }

    endlabel = name->ptr;
    endname = name->ptr + name->slen;

    label.ptr = (char*)name->ptr;

    while (endlabel != endname) {

        while (endlabel != endname && *endlabel != '.')
            ++endlabel;

        label.slen = (endlabel - label.ptr);

        if (size < label.slen+1)
            return -1;

        *p = (pj_uint8_t)label.slen;
        pj_memcpy(p+1, label.ptr, label.slen);

        size -= (int)(label.slen+1);
        p += (label.slen+1);

        if (endlabel != endname && *endlabel == '.')
            ++endlabel;
        label.ptr = (char*)endlabel;
    }

    if (size == 0)
        return -1;

    *p++ = '\0';

    return (int)(p-pos);
}

static int print_rr(pj_uint8_t *pkt, int size, pj_uint8_t *pos,
                    const pj_dns_parsed_rr *rr, struct label_tab *tab)
{
    pj_uint8_t *p = pos;
    int len;

    len = print_name(pkt, size, pos, &rr->name, tab);
    if (len < 0)
        return -1;

    p += len;
    size -= len;

    if (size < 8)
        return -1;

    PJ_TEST_EQ(rr->dnsclass, 1, NULL, return -1);

    write16(p+0, (pj_uint16_t)rr->type);        /* type     */
    write16(p+2, (pj_uint16_t)rr->dnsclass);    /* class    */
    write32(p+4, rr->ttl);                      /* TTL      */

    p += 8;
    size -= 8;

    if (rr->type == PJ_DNS_TYPE_A) {

        if (size < 6)
            return -1;

        /* RDLEN is 4 */
        write16(p, 4);

        /* Address */
        pj_memcpy(p+2, &rr->rdata.a.ip_addr, 4);

        p += 6;
        size -= 6;

    } else if (rr->type == PJ_DNS_TYPE_AAAA) {

        if (size < 18)
            return -1;

        /* RDLEN is 16 */
        write16(p, 16);

        /* Address */
        pj_memcpy(p+2, &rr->rdata.aaaa.ip_addr, 16);

        p += 18;
        size -= 18;

    } else if (rr->type == PJ_DNS_TYPE_CNAME ||
               rr->type == PJ_DNS_TYPE_NS ||
               rr->type == PJ_DNS_TYPE_PTR) {

        if (size < 4)
            return -1;

        len = print_name(pkt, size-2, p+2, &rr->rdata.cname.name, tab);
        if (len < 0)
            return -1;

        write16(p, (pj_uint16_t)len);

        p += (len + 2);
        size -= (len + 2);

    } else if (rr->type == PJ_DNS_TYPE_SRV) {

        if (size < 10)
            return -1;

        write16(p+2, rr->rdata.srv.prio);   /* Priority */
        write16(p+4, rr->rdata.srv.weight); /* Weight */
        write16(p+6, rr->rdata.srv.port);   /* Port */

        /* Target */
        len = print_name(pkt, size-8, p+8, &rr->rdata.srv.target, tab);
        if (len < 0)
            return -1;

        /* RDLEN */
        write16(p, (pj_uint16_t)(len + 6));

        p += (len + 8);
        size -= (len + 8);

    } else {
        pj_assert(!"Not supported");
        return -1;
    }

    return (int)(p-pos);
}

static int print_packet(const pj_dns_parsed_packet *rec, pj_uint8_t *pkt,
                        int size)
{
    pj_uint8_t *p = pkt;
    struct label_tab tab;
    int i, len;

    tab.count = 0;

#if 0
    pj_enter_critical_section();
    PJ_LOG(3,(THIS_FILE, "Sending response:"));
    pj_dns_dump_packet(rec);
    pj_leave_critical_section();
#endif

    if (size < (int)sizeof(pj_dns_hdr))
        return -1;

    /* Initialize header. This mock server only serializes responses, so set
     * the QR bit as a real server would.
     */
    write16(p+0,  rec->hdr.id);
    write16(p+2,  (pj_uint16_t)(rec->hdr.flags | PJ_DNS_SET_QR(1)));
    write16(p+4,  rec->hdr.qdcount);
    write16(p+6,  rec->hdr.anscount);
    write16(p+8,  rec->hdr.nscount);
    write16(p+10, rec->hdr.arcount);

    p = pkt + sizeof(pj_dns_hdr);
    size -= sizeof(pj_dns_hdr);

    /* Print queries */
    for (i=0; i<rec->hdr.qdcount; ++i) {

        len = print_name(pkt, size, p, &rec->q[i].name, &tab);
        if (len < 0)
            return -1;

        p += len;
        size -= len;

        if (size < 4)
            return -1;

        /* Set type */
        write16(p+0, (pj_uint16_t)rec->q[i].type);

        /* Set class (IN=1) */
        pj_assert(rec->q[i].dnsclass == 1);
        write16(p+2, rec->q[i].dnsclass);

        p += 4;
    }

    /* Print answers */
    for (i=0; i<rec->hdr.anscount; ++i) {
        len = print_rr(pkt, size, p, &rec->ans[i], &tab);
        if (len < 0)
            return -1;

        p += len;
        size -= len;
    }

    /* Print NS records */
    for (i=0; i<rec->hdr.nscount; ++i) {
        len = print_rr(pkt, size, p, &rec->ns[i], &tab);
        if (len < 0)
            return -1;

        p += len;
        size -= len;
    }

    /* Print additional records */
    for (i=0; i<rec->hdr.arcount; ++i) {
        len = print_rr(pkt, size, p, &rec->arr[i], &tab);
        if (len < 0)
            return -1;

        p += len;
        size -= len;
    }

    return (int)(p - pkt);
}


static int server_thread(void *p)
{
    struct server_t *srv = (struct server_t*)p;

    while (!thread_quit) {
        pj_fd_set_t rset;
        pj_time_val timeout = {0, 500};
        pj_sockaddr src_addr;
        pj_dns_parsed_packet *req;
        char pkt[1024];
        pj_ssize_t pkt_len;
        int rc, src_len;

        PJ_FD_ZERO(&rset);
        PJ_FD_SET(srv->sock, &rset);

        rc = pj_sock_select((int)(srv->sock+1), &rset, NULL, NULL, &timeout);
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

        PJ_LOG(5,(THIS_FILE, "Server %ld processing packet", srv - &g_server[0]));

        lock();
        rc = pj_dns_parse_packet(pool, pkt, (unsigned)pkt_len, &req);
        unlock();
        if (rc != PJ_SUCCESS) {
            app_perror("server error parsing packet", rc);
            continue;
        }

        srv->pkt_count++;

        /* Verify packet */
        if (req->hdr.qdcount != 1) {
            PJ_LOG(5,(THIS_FILE, "server receive multiple queries in a packet"));
            continue;
        }

        if (req->q[0].dnsclass != 1) {
            PJ_LOG(5,(THIS_FILE, "server receive query with invalid DNS class"));
            continue;
        }

        /* Simulate network RTT */
        pj_thread_sleep(50);

        if (srv->action == ACTION_IGNORE) {
            continue;
        } else if (srv->action == ACTION_REPLY) {
            srv->resp.hdr.id = req->hdr.id;
            pkt_len = print_packet(&srv->resp, (pj_uint8_t*)pkt, sizeof(pkt));
            pj_sock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
        } else if (srv->action == ACTION_CB) {
            pj_dns_parsed_packet *resp;
            (*srv->action_cb)(req, &resp);
            if (!resp)
                continue;
            resp->hdr.id = req->hdr.id;
            pkt_len = print_packet(resp, (pj_uint8_t*)pkt, sizeof(pkt));
            pj_sock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
        } else if (srv->action > 0) {
            req->hdr.flags |= PJ_DNS_SET_RCODE(srv->action);
            pkt_len = print_packet(req, (pj_uint8_t*)pkt, sizeof(pkt));
            pj_sock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
        }
    }

    return 0;
}

static int poll_worker_thread(void *p)
{
    PJ_UNUSED_ARG(p);

    while (!thread_quit) {
        pj_time_val delay = {0, 10};
        pj_timer_heap_poll(timer_heap, NULL);
        pj_ioqueue_poll(ioqueue, &delay);
    }

    return 0;
}

static void destroy(void);

static int init(pj_bool_t use_ipv6)
{
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    int i;

    if (use_ipv6) {
        nameservers[0] = pj_str("::1");
        nameservers[1] = pj_str("::1");
    } else {
        nameservers[0] = pj_str("127.0.0.1");
        nameservers[1] = pj_str("127.0.0.1");
    }

    pool = pj_pool_create(mem, NULL, 2000, 2000, NULL);

    PJ_TEST_SUCCESS(pj_mutex_create_simple(pool, "resolver_test", &mutex),
                    NULL, return -3);
    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 2, &sem), NULL, return -5);

    thread_quit = PJ_FALSE;

    for (i=0; i<2; ++i) {
        pj_sockaddr addr;
        int namelen;

        PJ_TEST_SUCCESS(pj_sock_socket(
                            (use_ipv6? pj_AF_INET6() : pj_AF_INET()),
                            pj_SOCK_DGRAM(), 0, &g_server[i].sock),
                        NULL, return -10);

        pj_sockaddr_init((use_ipv6? pj_AF_INET6() : pj_AF_INET()),
                         &addr, NULL, 0);

        PJ_TEST_SUCCESS(pj_sock_bind(g_server[i].sock, &addr,
                                     pj_sockaddr_get_len(&addr)),
                        NULL, return -20);

        namelen = sizeof(addr);
        PJ_TEST_SUCCESS(pj_sock_getsockname(g_server[i].sock, &addr,
                                            &namelen),
                        NULL, return -25);
        g_server[i].port = ports[i] = pj_sockaddr_get_port(&addr);

        PJ_TEST_SUCCESS(pj_thread_create(pool, NULL, &server_thread,
                                         &g_server[i],
                                         0, 0, &g_server[i].thread),
                        NULL, return -30);
    }

    PJ_TEST_SUCCESS(pj_timer_heap_create(pool, 16, &timer_heap), NULL, return -31);
    PJ_TEST_SUCCESS(pj_ioqueue_create(pool, 16, &ioqueue), NULL, return -32);
    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue, &resolver),
                    NULL, return -40);

    pj_dns_resolver_get_settings(resolver, &set);
    set.good_ns_ttl = 10;
    set.bad_ns_ttl = 10;
    pj_dns_resolver_set_settings(resolver, &set);

    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(resolver, 2, nameservers, ports),
                    NULL, return -41);
    PJ_TEST_SUCCESS(pj_thread_create(pool, NULL, &poll_worker_thread, NULL, 0, 0, &poll_thread),
                    NULL, return -42);

    return 0;
}


static void destroy(void)
{
    /* note: need to set global vars back to zero since test can be
     * repeated for IPv6
     */
    int i;

    thread_quit = PJ_TRUE;

    for (i=0; i<2; ++i) {
        pj_thread_join(g_server[i].thread);
        pj_sock_close(g_server[i].sock);
    }

    pj_thread_join(poll_thread);
    poll_thread = NULL;
    thread_quit = PJ_FALSE;

    pj_dns_resolver_destroy(resolver, PJ_FALSE);
    resolver = NULL;
    pj_ioqueue_destroy(ioqueue);
    ioqueue = NULL;
    pj_timer_heap_destroy(timer_heap);
    timer_heap = NULL;

    pj_sem_destroy(sem);
    sem = NULL;
    pj_mutex_destroy(mutex);
    mutex = NULL;
    pj_pool_release(pool);
    pool = NULL;

    pj_bzero(g_server, sizeof(g_server));
}


////////////////////////////////////////////////////////////////////////////
/* DNS A parser tests */
static int a_parser_test(void)
{
    pj_dns_parsed_packet pkt;
    pj_dns_a_record rec;

    PJ_LOG(3,(THIS_FILE, "  DNS A record parser tests"));

    pkt.q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    pkt.ans = (pj_dns_parsed_rr*)
              pj_pool_calloc(pool, 32, sizeof(pj_dns_parsed_rr));

    /* Simple answer with direct A record, but with addition of
     * a CNAME and another A to confuse the parser.
     */
    PJ_LOG(3,(THIS_FILE, "    A RR with duplicate CNAME/A"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 3;

    /* This is the RR corresponding to the query */
    pkt.ans[0].name = pj_str("ahost");
    pkt.ans[0].type = PJ_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x01020304;

    /* CNAME to confuse the parser */
    pkt.ans[1].name = pj_str("ahost");
    pkt.ans[1].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.cname.name = pj_str("bhost");

    /* DNS A RR to confuse the parser */
    pkt.ans[2].name = pj_str("bhost");
    pkt.ans[2].type = PJ_DNS_TYPE_A;
    pkt.ans[2].dnsclass = 1;
    pkt.ans[2].ttl = 1;
    pkt.ans[2].rdata.a.ip_addr.s_addr = 0x0203;


    PJ_TEST_SUCCESS(pj_dns_parse_a_response(&pkt, &rec), NULL, return -100)
    PJ_TEST_EQ(pj_strcmp2(&rec.name, "ahost"), 0, NULL, return -110);
    PJ_TEST_EQ(rec.alias.slen, 0, NULL, return -112);
    PJ_TEST_EQ(rec.addr_count, 1, NULL, return -114);
    PJ_TEST_EQ(rec.addr[0].s_addr, 0x01020304, NULL, return -116);

    /* Answer with the target corresponds to a CNAME entry, but not
     * as the first record, and with additions of some CNAME and A
     * entries to confuse the parser.
     */
    PJ_LOG(3,(THIS_FILE, "    CNAME RR with duplicate CNAME/A"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 4;

    /* This is the DNS A record for the alias */
    pkt.ans[0].name = pj_str("ahostalias");
    pkt.ans[0].type = PJ_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x02020202;

    /* CNAME entry corresponding to the query */
    pkt.ans[1].name = pj_str("ahost");
    pkt.ans[1].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.cname.name = pj_str("ahostalias");

    /* Another CNAME to confuse the parser */
    pkt.ans[2].name = pj_str("ahost");
    pkt.ans[2].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[2].dnsclass = 1;
    pkt.ans[2].ttl = 1;
    pkt.ans[2].rdata.cname.name = pj_str("ahostalias2");

    /* Another DNS A to confuse the parser */
    pkt.ans[3].name = pj_str("ahostalias2");
    pkt.ans[3].type = PJ_DNS_TYPE_A;
    pkt.ans[3].dnsclass = 1;
    pkt.ans[3].ttl = 1;
    pkt.ans[3].rdata.a.ip_addr.s_addr = 0x03030303;

    PJ_TEST_SUCCESS(pj_dns_parse_a_response(&pkt, &rec), NULL, return -120);
    PJ_TEST_EQ(pj_strcmp2(&rec.name, "ahost"), 0, NULL, return -122);
    PJ_TEST_EQ(pj_strcmp2(&rec.alias, "ahostalias"), 0, NULL, return -124);
    PJ_TEST_EQ(rec.addr_count, 1, NULL, return -126);
    PJ_TEST_EQ(rec.addr[0].s_addr, 0x02020202, NULL, return -128);

    /*
     * No query section.
     */
    PJ_LOG(3,(THIS_FILE, "    No query section"));
    pkt.hdr.qdcount = 0;
    pkt.hdr.anscount = 0;

    PJ_TEST_EQ(pj_dns_parse_a_response(&pkt, &rec), PJLIB_UTIL_EDNSINANSWER,
               NULL, return -130);

    /*
     * No answer section.
     */
    PJ_LOG(3,(THIS_FILE, "    No answer section"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 0;

    PJ_TEST_EQ(pj_dns_parse_a_response(&pkt, &rec), PJLIB_UTIL_EDNSNOANSWERREC,
               NULL, return -140);

    /*
     * Answer doesn't match query.
     */
    PJ_LOG(3,(THIS_FILE, "    Answer doesn't match query"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 1;

    /* An answer that doesn't match the query */
    pkt.ans[0].name = pj_str("ahostalias");
    pkt.ans[0].type = PJ_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x02020202;

    PJ_TEST_EQ(pj_dns_parse_a_response(&pkt, &rec), PJLIB_UTIL_EDNSNOANSWERREC,
               NULL, return -150);


    /*
     * DNS CNAME that doesn't have corresponding DNS A.
     */
    PJ_LOG(3,(THIS_FILE, "    CNAME with no matching DNS A RR (1)"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 1;

    /* The CNAME */
    pkt.ans[0].name = pj_str("ahost");
    pkt.ans[0].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.cname.name = pj_str("ahostalias");

    PJ_TEST_EQ(pj_dns_parse_a_response(&pkt, &rec), PJLIB_UTIL_EDNSNOANSWERREC,
               NULL, return -160);


    /*
     * DNS CNAME that doesn't have corresponding DNS A.
     */
    PJ_LOG(3,(THIS_FILE, "    CNAME with no matching DNS A RR (2)"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 2;

    /* The CNAME */
    pkt.ans[0].name = pj_str("ahost");
    pkt.ans[0].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.cname.name = pj_str("ahostalias");

    /* DNS A record, but the name doesn't match */
    pkt.ans[1].name = pj_str("ahost");
    pkt.ans[1].type = PJ_DNS_TYPE_A;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.a.ip_addr.s_addr = 0x01020304;

    PJ_TEST_EQ(pj_dns_parse_a_response(&pkt, &rec), PJLIB_UTIL_EDNSNOANSWERREC,
               NULL, return -170);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* DNS A/AAAA parser tests */
static int addr_parser_test(void)
{
    pj_dns_parsed_packet pkt;
    pj_dns_addr_record rec;

    PJ_LOG(3,(THIS_FILE, "  DNS A/AAAA record parser tests"));

    pkt.q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    pkt.ans = (pj_dns_parsed_rr*)
              pj_pool_calloc(pool, 32, sizeof(pj_dns_parsed_rr));

    /* Simple answer with direct A record, but with addition of
     * a CNAME and another A to confuse the parser.
     */
    PJ_LOG(3,(THIS_FILE, "    A RR with duplicate CNAME/A"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 4;

    /* This is the RR corresponding to the query */
    pkt.ans[0].name = pj_str("ahost");
    pkt.ans[0].type = PJ_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x01020304;

    /* CNAME to confuse the parser */
    pkt.ans[1].name = pj_str("ahost");
    pkt.ans[1].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.cname.name = pj_str("bhost");

    /* DNS A RR to confuse the parser */
    pkt.ans[2].name = pj_str("bhost");
    pkt.ans[2].type = PJ_DNS_TYPE_A;
    pkt.ans[2].dnsclass = 1;
    pkt.ans[2].ttl = 1;
    pkt.ans[2].rdata.a.ip_addr.s_addr = 0x0203;

    /* Additional RR corresponding to the query, DNS AAAA RR */
    pkt.ans[3].name = pj_str("ahost");
    pkt.ans[3].type = PJ_DNS_TYPE_AAAA;
    pkt.ans[3].dnsclass = 1;
    pkt.ans[3].ttl = 1;
    s6_addr32(pkt.ans[3].rdata.aaaa.ip_addr, 0) = 0x01020304;


    PJ_TEST_SUCCESS(pj_dns_parse_addr_response(&pkt, &rec), NULL, return -200);
    PJ_TEST_EQ(pj_strcmp2(&rec.name, "ahost"), 0, NULL, return -210);
    PJ_TEST_EQ(rec.alias.slen, 0, NULL, return -212);
    PJ_TEST_EQ(rec.addr_count, 2, NULL, return -214);
    PJ_TEST_EQ(rec.addr[0].af, pj_AF_INET(), NULL, return -220);
    PJ_TEST_EQ(rec.addr[0].ip.v4.s_addr, 0x01020304, NULL, return -222);
    PJ_TEST_EQ(rec.addr[1].af, pj_AF_INET6(), NULL, return -230);
    PJ_TEST_EQ(s6_addr32(rec.addr[1].ip.v6, 0), 0x01020304, NULL, return -232);

    /* Answer with the target corresponds to a CNAME entry, but not
     * as the first record, and with additions of some CNAME and A
     * entries to confuse the parser.
     */
    PJ_LOG(3,(THIS_FILE, "    CNAME RR with duplicate CNAME/A"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 4;

    /* This is the DNS A record for the alias */
    pkt.ans[0].name = pj_str("ahostalias");
    pkt.ans[0].type = PJ_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x02020202;

    /* CNAME entry corresponding to the query */
    pkt.ans[1].name = pj_str("ahost");
    pkt.ans[1].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.cname.name = pj_str("ahostalias");

    /* Another CNAME to confuse the parser */
    pkt.ans[2].name = pj_str("ahost");
    pkt.ans[2].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[2].dnsclass = 1;
    pkt.ans[2].ttl = 1;
    pkt.ans[2].rdata.cname.name = pj_str("ahostalias2");

    /* Another DNS A to confuse the parser */
    pkt.ans[3].name = pj_str("ahostalias2");
    pkt.ans[3].type = PJ_DNS_TYPE_A;
    pkt.ans[3].dnsclass = 1;
    pkt.ans[3].ttl = 1;
    pkt.ans[3].rdata.a.ip_addr.s_addr = 0x03030303;

    PJ_TEST_SUCCESS(pj_dns_parse_addr_response(&pkt, &rec), NULL, return -240);
    PJ_TEST_EQ(pj_strcmp2(&rec.name, "ahost"), 0, NULL, return -242);
    PJ_TEST_EQ(pj_strcmp2(&rec.alias, "ahostalias"), 0, NULL, return -244);
    PJ_TEST_EQ(rec.addr_count, 1, NULL, return -246);
    PJ_TEST_EQ(rec.addr[0].ip.v4.s_addr, 0x02020202, NULL, return -248);

    /*
     * No query section.
     */
    PJ_LOG(3,(THIS_FILE, "    No query section"));
    pkt.hdr.qdcount = 0;
    pkt.hdr.anscount = 0;

    PJ_TEST_EQ(pj_dns_parse_addr_response(&pkt, &rec),
               PJLIB_UTIL_EDNSINANSWER, NULL, return -245);

    /*
     * No answer section.
     */
    PJ_LOG(3,(THIS_FILE, "    No answer section"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 0;

    PJ_TEST_EQ(pj_dns_parse_addr_response(&pkt, &rec),
               PJLIB_UTIL_EDNSNOANSWERREC, NULL, return -250);

    /*
     * Answer doesn't match query.
     */
    PJ_LOG(3,(THIS_FILE, "    Answer doesn't match query"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 1;

    /* An answer that doesn't match the query */
    pkt.ans[0].name = pj_str("ahostalias");
    pkt.ans[0].type = PJ_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x02020202;

    PJ_TEST_EQ(pj_dns_parse_addr_response(&pkt, &rec),
               PJLIB_UTIL_EDNSNOANSWERREC, NULL, return -260);


    /*
     * DNS CNAME that doesn't have corresponding DNS A.
     */
    PJ_LOG(3,(THIS_FILE, "    CNAME with no matching DNS A RR (1)"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 1;

    /* The CNAME */
    pkt.ans[0].name = pj_str("ahost");
    pkt.ans[0].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.cname.name = pj_str("ahostalias");

    PJ_TEST_EQ(pj_dns_parse_addr_response(&pkt, &rec),
               PJLIB_UTIL_EDNSNOANSWERREC, NULL, return -270);


    /*
     * DNS CNAME that doesn't have corresponding DNS A.
     */
    PJ_LOG(3,(THIS_FILE, "    CNAME with no matching DNS A RR (2)"));
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = PJ_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = pj_str("ahost");
    pkt.hdr.anscount = 2;

    /* The CNAME */
    pkt.ans[0].name = pj_str("ahost");
    pkt.ans[0].type = PJ_DNS_TYPE_CNAME;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.cname.name = pj_str("ahostalias");

    /* DNS A record, but the name doesn't match */
    pkt.ans[1].name = pj_str("ahost");
    pkt.ans[1].type = PJ_DNS_TYPE_A;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.a.ip_addr.s_addr = 0x01020304;

    PJ_TEST_EQ(pj_dns_parse_addr_response(&pkt, &rec),
               PJLIB_UTIL_EDNSNOANSWERREC, NULL, return -280);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Simple DNS test */
#define IP_ADDR0    0x00010203

static void dns_callback(void *user_data,
                         pj_status_t status,
                         pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);

    pj_sem_post(sem);

    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, return);
    PJ_ASSERT_ON_FAIL(resp, return);
    PJ_ASSERT_ON_FAIL(resp->hdr.anscount == 1, return);
    PJ_ASSERT_ON_FAIL(resp->ans[0].type == PJ_DNS_TYPE_A, return);
    PJ_ASSERT_ON_FAIL(resp->ans[0].rdata.a.ip_addr.s_addr == IP_ADDR0, return);

}


static int simple_test(void)
{
    pj_str_t name = pj_str("helloworld");
    pj_dns_parsed_packet *r;

    PJ_LOG(3,(THIS_FILE, "  simple successful test"));

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

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        resolver, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback, NULL, NULL),
                    NULL, return -300);

    pj_sem_wait(sem);
    pj_thread_sleep((unsigned)(set.qretr_delay * 1.2));


    /* Both servers must get packet */
    PJ_TEST_EQ(g_server[0].pkt_count, 1, NULL, return -310);
    PJ_TEST_EQ(g_server[1].pkt_count, 1, NULL, return -320);

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

    pj_sem_post(sem);

    PJ_TEST_EQ(status, PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_NXDOMAIN),
               NULL, return);
}




/* Callback for the destroy-with-pending-query test: flags any invocation
 * that happens after the resolver has been destroyed.
 */
static void dns_callback_destroy(void *user_data,
                                 pj_status_t status,
                                 pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(status);
    PJ_UNUSED_ARG(resp);

    if (destroy_done)
        cb_after_destroy = PJ_TRUE;
}


/* Destroy the resolver while a query is still pending and the timer heap is
 * being polled by another thread. The query's retransmit timer must be
 * cancelled by pj_dns_resolver_destroy(); otherwise it fires after the
 * socket is closed and sends on a NULL udp_key (assert/abort), or, with
 * asserts disabled, invokes the callback after a destroy(notify=PJ_FALSE).
 */
static int dns_destroy_pending_test(void)
{
    pj_str_t name = pj_str("name_destroy");
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    pj_dns_resolver *res;

    PJ_LOG(3,(THIS_FILE, "  destroy with pending query test"));

    destroy_done = PJ_FALSE;
    cb_after_destroy = PJ_FALSE;

    /* Servers ignore the query so it stays pending and keeps retransmitting. */
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = ACTION_IGNORE;

    /* Use a dedicated resolver over the shared, polled timer heap and
     * ioqueue, so destroying it does not disturb the test harness.
     */
    nameservers[0] = nameservers[1] = pj_str("127.0.0.1");
    ports[0] = g_server[0].port;
    ports[1] = g_server[1].port;

    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue,
                                           &res),
                    NULL, return -500);
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(res, 2, nameservers, ports),
                    NULL, return -505);

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        res, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_destroy, NULL, NULL),
                    NULL, return -510);

    /* Destroy while the poll thread is still running. */
    destroy_done = PJ_TRUE;
    pj_dns_resolver_destroy(res, PJ_FALSE);

    /* Wait past the retransmit delay: a surviving timer would fire here. */
    pj_thread_sleep((unsigned)(set.qretr_delay * 2));

    PJ_TEST_EQ(cb_after_destroy, PJ_FALSE, NULL, return -520);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Changing the nameservers while a query is pending */

#define IP_ADDR4    0x04040404

static volatile pj_bool_t set_ns_cb_called;
static pj_status_t set_ns_cb_status;
static pj_uint32_t set_ns_cb_addr;

static void dns_callback_set_ns(void *user_data,
                                pj_status_t status,
                                pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);

    set_ns_cb_status = status;
    if (status == PJ_SUCCESS && resp && resp->hdr.anscount)
        set_ns_cb_addr = resp->ans[0].rdata.a.ip_addr.s_addr;
    set_ns_cb_called = PJ_TRUE;
}


/* A pending query must be retransmitted to the new nameserver. */
static int dns_set_ns_during_query_test(void)
{
    pj_str_t name = pj_str("name_set_ns");
    pj_str_t ns_addr = pj_str("127.0.0.1");
    pj_uint16_t port;
    pj_dns_parsed_packet *r;
    pj_dns_resolver *res;
    unsigned i;

    PJ_LOG(3,(THIS_FILE, "  set nameservers during query test"));

    set_ns_cb_called = PJ_FALSE;
    set_ns_cb_status = PJ_EUNKNOWN;
    set_ns_cb_addr = 0;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;
    g_server[0].action = ACTION_IGNORE;

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
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR4;

    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue,
                                           &res),
                    NULL, return -800);

    port = g_server[0].port;
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(res, 1, &ns_addr, &port),
                    NULL, { pj_dns_resolver_destroy(res, PJ_FALSE);
                            return -805; });

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        res, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_set_ns, NULL, NULL),
                    NULL, { pj_dns_resolver_destroy(res, PJ_FALSE);
                            return -810; });

    for (i = 0; i < 100 && g_server[0].pkt_count == 0; ++i)
        pj_thread_sleep(20);
    PJ_TEST_EQ(g_server[0].pkt_count, 1, NULL,
               { pj_dns_resolver_destroy(res, PJ_FALSE); return -815; });

    port = g_server[1].port;
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(res, 1, &ns_addr, &port),
                    NULL, { pj_dns_resolver_destroy(res, PJ_FALSE);
                            return -820; });

    for (i = 0; i < 100 && !set_ns_cb_called; ++i)
        pj_thread_sleep((unsigned)(set.qretr_delay / 20));

    pj_dns_resolver_destroy(res, PJ_FALSE);

    PJ_TEST_TRUE(set_ns_cb_called, NULL, return -825);
    PJ_TEST_SUCCESS(set_ns_cb_status, NULL, return -830);
    PJ_TEST_EQ(set_ns_cb_addr, IP_ADDR4, NULL, return -835);
    PJ_TEST_EQ(g_server[0].pkt_count, 1, NULL, return -840);
    PJ_TEST_GTE(g_server[1].pkt_count, 1, NULL, return -845);

    return 0;
}


/* Clearing the cache, also from the callback of a cached answer */

#define IP_ADDR5    0x05050505
#define IP_ADDR6    0x06060606

static pj_dns_resolver *clear_cache_res;
static volatile pj_bool_t clear_cache_cb_called;
static pj_status_t clear_cache_cb_status;
static pj_uint32_t clear_cache_cb_addr;

static void dns_callback_clear_cache(void *user_data,
                                     pj_status_t status,
                                     pj_dns_parsed_packet *resp)
{
    if (user_data)
        pj_dns_resolver_clear_cache(clear_cache_res);

    clear_cache_cb_status = status;
    if (status == PJ_SUCCESS && resp && resp->hdr.anscount)
        clear_cache_cb_addr = resp->ans[0].rdata.a.ip_addr.s_addr;
    clear_cache_cb_called = PJ_TRUE;
}

static int clear_cache_query(const char *name_str, pj_uint32_t addr,
                             pj_bool_t clear_in_cb)
{
    pj_str_t name = pj_str((char*)name_str);
    unsigned i;

    clear_cache_cb_called = PJ_FALSE;
    clear_cache_cb_status = PJ_EUNKNOWN;
    clear_cache_cb_addr = 0;

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        clear_cache_res, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_clear_cache,
                        (clear_in_cb ? &clear_cache_res : NULL), NULL),
                    NULL, return -1);

    for (i = 0; i < 100 && !clear_cache_cb_called; ++i)
        pj_thread_sleep(20);

    PJ_TEST_TRUE(clear_cache_cb_called, NULL, return -1);
    PJ_TEST_SUCCESS(clear_cache_cb_status, NULL, return -1);
    PJ_TEST_EQ(clear_cache_cb_addr, addr, NULL, return -1);
    return 0;
}

static int dns_clear_cache_test(void)
{
    pj_str_t name = pj_str("name_clear_cache");
    pj_str_t static_name = pj_str("name_static");
    pj_str_t ns_addr = pj_str("127.0.0.1");
    pj_uint16_t port;
    pj_dns_parsed_packet *r, static_pkt;
    pj_dns_parsed_rr static_rr;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  clear cache test"));

    g_server[0].pkt_count = 0;
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
    r->ans[0].ttl = 60;
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR5;

    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue,
                                           &clear_cache_res),
                    NULL, return -850);

    port = g_server[0].port;
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(clear_cache_res, 1, &ns_addr,
                                           &port),
                    NULL, { rc = -855; goto on_return; });

    /* An entry added without TTL must survive the clearing */
    pj_bzero(&static_pkt, sizeof(static_pkt));
    pj_bzero(&static_rr, sizeof(static_rr));
    static_pkt.hdr.flags = PJ_DNS_SET_QR(1);
    static_pkt.hdr.anscount = 1;
    static_pkt.ans = &static_rr;
    static_rr.type = PJ_DNS_TYPE_A;
    static_rr.dnsclass = 1;
    static_rr.name = static_name;
    static_rr.rdata.a.ip_addr.s_addr = IP_ADDR6;
    PJ_TEST_SUCCESS(pj_dns_resolver_add_entry(clear_cache_res, &static_pkt,
                                              PJ_FALSE),
                    NULL, { rc = -857; goto on_return; });

    if (clear_cache_query("name_clear_cache", IP_ADDR5, PJ_FALSE)) {
        rc = -860;
        goto on_return;
    }
    PJ_TEST_EQ(pj_dns_resolver_get_cached_count(clear_cache_res), 2, NULL,
               { rc = -865; goto on_return; });

    /* The callback reads the answer after the cache has been cleared */
    if (clear_cache_query("name_clear_cache", IP_ADDR5, PJ_TRUE)) {
        rc = -870;
        goto on_return;
    }
    PJ_TEST_EQ(g_server[0].pkt_count, 1, NULL, { rc = -875; goto on_return; });
    PJ_TEST_EQ(pj_dns_resolver_get_cached_count(clear_cache_res), 1, NULL,
               { rc = -880; goto on_return; });

    if (clear_cache_query("name_static", IP_ADDR6, PJ_FALSE)) {
        rc = -882;
        goto on_return;
    }
    if (clear_cache_query("name_clear_cache", IP_ADDR5, PJ_FALSE)) {
        rc = -885;
        goto on_return;
    }
    PJ_TEST_EQ(g_server[0].pkt_count, 2, NULL, { rc = -890; goto on_return; });

    /* Nothing holds the entries now, so they are freed at once */
    PJ_TEST_SUCCESS(pj_dns_resolver_clear_cache(clear_cache_res), NULL,
                    { rc = -892; goto on_return; });
    PJ_TEST_EQ(pj_dns_resolver_get_cached_count(clear_cache_res), 1, NULL,
               { rc = -895; goto on_return; });

on_return:
    pj_dns_resolver_destroy(clear_cache_res, PJ_FALSE);
    clear_cache_res = NULL;
    return rc;
}


/* Callback for the start-during-destroy test: starts a new query from
 * inside the PJ_ECANCELLED notification delivered by
 * pj_dns_resolver_destroy(), the way the SRV resolver's fallback does.
 */
static void dns_callback_start_during_destroy(void *user_data,
                                              pj_status_t status,
                                              pj_dns_parsed_packet *resp)
{
    pj_str_t name = pj_str("name_restart");

    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(resp);

    if (status != PJ_ECANCELLED)
        return;

    /* Seed the out-pointer with a dummy, as the SRV resolver does, to
     * verify that a rejected start still writes it.
     */
    start_during_destroy_query = (pj_dns_async_query*)0x1;
    start_during_destroy_status =
        pj_dns_resolver_start_query(start_during_destroy_res, &name,
                                    PJ_DNS_TYPE_A, 0,
                                    &dns_callback_destroy, NULL,
                                    &start_during_destroy_query);
}


/* Start a query from inside destroy's cancel notification: the resolver
 * must reject it with PJ_EGONE, write the out-pointer, and leave no timer
 * armed past the teardown.
 */
static int dns_start_during_destroy_test(void)
{
    pj_str_t name = pj_str("name_sdd");
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];

    PJ_LOG(3,(THIS_FILE, "  start query during destroy test"));

    destroy_done = PJ_FALSE;
    cb_after_destroy = PJ_FALSE;
    start_during_destroy_status = PJ_SUCCESS;
    start_during_destroy_query = NULL;

    /* Servers ignore the query so it stays pending. */
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = ACTION_IGNORE;

    nameservers[0] = nameservers[1] = pj_str("127.0.0.1");
    ports[0] = g_server[0].port;
    ports[1] = g_server[1].port;

    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue,
                                           &start_during_destroy_res),
                    NULL, return -720);
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(start_during_destroy_res, 2,
                                           nameservers, ports),
                    NULL, return -725);

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        start_during_destroy_res, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_start_during_destroy, NULL, NULL),
                    NULL, return -730);

    destroy_done = PJ_TRUE;
    pj_dns_resolver_destroy(start_during_destroy_res, PJ_TRUE);

    /* The cancel notification must have run and its re-entrant start must
     * have been rejected with the out-pointer cleared.
     */
    PJ_TEST_EQ(start_during_destroy_status, PJ_EGONE, NULL, return -740);
    PJ_TEST_TRUE(start_during_destroy_query == NULL, NULL, return -745);

    /* Wait past the retransmit delay: a timer armed by the rejected start
     * would fire here.
     */
    pj_thread_sleep((unsigned)(set.qretr_delay * 2));

    PJ_TEST_EQ(cb_after_destroy, PJ_FALSE, NULL, return -750);

    return 0;
}


/* Callback for the cancel-in-callback test: on the first delivery it cancels
 * its own query with notify=PJ_TRUE. Before the callback is captured and
 * cleared under the group lock, the query still held a live cb, so cancel
 * delivered a second (PJ_ECANCELLED) callback for the same query.
 */
static void dns_callback_cancel_in_cb(void *user_data,
                                      pj_status_t status,
                                      pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(status);
    PJ_UNUSED_ARG(resp);

    cancel_in_cb_count++;

    if (cancel_in_cb_count == 1 && cancel_in_cb_query) {
        pj_dns_async_query *q = cancel_in_cb_query;
        cancel_in_cb_query = NULL;
        cancel_in_cb_cancelled = PJ_TRUE;
        pj_dns_resolver_cancel_query(q, PJ_TRUE);
    }

    pj_sem_post(sem);
}


/* Cancelling a query from inside its own completion callback must not deliver
 * the callback a second time (the cb is captured and cleared under the lock
 * before it is invoked). Asserts the callback runs exactly once.
 */
static int dns_cancel_in_callback_test(void)
{
    pj_str_t name = pj_str("name_cancel_in_cb");

    PJ_LOG(3,(THIS_FILE, "  cancel query from within its callback test"));

    cancel_in_cb_count = 0;
    cancel_in_cb_query = NULL;
    cancel_in_cb_cancelled = PJ_FALSE;

    /* Servers answer so the query completes and the callback fires. */
    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        resolver, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_cancel_in_cb, NULL, &cancel_in_cb_query),
                    NULL, return -670);

    pj_sem_wait(sem);

    /* Guard against a vacuous pass: the assertions below only pin the
     * behavior if the cancel actually ran inside the callback.
     */
    PJ_TEST_EQ(cancel_in_cb_cancelled, PJ_TRUE, NULL, return -672);
    PJ_TEST_EQ(cancel_in_cb_count, 1, NULL, return -674);

    return 0;
}


/* Same as dns_callback_cancel_in_cb, but for the timeout variant below. */
static void dns_callback_cancel_in_tocb(void *user_data,
                                        pj_status_t status,
                                        pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(resp);

    cancel_in_tocb_count++;

    if (cancel_in_tocb_count == 1) {
        cancel_in_tocb_status = status;
        if (cancel_in_tocb_query) {
            pj_dns_async_query *q = cancel_in_tocb_query;
            cancel_in_tocb_query = NULL;
            cancel_in_tocb_cancelled = PJ_TRUE;
            pj_dns_resolver_cancel_query(q, PJ_TRUE);
        }
    }

    pj_sem_post(sem);
}


/* Same as dns_cancel_in_callback_test, but through the timeout path: the
 * servers do not respond, so the callback is delivered by on_timeout()
 * rather than on_read_complete(). Uses a dedicated resolver with short
 * retransmission settings to keep the timeout fast.
 */
static int dns_cancel_in_timeout_callback_test(void)
{
    pj_str_t name = pj_str("name_cancel_in_tocb");
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    pj_dns_settings lset;
    pj_dns_resolver *res;

    PJ_LOG(3,(THIS_FILE, "  cancel query from within its timeout callback test"));

    cancel_in_tocb_count = 0;
    cancel_in_tocb_query = NULL;
    cancel_in_tocb_cancelled = PJ_FALSE;
    cancel_in_tocb_status = PJ_SUCCESS;

    /* Servers ignore the query so it times out. */
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = ACTION_IGNORE;

    nameservers[0] = nameservers[1] = pj_str("127.0.0.1");
    ports[0] = g_server[0].port;
    ports[1] = g_server[1].port;

    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue,
                                           &res),
                    NULL, return -650);
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(res, 2, nameservers, ports),
                    NULL, return -651);

    pj_dns_resolver_get_settings(res, &lset);
    lset.qretr_delay = 200;
    lset.qretr_count = 1;
    PJ_TEST_SUCCESS(pj_dns_resolver_set_settings(res, &lset),
                    NULL, return -652);

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        res, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_cancel_in_tocb, NULL,
                        &cancel_in_tocb_query),
                    NULL, return -653);

    pj_sem_wait(sem);

    PJ_TEST_EQ(cancel_in_tocb_status, PJ_ETIMEDOUT, NULL, return -654);
    PJ_TEST_EQ(cancel_in_tocb_cancelled, PJ_TRUE, NULL, return -655);
    PJ_TEST_EQ(cancel_in_tocb_count, 1, NULL, return -656);

    pj_dns_resolver_destroy(res, PJ_FALSE);

    return 0;
}


static void dns_callback_cancel_child_parent(void *user_data,
                                             pj_status_t status,
                                             pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(status);
    PJ_UNUSED_ARG(resp);

    cancel_child_parent_count++;

    pj_sem_post(sem);
}

/* Callback of the coalesced child query; cancels its own query on the first
 * delivery, like dns_callback_cancel_in_cb.
 */
static void dns_callback_cancel_child(void *user_data,
                                      pj_status_t status,
                                      pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(resp);

    cancel_child_child_count++;

    if (cancel_child_child_count == 1) {
        cancel_child_status = status;
        if (cancel_child_query) {
            pj_dns_async_query *q = cancel_child_query;
            cancel_child_query = NULL;
            cancel_child_cancelled = PJ_TRUE;
            pj_dns_resolver_cancel_query(q, PJ_TRUE);
        }
    }

    pj_sem_post(sem);
}


/* Same again, but cancelling a coalesced CHILD query from inside its own
 * callback, to cover the child (ccb) capture loop. The servers ignore the
 * query, so the parent stays pending in the resolver's hash tables until
 * its timeout fires; the second start_query() below is therefore guaranteed
 * to join it as a child, and both callbacks are delivered by the same
 * on_timeout() pass.
 */
static int dns_cancel_child_in_callback_test(void)
{
    pj_str_t name = pj_str("name_cancel_child_tocb");
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    pj_dns_settings lset;
    pj_dns_resolver *res;

    PJ_LOG(3,(THIS_FILE, "  cancel child query from within its callback test"));

    cancel_child_parent_count = 0;
    cancel_child_child_count = 0;
    cancel_child_query = NULL;
    cancel_child_cancelled = PJ_FALSE;
    cancel_child_status = PJ_SUCCESS;

    /* Servers ignore the query so it times out. */
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = ACTION_IGNORE;

    nameservers[0] = nameservers[1] = pj_str("127.0.0.1");
    ports[0] = g_server[0].port;
    ports[1] = g_server[1].port;

    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue,
                                           &res),
                    NULL, return -660);
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(res, 2, nameservers, ports),
                    NULL, return -661);

    pj_dns_resolver_get_settings(res, &lset);
    lset.qretr_delay = 200;
    lset.qretr_count = 1;
    PJ_TEST_SUCCESS(pj_dns_resolver_set_settings(res, &lset),
                    NULL, return -662);

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        res, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_cancel_child_parent, NULL, NULL),
                    NULL, return -663);
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        res, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_cancel_child, NULL,
                        &cancel_child_query),
                    NULL, return -664);

    /* One delivery for the parent, one for the child. */
    pj_sem_wait(sem);
    pj_sem_wait(sem);

    PJ_TEST_EQ(cancel_child_status, PJ_ETIMEDOUT, NULL, return -665);
    PJ_TEST_EQ(cancel_child_cancelled, PJ_TRUE, NULL, return -666);
    PJ_TEST_EQ(cancel_child_child_count, 1, NULL, return -667);
    PJ_TEST_EQ(cancel_child_parent_count, 1, NULL, return -668);

    pj_dns_resolver_destroy(res, PJ_FALSE);

    return 0;
}



/* Probe thread for the cancel-unlocked test: once the callback signals that
 * it is running, take the resolver group lock through a read-only API. This
 * blocks until the callback returns if the callback still holds the lock.
 */
static int cancel_unlocked_probe_thread(void *arg)
{
    pj_dns_settings lset;

    PJ_UNUSED_ARG(arg);

    pj_sem_wait(cancel_unlocked_entered);
    pj_dns_resolver_get_settings(cancel_unlocked_res, &lset);
    pj_sem_post(cancel_unlocked_probed);

    return 0;
}


/* Callback delivered by pj_dns_resolver_cancel_query(): signals the probe
 * thread, then waits for it to acquire the resolver lock. The probe can only
 * succeed while this callback is running if the lock was released before the
 * callback was invoked.
 */
static void dns_callback_cancel_unlocked(void *user_data,
                                         pj_status_t status,
                                         pj_dns_parsed_packet *resp)
{
    unsigned i;

    PJ_UNUSED_ARG(user_data);
    PJ_UNUSED_ARG(status);
    PJ_UNUSED_ARG(resp);

    cancel_unlocked_count++;

    pj_sem_post(cancel_unlocked_entered);

    /* Bounded wait, so the test fails rather than hangs when the callback
     * is invoked under the lock.
     */
    for (i=0; i<500; ++i) {
        if (pj_sem_trywait(cancel_unlocked_probed) == PJ_SUCCESS) {
            cancel_unlocked_probe_seen = PJ_TRUE;
            break;
        }
        pj_thread_sleep(10);
    }
}


/* pj_dns_resolver_cancel_query() must invoke the application callback with
 * the group lock released, as every other delivery site does, so that a
 * callback re-entering the resolver or taking another lock cannot deadlock
 * or invert the lock order.
 */
static int dns_cancel_unlocked_test(void)
{
    pj_str_t name = pj_str("name_cancel_unlocked");
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    pj_dns_async_query *q = NULL;
    pj_thread_t *probe = NULL;

    PJ_LOG(3,(THIS_FILE, "  cancel query delivers callback unlocked test"));

    cancel_unlocked_count = 0;
    cancel_unlocked_probe_seen = PJ_FALSE;

    /* Servers ignore the query so it stays pending until cancelled. */
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = ACTION_IGNORE;

    nameservers[0] = nameservers[1] = pj_str("127.0.0.1");
    ports[0] = g_server[0].port;
    ports[1] = g_server[1].port;

    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue,
                                           &cancel_unlocked_res),
                    NULL, return -760);
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(cancel_unlocked_res, 2,
                                           nameservers, ports),
                    NULL, return -761);

    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 1, &cancel_unlocked_entered),
                    NULL, return -762);
    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 1, &cancel_unlocked_probed),
                    NULL, return -763);
    PJ_TEST_SUCCESS(pj_thread_create(pool, NULL, &cancel_unlocked_probe_thread,
                                     NULL, 0, 0, &probe),
                    NULL, return -764);

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        cancel_unlocked_res, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_cancel_unlocked, NULL, &q),
                    NULL, return -765);

    pj_dns_resolver_cancel_query(q, PJ_TRUE);

    pj_thread_join(probe);
    pj_thread_destroy(probe);
    pj_sem_destroy(cancel_unlocked_entered);
    pj_sem_destroy(cancel_unlocked_probed);
    pj_dns_resolver_destroy(cancel_unlocked_res, PJ_FALSE);

    /* The cancelled query was already on the wire. Let the servers consume it
     * while they are still ignoring, so that it is not mistaken for the first
     * query of whichever test installs an action_cb next.
     */
    pj_thread_sleep(1000);

    PJ_TEST_EQ(cancel_unlocked_count, 1, NULL, return -766);
    PJ_TEST_EQ(cancel_unlocked_probe_seen, PJ_TRUE, NULL, return -767);

    return 0;
}

/* DNS test */
static int dns_test(void)
{
    pj_str_t name = pj_str("name00");
    enum { D = 2 };

    PJ_LOG(3,(THIS_FILE, "  simple error response test"));

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        resolver, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_1b, NULL, NULL),
                    NULL, return -400);

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* Now only one of the servers should get packet, since both servers are
     * in STATE_ACTIVE state
     */
    PJ_TEST_EQ(g_server[0].pkt_count + g_server[1].pkt_count, 1,
               NULL, return -410);

    /* Wait to allow active period to complete and get into probing state */
    PJ_LOG(3,(THIS_FILE, "  waiting for active NS to expire (%d sec)",
                         set.good_ns_ttl));
    pj_thread_sleep((set.good_ns_ttl+D) * 1000);

    /* 
     * Fail-over test 
     */
    PJ_LOG(3,(THIS_FILE, "  failing server0"));
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name01");
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                            resolver, &name, PJ_DNS_TYPE_A, 0,
                            &dns_callback_1b, NULL, NULL),
                    NULL, return -420);

    pj_sem_wait(sem);

    /* Both servers must get packet as both are in probing state */
    PJ_TEST_GTE(g_server[0].pkt_count, 1, NULL, return -430);
    PJ_TEST_EQ(g_server[1].pkt_count, 1, NULL, return -435);

    /*
     * Check that both servers still receive requests, since they are
     * in probing & active state.
     */
    PJ_LOG(3,(THIS_FILE, "  checking both NS during probing period"));
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name02");
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                            resolver, &name, PJ_DNS_TYPE_A, 0,
                            &dns_callback_1b, NULL, NULL),
                    NULL, return -440);

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* Both servers must get packet as both are in probing & active state */
    PJ_TEST_GTE(g_server[0].pkt_count, 1, NULL, return -450);
    PJ_TEST_EQ(g_server[1].pkt_count, 1, NULL, return -454);

    /* Wait to allow probing period to complete, server 0 will be in bad state */
    PJ_LOG(3,(THIS_FILE, "  waiting for probing state to end (%d sec)",
                         set.qretr_delay * 
                         (set.qretr_count+2) / 1000));
    pj_thread_sleep(1000 + set.qretr_delay * (set.qretr_count + 2));


    /*
     * Now only server 1 should get requests.
     */
    PJ_LOG(3,(THIS_FILE, "  verifying only good NS is used"));
    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name03");
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        resolver, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_1b, NULL, NULL),
                    NULL, return -460);

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* Only server 1 get the request */
    PJ_TEST_EQ(g_server[0].pkt_count, 0, NULL, return -470);
    PJ_TEST_EQ(g_server[1].pkt_count, 1, NULL, return -474);

    /* Wait to allow active & bad period to complete, both will be in probing state */
    PJ_LOG(3,(THIS_FILE, "  waiting for active NS to expire (%d sec)",
                         set.good_ns_ttl));
    pj_thread_sleep((set.good_ns_ttl+D) * 1000);

    /*
     * Now fail server 1 to switch to server 0
     */
    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = ACTION_IGNORE;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name04");
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        resolver, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_1b, NULL, NULL),
                    NULL, return -480);

    pj_sem_wait(sem);

    /* Wait to allow probing period to complete, server 0 remains active, server 1 will be bad */
    PJ_LOG(3,(THIS_FILE, "  waiting for probing state (%d sec)",
                         set.qretr_delay * (set.qretr_count+2) / 1000));
    pj_thread_sleep(1000 + set.qretr_delay * (set.qretr_count + 2));

    /*
     * Now only server 0 should get requests.
     */
    PJ_LOG(3,(THIS_FILE, "  verifying good NS"));
    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = ACTION_IGNORE;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = pj_str("name05");
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(
                        resolver, &name, PJ_DNS_TYPE_A, 0,
                        &dns_callback_1b, NULL, NULL),
                    NULL, return -484);

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* Only good NS should get request */
    PJ_TEST_EQ(g_server[0].pkt_count, 1, NULL, return -486);
    PJ_TEST_EQ(g_server[1].pkt_count, 0, NULL, return -488);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Resolver test, normal, with CNAME */
#define IP_ADDR1    0x02030405
#define PORT1       50061

static void action1_1(const pj_dns_parsed_packet *pkt,
                      pj_dns_parsed_packet **p_res)
{
    pj_dns_parsed_packet *res;
    char *target = "sip.somedomain.com";

    lock();
    res = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);

    if (res->q == NULL) {
        res->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    }
    if (res->ans == NULL) {
        res->ans = (pj_dns_parsed_rr*) 
                  pj_pool_calloc(pool, 4, sizeof(pj_dns_parsed_rr));
    }
    unlock();

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
        res->ans[0].rdata.srv.port = PORT1;
        res->ans[0].rdata.srv.target = pj_str(target);

    } else if (pkt->q[0].type == PJ_DNS_TYPE_A) {
        char *alias = "sipalias.somedomain.com";

        pj_assert(pj_strcmp2(&res->q[0].name, target)==0);

        res->hdr.anscount = 2;
        res->ans[0].type = PJ_DNS_TYPE_CNAME;
        res->ans[0].dnsclass = 1;
        res->ans[0].ttl = 1000; /* resolver should select minimum TTL */
        res->ans[0].name = res->q[0].name;
        res->ans[0].rdata.cname.name = pj_str(alias);

        res->ans[1].type = PJ_DNS_TYPE_A;
        res->ans[1].dnsclass = 1;
        res->ans[1].ttl = 1;
        res->ans[1].name = pj_str(alias);
        res->ans[1].rdata.a.ip_addr.s_addr = IP_ADDR1;

    } else if (pkt->q[0].type == PJ_DNS_TYPE_AAAA) {
        char *alias = "sipalias.somedomain.com";

        pj_assert(pj_strcmp2(&res->q[0].name, target)==0);

        res->hdr.anscount = 2;
        res->ans[0].type = PJ_DNS_TYPE_CNAME;
        res->ans[0].dnsclass = 1;
        res->ans[0].ttl = 1000; /* resolver should select minimum TTL */
        res->ans[0].name = res->q[0].name;
        res->ans[0].rdata.cname.name = pj_str(alias);

        res->ans[1].type = PJ_DNS_TYPE_AAAA;
        res->ans[1].dnsclass = 1;
        res->ans[1].ttl = 1;
        res->ans[1].name = pj_str(alias);
        s6_addr32(res->ans[1].rdata.aaaa.ip_addr, 0) = IP_ADDR1;
        s6_addr32(res->ans[1].rdata.aaaa.ip_addr, 1) = IP_ADDR1;
        s6_addr32(res->ans[1].rdata.aaaa.ip_addr, 2) = IP_ADDR1;
        s6_addr32(res->ans[1].rdata.aaaa.ip_addr, 3) = IP_ADDR1;
    }

    *p_res = res;
}

static void srv_cb_1(void *user_data,
                     pj_status_t status,
                     const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    pj_sem_post(sem);

    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, return);
    PJ_ASSERT_ON_FAIL(rec->count == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].priority == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].weight == 2, return);
    PJ_ASSERT_ON_FAIL(pj_strcmp2(&rec->entry[0].server.name, "sip.somedomain.com")==0,
                      return);
    PJ_ASSERT_ON_FAIL(pj_strcmp2(&rec->entry[0].server.alias, "sipalias.somedomain.com")==0,
                      return);

    /* IPv4 only */
    PJ_ASSERT_ON_FAIL(rec->entry[0].server.addr_count == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].ip.v4.s_addr == IP_ADDR1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].port == PORT1, return);

    
}


static void srv_cb_1b(void *user_data,
                      pj_status_t status,
                      const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    pj_sem_post(sem);

    PJ_ASSERT_ON_FAIL(status==PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_NXDOMAIN),
                      return);
    PJ_ASSERT_ON_FAIL(rec->count == 0, return);
}


static void srv_cb_1c(void *user_data,
                      pj_status_t status,
                      const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    pj_sem_post(sem);

    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, return);
    PJ_ASSERT_ON_FAIL(rec->count == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].priority == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].weight == 2, return);

    PJ_ASSERT_ON_FAIL(pj_strcmp2(&rec->entry[0].server.name, "sip.somedomain.com")==0,
                      return);
    PJ_ASSERT_ON_FAIL(pj_strcmp2(&rec->entry[0].server.alias, "sipalias.somedomain.com")==0,
                      return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].port == PORT1, return);

    /* IPv4 and IPv6 */
    PJ_ASSERT_ON_FAIL(rec->entry[0].server.addr_count == 2, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].af == pj_AF_INET() &&
                      rec->entry[0].server.addr[0].ip.v4.s_addr == IP_ADDR1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].server.addr[1].af == pj_AF_INET6() &&
                      s6_addr32(rec->entry[0].server.addr[1].ip.v6, 0) == IP_ADDR1, return);
}


static void srv_cb_1d(void *user_data,
                      pj_status_t status,
                      const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    pj_sem_post(sem);

    PJ_ASSERT_ON_FAIL(status == PJ_SUCCESS, return);
    PJ_ASSERT_ON_FAIL(rec->count == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].priority == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].weight == 2, return);

    PJ_ASSERT_ON_FAIL(pj_strcmp2(&rec->entry[0].server.name, "sip.somedomain.com")==0,
                      return);
    PJ_ASSERT_ON_FAIL(pj_strcmp2(&rec->entry[0].server.alias, "sipalias.somedomain.com")==0,
                      return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].port == PORT1, return);

    /* IPv6 only */
    PJ_ASSERT_ON_FAIL(rec->entry[0].server.addr_count == 1, return);
    PJ_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].af == pj_AF_INET6() &&
                      s6_addr32(rec->entry[0].server.addr[0].ip.v6, 0) == IP_ADDR1, return);
}


static int srv_resolver_test(void)
{
    pj_str_t domain = pj_str("somedomain.com");
    pj_str_t res_name = pj_str("_sip._udp.");

    /* Last servers state: server 0=active, server 1=bad*/

    /* Successful scenario */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): success scenario"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action1_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action1_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, 5061, pool, resolver, PJ_TRUE,
                        NULL, &srv_cb_1, NULL),
                    NULL, return -500);

    pj_sem_wait(sem);

    /* Because of previous tests, only NS 1 should get the request */
    PJ_TEST_EQ(g_server[0].pkt_count, 2, NULL, return -510);  /* 2 because of SRV and A resolution */
    PJ_TEST_EQ(g_server[1].pkt_count, 0, NULL, return -512);


    /* Wait until cache expires */
    PJ_LOG(3,(THIS_FILE, "  waiting for cache to expire (~1 secs).."));
    pj_thread_sleep(1000 + 100);


    /* DNS SRV option PJ_DNS_SRV_RESOLVE_AAAA */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): option PJ_DNS_SRV_RESOLVE_AAAA"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action1_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action1_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, 5061, pool, resolver,
                        PJ_DNS_SRV_RESOLVE_AAAA,
                        NULL, &srv_cb_1c, NULL),
                    NULL, return -520);

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    /* DNS SRV option PJ_DNS_SRV_RESOLVE_AAAA_ONLY */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): option PJ_DNS_SRV_RESOLVE_AAAA_ONLY"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action1_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action1_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, 5061, pool, resolver,
                        PJ_DNS_SRV_RESOLVE_AAAA_ONLY,
                        NULL, &srv_cb_1d, NULL),
                    NULL, return -530);

    pj_sem_wait(sem);
    pj_thread_sleep(1000);


    /* Successful scenario */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): parallel queries"));
    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, 5061, pool, resolver, PJ_TRUE,
                        NULL, &srv_cb_1, NULL),
                    NULL, return -540);


    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, 5061, pool, resolver, PJ_TRUE,
                        NULL, &srv_cb_1, NULL),
                    NULL, return -550);

    pj_sem_wait(sem);
    pj_sem_wait(sem);

    /* Only server one should get a query */
    PJ_TEST_EQ(g_server[0].pkt_count, 2, NULL, return -554);  /* 2 because of SRV and A resolution */
    PJ_TEST_EQ(g_server[1].pkt_count, 0, NULL, return -556);

    /* Since TTL is one, subsequent queries should fail */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): cache expires scenario"));

    pj_thread_sleep(1000 + 100);

    g_server[0].action = PJ_DNS_RCODE_NXDOMAIN;
    g_server[1].action = PJ_DNS_RCODE_NXDOMAIN;

    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, 5061, pool, resolver, PJ_TRUE,
                        NULL, &srv_cb_1b, NULL),
                    NULL, return -560);

    pj_sem_wait(sem);
    pj_thread_sleep(1000);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Fallback because there's no SRV in answer */
#define TARGET      "domain2.com"
#define IP_ADDR2    0x02030405
#define PORT2       50062

static void action2_1(const pj_dns_parsed_packet *pkt,
                      pj_dns_parsed_packet **p_res)
{
    pj_dns_parsed_packet *res;

    lock();
    res = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);

    res->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    res->ans = (pj_dns_parsed_rr*) 
               pj_pool_calloc(pool, 4, sizeof(pj_dns_parsed_rr));
    unlock();

    res->hdr.qdcount = 1;
    res->q[0].type = pkt->q[0].type;
    res->q[0].dnsclass = pkt->q[0].dnsclass;
    res->q[0].name = pkt->q[0].name;

    if (pkt->q[0].type == PJ_DNS_TYPE_SRV) {

        pj_assert(pj_strcmp2(&pkt->q[0].name, "_sip._udp." TARGET)==0);

        res->hdr.anscount = 1;
        res->ans[0].type = PJ_DNS_TYPE_A;    // <-- this will cause the fallback
        res->ans[0].dnsclass = 1;
        res->ans[0].name = res->q[0].name;
        res->ans[0].ttl = 1;
        res->ans[0].rdata.srv.prio = 1;
        res->ans[0].rdata.srv.weight = 2;
        res->ans[0].rdata.srv.port = PORT2;
        res->ans[0].rdata.srv.target = pj_str("sip01." TARGET);

    } else if (pkt->q[0].type == PJ_DNS_TYPE_A) {
        char *alias = "sipalias01." TARGET;

        pj_assert(pj_strcmp2(&res->q[0].name, TARGET)==0);

        res->hdr.anscount = 2;
        res->ans[0].type = PJ_DNS_TYPE_CNAME;
        res->ans[0].dnsclass = 1;
        res->ans[0].name = res->q[0].name;
        res->ans[0].ttl = 1;
        res->ans[0].rdata.cname.name = pj_str(alias);

        res->ans[1].type = PJ_DNS_TYPE_A;
        res->ans[1].dnsclass = 1;
        res->ans[1].name = pj_str(alias);
        res->ans[1].ttl = 1;
        res->ans[1].rdata.a.ip_addr.s_addr = IP_ADDR2;

    } else if (pkt->q[0].type == PJ_DNS_TYPE_AAAA) {
        char *alias = "sipalias01." TARGET;

        pj_assert(pj_strcmp2(&res->q[0].name, TARGET)==0);

        res->hdr.anscount = 2;
        res->ans[0].type = PJ_DNS_TYPE_CNAME;
        res->ans[0].dnsclass = 1;
        res->ans[0].name = res->q[0].name;
        res->ans[0].ttl = 1;
        res->ans[0].rdata.cname.name = pj_str(alias);

        res->ans[1].type = PJ_DNS_TYPE_AAAA;
        res->ans[1].dnsclass = 1;
        res->ans[1].ttl = 1;
        res->ans[1].name = pj_str(alias);
        s6_addr32(res->ans[1].rdata.aaaa.ip_addr, 0) = IP_ADDR2;
        s6_addr32(res->ans[1].rdata.aaaa.ip_addr, 1) = IP_ADDR2;
        s6_addr32(res->ans[1].rdata.aaaa.ip_addr, 2) = IP_ADDR2;
        s6_addr32(res->ans[1].rdata.aaaa.ip_addr, 3) = IP_ADDR2;
    }

    *p_res = res;
}

#define SRV_CB_CHECK(cond, err) if(!(cond)) { *cb_err=err; goto on_return; }

static void srv_cb_2(void *user_data,
                     pj_status_t status,
                     const pj_dns_srv_record *rec)
{
    int *cb_err = (int*)user_data;

    SRV_CB_CHECK(status == PJ_SUCCESS, -10);
    SRV_CB_CHECK(rec->count == 1, -20);
    SRV_CB_CHECK(rec->entry[0].priority == 0, -30);
    SRV_CB_CHECK(rec->entry[0].weight == 0, -40);
    SRV_CB_CHECK(pj_strcmp2(&rec->entry[0].server.name, TARGET)==0, -50);
    SRV_CB_CHECK(pj_strcmp2(&rec->entry[0].server.alias,
                            "sipalias01." TARGET)==0, -60);
    SRV_CB_CHECK(rec->entry[0].port == PORT2, -70);

    /* IPv4 only */
    SRV_CB_CHECK(rec->entry[0].server.addr_count == 1, -80);
    SRV_CB_CHECK(rec->entry[0].server.addr[0].af == pj_AF_INET() &&
                 rec->entry[0].server.addr[0].ip.v4.s_addr == IP_ADDR2, -90);

on_return:
    pj_sem_post(sem);
}

static void srv_cb_2a(void *user_data,
                      pj_status_t status,
                      const pj_dns_srv_record *rec)
{
    int *cb_err = (int*)user_data;

    SRV_CB_CHECK(status == PJ_SUCCESS, -10);
    SRV_CB_CHECK(rec->count == 1, -20);
    SRV_CB_CHECK(rec->entry[0].priority == 0, -30);
    SRV_CB_CHECK(rec->entry[0].weight == 0, -40);
    SRV_CB_CHECK(pj_strcmp2(&rec->entry[0].server.name, TARGET)==0, -50);
    SRV_CB_CHECK(pj_strcmp2(&rec->entry[0].server.alias,
                            "sipalias01." TARGET)==0, -60);
    SRV_CB_CHECK(rec->entry[0].port == PORT2, -70);

    /* IPv4 and IPv6 */
    SRV_CB_CHECK(rec->entry[0].server.addr_count == 2, -80);
    SRV_CB_CHECK(rec->entry[0].server.addr[0].af == pj_AF_INET() &&
                 rec->entry[0].server.addr[0].ip.v4.s_addr == IP_ADDR2, -90);
    SRV_CB_CHECK(rec->entry[0].server.addr[1].af == pj_AF_INET6() &&
                 s6_addr32(rec->entry[0].server.addr[1].ip.v6, 0) == IP_ADDR2,
                 -100);

on_return:
    pj_sem_post(sem);
}

static void srv_cb_2b(void *user_data,
                      pj_status_t status,
                      const pj_dns_srv_record *rec)
{
    int *cb_err = (int*)user_data;

    SRV_CB_CHECK(status == PJ_SUCCESS, -10);
    SRV_CB_CHECK(rec->count == 1, -20);
    SRV_CB_CHECK(rec->entry[0].priority == 0, -30);
    SRV_CB_CHECK(rec->entry[0].weight == 0, -40);
    SRV_CB_CHECK(pj_strcmp2(&rec->entry[0].server.name, TARGET)==0, -50);
    SRV_CB_CHECK(pj_strcmp2(&rec->entry[0].server.alias,
                            "sipalias01." TARGET)==0, -60);
    SRV_CB_CHECK(rec->entry[0].port == PORT2, -70);

    /* IPv6 only */
    SRV_CB_CHECK(rec->entry[0].server.addr_count == 1, -80);
    SRV_CB_CHECK(rec->entry[0].server.addr[0].af == pj_AF_INET6() &&
                 s6_addr32(rec->entry[0].server.addr[0].ip.v6, 0) == IP_ADDR2,
                 -90);

on_return:
    pj_sem_post(sem);
}

static int srv_resolver_fallback_test(void)
{
    pj_str_t domain = pj_str(TARGET);
    pj_str_t res_name = pj_str("_sip._udp.");
    int cb_err = 0;

    /* Fallback test */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): fallback test"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action2_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action2_1;

    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, PORT2, pool, resolver, PJ_TRUE,
                        &cb_err, &srv_cb_2, NULL),
                    NULL, return -600);

    pj_sem_wait(sem);

    PJ_TEST_EQ(cb_err, 0, "srv_resolve cb error", return -605);

    /* Subsequent query should just get the response from the cache */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): cache test"));
    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, PORT2, pool, resolver, PJ_TRUE,
                        &cb_err, &srv_cb_2, NULL),
                    NULL, return -610);

    pj_sem_wait(sem);

    PJ_TEST_EQ(cb_err,  0, "srv_resolve cb error", return -615);

    PJ_TEST_EQ(g_server[0].pkt_count, 0, "must be from cache", return -620);
    PJ_TEST_EQ(g_server[1].pkt_count, 0, "must be from cache", return -625);

    /* Clear cache */
    pj_thread_sleep(1000);

    /* Fallback with PJ_DNS_SRV_FALLBACK_A and PJ_DNS_SRV_FALLBACK_AAAA */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): fallback to DNS A and AAAA"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action2_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action2_1;

    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, PORT2, pool, resolver,
                        PJ_DNS_SRV_FALLBACK_A | PJ_DNS_SRV_FALLBACK_AAAA,
                        &cb_err, &srv_cb_2a, NULL),
                    NULL, return -630);

    pj_sem_wait(sem);

    PJ_TEST_EQ(cb_err, 0, "srv_resolve cb error", return -635);

    /* Clear cache */
    pj_thread_sleep(1000);

    /* Fallback with PJ_DNS_SRV_FALLBACK_AAAA only */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): fallback to DNS AAAA only"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action2_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action2_1;

    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, PORT2, pool, resolver,
                        PJ_DNS_SRV_FALLBACK_AAAA,
                        &cb_err, &srv_cb_2b, NULL),
                    NULL, return -640);

    pj_sem_wait(sem);

    PJ_TEST_EQ(cb_err, 0, "srv_resolve cb error", return -645);

    /* Clear cache */
    pj_thread_sleep(1000);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Too many SRV or A entries */
#define DOMAIN3     "d3"
#define SRV_COUNT3  (PJ_DNS_SRV_MAX_ADDR+1)
#define A_COUNT3    (PJ_DNS_MAX_IP_IN_A_REC+1)
#define PORT3       50063
#define IP_ADDR3    0x03030303

static void action3_1(const pj_dns_parsed_packet *pkt,
                      pj_dns_parsed_packet **p_res)
{
    pj_dns_parsed_packet *res;
    unsigned i;

    lock();
    res = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);

    if (res->q == NULL) {
        res->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    }
    unlock();

    res->hdr.qdcount = 1;
    res->q[0].type = pkt->q[0].type;
    res->q[0].dnsclass = pkt->q[0].dnsclass;
    res->q[0].name = pkt->q[0].name;

    if (pkt->q[0].type == PJ_DNS_TYPE_SRV) {

        pj_assert(pj_strcmp2(&pkt->q[0].name, "_sip._udp." DOMAIN3)==0);

        res->hdr.anscount = SRV_COUNT3;
        lock();
        res->ans = (pj_dns_parsed_rr*) 
                   pj_pool_calloc(pool, SRV_COUNT3, sizeof(pj_dns_parsed_rr));
        unlock();

        for (i=0; i<SRV_COUNT3; ++i) {
            char *target;

            res->ans[i].type = PJ_DNS_TYPE_SRV;
            res->ans[i].dnsclass = 1;
            res->ans[i].name = res->q[0].name;
            res->ans[i].ttl = 1;
            res->ans[i].rdata.srv.prio = (pj_uint16_t)i;
            res->ans[i].rdata.srv.weight = 2;
            res->ans[i].rdata.srv.port = (pj_uint16_t)(PORT3+i);

            lock();
            target = (char*)pj_pool_alloc(pool, 16);
            unlock();
            pj_ansi_snprintf(target, 16, "sip%02d." DOMAIN3, i);
            res->ans[i].rdata.srv.target = pj_str(target);
        }

    } else if (pkt->q[0].type == PJ_DNS_TYPE_A) {

        //pj_assert(pj_strcmp2(&res->q[0].name, "sip." DOMAIN3)==0);

        res->hdr.anscount = A_COUNT3;
        lock();
        res->ans = (pj_dns_parsed_rr*) 
                   pj_pool_calloc(pool, A_COUNT3, sizeof(pj_dns_parsed_rr));
        unlock();

        for (i=0; i<A_COUNT3; ++i) {
            res->ans[i].type = PJ_DNS_TYPE_A;
            res->ans[i].dnsclass = 1;
            res->ans[i].ttl = 1;
            res->ans[i].name = res->q[0].name;
            res->ans[i].rdata.a.ip_addr.s_addr = IP_ADDR3+i;
        }
    }

    *p_res = res;
}

static void srv_cb_3(void *user_data,
                     pj_status_t status,
                     const pj_dns_srv_record *rec)
{
    unsigned i;
    int *cb_err = (int*)user_data;

    PJ_UNUSED_ARG(status);
    PJ_UNUSED_ARG(rec);

    SRV_CB_CHECK(status == PJ_SUCCESS, -10);
    SRV_CB_CHECK(rec->count == PJ_DNS_SRV_MAX_ADDR, -20);

    for (i=0; i<PJ_DNS_SRV_MAX_ADDR; ++i) {
        unsigned j;

        SRV_CB_CHECK(rec->entry[i].priority == i, -30);
        SRV_CB_CHECK(rec->entry[i].weight == 2, -40);
        //pj_assert(pj_strcmp2(&rec->entry[i].server.name, "sip." DOMAIN3)==0);
        SRV_CB_CHECK(rec->entry[i].server.alias.slen == 0, -50);
        SRV_CB_CHECK(rec->entry[i].port == PORT3+i, -60);

        SRV_CB_CHECK(rec->entry[i].server.addr_count == PJ_DNS_MAX_IP_IN_A_REC, -70);

        for (j=0; j<PJ_DNS_MAX_IP_IN_A_REC; ++j) {
            SRV_CB_CHECK(rec->entry[i].server.addr[j].ip.v4.s_addr == IP_ADDR3+j, -80);
        }
    }

on_return:
    pj_sem_post(sem);
}

static int srv_resolver_many_test(void)
{
    pj_str_t domain = pj_str(DOMAIN3);
    pj_str_t res_name = pj_str("_sip._udp.");
    int cb_err = 0;

    /* Successful scenario */
    PJ_LOG(3,(THIS_FILE, "  srv_resolve(): too many entries test"));

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action3_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action3_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    PJ_TEST_SUCCESS(pj_dns_srv_resolve(
                        &domain, &res_name, 1, pool, resolver, PJ_TRUE,
                        &cb_err, &srv_cb_3, NULL),
                    NULL, return -700);

    pj_sem_wait(sem);

    PJ_TEST_EQ(cb_err, 0, "srv_resolve cb error", return -710);

    return 0;
}


////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////
/* Resetting the state of the nameservers */

static volatile pj_bool_t reset_ns_cb_called;
static pj_status_t reset_ns_cb_status;
static pj_uint32_t reset_ns_cb_addr;

static void dns_callback_reset_ns(void *user_data,
                                  pj_status_t status,
                                  pj_dns_parsed_packet *resp)
{
    PJ_UNUSED_ARG(user_data);

    reset_ns_cb_status = status;
    if (status == PJ_SUCCESS && resp && resp->hdr.anscount)
        reset_ns_cb_addr = resp->ans[0].rdata.a.ip_addr.s_addr;
    reset_ns_cb_called = PJ_TRUE;
}

static int wait_reset_ns_cb(void)
{
    unsigned i;

    for (i = 0; !reset_ns_cb_called && i < 500; ++i)
        pj_thread_sleep(10);
    return reset_ns_cb_called ? 0 : -1;
}

/* Wait for the servers to have counted the packets sent to them */
static int wait_pkt_count(unsigned count)
{
    unsigned i;

    for (i = 0; g_server[0].pkt_count + g_server[1].pkt_count < count &&
                i < 500; ++i)
    {
        pj_thread_sleep(10);
    }
    return g_server[0].pkt_count + g_server[1].pkt_count >= count ? 0 : -1;
}

/* Fail the test, cleaning up at its end */
#define SYS_FAIL(code)  { rc = (code); goto on_return; }

/* After the reset, a nameserver marked as bad is tried again, while the
 * cache and the pending queries are kept.
 */
static int dns_reset_ns_state_test(void)
{
    pj_str_t name1 = pj_str("name_reset1");
    pj_str_t name2 = pj_str("name_reset2");
    pj_str_t ns_addr = pj_str("127.0.0.1");
    pj_uint16_t port = g_server[0].port;
    pj_dns_parsed_packet *r;
    pj_dns_resolver *res = NULL;
    pj_dns_settings lset;
    pj_dns_async_query *q;
    unsigned sent;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  reset nameserver state test"));

    /* A single nameserver, so that a response marks it alone */
    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue,
                                           &res),
                    NULL, SYS_FAIL(-900));
    pj_dns_resolver_get_settings(res, &lset);
    lset.qretr_delay = 200;
    lset.qretr_count = 3;
    pj_dns_resolver_set_settings(res, &lset);
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(res, 1, &ns_addr, &port),
                    NULL, SYS_FAIL(-901));

    /* A refusal marks the nameserver as bad */
    g_server[0].action = PJ_DNS_RCODE_REFUSED;
    g_server[0].pkt_count = 0;
    reset_ns_cb_called = PJ_FALSE;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_reset_ns, NULL,
                                                NULL),
                    NULL, SYS_FAIL(-902));
    PJ_TEST_EQ(wait_reset_ns_cb(), 0, NULL, SYS_FAIL(-903));
    PJ_TEST_EQ(reset_ns_cb_status,
               PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_REFUSED),
               NULL, SYS_FAIL(-904));

    /* Marked as bad: a query can't be sent */
    sent = g_server[0].pkt_count + g_server[1].pkt_count;
    PJ_TEST_EQ(pj_dns_resolver_start_query(res, &name2, PJ_DNS_TYPE_A, 0,
                                           &dns_callback_reset_ns, NULL,
                                           NULL),
               PJLIB_UTIL_EDNSNOWORKINGNS, NULL, SYS_FAIL(-905));

    /* Tried again after the reset */
    PJ_TEST_SUCCESS(pj_dns_resolver_reset_ns_state(res), NULL, SYS_FAIL(-906));
    g_server[0].action = ACTION_IGNORE;
    reset_ns_cb_called = PJ_FALSE;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name2, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_reset_ns, NULL,
                                                &q),
                    NULL, SYS_FAIL(-907));
    PJ_TEST_NOT_NULL(q, NULL, SYS_FAIL(-908));
    PJ_TEST_EQ(wait_pkt_count(sent + 1), 0, NULL, SYS_FAIL(-909));

    /* The pending query goes on after another reset, and gets the answer
     * to its retransmission
     */
    PJ_TEST_SUCCESS(pj_dns_resolver_reset_ns_state(res), NULL, SYS_FAIL(-910));
    r = &g_server[0].resp;
    r->hdr.qdcount = 1;
    r->hdr.anscount = 1;
    r->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    r->q[0].type = PJ_DNS_TYPE_A;
    r->q[0].dnsclass = 1;
    r->q[0].name = name2;
    r->ans = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_rr);
    r->ans[0].type = PJ_DNS_TYPE_A;
    r->ans[0].dnsclass = 1;
    r->ans[0].name = name2;
    r->ans[0].ttl = 300;
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;
    g_server[0].action = ACTION_REPLY;
    PJ_TEST_EQ(wait_reset_ns_cb(), 0, NULL, SYS_FAIL(-911));
    PJ_TEST_SUCCESS(reset_ns_cb_status, NULL, SYS_FAIL(-912));
    PJ_TEST_EQ(reset_ns_cb_addr, IP_ADDR0, NULL, SYS_FAIL(-913));

    /* The cache is kept: answered from it, nothing sent */
    sent = g_server[0].pkt_count + g_server[1].pkt_count;
    PJ_TEST_SUCCESS(pj_dns_resolver_reset_ns_state(res), NULL, SYS_FAIL(-914));
    reset_ns_cb_called = PJ_FALSE;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name2, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_reset_ns, NULL,
                                                &q),
                    NULL, SYS_FAIL(-915));
    PJ_TEST_TRUE(q == NULL, NULL, SYS_FAIL(-916));
    PJ_TEST_TRUE(reset_ns_cb_called, NULL, SYS_FAIL(-917));
    PJ_TEST_EQ(g_server[0].pkt_count + g_server[1].pkt_count, sent, NULL,
               SYS_FAIL(-918));

    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;


on_return:
    if (res)
        pj_dns_resolver_destroy(res, PJ_FALSE);
    return rc;
}


////////////////////////////////////////////////////////////////////////////
/* Cancelling a query */

static int cancel_pending_cb_count[2];
static pj_status_t cancel_pending_cb_status[2];

static void dns_callback_cancel_pending(void *user_data,
                                        pj_status_t status,
                                        pj_dns_parsed_packet *resp)
{
    unsigned i = (unsigned)(pj_ssize_t)user_data;

    PJ_UNUSED_ARG(resp);

    cancel_pending_cb_count[i]++;
    cancel_pending_cb_status[i] = status;
}

/* A cancelled query goes on, so that the next query for the name, which
 * joins it, is answered; it would stay pending for good otherwise.
 */
static int dns_cancel_pending_test(void)
{
    pj_str_t name = pj_str("cancelled");
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    pj_dns_resolver *res = NULL;
    pj_dns_settings lset;
    pj_dns_async_query *q;
    unsigned i;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  cancelled query goes on test"));

    nameservers[0] = nameservers[1] = pj_str("127.0.0.1");
    ports[0] = g_server[0].port;
    ports[1] = g_server[1].port;
    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue,
                                           &res),
                    NULL, SYS_FAIL(-1130));
    pj_dns_resolver_get_settings(res, &lset);
    lset.qretr_delay = 100;
    lset.qretr_count = 2;
    pj_dns_resolver_set_settings(res, &lset);
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(res, 2, nameservers, ports),
                    NULL, SYS_FAIL(-1131));
    g_server[0].action = g_server[1].action = ACTION_IGNORE;
    pj_bzero(cancel_pending_cb_count, sizeof(cancel_pending_cb_count));

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_cancel_pending,
                                                (void*)0, &q),
                    NULL, SYS_FAIL(-1132));
    PJ_TEST_NOT_NULL(q, NULL, SYS_FAIL(-1133));
    PJ_TEST_SUCCESS(pj_dns_resolver_cancel_query(q, PJ_FALSE), NULL,
                    SYS_FAIL(-1134));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_cancel_pending,
                                                (void*)1, &q),
                    NULL, SYS_FAIL(-1135));
    for (i = 0; !cancel_pending_cb_count[1] && i < 300; ++i)
        pj_thread_sleep(10);
    PJ_TEST_EQ(cancel_pending_cb_count[1], 1, NULL, SYS_FAIL(-1136));
    PJ_TEST_EQ(cancel_pending_cb_status[1], PJ_ETIMEDOUT, NULL,
               SYS_FAIL(-1137));
    PJ_TEST_EQ(cancel_pending_cb_count[0], 0, NULL, SYS_FAIL(-1138));

    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;

on_return:
    if (res)
        pj_dns_resolver_destroy(res, PJ_FALSE);
    return rc;
}


////////////////////////////////////////////////////////////////////////////
/* System resolver fallback */

#define SYS_ADDR    0x0A000001

/* The system resolver of the tests: answers SYS_ADDR, or the status set,
 * after waiting on the semaphore when one is set.
 */
static struct {
    volatile int  count;
    int           af;
    char          name[PJ_MAX_HOSTNAME];
    pj_status_t   status;
    pj_sem_t     *block;
    volatile pj_bool_t unblocked;   /* a blocked lookup went on */
    unsigned      qretr_delay;      /* of the resolver of the test */
} sys_state;

static pj_status_t test_sys_lookup(int af, const pj_str_t *name,
                                   unsigned *count, pj_addrinfo ai[])
{
    /* Read once: the cleanup of a test clears it */
    pj_sem_t *block = sys_state.block;

    sys_state.count++;
    sys_state.af = af;
    pj_ansi_strxcpy2(sys_state.name, name, sizeof(sys_state.name));
    if (block) {
        pj_sem_wait(block);
        sys_state.unblocked = PJ_TRUE;
    }
    if (sys_state.status != PJ_SUCCESS)
        return sys_state.status;

    pj_bzero(&ai[0], sizeof(ai[0]));
    pj_sockaddr_init(af, &ai[0].ai_addr, NULL, 0);
    if (af == pj_AF_INET())
        ai[0].ai_addr.ipv4.sin_addr.s_addr = SYS_ADDR;
    else
        ai[0].ai_addr.ipv6.sin6_addr.s6_addr[15] = 1;
    *count = 1;
    return PJ_SUCCESS;
}

static struct {
    volatile int  called;
    pj_status_t   status;
    int           type;
    pj_uint32_t   addr;         /* DNS A */
    pj_uint8_t    addr6_last;   /* DNS AAAA, its last byte */
} sys_cb[4];

static void dns_callback_sys(void *user_data,
                             pj_status_t status,
                             pj_dns_parsed_packet *resp)
{
    unsigned i = (unsigned)(pj_ssize_t)user_data;

    sys_cb[i].called++;
    sys_cb[i].status = status;
    sys_cb[i].type = 0;
    sys_cb[i].addr = 0;
    sys_cb[i].addr6_last = 0;
    if (status == PJ_SUCCESS && resp && resp->hdr.anscount) {
        sys_cb[i].type = resp->ans[0].type;
        if (resp->ans[0].type == PJ_DNS_TYPE_A)
            sys_cb[i].addr = resp->ans[0].rdata.a.ip_addr.s_addr;
        else if (resp->ans[0].type == PJ_DNS_TYPE_AAAA)
            sys_cb[i].addr6_last = resp->ans[0].rdata.aaaa.ip_addr.s6_addr[15];
    }
}

static void sys_reset(void)
{
    pj_bzero(&sys_state, sizeof(sys_state));
    pj_bzero(sys_cb, sizeof(sys_cb));
    g_server[0].pkt_count = g_server[1].pkt_count = 0;
}

/* Whether no server can be sent to, i.e. all are marked as bad: without
 * the option, such a query fails at once. The option is off meanwhile, so
 * no other query than a previous probe may be pending.
 */
static pj_bool_t sys_none_works(pj_dns_resolver *res)
{
    pj_str_t name = pj_str("probe");
    pj_dns_settings lset;
    pj_bool_t fallback;
    pj_status_t status;

    pj_dns_resolver_get_settings(res, &lset);
    fallback = lset.sys_fallback;
    lset.sys_fallback = PJ_FALSE;
    pj_dns_resolver_set_settings(res, &lset);
    status = pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                         &dns_callback_sys, (void*)3, NULL);
    lset.sys_fallback = fallback;
    pj_dns_resolver_set_settings(res, &lset);
    return status == PJLIB_UTIL_EDNSNOWORKINGNS;
}

#if !PJ_HAS_THREADS
/* Without threads: a timer heap and an ioqueue polled by the test, a
 * nameserver which is a socket nobody reads from, the system resolver
 * answering at once, from the timer of the resolver.
 */
static struct {
    pj_pool_t          *pool;
    pj_timer_heap_t    *th;
    pj_ioqueue_t       *ioq;
    pj_sock_t           sock;
    pj_dns_resolver    *res;
    int                 seq;        /* callbacks so far */
    int                 seq_of[4];  /* the callback's rank, per query */
} nt;

static void nt_poll(unsigned ms)
{
    pj_time_val delay = {0, 10};
    pj_time_val t0, now;

    pj_gettimeofday(&t0);
    do {
        pj_timer_heap_poll(nt.th, NULL);
        pj_ioqueue_poll(nt.ioq, &delay);
        pj_gettimeofday(&now);
        PJ_TIME_VAL_SUB(now, t0);
    } while (PJ_TIME_VAL_MSEC(now) < (long)ms);
}

/* Poll until the query's callback, or the deadline */
static int nt_wait_cb(unsigned i)
{
    unsigned n;

    for (n = 0; !sys_cb[i].called && n < 500; ++n)
        nt_poll(10);
    return sys_cb[i].called ? 0 : -1;
}

static void dns_callback_nt(void *user_data,
                            pj_status_t status,
                            pj_dns_parsed_packet *resp)
{
    unsigned i = (unsigned)(pj_ssize_t)user_data;

    dns_callback_sys(user_data, status, resp);
    nt.seq_of[i] = ++nt.seq;
}

/* Starts the same name again from the callback */
static void dns_callback_nt_again(void *user_data,
                                  pj_status_t status,
                                  pj_dns_parsed_packet *resp)
{
    pj_str_t name = pj_str("nt_c");

    dns_callback_nt(user_data, status, resp);
    pj_dns_resolver_start_query(nt.res, &name, PJ_DNS_TYPE_A, 0,
                                &dns_callback_nt, (void*)3, NULL);
}

/* Destroys the resolver from the callback */
static void dns_callback_nt_destroy(void *user_data,
                                    pj_status_t status,
                                    pj_dns_parsed_packet *resp)
{
    dns_callback_nt(user_data, status, resp);
    pj_dns_resolver_destroy(nt.res, PJ_TRUE);
    nt.res = NULL;
}

static int nt_start(pj_dns_callback *cb, const char *name, int type,
                    unsigned i, pj_dns_async_query **p_q)
{
    pj_str_t n = pj_str((char*)name);

    return pj_dns_resolver_start_query(nt.res, &n, type, 0, cb,
                                       (void*)(pj_ssize_t)i, p_q);
}

static int nt_setup(void)
{
    pj_sockaddr addr;
    int addr_len = sizeof(addr);
    pj_str_t ns = pj_str("127.0.0.1");
    pj_uint16_t port;
    pj_dns_settings lset;

    sys_reset();
    pj_bzero(&nt, sizeof(nt));
    nt.sock = PJ_INVALID_SOCKET;
    nt.pool = pj_pool_create(mem, "nothreads", 4000, 4000, NULL);
    if (!nt.pool)
        return -1;
    if (pj_timer_heap_create(nt.pool, 32, &nt.th) != PJ_SUCCESS ||
        pj_ioqueue_create(nt.pool, 8, &nt.ioq) != PJ_SUCCESS)
    {
        return -2;
    }
    pj_sockaddr_init(pj_AF_INET(), &addr, &ns, 0);
    if (pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &nt.sock) !=
            PJ_SUCCESS ||
        pj_sock_bind(nt.sock, &addr, pj_sockaddr_get_len(&addr)) !=
            PJ_SUCCESS ||
        pj_sock_getsockname(nt.sock, &addr, &addr_len) != PJ_SUCCESS)
    {
        return -3;
    }
    port = pj_sockaddr_get_port(&addr);
    if (pj_dns_resolver_create(mem, NULL, 0, nt.th, nt.ioq, &nt.res) !=
        PJ_SUCCESS)
    {
        return -4;
    }
    pj_dns_resolver_get_settings(nt.res, &lset);
    lset.qretr_delay = 50;
    lset.qretr_count = 1;
    lset.sys_fallback = PJ_TRUE;
    lset.sys_lookup = &test_sys_lookup;
    pj_dns_resolver_set_settings(nt.res, &lset);
    sys_state.qretr_delay = 50;
    if (pj_dns_resolver_set_ns(nt.res, 1, &ns, &port) != PJ_SUCCESS)
        return -5;
    return 0;
}

static void nt_teardown(void)
{
    if (nt.res)
        pj_dns_resolver_destroy(nt.res, PJ_FALSE);
    if (nt.sock != PJ_INVALID_SOCKET)
        pj_sock_close(nt.sock);
    if (nt.ioq)
        pj_ioqueue_destroy(nt.ioq);
    if (nt.th)
        pj_timer_heap_destroy(nt.th);
    if (nt.pool)
        pj_pool_release(nt.pool);
    pj_bzero(&nt, sizeof(nt));
}

/* The lookup from the timer of the resolver: the queries of a name join
 * the one being looked up, a cancelled one is not reported, a query
 * started from the callback is one of its own, and the resolver may be
 * destroyed from a callback.
 */
int resolver_nothreads_test(void)
{
    pj_dns_async_query *q;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback without threads"));

    PJ_TEST_EQ(nt_setup(), 0, NULL, SYS_FAIL(-1400));

    /* Timed out, looked up from the timer, not within start_query() */
    PJ_TEST_SUCCESS(nt_start(&dns_callback_nt, "nt_a", PJ_DNS_TYPE_A, 0,
                             NULL),
                    NULL, SYS_FAIL(-1401));
    PJ_TEST_SUCCESS(nt_start(&dns_callback_nt, "nt_a", PJ_DNS_TYPE_A, 1,
                             NULL),
                    NULL, SYS_FAIL(-1402));
    PJ_TEST_EQ(sys_state.count, 0, NULL, SYS_FAIL(-1403));
    PJ_TEST_EQ(nt_wait_cb(0), 0, NULL, SYS_FAIL(-1404));
    PJ_TEST_EQ(nt_wait_cb(1), 0, NULL, SYS_FAIL(-1405));
    PJ_TEST_SUCCESS(sys_cb[0].status, NULL, SYS_FAIL(-1406));
    PJ_TEST_SUCCESS(sys_cb[1].status, NULL, SYS_FAIL(-1407));
    PJ_TEST_EQ(sys_cb[0].addr, SYS_ADDR, NULL,
               SYS_FAIL(-1408));
    PJ_TEST_EQ(sys_state.count, 1, NULL, SYS_FAIL(-1409));

    /* The nameserver bad once its probing is over: the queries of a name
     * join the one being looked up, from the timer
     */
    nt_poll(3 * 50);
    PJ_TEST_TRUE(sys_none_works(nt.res), NULL,
                 SYS_FAIL(-1410));
    sys_reset();
    sys_state.qretr_delay = 50;
    PJ_TEST_SUCCESS(nt_start(&dns_callback_nt, "nt_b", PJ_DNS_TYPE_A, 0,
                             NULL),
                    NULL, SYS_FAIL(-1411));
    PJ_TEST_SUCCESS(nt_start(&dns_callback_nt, "nt_b", PJ_DNS_TYPE_A, 1,
                             NULL),
                    NULL, SYS_FAIL(-1412));
    PJ_TEST_SUCCESS(nt_start(&dns_callback_nt, "nt_b", PJ_DNS_TYPE_AAAA, 2,
                             NULL),
                    NULL, SYS_FAIL(-1413));
    PJ_TEST_SUCCESS(nt_start(&dns_callback_nt, "nt_b", PJ_DNS_TYPE_A, 3,
                             &q),
                    NULL, SYS_FAIL(-1414));
    PJ_TEST_SUCCESS(pj_dns_resolver_cancel_query(q, PJ_FALSE), NULL,
                    SYS_FAIL(-1415));
    PJ_TEST_EQ(sys_state.count, 0, NULL, SYS_FAIL(-1416));
    PJ_TEST_EQ(nt_wait_cb(0), 0, NULL, SYS_FAIL(-1417));
    PJ_TEST_EQ(nt_wait_cb(1), 0, NULL, SYS_FAIL(-1418));
    PJ_TEST_EQ(nt_wait_cb(2), 0, NULL, SYS_FAIL(-1419));
    nt_poll(100);
    PJ_TEST_EQ(sys_cb[0].called + sys_cb[1].called + sys_cb[2].called, 3,
               NULL, SYS_FAIL(-1420));
    PJ_TEST_EQ(sys_cb[3].called, 0, NULL, SYS_FAIL(-1421));
    PJ_TEST_EQ(sys_cb[2].type, PJ_DNS_TYPE_AAAA, NULL,
               SYS_FAIL(-1422));
    /* One lookup per type, the first query reported before the joined */
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1423));
    PJ_TEST_TRUE(nt.seq_of[0] < nt.seq_of[1], NULL,
                 SYS_FAIL(-1424));

    /* A query started from the callback, for the name being reported, is
     * one of its own
     */
    sys_reset();
    sys_state.qretr_delay = 50;
    nt.seq = 0;
    PJ_TEST_SUCCESS(nt_start(&dns_callback_nt_again, "nt_c", PJ_DNS_TYPE_A,
                             0, NULL),
                    NULL, SYS_FAIL(-1425));
    PJ_TEST_EQ(nt_wait_cb(0), 0, NULL, SYS_FAIL(-1426));
    PJ_TEST_EQ(nt_wait_cb(3), 0, NULL, SYS_FAIL(-1427));
    PJ_TEST_SUCCESS(sys_cb[3].status, NULL, SYS_FAIL(-1428));
    PJ_TEST_EQ(sys_cb[3].called, 1, NULL, SYS_FAIL(-1429));
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1430));

    /* Destroyed from a callback: the query waiting is reported as
     * cancelled, once, the joined one with the answer, nothing is left
     */
    sys_reset();
    sys_state.qretr_delay = 50;
    PJ_TEST_SUCCESS(nt_start(&dns_callback_nt_destroy, "nt_d", PJ_DNS_TYPE_A,
                             0, NULL),
                    NULL, SYS_FAIL(-1431));
    PJ_TEST_SUCCESS(nt_start(&dns_callback_nt, "nt_d", PJ_DNS_TYPE_A, 1,
                             NULL),
                    NULL, SYS_FAIL(-1432));
    PJ_TEST_SUCCESS(nt_start(&dns_callback_nt, "nt_e", PJ_DNS_TYPE_A, 2,
                             NULL),
                    NULL, SYS_FAIL(-1433));
    PJ_TEST_EQ(nt_wait_cb(0), 0, NULL, SYS_FAIL(-1434));
    nt_poll(100);
    PJ_TEST_EQ(nt.res, NULL, NULL, SYS_FAIL(-1435));
    PJ_TEST_EQ(sys_cb[1].called, 1, NULL, SYS_FAIL(-1436));
    PJ_TEST_SUCCESS(sys_cb[1].status, NULL, SYS_FAIL(-1437));
    PJ_TEST_EQ(sys_cb[2].called, 1, NULL, SYS_FAIL(-1438));
    PJ_TEST_EQ(sys_cb[2].status, PJ_ECANCELLED, NULL,
               SYS_FAIL(-1439));
    PJ_TEST_EQ(sys_state.count, 1, NULL, SYS_FAIL(-1440));
    PJ_TEST_EQ(pj_timer_heap_count(nt.th), 0, NULL,
               SYS_FAIL(-1441));

on_return:
    nt_teardown();
    if (rc)
        PJ_LOG(1,(THIS_FILE, "    error: no-threads fallback [%d]", rc));
    return rc;
}
#endif  /* !PJ_HAS_THREADS */

#if PJ_HAS_THREADS
static int wait_sys_cb(unsigned i)
{
    unsigned n;

    for (n = 0; !sys_cb[i].called && n < 300; ++n)
        pj_thread_sleep(10);
    return sys_cb[i].called ? 0 : -1;
}

/* After a test, passed or not: the lookups waiting on the semaphore go
 * on, then the resolver, which waits for them, is destroyed, then the
 * semaphore.
 */
static void sys_cleanup(pj_dns_resolver **res)
{
    pj_sem_t *block = sys_state.block;
    unsigned i;

    sys_state.block = NULL;
    if (block) {
        for (i = 0; i < 4; ++i)
            pj_sem_post(block);
    }
    if (*res) {
        pj_dns_resolver_destroy(*res, PJ_FALSE);
        *res = NULL;
    }
    if (block)
        pj_sem_destroy(block);
}

/* A resolver on the first ns_count servers, with short retransmissions */
static pj_dns_resolver *sys_resolver_ns(pj_bool_t fallback,
                                        unsigned qretr_delay,
                                        unsigned ns_count)
{
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    pj_dns_resolver *res;
    pj_dns_settings lset;

    nameservers[0] = nameservers[1] = pj_str("127.0.0.1");
    ports[0] = g_server[0].port;
    ports[1] = g_server[1].port;
    if (pj_dns_resolver_create(mem, NULL, 0, timer_heap, ioqueue, &res) !=
        PJ_SUCCESS)
    {
        return NULL;
    }
    pj_dns_resolver_get_settings(res, &lset);
    lset.qretr_delay = qretr_delay;
    lset.qretr_count = 2;
    lset.sys_fallback = fallback;
    lset.sys_lookup = &test_sys_lookup;
    pj_dns_resolver_set_settings(res, &lset);
    sys_state.qretr_delay = qretr_delay;
    if (pj_dns_resolver_set_ns(res, ns_count, nameservers, ports) !=
        PJ_SUCCESS)
    {
        pj_dns_resolver_destroy(res, PJ_FALSE);
        return NULL;
    }
    return res;
}

static pj_dns_resolver *sys_resolver(pj_bool_t fallback, unsigned qretr_delay)
{
    return sys_resolver_ns(fallback, qretr_delay, 2);
}

/* Set how many lookups the resolver runs at once */
static void sys_set_threads(pj_dns_resolver *res, unsigned threads)
{
    pj_dns_settings lset;

    pj_dns_resolver_get_settings(res, &lset);
    lset.sys_threads = threads;
    pj_dns_resolver_set_settings(res, &lset);
}

/* The servers probed by a query which timed out stay probed, and are sent
 * to, until (qretr_count + 2) retransmit delays after its first
 * transmission; the query timed out one delay after its last one.
 */
static void sys_wait_probing(void)
{
    pj_thread_sleep(3 * sys_state.qretr_delay);
}

/* Wait for the system resolver to have been asked count times */
static int wait_sys_count(int count)
{
    unsigned i;

    for (i = 0; sys_state.count < count && i < 500; ++i)
        pj_thread_sleep(10);
    return sys_state.count >= count ? 0 : -1;
}

/* Time a query out on the servers, not trusted yet, and get the answer of
 * the system resolver; then let the resolver stop probing them, so that
 * the next query is not sent.
 */
static int sys_time_out(pj_dns_resolver *res, const char *name, unsigned i)
{
    pj_str_t n = pj_str((char*)name);

    g_server[0].action = g_server[1].action = ACTION_IGNORE;
    sys_cb[i].called = 0;
    if (pj_dns_resolver_start_query(res, &n, PJ_DNS_TYPE_A, 0,
                                    &dns_callback_sys, (void*)(pj_ssize_t)i,
                                    NULL) != PJ_SUCCESS)
    {
        return -1;
    }
    if (wait_sys_cb(i) != 0)
        return -2;
    sys_wait_probing();
    return 0;
}

/* The query which times out on the nameservers while none is trusted is
 * resolved with the system resolver, and once they are not probed anymore
 * the next ones at once, without being sent; a timeout on a trusted
 * nameserver is reported as without the option, which reports every
 * timeout.
 */
static int dns_sys_fallback_timeout_test(void)
{
    pj_str_t name0 = pj_str("sysname0");
    pj_str_t name1 = pj_str("sysname1");
    pj_str_t name = pj_str("sysname2");
    pj_dns_parsed_packet *r;
    pj_dns_resolver *res = NULL;
    pj_dns_async_query *q;
    unsigned sent;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, timeout test"));

    sys_reset();
    /* Long enough for the servers not to be marked as bad by the probing
     * which follows their first query, but by the timeout
     */
    res = sys_resolver(PJ_TRUE, 300);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1000));

    /* The servers answer: they are known to work */
    r = &g_server[0].resp;
    r->hdr.qdcount = 1;
    r->hdr.anscount = 1;
    r->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    r->q[0].type = PJ_DNS_TYPE_A;
    r->q[0].dnsclass = 1;
    r->q[0].name = name0;
    r->ans = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_rr);
    r->ans[0].type = PJ_DNS_TYPE_A;
    r->ans[0].dnsclass = 1;
    r->ans[0].name = name0;
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;
    g_server[1].resp = g_server[0].resp;
    g_server[0].action = g_server[1].action = ACTION_REPLY;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name0, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys,
                                                (void*)3, NULL),
                    NULL, SYS_FAIL(-1008));
    PJ_TEST_EQ(wait_sys_cb(3), 0, NULL, SYS_FAIL(-1009));
    PJ_TEST_EQ(sys_cb[3].addr, IP_ADDR0, NULL, SYS_FAIL(-1018));
    PJ_TEST_EQ(sys_state.count, 0, NULL, SYS_FAIL(-1019));
    PJ_TEST_EQ(wait_pkt_count(2), 0, NULL, SYS_FAIL(-1027));
    sent = g_server[0].pkt_count + g_server[1].pkt_count;

    /* Then they die: trusted since they answered, the timeout is theirs
     * to notice, and the query fails as without the option
     */
    PJ_TEST_EQ(sys_time_out(res, "sysname1", 0), 0, NULL, SYS_FAIL(-1001));
    PJ_TEST_EQ(sys_cb[0].status, PJ_ETIMEDOUT, NULL, SYS_FAIL(-1002));
    PJ_TEST_EQ(sys_state.count, 0, NULL, SYS_FAIL(-1004));
    PJ_TEST_EQ(wait_pkt_count(sent + 2), 0, NULL, SYS_FAIL(-1025));
    sent = g_server[0].pkt_count + g_server[1].pkt_count;

    /* Reset, as on a network change: the next query probes them, times
     * out and is resolved with the system resolver
     */
    PJ_TEST_SUCCESS(pj_dns_resolver_reset_ns_state(res), NULL,
                    SYS_FAIL(-1201));
    PJ_TEST_EQ(sys_time_out(res, "sysname1b", 1), 0, NULL, SYS_FAIL(-1007));
    PJ_TEST_SUCCESS(sys_cb[1].status, NULL, SYS_FAIL(-1003));
    PJ_TEST_EQ(sys_cb[1].addr, SYS_ADDR, NULL, SYS_FAIL(-1005));
    PJ_TEST_EQ(sys_state.count, 1, NULL, SYS_FAIL(-1024));
    PJ_TEST_EQ(sys_state.af, pj_AF_INET(), NULL, SYS_FAIL(-1202));
    PJ_TEST_EQ(pj_ansi_strcmp(sys_state.name, "sysname1b"), 0, NULL,
               SYS_FAIL(-1006));
    PJ_TEST_TRUE(g_server[0].pkt_count + g_server[1].pkt_count > sent,
                 NULL, SYS_FAIL(-1203));

    /* Both servers are bad now: not sent, answered later all the same */
    PJ_TEST_TRUE(sys_none_works(res), NULL, SYS_FAIL(-1026));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys,
                                                (void*)2, &q),
                    NULL, SYS_FAIL(-1010));
    PJ_TEST_NOT_NULL(q, NULL, SYS_FAIL(-1011));
    PJ_TEST_EQ(sys_cb[2].called, 0, NULL, SYS_FAIL(-1012));
    PJ_TEST_EQ(wait_sys_cb(2), 0, NULL, SYS_FAIL(-1013));
    PJ_TEST_SUCCESS(sys_cb[2].status, NULL, SYS_FAIL(-1014));
    PJ_TEST_EQ(sys_cb[2].addr, SYS_ADDR, NULL, SYS_FAIL(-1015));
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1016));
    PJ_TEST_TRUE(sys_none_works(res), NULL, SYS_FAIL(-1017));

    /* As is a DNS AAAA query */
    sys_cb[1].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_AAAA,
                                                0, &dns_callback_sys,
                                                (void*)1, &q),
                    NULL, SYS_FAIL(-1028));
    PJ_TEST_EQ(wait_sys_cb(1), 0, NULL, SYS_FAIL(-1029));
    PJ_TEST_SUCCESS(sys_cb[1].status, NULL, SYS_FAIL(-1056));
    PJ_TEST_EQ(sys_cb[1].type, PJ_DNS_TYPE_AAAA, NULL, SYS_FAIL(-1057));
    PJ_TEST_EQ(sys_cb[1].addr6_last, 1, NULL, SYS_FAIL(-1058));
    PJ_TEST_EQ(sys_state.af, pj_AF_INET6(), NULL, SYS_FAIL(-1059));
    PJ_TEST_EQ(sys_state.count, 3, NULL, SYS_FAIL(-1074));

    /* The name which timed out is answered now, the query was recycled;
     * the one answered by the system resolver is cached meanwhile
     */
    sys_cb[0].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                NULL),
                    NULL, SYS_FAIL(-1075));
    PJ_TEST_EQ(wait_sys_cb(0), 0, NULL, SYS_FAIL(-1076));
    PJ_TEST_EQ(sys_cb[0].addr, SYS_ADDR, NULL, SYS_FAIL(-1077));
    sys_cb[2].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)2,
                                                &q),
                    NULL, SYS_FAIL(-1078));
    PJ_TEST_EQ(q, NULL, NULL, SYS_FAIL(-1079));
    PJ_TEST_EQ(sys_cb[2].called, 1, NULL, SYS_FAIL(-1204));
    PJ_TEST_EQ(sys_cb[2].addr, SYS_ADDR, NULL, SYS_FAIL(-1099));
    PJ_TEST_EQ(sys_state.count, 4, NULL, SYS_FAIL(-1152));

    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;

    /* Without the option, the timeout is reported */
    res = sys_resolver(PJ_FALSE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1020));
    PJ_TEST_EQ(sys_time_out(res, "sysname3", 0), 0, NULL, SYS_FAIL(-1021));
    PJ_TEST_EQ(sys_cb[0].status, PJ_ETIMEDOUT, NULL, SYS_FAIL(-1022));
    PJ_TEST_EQ(sys_state.count, 4, NULL, SYS_FAIL(-1023));
    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;


on_return:
    sys_cleanup(&res);
    return rc;
}

static struct {
    int                 called;
    pj_status_t         status;
    pj_dns_srv_record   rec;
} sys_srv;

static void srv_cb_sys(void *user_data, pj_status_t status,
                       const pj_dns_srv_record *rec)
{
    PJ_UNUSED_ARG(user_data);

    sys_srv.called++;
    sys_srv.status = status;
    if (rec)
        pj_memcpy(&sys_srv.rec, rec, sizeof(*rec));
    else
        pj_bzero(&sys_srv.rec, sizeof(sys_srv.rec));
}

static int wait_sys_srv(void)
{
    unsigned n;

    for (n = 0; !sys_srv.called && n < 300; ++n)
        pj_thread_sleep(10);
    return sys_srv.called ? 0 : -1;
}

/* The SRV resolution falls back to the address of the domain from the
 * system resolver, as it does on a negative answer, and fails without that
 * fallback; a cached SRV record has its target resolved with the system
 * resolver, on its port.
 */
static int dns_sys_fallback_srv_test(void)
{
    pj_str_t domain = pj_str("example.test");
    pj_str_t cached = pj_str("cached.test");
    pj_str_t none = pj_str("none.test");
    pj_str_t res_name = pj_str("_sip._udp.");
    pj_str_t srv_name = pj_str("_sip._udp.cached.test");
    pj_str_t srv_none = pj_str("_sip._udp.none.test");
    pj_str_t target = pj_str("host.cached.test");
    pj_dns_parsed_packet pkt;
    pj_dns_parsed_query question;
    pj_dns_parsed_rr ans;
    pj_dns_resolver *res = NULL;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, SRV test"));

    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1030));
    PJ_TEST_EQ(sys_time_out(res, "sysname4", 0), 0, NULL, SYS_FAIL(-1031));
    PJ_TEST_TRUE(sys_none_works(res), NULL, SYS_FAIL(-1055));

    pj_bzero(&sys_srv, sizeof(sys_srv));
    PJ_TEST_SUCCESS(pj_dns_srv_resolve(&domain, &res_name, 5060, pool, res,
                                       PJ_DNS_SRV_FALLBACK_A, NULL,
                                       &srv_cb_sys, NULL),
                    NULL, SYS_FAIL(-1032));
    PJ_TEST_EQ(sys_srv.called, 0, NULL, SYS_FAIL(-1033));
    PJ_TEST_EQ(wait_sys_srv(), 0, NULL, SYS_FAIL(-1034));
    PJ_TEST_SUCCESS(sys_srv.status, NULL, SYS_FAIL(-1035));
    PJ_TEST_EQ(sys_srv.rec.count, 1, NULL, SYS_FAIL(-1036));
    PJ_TEST_EQ(sys_srv.rec.entry[0].port, 5060, NULL, SYS_FAIL(-1037));
    PJ_TEST_EQ(sys_srv.rec.entry[0].server.addr_count, 1, NULL,
               SYS_FAIL(-1038));
    PJ_TEST_EQ(sys_srv.rec.entry[0].server.addr[0].ip.v4.s_addr, SYS_ADDR,
               NULL, SYS_FAIL(-1039));
    PJ_TEST_EQ(pj_ansi_strcmp(sys_state.name, "example.test"), 0, NULL,
               SYS_FAIL(-1040));
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1041));

    /* Not asked to fall back: the failure of the nameservers */
    pj_bzero(&sys_srv, sizeof(sys_srv));
    PJ_TEST_SUCCESS(pj_dns_srv_resolve(&domain, &res_name, 5060, pool, res,
                                       0, NULL, &srv_cb_sys, NULL),
                    NULL, SYS_FAIL(-1042));
    PJ_TEST_EQ(wait_sys_srv(), 0, NULL, SYS_FAIL(-1043));
    PJ_TEST_EQ(sys_srv.status, PJLIB_UTIL_EDNSNOWORKINGNS, NULL,
               SYS_FAIL(-1044));
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1045));

    /* A cached negative answer, the domain having no SRV record: the
     * domain's address from the system resolver, on the default port
     */
    pj_bzero(&pkt, sizeof(pkt));
    pj_bzero(&question, sizeof(question));
    pkt.hdr.flags = PJ_DNS_SET_QR(1);
    pkt.hdr.qdcount = 1;
    pkt.q = &question;
    question.name = srv_none;
    question.type = PJ_DNS_TYPE_SRV;
    question.dnsclass = 1;
    PJ_TEST_SUCCESS(pj_dns_resolver_add_entry(res, &pkt, PJ_TRUE), NULL,
                    SYS_FAIL(-1181));
    pj_bzero(&sys_srv, sizeof(sys_srv));
    PJ_TEST_SUCCESS(pj_dns_srv_resolve(&none, &res_name, 5060, pool, res,
                                       PJ_DNS_SRV_FALLBACK_A, NULL,
                                       &srv_cb_sys, NULL),
                    NULL, SYS_FAIL(-1182));
    PJ_TEST_EQ(wait_sys_srv(), 0, NULL, SYS_FAIL(-1183));
    PJ_TEST_SUCCESS(sys_srv.status, NULL, SYS_FAIL(-1184));
    PJ_TEST_EQ(sys_srv.rec.count, 1, NULL, SYS_FAIL(-1185));
    PJ_TEST_EQ(sys_srv.rec.entry[0].port, 5060, NULL, SYS_FAIL(-1186));
    PJ_TEST_EQ(sys_srv.rec.entry[0].server.addr[0].ip.v4.s_addr, SYS_ADDR,
               NULL, SYS_FAIL(-1187));
    PJ_TEST_EQ(pj_ansi_strcmp(sys_state.name, "none.test"), 0, NULL,
               SYS_FAIL(-1188));

    /* A cached SRV record, its target not: the target's address from the
     * system resolver, on the port of the record
     */
    pj_bzero(&pkt, sizeof(pkt));
    pj_bzero(&ans, sizeof(ans));
    pkt.hdr.flags = PJ_DNS_SET_QR(1);
    pkt.hdr.anscount = 1;
    pkt.ans = &ans;
    ans.name = srv_name;
    ans.type = PJ_DNS_TYPE_SRV;
    ans.dnsclass = 1;
    ans.ttl = 300;
    ans.rdata.srv.prio = 10;
    ans.rdata.srv.weight = 0;
    ans.rdata.srv.port = 5070;
    ans.rdata.srv.target = target;
    PJ_TEST_SUCCESS(pj_dns_resolver_add_entry(res, &pkt, PJ_TRUE), NULL,
                    SYS_FAIL(-1046));
    pj_bzero(&sys_srv, sizeof(sys_srv));
    PJ_TEST_SUCCESS(pj_dns_srv_resolve(&cached, &res_name, 5060, pool, res,
                                       PJ_DNS_SRV_FALLBACK_A, NULL,
                                       &srv_cb_sys, NULL),
                    NULL, SYS_FAIL(-1047));
    PJ_TEST_EQ(wait_sys_srv(), 0, NULL, SYS_FAIL(-1048));
    PJ_TEST_SUCCESS(sys_srv.status, NULL, SYS_FAIL(-1049));
    PJ_TEST_EQ(sys_srv.rec.count, 1, NULL, SYS_FAIL(-1050));
    PJ_TEST_EQ(sys_srv.rec.entry[0].port, 5070, NULL, SYS_FAIL(-1051));
    PJ_TEST_EQ(sys_srv.rec.entry[0].server.addr[0].ip.v4.s_addr, SYS_ADDR,
               NULL, SYS_FAIL(-1052));
    PJ_TEST_EQ(pj_ansi_strcmp(sys_state.name, "host.cached.test"), 0, NULL,
               SYS_FAIL(-1053));
    PJ_TEST_TRUE(sys_none_works(res), NULL, SYS_FAIL(-1054));

    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;

on_return:
    sys_cleanup(&res);
    return rc;
}

/* The cache comes first: a cached address, and a cached NXDOMAIN answer for
 * the other address type of the name.
 */
static int dns_sys_fallback_cache_test(void)
{
    pj_str_t cached = pj_str("cachedname");
    pj_str_t unknown = pj_str("nxname");
    pj_dns_parsed_packet pkt;
    pj_dns_parsed_query question;
    pj_dns_parsed_rr ans;
    pj_dns_resolver *res = NULL;
    pj_dns_async_query *q;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, cache test"));

    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1060));
    PJ_TEST_EQ(sys_time_out(res, "sysname5", 0), 0, NULL, SYS_FAIL(-1061));

    pj_bzero(&pkt, sizeof(pkt));
    pj_bzero(&ans, sizeof(ans));
    pkt.hdr.flags = PJ_DNS_SET_QR(1);
    pkt.hdr.anscount = 1;
    pkt.ans = &ans;
    ans.name = cached;
    ans.type = PJ_DNS_TYPE_A;
    ans.dnsclass = 1;
    ans.ttl = 300;
    ans.rdata.a.ip_addr.s_addr = IP_ADDR0;
    PJ_TEST_SUCCESS(pj_dns_resolver_add_entry(res, &pkt, PJ_TRUE), NULL,
                    SYS_FAIL(-1062));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &cached, PJ_DNS_TYPE_A,
                                                0, &dns_callback_sys,
                                                (void*)1, &q),
                    NULL, SYS_FAIL(-1063));
    PJ_TEST_TRUE(q == NULL, NULL, SYS_FAIL(-1064));
    PJ_TEST_EQ(sys_cb[1].called, 1, NULL, SYS_FAIL(-1065));
    PJ_TEST_EQ(sys_cb[1].addr, IP_ADDR0, NULL, SYS_FAIL(-1066));
    PJ_TEST_EQ(sys_state.count, 1, NULL, SYS_FAIL(-1067));

    /* The name doesn't exist: not asked for its other address type */
    pj_bzero(&pkt, sizeof(pkt));
    pj_bzero(&question, sizeof(question));
    pkt.hdr.flags = PJ_DNS_SET_QR(1) | PJ_DNS_SET_RCODE(PJ_DNS_RCODE_NXDOMAIN);
    pkt.hdr.qdcount = 1;
    pkt.q = &question;
    question.name = unknown;
    question.type = PJ_DNS_TYPE_A;
    question.dnsclass = 1;
    PJ_TEST_SUCCESS(pj_dns_resolver_add_entry(res, &pkt, PJ_TRUE), NULL,
                    SYS_FAIL(-1068));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &unknown,
                                                PJ_DNS_TYPE_AAAA, 0,
                                                &dns_callback_sys,
                                                (void*)2, &q),
                    NULL, SYS_FAIL(-1069));
    PJ_TEST_NOT_NULL(q, NULL, SYS_FAIL(-1070));
    PJ_TEST_EQ(wait_sys_cb(2), 0, NULL, SYS_FAIL(-1071));
    PJ_TEST_EQ(sys_cb[2].status,
               PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_NXDOMAIN), NULL,
               SYS_FAIL(-1072));
    PJ_TEST_EQ(sys_state.count, 1, NULL, SYS_FAIL(-1073));

    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;

on_return:
    sys_cleanup(&res);
    return rc;
}

/* The queries for a name being looked up join it: one lookup, all
 * answered; a cancelled one is not.
 */
static int dns_sys_fallback_join_test(void)
{
    pj_str_t name = pj_str("joined");
    pj_dns_resolver *res = NULL;
    pj_dns_async_query *q0, *q1, *q2;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, joined queries test"));

    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1080));
    PJ_TEST_EQ(sys_time_out(res, "sysname6", 3), 0, NULL, SYS_FAIL(-1081));

    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 1, &sys_state.block), NULL,
                    SYS_FAIL(-1082));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                &q0),
                    NULL, SYS_FAIL(-1083));
    PJ_TEST_EQ(wait_sys_count(2), 0, NULL, SYS_FAIL(-1098));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)1,
                                                &q1),
                    NULL, SYS_FAIL(-1084));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)2,
                                                &q2),
                    NULL, SYS_FAIL(-1085));
    PJ_TEST_TRUE(q0 && q1 && q2, NULL, SYS_FAIL(-1086));
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1087));

    /* The first one silently, the last one with the notification */
    PJ_TEST_SUCCESS(pj_dns_resolver_cancel_query(q0, PJ_FALSE), NULL,
                    SYS_FAIL(-1088));
    PJ_TEST_SUCCESS(pj_dns_resolver_cancel_query(q2, PJ_TRUE), NULL,
                    SYS_FAIL(-1089));
    PJ_TEST_EQ(sys_cb[2].called, 1, NULL, SYS_FAIL(-1090));
    PJ_TEST_EQ(sys_cb[2].status, PJ_ECANCELLED, NULL, SYS_FAIL(-1091));

    pj_sem_post(sys_state.block);
    PJ_TEST_EQ(wait_sys_cb(1), 0, NULL, SYS_FAIL(-1092));
    PJ_TEST_SUCCESS(sys_cb[1].status, NULL, SYS_FAIL(-1093));
    PJ_TEST_EQ(sys_cb[1].addr, SYS_ADDR, NULL, SYS_FAIL(-1094));
    /* The query is reported before the queries which joined it */
    PJ_TEST_EQ(sys_cb[0].called, 0, NULL, SYS_FAIL(-1095));
    PJ_TEST_EQ(sys_cb[2].called, 1, NULL, SYS_FAIL(-1096));
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1097));

    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;
    pj_sem_destroy(sys_state.block);
    sys_state.block = NULL;

on_return:
    sys_cleanup(&res);
    return rc;
}

/* Lets the lookup go on once the destroy of the resolver has started and
 * waits for it: a query is refused from then on.
 */
static int sys_unblock_thread(void *arg)
{
    pj_dns_resolver *res = (pj_dns_resolver*)arg;
    pj_str_t name = pj_str("unblock");

    while (pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0, NULL,
                                       NULL, NULL) != PJ_EGONE)
    {
        pj_thread_sleep(10);
    }
    pj_sem_post(sys_state.block);
    return 0;
}

/* Destroying the resolver during a lookup waits for it, and reports the
 * queries once.
 */
static int dns_sys_fallback_destroy_test(void)
{
    pj_str_t name = pj_str("destroyed");
    pj_dns_resolver *res = NULL;
    pj_thread_t *thread = NULL;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, destroy test"));

    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1100));
    PJ_TEST_EQ(sys_time_out(res, "sysname7", 3), 0, NULL, SYS_FAIL(-1101));

    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 1, &sys_state.block), NULL,
                    SYS_FAIL(-1102));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                NULL),
                    NULL, SYS_FAIL(-1103));
    PJ_TEST_EQ(wait_sys_count(2), 0, NULL, SYS_FAIL(-1112));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)1,
                                                NULL),
                    NULL, SYS_FAIL(-1104));
    PJ_TEST_SUCCESS(pj_thread_create(pool, NULL, &sys_unblock_thread, res,
                                     0, 0, &thread),
                    NULL, SYS_FAIL(-1105));

    /* Returns once the lookup went on */
    pj_dns_resolver_destroy(res, PJ_TRUE);
    res = NULL;
    PJ_TEST_TRUE(sys_state.unblocked, NULL, SYS_FAIL(-1106));
    PJ_TEST_EQ(sys_cb[0].called, 1, NULL, SYS_FAIL(-1107));
    PJ_TEST_EQ(sys_cb[0].status, PJ_ECANCELLED, NULL, SYS_FAIL(-1108));
    PJ_TEST_EQ(sys_cb[1].called, 1, NULL, SYS_FAIL(-1109));
    PJ_TEST_EQ(sys_cb[1].status, PJ_ECANCELLED, NULL, SYS_FAIL(-1110));

    pj_thread_join(thread);
    pj_thread_destroy(thread);
    thread = NULL;
    /* A late report would come within this */
    pj_thread_sleep(200);
    PJ_TEST_EQ(sys_cb[0].called + sys_cb[1].called, 2, NULL, SYS_FAIL(-1111));

    pj_sem_destroy(sys_state.block);
    sys_state.block = NULL;

on_return:
    /* It posts the semaphore: before it is destroyed */
    if (thread) {
        pj_thread_join(thread);
        pj_thread_destroy(thread);
    }
    sys_cleanup(&res);
    return rc;
}

/* When the system resolver fails, the error of the nameservers is reported:
 * none works, or the query timed out.
 */
static int dns_sys_fallback_failure_test(void)
{
    pj_str_t name = pj_str("sysfail2");
    pj_dns_resolver *res = NULL;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, failure test"));

    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1120));
    PJ_TEST_EQ(sys_time_out(res, "sysfail1", 0), 0, NULL, SYS_FAIL(-1121));

    sys_state.status = PJ_ERESOLVE;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)1,
                                                NULL),
                    NULL, SYS_FAIL(-1122));
    PJ_TEST_EQ(wait_sys_cb(1), 0, NULL, SYS_FAIL(-1123));
    PJ_TEST_EQ(sys_cb[1].status, PJLIB_UTIL_EDNSNOWORKINGNS, NULL,
               SYS_FAIL(-1124));
    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;

    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1125));
    PJ_TEST_EQ(sys_time_out(res, "sysfail3", 2), 0, NULL, SYS_FAIL(-1126));
    PJ_TEST_EQ(sys_cb[2].status, PJ_ETIMEDOUT, NULL, SYS_FAIL(-1127));
    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;


on_return:
    sys_cleanup(&res);
    return rc;
}

/* A server which answers all but one type of query, or one name */
static struct {
    int         drop_type;
    const char *drop_name;
} sys_drop;

static void drop_cb(const pj_dns_parsed_packet *pkt,
                    pj_dns_parsed_packet **p_res)
{
    pj_dns_parsed_packet *res;

    if (pkt->q[0].type == sys_drop.drop_type ||
        (sys_drop.drop_name &&
         pj_strcmp2(&pkt->q[0].name, sys_drop.drop_name) == 0))
    {
        *p_res = NULL;
        return;
    }

    lock();
    res = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);
    res->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    res->ans = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_rr);
    unlock();

    res->hdr.qdcount = 1;
    res->q[0] = pkt->q[0];
    res->hdr.anscount = 1;
    res->ans[0].type = pkt->q[0].type;
    res->ans[0].dnsclass = 1;
    res->ans[0].name = pkt->q[0].name;
    res->ans[0].ttl = 300;
    if (pkt->q[0].type == PJ_DNS_TYPE_SRV) {
        res->ans[0].rdata.srv.prio = 1;
        res->ans[0].rdata.srv.weight = 0;
        res->ans[0].rdata.srv.port = 5070;
        res->ans[0].rdata.srv.target = pj_str("host.trusted.test");
    } else if (pkt->q[0].type == PJ_DNS_TYPE_A) {
        res->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;
    } else {
        res->ans[0].rdata.aaaa.ip_addr.s6_addr[15] = 2;
    }
    *p_res = res;
}

/* Time out n queries of the type on the server, and expect the given
 * status for each.
 */
static int sys_time_out_type(pj_dns_resolver *res, const char *name,
                             int type, unsigned n, pj_status_t expected)
{
    pj_str_t nm = pj_str((char*)name);
    unsigned i;

    for (i = 0; i < n; ++i) {
        sys_cb[0].called = 0;
        if (pj_dns_resolver_start_query(res, &nm, type, 0, &dns_callback_sys,
                                        (void*)0, NULL) != PJ_SUCCESS)
        {
            return -1;
        }
        if (wait_sys_cb(0) != 0)
            return -2;
        if (sys_cb[0].status != expected)
            return -3;
    }
    return 0;
}

/* The option leaves the state of the nameservers alone: a trusted one,
 * which drops one type of query or one name, keeps being trusted, and the
 * timeouts are reported as without the option; the resolver's own probing
 * decides, in both cases alike; a dead server next to a working one is the
 * resolver's to notice too.
 */
static int dns_sys_fallback_trusted_test(void)
{
    pj_str_t domain = pj_str("trusted.test");
    pj_str_t domain2 = pj_str("other.test");
    pj_str_t res_name = pj_str("_sip._udp.");
    pj_str_t name = pj_str("name.trusted.test");
    pj_dns_resolver *res = NULL;
    pj_dns_settings lset;
    pj_bool_t fallback;
    unsigned sent, pass;
    pj_status_t status;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, trusted servers test"));

    /* With and without the option: the same, but for the last request */
    for (pass = 0; pass < 2; ++pass) {
        fallback = (pass == 0);
        sys_reset();
        pj_bzero(&sys_drop, sizeof(sys_drop));
        sys_drop.drop_type = PJ_DNS_TYPE_AAAA;
        g_server[0].action = ACTION_CB;
        g_server[0].action_cb = &drop_cb;
        res = sys_resolver_ns(fallback, 100, 1);
        PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1210));

        /* Trusted once it answers; the AAAA queries time out, as many
         * times as the probing would take, and it stays trusted
         */
        PJ_TEST_EQ(sys_time_out_type(res, "name.trusted.test", PJ_DNS_TYPE_A,
                                     1, PJ_SUCCESS),
                   0, NULL, SYS_FAIL(-1211));
        PJ_TEST_EQ(sys_cb[0].addr, IP_ADDR0, NULL, SYS_FAIL(-1212));
        PJ_TEST_EQ(sys_time_out_type(res, "name.trusted.test",
                                     PJ_DNS_TYPE_AAAA, 3, PJ_ETIMEDOUT),
                   0, NULL, SYS_FAIL(-1213));
        PJ_TEST_EQ(sys_state.count, 0, NULL, SYS_FAIL(-1214));

        /* SRV still answered by DNS, with its port and target */
        pj_bzero(&sys_srv, sizeof(sys_srv));
        PJ_TEST_SUCCESS(pj_dns_srv_resolve(&domain, &res_name, 5060, pool,
                                           res, PJ_DNS_SRV_FALLBACK_A, NULL,
                                           &srv_cb_sys, NULL),
                        NULL, SYS_FAIL(-1215));
        PJ_TEST_EQ(wait_sys_srv(), 0, NULL, SYS_FAIL(-1216));
        PJ_TEST_SUCCESS(sys_srv.status, NULL, SYS_FAIL(-1217));
        PJ_TEST_EQ(sys_srv.rec.entry[0].port, 5070, NULL, SYS_FAIL(-1218));
        PJ_TEST_EQ(sys_srv.rec.entry[0].server.addr[0].ip.v4.s_addr,
                   IP_ADDR0, NULL, SYS_FAIL(-1219));
        PJ_TEST_EQ(sys_state.count, 0, NULL, SYS_FAIL(-1220));

        /* One name dropped, trusted all the same; the answer which
         * follows is trusted for a second only, for what comes next
         */
        pj_dns_resolver_get_settings(res, &lset);
        lset.good_ns_ttl = 1;
        pj_dns_resolver_set_settings(res, &lset);
        sys_drop.drop_name = "silent.trusted.test";
        PJ_TEST_EQ(sys_time_out_type(res, "silent.trusted.test",
                                     PJ_DNS_TYPE_A, 1, PJ_ETIMEDOUT),
                   0, NULL, SYS_FAIL(-1221));
        PJ_TEST_EQ(sys_time_out_type(res, "name2.trusted.test",
                                     PJ_DNS_TYPE_A, 1, PJ_SUCCESS),
                   0, NULL, SYS_FAIL(-1222));
        PJ_TEST_EQ(sys_state.count, 0, NULL, SYS_FAIL(-1223));
        sys_drop.drop_name = NULL;

        /* The resolver's own probing happens to be an AAAA query: the
         * server is marked as bad either way, and only then does the
         * option apply, the SRV request going to the domain's address
         */
        pj_thread_sleep(1500);
        sent = g_server[0].pkt_count;
        PJ_TEST_EQ(sys_time_out_type(res, "name.trusted.test",
                                     PJ_DNS_TYPE_AAAA, 1,
                                     fallback ? PJ_SUCCESS : PJ_ETIMEDOUT),
                   0, NULL, SYS_FAIL(-1224));
        PJ_TEST_TRUE(g_server[0].pkt_count > sent, NULL, SYS_FAIL(-1225));
        sys_wait_probing();
        PJ_TEST_TRUE(sys_none_works(res), NULL, SYS_FAIL(-1226));
        /* Another domain: the SRV record of the first one is cached */
        pj_bzero(&sys_srv, sizeof(sys_srv));
        status = pj_dns_srv_resolve(&domain2, &res_name, 5060, pool, res,
                                PJ_DNS_SRV_FALLBACK_A, NULL, &srv_cb_sys,
                                NULL);
        if (fallback) {
            PJ_TEST_SUCCESS(status, NULL, SYS_FAIL(-1227));
            PJ_TEST_EQ(wait_sys_srv(), 0, NULL, SYS_FAIL(-1228));
            PJ_TEST_SUCCESS(sys_srv.status, NULL, SYS_FAIL(-1229));
            PJ_TEST_EQ(sys_srv.rec.entry[0].port, 5060, NULL, SYS_FAIL(-1230));
            PJ_TEST_EQ(sys_srv.rec.entry[0].server.addr[0].ip.v4.s_addr,
                       SYS_ADDR, NULL, SYS_FAIL(-1231));
            PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1232));
        } else {
            PJ_TEST_EQ(status, PJLIB_UTIL_EDNSNOWORKINGNS, NULL,
                       SYS_FAIL(-1233));
            PJ_TEST_EQ(sys_state.count, 0, NULL, SYS_FAIL(-1234));
        }
        pj_dns_resolver_destroy(res, PJ_FALSE);
        res = NULL;

        if (!fallback)
            break;
    }

    /* A dead server next to a working one: answered, no lookup */
    sys_reset();
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = ACTION_REPLY;
    g_server[1].resp.hdr.qdcount = 1;
    g_server[1].resp.hdr.anscount = 1;
    g_server[1].resp.q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    g_server[1].resp.q[0].type = PJ_DNS_TYPE_A;
    g_server[1].resp.q[0].dnsclass = 1;
    g_server[1].resp.q[0].name = name;
    g_server[1].resp.ans = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_rr);
    g_server[1].resp.ans[0].type = PJ_DNS_TYPE_A;
    g_server[1].resp.ans[0].dnsclass = 1;
    g_server[1].resp.ans[0].name = name;
    g_server[1].resp.ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1235));
    PJ_TEST_EQ(sys_time_out_type(res, "name.trusted.test", PJ_DNS_TYPE_A, 1,
                                 PJ_SUCCESS),
               0, NULL, SYS_FAIL(-1236));
    PJ_TEST_EQ(sys_cb[0].addr, IP_ADDR0, NULL, SYS_FAIL(-1237));
    PJ_TEST_EQ(sys_state.count, 0, NULL, SYS_FAIL(-1238));
    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;


on_return:
    sys_cleanup(&res);
    return rc;
}

/* A query nobody waits for anymore is not looked up: cancelled before its
 * timeout, or while waiting for the lookup thread; one started without a
 * callback fails at once, as without the option.
 */
static int dns_sys_fallback_cancel_test(void)
{
    pj_str_t name1 = pj_str("cancel1");
    pj_str_t name2 = pj_str("cancel2");
    pj_str_t name3 = pj_str("cancel3");
    pj_str_t name4 = pj_str("cancel4");
    pj_str_t name5 = pj_str("cancel5");
    pj_dns_resolver *res = NULL;
    pj_dns_settings lset;
    pj_dns_async_query *q;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, cancelled queries"));

    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1240));

    /* Cancelled while sent: the timeout doesn't look it up */
    g_server[0].action = g_server[1].action = ACTION_IGNORE;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                &q),
                    NULL, SYS_FAIL(-1241));
    PJ_TEST_SUCCESS(pj_dns_resolver_cancel_query(q, PJ_FALSE), NULL,
                    SYS_FAIL(-1242));
    PJ_TEST_EQ(sys_time_out(res, "cancel1b", 1), 0, NULL, SYS_FAIL(-1243));
    PJ_TEST_SUCCESS(sys_cb[1].status, NULL, SYS_FAIL(-1244));
    PJ_TEST_EQ(sys_cb[0].called, 0, NULL, SYS_FAIL(-1245));
    PJ_TEST_EQ(sys_state.count, 1, NULL, SYS_FAIL(-1246));
    PJ_TEST_EQ(pj_ansi_strcmp(sys_state.name, "cancel1b"), 0, NULL,
               SYS_FAIL(-1247));

    /* Cancelled while waiting for the one lookup thread: skipped */
    sys_set_threads(res, 1);
    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 8, &sys_state.block), NULL,
                    SYS_FAIL(-1248));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name2, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)2,
                                                NULL),
                    NULL, SYS_FAIL(-1249));
    PJ_TEST_EQ(wait_sys_count(2), 0, NULL, SYS_FAIL(-1250));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name3, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)3,
                                                &q),
                    NULL, SYS_FAIL(-1251));
    PJ_TEST_SUCCESS(pj_dns_resolver_cancel_query(q, PJ_FALSE), NULL,
                    SYS_FAIL(-1252));
    pj_sem_post(sys_state.block);
    PJ_TEST_EQ(wait_sys_cb(2), 0, NULL, SYS_FAIL(-1253));
    PJ_TEST_SUCCESS(sys_cb[2].status, NULL, SYS_FAIL(-1254));
    pj_thread_sleep(200);
    PJ_TEST_EQ(sys_cb[3].called, 0, NULL, SYS_FAIL(-1255));
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1256));

    /* Started without a callback: the error of the nameservers */
    PJ_TEST_EQ(pj_dns_resolver_start_query(res, &name3, PJ_DNS_TYPE_A, 0,
                                           NULL, NULL, &q),
               PJLIB_UTIL_EDNSNOWORKINGNS, NULL, SYS_FAIL(-1257));
    PJ_TEST_EQ(q, NULL, NULL, SYS_FAIL(-1258));
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1259));

    /* The option turned off while a query waits for the lookup thread,
     * as PJSUA does when destroying: the error of the nameservers
     */
    sys_cb[3].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name2, PJ_DNS_TYPE_AAAA,
                                                0, &dns_callback_sys,
                                                (void*)2, NULL),
                    NULL, SYS_FAIL(-1380));
    PJ_TEST_EQ(wait_sys_count(3), 0, NULL, SYS_FAIL(-1381));
    sys_cb[2].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name3, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)3,
                                                NULL),
                    NULL, SYS_FAIL(-1382));
    pj_dns_resolver_get_settings(res, &lset);
    lset.sys_fallback = PJ_FALSE;
    pj_dns_resolver_set_settings(res, &lset);
    pj_sem_post(sys_state.block);
    PJ_TEST_EQ(wait_sys_cb(2), 0, NULL, SYS_FAIL(-1383));
    PJ_TEST_EQ(wait_sys_cb(3), 0, NULL, SYS_FAIL(-1384));
    PJ_TEST_EQ(sys_cb[3].status, PJLIB_UTIL_EDNSNOWORKINGNS, NULL,
               SYS_FAIL(-1385));
    PJ_TEST_EQ(sys_state.count, 3, NULL, SYS_FAIL(-1386));

    /* A query cancelled while waiting, one which joined it not: looked up
     * for the latter
     */
    lset.sys_fallback = PJ_TRUE;
    pj_dns_resolver_set_settings(res, &lset);
    pj_bzero(sys_cb, sizeof(sys_cb));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name4, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                NULL),
                    NULL, SYS_FAIL(-1387));
    PJ_TEST_EQ(wait_sys_count(4), 0, NULL, SYS_FAIL(-1388));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name5, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)1,
                                                &q),
                    NULL, SYS_FAIL(-1389));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name5, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)2,
                                                NULL),
                    NULL, SYS_FAIL(-1390));
    PJ_TEST_SUCCESS(pj_dns_resolver_cancel_query(q, PJ_FALSE), NULL,
                    SYS_FAIL(-1391));
    /* For the lookup in progress, and the one of the query which joined */
    pj_sem_post(sys_state.block);
    pj_sem_post(sys_state.block);
    PJ_TEST_EQ(wait_sys_cb(2), 0, NULL, SYS_FAIL(-1392));
    PJ_TEST_SUCCESS(sys_cb[2].status, NULL, SYS_FAIL(-1393));
    PJ_TEST_EQ(sys_cb[2].addr, SYS_ADDR, NULL, SYS_FAIL(-1394));
    PJ_TEST_EQ(sys_cb[1].called, 0, NULL, SYS_FAIL(-1395));
    PJ_TEST_EQ(sys_state.count, 5, NULL, SYS_FAIL(-1396));

    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;
    pj_sem_destroy(sys_state.block);
    sys_state.block = NULL;

on_return:
    sys_cleanup(&res);
    return rc;
}

/* Set the server up to answer the A record of the name */
static void sys_server_answers(unsigned srv, const pj_str_t *name)
{
    pj_dns_parsed_packet *r = &g_server[srv].resp;

    pj_bzero(r, sizeof(*r));
    r->hdr.qdcount = 1;
    r->hdr.anscount = 1;
    r->q = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_query);
    r->q[0].type = PJ_DNS_TYPE_A;
    r->q[0].dnsclass = 1;
    r->q[0].name = *name;
    r->ans = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_rr);
    r->ans[0].type = PJ_DNS_TYPE_A;
    r->ans[0].dnsclass = 1;
    r->ans[0].name = *name;
    r->ans[0].ttl = 300;
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;
    g_server[srv].action = ACTION_REPLY;
}

/* A nameserver which refuses, or fails an address, is not answering about
 * the name: the system resolver is asked, and the error is cached only when
 * it fails too; a failed AAAA stands; SRV goes to the domain's address.
 */
static int dns_sys_fallback_rcode_test(void)
{
    pj_str_t name1 = pj_str("refused1");
    pj_str_t name2 = pj_str("refused2");
    pj_str_t name3 = pj_str("servfail1");
    pj_str_t name4 = pj_str("servfail2");
    pj_str_t domain = pj_str("rcode.test");
    pj_str_t res_name = pj_str("_sip._udp.");
    pj_dns_resolver *res = NULL;
    pj_dns_async_query *q;
    unsigned sent;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, refused test"));

    sys_reset();
    g_server[0].action = PJ_DNS_RCODE_REFUSED;
    res = sys_resolver_ns(PJ_TRUE, 100, 1);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1260));

    /* Refused: the system resolver's answer */
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                NULL),
                    NULL, SYS_FAIL(-1261));
    PJ_TEST_EQ(wait_sys_cb(0), 0, NULL, SYS_FAIL(-1262));
    PJ_TEST_SUCCESS(sys_cb[0].status, NULL, SYS_FAIL(-1263));
    PJ_TEST_EQ(sys_cb[0].addr, SYS_ADDR, NULL, SYS_FAIL(-1264));
    PJ_TEST_EQ(sys_state.count, 1, NULL, SYS_FAIL(-1265));

    /* What is cached is that answer, not the refusal; once the server
     * answers, its answer
     */
    sys_cb[0].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                &q),
                    NULL, SYS_FAIL(-1311));
    PJ_TEST_EQ(q, NULL, NULL, SYS_FAIL(-1312));
    PJ_TEST_EQ(sys_cb[0].called, 1, NULL, SYS_FAIL(-1313));
    PJ_TEST_EQ(sys_cb[0].addr, SYS_ADDR, NULL, SYS_FAIL(-1314));
    PJ_TEST_SUCCESS(pj_dns_resolver_clear_cache(res), NULL, SYS_FAIL(-1315));
    sys_server_answers(0, &name1);
    PJ_TEST_SUCCESS(pj_dns_resolver_reset_ns_state(res), NULL,
                    SYS_FAIL(-1266));
    sys_cb[0].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                &q),
                    NULL, SYS_FAIL(-1267));
    PJ_TEST_NOT_NULL(q, NULL, SYS_FAIL(-1268));
    PJ_TEST_EQ(wait_sys_cb(0), 0, NULL, SYS_FAIL(-1269));
    PJ_TEST_EQ(sys_cb[0].addr, IP_ADDR0, NULL, SYS_FAIL(-1270));
    PJ_TEST_EQ(sys_state.count, 1, NULL, SYS_FAIL(-1271));

    /* Refused and the system resolver fails: refused, and cached */
    g_server[0].action = PJ_DNS_RCODE_REFUSED;
    sys_state.status = PJ_ERESOLVE;
    PJ_TEST_SUCCESS(pj_dns_resolver_reset_ns_state(res), NULL,
                    SYS_FAIL(-1272));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name2, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)1,
                                                NULL),
                    NULL, SYS_FAIL(-1273));
    PJ_TEST_EQ(wait_sys_cb(1), 0, NULL, SYS_FAIL(-1274));
    PJ_TEST_EQ(sys_cb[1].status,
               PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_REFUSED), NULL,
               SYS_FAIL(-1275));
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1276));
    sys_cb[1].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name2, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)1,
                                                &q),
                    NULL, SYS_FAIL(-1277));
    PJ_TEST_EQ(q, NULL, NULL, SYS_FAIL(-1278));
    PJ_TEST_EQ(sys_cb[1].called, 1, NULL, SYS_FAIL(-1279));
    PJ_TEST_EQ(sys_cb[1].status,
               PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_REFUSED), NULL,
               SYS_FAIL(-1280));
    PJ_TEST_EQ(sys_state.count, 2, NULL, SYS_FAIL(-1281));
    sys_state.status = PJ_SUCCESS;

    /* Server failure: an address from the system resolver, the server
     * trusted all the same; a failed AAAA stands
     */
    g_server[0].action = PJ_DNS_RCODE_SERVFAIL;
    PJ_TEST_SUCCESS(pj_dns_resolver_reset_ns_state(res), NULL,
                    SYS_FAIL(-1282));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name3, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)2,
                                                NULL),
                    NULL, SYS_FAIL(-1283));
    PJ_TEST_EQ(wait_sys_cb(2), 0, NULL, SYS_FAIL(-1284));
    PJ_TEST_SUCCESS(sys_cb[2].status, NULL, SYS_FAIL(-1285));
    PJ_TEST_EQ(sys_cb[2].addr, SYS_ADDR, NULL, SYS_FAIL(-1286));
    PJ_TEST_EQ(sys_state.count, 3, NULL, SYS_FAIL(-1287));
    sys_cb[2].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name3,
                                                PJ_DNS_TYPE_AAAA, 0,
                                                &dns_callback_sys, (void*)2,
                                                NULL),
                    NULL, SYS_FAIL(-1288));
    PJ_TEST_EQ(wait_sys_cb(2), 0, NULL, SYS_FAIL(-1289));
    PJ_TEST_EQ(sys_cb[2].status,
               PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_SERVFAIL), NULL,
               SYS_FAIL(-1290));
    PJ_TEST_EQ(sys_state.count, 3, NULL, SYS_FAIL(-1291));
    sent = g_server[0].pkt_count;
    sys_cb[2].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name4, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)2,
                                                NULL),
                    NULL, SYS_FAIL(-1292));
    PJ_TEST_EQ(wait_sys_cb(2), 0, NULL, SYS_FAIL(-1293));
    PJ_TEST_SUCCESS(sys_cb[2].status, NULL, SYS_FAIL(-1294));
    PJ_TEST_TRUE(g_server[0].pkt_count > sent, NULL, SYS_FAIL(-1295));
    PJ_TEST_EQ(sys_state.count, 4, NULL, SYS_FAIL(-1296));

    /* SRV failed by the server: the domain's address, from the system
     * resolver as its A query is failed too
     */
    pj_bzero(&sys_srv, sizeof(sys_srv));
    PJ_TEST_SUCCESS(pj_dns_srv_resolve(&domain, &res_name, 5060, pool, res,
                                       PJ_DNS_SRV_FALLBACK_A, NULL,
                                       &srv_cb_sys, NULL),
                    NULL, SYS_FAIL(-1297));
    PJ_TEST_EQ(wait_sys_srv(), 0, NULL, SYS_FAIL(-1298));
    PJ_TEST_SUCCESS(sys_srv.status, NULL, SYS_FAIL(-1299));
    PJ_TEST_EQ(sys_srv.rec.entry[0].port, 5060, NULL, SYS_FAIL(-1300));
    PJ_TEST_EQ(sys_srv.rec.entry[0].server.addr[0].ip.v4.s_addr, SYS_ADDR,
               NULL, SYS_FAIL(-1301));
    PJ_TEST_EQ(sys_state.count, 5, NULL, SYS_FAIL(-1302));
    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;

    /* Without the option: refused, and cached */
    sys_reset();
    g_server[0].action = PJ_DNS_RCODE_REFUSED;
    res = sys_resolver_ns(PJ_FALSE, 100, 1);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1303));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                NULL),
                    NULL, SYS_FAIL(-1304));
    PJ_TEST_EQ(wait_sys_cb(0), 0, NULL, SYS_FAIL(-1305));
    PJ_TEST_EQ(sys_cb[0].status,
               PJ_STATUS_FROM_DNS_RCODE(PJ_DNS_RCODE_REFUSED), NULL,
               SYS_FAIL(-1306));
    sys_cb[0].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                &q),
                    NULL, SYS_FAIL(-1307));
    PJ_TEST_EQ(q, NULL, NULL, SYS_FAIL(-1308));
    PJ_TEST_EQ(sys_cb[0].called, 1, NULL, SYS_FAIL(-1309));
    PJ_TEST_EQ(sys_state.count, 0, NULL, SYS_FAIL(-1310));
    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;


on_return:
    sys_cleanup(&res);
    return rc;
}

/* The lookups run at the same time, as many as the threads setting; what
 * the system resolver answered is cached while the nameservers are left
 * alone, unless caching is off; its failure is not.
 */
static int dns_sys_fallback_pool_test(void)
{
    pj_str_t name1 = pj_str("pool1");
    pj_str_t name2 = pj_str("pool2");
    pj_str_t name4 = pj_str("pool4");
    pj_str_t name5 = pj_str("pool5");
    pj_str_t name6 = pj_str("pool6");
    pj_str_t name7 = pj_str("pool7");
    pj_str_t name8 = pj_str("pool8");
    pj_dns_resolver *res = NULL;
    pj_dns_settings lset;
    pj_dns_async_query *q;
    unsigned i;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, lookup threads test"));

    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1320));
    PJ_TEST_EQ(sys_time_out(res, "pool0", 3), 0, NULL, SYS_FAIL(-1321));

    /* Three names at once: three lookups while all are blocked */
    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 8, &sys_state.block), NULL,
                    SYS_FAIL(-1322));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                NULL),
                    NULL, SYS_FAIL(-1323));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1,
                                                PJ_DNS_TYPE_AAAA, 0,
                                                &dns_callback_sys, (void*)1,
                                                NULL),
                    NULL, SYS_FAIL(-1324));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name2, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)2,
                                                NULL),
                    NULL, SYS_FAIL(-1325));
    PJ_TEST_EQ(wait_sys_count(4), 0, NULL, SYS_FAIL(-1326));
    for (i = 0; i < 3; ++i)
        pj_sem_post(sys_state.block);
    for (i = 0; i < 3; ++i) {
        PJ_TEST_EQ(wait_sys_cb(i), 0, NULL, SYS_FAIL(-1327));
        PJ_TEST_SUCCESS(sys_cb[i].status, NULL, SYS_FAIL(-1328));
    }
    PJ_TEST_EQ(sys_cb[0].addr, SYS_ADDR, NULL, SYS_FAIL(-1329));
    PJ_TEST_EQ(sys_cb[1].addr6_last, 1, NULL, SYS_FAIL(-1330));
    PJ_TEST_EQ(sys_state.count, 4, NULL, SYS_FAIL(-1331));

    /* Cached meanwhile: answered at once, no lookup */
    sys_cb[0].called = 0;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                &q),
                    NULL, SYS_FAIL(-1332));
    PJ_TEST_EQ(q, NULL, NULL, SYS_FAIL(-1333));
    PJ_TEST_EQ(sys_cb[0].called, 1, NULL, SYS_FAIL(-1334));
    PJ_TEST_EQ(sys_cb[0].addr, SYS_ADDR, NULL, SYS_FAIL(-1335));
    PJ_TEST_EQ(sys_state.count, 4, NULL, SYS_FAIL(-1336));
    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;
    pj_sem_destroy(sys_state.block);
    sys_state.block = NULL;

    /* Two threads: the third name waits for one of them */
    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1337));
    sys_set_threads(res, 2);
    PJ_TEST_EQ(sys_time_out(res, "pool3", 3), 0, NULL, SYS_FAIL(-1338));
    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 8, &sys_state.block), NULL,
                    SYS_FAIL(-1339));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name4, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                NULL),
                    NULL, SYS_FAIL(-1340));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name5, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)1,
                                                NULL),
                    NULL, SYS_FAIL(-1341));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name6, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)2,
                                                NULL),
                    NULL, SYS_FAIL(-1342));
    PJ_TEST_EQ(wait_sys_count(3), 0, NULL, SYS_FAIL(-1343));
    pj_thread_sleep(200);
    PJ_TEST_EQ(sys_state.count, 3, NULL, SYS_FAIL(-1344));
    pj_sem_post(sys_state.block);
    PJ_TEST_EQ(wait_sys_count(4), 0, NULL, SYS_FAIL(-1345));
    pj_sem_post(sys_state.block);
    pj_sem_post(sys_state.block);
    for (i = 0; i < 3; ++i) {
        PJ_TEST_EQ(wait_sys_cb(i), 0, NULL, SYS_FAIL(-1346));
        PJ_TEST_SUCCESS(sys_cb[i].status, NULL, SYS_FAIL(-1347));
    }
    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;
    pj_sem_destroy(sys_state.block);
    sys_state.block = NULL;

    /* Caching off: looked up again; so is a name the system resolver
     * failed
     */
    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1348));
    pj_dns_resolver_get_settings(res, &lset);
    lset.cache_max_ttl = 0;
    pj_dns_resolver_set_settings(res, &lset);
    PJ_TEST_EQ(sys_time_out(res, "pool3", 3), 0, NULL, SYS_FAIL(-1349));
    for (i = 0; i < 2; ++i) {
        sys_cb[0].called = 0;
        PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name7,
                                                    PJ_DNS_TYPE_A, 0,
                                                    &dns_callback_sys,
                                                    (void*)0, NULL),
                        NULL, SYS_FAIL(-1350));
        PJ_TEST_EQ(wait_sys_cb(0), 0, NULL, SYS_FAIL(-1351));
        PJ_TEST_SUCCESS(sys_cb[0].status, NULL, SYS_FAIL(-1352));
    }
    PJ_TEST_EQ(sys_state.count, 3, NULL, SYS_FAIL(-1353));
    lset.cache_max_ttl = PJ_DNS_RESOLVER_MAX_TTL;
    pj_dns_resolver_set_settings(res, &lset);
    sys_state.status = PJ_ERESOLVE;
    for (i = 0; i < 2; ++i) {
        sys_cb[0].called = 0;
        PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name8,
                                                    PJ_DNS_TYPE_A, 0,
                                                    &dns_callback_sys,
                                                    (void*)0, NULL),
                        NULL, SYS_FAIL(-1354));
        PJ_TEST_EQ(wait_sys_cb(0), 0, NULL, SYS_FAIL(-1355));
        PJ_TEST_EQ(sys_cb[0].status, PJLIB_UTIL_EDNSNOWORKINGNS, NULL,
                   SYS_FAIL(-1356));
    }
    PJ_TEST_EQ(sys_state.count, 5, NULL, SYS_FAIL(-1357));
    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;


on_return:
    sys_cleanup(&res);
    return rc;
}

/* A pool which can't grow: the timer heap on it can't either, past its
 * first entries.
 */
static void nogrow_cb(pj_pool_t *p, pj_size_t size)
{
    PJ_UNUSED_ARG(p);
    PJ_UNUSED_ARG(size);
}

static void dummy_timer_cb(pj_timer_heap_t *th, pj_timer_entry *e)
{
    PJ_UNUSED_ARG(th);
    PJ_UNUSED_ARG(e);
}

/* Poll the timer heap of the test until the callback */
static int sys_poll_cb(pj_timer_heap_t *th, unsigned i)
{
    unsigned n;

    for (n = 0; !sys_cb[i].called && n < 500; ++n) {
        pj_timer_heap_poll(th, NULL);
        pj_thread_sleep(10);
    }
    return sys_cb[i].called ? 0 : -1;
}

/* When the report of a query can't be scheduled, out of memory, it is
 * reported all the same: from the lookup thread, or as a failure before
 * pj_dns_resolver_start_query() returns.
 */
static int dns_sys_fallback_schedule_test(void)
{
    static pj_timer_entry dummy[128];
    pj_str_t name0 = pj_str("sched0");
    pj_str_t name1 = pj_str("sched1");
    pj_str_t name2 = pj_str("sched2");
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    pj_time_val delay = {60, 0};
    pj_pool_t *nogrow = NULL;
    pj_timer_heap_t *th = NULL;
    pj_dns_resolver *res = NULL;
    pj_dns_settings lset;
    pj_dns_async_query *q;
    unsigned i, filled = 0;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, timer heap full test"));

    sys_reset();
    nogrow = pj_pool_create(mem, "nogrow", 4000, 0, &nogrow_cb);
    PJ_TEST_NOT_NULL(nogrow, NULL, SYS_FAIL(-1360));
    PJ_TEST_SUCCESS(pj_timer_heap_create(nogrow, 4, &th), NULL,
                    SYS_FAIL(-1361));
    PJ_TEST_SUCCESS(pj_dns_resolver_create(mem, NULL, 0, th, ioqueue, &res),
                    NULL, SYS_FAIL(-1362));
    pj_dns_resolver_get_settings(res, &lset);
    lset.qretr_delay = 100;
    lset.qretr_count = 2;
    lset.sys_fallback = PJ_TRUE;
    lset.sys_lookup = &test_sys_lookup;
    pj_dns_resolver_set_settings(res, &lset);
    sys_state.qretr_delay = 100;
    nameservers[0] = nameservers[1] = pj_str("127.0.0.1");
    ports[0] = g_server[0].port;
    ports[1] = g_server[1].port;
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(res, 2, nameservers, ports), NULL,
                    SYS_FAIL(-1363));

    /* Timed out on this heap, reported from it */
    g_server[0].action = g_server[1].action = ACTION_IGNORE;
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name0, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                NULL),
                    NULL, SYS_FAIL(-1364));
    PJ_TEST_EQ(sys_poll_cb(th, 0), 0, NULL, SYS_FAIL(-1365));
    PJ_TEST_SUCCESS(sys_cb[0].status, NULL, SYS_FAIL(-1366));
    sys_wait_probing();
    PJ_TEST_TRUE(sys_none_works(res), NULL, SYS_FAIL(-1367));

    /* A lookup in progress, then the heap is filled up */
    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 8, &sys_state.block), NULL,
                    SYS_FAIL(-1368));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)1,
                                                NULL),
                    NULL, SYS_FAIL(-1369));
    PJ_TEST_EQ(wait_sys_count(2), 0, NULL, SYS_FAIL(-1370));
    for (filled = 0; filled < PJ_ARRAY_SIZE(dummy); ++filled) {
        pj_timer_entry_init(&dummy[filled], 0, NULL, &dummy_timer_cb);
        if (pj_timer_heap_schedule(th, &dummy[filled], &delay) != PJ_SUCCESS)
            break;
    }
    PJ_TEST_TRUE(filled < PJ_ARRAY_SIZE(dummy), NULL, SYS_FAIL(-1371));

    /* A query to fail later can't be: failed now */
    PJ_TEST_EQ(pj_dns_resolver_start_query(res, &name2, PJ_DNS_TYPE_SRV, 0,
                                           &dns_callback_sys, (void*)2, &q),
               PJLIB_UTIL_EDNSNOWORKINGNS, NULL, SYS_FAIL(-1372));
    PJ_TEST_EQ(q, NULL, NULL, SYS_FAIL(-1373));
    PJ_TEST_EQ(sys_cb[2].called, 0, NULL, SYS_FAIL(-1374));

    /* The lookup is reported from its thread, the heap being full */
    pj_sem_post(sys_state.block);
    PJ_TEST_EQ(wait_sys_cb(1), 0, NULL, SYS_FAIL(-1375));
    PJ_TEST_SUCCESS(sys_cb[1].status, NULL, SYS_FAIL(-1376));
    PJ_TEST_EQ(sys_cb[1].addr, SYS_ADDR, NULL, SYS_FAIL(-1377));

on_return:
    for (i = 0; i < filled; ++i)
        pj_timer_heap_cancel(th, &dummy[i]);
    sys_cleanup(&res);
    if (th)
        pj_timer_heap_destroy(th);
    if (nogrow)
        pj_pool_release(nogrow);
    return rc;
}

/* The nameservers set or reset after a query was sent are not trusted:
 * its timeout is resolved with the system resolver, and they are probed.
 */
static int dns_sys_fallback_stale_test(void)
{
    pj_str_t name1 = pj_str("stale1");
    pj_str_t name2 = pj_str("stale2");
    pj_str_t nameservers[2];
    pj_uint16_t ports[2];
    pj_dns_resolver *res = NULL;
    pj_dns_settings lset;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, stale query test"));

    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1140));
    /* Sent once: a retransmission would be after the reset */
    pj_dns_resolver_get_settings(res, &lset);
    lset.qretr_count = 1;
    pj_dns_resolver_set_settings(res, &lset);
    g_server[0].action = g_server[1].action = ACTION_IGNORE;

    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name1, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)0,
                                                NULL),
                    NULL, SYS_FAIL(-1141));
    PJ_TEST_SUCCESS(pj_dns_resolver_reset_ns_state(res), NULL, SYS_FAIL(-1142));
    PJ_TEST_EQ(wait_sys_cb(0), 0, NULL, SYS_FAIL(-1143));
    PJ_TEST_SUCCESS(sys_cb[0].status, NULL, SYS_FAIL(-1144));
    PJ_TEST_TRUE(!sys_none_works(res), NULL, SYS_FAIL(-1145));

    /* The same when they are set again */
    PJ_TEST_SUCCESS(pj_dns_resolver_reset_ns_state(res), NULL, SYS_FAIL(-1146));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name2, PJ_DNS_TYPE_A, 0,
                                                &dns_callback_sys, (void*)1,
                                                NULL),
                    NULL, SYS_FAIL(-1147));
    nameservers[0] = nameservers[1] = pj_str("127.0.0.1");
    ports[0] = g_server[0].port;
    ports[1] = g_server[1].port;
    PJ_TEST_SUCCESS(pj_dns_resolver_set_ns(res, 2, nameservers, ports),
                    NULL, SYS_FAIL(-1148));
    PJ_TEST_EQ(wait_sys_cb(1), 0, NULL, SYS_FAIL(-1149));
    PJ_TEST_SUCCESS(sys_cb[1].status, NULL, SYS_FAIL(-1150));
    PJ_TEST_TRUE(!sys_none_works(res), NULL, SYS_FAIL(-1151));

    pj_dns_resolver_destroy(res, PJ_FALSE);
    res = NULL;

on_return:
    sys_cleanup(&res);
    return rc;
}
static pj_sem_t *late_timer_sem;
static pj_sem_t *late_cb_sem;
static volatile pj_bool_t late_timer_entered;
static volatile pj_bool_t late_cb_entered;

/* Holds the thread polling the timer heap, so that nothing is reported */
static void late_timer_cb(pj_timer_heap_t *th, pj_timer_entry *e)
{
    PJ_UNUSED_ARG(th);
    PJ_UNUSED_ARG(e);
    late_timer_entered = PJ_TRUE;
    pj_sem_wait(late_timer_sem);
}

/* Blocks while reporting the first query */
static void dns_callback_late(void *user_data,
                              pj_status_t status,
                              pj_dns_parsed_packet *resp)
{
    unsigned i = (unsigned)(pj_ssize_t)user_data;

    PJ_UNUSED_ARG(resp);

    sys_cb[i].called++;
    sys_cb[i].status = status;
    if (i == 0) {
        late_cb_entered = PJ_TRUE;
        pj_sem_wait(late_cb_sem);
    }
}

/* The queries looked up but not reported yet when the resolver is
 * destroyed are reported as cancelled by the destroy, not afterwards.
 */
static int dns_sys_fallback_late_destroy_test(void)
{
    pj_str_t name_a = pj_str("late_a");
    pj_str_t name_b = pj_str("late_b");
    pj_str_t name_c = pj_str("late_c");
    static pj_timer_entry hold;
    pj_time_val delay = {0, 0};
    pj_dns_resolver *res = NULL;
    pj_status_t status_b, status_c;
    int called_b, called_c;
    unsigned i;
    int rc = 0;

    PJ_LOG(3,(THIS_FILE, "  system resolver fallback, late destroy test"));
    late_timer_sem = late_cb_sem = NULL;

    sys_reset();
    res = sys_resolver(PJ_TRUE, 100);
    PJ_TEST_NOT_NULL(res, NULL, SYS_FAIL(-1160));
    /* One lookup at a time: the test is about their order */
    sys_set_threads(res, 1);
    PJ_TEST_EQ(sys_time_out(res, "late0", 3), 0, NULL, SYS_FAIL(-1161));
    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 1, &late_timer_sem), NULL,
                    SYS_FAIL(-1162));
    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 1, &late_cb_sem), NULL,
                    SYS_FAIL(-1163));
    PJ_TEST_SUCCESS(pj_sem_create(pool, NULL, 0, 1, &sys_state.block), NULL,
                    SYS_FAIL(-1164));
    late_timer_entered = late_cb_entered = PJ_FALSE;

    /* Nothing is reported while the timer heap is held */
    pj_timer_entry_init(&hold, 0, NULL, &late_timer_cb);
    PJ_TEST_SUCCESS(pj_timer_heap_schedule(timer_heap, &hold, &delay), NULL,
                    SYS_FAIL(-1165));
    for (i = 0; !late_timer_entered && i < 300; ++i)
        pj_thread_sleep(10);
    PJ_TEST_TRUE(late_timer_entered, NULL, SYS_FAIL(-1166));

    /* The lookups go one by one: when the third one is being looked up,
     * the first two are waiting to be reported
     */
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name_a, PJ_DNS_TYPE_A,
                                                0, &dns_callback_late,
                                                (void*)0, NULL),
                    NULL, SYS_FAIL(-1167));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name_b, PJ_DNS_TYPE_A,
                                                0, &dns_callback_late,
                                                (void*)1, NULL),
                    NULL, SYS_FAIL(-1168));
    PJ_TEST_SUCCESS(pj_dns_resolver_start_query(res, &name_c, PJ_DNS_TYPE_A,
                                                0, &dns_callback_late,
                                                (void*)2, NULL),
                    NULL, SYS_FAIL(-1169));
    PJ_TEST_EQ(wait_sys_count(2), 0, NULL, SYS_FAIL(-1170));
    pj_sem_post(sys_state.block);
    PJ_TEST_EQ(wait_sys_count(3), 0, NULL, SYS_FAIL(-1171));
    pj_sem_post(sys_state.block);
    PJ_TEST_EQ(wait_sys_count(4), 0, NULL, SYS_FAIL(-1172));

    /* Reporting the first one blocks, the second one waits */
    pj_sem_post(late_timer_sem);
    for (i = 0; !late_cb_entered && i < 300; ++i)
        pj_thread_sleep(10);
    PJ_TEST_TRUE(late_cb_entered, NULL, SYS_FAIL(-1173));
    PJ_TEST_EQ(sys_cb[1].called, 0, NULL, SYS_FAIL(-1174));

    /* The destroy reports the second and the third as cancelled */
    pj_sem_post(sys_state.block);
    pj_dns_resolver_destroy(res, PJ_TRUE);
    res = NULL;
    called_b = sys_cb[1].called;
    status_b = sys_cb[1].status;
    called_c = sys_cb[2].called;
    status_c = sys_cb[2].status;

    /* Not again once the first one is reported */
    pj_sem_post(late_cb_sem);
    /* A late report would come within this */
    pj_thread_sleep(200);
    PJ_TEST_EQ(called_b, 1, NULL, SYS_FAIL(-1175));
    PJ_TEST_EQ(status_b, PJ_ECANCELLED, NULL, SYS_FAIL(-1176));
    PJ_TEST_EQ(called_c, 1, NULL, SYS_FAIL(-1177));
    PJ_TEST_EQ(status_c, PJ_ECANCELLED, NULL, SYS_FAIL(-1178));
    PJ_TEST_EQ(sys_cb[0].called, 1, NULL, SYS_FAIL(-1179));
    PJ_TEST_EQ(sys_cb[1].called + sys_cb[2].called, 2, NULL, SYS_FAIL(-1180));

    pj_sem_destroy(sys_state.block);
    sys_state.block = NULL;
    pj_sem_destroy(late_cb_sem);
    pj_sem_destroy(late_timer_sem);
    late_cb_sem = late_timer_sem = NULL;

on_return:
    /* The thread polling the timer heap is held by the timer, or by the
     * callback, until posted; failed, the semaphores are left to the pool
     */
    if (late_timer_sem &&
        pj_timer_heap_cancel_if_active(timer_heap, &hold, 0) == 0)
    {
        pj_sem_post(late_timer_sem);
    }
    if (late_cb_sem)
        pj_sem_post(late_cb_sem);
    sys_cleanup(&res);
    return rc;
}
#endif  /* PJ_HAS_THREADS */


int resolver_test(void)
{
    int rc;
    
    PJ_LOG(3,(THIS_FILE, "init"));
    rc = init(PJ_FALSE);
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "a_parser_test"));
    rc = a_parser_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "addr_parser_test"));
    rc = addr_parser_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "simple_test"));
    rc = simple_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_test"));
    rc = dns_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_cancel_in_callback_test"));
    rc = dns_cancel_in_callback_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_cancel_in_timeout_callback_test"));
    rc = dns_cancel_in_timeout_callback_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_cancel_child_in_callback_test"));
    rc = dns_cancel_child_in_callback_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "srv_resolver_test"));
    rc = srv_resolver_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "srv_resolver_fallback_test"));
    rc = srv_resolver_fallback_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "srv_resolver_many_test"));
    rc = srv_resolver_many_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_destroy_pending_test"));
    rc = dns_destroy_pending_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_set_ns_during_query_test"));
    rc = dns_set_ns_during_query_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_clear_cache_test"));
    rc = dns_clear_cache_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_cancel_unlocked_test"));
    rc = dns_cancel_unlocked_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_start_during_destroy_test"));
    rc = dns_start_during_destroy_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_reset_ns_state_test"));
    rc = dns_reset_ns_state_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_cancel_pending_test"));
    rc = dns_cancel_pending_test();
    if (rc != 0)
        goto on_error;

#if PJ_HAS_THREADS
    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_timeout_test"));
    rc = dns_sys_fallback_timeout_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_srv_test"));
    rc = dns_sys_fallback_srv_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_cache_test"));
    rc = dns_sys_fallback_cache_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_join_test"));
    rc = dns_sys_fallback_join_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_destroy_test"));
    rc = dns_sys_fallback_destroy_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_failure_test"));
    rc = dns_sys_fallback_failure_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_trusted_test"));
    rc = dns_sys_fallback_trusted_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_cancel_test"));
    rc = dns_sys_fallback_cancel_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_rcode_test"));
    rc = dns_sys_fallback_rcode_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_pool_test"));
    rc = dns_sys_fallback_pool_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_schedule_test"));
    rc = dns_sys_fallback_schedule_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_stale_test"));
    rc = dns_sys_fallback_stale_test();
    if (rc != 0)
        goto on_error;

    PJ_LOG(3,(THIS_FILE, "dns_sys_fallback_late_destroy_test"));
    rc = dns_sys_fallback_late_destroy_test();
    if (rc != 0)
        goto on_error;
#endif

    destroy();


#if PJ_HAS_IPV6
    /* Similar tests using IPv6 socket and without parser tests */
    PJ_LOG(3,(THIS_FILE, "Re-run DNS resolution tests using IPv6 socket"));

    rc = init(PJ_TRUE);
    if (rc != 0)
        goto on_error;

    rc = simple_test();
    if (rc != 0)
        goto on_error;

    rc = dns_test();
    if (rc != 0)
        goto on_error;

    rc = srv_resolver_test();
    if (rc != 0)
        goto on_error;

    rc = srv_resolver_fallback_test();
    if (rc != 0)
        goto on_error;

    rc = srv_resolver_many_test();
    if (rc != 0)
        goto on_error;

    destroy();
#endif

    return 0;

on_error:
    destroy();
    return rc;
}

