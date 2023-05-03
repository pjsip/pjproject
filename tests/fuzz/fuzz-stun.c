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

#include <pjnath.h>

#define kMinInputLength 10
#define kMaxInputLength 5120

pj_pool_factory *mem;

int stun_parse(uint8_t *data,size_t Size) {

    pj_status_t status;
    pj_pool_t *pool;
    pj_stun_msg *msg;
    pj_stun_auth_cred cred;

    const pj_str_t USERNAME = {"A", 1};
    const pj_str_t PASSWORD = {"A", 1};

    pool = pj_pool_create(mem, "decode_test", 1024, 1024, NULL);

    status = pj_stun_msg_decode(pool, data, Size, PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET, &msg, NULL, NULL);
    if(status != PJ_SUCCESS){
        goto ret_error;
    }

    pj_bzero(&cred, sizeof(cred));
    cred.type = PJ_STUN_AUTH_CRED_STATIC;
    cred.data.static_cred.username = USERNAME;
    cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
    cred.data.static_cred.data = PASSWORD;

    pj_stun_authenticate_request(data, (unsigned)Size, msg, &cred, pool, NULL, NULL);

    pj_pool_release(pool);
    return status;

ret_error:
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

    /* Call fuzzer */
    ret = stun_parse(data, Size);

    free(data);
    pj_caching_pool_destroy(&caching_pool);

    return ret;
}
