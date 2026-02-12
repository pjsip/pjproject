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

#define kMinInputLength 24
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

/* TURN session callback stub */
static pj_status_t turn_on_send_pkt(pj_turn_session *sess,
                             const pj_uint8_t *pkt,
                             unsigned pkt_len,
                             const pj_sockaddr_t *dst_addr,
                             unsigned dst_addr_len)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(pkt_len);
    PJ_UNUSED_ARG(dst_addr);
    PJ_UNUSED_ARG(dst_addr_len);
    return PJ_SUCCESS;
}

int stun_parse(uint8_t *data, size_t Size)
{
    pj_pool_t *pool;
    pj_stun_msg *msg = NULL;
    pj_stun_msg *response = NULL;
    pj_stun_auth_cred cred;
    pj_str_t username, password, realm, nonce;
    unsigned char *encoded_buf;
    pj_size_t encoded_len;

    /* Extract all flags at the beginning */
    uint8_t flag_cred_username = data[0] & 0x01;
    uint8_t flag_cred_password = data[0] & 0x02;
    uint8_t flag_msg_type = data[1];
    uint8_t flag_error_code = data[2] & 0x01;
    uint8_t flag_unknown_attrs = data[2] & 0x02;
    uint8_t flag_turn_enabled = data[3] & 0x01;

    /* Advance data pointer and adjust size */
    data += 4;
    Size -= 4;

    /* Use fixed credentials based on extracted flags */
    if (flag_cred_username) {
        username = pj_str(ALT_USERNAME);
    } else {
        username = pj_str(FIXED_USERNAME);
    }

    if (flag_cred_password) {
        password = pj_str(ALT_PASSWORD);
    } else {
        password = pj_str(FIXED_PASSWORD);
    }

    realm = pj_str(FIXED_REALM);
    nonce = pj_str(FIXED_NONCE);

    pool = pj_pool_create(mem, "stun_fuzz", 4096, 4096, NULL);
    encoded_buf = pj_pool_alloc(pool, kMaxInputLength);

    /* Path 1: Decode existing STUN message using full remaining data */
    pj_stun_msg_decode(pool, data, Size,
                       PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET,
                       &msg, NULL, NULL);

    /* Path 2: Authenticate (execute regardless of decode result for coverage) */
    pj_bzero(&cred, sizeof(cred));
    cred.type = PJ_STUN_AUTH_CRED_STATIC;
    cred.data.static_cred.username = username;
    cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
    cred.data.static_cred.data = password;
    cred.data.static_cred.realm = realm;
    cred.data.static_cred.nonce = nonce;

    if (msg) {
        pj_stun_authenticate_request(data, (unsigned)Size,
                                      msg, &cred, pool, NULL, NULL);
    }

    /* Path 3: Create new STUN messages (different types based on flag) */
    {
        int msg_type;
        int type_selector = flag_msg_type % 8;

        /* Select proper STUN message type (method + class) */
        if (type_selector < 2) {
            msg_type = PJ_STUN_BINDING_REQUEST;
        } else if (type_selector < 4) {
            msg_type = PJ_STUN_ALLOCATE_REQUEST;
        } else if (type_selector < 6) {
            msg_type = PJ_STUN_REFRESH_REQUEST;
        } else {
            msg_type = PJ_STUN_BINDING_INDICATION;
        }

        pj_stun_msg_create(pool, msg_type, PJ_STUN_MAGIC, NULL, &response);
    }

    /* Path 4: Add various attributes to message using full remaining data */
    if (response) {
        pj_sockaddr_in addr;
        pj_str_t software = pj_str("FuzzTest");

        /* Add USERNAME attribute */
        pj_stun_msg_add_string_attr(pool, response, PJ_STUN_ATTR_USERNAME, &username);

        /* Add REALM attribute */
        pj_stun_msg_add_string_attr(pool, response, PJ_STUN_ATTR_REALM, &realm);

        /* Add NONCE attribute */
        pj_stun_msg_add_string_attr(pool, response, PJ_STUN_ATTR_NONCE, &nonce);

        /* Add SOFTWARE attribute */
        pj_stun_msg_add_string_attr(pool, response, PJ_STUN_ATTR_SOFTWARE, &software);

        /* Add MAPPED-ADDRESS attribute using data */
        pj_bzero(&addr, sizeof(addr));
        addr.sin_family = pj_AF_INET();
        if (Size >= 6) {
            pj_memcpy(&addr.sin_addr, data, 4);
            pj_memcpy(&addr.sin_port, data + 4, 2);
        }
        pj_stun_msg_add_sockaddr_attr(pool, response, PJ_STUN_ATTR_MAPPED_ADDR,
                                       PJ_FALSE, (pj_sockaddr_t*)&addr, sizeof(addr));

        /* Add XOR-MAPPED-ADDRESS */
        pj_stun_msg_add_sockaddr_attr(pool, response, PJ_STUN_ATTR_XOR_MAPPED_ADDR,
                                       PJ_TRUE, (pj_sockaddr_t*)&addr, sizeof(addr));

        /* Add ERROR-CODE attribute based on flag */
        if (flag_error_code && Size >= 7) {
            int err_code = (data[6] % 200) + 400;
            pj_str_t err_reason = pj_str("Test Error");
            pj_stun_msg_add_errcode_attr(pool, response, err_code, &err_reason);
        }

        /* Add UNKNOWN-ATTRIBUTES if decode found unknown attrs */
        if (msg && flag_unknown_attrs && Size >= 8) {
            pj_uint16_t unknown_attrs[4] = {0x8001, 0x8002, 0x8003, 0x8004};
            pj_stun_msg_add_unknown_attr(pool, response,
                                          (data[7] % 4) + 1, unknown_attrs);
        }

        /* Path 5: Add MESSAGE-INTEGRITY and encode with proper key */
        pj_stun_msg_add_msgint_attr(pool, response);

        /* Create authentication key for MESSAGE-INTEGRITY calculation */
        pj_str_t key;
        pj_stun_create_key(pool, &key, &realm, &username,
                          PJ_STUN_PASSWD_PLAIN, &password);

        /* Encode message with MESSAGE-INTEGRITY */
        pj_stun_msg_encode(response, encoded_buf, kMaxInputLength,
                           0, &key, &encoded_len);

        /* Path 7: Attribute lookup and manipulation */
        pj_stun_msg_find_attr(response, PJ_STUN_ATTR_USERNAME, 0);
        pj_stun_msg_find_attr(response, PJ_STUN_ATTR_REALM, 0);
        pj_stun_msg_find_attr(response, PJ_STUN_ATTR_MESSAGE_INTEGRITY, 0);
    }

    /* Path 8: Create error response (only if msg is a request) */
    if (msg && PJ_STUN_IS_REQUEST(msg->hdr.type) && Size >= 9) {
        pj_str_t err_msg = pj_str("Unauthorized");
        pj_stun_msg_create_response(pool, msg, (data[8] % 200) + 400,
                                     &err_msg, &response);
    }

    /* Path 9: TURN session operations using full remaining data */
    if (Size >= 20 && flag_turn_enabled) {
        pj_turn_session *turn_sess = NULL;
        pj_turn_session_cb turn_cb;
        pj_stun_config stun_cfg;
        pj_ioqueue_t *ioqueue = NULL;
        pj_timer_heap_t *timer_heap = NULL;
        pj_status_t status;

        /* Setup STUN config for TURN */
        pj_stun_config_init(&stun_cfg, mem, 0, ioqueue, timer_heap);
        status = pj_ioqueue_create(pool, 4, &ioqueue);
        if (status != PJ_SUCCESS) goto on_turn_cleanup;
        status = pj_timer_heap_create(pool, 16, &timer_heap);
        if (status != PJ_SUCCESS) goto on_turn_cleanup;
        stun_cfg.ioqueue = ioqueue;
        stun_cfg.timer_heap = timer_heap;

        /* Create TURN session */
        pj_bzero(&turn_cb, sizeof(turn_cb));
        turn_cb.on_send_pkt = &turn_on_send_pkt;

        status = pj_turn_session_create(&stun_cfg, "turn_fuzz", pj_AF_INET(),
                                       PJ_TURN_TP_UDP, NULL, &turn_cb, 0, NULL, &turn_sess);

        if (status == PJ_SUCCESS && turn_sess) {
            /* Test TURN sendto (will return PJ_EIGNORED since session not READY).
             * Note: Driving session to READY requires simulating allocation with async
             * I/O and timer callbacks, impractical for fuzzing. This still exercises
             * session creation and sendto API with various inputs. */
            pj_sockaddr_in peer_addr;
            pj_bzero(&peer_addr, sizeof(peer_addr));
            peer_addr.sin_family = pj_AF_INET();

            if (Size >= 6) {
                pj_memcpy(&peer_addr.sin_addr, data, 4);
                pj_memcpy(&peer_addr.sin_port, data + 4, 2);

                size_t payload_len = Size - 6;
                if (payload_len > 512) payload_len = 512;

                if (payload_len > 0) {
                    pj_turn_session_sendto(turn_sess, data + 6, payload_len,
                                          (pj_sockaddr_t*)&peer_addr, sizeof(peer_addr));
                }
            }

            pj_turn_session_destroy(turn_sess, PJ_SUCCESS);
        }

on_turn_cleanup:
        if (timer_heap) pj_timer_heap_destroy(timer_heap);
        if (ioqueue) pj_ioqueue_destroy(ioqueue);
    }

    pj_pool_release(pool);
    return 0;
}


extern int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    int ret = 0;
    uint8_t *data;
    pj_caching_pool caching_pool;

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    /* Add NULL byte */
    data = (uint8_t *)calloc((Size+1), sizeof(uint8_t));
    memcpy((void *)data, (void *)Data, Size);

    /* init Calls */
    pj_init();
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    pj_log_set_level(0);

    mem = &caching_pool.factory;

    /* Call fuzzer */
    ret = stun_parse(data, Size);

    free(data);
    pj_caching_pool_destroy(&caching_pool);

    return ret;
}
