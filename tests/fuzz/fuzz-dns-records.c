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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <pjlib.h>
#include <pjlib-util/dns.h>
#include <pjlib-util/resolver.h>

#define POOL_SIZE 4000

/* Global state for one-time initialization */
static pj_caching_pool caching_pool;
static pj_pool_factory *mem;

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    static int initialized = 0;
    pj_pool_t *pool;
    pj_status_t status;
    pj_dns_parsed_packet *dns;

    /* One-time initialization */
    if (!initialized) {
        pj_init();
        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
        pj_log_set_level(0);
        mem = &caching_pool.factory;
        initialized = 1;
    }

    /* Skip tiny inputs */
    if (Size < 12)
        return 0;

    /* Create pool for this iteration */
    pool = pj_pool_create(mem, "dns", POOL_SIZE, POOL_SIZE, NULL);
    if (!pool)
        return 0;

    /* Test DNS packet parsing */
    status = pj_dns_parse_packet(pool, (void*)Data, Size, &dns);
    
    if (status == PJ_SUCCESS && dns && dns->hdr.anscount > 0) {
        /* Test A record parsing */
        pj_dns_a_record a_rec;
        pj_dns_parse_a_response(dns, &a_rec);
        
        /* Test addr record parsing  */
        pj_dns_addr_record addr_rec;
        pj_dns_parse_addr_response(dns, &addr_rec);
    }

    /* Release pool */
    pj_pool_release(pool);

    return 0;
}
