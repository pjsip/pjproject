/* 
 * Copyright (C) 2023 Teluu Inc. (http://www.teluu.com)
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pjlib.h>

#include <pjlib-util/dns.h>

#define kMinInputLength 10
#define kMaxInputLength 5120

pj_pool_factory *mem;

/* Exercise pj_dns_make_query */
static void dns_make_query_fuzz(const char *data, size_t size)
{
    static const pj_uint16_t qtypes[] = {
        PJ_DNS_TYPE_A, PJ_DNS_TYPE_CNAME, PJ_DNS_TYPE_NS,
        PJ_DNS_TYPE_PTR, PJ_DNS_TYPE_SRV, PJ_DNS_TYPE_AAAA
    };
    pj_pool_t *pool;
    pj_dns_parsed_packet *pkt;
    pj_str_t name;
    char qbuf[512];
    unsigned qbuf_size;
    int qtype;

    pool = pj_pool_create(mem, "dns_mkq", 2000, 2000, NULL);
    if (!pool) {
        return;
    }

    name.ptr = (char *)data;
    name.slen = (pj_ssize_t)(size > 63 ? 63 : size);
    qtype = (int)qtypes[(unsigned char)data[0] % 6];
    qbuf_size = sizeof(qbuf);

    if (pj_dns_make_query(qbuf, &qbuf_size, 0x1234, qtype, &name) == PJ_SUCCESS) {
        pkt = NULL;
        pj_dns_parse_packet(pool, qbuf, qbuf_size, &pkt);
    }

    pj_pool_release(pool);
}

int dns_parser(char *data, size_t size)
{

    int ret = 0;
	pj_pool_t *pool;
	pj_status_t status;
    pj_dns_parsed_packet *dns;

    pool = pj_pool_create(mem, "dns_test", 4000, 4000, NULL);

    status = pj_dns_parse_packet(pool, data, size, &dns);
    if (status != PJ_SUCCESS)
        ret = 1;

    pj_pool_release(pool);

    return ret;
}

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    int ret = 0;
    char *data;
    pj_caching_pool caching_pool;

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    /* Add null termination for the data */
    data = (char *)calloc((Size+1), sizeof(char));
    memcpy((void *)data, (void *)Data, Size);

    /* Init */
    pj_init();
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    pj_log_set_level(0);

    mem = &caching_pool.factory;

    /* Fuzz */
    ret = dns_parser(data, Size);
    dns_make_query_fuzz(data, Size);

    free(data);
    pj_caching_pool_destroy(&caching_pool);

    return ret;
}
