/* $Id$ */
/* 
 * Copyright (C)2003-2007 Benny Prijono <benny@prijono.org>
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
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/errno.h>
#include <pj/ip_helper.h>
#include <pj/compat/socket.h>


PJ_DEF(pj_status_t) pj_gethostbyname(const pj_str_t *hostname, pj_hostent *phe)
{
    struct hostent *he;
    char copy[PJ_MAX_HOSTNAME];

    pj_assert(hostname && hostname ->slen < PJ_MAX_HOSTNAME);
    
    if (hostname->slen >= PJ_MAX_HOSTNAME)
	return PJ_ENAMETOOLONG;

    pj_memcpy(copy, hostname->ptr, hostname->slen);
    copy[ hostname->slen ] = '\0';

    he = gethostbyname(copy);
    if (!he) {
	return PJ_ERESOLVE;
	/* DO NOT use pj_get_netos_error() since host resolution error
	 * is reported in h_errno instead of errno!
	return pj_get_netos_error();
	 */
    }

    phe->h_name = he->h_name;
    phe->h_aliases = he->h_aliases;
    phe->h_addrtype = he->h_addrtype;
    phe->h_length = he->h_length;
    phe->h_addr_list = he->h_addr_list;

    return PJ_SUCCESS;
}

/* Get the default IP interface */
PJ_DEF(pj_status_t) pj_getdefaultipinterface(pj_in_addr *addr)
{
    pj_sock_t fd;
    pj_str_t cp;
    pj_sockaddr_in a;
    int len;
    pj_status_t status;

    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &fd);
    if (status != PJ_SUCCESS) {
	return status;
    }

    cp = pj_str("1.1.1.1");
    pj_sockaddr_in_init(&a, &cp, 53);

    status = pj_sock_connect(fd, &a, sizeof(a));
    if (status != PJ_SUCCESS) {
	pj_sock_close(fd);
	return status;
    }

    len = sizeof(a);
    status = pj_sock_getsockname(fd, &a, &len);
    if (status != PJ_SUCCESS) {
	pj_sock_close(fd);
	return status;
    }

    pj_sock_close(fd);

    *addr = a.sin_addr;

    /* Success */
    return PJ_SUCCESS;
}


/* Resolve the IP address of local machine */
PJ_DEF(pj_status_t) pj_gethostip(pj_in_addr *addr)
{
    const pj_str_t *hostname = pj_gethostname();
    struct pj_hostent he;
    pj_status_t status;


#ifdef _MSC_VER
    /* Get rid of "uninitialized he variable" with MS compilers */
    pj_bzero(&he, sizeof(he));
#endif

    /* Try with resolving local hostname first */
    status = pj_gethostbyname(hostname, &he);
    if (status == PJ_SUCCESS) {
	*addr = *(pj_in_addr*)he.h_addr;
    }


    /* If we end up with 127.x.x.x, resolve the IP by getting the default
     * interface to connect to some public host.
     */
    if (status != PJ_SUCCESS || (pj_ntohl(addr->s_addr) >> 24)==127 ||
	addr->s_addr == 0) 
    {
	status = pj_getdefaultipinterface(addr);
    }

    /* As the last resort, get the first available interface */
    if (status != PJ_SUCCESS) {
	pj_in_addr addrs[2];
	unsigned count = PJ_ARRAY_SIZE(addrs);

	status = pj_enum_ip_interface(&count, addrs);
	if (status == PJ_SUCCESS) {
	    if (count != 0) {
		*addr = addrs[0];
	    } else {
		/* Just return 127.0.0.1 */
		addr->s_addr = pj_htonl (0x7f000001);
	    }
	}
    }

    return status;
}


/* Resolve IPv4/IPv6 address */
PJ_DEF(pj_status_t) pj_getaddrinfo(const pj_str_t *nodename, int af,
				   unsigned *count, pj_addrinfo ai[])
{
#if defined(PJ_SOCK_HAS_GETADDRINFO) && PJ_SOCK_HAS_GETADDRINFO!=0
    char nodecopy[PJ_MAX_HOSTNAME];
    struct addrinfo hint, *res;
    unsigned i;
    int rc;

    PJ_ASSERT_RETURN(nodename && count && *count && ai, PJ_EINVAL);
    PJ_ASSERT_RETURN(nodename->ptr && nodename->slen, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EINVAL);

    /* Copy node name to null terminated string. */
    if (nodename->slen >= PJ_MAX_HOSTNAME)
	return PJ_ENAMETOOLONG;
    pj_memcpy(nodecopy, nodename->ptr, nodename->slen);
    nodecopy[nodename->slen] = '\0';

    /* Call getaddrinfo() */
    pj_bzero(&hint, sizeof(hint));
    hint.ai_family = af;

    rc = getaddrinfo(nodecopy, NULL, &hint, &res);
    if (rc != 0)
	return PJ_ERESOLVE;

    /* Enumerate each item in the result */
    for (i=0; i<*count && res; res=res->ai_next) {
	int len;

	/* Ignore unwanted address families */
	if (af!=PJ_AF_UNSPEC && res->ai_family != af)
	    continue;

	/* Ignore name that's too long */
	len = pj_ansi_strlen(res->ai_canonname);
	if (len >= PJ_MAX_HOSTNAME)
	    continue;

	/* Store canonical name */
	pj_ansi_strcpy(ai[i].ai_canonname, res->ai_canonname);

	/* Store address */
	PJ_ASSERT_ON_FAIL(res->ai_addrlen <= sizeof(pj_sockaddr), continue);
	pj_memcpy(&ai[i].ai_addr, res->ai_addr, res->ai_addrlen);

	/* Next slot */
	++i;
    }

    *count = i;

    /* Done */
    return PJ_SUCCESS;

#else	/* PJ_SOCK_HAS_GETADDRINFO */
    /* IPv6 is not supported */
    PJ_UNUSED_ARG(nodename);
    PJ_UNUSED_ARG(af);
    PJ_UNUSED_ARG(ai);

    PJ_ASSERT_RETURN(count, PJ_EINVAL);
    *count = 0;

    return PJ_EIPV6NOTSUP;
#endif	/* PJ_SOCK_HAS_GETADDRINFO */
}

