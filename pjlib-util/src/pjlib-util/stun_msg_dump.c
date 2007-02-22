/* $Id$ */
/* 
 * Copyright (C) 2003-2005 Benny Prijono <benny@prijono.org>
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
#include <pjlib-util/stun_msg.h>
#include <pjlib-util/errno.h>
#include <pj/assert.h>
#include <pj/string.h>


#define APPLY()		if (len < 1 || len >= (end-p)) \
			    goto on_return; \
			p += len

static int print_attr(char *buffer, unsigned length,
		      const pj_stun_attr_hdr *ahdr)
{
    char *p = buffer, *end = buffer + length;
    int len;

    len = pj_ansi_snprintf(buffer, end-p,
			   "  %s: length=%d",
			   pj_stun_get_attr_name(ahdr->type),
			   (int)ahdr->length);
    APPLY();


    switch (ahdr->type) {
    case PJ_STUN_ATTR_MAPPED_ADDR:
    case PJ_STUN_ATTR_RESPONSE_ADDR:
    case PJ_STUN_ATTR_SOURCE_ADDR:
    case PJ_STUN_ATTR_CHANGED_ADDR:
    case PJ_STUN_ATTR_REFLECTED_FROM:
    case PJ_STUN_ATTR_REMOTE_ADDRESS:
    case PJ_STUN_ATTR_RELAY_ADDRESS:
    case PJ_STUN_ATTR_XOR_MAPPED_ADDRESS:
    case PJ_STUN_ATTR_REQUESTED_IP:
    case PJ_STUN_ATTR_XOR_REFLECTED_FROM:
    case PJ_STUN_ATTR_XOR_INTERNAL_ADDR:
    case PJ_STUN_ATTR_ALTERNATE_SERVER:
	{
	    const pj_stun_generic_ip_addr_attr *attr;

	    attr = (const pj_stun_generic_ip_addr_attr*)ahdr;

	    if (attr->addr.addr.sa_family == PJ_AF_INET) {
		len = pj_ansi_snprintf(buffer, end-p,
				       ", IPv4 addr=%s:%d\n",
				       pj_inet_ntoa(attr->addr.ipv4.sin_addr),
				       pj_ntohs(attr->addr.ipv4.sin_port));

	    } else if (attr->addr.addr.sa_family == PJ_AF_INET6) {
		len = pj_ansi_snprintf(buffer, end-p,
				       ", IPv6 addr present\n");
	    } else {
		len = pj_ansi_snprintf(buffer, end-p,
				       ", INVALID ADDRESS FAMILY!\n");
	    }
	}
	break;

    case PJ_STUN_ATTR_CHANGE_REQUEST:
    case PJ_STUN_ATTR_LIFETIME:
    case PJ_STUN_ATTR_BANDWIDTH:
    case PJ_STUN_ATTR_REQUESTED_ADDR_TYPE:
    case PJ_STUN_ATTR_REQUESTED_PORT_PROPS:
    case PJ_STUN_ATTR_REQUESTED_TRANSPORT:
    case PJ_STUN_ATTR_TIMER_VAL:
    case PJ_STUN_ATTR_PRIORITY:
    case PJ_STUN_ATTR_FINGERPRINT:
    case PJ_STUN_ATTR_REFRESH_INTERVAL:
	{
	    const pj_stun_generic_uint_attr *attr;

	    attr = (const pj_stun_generic_uint_attr*)ahdr;
	    len = pj_ansi_snprintf(buffer, end-p,
				   ", value=%d (%x)\n",
				   (pj_uint32_t)attr->value,
				   (pj_uint32_t)attr->value);
	}
	break;

    case PJ_STUN_ATTR_USERNAME:
    case PJ_STUN_ATTR_PASSWORD:
    case PJ_STUN_ATTR_REALM:
    case PJ_STUN_ATTR_NONCE:
    case PJ_STUN_ATTR_SERVER:
	{
	    const pj_stun_generic_string_attr *attr;

	    attr = (pj_stun_generic_string_attr*)ahdr;
	    len = pj_ansi_snprintf(buffer, end-p,
				   ", value=\"%.*s\"\n",
				   (int)attr->value.slen,
				   attr->value.ptr);
	}
	break;

    case PJ_STUN_ATTR_ERROR_CODE:
	{
	    const pj_stun_error_code_attr *attr;

	    attr = (const pj_stun_error_code_attr*) ahdr;
	    len = pj_ansi_snprintf(buffer, end-p,
				   ", err_code=%d, reason=\"%.*s\"\n",
				   attr->err_class*100 + attr->number,
				   (int)attr->reason.slen,
				   attr->reason.ptr);
	}
	break;

    case PJ_STUN_ATTR_UNKNOWN_ATTRIBUTES:
	{
	    const pj_stun_unknown_attr *attr;
	    unsigned j;

	    attr = (const pj_stun_unknown_attr*) ahdr;

	    len = pj_ansi_snprintf(buffer, end-p,
				   ", unknown list:");
	    APPLY();

	    for (j=0; j<attr->attr_count; ++j) {
		len = pj_ansi_snprintf(buffer, end-p,
				       " %d",
				       (int)attr->attrs[j]);
		APPLY();
	    }
	}
	break;

    case PJ_STUN_ATTR_MESSAGE_INTEGRITY:
    case PJ_STUN_ATTR_DATA:
    case PJ_STUN_ATTR_USE_CANDIDATE:
    default:
	len = pj_ansi_snprintf(buffer, end-p, "\n");

	break;
    }

    APPLY();

    return (p-buffer);

on_return:
    return len;
}


/*
 * Dump STUN message to a printable string output.
 */
PJ_DEF(char*) pj_stun_msg_dump(const pj_stun_msg *msg,
			       char *buffer,
			       unsigned *length)
{
    char *p, *end;
    int len;
    unsigned i;

    PJ_ASSERT_RETURN(msg && buffer && length, NULL);

    p = buffer;
    end = buffer + (*length);

    len = pj_ansi_snprintf(p, end-p, "STUN %s %s\n",
			   pj_stun_get_method_name(msg->hdr.type),
			   pj_stun_get_class_name(msg->hdr.type));
    APPLY();

    len = pj_ansi_snprintf(p, end-p, 
			    " Hdr: length=%d, magic=%x, tsx_id=%x %x %x\n"
			    " Attributes:\n",
			   msg->hdr.length,
			   msg->hdr.magic,
			   *(pj_uint32_t*)&msg->hdr.tsx_id[0],
			   *(pj_uint32_t*)&msg->hdr.tsx_id[4],
			   *(pj_uint32_t*)&msg->hdr.tsx_id[8]);
    APPLY();

    for (i=0; i<msg->attr_count; ++i) {
	len = print_attr(p, end-p, msg->attr[i]);
	APPLY();
    }

on_return:
    *p = '\0';
    *length = (p-buffer);
    return buffer;

}


#undef APPLY
