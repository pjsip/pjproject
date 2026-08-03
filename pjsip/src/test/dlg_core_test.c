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

/* Test-only hook exported from sip_dialog.c (not in any public header): returns
 * the tag_hval that the dialog layer would store for the given tag. */
PJ_DECL(pj_uint32_t) pjsip_dlg_test_calc_tag_hval(const pj_str_t *tag);

/*
 * Regression test for issue #5100: pj_hash_calc_tolower() can legitimately
 * return zero for certain inputs, but the UA layer reserves a tag_hval of zero
 * as an "uncomputed" sentinel (see the assertion in pjsip_ua_register_dlg()).
 * A dialog whose local tag hashes to zero must therefore still be assigned a
 * nonzero tag_hval, otherwise dialog registration fails.
 */
static int tag_hval_zero_test(void)
{
    /* This particular UUID djb2-hashes (lowercased, 32-bit) to exactly zero;
     * it is the value from the original bug report. */
    const pj_str_t zero_tag = { "d48600d9-514b-4655-a606-43a65dc1f54b", 36 };
    pj_uint32_t raw, hval;

    /* Confirm the trigger condition still holds: the raw hash is zero. If the
     * hash algorithm ever changes, this vector may need to be regenerated. */
    raw = pj_hash_calc_tolower(0, NULL, &zero_tag);
    PJ_TEST_EQ(raw, 0, "test vector no longer hashes to zero", return -10);

    /* The dialog layer must coerce the zero hash to a nonzero tag_hval. */
    hval = pjsip_dlg_test_calc_tag_hval(&zero_tag);
    PJ_TEST_NON_ZERO(hval, "tag_hval must never be the zero sentinel",
                     return -20);

    /* A tag with a normal (nonzero) hash must be left untouched. */
    {
        const pj_str_t norm_tag = { "abc123", 6 };
        raw = pj_hash_calc_tolower(0, NULL, &norm_tag);
        PJ_TEST_NON_ZERO(raw, "unexpected zero hash for sanity vector",
                         return -30);
        hval = pjsip_dlg_test_calc_tag_hval(&norm_tag);
        PJ_TEST_EQ(hval, raw, "nonzero hash must be preserved", return -40);
    }

    return 0;
}

int dlg_core_test(void)
{
    return tag_hval_zero_test();
}
