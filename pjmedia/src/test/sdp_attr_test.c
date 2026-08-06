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
#include <pjmedia/sdp.h>
#include "test.h"

#define THIS_FILE   "sdp_attr_test.c"

/* Tests for the numeric fields of the SDP attribute parsers. The numeric
 * values are parsed into 32 bit fields, so values that do not fit must be
 * rejected rather than stored wrapped. Note that the parsers used to
 * accumulate into an unsigned long without any overflow detection, so on
 * LP64 platforms "4294975296" (2^32 + 8000) was stored as a clock rate of
 * 8000, and "18446744073709556676" (2^64 + 5060) passed the RTCP port
 * range check as port 5060.
 */

/* Largest value that fits, and the smallest ones that don't. */
#define STR_UINT32_MAX          "4294967295"
#define STR_UINT32_MAX_PLUS_1   "4294967296"
#define STR_ULONG_MAX_PLUS_1    "18446744073709551616"


static int rtpmap_test(pj_pool_t *pool)
{
    struct test_vec
    {
        char       *value;
        pj_status_t exp_status;
        unsigned    exp_clock_rate;
    } test_vec[] =
    {
        /* Valid */
        { "0 PCMU/8000",                    PJ_SUCCESS, 8000 },
        { "98 L16/16000/2",                 PJ_SUCCESS, 16000 },
        { "96 opus/48000/2",                PJ_SUCCESS, 48000 },
        { "101 telephone-event/8000",       PJ_SUCCESS, 8000 },
        { "111 X/" STR_UINT32_MAX,          PJ_SUCCESS, 4294967295UL },

        /* Out of range */
        { "111 X/" STR_UINT32_MAX_PLUS_1,   PJMEDIA_SDP_EINRTPMAP, 0 },
        { "111 X/" STR_ULONG_MAX_PLUS_1,    PJMEDIA_SDP_EINRTPMAP, 0 },
        { "0 PCMU/4294975296",              PJMEDIA_SDP_EINRTPMAP, 0 },
        { "0 PCMU/18446744073709559616",    PJMEDIA_SDP_EINRTPMAP, 0 },
    };
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(test_vec); ++i) {
        pjmedia_sdp_attr attr;
        pjmedia_sdp_rtpmap rtpmap;
        pjmedia_sdp_rtpmap *p_rtpmap;
        pj_status_t status;

        attr.name = pj_str("rtpmap");
        attr.value = pj_str(test_vec[i].value);

        status = pjmedia_sdp_attr_get_rtpmap(&attr, &rtpmap);
        PJ_TEST_EQ(status, test_vec[i].exp_status, test_vec[i].value,
                   return -10);

        /* On failure the clock rate must be left at zero, i.e. a wrapped
         * value must not be exposed to the caller.
         */
        PJ_TEST_EQ(rtpmap.clock_rate, test_vec[i].exp_clock_rate,
                   test_vec[i].value, return -20);

        /* Same expectation via the pool based variant */
        status = pjmedia_sdp_attr_to_rtpmap(pool, &attr, &p_rtpmap);
        PJ_TEST_EQ(status, test_vec[i].exp_status, test_vec[i].value,
                   return -30);
    }

    return 0;
}


static int rtcp_test(void)
{
    struct test_vec
    {
        char       *value;
        pj_status_t exp_status;
        unsigned    exp_port;
    } test_vec[] =
    {
        /* Valid */
        { "5060",                           PJ_SUCCESS, 5060 },
        { "65535",                          PJ_SUCCESS, 65535 },
        { "65535 IN IP4 127.0.0.1",         PJ_SUCCESS, 65535 },

        /* Out of range. The wrapping ones used to defeat the port range
         * check because the wrap happened before the check.
         */
        { "65536",                          PJMEDIA_SDP_EINRTCP, 0 },
        { STR_UINT32_MAX_PLUS_1,            PJMEDIA_SDP_EINRTCP, 0 },
        { STR_ULONG_MAX_PLUS_1,             PJMEDIA_SDP_EINRTCP, 0 },
        { "18446744073709556676",           PJMEDIA_SDP_EINRTCP, 0 },
    };
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(test_vec); ++i) {
        pjmedia_sdp_attr attr;
        pjmedia_sdp_rtcp_attr rtcp;
        pj_status_t status;

        attr.name = pj_str("rtcp");
        attr.value = pj_str(test_vec[i].value);

        status = pjmedia_sdp_attr_get_rtcp(&attr, &rtcp);
        PJ_TEST_EQ(status, test_vec[i].exp_status, test_vec[i].value,
                   return -110);
        PJ_TEST_EQ(rtcp.port, test_vec[i].exp_port, test_vec[i].value,
                   return -120);
    }

    return 0;
}


static int ssrc_test(void)
{
    struct test_vec
    {
        char       *value;
        pj_status_t exp_status;
        pj_uint32_t exp_ssrc;
    } test_vec[] =
    {
        /* Valid */
        { "1234567890 cname:abc",           PJ_SUCCESS, 1234567890UL },
        { STR_UINT32_MAX " cname:abc",      PJ_SUCCESS, 4294967295UL },

        /* Out of range */
        { STR_UINT32_MAX_PLUS_1 " cname:abc", PJMEDIA_SDP_EINSSRC, 0 },
        { STR_ULONG_MAX_PLUS_1 " cname:abc",  PJMEDIA_SDP_EINSSRC, 0 },
    };
    unsigned i;

    for (i=0; i<PJ_ARRAY_SIZE(test_vec); ++i) {
        pjmedia_sdp_attr attr;
        pjmedia_sdp_ssrc_attr ssrc;
        pj_status_t status;

        attr.name = pj_str("ssrc");
        attr.value = pj_str(test_vec[i].value);

        status = pjmedia_sdp_attr_get_ssrc(&attr, &ssrc);
        PJ_TEST_EQ(status, test_vec[i].exp_status, test_vec[i].value,
                   return -210);
        PJ_TEST_EQ(ssrc.ssrc, test_vec[i].exp_ssrc, test_vec[i].value,
                   return -220);
    }

    return 0;
}


int sdp_attr_test(void)
{
    pj_pool_t *pool;
    int rc;

    pool = pj_pool_create(mem, "sdp_attr_test", 4000, 4000, NULL);
    if (!pool)
        return PJ_ENOMEM;

    PJ_LOG(3,(THIS_FILE, "  rtpmap attribute"));
    rc = rtpmap_test(pool);

    if (rc == 0) {
        PJ_LOG(3,(THIS_FILE, "  rtcp attribute"));
        rc = rtcp_test();
    }

    if (rc == 0) {
        PJ_LOG(3,(THIS_FILE, "  ssrc attribute"));
        rc = ssrc_test();
    }

    pj_pool_release(pool);
    return rc;
}
