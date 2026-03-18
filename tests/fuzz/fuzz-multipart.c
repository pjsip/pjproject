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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pjlib.h>
#include <pjsip.h>
#include <pjsip/sip_multipart.h>

pj_pool_factory *mem;

/* Parse multipart SIP message */
int multipart_parse(uint8_t *data, size_t size) {
    pj_pool_t *pool;
    char *msg_buf;
    pjsip_parser_err_report err_list;
    pjsip_msg *msg;
    pjsip_msg_body *body;
    pjsip_msg_body *multipart_body;
    size_t msg_size;

    pool = pj_pool_create(mem, "multipart", 4096, 1024, NULL);

    /* SIP message template with multipart body */
    const char *sip_header = 
        "INVITE sip:user@example.com SIP/2.0\r\n"
        "From: <sip:caller@example.com>\r\n"
        "To: <sip:user@example.com>\r\n"
        "Call-ID: test@example.com\r\n"
        "CSeq: 1 INVITE\r\n"
        "Via: SIP/2.0/UDP example.com\r\n"
        "Content-Type: multipart/mixed ; boundary=\"BOUNDARY123\"\r\n"
        "Content-Length: ";
    
    const char *boundary_start = "--BOUNDARY123\r\n";
    const char *part_header = "Content-Type: text/plain\r\n\r\n";
    const char *boundary_end = "\r\n--BOUNDARY123--\r\n";
    
    size_t header_len = strlen(sip_header);
    size_t boundary_start_len = strlen(boundary_start);
    size_t part_header_len = strlen(part_header);
    size_t boundary_end_len = strlen(boundary_end);

    /* Compute body size */
    size_t body_len = boundary_start_len + part_header_len + size + boundary_end_len;

    char cl_buf[20];
    int cl_len = pj_ansi_snprintf(cl_buf, sizeof(cl_buf), "%zu", body_len);

    if (cl_len <= 0 || cl_len >= (int)sizeof(cl_buf)) {
        pj_pool_release(pool);
        return 0;
    }

    /* Allocate message buffer */
    msg_size = header_len + cl_len + 4 + body_len;

    msg_buf = (char*)pj_pool_calloc(pool, msg_size + 1, 1);

    /* Build complete message */
    size_t pos = 0;
    pj_memcpy(msg_buf + pos, sip_header, header_len);
    pos += header_len;
    pj_memcpy(msg_buf + pos, cl_buf, cl_len);
    pos += cl_len;
    pj_memcpy(msg_buf + pos, "\r\n\r\n", 4);
    pos += 4;

    /* Multipart body */
    pj_memcpy(msg_buf + pos, boundary_start, boundary_start_len);
    pos += boundary_start_len;
    pj_memcpy(msg_buf + pos, part_header, part_header_len);
    pos += part_header_len;

    if (size > 0) {
        pj_memcpy(msg_buf + pos, data, size);
        pos += size;
    }

    pj_memcpy(msg_buf + pos, boundary_end, boundary_end_len);
    pos += boundary_end_len;

    /* Parse SIP message */
    pj_list_init(&err_list);
    msg = pjsip_parse_msg(pool, msg_buf, pos, &err_list);

    if (!msg || !msg->body) {
        pj_pool_release(pool);
        return 0;
    }

    body = msg->body;

    /* Parse multipart body */
    if (body->content_type.type.slen > 0 &&
        pj_stricmp2(&body->content_type.type, "multipart") == 0)
    {
        multipart_body = pjsip_multipart_parse(pool, (char *)body->data,
                                               body->len, &body->content_type, 0);

        if (multipart_body) {
            /* Process parts */
            pjsip_multipart_part *part = pjsip_multipart_get_first_part(multipart_body);
            while (part) {
                if (part->body && part->body->data && part->body->len > 0) {
                    (void)part->body->data;
                }
                part = pjsip_multipart_get_next_part(multipart_body, part);
            }
        }
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
    static pj_bool_t initialized = PJ_FALSE;

    data = (uint8_t *)calloc((Size+1), sizeof(uint8_t));
    memcpy((void *)data, (void *)Data, Size);

    /* Initialise PJLIB once */
    if (!initialized) {
        pj_init();
        pj_caching_pool_init(&caching_pool, &pj_pool_factory_default_policy, 0);
        pj_log_set_level(0);
        mem = &caching_pool.factory;
        initialized = PJ_TRUE;
    }

    ret = multipart_parse(data, Size);

    free(data);

    return ret;
}
