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
#include <pjlib-util.h>
#include <pjlib.h>
#include "test.h"

#define THIS_FILE "dns_packet_test.c"

struct dns_case
{
    const char *name;
    const pj_uint8_t *packet;
    unsigned size;
    pj_bool_t accept;
};

static const pj_uint8_t valid_root_query[] = {
    0x00,0x01, 0x01,0x00, 0x00,0x01, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00, 0x00,0x01, 0x00,0x01
};
static const pj_uint8_t valid_compressed_a[] = {
    0x00,0x02, 0x81,0x80, 0x00,0x01, 0x00,0x01,
    0x00,0x00, 0x00,0x00, 0x03,'s','i','p', 0x07,'e','x','a','m','p','l','e',
    0x03,'c','o','m',0x00, 0x00,0x01,0x00,0x01,
    0xc0,0x0c, 0x00,0x01,0x00,0x01, 0x00,0x00,0x00,0x3c,
    0x00,0x04, 0xc0,0x00,0x02,0x01
};
static const pj_uint8_t short_header[] = { 0x00 };
static const pj_uint8_t missing_question[] = {
    0,1, 0,0, 0,1, 0,0, 0,0, 0,0
};
static const pj_uint8_t truncated_question_fields[] = {
    0,2, 0,0, 0,1, 0,0, 0,0, 0,0, 0, 0,1
};
static const pj_uint8_t incomplete_pointer[] = {
    0,3, 0,0, 0,1, 0,0, 0,0, 0,0, 0xc0
};
static const pj_uint8_t invalid_pointer[] = {
    0,4, 0,0, 0,1, 0,0, 0,0, 0,0, 0xc0,0xff, 0,1,0,1
};
static const pj_uint8_t pointer_cycle[] = {
    0,5, 0,0, 0,1, 0,0, 0,0, 0,0, 0xc0,0x0c, 0,1,0,1
};
static const pj_uint8_t short_a_rdata[] = {
    0,6, 0x81,0x80, 0,0, 0,1, 0,0, 0,0,
    0, 0,1, 0,1, 0,0,0,0, 0,3, 'a','b','c'
};
static const pj_uint8_t short_aaaa_rdata[] = {
    0,7, 0x81,0x80, 0,0, 0,1, 0,0, 0,0,
    0, 0,28, 0,1, 0,0,0,0, 0,15,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
static const pj_uint8_t short_srv_rdata[] = {
    0,8, 0x81,0x80, 0,0, 0,1, 0,0, 0,0,
    0, 0,33, 0,1, 0,0,0,0, 0,5, 0,1,0,2,0
};
static const pj_uint8_t incomplete_cname_pointer[] = {
    0,9, 0x81,0x80, 0,0, 0,1, 0,0, 0,0,
    0, 0,5, 0,1, 0,0,0,0, 0,1, 0xc0
};
static const pj_uint8_t excessive_count[] = {
    0,10, 0,0, 0xff,0xff, 0,0, 0,0, 0,0
};

int dns_packet_test(void)
{
    static const struct dns_case cases[] = {
        {"valid root query", valid_root_query, sizeof(valid_root_query), PJ_TRUE},
        {"valid compressed A", valid_compressed_a, sizeof(valid_compressed_a), PJ_TRUE},
        {"short header", short_header, sizeof(short_header), PJ_FALSE},
        {"missing question", missing_question, sizeof(missing_question), PJ_FALSE},
        {"truncated question fields", truncated_question_fields,
         sizeof(truncated_question_fields), PJ_FALSE},
        {"incomplete pointer", incomplete_pointer, sizeof(incomplete_pointer), PJ_FALSE},
        {"invalid pointer", invalid_pointer, sizeof(invalid_pointer), PJ_FALSE},
        {"pointer cycle", pointer_cycle, sizeof(pointer_cycle), PJ_FALSE},
        {"short A RDATA", short_a_rdata, sizeof(short_a_rdata), PJ_FALSE},
        {"short AAAA RDATA", short_aaaa_rdata, sizeof(short_aaaa_rdata), PJ_FALSE},
        {"short SRV RDATA", short_srv_rdata, sizeof(short_srv_rdata), PJ_FALSE},
        {"incomplete CNAME pointer", incomplete_cname_pointer,
         sizeof(incomplete_cname_pointer), PJ_FALSE},
        {"excessive section count", excessive_count, sizeof(excessive_count), PJ_FALSE}
    };
    unsigned i;

    for (i = 0; i < PJ_ARRAY_SIZE(cases); ++i) {
        pj_pool_t *pool;
        pj_dns_parsed_packet *parsed = NULL;
        pj_status_t status;

        pool = pj_pool_create(mem, "dns-packet", 2048, 2048, NULL);
        if (!pool)
            return -100;

        status = pj_dns_parse_packet(pool, cases[i].packet, cases[i].size,
                                     &parsed);
        pj_pool_release(pool);

        if ((status == PJ_SUCCESS) != cases[i].accept) {
            PJ_LOG(1,(THIS_FILE, "case '%s' unexpectedly %s (status=%d)",
                      cases[i].name,
                      status == PJ_SUCCESS ? "succeeded" : "failed",
                      status));
            return -(int)i - 1;
        }
    }

    return 0;
}
