/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pj/hash.h>
#include <pj/rand.h>
#include <pj/log.h>
#include <pj/pool.h>
#include "test.h"

#if INCLUDE_HASH_TEST

#define HASH_COUNT  31
#define THIS_FILE   "hash_test.c"

#if defined(PJ_HASH_TABLE_USE_SIPHASH) && PJ_HASH_TABLE_USE_SIPHASH!=0 && \
    defined(PJ_HAS_INT64) && PJ_HAS_INT64!=0
#  define TEST_SIPHASH   1
#else
#  define TEST_SIPHASH   0
#endif

#if TEST_SIPHASH
/* Internal test hook exported by pjlib (see pjlib/src/pj/hash.c); not part of
 * the public API, declared here only for this unit test.
 */
PJ_DECL(pj_uint64_t) pj_hash_test_siphash24(pj_uint64_t k0, pj_uint64_t k1,
                                            const void *data, unsigned len);
#endif


static int hash_test_with_key(pj_pool_t *pool, unsigned char key)
{
    pj_hash_table_t *ht;
    unsigned value = 0x12345;
    pj_hash_iterator_t it_buf, *it;
    unsigned *entry;

    PJ_TEST_NOT_NULL( (ht=pj_hash_create(pool, HASH_COUNT)), NULL, return -10);

    pj_hash_set(pool, ht, &key, sizeof(key), 0, &value);

    PJ_TEST_NOT_NULL((entry=(unsigned*)pj_hash_get(ht,&key,sizeof(key),NULL)),
                     NULL, return -20);
    PJ_TEST_EQ( *entry, value, NULL, return -25);
    PJ_TEST_EQ(pj_hash_count(ht), 1, NULL, return -30);
    PJ_TEST_NOT_NULL((it=pj_hash_first(ht, &it_buf)), NULL, return -40);
    PJ_TEST_NOT_NULL((entry=(unsigned*)pj_hash_this(ht, it)), NULL,
                     return -50);
    PJ_TEST_EQ(*entry, value, NULL, return -60);
    PJ_TEST_EQ(pj_hash_next(ht, it), NULL, NULL, return -70);

    /* Erase item */

    pj_hash_set(NULL, ht, &key, sizeof(key), 0, NULL);

    PJ_TEST_EQ(pj_hash_get(ht, &key, sizeof(key), NULL), NULL, NULL,
               return -80);
    PJ_TEST_EQ(pj_hash_count(ht), 0, NULL, return -90);
    PJ_TEST_EQ(pj_hash_first(ht, &it_buf), NULL, NULL, return -100);
    return 0;
}


static int hash_collision_test(pj_pool_t *pool)
{
    enum {
        COUNT = HASH_COUNT * 4
    };
    pj_hash_table_t *ht;
    pj_hash_iterator_t it_buf, *it;
    unsigned char *values;
    unsigned i;

    PJ_TEST_NOT_NULL((ht=pj_hash_create(pool, HASH_COUNT)), NULL, return -200);

    values = (unsigned char*) pj_pool_alloc(pool, COUNT);

    for (i=0; i<COUNT; ++i) {
        values[i] = (unsigned char)i;
        pj_hash_set(pool, ht, &i, sizeof(i), 0, &values[i]);
    }

    PJ_TEST_EQ(pj_hash_count(ht), COUNT, NULL, return -210);

    for (i=0; i<COUNT; ++i) {
        unsigned char *entry;
        entry = (unsigned char*) pj_hash_get(ht, &i, sizeof(i), NULL);
        PJ_TEST_NOT_NULL(entry, NULL, return -220);
        PJ_TEST_EQ(*entry, values[i], NULL, return -230);
    }

    i = 0;
    it = pj_hash_first(ht, &it_buf);
    while (it) {
        ++i;
        it = pj_hash_next(ht, it);
    }

    PJ_TEST_EQ(i, COUNT, NULL, return -240);
    return 0;
}


#if TEST_SIPHASH
/*
 * Verify the keyed SipHash-2-4 core against the canonical reference vectors.
 */
static int siphash_vector_test(void)
{
    /* Reference vectors: key = bytes 00..0f (little endian), input =
     * bytes 00..len-1, for len = 0..32.
     */
    static const pj_uint64_t expected[] = {
        PJ_UINT64(0x726fdb47dd0e0e31), PJ_UINT64(0x74f839c593dc67fd),
        PJ_UINT64(0x0d6c8009d9a94f5a), PJ_UINT64(0x85676696d7fb7e2d),
        PJ_UINT64(0xcf2794e0277187b7), PJ_UINT64(0x18765564cd99a68d),
        PJ_UINT64(0xcbc9466e58fee3ce), PJ_UINT64(0xab0200f58b01d137),
        PJ_UINT64(0x93f5f5799a932462), PJ_UINT64(0x9e0082df0ba9e4b0),
        PJ_UINT64(0x7a5dbbc594ddb9f3), PJ_UINT64(0xf4b32f46226bada7),
        PJ_UINT64(0x751e8fbc860ee5fb), PJ_UINT64(0x14ea5627c0843d90),
        PJ_UINT64(0xf723ca908e7af2ee), PJ_UINT64(0xa129ca6149be45e5),
        PJ_UINT64(0x3f2acc7f57c29bdb), PJ_UINT64(0x699ae9f52cbe4794),
        PJ_UINT64(0x4bc1b3f0968dd39c), PJ_UINT64(0xbb6dc91da77961bd),
        PJ_UINT64(0xbed65cf21aa2ee98), PJ_UINT64(0xd0f2cbb02e3b67c7),
        PJ_UINT64(0x93536795e3a33e88), PJ_UINT64(0xa80c038ccd5ccec8),
        PJ_UINT64(0xb8ad50c6f649af94), PJ_UINT64(0xbce192de8a85b8ea),
        PJ_UINT64(0x17d835b85bbb15f3), PJ_UINT64(0x2f2e6163076bcfad),
        PJ_UINT64(0xde4daaaca71dc9a5), PJ_UINT64(0xa6a2506687956571),
        PJ_UINT64(0xad87a3535c49ef28), PJ_UINT64(0x32d892fad841c342),
        PJ_UINT64(0x7127512f72f27cce)
    };
    pj_uint64_t k0 = PJ_UINT64(0x0706050403020100);
    pj_uint64_t k1 = PJ_UINT64(0x0f0e0d0c0b0a0908);
    pj_uint8_t in[32];
    unsigned i;

    for (i=0; i<sizeof(in); ++i)
        in[i] = (pj_uint8_t)i;

    for (i=0; i<PJ_ARRAY_SIZE(expected); ++i) {
        pj_uint64_t h = pj_hash_test_siphash24(k0, k1, in, i);
        if (h != expected[i]) {
            PJ_LOG(1,(THIS_FILE,
                      "   error: SipHash-2-4 vector mismatch at len %u", i));
            return -300;
        }
    }
    return 0;
}
#endif  /* TEST_SIPHASH */


/*
 * Case-insensitive keys (pj_hash_*_lower) must map to the same entry
 * regardless of case, and must be a separate namespace from the
 * case-sensitive variants.
 */
static int hash_lower_test(pj_pool_t *pool)
{
    pj_hash_table_t *ht;
    unsigned v1 = 111, v2 = 222;
    void *e;

    PJ_TEST_NOT_NULL((ht=pj_hash_create(pool, HASH_COUNT)), NULL, return -400);

    /* Insert a mixed-case key via the lower variant. */
    pj_hash_set_lower(pool, ht, "SIP:Alice@Example.COM", PJ_HASH_KEY_STRING,
                      0, &v1);

    /* Lookup with a different case must find the same entry. */
    e = pj_hash_get_lower(ht, "sip:alice@example.com", PJ_HASH_KEY_STRING,
                          NULL);
    if (e != &v1) return -410;

    /* Case-sensitive lookup of that same key is a different namespace and
     * must NOT find the lower-cased entry.
     */
    e = pj_hash_get(ht, "SIP:Alice@Example.COM", PJ_HASH_KEY_STRING, NULL);
    PJ_TEST_EQ(e, NULL, NULL, return -420);

    /* Setting via yet another case updates the same single entry. */
    pj_hash_set_lower(pool, ht, "sip:ALICE@EXAMPLE.com", PJ_HASH_KEY_STRING,
                      0, &v2);
    PJ_TEST_EQ(pj_hash_count(ht), 1, NULL, return -430);
    e = pj_hash_get_lower(ht, "SIP:alice@example.COM", PJ_HASH_KEY_STRING,
                          NULL);
    if (e != &v2) return -440;

    return 0;
}


/*
 * The documented hval in/out behavior of pj_hash_get(): a zero hval is filled
 * with the deterministic djb2 value (matching pj_hash_calc), and a nonzero
 * hval is left untouched while the lookup still succeeds.
 */
static int hash_hval_test(pj_pool_t *pool)
{
    pj_hash_table_t *ht;
    unsigned value = 0x999;
    const char *KEY = "call-id-1234567890@host";
    pj_uint32_t hval, djb2;
    void *e;

    PJ_TEST_NOT_NULL((ht=pj_hash_create(pool, HASH_COUNT)), NULL, return -500);

    djb2 = pj_hash_calc(0, KEY, PJ_HASH_KEY_STRING);

    /* get() with a zero hval must fill it with the djb2 value, whether or not
     * the key is present.
     */
    hval = 0;
    e = pj_hash_get(ht, KEY, PJ_HASH_KEY_STRING, &hval);
    PJ_TEST_EQ(e, NULL, NULL, return -510);
    PJ_TEST_EQ(hval, djb2, NULL, return -520);

    /* get-then-set: the returned hval can be reused to insert. */
    pj_hash_set(pool, ht, KEY, PJ_HASH_KEY_STRING, hval, &value);
    PJ_TEST_EQ(pj_hash_count(ht), 1, NULL, return -530);
    e = pj_hash_get(ht, KEY, PJ_HASH_KEY_STRING, NULL);
    if (e != &value) return -540;

    /* A nonzero hval must be left untouched, and the lookup must still
     * succeed (bucketing does not depend on the supplied hval).
     */
    hval = djb2;
    e = pj_hash_get(ht, KEY, PJ_HASH_KEY_STRING, &hval);
    if (e != &value) return -550;
    PJ_TEST_EQ(hval, djb2, NULL, return -560);

    return 0;
}


/*
 * Hash table test.
 */
int hash_test(void)
{
    pj_pool_t *pool = pj_pool_create(mem, "hash", 512, 512, NULL);
    int rc;
    unsigned i;

    /* Test to fill in each row in the table */
    for (i=0; i<=HASH_COUNT; ++i) {
        rc = hash_test_with_key(pool, (unsigned char)i);
        if (rc != 0) {
            pj_pool_release(pool);
            return rc;
        }
    }

    /* Collision test */
    rc = hash_collision_test(pool);
    if (rc != 0) {
        pj_pool_release(pool);
        return rc;
    }

#if TEST_SIPHASH
    /* Canonical SipHash-2-4 vectors */
    rc = siphash_vector_test();
    if (rc != 0) {
        pj_pool_release(pool);
        return rc;
    }
#endif

    /* Case-insensitive keys */
    rc = hash_lower_test(pool);
    if (rc != 0) {
        pj_pool_release(pool);
        return rc;
    }

    /* hval in/out contract */
    rc = hash_hval_test(pool);
    if (rc != 0) {
        pj_pool_release(pool);
        return rc;
    }

    pj_pool_release(pool);
    return 0;
}

#endif  /* INCLUDE_HASH_TEST */

