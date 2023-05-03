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

#define kMinInputLength 10
#define kMaxInputLength 5120

pj_pool_factory *mem;

int Json_parse(uint8_t *data, size_t Size) {

    pj_pool_t *pool;
    pj_json_elem *elem;
    pj_json_err_info err;

    char *output;
    unsigned int output_size;

    pool = pj_pool_create(mem, "json", 1000, 1000, NULL);

    elem = pj_json_parse(pool, (char *)data, (unsigned *)&Size, &err);
    if (!elem) {
        goto on_error;
    }

    output_size = Size * 2;
    output = pj_pool_alloc(pool, output_size);

    if (pj_json_write(elem, output, &output_size)) {
        goto on_error;
    }

    pj_pool_release(pool);
    return 0;

on_error:
    pj_pool_release(pool);
    return 1;
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

    /* Call fuzzer */
    ret = Json_parse(data, Size);

    free(data);
    pj_caching_pool_destroy(&caching_pool);

    return ret;
}
