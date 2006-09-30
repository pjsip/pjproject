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
#include <pj/assert.h>
#include <pj/log.h>

#define THIS_FILE   "dns_dump.c"
#define LEVEL	    3

static const char *get_rr_name(int type)
{
    switch (type) {
    case PJ_DNS_TYPE_A:	    return "A";
    case PJ_DNS_TYPE_NS:    return "NS";
    case PJ_DNS_TYPE_CNAME: return "CNAME";
    case PJ_DNS_TYPE_PTR:   return "PTR";
    case PJ_DNS_TYPE_MX:    return "MX";
    case PJ_DNS_TYPE_TXT:   return "TXT";
    case PJ_DNS_TYPE_SRV:   return "SRV";
    case PJ_DNS_TYPE_NAPTR: return "NAPTR";
    }
    return "(Unknown)";
}

static void dump_answer(unsigned index, const pj_dns_parsed_rr *rr)
{
    const pj_str_t root_name = { "<Root>", 6 };
    const pj_str_t *name = &rr->name;

    if (name->slen == 0)
	name = &root_name;

    PJ_LOG(3,(THIS_FILE, " %d. %s record (type=%d)", index, get_rr_name(rr->type),
			rr->type));
    PJ_LOG(3,(THIS_FILE, "    Name: %.*s", (int)name->slen, name->ptr));
    PJ_LOG(3,(THIS_FILE, "    TTL: %u", rr->ttl));
    PJ_LOG(3,(THIS_FILE, "    Data length: %u", rr->rdlength));

    if (rr->type == PJ_DNS_TYPE_SRV) {
	PJ_LOG(3,(THIS_FILE, "    SRV: prio=%d, weight=%d %.*s:%d", 
			     rr->rdata.srv.prio, rr->rdata.srv.weight,
			     (int)rr->rdata.srv.target.slen, rr->rdata.srv.target.ptr,
			     rr->rdata.srv.port));
    } else if (rr->type == PJ_DNS_TYPE_CNAME ||
	       rr->type == PJ_DNS_TYPE_NS ||
	       rr->type == PJ_DNS_TYPE_PTR) 
    {
	PJ_LOG(3,(THIS_FILE, "    Name: %.*s",
		  (int)rr->rdata.cname.name.slen,
		  rr->rdata.cname.name.ptr));
    } else if (rr->type == PJ_DNS_TYPE_A) {
	PJ_LOG(3,(THIS_FILE, "    IP address: %.*s",
		  (int)rr->rdata.a.ip_addr.slen,
		  rr->rdata.a.ip_addr.ptr));
    }
}


PJ_DEF(void) pj_dns_dump_response(const pj_dns_parsed_response *res)
{
    unsigned i;

    PJ_ASSERT_ON_FAIL(res != NULL, return);

    /* Header part */
    PJ_LOG(3,(THIS_FILE, "Domain Name System packet (%s):",
		(PJ_DNS_GET_QR(res->hdr.flags) ? "response" : "query")));
    PJ_LOG(3,(THIS_FILE, " Transaction ID: %d", res->hdr.id));
    PJ_LOG(3,(THIS_FILE, " Flags: type=%s, opcode=%d, authoritative=%d, truncated=%d, rcode=%d", 
		(PJ_DNS_GET_QR(res->hdr.flags) ? "response" : "query"),
		 PJ_DNS_GET_OPCODE(res->hdr.flags),
		 PJ_DNS_GET_AA(res->hdr.flags),
		 PJ_DNS_GET_TC(res->hdr.flags),
		 PJ_DNS_GET_RCODE(res->hdr.flags)));
    PJ_LOG(3,(THIS_FILE, " Nb of question RR: %d", res->hdr.qdcount));
    PJ_LOG(3,(THIS_FILE, " Nb of answer RR: %d", res->hdr.anscount));
    PJ_LOG(3,(THIS_FILE, " Nb of authority RR: %d", res->hdr.nscount));
    PJ_LOG(3,(THIS_FILE, " Nb of additional RR: %d", res->hdr.arcount));
    PJ_LOG(3,(THIS_FILE, ""));

    /* Dump answers */
    if (res->hdr.anscount) {
	PJ_LOG(3,(THIS_FILE, " Answers RR:"));

	for (i=0; i<res->hdr.anscount; ++i) {
	    dump_answer(i, &res->ans[i]);
	}
	PJ_LOG(3,(THIS_FILE, ""));
    }

    /* Dump NS sections */
    if (res->hdr.anscount) {
	PJ_LOG(3,(THIS_FILE, " NS Authority RR:"));

	for (i=0; i<res->hdr.nscount; ++i) {
	    dump_answer(i, &res->ns[i]);
	}
	PJ_LOG(3,(THIS_FILE, ""));
    }

    /* Dump Additional info sections */
    if (res->hdr.arcount) {
	PJ_LOG(3,(THIS_FILE, " Additional Info RR:"));

	for (i=0; i<res->hdr.arcount; ++i) {
	    dump_answer(i, &res->arr[i]);
	}
	PJ_LOG(3,(THIS_FILE, ""));
    }

}

