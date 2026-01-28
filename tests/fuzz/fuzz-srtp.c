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
#include <stdlib.h>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <pjmedia/transport_srtp.h>
#include <pjmedia/transport_loop.h>

#define kMinInputLength 16
#define kMaxInputLength 5120

/* For SRTP session testing */
pj_pool_factory *mem;

/* SRTP key material: raw binary format */
static const unsigned char key_30[30] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d
};

static const unsigned char key_46[46] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d
};

static void setup_crypto_config(uint8_t crypto_selector,
                                pjmedia_srtp_crypto *tx_crypto,
                                pjmedia_srtp_crypto *rx_crypto)
{
    const char *crypto_names[] = {
        "AES_CM_128_HMAC_SHA1_80",
        "AES_CM_128_HMAC_SHA1_32",
        "AES_CM_256_HMAC_SHA1_80",
        "AES_CM_256_HMAC_SHA1_32",
        "NULL_HMAC_SHA1_80",
        "NULL_HMAC_SHA1_32"
    };

    pj_bzero(tx_crypto, sizeof(*tx_crypto));
    pj_bzero(rx_crypto, sizeof(*rx_crypto));
    tx_crypto->name = pj_str((char*)crypto_names[crypto_selector]);
    rx_crypto->name = pj_str((char*)crypto_names[crypto_selector]);

    if (crypto_selector == 0 || crypto_selector == 1 ||
        crypto_selector == 4 || crypto_selector == 5) {
        tx_crypto->key.ptr = (char*)key_30;
        tx_crypto->key.slen = 30;
        rx_crypto->key.ptr = (char*)key_30;
        rx_crypto->key.slen = 30;
    } else {
        tx_crypto->key.ptr = (char*)key_46;
        tx_crypto->key.slen = 46;
        rx_crypto->key.ptr = (char*)key_46;
        rx_crypto->key.slen = 46;
    }
}

static pj_status_t create_srtp_transport(pjmedia_endpt *endpt,
                                         pjmedia_transport **loop_tp,
                                         pjmedia_transport **srtp_tp)
{
    pj_status_t status;
    pjmedia_srtp_setting srtp_opt;

    status = pjmedia_transport_loop_create(endpt, loop_tp);
    if (status != PJ_SUCCESS)
        return status;

    pjmedia_srtp_setting_default(&srtp_opt);
    srtp_opt.close_member_tp = PJ_FALSE;
    srtp_opt.use = PJMEDIA_SRTP_MANDATORY;

    status = pjmedia_transport_srtp_create(endpt, *loop_tp, &srtp_opt, srtp_tp);
    if (status != PJ_SUCCESS) {
        pjmedia_transport_close(*loop_tp);
        return status;
    }

    return PJ_SUCCESS;
}

static void fuzz_srtp_decrypt(pjmedia_transport *srtp_tp,
                              pj_bool_t is_rtp,
                              const uint8_t *pkt_data,
                              int pkt_len,
                              pj_pool_t *pool)
{
    unsigned char *pkt_buf;
    int dec_len;

    if (pkt_len <= 0)
        return;

    pkt_buf = (unsigned char*)pj_pool_alloc(pool, pkt_len);
    if (!pkt_buf)
        return;

    pj_memcpy(pkt_buf, pkt_data, pkt_len);

    dec_len = pkt_len;
    pjmedia_transport_srtp_decrypt_pkt(srtp_tp, is_rtp, pkt_buf, &dec_len);
}

void srtp_test(const uint8_t *data, size_t size)
{
    pjmedia_endpt *endpt = NULL;
    pj_pool_t *pool = NULL;
    pj_status_t status;
    pjmedia_transport *loop_tp = NULL;
    pjmedia_transport *srtp_tp = NULL;
    pjmedia_srtp_crypto tx_crypto, rx_crypto;

    /* Create media endpoint */
    status = pjmedia_endpt_create(mem, NULL, 1, &endpt);
    if (status != PJ_SUCCESS)
        return;

    /* Initialize SRTP library */
    status = pjmedia_srtp_init_lib(endpt);
    if (status != PJ_SUCCESS)
        goto cleanup;

    /* Create memory pool */
    pool = pj_pool_create(mem, "srtp-fuzz", 4000, 4000, NULL);
    if (!pool)
        goto cleanup;

    /* Create transport stack */
    status = create_srtp_transport(endpt, &loop_tp, &srtp_tp);
    if (status != PJ_SUCCESS)
        goto cleanup;

    /* Setup crypto configuration from fuzzer input */
    setup_crypto_config(data[0] % 6, &tx_crypto, &rx_crypto);

    /* Start SRTP session */
    status = pjmedia_transport_srtp_start(srtp_tp, &tx_crypto, &rx_crypto);
    if (status != PJ_SUCCESS)
        goto cleanup;

    /* Test SRTP decryption with packet data */
    fuzz_srtp_decrypt(srtp_tp, (data[1] & 0x01) ? PJ_TRUE : PJ_FALSE,
                      data + 4, (int)(size - 4), pool);

cleanup:
    if (srtp_tp)
        pjmedia_transport_close(srtp_tp);
    if (loop_tp)
        pjmedia_transport_close(loop_tp);
    if (pool)
        pj_pool_release(pool);
    if (endpt)
        pjmedia_endpt_destroy(endpt);
}

extern int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
    pj_caching_pool caching_pool;

    if (Size < kMinInputLength || Size > kMaxInputLength) {
        return 1;
    }

    /* Init */
    pj_init();
    pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
    pj_log_set_level(0);

    /* Configure the global blocking pool */
    mem = &caching_pool.factory;

    /* Fuzz SRTP */
    srtp_test(Data, Size);

    /* Cleanup */
    pj_caching_pool_destroy(&caching_pool);

    return 0;
}
