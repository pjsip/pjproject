/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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


/**
 * Initialize a DNS query transaction.
 */
PJ_DEF(pj_status_t) pj_dns_make_query( void *packet,
				       unsigned *size,
				       pj_uint16_t id,
				       pj_dns_type qtype,
				       const pj_str_t *name)
{
    pj_dns_hdr *hdr;
    char *query, *p;
    const char *startlabel, *endlabel, *endname;
    pj_uint16_t tmp;
    unsigned d;

    /* Sanity check */
    PJ_ASSERT_RETURN(packet && size && qtype && name, PJ_EINVAL);

    /* Calculate total number of bytes required. */
    d = sizeof(pj_dns_hdr) + name->slen + 4;

    /* Check that size is sufficient. */
    PJ_ASSERT_RETURN(*size >= d, PJLIB_UTIL_EDNSQRYTOOSMALL);

    /* Initialize header */
    hdr = packet;
    pj_bzero(hdr, sizeof(struct pj_dns_hdr));
    hdr->id = pj_htons(id);
    hdr->flags = pj_htons(PJ_DNS_SET_RD(1));
    hdr->qdcount = pj_htons(1);

    /* Initialize query */
    query = p = (char*)(hdr+1);

    /* Tokenize name */
    startlabel = endlabel = name->ptr;
    endname = name->ptr + name->slen;
    while (endlabel != endname) {
	while (endlabel != endname && *endlabel != '.')
	    ++endlabel;
	*p++ = (char)(endlabel - startlabel);
	pj_memcpy(p, startlabel, endlabel-startlabel);
	p += (endlabel-startlabel);
	if (endlabel != endname && *endlabel == '.')
	    ++endlabel;
	startlabel = endlabel;
    }
    *p++ = '\0';

    /* Set type */
    tmp = pj_htons((pj_uint16_t)(qtype));
    pj_memcpy(p, &tmp, 2);
    p += 2;

    /* Set class (IN=1) */
    tmp = pj_htons(1);
    pj_memcpy(p, &tmp, 2);
    p += 2;

    /* Done, calculate length */
    *size = p - (char*)packet;

    return 0;
}


/* Get a name length (note: name consists of multiple labels and
 * it may contain pointers when name compression is applied) 
 */
static pj_status_t get_name_len(int rec_counter, const char *pkt, 
				const char *start, const char *max, 
				int *parsed_len, int *name_len)
{
    const char *p;
    pj_status_t status;

    /* Limit the number of recursion */
    if (rec_counter > 10) {
	/* Too many name recursion */
	return PJLIB_UTIL_EDNSINNAMEPTR;
    }

    *name_len = *parsed_len = 0;
    p = start;
    while (*p) {
	if ((*p & 0xc0) == 0xc0) {
	    /* Compression is found! */
	    int ptr_len = 0;
	    int dummy;
	    pj_uint16_t offset;

	    /* Get the 14bit offset */
	    pj_memcpy(&offset, p, 2);
	    offset ^= pj_htons((pj_uint16_t)(0xc0 << 8));
	    offset = pj_ntohs(offset);

	    /* Check that offset is valid */
	    if (offset >= max - pkt)
		return PJLIB_UTIL_EDNSINNAMEPTR;

	    /* Get the name length from that offset. */
	    status = get_name_len(rec_counter+1, pkt, pkt + offset, max, 
				  &dummy, &ptr_len);
	    if (status != PJ_SUCCESS)
		return status;

	    *parsed_len += 2;
	    *name_len += ptr_len;

	    return PJ_SUCCESS;
	} else {
	    unsigned label_len = *p;

	    /* Check that label length is valid */
	    if (pkt+label_len > max)
		return PJLIB_UTIL_EDNSINNAMEPTR;

	    p += (label_len + 1);
	    *parsed_len += (label_len + 1);

	    if (*p != 0)
		++label_len;
	    
	    *name_len += label_len;

	    if (p >= max)
		return PJLIB_UTIL_EDNSINSIZE;
	}
    }
    ++p;
    (*parsed_len)++;

    return PJ_SUCCESS;
}


/* Parse and copy name (note: name consists of multiple labels and
 * it may contain pointers when compression is applied).
 */
static pj_status_t get_name(int rec_counter, const char *pkt, 
			    const char *start, const char *max,
			    pj_str_t *name)
{
    const char *p;
    pj_status_t status;

    /* Limit the number of recursion */
    if (rec_counter > 10) {
	/* Too many name recursion */
	return PJLIB_UTIL_EDNSINNAMEPTR;
    }

    p = start;
    while (*p) {
	if ((*p & 0xc0) == 0xc0) {
	    /* Compression is found! */
	    pj_uint16_t offset;

	    /* Get the 14bit offset */
	    pj_memcpy(&offset, p, 2);
	    offset ^= pj_htons((pj_uint16_t)(0xc0 << 8));
	    offset = pj_ntohs(offset);

	    /* Check that offset is valid */
	    if (offset >= max - pkt)
		return PJLIB_UTIL_EDNSINNAMEPTR;

	    /* Retrieve the name from that offset. */
	    status = get_name(rec_counter+1, pkt, pkt + offset, max, name);
	    if (status != PJ_SUCCESS)
		return status;

	    return PJ_SUCCESS;
	} else {
	    unsigned label_len = *p;

	    /* Check that label length is valid */
	    if (pkt+label_len > max)
		return PJLIB_UTIL_EDNSINNAMEPTR;

	    pj_memcpy(name->ptr + name->slen, p+1, label_len);
	    name->slen += label_len;

	    p += label_len + 1;
	    if (*p != 0) {
		*(name->ptr + name->slen) = '.';
		++name->slen;
	    }

	    if (p >= max)
		return PJLIB_UTIL_EDNSINSIZE;
	}
    }

    return PJ_SUCCESS;
}


/* Skip query records. */
static pj_status_t skip_query(const char *pkt, const char *start, 
			      const char *max, int *skip_len)
{
    int name_len = 0;
    pj_status_t status;

    status = get_name_len(0, pkt, start, max, skip_len, &name_len);
    if (status != PJ_SUCCESS)
	return status;

    (*skip_len) += 4;
    return PJ_SUCCESS;
}


/* Parse RR records */
static pj_status_t parse_rr(pj_dns_parsed_rr *rr, pj_pool_t *pool,
			    const char *pkt,
			    const char *start, const char *max, 
			    int *parsed_len)
{
    const char *p = start;
    int name_len, name_part_len;
    pj_status_t status;

    /* Get the length of the name */
    status = get_name_len(0, pkt, start, max, &name_part_len, &name_len);
    if (status != PJ_SUCCESS)
	return status;

    /* Allocate memory for the name */
    rr->name.ptr = pj_pool_alloc(pool, name_len+4);
    rr->name.slen = 0;

    /* Get the name */
    status = get_name(0, pkt, start, max, &rr->name);
    if (status != PJ_SUCCESS)
	return status;

    p = (start + name_part_len);

    /* Check the size can accomodate next few fields. */
    if (p+10 > max)
	return PJLIB_UTIL_EDNSINSIZE;

    /* Get the type */
    pj_memcpy(&rr->type, p, 2);
    rr->type = pj_ntohs(rr->type);
    p += 2;
    
    /* Get the class */
    pj_memcpy(&rr->class_, p, 2);
    rr->class_ = pj_ntohs(rr->class_);
    p += 2;

    /* Class MUST be IN */
    if (rr->class_ != 1)
	return PJLIB_UTIL_EDNSINCLASS;

    /* Get TTL */
    pj_memcpy(&rr->ttl, p, 4);
    rr->ttl = pj_htonl(rr->ttl);
    p += 4;

    /* Get rdlength */
    pj_memcpy(&rr->rdlength, p, 2);
    rr->rdlength = pj_htons(rr->rdlength);
    p += 2;

    /* Check that length is valid */
    if (p + rr->rdlength > max)
	return PJLIB_UTIL_EDNSINSIZE;

    /* Copy the raw data */
    rr->data = pj_pool_alloc(pool, rr->rdlength);
    pj_memcpy(rr->data, p, rr->rdlength);

    /* Parse some well known records */
    if (rr->type == PJ_DNS_TYPE_A) {
	pj_in_addr ip_addr;

	pj_memcpy(&ip_addr, p, 4);
	pj_strdup2(pool, &rr->rdata.a.ip_addr, pj_inet_ntoa(ip_addr));

	p += 4;

    } else if (rr->type == PJ_DNS_TYPE_CNAME ||
	       rr->type == PJ_DNS_TYPE_NS ||
	       rr->type == PJ_DNS_TYPE_PTR) 
    {

	/* Get the length of the target name */
	status = get_name_len(0, pkt, p, max, &name_part_len, &name_len);
	if (status != PJ_SUCCESS)
	    return status;

	/* Allocate memory for the name */
	rr->rdata.cname.name.ptr = pj_pool_alloc(pool, name_len);
	rr->rdata.cname.name.slen = 0;

	/* Get the name */
	status = get_name(0, pkt, p, max, &rr->rdata.cname.name);
	if (status != PJ_SUCCESS)
	    return status;

	p += name_part_len;

    } else if (rr->type == PJ_DNS_TYPE_SRV) {

	/* Priority */
	pj_memcpy(&rr->rdata.srv.prio, p, 2);
	rr->rdata.srv.prio = pj_ntohs(rr->rdata.srv.prio);
	p += 2;

	/* Weight */
	pj_memcpy(&rr->rdata.srv.weight, p, 2);
	rr->rdata.srv.weight = pj_ntohs(rr->rdata.srv.weight);
	p += 2;

	/* Port */
	pj_memcpy(&rr->rdata.srv.port, p, 2);
	rr->rdata.srv.port = pj_ntohs(rr->rdata.srv.port);
	p += 2;
	
	/* Get the length of the target name */
	status = get_name_len(0, pkt, p, max, &name_part_len, &name_len);
	if (status != PJ_SUCCESS)
	    return status;

	/* Allocate memory for the name */
	rr->rdata.srv.target.ptr = pj_pool_alloc(pool, name_len);
	rr->rdata.srv.target.slen = 0;

	/* Get the name */
	status = get_name(0, pkt, p, max, &rr->rdata.srv.target);
	if (status != PJ_SUCCESS)
	    return status;
	p += name_part_len;

    } else {
	p += rr->rdlength;
    }

    *parsed_len = (int)(p - start);
    return PJ_SUCCESS;
}


/*
 * Parse raw DNS response packet into DNS response structure.
 */
PJ_DEF(pj_status_t) pj_dns_parse_response( pj_pool_t *pool,
					   const void *packet,
					   unsigned size,
					   pj_dns_parsed_response **p_res)
{
    pj_dns_parsed_response *res;
    char *start, *end;
    pj_status_t status;
    unsigned i;

    /* Sanity checks */
    PJ_ASSERT_RETURN(pool && packet && size && p_res, PJ_EINVAL);

    /* Packet size must be at least as big as the header */
    if (size < sizeof(pj_dns_hdr))
	return PJLIB_UTIL_EDNSINSIZE;

    /* Create the response */
    res = pj_pool_zalloc(pool, sizeof(pj_dns_parsed_response));

    /* Copy the DNS header, and convert endianness to host byte order */
    pj_memcpy(&res->hdr, packet, sizeof(pj_dns_hdr));
    res->hdr.id	      = pj_ntohs(res->hdr.id);
    res->hdr.flags    = pj_ntohs(res->hdr.flags);
    res->hdr.qdcount  = pj_ntohs(res->hdr.qdcount);
    res->hdr.anscount = pj_ntohs(res->hdr.anscount);
    res->hdr.nscount  = pj_ntohs(res->hdr.nscount);
    res->hdr.arcount  = pj_ntohs(res->hdr.arcount);

    /* Mark start and end of payload */
    start = ((char*)packet) + sizeof(pj_dns_hdr);
    end = ((char*)packet) + size;

    /* If we have query records (some DNS servers do send them), skip
     * the records.
     */
    for (i=0; i<res->hdr.qdcount; ++i) {
	int skip_len;

	status = skip_query(packet, start, end, &skip_len);
	if (status != PJ_SUCCESS)
	    return status;

	start += skip_len;
    }

    /* Parse answer, if any */
    if (res->hdr.anscount) {
	res->ans = pj_pool_zalloc(pool, res->hdr.anscount * 
					sizeof(pj_dns_parsed_rr));

	for (i=0; i<res->hdr.anscount; ++i) {
	    int parsed_len;

	    status = parse_rr(&res->ans[i], pool, packet, start, end, 
			      &parsed_len);
	    if (status != PJ_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Parse authoritative NS records, if any */
    if (res->hdr.nscount) {
	res->ns = pj_pool_zalloc(pool, res->hdr.nscount *
				       sizeof(pj_dns_parsed_rr));

	for (i=0; i<res->hdr.nscount; ++i) {
	    int parsed_len;

	    status = parse_rr(&res->ns[i], pool, packet, start, end, 
			      &parsed_len);
	    if (status != PJ_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Parse additional RR answer, if any */
    if (res->hdr.arcount) {
	res->arr = pj_pool_zalloc(pool, res->hdr.arcount *
					sizeof(pj_dns_parsed_rr));

	for (i=0; i<res->hdr.arcount; ++i) {
	    int parsed_len;

	    status = parse_rr(&res->arr[i], pool, packet, start, end, 
			      &parsed_len);
	    if (status != PJ_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Looks like everything is okay */
    *p_res = res;

    return PJ_SUCCESS;
}
