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
#include <pjlib-util.h>
//#include <sip_uri.h>

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjsip.h>
#include <pjsip/sip_types.h>

#include <pjsip.h>
#include <pjlib.h>


#define kMinInputLength 10
#define kMaxInputLength 1024

/* Defined in sip_parser.c */
void init_sip_parser(void);
void deinit_sip_parser(void);

pj_pool_factory *mem;

int uri_parse(uint8_t *data, size_t Size) {

    pj_status_t status = 0 ;
    pj_pool_t *pool;
    pjsip_uri *uri;

    pool = pj_pool_create(mem, "uri", 1000, 1000, NULL);

    uri = pjsip_parse_uri(pool, (char *)data, Size, 0);

    if (!uri) {
        status = 1;
    }

    pj_pool_release(pool);

    return status;
}

extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    int ret = 0;
    uint8_t *data;
    pj_caching_pool caching_pool;

    /* Add NULL byte */
    data = (uint8_t *)calloc((Size+1), sizeof(uint8_t));
    memcpy((void *)data, (void *)Data, Size);

    /* init Calls */
    pj_init();
    pj_caching_pool_init( &caching_pool, &pj_pool_factory_default_policy, 0);
    pj_log_set_level(0);

    mem = &caching_pool.factory;

    init_sip_parser();

    /* Call fuzzer */
    ret = uri_parse(data, Size);

    free(data);
    deinit_sip_parser();
    pj_caching_pool_destroy(&caching_pool);

    return ret;
}
