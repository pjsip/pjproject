/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pjsip/sip_multipart.h>
#include <pjsip/sip_parser.h>
#include <pjlib-util/scanner.h>
#include <pjlib-util/string.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/errno.h>
#include <pj/except.h>
#include <pj/guid.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#define THIS_FILE               "sip_multipart.c"

#define IS_SPACE(c)     ((c)==' ' || (c)=='\t')

#if 0
#   define TRACE_(x)    PJ_LOG(4,x)
#else
#   define TRACE_(x)
#endif

/* Type of "data" in multipart pjsip_msg_body */
struct multipart_data
{
    pj_str_t              boundary;
    pjsip_multipart_part  part_head;
    pj_str_t              raw_data;
};


static int multipart_print_body(struct pjsip_msg_body *msg_body,
                                char *buf, pj_size_t size)
{
    const struct multipart_data *m_data;
    pj_str_t clen_hdr =  { "Content-Length: ", 16};
    pjsip_multipart_part *part;
    char *p = buf, *end = buf+size;

#define SIZE_LEFT()     (end-p)

    m_data = (const struct multipart_data*)msg_body->data;

    PJ_ASSERT_RETURN(m_data && !pj_list_empty(&m_data->part_head), PJ_EINVAL);

    part = m_data->part_head.next;
    while (part != &m_data->part_head) {
        enum { CLEN_SPACE = 5 };
        char *clen_pos;
        const pjsip_hdr *hdr;
        pj_bool_t ctype_printed = PJ_FALSE;

        clen_pos = NULL;

        /* Print delimiter */
        if (SIZE_LEFT() <= (m_data->boundary.slen+8) << 1)
            return -1;
        *p++ = 13; *p++ = 10; *p++ = '-'; *p++ = '-';
        pj_memcpy(p, m_data->boundary.ptr, m_data->boundary.slen);
        p += m_data->boundary.slen;
        *p++ = 13; *p++ = 10;

        /* Print optional headers */
        hdr = part->hdr.next;
        while (hdr != &part->hdr) {
            int printed = pjsip_hdr_print_on((pjsip_hdr*)hdr, p,
                                             SIZE_LEFT()-2);
            if (printed < 0)
                return -1;
            p += printed;
            *p++ = '\r';
            *p++ = '\n';

            if (!ctype_printed && hdr->type == PJSIP_H_CONTENT_TYPE)
                ctype_printed = PJ_TRUE;            

            hdr = hdr->next;
        }

        /* Automaticly adds Content-Type and Content-Length headers, only
         * if content_type is set in the message body and haven't been printed.
         */
        if (part->body && part->body->content_type.type.slen && !ctype_printed) 
        {
            pj_str_t ctype_hdr = { "Content-Type: ", 14};
            const pjsip_media_type *media = &part->body->content_type;

            if (pjsip_cfg()->endpt.use_compact_form) {
                ctype_hdr.ptr = "c: ";
                ctype_hdr.slen = 3;
            }

            /* Add Content-Type header. */
            if ( (end-p) < 24 + media->type.slen + media->subtype.slen) {
                return -1;
            }
            pj_memcpy(p, ctype_hdr.ptr, ctype_hdr.slen);
            p += ctype_hdr.slen;
            p += pjsip_media_type_print(p, (unsigned)(end-p), media);
            *p++ = '\r';
            *p++ = '\n';

            /* Add Content-Length header. */
            if ((end-p) < clen_hdr.slen + 12 + 2) {
                return -1;
            }
            pj_memcpy(p, clen_hdr.ptr, clen_hdr.slen);
            p += clen_hdr.slen;

            /* Print blanks after "Content-Length:", this is where we'll put
             * the content length value after we know the length of the
             * body.
             */
            pj_memset(p, ' ', CLEN_SPACE);
            clen_pos = p;
            p += CLEN_SPACE;
            *p++ = '\r';
            *p++ = '\n';
        }

        /* Empty newline */
        *p++ = 13; *p++ = 10;

        /* Print the body */
        pj_assert(part->body != NULL);
        if (part->body) {
            int printed = part->body->print_body(part->body, p, SIZE_LEFT());
            if (printed < 0)
                return -1;
            p += printed;

            /* Now that we have the length of the body, print this to the
             * Content-Length header.
             */
            if (clen_pos) {
                char tmp[16];
                int len;

                len = pj_utoa(printed, tmp);
                if (len > CLEN_SPACE) len = CLEN_SPACE;
                pj_memcpy(clen_pos+CLEN_SPACE-len, tmp, len);
            }
        }

        part = part->next;
    }

    /* Print closing delimiter */
    if (SIZE_LEFT() < m_data->boundary.slen+8)
        return -1;
    *p++ = 13; *p++ = 10; *p++ = '-'; *p++ = '-';
    pj_memcpy(p, m_data->boundary.ptr, m_data->boundary.slen);
    p += m_data->boundary.slen;
    *p++ = '-'; *p++ = '-'; *p++ = 13; *p++ = 10;

#undef SIZE_LEFT

    return (int)(p - buf);
}

static void* multipart_clone_data(pj_pool_t *pool, const void *data,
                                  unsigned len)
{
    const struct multipart_data *src;
    struct multipart_data *dst;
    const pjsip_multipart_part *src_part;

    PJ_UNUSED_ARG(len);

    src = (const struct multipart_data*) data;
    dst = PJ_POOL_ALLOC_T(pool, struct multipart_data);
    pj_list_init(&dst->part_head);

    pj_strdup(pool, &dst->boundary, &src->boundary);

    src_part = src->part_head.next;
    while (src_part != &src->part_head) {
        pjsip_multipart_part *dst_part;
        const pjsip_hdr *src_hdr;

        dst_part = pjsip_multipart_create_part(pool);

        src_hdr = src_part->hdr.next;
        while (src_hdr != &src_part->hdr) {
            pjsip_hdr *dst_hdr = (pjsip_hdr*)pjsip_hdr_clone(pool, src_hdr);
            pj_list_push_back(&dst_part->hdr, dst_hdr);
            src_hdr = src_hdr->next;
        }

        dst_part->body = pjsip_msg_body_clone(pool, src_part->body);

        pj_list_push_back(&dst->part_head, dst_part);

        src_part = src_part->next;
    }

    return (void*)dst;
}

/*
 * Create an empty multipart body.
 */
PJ_DEF(pjsip_msg_body*) pjsip_multipart_create( pj_pool_t *pool,
                                                const pjsip_media_type *ctype,
                                                const pj_str_t *boundary)
{
    pjsip_msg_body *body;
    pjsip_param *ctype_param;
    struct multipart_data *mp_data;
    pj_str_t STR_BOUNDARY = { "boundary", 8 };

    PJ_ASSERT_RETURN(pool, NULL);

    body = PJ_POOL_ZALLOC_T(pool, pjsip_msg_body);

    /* content-type */
    if (ctype && ctype->type.slen) {
        pjsip_media_type_cp(pool, &body->content_type, ctype);
    } else {
        pj_str_t STR_MULTIPART = {"multipart", 9};
        pj_str_t STR_MIXED = { "mixed", 5 };

        pjsip_media_type_init(&body->content_type,
                              &STR_MULTIPART, &STR_MIXED);
    }

    /* multipart data */
    mp_data = PJ_POOL_ZALLOC_T(pool, struct multipart_data);
    pj_list_init(&mp_data->part_head);
    if (boundary) {
        pj_strdup(pool, &mp_data->boundary, boundary);
    } else {
        pj_create_unique_string(pool, &mp_data->boundary);
    }
    body->data = mp_data;

    /* Add ";boundary" parameter to content_type parameter. */
    ctype_param = pjsip_param_find(&body->content_type.param, &STR_BOUNDARY);
    if (!ctype_param) {
        ctype_param = PJ_POOL_ALLOC_T(pool, pjsip_param);
        ctype_param->name = STR_BOUNDARY;
        pj_list_push_back(&body->content_type.param, ctype_param);
    }
    ctype_param->value = mp_data->boundary;

    /* function pointers */
    body->print_body = &multipart_print_body;
    body->clone_data = &multipart_clone_data;

    return body;
}

/*
 * Create an empty multipart part.
 */
PJ_DEF(pjsip_multipart_part*) pjsip_multipart_create_part(pj_pool_t *pool)
{
    pjsip_multipart_part *mp;

    mp = PJ_POOL_ZALLOC_T(pool, pjsip_multipart_part);
    pj_list_init(&mp->hdr);

    return mp;
}


/*
 * Deep clone.
 */
PJ_DEF(pjsip_multipart_part*)
pjsip_multipart_clone_part(pj_pool_t *pool,
                           const pjsip_multipart_part *src)
{
    pjsip_multipart_part *dst;
    const pjsip_hdr *hdr;

    dst = pjsip_multipart_create_part(pool);

    hdr = src->hdr.next;
    while (hdr != &src->hdr) {
        pj_list_push_back(&dst->hdr, pjsip_hdr_clone(pool, hdr));
        hdr = hdr->next;
    }

    dst->body = pjsip_msg_body_clone(pool, src->body);

    return dst;
}


/*
 * Add a part into multipart bodies.
 */
PJ_DEF(pj_status_t) pjsip_multipart_add_part( pj_pool_t *pool,
                                              pjsip_msg_body *mp,
                                              pjsip_multipart_part *part)
{
    struct multipart_data *m_data;

    /* All params must be specified */
    PJ_ASSERT_RETURN(pool && mp && part, PJ_EINVAL);

    /* mp must really point to an actual multipart msg body */
    PJ_ASSERT_RETURN(mp->print_body==&multipart_print_body, PJ_EINVAL);

    /* The multipart part must contain a valid message body */
    PJ_ASSERT_RETURN(part->body && part->body->print_body, PJ_EINVAL);

    m_data = (struct multipart_data*)mp->data;
    pj_list_push_back(&m_data->part_head, part);

    PJ_UNUSED_ARG(pool);

    return PJ_SUCCESS;
}

/*
 * Get the first part of multipart bodies.
 */
PJ_DEF(pjsip_multipart_part*)
pjsip_multipart_get_first_part(const pjsip_msg_body *mp)
{
    struct multipart_data *m_data;

    /* Must specify mandatory params */
    PJ_ASSERT_RETURN(mp, NULL);

    /* mp must really point to an actual multipart msg body */
    PJ_ASSERT_RETURN(mp->print_body==&multipart_print_body, NULL);

    m_data = (struct multipart_data*)mp->data;
    if (pj_list_empty(&m_data->part_head))
        return NULL;

    return m_data->part_head.next;
}

/*
 * Get the next part after the specified part.
 */
PJ_DEF(pjsip_multipart_part*)
pjsip_multipart_get_next_part(const pjsip_msg_body *mp,
                              pjsip_multipart_part *part)
{
    struct multipart_data *m_data;

    /* Must specify mandatory params */
    PJ_ASSERT_RETURN(mp && part, NULL);

    /* mp must really point to an actual multipart msg body */
    PJ_ASSERT_RETURN(mp->print_body==&multipart_print_body, NULL);

    m_data = (struct multipart_data*)mp->data;

    /* the part parameter must be really member of the list */
    PJ_ASSERT_RETURN(pj_list_find_node(&m_data->part_head, part) != NULL,
                     NULL);

    if (part->next == &m_data->part_head)
        return NULL;

    return part->next;
}

/*
 * Find a body inside multipart bodies which has the specified content type.
 */
PJ_DEF(pjsip_multipart_part*)
pjsip_multipart_find_part( const pjsip_msg_body *mp,
                           const pjsip_media_type *content_type,
                           const pjsip_multipart_part *start)
{
    struct multipart_data *m_data;
    pjsip_multipart_part *part;

    /* Must specify mandatory params */
    PJ_ASSERT_RETURN(mp && content_type, NULL);

    /* mp must really point to an actual multipart msg body */
    PJ_ASSERT_RETURN(mp->print_body==&multipart_print_body, NULL);

    m_data = (struct multipart_data*)mp->data;

    if (start)
        part = start->next;
    else
        part = m_data->part_head.next;

    while (part != &m_data->part_head) {
        if (pjsip_media_type_cmp(&part->body->content_type,
                                 content_type, 0)==0)
        {
            return part;
        }
        part = part->next;
    }

    return NULL;
}

/*
 * Find a body inside multipart bodies which has the header and value.
 */
PJ_DEF(pjsip_multipart_part*)
pjsip_multipart_find_part_by_header_str(pj_pool_t *pool,
                                    const pjsip_msg_body *mp,
                                    const pj_str_t *hdr_name,
                                    const pj_str_t *hdr_value,
                                    const pjsip_multipart_part *start)
{
    struct multipart_data *m_data;
    pjsip_multipart_part *part;
    pjsip_hdr *found_hdr;
    pj_str_t found_hdr_str;
    pj_str_t found_hdr_value;
    pj_size_t expected_hdr_slen;
    pj_size_t buf_size;
    pj_ssize_t hdr_name_len;
#define REASONABLE_PADDING 32
#define SEPARATOR_LEN 2
    /* Must specify mandatory params */
    PJ_ASSERT_RETURN(mp && hdr_name && hdr_value, NULL);

    /* mp must really point to an actual multipart msg body */
    PJ_ASSERT_RETURN(mp->print_body==&multipart_print_body, NULL);

    /*
     * We'll need to "print" each header we find to test it but
     * allocating a buffer of PJSIP_MAX_URL_SIZE is overkill.
     * Instead, we'll allocate one large enough to hold the search
     * header name, the ": " separator, the search hdr value, and
     * the NULL terminator.  If we can't print the found header
     * into that buffer then it can't be a match.
     *
     * Some header print functions such as generic_int require enough
     * space to print the maximum possible header length so we'll
     * add a reasonable amount to the print buffer size.
     */
    expected_hdr_slen = hdr_name->slen + SEPARATOR_LEN + hdr_value->slen;
    buf_size = expected_hdr_slen + REASONABLE_PADDING;
    found_hdr_str.ptr = pj_pool_alloc(pool, buf_size);
    found_hdr_str.slen = 0;
    hdr_name_len = hdr_name->slen + SEPARATOR_LEN;

    m_data = (struct multipart_data*)mp->data;

    if (start)
        part = start->next;
    else
        part = m_data->part_head.next;

    while (part != &m_data->part_head) {
        found_hdr = NULL;
        while ((found_hdr = pjsip_hdr_find_by_name(&part->hdr, hdr_name,
            (found_hdr ? found_hdr->next : NULL))) != NULL) {

            found_hdr_str.slen = pjsip_hdr_print_on((void*) found_hdr, found_hdr_str.ptr, buf_size);
            /*
             * If the buffer was too small (slen = -1) or the result wasn't
             * the same length as the search header, it can't be a match.
             */
            if (found_hdr_str.slen != (pj_ssize_t)expected_hdr_slen) {
                continue;
            }
            /*
             * Set the value overlay to start at the found header value...
             */
            found_hdr_value.ptr = found_hdr_str.ptr + hdr_name_len;
            found_hdr_value.slen = found_hdr_str.slen - hdr_name_len;
            /* ...and compare it to the supplied header value. */
            if (pj_strcmp(hdr_value, &found_hdr_value) == 0) {
                return part;
            }
        }
        part = part->next;
    }
    return NULL;
#undef SEPARATOR_LEN
#undef REASONABLE_PADDING
}

PJ_DEF(pjsip_multipart_part*)
pjsip_multipart_find_part_by_header(pj_pool_t *pool,
                                    const pjsip_msg_body *mp,
                                    void *search_for,
                                    const pjsip_multipart_part *start)
{
    pjsip_hdr *search_hdr = search_for;
    pj_str_t search_buf;

    /* Must specify mandatory params */
    PJ_ASSERT_RETURN(mp && search_hdr, NULL);

    /* mp must really point to an actual multipart msg body */
    PJ_ASSERT_RETURN(mp->print_body==&multipart_print_body, NULL);

    /*
     * Unfortunately, there isn't enough information to determine
     * the maximum printed size of search_hdr at this point so we
     * have to allocate a reasonable max.
     */
    search_buf.ptr = pj_pool_alloc(pool, PJSIP_MAX_URL_SIZE);
    search_buf.slen = pjsip_hdr_print_on(search_hdr, search_buf.ptr, PJSIP_MAX_URL_SIZE - 1);
    if (search_buf.slen <= 0) {
        return NULL;
    }
    /*
     * Set the header value to start after the header name plus the ":", then
     * strip leading and trailing whitespace.
     */
    search_buf.ptr += (search_hdr->name.slen + 1);
    search_buf.slen -= (search_hdr->name.slen + 1);
    pj_strtrim(&search_buf);

    return pjsip_multipart_find_part_by_header_str(pool, mp, &search_hdr->name, &search_buf, start);
}

/*
 * Convert a Content-ID URI to it's corresponding header value.
 * RFC2392 says...
 * A "cid" URL is converted to the corresponding Content-ID message
 * header by removing the "cid:" prefix, converting the % encoded
 * character(s) to their equivalent US-ASCII characters, and enclosing
 * the remaining parts with an angle bracket pair, "<" and ">".
 *
 * This implementation will accept URIs with or without the "cid:"
 * scheme and optional angle brackets.
 */
static pj_str_t cid_uri_to_hdr_value(pj_pool_t *pool, pj_str_t *cid_uri)
{
    pj_size_t cid_len = pj_strlen(cid_uri);
    pj_size_t alloc_len = cid_len + 2 /* for the leading and trailing angle brackets */;
    pj_str_t uri_overlay;
    pj_str_t cid_hdr;
    pj_str_t hdr_overlay;

    pj_strassign(&uri_overlay, cid_uri);
    /* If the URI is already enclosed in angle brackets, remove them. */
    if (uri_overlay.ptr[0] == '<') {
        uri_overlay.ptr++;
        uri_overlay.slen -= 2;
    }
    /* If the URI starts with the "cid:" scheme, skip over it. */
    if (pj_strncmp2(&uri_overlay, "cid:", 4) == 0) {
        uri_overlay.ptr += 4;
        uri_overlay.slen -= 4;
    }
    /* Start building */
    cid_hdr.ptr = pj_pool_alloc(pool, alloc_len);
    cid_hdr.ptr[0] = '<';
    cid_hdr.slen = 1;
    hdr_overlay.ptr = cid_hdr.ptr + 1;
    hdr_overlay.slen = 0;
    pj_strcpy_unescape(&hdr_overlay, &uri_overlay);
    cid_hdr.slen += hdr_overlay.slen;
    cid_hdr.ptr[cid_hdr.slen] = '>';
    cid_hdr.slen++;

    return cid_hdr;
}

PJ_DEF(pjsip_multipart_part*)
pjsip_multipart_find_part_by_cid_str(pj_pool_t *pool,
                                 const pjsip_msg_body *mp,
                                 pj_str_t *cid)
{
    struct multipart_data *m_data;
    pjsip_multipart_part *part;
    pjsip_generic_string_hdr *found_hdr;
    static pj_str_t hdr_name = { "Content-ID", 10};
    pj_str_t hdr_value;

    PJ_ASSERT_RETURN(pool && mp && cid && (pj_strlen(cid) > 0), NULL);

    hdr_value = cid_uri_to_hdr_value(pool, cid);
    if (pj_strlen(&hdr_value) == 0) {
        return NULL;
    }

    m_data = (struct multipart_data*)mp->data;
    part = m_data->part_head.next;

    while (part != &m_data->part_head) {
        found_hdr = NULL;
        while ((found_hdr = pjsip_hdr_find_by_name(&part->hdr, &hdr_name,
            (found_hdr ? found_hdr->next : NULL))) != NULL) {
            if (pj_strcmp(&hdr_value, &found_hdr->hvalue) == 0) {
                return part;
            }
        }
        part = part->next;
    }
    return NULL;
}

PJ_DEF(pjsip_multipart_part*)
pjsip_multipart_find_part_by_cid_uri(pj_pool_t *pool,
                                 const pjsip_msg_body *mp,
                                 pjsip_other_uri *cid_uri)
{
    PJ_ASSERT_RETURN(pool && mp && cid_uri, NULL);

    if (pj_strcmp2(&cid_uri->scheme, "cid") != 0) {
        return NULL;
    }
    /*
     * We only need to pass the URI content so we
     * can do that directly.
     */
    return pjsip_multipart_find_part_by_cid_str(pool, mp, &cid_uri->content);
}

/* Parse a multipart part. "pct" is parent content-type  */
static pjsip_multipart_part *parse_multipart_part(pj_pool_t *pool,
                                                  char *start,
                                                  pj_size_t len,
                                                  const pjsip_media_type *pct)
{
    pjsip_multipart_part *part = pjsip_multipart_create_part(pool);
    char *p = start, *end = start+len, *end_hdr = NULL, *start_body = NULL;
    pjsip_ctype_hdr *ctype_hdr = NULL;

    TRACE_((THIS_FILE, "Parsing part: begin--\n%.*s\n--end",
            (int)len, start));

    /* Find the end of header area, by looking at an empty line */
    for (;;) {
        while (p!=end && *p!='\n') ++p;
        if (p==end) {
            start_body = end;
            break;
        }
        if ((p==start) || (p==start+1 && *(p-1)=='\r')) {
            /* Empty header section */
            end_hdr = start;
            start_body = ++p;
            break;
        } else if (p==end-1) {
            /* Empty body section */
            end_hdr = end;
            start_body = ++p;
        } else if ((p>=start+1 && *(p-1)=='\n') ||
                   (p>=start+2 && *(p-1)=='\r' && *(p-2)=='\n'))
        {
            /* Found it */
            end_hdr = (*(p-1)=='\r') ? (p-1) : p;
            start_body = ++p;
            break;
        } else {
            ++p;
        }
    }

    /* Parse the headers */
    if (end_hdr-start > 0) {
        pjsip_hdr *hdr;
        pj_status_t status;

        status = pjsip_parse_headers(pool, start, end_hdr-start, 
                                     &part->hdr, 0);
        if (status != PJ_SUCCESS) {
            PJ_PERROR(2,(THIS_FILE, status, "Warning: error parsing multipart"
                                            " header"));
        }

        /* Find Content-Type header */
        hdr = part->hdr.next;
        while (hdr != &part->hdr) {
            TRACE_((THIS_FILE, "Header parsed: %.*s", (int)hdr->name.slen,
                    hdr->name.ptr));
            if (hdr->type == PJSIP_H_CONTENT_TYPE) {
                ctype_hdr = (pjsip_ctype_hdr*)hdr;
            }
            hdr = hdr->next;
        }
    }

    /* Assign the body */
    part->body = PJ_POOL_ZALLOC_T(pool, pjsip_msg_body);
    if (ctype_hdr) {
        pjsip_media_type_cp(pool, &part->body->content_type, &ctype_hdr->media);
    } else if (pct && pj_stricmp2(&pct->subtype, "digest")==0) {
        part->body->content_type.type = pj_str("message");
        part->body->content_type.subtype = pj_str("rfc822");
    } else {
        part->body->content_type.type = pj_str("text");
        part->body->content_type.subtype = pj_str("plain");
    }

    if (start_body < end) {
        part->body->data = start_body;
        part->body->len = (unsigned)(end - start_body);
    } else {
        part->body->data = (void*)"";
        part->body->len = 0;
    }
    TRACE_((THIS_FILE, "Body parsed: \"%.*s\"", (int)part->body->len,
            part->body->data));
    part->body->print_body = &pjsip_print_text_body;
    part->body->clone_data = &pjsip_clone_text_data;

    return part;
}

/* Public function to parse multipart message bodies into its parts */
PJ_DEF(pjsip_msg_body*) pjsip_multipart_parse(pj_pool_t *pool,
                                              char *buf, pj_size_t len,
                                              const pjsip_media_type *ctype,
                                              unsigned options)
{
    pj_str_t boundary, delim;
    char *curptr, *endptr;
    const pjsip_param *ctype_param;
    const pj_str_t STR_BOUNDARY = { "boundary", 8 };
    pjsip_msg_body *body = NULL;

    PJ_ASSERT_RETURN(pool && buf && len && ctype && !options, NULL);

    TRACE_((THIS_FILE, "Started parsing multipart body"));

    /* Get the boundary value in the ctype */
    boundary.ptr = NULL;
    boundary.slen = 0;
    ctype_param = pjsip_param_find(&ctype->param, &STR_BOUNDARY);
    if (ctype_param) {
        boundary = ctype_param->value;
        if (boundary.slen>2 && *boundary.ptr=='"') {
            /* Remove quote */
            boundary.ptr++;
            boundary.slen -= 2;
        }
        TRACE_((THIS_FILE, "Boundary is specified: '%.*s'", (int)boundary.slen,
                boundary.ptr));
    }

    if (!boundary.slen) {
        /* Boundary not found or not specified. Try to be clever, get
         * the boundary from the body.
         */
        char *p=buf, *end=buf+len;

        PJ_LOG(4,(THIS_FILE, "Warning: boundary parameter not found or "
                             "not specified when parsing multipart body"));

        /* Find the first "--". This "--" must be right after a CRLF, unless
         * it really appears at the start of the buffer.
         */
        for (;;) {
            while (p!=end && *p!='-') ++p;

            if (p == end)
                break;

            if ((p+1<end) && *(p+1)=='-' &&
                ((p>buf && *(p-1)=='\n') || (p==buf)))
            {
                p+=2;
                break;
            } else {
                ++p;
            }
        }

        if (p==end) {
            /* Unable to determine boundary. Maybe this is not a multipart
             * message?
             */
            PJ_LOG(4,(THIS_FILE, "Error: multipart boundary not specified and"
                                 " unable to calculate from the body"));
            return NULL;
        }

        boundary.ptr = p;
        while (p!=end && !pj_isspace(*p)) ++p;
        boundary.slen = p - boundary.ptr;

        TRACE_((THIS_FILE, "Boundary is calculated: '%.*s'",
                (int)boundary.slen, boundary.ptr));
    }


    /* Build the delimiter:
     *   delimiter = "--" boundary
     */
    delim.slen = boundary.slen+2;
    delim.ptr = (char*)pj_pool_alloc(pool, (int)delim.slen);
    delim.ptr[0] = '-';
    delim.ptr[1] = '-';
    pj_memcpy(delim.ptr+2, boundary.ptr, boundary.slen);

    /* Start parsing the body, skip until the first delimiter. */
    curptr = buf;
    endptr = buf + len;
    {
        pj_str_t strbody;

        strbody.ptr = buf; strbody.slen = len;
        curptr = pj_strstr(&strbody, &delim);
        if (!curptr)
            return NULL;
    }

    body = pjsip_multipart_create(pool, ctype, &boundary);

    /* Save full raw body */
    {
        struct multipart_data *mp = (struct multipart_data*)body->data;
        pj_strset(&mp->raw_data, buf, len);
    }

    for (;;) {
        char *start_body, *end_body;
        pjsip_multipart_part *part;

        /* Eat the boundary */
        curptr += delim.slen;
        if (*curptr=='-' && curptr<endptr-1 && *(curptr+1)=='-') {
            /* Found the closing delimiter */
            curptr += 2;
            break;
        }
        /* Optional whitespace after delimiter */
        while (curptr!=endptr && IS_SPACE(*curptr)) ++curptr;
        /* Mandatory CRLF */
        if (*curptr=='\r') ++curptr;
        if (*curptr!='\n') {
            /* Expecting a newline here */
            PJ_LOG(2, (THIS_FILE, "Failed to find newline"));

            return NULL;
        }
        ++curptr;

        /* We now in the start of the body */
        start_body = curptr;

        /* Find the next delimiter */
        {
            pj_str_t subbody;

            subbody.ptr = curptr; subbody.slen = endptr - curptr;
            curptr = pj_strstr(&subbody, &delim);
            if (!curptr) {
                /* We're really expecting end delimiter to be found. */
                PJ_LOG(2, (THIS_FILE, "Failed to find end-delimiter"));
                return NULL;
            }
        }

        end_body = curptr;

        /* Note that when body is empty, end_body will be equal
         * to start_body.
         */
        if (end_body > start_body) {
            /* The newline preceeding the delimiter is conceptually part of
             * the delimiter, so trim it from the body.
             */
            if (*(end_body-1) == '\n')
                --end_body;
            if (end_body > start_body && *(end_body-1) == '\r')
                --end_body;
        }

        /* Now that we have determined the part's boundary, parse it
         * to get the header and body part of the part.
         */
        part = parse_multipart_part(pool, start_body, end_body - start_body,
                                    ctype);
        if (part) {
            TRACE_((THIS_FILE, "Adding part"));
            pjsip_multipart_add_part(pool, body, part);
        } else {
            PJ_LOG(2, (THIS_FILE, "Failed to add part"));
        }
    }
    TRACE_((THIS_FILE, "pjsip_multipart_parse finished: %p", body));

    return body;
}


PJ_DEF(pj_status_t) pjsip_multipart_get_raw( pjsip_msg_body *mp,
                                             pj_str_t *boundary,
                                             pj_str_t *raw_data)
{
    struct multipart_data *m_data;

    /* Must specify mandatory params */
    PJ_ASSERT_RETURN(mp, PJ_EINVAL);

    /* mp must really point to an actual multipart msg body */
    PJ_ASSERT_RETURN(mp->print_body==&multipart_print_body, PJ_EINVAL);

    m_data = (struct multipart_data*)mp->data;

    if (boundary)
        *boundary = m_data->boundary;

    if (raw_data)
        *raw_data = m_data->raw_data;

    return PJ_SUCCESS;
}
