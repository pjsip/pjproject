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

#include "test.h"
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "dlg_core_test.c"

/*
 * Regression test for issue #5100: pj_hash_calc_tolower() can legitimately
 * return zero for certain inputs. The dialog set hash table in sip_ua_layer.c
 * keys dialog sets by the (lowercased) local tag, and pjsip_ua_register_dlg()
 * used to reject a zero tag hash as if it were an "uncomputed" marker. A zero
 * hash is however a valid value: the dialog table must be able to register a
 * dialog set under such a tag and then find it again on incoming traffic.
 *
 * This test exercises the exact hash table usage pattern of sip_ua_layer.c
 * (register with the tag hash, then look up both with NULL and with the cached
 * hash) using a tag that is known to hash to zero, and verifies the dialog set
 * round-trips. It fails if any scheme stores the entry under a different hash
 * than the lookups recompute (e.g. remapping the zero hash on insert only).
 */
static int tag_hval_zero_test(void)
{
    /* This particular UUID djb2-hashes (lowercased, 32-bit) to exactly zero;
     * it is the local tag from the original bug report. */
    const pj_str_t zero_tag = { "d48600d9-514b-4655-a606-43a65dc1f54b", 36 };
    int marker = 12345;         /* stand-in for a dlg_set pointer value */
    pj_pool_t *pool;
    pj_hash_table_t *ht;
    pj_uint32_t hval;
    void *found;
    int rc = 0;

    /* Confirm the trigger condition still holds: the raw hash is zero. If the
     * hash algorithm ever changes, this vector may need to be regenerated. */
    hval = pj_hash_calc_tolower(0, NULL, &zero_tag);
    PJ_TEST_EQ(hval, 0, "test vector no longer hashes to zero", return -10);

    pool = pjsip_endpt_create_pool(endpt, "dlgtest", 4000, 4000);
    PJ_TEST_NOT_NULL(pool, NULL, return -20);
    ht = pj_hash_create(pool, 32);
    PJ_TEST_NOT_NULL(ht, NULL, {rc = -30; goto on_return;});

    /* Register the entry, mirroring pjsip_ua_register_dlg(): pass the cached
     * (zero) tag hash. */
    pj_hash_set_lower(pool, ht, zero_tag.ptr, (unsigned)zero_tag.slen,
                      hval, &marker);

    /* Lookup as the incoming-message paths do (sip_ua_layer.c:503/623/822):
     * NULL hval, so the table recomputes the hash. */
    found = pj_hash_get_lower(ht, zero_tag.ptr, (unsigned)zero_tag.slen, NULL);
    PJ_TEST_EQ(found, &marker, "zero-hash entry not found via recompute",
               {rc = -40; goto on_return;});

    /* Lookup as pjsip_ua_register_dlg()/unregister do: pass the cached hash. */
    found = pj_hash_get_lower(ht, zero_tag.ptr, (unsigned)zero_tag.slen, &hval);
    PJ_TEST_EQ(found, &marker, "zero-hash entry not found via cached hval",
               {rc = -50; goto on_return;});

    /* A different tag must not collide onto the zero-hash entry. */
    {
        const pj_str_t other = { "abc123", 6 };
        found = pj_hash_get_lower(ht, other.ptr, (unsigned)other.slen, NULL);
        PJ_TEST_EQ(found, NULL, "unexpected match for a different tag",
                   {rc = -60; goto on_return;});
    }

on_return:
    pjsip_endpt_release_pool(endpt, pool);
    return rc;
}

int dlg_core_test(void)
{
    return tag_hval_zero_test();
}
