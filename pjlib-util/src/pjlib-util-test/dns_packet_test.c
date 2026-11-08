/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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

#define THIS_FILE "dns_packet_test.c"

struct dns_case
{
    const char *name;
    const pj_uint8_t *packet;
    unsigned size;
    pj_status_t status;
};

static const pj_uint8_t valid_root_query[] = {
    0x00,0x01, 0x01,0x00, 0x00,0x01, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00, 0x00,0x01, 0x00,0x01
};
static const pj_uint8_t valid_compressed_a[] = {
    0x00,0x02, 0x81,0x80, 0x00,0x01, 0x00,0x01,
    0x00,0x00, 0x00,0x00, 0x03,'s','i','p', 0x07,'e','x','a','m','p','l','e',
    0x03,'c','o','m',0x00, 0x00,0x01,0x00,0x01,
    0xc0,0x0c, 0x00,0x01,0x00,0x01, 0x00,0x00,0x00,0x3c,
    0x00,0x04, 0xc0,0x00,0x02,0x01
};
static const pj_uint8_t valid_pointer_chain[] = {
    0,3, 0,0, 0,1, 0,0, 0,0, 0,0,
    0xc0,0x12, 0,1,0,1,
    0xc0,0x14, 0xc0,0x16, 0xc0,0x18, 0xc0,0x1a, 1,'x',0
};
static const pj_uint8_t excessive_pointer_chain[] = {
    0,4, 0,0, 0,1, 0,0, 0,0, 0,0,
    0xc0,0x12, 0,1,0,1,
    0xc0,0x14, 0xc0,0x16, 0xc0,0x18, 0xc0,0x1a, 0xc0,0x1c, 1,'x',0
};
static const pj_uint8_t valid_all_records[] = {
    /* Header and A question */
    0x42,0x42,0x85,0x80,0,1,0,6,0,1,0,1,
    3,'w','w','w',7,'e','x','a','m','p','l','e',3,'c','o','m',0, 0,1,0,1,
    /* CNAME answer */
    0xc0,0x0c, 0,5,0,1, 0,0,0,60, 0,8,
    5,'a','l','i','a','s',0xc0,0x10,
    /* A answer */
    0xc0,0x2d, 0,1,0,1, 0,0,0,60, 0,4, 192,0,2,1,
    /* AAAA answer */
    0xc0,0x2d, 0,28,0,1, 0,0,0,60, 0,16,
    0x20,0x01,0x0d,0xb8,0,0,0,0,0,0,0,0,0,0,0,1,
    /* PTR answer */
    1,'1',1,'2',1,'0',3,'1','9','2',7,'i','n','-','a','d','d','r',
    4,'a','r','p','a',0, 0,12,0,1, 0,0,0,60, 0,6,
    3,'p','t','r',0xc0,0x10,
    /* SRV answer */
    4,'_','s','i','p',4,'_','t','c','p',0xc0,0x10,
    0,33,0,1, 0,0,0,60, 0,12,
    0,10,0,20,0x13,0xc4, 3,'s','i','p',0xc0,0x10,
    /* Unknown answer */
    0, 0xfd,0xe8,0,3, 0,0,0,0, 0,2,0xde,0xad,
    /* NS authority record */
    0xc0,0x10, 0,2,0,1, 0,0,1,0x2c, 0,6,
    3,'n','s','1',0xc0,0x10,
    /* Additional A record */
    0xc0,0xc4, 0,1,0,1, 0,0,1,0x2c, 0,4, 198,51,100,53
};
static const pj_uint8_t short_header[] = { 0x00 };
static const pj_uint8_t missing_question[] = {
    0,1, 0,0, 0,1, 0,0, 0,0, 0,0
};
static const pj_uint8_t truncated_question_fields[] = {
    0,2, 0,0, 0,1, 0,0, 0,0, 0,0, 0, 0,1
};
static const pj_uint8_t incomplete_pointer[] = {
    0,3, 0,0, 0,1, 0,0, 0,0, 0,0, 0xc0
};
static const pj_uint8_t invalid_pointer[] = {
    0,4, 0,0, 0,1, 0,0, 0,0, 0,0, 0xc0,0xff, 0,1,0,1
};
static const pj_uint8_t pointer_cycle[] = {
    0,5, 0,0, 0,1, 0,0, 0,0, 0,0, 0xc0,0x0c, 0,1,0,1
};
static const pj_uint8_t short_a_rdata[] = {
    0,6, 0x81,0x80, 0,0, 0,1, 0,0, 0,0,
    0, 0,1, 0,1, 0,0,0,0, 0,3, 'a','b','c'
};
static const pj_uint8_t short_aaaa_rdata[] = {
    0,7, 0x81,0x80, 0,0, 0,1, 0,0, 0,0,
    0, 0,28, 0,1, 0,0,0,0, 0,15,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
static const pj_uint8_t short_srv_rdata[] = {
    0,8, 0x81,0x80, 0,0, 0,1, 0,0, 0,0,
    0, 0,33, 0,1, 0,0,0,0, 0,5, 0,1,0,2,0
};
static const pj_uint8_t incomplete_cname_pointer[] = {
    0,9, 0x81,0x80, 0,0, 0,1, 0,0, 0,0,
    0, 0,5, 0,1, 0,0,0,0, 0,1, 0xc0
};
static const pj_uint8_t excessive_count[] = {
    0,10, 0,0, 0xff,0xff, 0,0, 0,0, 0,0
};

/* Each of these has enough packet bytes but an invalid local RDATA extent */
static const pj_uint8_t a_short_extent[] = {
    0,11, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0,1,0,1, 0,0,0,0, 0,3, 1,2,3,4
};
static const pj_uint8_t a_long_extent[] = {
    0,12, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0,1,0,1, 0,0,0,0, 0,5, 1,2,3,4,5
};
static const pj_uint8_t aaaa_short_extent[] = {
    0,13, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0,28,0,1, 0,0,0,0, 0,15,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
};
static const pj_uint8_t srv_short_extent[] = {
    0,14, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0,33,0,1, 0,0,0,0, 0,5, 0,1,0,2,0,3,0
};
static const pj_uint8_t cname_short_extent[] = {
    0,15, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0,5,0,1, 0,0,0,0, 0,1, 0xc0,0x0c
};
static const pj_uint8_t cname_long_extent[] = {
    0,16, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0,5,0,1, 0,0,0,0, 0,2, 0,0xaa
};
static const pj_uint8_t empty_a_rdata[] = {
    0,17, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0,1,0,1, 0,0,0,0, 0,0
};
static const pj_uint8_t srv_target_missing[] = {
    0,18, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0,33,0,1, 0,0,0,0, 0,6, 0,1,0,2,0,3
};
static const pj_uint8_t cname_target_outside_rdata[] = {
    0,19, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0,5,0,1, 0,0,0,0, 0,2, 0xc0,0x19, 1,'x',0
};
static const pj_uint8_t cname_origin_crosses_rdata[] = {
    0,20, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0,5,0,1, 0,0,0,0, 0,2, 3,'a','b','c',0
};
static const pj_uint8_t empty_unknown_rdata[] = {
    0,21, 0x81,0x80, 0,0,0,1,0,0,0,0,
    0, 0xfd,0xe8,0,3, 0,0,0,0, 0,0
};

static pj_status_t parse_packet(const pj_uint8_t *packet, unsigned size,
                                pj_dns_parsed_packet **parsed,
                                pj_pool_t **pool)
{
    pj_status_t status;

    *pool = pj_pool_create(mem, "dns-packet", 4096, 4096, NULL);
    if (!*pool)
        return PJ_ENOMEM;

    *parsed = NULL;
    status = pj_dns_parse_packet(*pool, packet, size, parsed);
    return status;
}

static int check_status_cases(void)
{
    static const struct dns_case cases[] = {
        {"valid root query", valid_root_query, sizeof(valid_root_query),
         PJ_SUCCESS},
        {"valid compressed A", valid_compressed_a, sizeof(valid_compressed_a),
         PJ_SUCCESS},
        {"valid pointer chain", valid_pointer_chain,
         sizeof(valid_pointer_chain), PJ_SUCCESS},
        {"excessive pointer chain", excessive_pointer_chain,
         sizeof(excessive_pointer_chain), PJLIB_UTIL_EDNSINNAMEPTR},
        {"short header", short_header, sizeof(short_header),
         PJLIB_UTIL_EDNSINSIZE},
        {"missing question", missing_question, sizeof(missing_question),
         PJLIB_UTIL_EDNSINNAMEPTR},
        {"truncated question fields", truncated_question_fields,
         sizeof(truncated_question_fields), PJLIB_UTIL_EDNSINSIZE},
        {"incomplete pointer", incomplete_pointer, sizeof(incomplete_pointer),
         PJLIB_UTIL_EDNSINNAMEPTR},
        {"invalid pointer", invalid_pointer, sizeof(invalid_pointer),
         PJLIB_UTIL_EDNSINNAMEPTR},
        {"pointer cycle", pointer_cycle, sizeof(pointer_cycle),
         PJLIB_UTIL_EDNSINNAMEPTR},
        {"short A RDATA", short_a_rdata, sizeof(short_a_rdata),
         PJLIB_UTIL_EDNSINSIZE},
        {"short AAAA RDATA", short_aaaa_rdata, sizeof(short_aaaa_rdata),
         PJLIB_UTIL_EDNSINSIZE},
        {"short SRV RDATA", short_srv_rdata, sizeof(short_srv_rdata),
         PJLIB_UTIL_EDNSINSIZE},
        {"incomplete CNAME pointer", incomplete_cname_pointer,
         sizeof(incomplete_cname_pointer), PJLIB_UTIL_EDNSINNAMEPTR},
        {"excessive section count", excessive_count, sizeof(excessive_count),
         PJ_ETOOMANY},
        {"A short local extent", a_short_extent, sizeof(a_short_extent),
         PJLIB_UTIL_EDNSINSIZE},
        {"A long local extent", a_long_extent, sizeof(a_long_extent),
         PJLIB_UTIL_EDNSINSIZE},
        {"AAAA short local extent", aaaa_short_extent,
         sizeof(aaaa_short_extent), PJLIB_UTIL_EDNSINSIZE},
        {"SRV short local extent", srv_short_extent,
         sizeof(srv_short_extent), PJLIB_UTIL_EDNSINSIZE},
        {"CNAME short local extent", cname_short_extent,
         sizeof(cname_short_extent), PJLIB_UTIL_EDNSINNAMEPTR},
        {"CNAME long local extent", cname_long_extent,
         sizeof(cname_long_extent), PJLIB_UTIL_EDNSINSIZE},
        {"empty A RDATA", empty_a_rdata, sizeof(empty_a_rdata),
         PJLIB_UTIL_EDNSINSIZE},
        {"SRV target missing", srv_target_missing,
         sizeof(srv_target_missing), PJLIB_UTIL_EDNSINNAMEPTR},
        {"CNAME target outside RDATA", cname_target_outside_rdata,
         sizeof(cname_target_outside_rdata), PJ_SUCCESS},
        {"CNAME origin crosses RDATA", cname_origin_crosses_rdata,
         sizeof(cname_origin_crosses_rdata), PJLIB_UTIL_EDNSINNAMEPTR},
        {"empty unknown RDATA", empty_unknown_rdata,
         sizeof(empty_unknown_rdata), PJ_SUCCESS}
    };
    unsigned i;
    int failed = 0;

    for (i = 0; i < PJ_ARRAY_SIZE(cases); ++i) {
        pj_dns_parsed_packet *parsed;
        pj_pool_t *pool;
        pj_status_t status;

        status = parse_packet(cases[i].packet, cases[i].size, &parsed, &pool);
        if (status != cases[i].status) {
            PJ_LOG(1,(THIS_FILE, "case '%s': expected %d, got %d",
                      cases[i].name, cases[i].status, status));
            ++failed;
        }
        if (pool)
            pj_pool_release(pool);
    }

    return failed;
}

static int check_valid_structures(void)
{
    static const pj_uint8_t ipv4[] = {192, 0, 2, 1};
    static const pj_uint8_t additional_ipv4[] = {198, 51, 100, 53};
    static const pj_uint8_t ipv6[] = {
        0x20,0x01,0x0d,0xb8,0,0,0,0,0,0,0,0,0,0,0,1
    };
    pj_dns_parsed_packet *pkt;
    pj_pool_t *pool;
    pj_status_t status;
    int failed = 0;

    status = parse_packet(valid_all_records, sizeof(valid_all_records),
                          &pkt, &pool);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "all-record packet failed: %d", status));
        if (pool)
            pj_pool_release(pool);
        return 1;
    }

    PJ_TEST_EQ(pkt->hdr.id, 0x4242, "header ID", ++failed);
    PJ_TEST_EQ(pkt->hdr.flags, 0x8580, "header flags", ++failed);
    PJ_TEST_EQ(pkt->hdr.qdcount, 1, "question count", ++failed);
    PJ_TEST_EQ(pkt->hdr.anscount, 6, "answer count", ++failed);
    PJ_TEST_EQ(pkt->hdr.nscount, 1, "authority count", ++failed);
    PJ_TEST_EQ(pkt->hdr.arcount, 1, "additional count", ++failed);

    PJ_TEST_EQ(pj_strcmp2(&pkt->q[0].name, "www.example.com"), 0,
               "question name", ++failed);
    PJ_TEST_EQ(pkt->q[0].type, PJ_DNS_TYPE_A, "question type", ++failed);
    PJ_TEST_EQ(pkt->q[0].dnsclass, 1, "question class", ++failed);

    PJ_TEST_EQ(pkt->ans[0].type, PJ_DNS_TYPE_CNAME,
               "CNAME type", ++failed);
    PJ_TEST_EQ(pkt->ans[0].dnsclass, 1, "CNAME class", ++failed);
    PJ_TEST_EQ(pkt->ans[0].ttl, 60, "CNAME TTL", ++failed);
    PJ_TEST_EQ(pkt->ans[0].rdlength, 8, "CNAME RDLENGTH", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->ans[0].name, "www.example.com"), 0,
               "CNAME owner", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->ans[0].rdata.cname.name,
                          "alias.example.com"), 0,
               "CNAME target", ++failed);

    PJ_TEST_EQ(pkt->ans[1].type, PJ_DNS_TYPE_A, "A type", ++failed);
    PJ_TEST_EQ(pkt->ans[1].dnsclass, 1, "A class", ++failed);
    PJ_TEST_EQ(pkt->ans[1].ttl, 60, "A TTL", ++failed);
    PJ_TEST_EQ(pkt->ans[1].rdlength, 4, "A RDLENGTH", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->ans[1].name, "alias.example.com"), 0,
               "A owner", ++failed);
    PJ_TEST_EQ(pj_memcmp(&pkt->ans[1].rdata.a.ip_addr, ipv4,
                         sizeof(ipv4)), 0, "A address", ++failed);

    PJ_TEST_EQ(pkt->ans[2].type, PJ_DNS_TYPE_AAAA,
               "AAAA type", ++failed);
    PJ_TEST_EQ(pkt->ans[2].dnsclass, 1, "AAAA class", ++failed);
    PJ_TEST_EQ(pkt->ans[2].ttl, 60, "AAAA TTL", ++failed);
    PJ_TEST_EQ(pkt->ans[2].rdlength, 16, "AAAA RDLENGTH", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->ans[2].name, "alias.example.com"), 0,
               "AAAA owner", ++failed);
    PJ_TEST_EQ(pj_memcmp(&pkt->ans[2].rdata.aaaa.ip_addr, ipv6,
                         sizeof(ipv6)), 0, "AAAA address", ++failed);

    PJ_TEST_EQ(pkt->ans[3].type, PJ_DNS_TYPE_PTR, "PTR type", ++failed);
    PJ_TEST_EQ(pkt->ans[3].dnsclass, 1, "PTR class", ++failed);
    PJ_TEST_EQ(pkt->ans[3].ttl, 60, "PTR TTL", ++failed);
    PJ_TEST_EQ(pkt->ans[3].rdlength, 6, "PTR RDLENGTH", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->ans[3].name,
                          "1.2.0.192.in-addr.arpa"), 0,
               "PTR owner", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->ans[3].rdata.ptr.name,
                          "ptr.example.com"), 0,
               "PTR target", ++failed);

    PJ_TEST_EQ(pkt->ans[4].type, PJ_DNS_TYPE_SRV, "SRV type", ++failed);
    PJ_TEST_EQ(pkt->ans[4].dnsclass, 1, "SRV class", ++failed);
    PJ_TEST_EQ(pkt->ans[4].ttl, 60, "SRV TTL", ++failed);
    PJ_TEST_EQ(pkt->ans[4].rdlength, 12, "SRV RDLENGTH", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->ans[4].name,
                          "_sip._tcp.example.com"), 0,
               "SRV owner", ++failed);
    PJ_TEST_EQ(pkt->ans[4].rdata.srv.prio, 10,
               "SRV priority", ++failed);
    PJ_TEST_EQ(pkt->ans[4].rdata.srv.weight, 20,
               "SRV weight", ++failed);
    PJ_TEST_EQ(pkt->ans[4].rdata.srv.port, 5060,
               "SRV port", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->ans[4].rdata.srv.target,
                          "sip.example.com"), 0,
               "SRV target", ++failed);

    PJ_TEST_EQ(pkt->ans[5].name.slen, 0, "unknown owner", ++failed);
    PJ_TEST_EQ(pkt->ans[5].type, 65000, "unknown type", ++failed);
    PJ_TEST_EQ(pkt->ans[5].dnsclass, 3, "unknown class", ++failed);
    PJ_TEST_EQ(pkt->ans[5].ttl, 0, "unknown TTL", ++failed);
    PJ_TEST_EQ(pkt->ans[5].rdlength, 2,
               "unknown RDLENGTH", ++failed);
    PJ_TEST_EQ(pj_memcmp(pkt->ans[5].data, "\xde\xad", 2), 0,
               "unknown RDATA", ++failed);

    PJ_TEST_EQ(pkt->ns[0].type, PJ_DNS_TYPE_NS, "NS type", ++failed);
    PJ_TEST_EQ(pkt->ns[0].dnsclass, 1, "NS class", ++failed);
    PJ_TEST_EQ(pkt->ns[0].ttl, 300, "NS TTL", ++failed);
    PJ_TEST_EQ(pkt->ns[0].rdlength, 6, "NS RDLENGTH", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->ns[0].name, "example.com"), 0,
               "NS owner", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->ns[0].rdata.ns.name,
                          "ns1.example.com"), 0,
               "NS target", ++failed);

    PJ_TEST_EQ(pkt->arr[0].type, PJ_DNS_TYPE_A,
               "additional A type", ++failed);
    PJ_TEST_EQ(pkt->arr[0].dnsclass, 1,
               "additional A class", ++failed);
    PJ_TEST_EQ(pkt->arr[0].ttl, 300, "additional A TTL", ++failed);
    PJ_TEST_EQ(pkt->arr[0].rdlength, 4,
               "additional A RDLENGTH", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&pkt->arr[0].name, "ns1.example.com"), 0,
               "additional A owner", ++failed);
    PJ_TEST_EQ(pj_memcmp(&pkt->arr[0].rdata.a.ip_addr, additional_ipv4,
                         sizeof(additional_ipv4)), 0,
               "additional A address", ++failed);

    pj_pool_release(pool);
    return failed;
}

static int check_name_boundaries(void)
{
    static const pj_uint8_t invalid_label_lengths[] = {64, 127, 128, 191};
    pj_uint8_t packet[280];
    char literal_name[254];
    pj_dns_parsed_packet *parsed;
    pj_pool_t *pool;
    pj_status_t status;
    unsigned last_label;
    unsigned name_root_pos;
    unsigned root_pos;
    unsigned pos;
    unsigned i;
    int failed = 0;

    pj_bzero(packet, sizeof(packet));
    pj_bzero(literal_name, sizeof(literal_name));
    packet[1] = 8;
    packet[2] = 1;
    packet[5] = 2;
    pos = 12;
    root_pos = pos;
    packet[pos++] = 0;
    packet[pos++] = 0;
    packet[pos++] = PJ_DNS_TYPE_A;
    packet[pos++] = 0;
    packet[pos++] = 1;
    packet[pos++] = 63;
    pj_memset(packet + pos, 'a', 63);
    pos += 63;
    packet[pos++] = 63;
    pj_memset(packet + pos, 'b', 63);
    pos += 63;
    packet[pos++] = 63;
    pj_memset(packet + pos, 'c', 63);
    pos += 63;
    last_label = pos;
    packet[pos++] = 61;
    pj_memset(packet + pos, 'd', 61);
    pos += 61;
    name_root_pos = pos;
    packet[pos++] = 0;
    packet[pos++] = 0;
    packet[pos++] = PJ_DNS_TYPE_AAAA;
    packet[pos++] = 0;
    packet[pos++] = 1;

    status = parse_packet(packet, pos, &parsed, &pool);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "maximum valid name failed (status=%d)", status));
        ++failed;
    } else if (parsed->q[0].name.slen != 0 ||
               parsed->q[0].name.ptr[0] != '\0')
    {
        PJ_LOG(1,(THIS_FILE, "root name termination mismatch"));
        ++failed;
    } else if (parsed->q[1].name.slen != 253 ||
               parsed->q[1].name.ptr[63] != '.' ||
               parsed->q[1].name.ptr[127] != '.' ||
               parsed->q[1].name.ptr[191] != '.' ||
               parsed->q[1].name.ptr[253] != '\0')
    {
        PJ_LOG(1,(THIS_FILE, "maximum valid name mismatch"));
        ++failed;
    } else {
        pj_memcpy(literal_name, parsed->q[1].name.ptr,
                  parsed->q[1].name.slen);
    }
    if (pool)
        pj_pool_release(pool);

    /* Replace the literal root with a pointer to the earlier root question */
    pj_memmove(packet + name_root_pos + 2, packet + name_root_pos + 1,
               pos - name_root_pos - 1);
    packet[name_root_pos] = 0xc0;
    packet[name_root_pos + 1] = (pj_uint8_t)root_pos;
    ++pos;

    status = parse_packet(packet, pos, &parsed, &pool);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "compressed maximum name failed (status=%d)",
                  status));
        ++failed;
    } else if (parsed->q[1].name.slen != 253 ||
               pj_memcmp(parsed->q[1].name.ptr, literal_name, 253) != 0)
    {
        PJ_LOG(1,(THIS_FILE, "compressed maximum name mismatch"));
        ++failed;
    }
    if (pool)
        pj_pool_release(pool);

    pj_memmove(packet + name_root_pos + 1, packet + name_root_pos,
               pos - name_root_pos);
    packet[name_root_pos] = 'd';
    packet[last_label] = 62;
    ++pos;

    status = parse_packet(packet, pos, &parsed, &pool);
    if (status != PJ_ENAMETOOLONG) {
        PJ_LOG(1,(THIS_FILE, "overlong name: expected %d, got %d",
                  PJ_ENAMETOOLONG, status));
        ++failed;
    }
    if (pool)
        pj_pool_release(pool);

    for (i = 0; i < PJ_ARRAY_SIZE(invalid_label_lengths); ++i) {
        pj_uint8_t label_len = invalid_label_lengths[i];

        pj_bzero(packet, sizeof(packet));
        packet[5] = 1;
        pos = sizeof(pj_dns_hdr);
        packet[pos++] = label_len;
        pj_memset(packet + pos, 'x', label_len);
        pos += label_len;
        packet[pos++] = 0;
        packet[pos++] = 0;
        packet[pos++] = PJ_DNS_TYPE_A;
        packet[pos++] = 0;
        packet[pos++] = 1;

        status = parse_packet(packet, pos, &parsed, &pool);
        if (status != PJLIB_UTIL_EDNSINNAMEPTR) {
            PJ_LOG(1,(THIS_FILE,
                      "label length %u: expected %d, got %d",
                      label_len, PJLIB_UTIL_EDNSINNAMEPTR, status));
            ++failed;
        }
        if (pool)
            pj_pool_release(pool);
    }

    return failed;
}

static int check_rdata_compression_target(void)
{
    pj_dns_parsed_packet *parsed;
    pj_pool_t *pool;
    pj_status_t status;
    int failed = 0;

    status = parse_packet(cname_target_outside_rdata,
                          sizeof(cname_target_outside_rdata),
                          &parsed, &pool);
    if (status != PJ_SUCCESS) {
        PJ_LOG(1,(THIS_FILE, "external compression target failed: %d",
                  status));
        if (pool)
            pj_pool_release(pool);
        return 1;
    }

    PJ_TEST_EQ(parsed->hdr.anscount, 1,
               "external target answer count", ++failed);
    PJ_TEST_EQ(parsed->ans[0].rdlength, 2,
               "external target origin length", ++failed);
    PJ_TEST_EQ(pj_strcmp2(&parsed->ans[0].rdata.cname.name, "x"), 0,
               "external compression target name", ++failed);

    pj_pool_release(pool);
    return failed;
}

static int check_truncation_points(const char *name,
                                   const pj_uint8_t *packet,
                                   unsigned packet_size)
{
    unsigned size;
    int failed = 0;

    for (size = sizeof(pj_dns_hdr); size < packet_size; ++size) {
        pj_dns_parsed_packet *parsed;
        pj_pool_t *pool;
        pj_status_t status;

        status = parse_packet(packet, size, &parsed, &pool);
        if (status == PJ_SUCCESS) {
            PJ_LOG(1,(THIS_FILE, "%s truncation at %u accepted", name,
                      size));
            ++failed;
        }
        if (pool)
            pj_pool_release(pool);
    }
    return failed;
}

static int check_truncations(void)
{
    int failed = 0;

    failed += check_truncation_points("compressed A", valid_compressed_a,
                                      sizeof(valid_compressed_a));
    failed += check_truncation_points("all records", valid_all_records,
                                      sizeof(valid_all_records));
    return failed;
}

int dns_packet_test(void)
{
    int failed = 0;

    failed += check_status_cases();
    failed += check_valid_structures();
    failed += check_name_boundaries();
    failed += check_rdata_compression_target();
    failed += check_truncations();

    return failed ? -failed : 0;
}
