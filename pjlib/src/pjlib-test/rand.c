/* 
 * Copyright (C) 2008-2026 Teluu Inc. (http://www.teluu.com)
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
#include <pj/rand.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>
#include "test.h"

#define THIS_FILE   "rand.c"

#if INCLUDE_RAND_TEST

#define COUNT  1024
static int values[COUNT];

/*
 * rand_test(), simply generates COUNT number of random number and
 * check that there's no duplicate numbers.
 */
int rand_test(void)
{
    int i;

    for (i=0; i<COUNT; ++i) {
        int j;

        values[i] = pj_rand();
        for (j=0; j<i; ++j) {
            if (values[i] == values[j]) {
                PJ_LOG(3,("test", "error: duplicate value %d at %d-th index",
                         values[i], i));
                return -10;
            }
        }
    }

    return 0;
}

/*
 * csrand_test(): pj_csrand_bytes() either reports PJ_ENOTSUP or fills
 * exactly the requested bytes, every one of them, with output that looks
 * random. Never anything in between.
 */
#define CSRAND_GUARD    0x5A
#define CSRAND_FILL     0xA5
#define CSRAND_MAX      8192

int csrand_test(void)
{
    static const unsigned sizes[] = { 1, 3, 16, 255, 256, 257, 1027,
                                      CSRAND_MAX };
    pj_uint8_t a[32], b[32];
    pj_pool_t *pool;
    pj_uint8_t *buf, *seen;
    pj_status_t status;
    unsigned i, j, k, ones;
    int rc = 0;

    PJ_TEST_SUCCESS(pj_csrand_bytes(NULL, 0), "zero length", return -10);

    pj_bzero(a, sizeof(a));
    pj_bzero(b, sizeof(b));
    status = pj_csrand_bytes(a, sizeof(a));
    if (status == PJ_ENOTSUP) {
        PJ_LOG(3,(THIS_FILE, "  pj_csrand_bytes() not supported here"));
        return 0;
    }
    PJ_TEST_SUCCESS(status, "pj_csrand_bytes()", return -20);
    PJ_TEST_SUCCESS(pj_csrand_bytes(b, sizeof(b)), NULL, return -21);
    PJ_TEST_TRUE(pj_memcmp(a, b, sizeof(a)) != 0, "identical outputs",
                 return -22);
    for (i=0; i<sizeof(a) && a[i]==0; ++i)
        ;
    PJ_TEST_TRUE(i < sizeof(a), "all-zero output", return -23);

    PJ_TEST_NOT_NULL((pool=pj_pool_create(mem, "csrand", 2*CSRAND_MAX+256,
                                          0, NULL)),
                     NULL, return -30);
    buf = (pj_uint8_t*)pj_pool_alloc(pool, CSRAND_MAX + 2);
    seen = (pj_uint8_t*)pj_pool_alloc(pool, CSRAND_MAX);

    /* Odd sizes, sizes around getrandom()'s 256-byte atomic limit, and an
     * unaligned start. The guard bytes either side must stay untouched,
     * and every byte must differ from the pre-fill in at least one of a
     * few draws.
     */
    for (i=0; i<PJ_ARRAY_SIZE(sizes); ++i) {
        unsigned n = sizes[i];
        unsigned left = n;

        pj_bzero(seen, n);
        for (j=0; j<8 && left; ++j) {
            pj_memset(buf, CSRAND_FILL, n + 2);
            buf[0] = buf[n + 1] = CSRAND_GUARD;

            PJ_TEST_SUCCESS(pj_csrand_bytes(buf + 1, n), NULL,
                            {rc = -40; goto on_return;});
            PJ_TEST_TRUE(buf[0] == CSRAND_GUARD && buf[n+1] == CSRAND_GUARD,
                         "write outside the buffer",
                         {rc = -41; goto on_return;});
            for (k=0; k<n; ++k) {
                if (!seen[k] && buf[k + 1] != CSRAND_FILL) {
                    seen[k] = 1;
                    --left;
                }
            }
        }
        if (left) {
            PJ_LOG(1,(THIS_FILE, "  %u of %u bytes never written",
                      left, n));
            rc = -42;
            goto on_return;
        }
    }

    /* Bit balance of the last, largest draw: 65536 bits, mean 32768,
     * standard deviation 128. Eight deviations either side.
     */
    for (i=1, ones=0; i<=CSRAND_MAX; ++i) {
        pj_uint8_t v = buf[i];
        while (v) {
            ones += (v & 1);
            v >>= 1;
        }
    }
    if (ones < CSRAND_MAX*4 - 1024 || ones > CSRAND_MAX*4 + 1024) {
        PJ_LOG(1,(THIS_FILE, "  biased output: %u of %u bits set",
                  ones, CSRAND_MAX*8));
        rc = -50;
    }

on_return:
    pj_pool_release(pool);
    return rc;
}

#endif  /* INCLUDE_RAND_TEST */

