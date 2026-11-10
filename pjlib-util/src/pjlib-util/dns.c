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
#include <pjlib-util/dns.h>
#include <pjlib-util/errno.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/string.h>


/* Maximum number of DNS name compression pointers to follow.
 * RFC 1035 compression in practice never exceeds 2-3 levels.
 */
#define MAX_NAME_POINTER_HOPS  5

/* Maximum length of an expanded dotted name without a trailing dot */
#define MAX_DNS_NAME_LENGTH  253

/* Invariant: pos <= end <= packet_size.
 * packet_size bounds compression targets, while end may restrict reads at
 * the original location to a query or RDATA extent.
 */
typedef struct dns_cursor
{
    const pj_uint8_t *packet;
    pj_size_t packet_size;
    pj_size_t pos;
    pj_size_t end;
} dns_cursor;

static pj_size_t cursor_remaining(const dns_cursor *cursor)
{
    return cursor->end - cursor->pos;
}

static pj_status_t cursor_take(dns_cursor *cursor, pj_size_t size,
                               const pj_uint8_t **data)
{
    if (cursor->pos > cursor->end || size > cursor->end - cursor->pos)
        return PJLIB_UTIL_EDNSINSIZE;

    if (data)
        *data = cursor->packet + cursor->pos;
    cursor->pos += size;
    return PJ_SUCCESS;
}

static pj_status_t cursor_read8(dns_cursor *cursor, pj_uint8_t *value)
{
    const pj_uint8_t *data;
    pj_status_t status;

    status = cursor_take(cursor, 1, &data);
    if (status != PJ_SUCCESS)
        return status;
    *value = data[0];
    return PJ_SUCCESS;
}

static pj_status_t cursor_read16(dns_cursor *cursor, pj_uint16_t *value)
{
    const pj_uint8_t *data;
    pj_status_t status;

    status = cursor_take(cursor, 2, &data);
    if (status != PJ_SUCCESS)
        return status;
    *value = (pj_uint16_t)((data[0] << 8) | data[1]);
    return PJ_SUCCESS;
}

static pj_status_t cursor_read32(dns_cursor *cursor, pj_uint32_t *value)
{
    const pj_uint8_t *data;
    pj_status_t status;

    status = cursor_take(cursor, 4, &data);
    if (status != PJ_SUCCESS)
        return status;
    *value = ((pj_uint32_t)data[0] << 24) |
             ((pj_uint32_t)data[1] << 16) |
             ((pj_uint32_t)data[2] << 8) | data[3];
    return PJ_SUCCESS;
}

static pj_status_t cursor_read_header(dns_cursor *cursor, pj_dns_hdr *header)
{
    const pj_uint8_t *data;
    pj_status_t status;

    status = cursor_take(cursor, sizeof(*header), &data);
    if (status != PJ_SUCCESS)
        return status;

    pj_memcpy(header, data, sizeof(*header));
    header->id = pj_ntohs(header->id);
    header->flags = pj_ntohs(header->flags);
    header->qdcount = pj_ntohs(header->qdcount);
    header->anscount = pj_ntohs(header->anscount);
    header->nscount = pj_ntohs(header->nscount);
    header->arcount = pj_ntohs(header->arcount);
    return PJ_SUCCESS;
}

static pj_status_t cursor_read(dns_cursor *cursor, void *dst, pj_size_t size)
{
    const pj_uint8_t *data;
    pj_status_t status;

    status = cursor_take(cursor, size, &data);
    if (status != PJ_SUCCESS)
        return status;
    if (size)
        pj_memcpy(dst, data, size);
    return PJ_SUCCESS;
}

static pj_status_t cursor_child(dns_cursor *cursor, pj_size_t size,
                                dns_cursor *child)
{
    if (cursor->pos > cursor->end || size > cursor->end - cursor->pos)
        return PJLIB_UTIL_EDNSINSIZE;

    *child = *cursor;
    child->end = child->pos + size;
    cursor->pos += size;
    return PJ_SUCCESS;
}

static pj_status_t emit_name(char *dst, pj_size_t capacity,
                             pj_size_t *length, const pj_uint8_t *data,
                             pj_size_t size)
{
    if (size > capacity - *length)
        return PJ_ENAMETOOLONG;

    if (dst && size)
        pj_memcpy(dst + *length, data, size);
    *length += size;
    return PJ_SUCCESS;
}

/* Decode a name for both measurement and bounded copying. Only bytes encoded
 * at the origin advance origin; compression targets use packet-wide bounds.
 */
static pj_status_t decode_name(dns_cursor *origin, char *dst,
                               pj_size_t capacity, pj_size_t *name_len)
{
    dns_cursor scan = *origin;
    pj_bool_t at_origin = PJ_TRUE;
    unsigned pointer_count = 0;
    pj_size_t length = 0;

    for (;;) {
        const pj_uint8_t *label;
        pj_uint8_t octet;
        pj_status_t status;

        status = cursor_read8(&scan, &octet);
        if (status != PJ_SUCCESS)
            return PJLIB_UTIL_EDNSINNAMEPTR;
        if (at_origin)
            origin->pos = scan.pos;

        if (octet == 0)
            break;

        if ((octet & 0xc0) == 0xc0) {
            pj_uint8_t low;
            pj_size_t offset;

            /* Compression is found, consume both bytes at the origin */
            status = cursor_read8(&scan, &low);
            if (status != PJ_SUCCESS)
                return PJLIB_UTIL_EDNSINNAMEPTR;
            if (at_origin)
                origin->pos = scan.pos;

            /* Get the 14-bit offset and validate it against the packet */
            offset = ((pj_size_t)(octet & 0x3f) << 8) | low;
            /* Limit the number of compression pointers followed */
            if (offset >= scan.packet_size ||
                ++pointer_count > MAX_NAME_POINTER_HOPS)
            {
                return PJLIB_UTIL_EDNSINNAMEPTR;
            }

            scan.pos = offset;
            scan.end = scan.packet_size;
            at_origin = PJ_FALSE;
            continue;
        }

        /* The remaining reserved prefixes are not valid label lengths. */
        if (octet > 63)
            return PJLIB_UTIL_EDNSINNAMEPTR;

        /* Check that label length is valid.
         * Each label consists of an octet length (of size 1) followed
         * by the octet of the specified length (octet). Then it must be
         * followed by either another label's octet length or a zero length
         * octet (that terminates the sequence).
         */
        if ((pj_size_t)octet >= cursor_remaining(&scan))
            return PJLIB_UTIL_EDNSINNAMEPTR;

        status = cursor_take(&scan, octet, &label);
        if (status != PJ_SUCCESS)
            return PJLIB_UTIL_EDNSINNAMEPTR;
        if (at_origin)
            origin->pos = scan.pos;

        /* Add separators only between labels; a pointer may resolve to root */
        if (length != 0) {
            static const pj_uint8_t dot = '.';

            status = emit_name(dst, capacity, &length, &dot, 1);
            if (status != PJ_SUCCESS)
                return status;
        }

        status = emit_name(dst, capacity, &length, label, octet);
        if (status != PJ_SUCCESS)
            return status;
    }

    *name_len = length;
    return PJ_SUCCESS;
}

static pj_status_t parse_name(dns_cursor *cursor, pj_pool_t *pool,
                              pj_str_t *name)
{
    dns_cursor measure = *cursor;
    pj_size_t measured_len;
    pj_size_t copied_len;
    pj_status_t status;

    /* Get the length of the name using the same traversal used for copying */
    status = decode_name(&measure, NULL, MAX_DNS_NAME_LENGTH, &measured_len);
    if (status != PJ_SUCCESS)
        return status;

    /* Allocate memory for the name and copy it with an explicit capacity */
    name->ptr = (char*)pj_pool_alloc(pool, measured_len + 1);
    status = decode_name(cursor, name->ptr, measured_len, &copied_len);
    if (status != PJ_SUCCESS)
        return status;
    if (copied_len != measured_len)
        return PJ_EBUG;

    name->ptr[copied_len] = '\0';
    name->slen = (pj_ssize_t)copied_len;
    return PJ_SUCCESS;
}


/* Reject trailing bytes in known resource data */
static pj_status_t cursor_require_empty(const dns_cursor *cursor)
{
    return cursor_remaining(cursor) == 0 ? PJ_SUCCESS :
           PJLIB_UTIL_EDNSINSIZE;
}


PJ_DEF(const char *) pj_dns_get_type_name(int type)
{
    switch (type) {
    case PJ_DNS_TYPE_A:     return "A";
    case PJ_DNS_TYPE_AAAA:  return "AAAA";
    case PJ_DNS_TYPE_SRV:   return "SRV";
    case PJ_DNS_TYPE_NS:    return "NS";
    case PJ_DNS_TYPE_CNAME: return "CNAME";
    case PJ_DNS_TYPE_PTR:   return "PTR";
    case PJ_DNS_TYPE_MX:    return "MX";
    case PJ_DNS_TYPE_TXT:   return "TXT";
    case PJ_DNS_TYPE_NAPTR: return "NAPTR";
    }
    return "(Unknown)";
}


static void write16(pj_uint8_t *p, pj_uint16_t val)
{
    p[0] = (pj_uint8_t)(val >> 8);
    p[1] = (pj_uint8_t)(val & 0xFF);
}


/**
 * Initialize a DNS query transaction.
 */
PJ_DEF(pj_status_t) pj_dns_make_query( void *packet,
                                       unsigned *size,
                                       pj_uint16_t id,
                                       int qtype,
                                       const pj_str_t *name)
{
    pj_uint8_t *p = (pj_uint8_t*)packet;
    const char *startlabel, *endlabel, *endname;
    pj_size_t d;

    /* Sanity check */
    PJ_ASSERT_RETURN(packet && size && qtype && name, PJ_EINVAL);

    /* Calculate total number of bytes required: DNS header, the encoded
     * query name, and the query type & class (4 bytes). The encoded name
     * needs 2 octets more than the dotted string - a leading length octet
     * and the trailing root (zero) octet.
     */
    d = sizeof(pj_dns_hdr) + (name->slen + 2) + 4;

    /* Check that size is sufficient. */
    PJ_ASSERT_RETURN(*size >= d, PJLIB_UTIL_EDNSQRYTOOSMALL);

    /* Initialize header */
    pj_bzero(p, sizeof(struct pj_dns_hdr));
    write16(p+0, id);
    write16(p+2, (pj_uint16_t)PJ_DNS_SET_RD(1));
    write16(p+4, (pj_uint16_t)1);

    /* Initialize query */
    p = ((pj_uint8_t*)packet)+sizeof(pj_dns_hdr);

    /* Tokenize name */
    startlabel = endlabel = name->ptr;
    endname = name->ptr + name->slen;
    while (endlabel != endname) {
        while (endlabel != endname && *endlabel != '.')
            ++endlabel;
        *p++ = (pj_uint8_t)(endlabel - startlabel);
        pj_memcpy(p, startlabel, endlabel-startlabel);
        p += (endlabel-startlabel);
        if (endlabel != endname && *endlabel == '.')
            ++endlabel;
        startlabel = endlabel;
    }
    *p++ = '\0';

    /* Set type */
    write16(p, (pj_uint16_t)qtype);
    p += 2;

    /* Set class (IN=1) */
    write16(p, 1);
    p += 2;

    /* Done, calculate length */
    *size = (unsigned)(p - (pj_uint8_t*)packet);

    return 0;
}


/* Parse query records. */
static pj_status_t parse_query(pj_dns_parsed_query *q, pj_pool_t *pool,
                               dns_cursor *cursor)
{
    pj_status_t status;

    /* Parse, allocate, and copy the query name */
    status = parse_name(cursor, pool, &q->name);
    if (status != PJ_SUCCESS)
        return status;

    /* Get the type and class */
    status = cursor_read16(cursor, &q->type);
    if (status != PJ_SUCCESS)
        return status;
    return cursor_read16(cursor, &q->dnsclass);
}


/* Parse RR records */
static pj_status_t parse_rr(pj_dns_parsed_rr *rr, pj_pool_t *pool,
                            dns_cursor *cursor)
{
    dns_cursor rdata;
    pj_status_t status;

    /* Parse, allocate, and copy the owner name */
    status = parse_name(cursor, pool, &rr->name);
    if (status != PJ_SUCCESS)
        return status;

    /* Get the type and class */
    status = cursor_read16(cursor, &rr->type);
    if (status != PJ_SUCCESS)
        return status;

    status = cursor_read16(cursor, &rr->dnsclass);
    if (status != PJ_SUCCESS)
        return status;

    /* Class MUST be IN */
    if (rr->dnsclass != 1) {
        /* Class is not IN, return error only if type is known (see #1889) */
        if (rr->type == PJ_DNS_TYPE_A     || rr->type == PJ_DNS_TYPE_AAAA  ||
            rr->type == PJ_DNS_TYPE_CNAME || rr->type == PJ_DNS_TYPE_NS    ||
            rr->type == PJ_DNS_TYPE_PTR   || rr->type == PJ_DNS_TYPE_SRV)
        {
            return PJLIB_UTIL_EDNSINCLASS;
        }
    }

    /* Get TTL and RDATA length */
    status = cursor_read32(cursor, &rr->ttl);
    if (status != PJ_SUCCESS)
        return status;
    status = cursor_read16(cursor, &rr->rdlength);
    if (status != PJ_SUCCESS)
        return status;
    /* Restrict origin reads to the declared RDATA extent */
    status = cursor_child(cursor, rr->rdlength, &rdata);
    if (status != PJ_SUCCESS)
        return status;

    /* Parse some well known records */
    if (rr->type == PJ_DNS_TYPE_A) {
        status = cursor_read(&rdata, &rr->rdata.a.ip_addr, 4);

    } else if (rr->type == PJ_DNS_TYPE_AAAA) {
        status = cursor_read(&rdata, &rr->rdata.aaaa.ip_addr, 16);

    } else if (rr->type == PJ_DNS_TYPE_CNAME ||
               rr->type == PJ_DNS_TYPE_NS ||
               rr->type == PJ_DNS_TYPE_PTR)
    {
        /* Get the target name */
        status = parse_name(&rdata, pool, &rr->rdata.cname.name);

    } else if (rr->type == PJ_DNS_TYPE_SRV) {
        /* Priority */
        status = cursor_read16(&rdata, &rr->rdata.srv.prio);
        if (status != PJ_SUCCESS)
            return status;
        /* Weight */
        status = cursor_read16(&rdata, &rr->rdata.srv.weight);
        if (status != PJ_SUCCESS)
            return status;
        /* Port */
        status = cursor_read16(&rdata, &rr->rdata.srv.port);
        if (status != PJ_SUCCESS)
            return status;
        /* Get the target name */
        status = parse_name(&rdata, pool, &rr->rdata.srv.target);

    } else {
        /* Copy the raw data */
        rr->data = pj_pool_alloc(pool, rr->rdlength);
        status = cursor_read(&rdata, rr->data, rr->rdlength);
    }

    if (status != PJ_SUCCESS)
        return status;
    return cursor_require_empty(&rdata);
}


/*
 * Parse raw DNS packet into DNS packet structure.
 */
PJ_DEF(pj_status_t) pj_dns_parse_packet( pj_pool_t *pool,
                                         const void *packet,
                                         unsigned size,
                                         pj_dns_parsed_packet **p_res)
{
    pj_dns_parsed_packet *res;
    pj_dns_hdr header;
    dns_cursor cursor;
    pj_status_t status;
    unsigned i;

    /* Sanity checks */
    PJ_ASSERT_RETURN(pool && packet && size && p_res, PJ_EINVAL);

    /* Mark the packet-wide bounds used by the top-level cursor */
    cursor.packet = (const pj_uint8_t*)packet;
    cursor.packet_size = size;
    cursor.pos = 0;
    cursor.end = size;

    /* Read the DNS header and convert it to host byte order */
    status = cursor_read_header(&cursor, &header);
    if (status != PJ_SUCCESS)
        return status;

    /* Create the structure */
    res = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);
    res->hdr = header;

    /* Sanity check section counts to prevent excessive allocation from
     * malicious packets.
     */
    if (res->hdr.qdcount > PJ_DNS_MAX_SECTION_COUNT ||
        res->hdr.anscount > PJ_DNS_MAX_SECTION_COUNT ||
        res->hdr.nscount > PJ_DNS_MAX_SECTION_COUNT ||
        res->hdr.arcount > PJ_DNS_MAX_SECTION_COUNT)
    {
        return PJ_ETOOMANY;
    }

    /* Parse query records (if any).
     */
    if (res->hdr.qdcount) {
        res->q = (pj_dns_parsed_query*)
                 pj_pool_zalloc(pool, res->hdr.qdcount *
                                      sizeof(pj_dns_parsed_query));
        for (i=0; i<res->hdr.qdcount; ++i) {
            status = parse_query(&res->q[i], pool, &cursor);
            if (status != PJ_SUCCESS)
                return status;
        }
    }

    /* Parse answer, if any */
    if (res->hdr.anscount) {
        res->ans = (pj_dns_parsed_rr*)
                   pj_pool_zalloc(pool, res->hdr.anscount * 
                                        sizeof(pj_dns_parsed_rr));

        for (i=0; i<res->hdr.anscount; ++i) {
            status = parse_rr(&res->ans[i], pool, &cursor);
            if (status != PJ_SUCCESS)
                return status;
        }
    }

    /* Parse authoritative NS records, if any */
    if (res->hdr.nscount) {
        res->ns = (pj_dns_parsed_rr*)
                  pj_pool_zalloc(pool, res->hdr.nscount *
                                       sizeof(pj_dns_parsed_rr));

        for (i=0; i<res->hdr.nscount; ++i) {
            status = parse_rr(&res->ns[i], pool, &cursor);
            if (status != PJ_SUCCESS)
                return status;
        }
    }

    /* Parse additional RR answer, if any */
    if (res->hdr.arcount) {
        res->arr = (pj_dns_parsed_rr*)
                   pj_pool_zalloc(pool, res->hdr.arcount *
                                        sizeof(pj_dns_parsed_rr));

        for (i=0; i<res->hdr.arcount; ++i) {
            status = parse_rr(&res->arr[i], pool, &cursor);
            if (status != PJ_SUCCESS)
                return status;
        }
    }

    /* Looks like everything is okay */
    *p_res = res;

    return PJ_SUCCESS;
}


/* Perform name compression scheme.
 * If a name is already in the nametable, when no need to duplicate
 * the string with the pool, but rather just use the pointer there.
 */
static void apply_name_table( unsigned *count,
                              pj_str_t nametable[],
                              const pj_str_t *src,
                              pj_pool_t *pool,
                              pj_str_t *dst)
{
    unsigned i;

    /* Scan strings in nametable */
    for (i=0; i<*count; ++i) {
        if (pj_stricmp(&nametable[i], src) == 0)
            break;
    }

    /* If name is found in nametable, use the pointer in the nametable */
    if (i != *count) {
        dst->ptr = nametable[i].ptr;
        dst->slen = nametable[i].slen;
        return;
    }

    /* Otherwise duplicate the string, and insert new name in nametable */
    pj_strdup(pool, dst, src);

    if (*count < PJ_DNS_MAX_NAMES_IN_NAMETABLE) {
        nametable[*count].ptr = dst->ptr;
        nametable[*count].slen = dst->slen;

        ++(*count);
    }
}

static void copy_query(pj_pool_t *pool, pj_dns_parsed_query *dst,
                       const pj_dns_parsed_query *src,
                       unsigned *nametable_count,
                       pj_str_t nametable[])
{
    pj_memcpy(dst, src, sizeof(*src));
    apply_name_table(nametable_count, nametable, &src->name, pool, &dst->name);
}


static void copy_rr(pj_pool_t *pool, pj_dns_parsed_rr *dst,
                    const pj_dns_parsed_rr *src,
                    unsigned *nametable_count,
                    pj_str_t nametable[])
{
    pj_memcpy(dst, src, sizeof(*src));
    apply_name_table(nametable_count, nametable, &src->name, pool, &dst->name);

    if (src->data) {
        dst->data = pj_pool_alloc(pool, src->rdlength);
        pj_memcpy(dst->data, src->data, src->rdlength);
    }

    if (src->type == PJ_DNS_TYPE_SRV) {
        apply_name_table(nametable_count, nametable, &src->rdata.srv.target, 
                         pool, &dst->rdata.srv.target);
    } else if (src->type == PJ_DNS_TYPE_A) {
        dst->rdata.a.ip_addr.s_addr =  src->rdata.a.ip_addr.s_addr;
    } else if (src->type == PJ_DNS_TYPE_AAAA) {
        pj_memcpy(&dst->rdata.aaaa.ip_addr, &src->rdata.aaaa.ip_addr,
                  sizeof(pj_in6_addr));
    } else if (src->type == PJ_DNS_TYPE_CNAME) {
        pj_strdup(pool, &dst->rdata.cname.name, &src->rdata.cname.name);
    } else if (src->type == PJ_DNS_TYPE_NS) {
        pj_strdup(pool, &dst->rdata.ns.name, &src->rdata.ns.name);
    } else if (src->type == PJ_DNS_TYPE_PTR) {
        pj_strdup(pool, &dst->rdata.ptr.name, &src->rdata.ptr.name);
    }
}

/*
 * Duplicate DNS packet.
 */
PJ_DEF(void) pj_dns_packet_dup(pj_pool_t *pool,
                               const pj_dns_parsed_packet*p,
                               unsigned options,
                               pj_dns_parsed_packet **p_dst)
{
    pj_dns_parsed_packet *dst;
    unsigned nametable_count = 0;
#if PJ_DNS_MAX_NAMES_IN_NAMETABLE
    pj_str_t nametable[PJ_DNS_MAX_NAMES_IN_NAMETABLE];
#else
    pj_str_t *nametable = NULL;
#endif
    unsigned i;

    PJ_ASSERT_ON_FAIL(pool && p && p_dst, return);

    /* Create packet and copy header */
    *p_dst = dst = PJ_POOL_ZALLOC_T(pool, pj_dns_parsed_packet);
    pj_memcpy(&dst->hdr, &p->hdr, sizeof(p->hdr));

    /* Initialize section counts in the target packet to zero.
     * If memory allocation fails during copying process, the target packet
     * should have a correct section counts.
     */
    dst->hdr.qdcount = 0;
    dst->hdr.anscount = 0;
    dst->hdr.nscount = 0;
    dst->hdr.arcount = 0;
        

    /* Copy query section */
    if (p->hdr.qdcount && (options & PJ_DNS_NO_QD)==0) {
        dst->q = (pj_dns_parsed_query*)
                 pj_pool_alloc(pool, p->hdr.qdcount * 
                                     sizeof(pj_dns_parsed_query));
        for (i=0; i<p->hdr.qdcount; ++i) {
            copy_query(pool, &dst->q[i], &p->q[i], 
                       &nametable_count, nametable);
            ++dst->hdr.qdcount;
        }
    }

    /* Copy answer section */
    if (p->hdr.anscount && (options & PJ_DNS_NO_ANS)==0) {
        dst->ans = (pj_dns_parsed_rr*)
                   pj_pool_alloc(pool, p->hdr.anscount * 
                                       sizeof(pj_dns_parsed_rr));
        for (i=0; i<p->hdr.anscount; ++i) {
            copy_rr(pool, &dst->ans[i], &p->ans[i],
                    &nametable_count, nametable);
            ++dst->hdr.anscount;
        }
    }

    /* Copy NS section */
    if (p->hdr.nscount && (options & PJ_DNS_NO_NS)==0) {
        dst->ns = (pj_dns_parsed_rr*)
                  pj_pool_alloc(pool, p->hdr.nscount * 
                                      sizeof(pj_dns_parsed_rr));
        for (i=0; i<p->hdr.nscount; ++i) {
            copy_rr(pool, &dst->ns[i], &p->ns[i],
                    &nametable_count, nametable);
            ++dst->hdr.nscount;
        }
    }

    /* Copy additional info section */
    if (p->hdr.arcount && (options & PJ_DNS_NO_AR)==0) {
        dst->arr = (pj_dns_parsed_rr*)
                   pj_pool_alloc(pool, p->hdr.arcount * 
                                       sizeof(pj_dns_parsed_rr));
        for (i=0; i<p->hdr.arcount; ++i) {
            copy_rr(pool, &dst->arr[i], &p->arr[i],
                    &nametable_count, nametable);
            ++dst->hdr.arcount;
        }
    }
}


PJ_DEF(void) pj_dns_init_srv_rr( pj_dns_parsed_rr *rec,
                                 const pj_str_t *res_name,
                                 unsigned dnsclass,
                                 unsigned ttl,
                                 unsigned prio,
                                 unsigned weight,
                                 unsigned port,
                                 const pj_str_t *target)
{
    pj_bzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = PJ_DNS_TYPE_SRV;
    rec->dnsclass = (pj_uint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.srv.prio = (pj_uint16_t) prio;
    rec->rdata.srv.weight = (pj_uint16_t) weight;
    rec->rdata.srv.port = (pj_uint16_t) port;
    rec->rdata.srv.target = *target;
}


PJ_DEF(void) pj_dns_init_cname_rr( pj_dns_parsed_rr *rec,
                                   const pj_str_t *res_name,
                                   unsigned dnsclass,
                                   unsigned ttl,
                                   const pj_str_t *name)
{
    pj_bzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = PJ_DNS_TYPE_CNAME;
    rec->dnsclass = (pj_uint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.cname.name = *name;
}


PJ_DEF(void) pj_dns_init_a_rr( pj_dns_parsed_rr *rec,
                               const pj_str_t *res_name,
                               unsigned dnsclass,
                               unsigned ttl,
                               const pj_in_addr *ip_addr)
{
    pj_bzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = PJ_DNS_TYPE_A;
    rec->dnsclass = (pj_uint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.a.ip_addr = *ip_addr;
}


PJ_DEF(void) pj_dns_init_aaaa_rr(pj_dns_parsed_rr *rec,
                                 const pj_str_t *res_name,
                                 unsigned dnsclass,
                                 unsigned ttl,
                                 const pj_in6_addr *ip_addr)
{
    pj_bzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = PJ_DNS_TYPE_AAAA;
    rec->dnsclass = (pj_uint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.aaaa.ip_addr = *ip_addr;
}

