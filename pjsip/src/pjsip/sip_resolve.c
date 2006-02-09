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
#include <pjsip/sip_resolve.h>
#include <pjsip/sip_transport.h>
#include <pjsip/sip_errno.h>
#include <pj/pool.h>
#include <pj/ctype.h>
#include <pj/assert.h>
#include <pj/log.h>

#define THIS_FILE   "sip_resolve.c"

struct pjsip_resolver_t
{
    void *dummy;
};

PJ_DEF(pjsip_resolver_t*) pjsip_resolver_create(pj_pool_t *pool)
{
    pjsip_resolver_t *resolver;
    resolver = (pjsip_resolver_t*) pj_pool_calloc(pool, 1, sizeof(*resolver));
    return resolver;
}

PJ_DEF(void) pjsip_resolver_destroy(pjsip_resolver_t *resolver)
{
    PJ_UNUSED_ARG(resolver);
}

static int is_str_ip(const pj_str_t *host)
{
    const char *p = host->ptr;
    const char *end = ((const char*)host->ptr) + host->slen;

    while (p != end) {
	if (pj_isdigit(*p) || *p=='.') {
	    ++p;
	} else {
	    return 0;
	}
    }
    return 1;
}

PJ_DEF(void) pjsip_resolve( pjsip_resolver_t *resolver,
			    pj_pool_t *pool,
			    pjsip_host_info *target,
			    void *token,
			    pjsip_resolver_callback *cb)
{
    struct pjsip_server_addresses svr_addr;
    pj_status_t status;
    int is_ip_addr;
    pjsip_transport_type_e type = target->type;

    PJ_UNUSED_ARG(resolver);
    PJ_UNUSED_ARG(pool);

    PJ_LOG(5,(THIS_FILE, "Resolving server '%.*s:%d' type=%s",
			 target->addr.host.slen,
			 target->addr.host.ptr,
			 target->addr.port,
			 pjsip_transport_get_type_name(type)));

    /* We only do synchronous resolving at this moment. */
    PJ_TODO(SUPPORT_RFC3263_SERVER_RESOLUTION)

    /* Is it IP address or hostname?. */
    is_ip_addr = is_str_ip(&target->addr.host);

    /* Set the transport type if not explicitly specified. 
     * RFC 3263 section 4.1 specify rules to set up this.
     */
    if (type == PJSIP_TRANSPORT_UNSPECIFIED) {
	if (is_ip_addr || (target->addr.port != 0)) {
#if PJ_HAS_TCP
	    if (target->flag & PJSIP_TRANSPORT_SECURE) 
	    {
		type = PJSIP_TRANSPORT_TLS;
	    } else if (target->flag & PJSIP_TRANSPORT_RELIABLE) 
	    {
		type = PJSIP_TRANSPORT_TCP;
	    } else 
#endif
	    {
		type = PJSIP_TRANSPORT_UDP;
	    }
	} else {
	    /* No type or explicit port is specified, and the address is
	     * not IP address.
	     * In this case, full resolution must be performed.
	     * But we don't support it (yet).
	     */
	    type = PJSIP_TRANSPORT_UDP;
	}

    }

    /* Set the port number if not specified. */
    if (target->addr.port == 0) {
	target->addr.port = pjsip_transport_get_default_port_for_type(type);
    }

    /* Resolve hostname. */
    if (!is_ip_addr) {
	status = pj_sockaddr_in_init((pj_sockaddr_in*)&svr_addr.entry[0].addr, 
				     &target->addr.host, 
				     (pj_uint16_t)target->addr.port);
    } else {
	status = pj_sockaddr_in_init((pj_sockaddr_in*)&svr_addr.entry[0].addr, 
				      &target->addr.host, 
				     (pj_uint16_t)target->addr.port);
    }

    if (status != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	PJ_LOG(4,(THIS_FILE, "Failed to resolve '%.*s'. Err=%d (%s)",
			     target->addr.host.slen,
			     target->addr.host.ptr,
			     status,
			     pj_strerror(status,errmsg,sizeof(errmsg)).ptr));
	(*cb)(status, token, &svr_addr);
	return;
    }

    /* Call the callback. */
    PJ_LOG(5,(THIS_FILE, "Server resolved: '%.*s:%d' type=%s has %d entries, "
			 "entry[0]=%s:%d type=%s",
			 target->addr.host.slen,
			 target->addr.host.ptr,
			 target->addr.port,
			 pjsip_transport_get_type_name(type),
			 1,
			 pj_inet_ntoa(((pj_sockaddr_in*)&svr_addr.entry[0].addr)->sin_addr),
			 target->addr.port,
			 pjsip_transport_get_type_name(type)));
    svr_addr.count = (status == PJ_SUCCESS) ? 1 : 0;
    svr_addr.entry[0].type = type;
    svr_addr.entry[0].addr_len = sizeof(pj_sockaddr_in);
    (*cb)(status, token, &svr_addr);
}

