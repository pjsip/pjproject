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
#include <stdint.h>
#include <string.h>
#include <pjlib.h>
#include <pjnath.h>

#define POOL_SIZE 8192
#define MAX_CANDIDATES 8

/* ICE callbacks - stateless stubs */
static void on_rx_data(pj_ice_strans *ice_st, unsigned comp_id,
                       void *pkt, pj_size_t size,
                       const pj_sockaddr_t *src_addr, unsigned src_addr_len)
{
    (void)ice_st; (void)comp_id; (void)pkt; (void)size;
    (void)src_addr; (void)src_addr_len;
}

static void on_ice_complete(pj_ice_strans *ice_st, pj_ice_strans_op op,
                            pj_status_t status)
{
    (void)ice_st; (void)op; (void)status;
}

/* Parse fuzzer input into remote candidates */
static unsigned parse_candidates(const uint8_t *data, size_t size,
                                 pj_ice_sess_cand *cands, unsigned max_cands)
{
    unsigned count = 0;
    size_t offset = 0;

    /* Need at least 12 bytes per candidate */
    while (offset + 12 <= size && count < max_cands) {
        pj_ice_sess_cand *cand = &cands[count];
        pj_bzero(cand, sizeof(*cand));

        /* Candidate type from fuzzer */
        cand->type = (pj_ice_cand_type)(data[offset] % PJ_ICE_CAND_TYPE_MAX);
        cand->comp_id = 1;
        cand->transport_id = 0;
        cand->local_pref = 65535;

        /* Priority from fuzzer (4 bytes) - cast to avoid UB */
        cand->prio = ((pj_uint32_t)data[offset+1] << 24) |
                     ((pj_uint32_t)data[offset+2] << 16) |
                     ((pj_uint32_t)data[offset+3] << 8) |
                     (pj_uint32_t)data[offset+4];

        /* Foundation - simple static string */
        cand->foundation = pj_str("candidate");

        /* IP address from fuzzer (4 bytes) */
        pj_sockaddr_in *addr = (pj_sockaddr_in*)&cand->addr;
        pj_bzero(addr, sizeof(*addr));
        addr->sin_family = pj_AF_INET();
        pj_memcpy(&addr->sin_addr, data + offset + 5, 4);
        addr->sin_port = pj_htons(((pj_uint16_t)data[offset+9] << 8) |
                                  (pj_uint16_t)data[offset+10]);

        /* Base/related address */
        pj_memcpy(&cand->base_addr, &cand->addr, sizeof(cand->addr));
        pj_memcpy(&cand->rel_addr, &cand->addr, sizeof(cand->addr));
        count++;
        offset += 12;
    }
    return count;
}

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    /* Initialize pjlib once */
    static int pj_initialized = 0;
    pj_caching_pool caching_pool;
    pj_ioqueue_t *ioqueue = NULL;
    pj_timer_heap_t *timer_heap = NULL;
    pj_stun_config stun_cfg;
    pj_ice_strans *ice_st = NULL;
    pj_pool_t *pool = NULL;
    pj_pool_t *env_pool = NULL;
    int i;

    if (Size < 30)
        return 0;

    if (!pj_initialized) {
        pj_init();
        pj_initialized = 1;
    }

    /* Create caching pool for this iteration */
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    pj_log_set_level(0);

    /* Create environment pool for ioqueue and timer_heap */
    env_pool = pj_pool_create(&caching_pool.factory, "ice_env", 4096, 4096, NULL);
    if (!env_pool) goto cleanup;

    /* Create ioqueue and timer heap for this iteration */
    if (pj_ioqueue_create(env_pool, 16, &ioqueue) != PJ_SUCCESS)
        goto cleanup;
    if (pj_timer_heap_create(env_pool, 64, &timer_heap) != PJ_SUCCESS)
        goto cleanup;

    pj_stun_config_init(&stun_cfg, &caching_pool.factory, 0, ioqueue, timer_heap);

    /* Create work pool */
    pool = pj_pool_create(&caching_pool.factory, "ice_fuzz", POOL_SIZE, POOL_SIZE, NULL);
    if (!pool) goto cleanup;

    pj_ice_strans_cb ice_cb;
    pj_ice_strans_cfg ice_cfg;

    /* Setup callbacks */
    pj_bzero(&ice_cb, sizeof(ice_cb));
    ice_cb.on_rx_data = on_rx_data;
    ice_cb.on_ice_complete = on_ice_complete;

    /* Configure ICE transport */
    pj_ice_strans_cfg_default(&ice_cfg);
    ice_cfg.stun_cfg = stun_cfg;
    ice_cfg.af = pj_AF_INET();
    ice_cfg.opt.aggressive = PJ_FALSE;

    /* Create ICE stream transport */
    if (pj_ice_strans_create("fuzz", &ice_cfg, 1, NULL, &ice_cb, &ice_st) != PJ_SUCCESS)
        goto cleanup;

    /* Initialize ICE session with credentials from fuzzer */
    pj_str_t local_ufrag, local_pwd;
    char ufrag_buf[16], pwd_buf[32];

    size_t ufrag_len = (Data[0] % 8) + 4;
    size_t pwd_len = (Data[1] % 16) + 16;

    if (ufrag_len + pwd_len > Size - 10) {
        ufrag_len = 4;
        pwd_len = 16;
    }

    pj_memcpy(ufrag_buf, Data + 2, ufrag_len);
    ufrag_buf[ufrag_len] = '\0';
    pj_memcpy(pwd_buf, Data + 2 + ufrag_len, pwd_len);
    pwd_buf[pwd_len] = '\0';

    local_ufrag = pj_str(ufrag_buf);
    local_pwd = pj_str(pwd_buf);

    /* Role from fuzzer */
    pj_ice_sess_role role = (Data[ufrag_len + pwd_len + 2] & 1) ?
                             PJ_ICE_SESS_ROLE_CONTROLLING :
                             PJ_ICE_SESS_ROLE_CONTROLLED;

    if (pj_ice_strans_init_ice(ice_st, role, &local_ufrag, &local_pwd) != PJ_SUCCESS)
        goto cleanup;

    /* Parse remote candidates from fuzzer input */
    size_t cand_offset = ufrag_len + pwd_len + 10;
    pj_ice_sess_cand remote_cands[MAX_CANDIDATES];
    unsigned remote_count = 0;

    if (cand_offset + 24 < Size) {
        remote_count = parse_candidates(Data + cand_offset, Size - cand_offset,
                                       remote_cands, MAX_CANDIDATES);
    }

    /* Start negotiation with remote candidates */
    if (remote_count > 0) {
        pj_str_t remote_ufrag = pj_str("remote");
        pj_str_t remote_pwd = pj_str("remotepassword");

        if (Size > cand_offset + 50) {
            size_t rem_ufrag_len = (Data[cand_offset] % 8) + 4;
            size_t rem_pwd_len = (Data[cand_offset+1] % 16) + 16;

            static char remote_ufrag_buf[16], remote_pwd_buf[32];
            if (rem_ufrag_len < sizeof(remote_ufrag_buf) &&
                rem_pwd_len < sizeof(remote_pwd_buf) &&
                cand_offset + rem_ufrag_len + rem_pwd_len < Size) {
                pj_memcpy(remote_ufrag_buf, Data + cand_offset + 2, rem_ufrag_len);
                remote_ufrag_buf[rem_ufrag_len] = '\0';
                pj_memcpy(remote_pwd_buf, Data + cand_offset + 2 + rem_ufrag_len, rem_pwd_len);
                remote_pwd_buf[rem_pwd_len] = '\0';
                remote_ufrag = pj_str(remote_ufrag_buf);
                remote_pwd = pj_str(remote_pwd_buf);
            }
        }

        pj_ice_strans_start_ice(ice_st, &remote_ufrag, &remote_pwd,
                               remote_count, remote_cands);
    }

    /* Process STUN messages from remaining fuzzer input */
    size_t stun_offset = cand_offset + (remote_count * 12) + 20;
    if (stun_offset + 20 < Size) {
        const uint8_t *stun_data = Data + stun_offset;
        size_t stun_len = Size - stun_offset;

        pj_stun_msg *stun_msg = NULL;
        if (pj_stun_msg_decode(pool, stun_data, stun_len,
                              PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET,
                              &stun_msg, NULL, NULL) == PJ_SUCCESS && stun_msg) {

            pj_sockaddr_in src_addr;
            pj_sockaddr_in_init(&src_addr, NULL, 0);
            if (remote_count > 0 && remote_cands[0].addr.addr.sa_family == pj_AF_INET()) {
                pj_memcpy(&src_addr, &remote_cands[0].addr, sizeof(src_addr));
            }

            pj_ice_strans_sendto2(ice_st, 1, stun_data, stun_len,
                                 (pj_sockaddr_t*)&src_addr, sizeof(src_addr));
        }
    }

    /* Poll timer heap to drive state machine */
    for (i = 0; i < 5; i++) {
        pj_time_val timeout = {0, 100};
        pj_timer_heap_poll(timer_heap, &timeout);
    }

    /* Test state queries */
    pj_ice_strans_get_state(ice_st);
    pj_ice_strans_has_sess(ice_st);
    pj_ice_strans_sess_is_running(ice_st);
    pj_ice_strans_sess_is_complete(ice_st);

    /* Enumerate candidates */
    if (remote_count > 0) {
        unsigned cand_count = MAX_CANDIDATES;
        pj_ice_sess_cand cands[MAX_CANDIDATES];
        pj_ice_strans_enum_cands(ice_st, 1, &cand_count, cands);
        pj_ice_sess_cand def_cand;
        pj_ice_strans_get_def_cand(ice_st, 1, &def_cand);
    }

    pj_ice_strans_stop_ice(ice_st);

cleanup:
    if (ice_st)
        pj_ice_strans_destroy(ice_st);
    if (pool)
        pj_pool_release(pool);
    if (timer_heap)
        pj_timer_heap_destroy(timer_heap);
    if (ioqueue)
        pj_ioqueue_destroy(ioqueue);
    if (env_pool)
        pj_pool_release(env_pool);

    pj_caching_pool_destroy(&caching_pool);

    return 0;
}
