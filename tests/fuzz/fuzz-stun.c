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

/* TURN session callback stubs */
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

static void turn_on_rx_data(pj_turn_session *sess,
                           void *pkt,
                           unsigned pkt_len,
                           const pj_sockaddr_t *peer_addr,
                           unsigned addr_len)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(pkt_len);
    PJ_UNUSED_ARG(peer_addr);
    PJ_UNUSED_ARG(addr_len);
}

static void turn_on_state(pj_turn_session *sess,
                         pj_turn_state_t old_state,
                         pj_turn_state_t new_state)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(old_state);
    PJ_UNUSED_ARG(new_state);
}

/* ICE session callback stubs */
static void ice_on_rx_data(pj_ice_strans *ice_st,
                          unsigned comp_id,
                          void *pkt,
                          pj_size_t size,
                          const pj_sockaddr_t *src_addr,
                          unsigned src_addr_len)
{
    PJ_UNUSED_ARG(ice_st);
    PJ_UNUSED_ARG(comp_id);
    PJ_UNUSED_ARG(pkt);
    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);
}

static void ice_on_ice_complete(pj_ice_strans *ice_st,
                                pj_ice_strans_op op,
                                pj_status_t status)
{
    PJ_UNUSED_ARG(ice_st);
    PJ_UNUSED_ARG(op);
    PJ_UNUSED_ARG(status);
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
    uint8_t flag_ice_enabled = data[3] & 0x04;

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

    /* Path 9: TURN session operations - sendto, packet reception, and allocate requests */
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
        turn_cb.on_rx_data = &turn_on_rx_data;
        turn_cb.on_state = &turn_on_state;

        status = pj_turn_session_create(&stun_cfg, "turn_fuzz", pj_AF_INET(),
                                       PJ_TURN_TP_UDP, NULL, &turn_cb, 0, NULL, &turn_sess);

        if (status == PJ_SUCCESS && turn_sess) {
            /* Configure TURN server and credentials */
            pj_str_t srv_addr_str = pj_str("127.0.0.1");
            int default_port = 3478;
            pj_stun_auth_cred cred;

            pj_turn_session_set_server(turn_sess, &srv_addr_str, default_port, NULL);

            pj_bzero(&cred, sizeof(cred));
            cred.type = PJ_STUN_AUTH_CRED_STATIC;
            cred.data.static_cred.username = pj_str(FIXED_USERNAME);
            cred.data.static_cred.data_type = PJ_STUN_PASSWD_PLAIN;
            cred.data.static_cred.data = pj_str(FIXED_PASSWORD);
            pj_turn_session_set_credential(turn_sess, &cred);

            /* Test TURN sendto API */
            if (Size >= 6) {
                pj_sockaddr_in peer_addr;
                pj_bzero(&peer_addr, sizeof(peer_addr));
                peer_addr.sin_family = pj_AF_INET();
                pj_memcpy(&peer_addr.sin_addr, data, 4);
                pj_memcpy(&peer_addr.sin_port, data + 4, 2);

                size_t payload_len = Size - 6;
                if (payload_len > 512) payload_len = 512;

                if (payload_len > 0) {
                    pj_turn_session_sendto(turn_sess, data + 6, payload_len,
                                          (pj_sockaddr_t*)&peer_addr, sizeof(peer_addr));
                }
            }

            /* Test TURN packet reception API */
            if (Size >= 40) {
                pj_turn_session_on_rx_pkt_param prm;
                pj_sockaddr_in src_addr;
                size_t pkt_len = Size;
                if (pkt_len > 1500) pkt_len = 1500;

                pj_bzero(&src_addr, sizeof(src_addr));
                src_addr.sin_family = pj_AF_INET();
                src_addr.sin_port = pj_htons(default_port);
                src_addr.sin_addr.s_addr = pj_htonl(0x7F000001);

                pj_bzero(&prm, sizeof(prm));
                prm.pkt = data;
                prm.pkt_len = pkt_len;
                prm.src_addr = (pj_sockaddr*)&src_addr;
                prm.src_addr_len = sizeof(src_addr);

                pj_turn_session_on_rx_pkt2(turn_sess, &prm);
            }

            pj_turn_session_destroy(turn_sess, PJ_SUCCESS);
        }

on_turn_cleanup:
        if (timer_heap) pj_timer_heap_destroy(timer_heap);
        if (ioqueue) pj_ioqueue_destroy(ioqueue);
    }

    /* Path 10: ICE session operations - connectivity checks and packet handling */
    if (Size >= 50 && flag_ice_enabled) {
        pj_ice_strans *ice_st = NULL;
        pj_ice_strans_cb ice_cb;
        pj_ice_strans_cfg ice_cfg;
        pj_stun_config stun_cfg;
        pj_ioqueue_t *ioqueue = NULL;
        pj_timer_heap_t *timer_heap = NULL;
        pj_status_t status;

        /* Setup infrastructure */
        pj_ioqueue_create(pool, 8, &ioqueue);
        pj_timer_heap_create(pool, 32, &timer_heap);
        pj_stun_config_init(&stun_cfg, mem, 0, ioqueue, timer_heap);

        /* Create ICE transport */
        pj_bzero(&ice_cb, sizeof(ice_cb));
        ice_cb.on_rx_data = &ice_on_rx_data;
        ice_cb.on_ice_complete = &ice_on_ice_complete;

        pj_ice_strans_cfg_default(&ice_cfg);
        ice_cfg.stun_cfg = stun_cfg;
        ice_cfg.af = pj_AF_INET();

        status = pj_ice_strans_create("ice_fuzz", &ice_cfg, 1,
                                     NULL, &ice_cb, &ice_st);

        if (status == PJ_SUCCESS && ice_st) {
            pj_str_t local_ufrag = pj_str(FIXED_USERNAME);
            pj_str_t local_pwd = pj_str(FIXED_PASSWORD);

            /* Initialize ICE with role based on flag */
            pj_ice_sess_role role = (data[0] & 0x04) ? 
                                    PJ_ICE_SESS_ROLE_CONTROLLING :
                                    PJ_ICE_SESS_ROLE_CONTROLLED;

            status = pj_ice_strans_init_ice(ice_st, role, &local_ufrag, &local_pwd);

            if (status == PJ_SUCCESS) {
                /* Query ICE state - exercises state management code */
                pj_ice_strans_get_state(ice_st);
                pj_ice_strans_has_sess(ice_st);
                pj_ice_strans_sess_is_running(ice_st);
                pj_ice_strans_sess_is_complete(ice_st);

                /* Test candidate enumeration if we have data */
                if (Size >= 20) {
                    unsigned comp_id = 1;
                    unsigned cand_cnt = 8;
                    pj_ice_sess_cand cands[8];
                    pj_ice_sess_cand def_cand;

                    pj_ice_strans_enum_cands(ice_st, comp_id, &cand_cnt, cands);
                    pj_ice_strans_get_def_cand(ice_st, comp_id, &def_cand);
                }
            }

            pj_ice_strans_destroy(ice_st);
        }

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
