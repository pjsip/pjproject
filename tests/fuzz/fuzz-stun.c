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

#define kMinInputLength 20
#define kMaxInputLength 5120

/* Fixed credentials for consistent authentication testing */
#define FIXED_USERNAME "testuser"
#define FIXED_PASSWORD "testpass123"
#define FIXED_REALM "testrealm.org"
#define FIXED_NONCE "1234567890abcdef"

/* Alternative credentials for testing mismatches */
#define ALT_USERNAME "altuser"
#define ALT_PASSWORD "altpass456"

pj_pool_factory *mem;

int stun_parse(uint8_t *data, size_t Size)
{
    pj_status_t status;
    pj_pool_t *pool;
    pj_stun_msg *msg = NULL;
    pj_stun_msg *response = NULL;
    pj_stun_auth_cred cred;
    pj_str_t USERNAME, PASSWORD, REALM, NONCE;
    unsigned char *encoded_buf;
    pj_size_t encoded_len;

    if (Size < 20)
        return 0;

    /* Use fixed credentials based on control byte */
    /* Bit 0: Use alternative username (for testing mismatch) */
    /* Bit 1: Use alternative password (for testing auth failure) */
    if (data[0] & 0x01) {
        USERNAME = pj_str(ALT_USERNAME);
    } else {
        USERNAME = pj_str(FIXED_USERNAME);
    }

    if (data[0] & 0x02) {
        PASSWORD = pj_str(ALT_PASSWORD);
    } else {
        PASSWORD = pj_str(FIXED_PASSWORD);
    }

    REALM = pj_str(FIXED_REALM);
    NONCE = pj_str(FIXED_NONCE);

    pool = pj_pool_create(mem, "stun_fuzz", 4096, 4096, NULL);
    encoded_buf = pj_pool_alloc(pool, kMaxInputLength);

    /* Path 1: Decode existing STUN message */
    status = pj_stun_msg_decode(pool, data, Size,
                                 PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET,
                                 &msg, NULL, NULL);

    /* Path 2: Authenticate (always execute, even if decode failed) */
    pj_bzero(&cred, sizeof(cred));
    cred.type = PJ_STUN_AUTH_CRED_STATIC;
    cred.data.static_cred.username = USERNAME;
    cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
    cred.data.static_cred.data = PASSWORD;
    cred.data.static_cred.realm = REALM;
    cred.data.static_cred.nonce = NONCE;

    if (msg) {
        pj_stun_authenticate_request(data, (unsigned)Size,
                                      msg, &cred, pool, NULL, NULL);
    }

    /* Path 3: Create new STUN messages (different types based on input) */
    {
        int method = (data[1] % 4) ? PJ_STUN_BINDING_METHOD :
                     ((data[2] % 2) ? PJ_STUN_ALLOCATE_METHOD : PJ_STUN_REFRESH_METHOD);

        pj_stun_msg_create(pool, method, PJ_STUN_MAGIC, NULL, &response);
    }

    /* Path 4: Add various attributes to message */
    if (response) {
        pj_sockaddr_in addr;
        pj_str_t software = pj_str("FuzzTest");

        /* Add USERNAME attribute */
        pj_stun_msg_add_string_attr(pool, response, PJ_STUN_ATTR_USERNAME, &USERNAME);

        /* Add REALM attribute */
        pj_stun_msg_add_string_attr(pool, response, PJ_STUN_ATTR_REALM, &REALM);

        /* Add NONCE attribute */
        pj_stun_msg_add_string_attr(pool, response, PJ_STUN_ATTR_NONCE, &NONCE);

        /* Add SOFTWARE attribute */
        pj_stun_msg_add_string_attr(pool, response, PJ_STUN_ATTR_SOFTWARE, &software);

        /* Add MAPPED-ADDRESS attribute */
        pj_bzero(&addr, sizeof(addr));
        addr.sin_family = pj_AF_INET();
        if (Size >= 10) {
            pj_memcpy(&addr.sin_addr, data + 4, 4);
            pj_memcpy(&addr.sin_port, data + 8, 2);
        }
        pj_stun_msg_add_sockaddr_attr(pool, response, PJ_STUN_ATTR_MAPPED_ADDR,
                                       PJ_TRUE, (pj_sockaddr_t*)&addr, sizeof(addr));

        /* Add XOR-MAPPED-ADDRESS */
        pj_stun_msg_add_sockaddr_attr(pool, response, PJ_STUN_ATTR_XOR_MAPPED_ADDR,
                                       PJ_TRUE, (pj_sockaddr_t*)&addr, sizeof(addr));

        /* Add ERROR-CODE attribute */
        if (data[3] % 2) {
            int err_code = (data[10] % 200) + 400;
            pj_str_t err_reason = pj_str("Test Error");
            pj_stun_msg_add_errcode_attr(pool, response, err_code, &err_reason);
        }

        /* Add UNKNOWN-ATTRIBUTES if decode found unknown attrs */
        if (msg && data[11] % 2) {
            pj_uint16_t unknown_attrs[4] = {0x8001, 0x8002, 0x8003, 0x8004};
            pj_stun_msg_add_unknown_attr(pool, response,
                                          (data[12] % 4) + 1, unknown_attrs);
        }

        /* Path 5: Encode the message */
        status = pj_stun_msg_encode(response, encoded_buf, kMaxInputLength,
                                     0, NULL, &encoded_len);

        /* Path 6: Add message integrity (if encoding succeeded) */
        if (status == PJ_SUCCESS && encoded_len > 0) {
            pj_stun_msg_add_msgint_attr(pool, response);
        }

        /* Path 7: Attribute lookup and manipulation */
        pj_stun_msg_find_attr(response, PJ_STUN_ATTR_USERNAME, 0);
        pj_stun_msg_find_attr(response, PJ_STUN_ATTR_REALM, 0);
        pj_stun_msg_find_attr(response, PJ_STUN_ATTR_MESSAGE_INTEGRITY, 0);
    }

    /* Path 8: Create error response (only if msg is a request) */
    if (msg && PJ_STUN_IS_REQUEST(msg->hdr.type)) {
        pj_str_t err_msg = pj_str("Unauthorized");
        pj_stun_msg_create_response(pool, msg, (data[13] % 200) + 400,
                                     &err_msg, &response);
    }

    pj_pool_release(pool);
    return 0;
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
