/* $Id$
 */

#include <pjsip/sip_resolve.h>
#include <pjsip/sip_transport.h>
#include <pj/pool.h>
#include <pj/ctype.h>
#include <pj/assert.h>

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
			    pjsip_host_port *target,
			    void *token,
			    pjsip_resolver_callback *cb)
{
    struct pjsip_server_addresses svr_addr;
    pj_status_t status;
    int is_ip_addr;
    pjsip_transport_type_e type = target->type;

    PJ_UNUSED_ARG(resolver);
    PJ_UNUSED_ARG(pool);

    /* We only do synchronous resolving at this moment. */
    PJ_TODO(SUPPORT_RFC3263_SERVER_RESOLUTION)

    /* Is it IP address or hostname?. */
    is_ip_addr = is_str_ip(&target->host);

    /* Set the transport type if not explicitly specified. 
     * RFC 3263 section 4.1 specify rules to set up this.
     */
    if (type == PJSIP_TRANSPORT_UNSPECIFIED) {
	if (is_ip_addr || (target->port != 0)) {
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
    if (target->port == 0) {
	target->port = pjsip_transport_get_default_port_for_type(type);
    }

    /* Resolve hostname. */
    if (!is_ip_addr) {
	status = pj_sockaddr_in_init(&svr_addr.entry[0].addr, &target->host, 
				     (pj_uint16_t)target->port);
    } else {
	status = pj_sockaddr_in_init(&svr_addr.entry[0].addr, &target->host, 
				     (pj_uint16_t)target->port);
	pj_assert(status == PJ_SUCCESS);
    }

    /* Call the callback. */
    svr_addr.count = (status == PJ_SUCCESS) ? 1 : 0;
    svr_addr.entry[0].type = type;
    (*cb)(status, token, &svr_addr);
}

