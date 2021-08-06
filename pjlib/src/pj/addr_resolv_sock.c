/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/errno.h>
#include <pj/ip_helper.h>
#include <pj/compat/socket.h>

#if defined(PJ_GETADDRINFO_USE_CFHOST) && PJ_GETADDRINFO_USE_CFHOST!=0
#   include <CoreFoundation/CFString.h>
#   include <CFNetwork/CFHost.h>
#endif

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

/* Resolve IPv4/IPv6 address */
PJ_DEF(pj_status_t) pj_getaddrinfo(int af, const pj_str_t *nodename,
				   unsigned *count, pj_addrinfo ai[])
{
#if defined(PJ_SOCK_HAS_GETADDRINFO) && PJ_SOCK_HAS_GETADDRINFO!=0
    char nodecopy[PJ_MAX_HOSTNAME];
    pj_bool_t has_addr = PJ_FALSE;
    unsigned i;
#if defined(PJ_GETADDRINFO_USE_CFHOST) && PJ_GETADDRINFO_USE_CFHOST!=0
    CFStringRef hostname;
    CFHostRef hostRef;
    pj_status_t status = PJ_SUCCESS;
#else
    int rc;
    struct addrinfo hint, *res, *orig_res;
#endif

    PJ_ASSERT_RETURN(nodename && count && *count && ai, PJ_EINVAL);
    PJ_ASSERT_RETURN(nodename->ptr && nodename->slen, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6 ||
		     af==PJ_AF_UNSPEC, PJ_EINVAL);

#if PJ_WIN32_WINCE

    /* Check if nodename is IP address */
    pj_bzero(&ai[0], sizeof(ai[0]));
    if ((af==PJ_AF_INET || af==PJ_AF_UNSPEC) &&
	pj_inet_pton(PJ_AF_INET, nodename,
		     &ai[0].ai_addr.ipv4.sin_addr) == PJ_SUCCESS)
    {
	af = PJ_AF_INET;
	has_addr = PJ_TRUE;
    } else if ((af==PJ_AF_INET6 || af==PJ_AF_UNSPEC) &&
	       pj_inet_pton(PJ_AF_INET6, nodename,
	                    &ai[0].ai_addr.ipv6.sin6_addr) == PJ_SUCCESS)
    {
	af = PJ_AF_INET6;
	has_addr = PJ_TRUE;
    }

    if (has_addr) {
	pj_str_t tmp;

	tmp.ptr = ai[0].ai_canonname;
	pj_strncpy_with_null(&tmp, nodename, PJ_MAX_HOSTNAME);
	ai[0].ai_addr.addr.sa_family = (pj_uint16_t)af;
	*count = 1;

	return PJ_SUCCESS;
    }

#else /* PJ_WIN32_WINCE */
    PJ_UNUSED_ARG(has_addr);
#endif

    /* Copy node name to null terminated string. */
    if (nodename->slen >= PJ_MAX_HOSTNAME)
	return PJ_ENAMETOOLONG;
    pj_memcpy(nodecopy, nodename->ptr, nodename->slen);
    nodecopy[nodename->slen] = '\0';

#if defined(PJ_GETADDRINFO_USE_CFHOST) && PJ_GETADDRINFO_USE_CFHOST!=0
    hostname =  CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, nodecopy,
						kCFStringEncodingASCII,
						kCFAllocatorNull);
    hostRef = CFHostCreateWithName(kCFAllocatorDefault, hostname);
    if (CFHostStartInfoResolution(hostRef, kCFHostAddresses, nil)) {
	CFArrayRef addrRef = CFHostGetAddressing(hostRef, nil);
	i = 0;
	if (addrRef != nil) {
	    CFIndex idx, naddr;
	    
	    naddr = CFArrayGetCount(addrRef);
	    for (idx = 0; idx < naddr && i < *count; idx++) {
		struct sockaddr *addr;
		size_t addr_size;
		
		addr = (struct sockaddr *)
		       CFDataGetBytePtr(CFArrayGetValueAtIndex(addrRef, idx));
		/* This should not happen. */
		pj_assert(addr);
		
		/* Ignore unwanted address families */
		if (af!=PJ_AF_UNSPEC && addr->sa_family != af)
		    continue;

		/* Store canonical name */
		pj_ansi_strcpy(ai[i].ai_canonname, nodecopy);
		
		/* Store address */
		addr_size = sizeof(*addr);
		if (addr->sa_family == PJ_AF_INET6) {
		    addr_size = addr->sa_len;
		}
		PJ_ASSERT_ON_FAIL(addr_size <= sizeof(pj_sockaddr), continue);
		pj_memcpy(&ai[i].ai_addr, addr, addr_size);
		PJ_SOCKADDR_RESET_LEN(&ai[i].ai_addr);
		
		i++;
	    }
	}
	
	*count = i;
	if (*count == 0)
	    status = PJ_ERESOLVE;

    } else {
	status = PJ_ERESOLVE;
    }
    
    CFRelease(hostRef);
    CFRelease(hostname);
    
    return status;
#else
    /* Call getaddrinfo() */
    pj_bzero(&hint, sizeof(hint));
    hint.ai_family = af;
    /* Zero value of ai_socktype means the implementation shall attempt
     * to resolve the service name for all supported socket types */
    hint.ai_socktype = 0;

    rc = getaddrinfo(nodecopy, NULL, &hint, &res);
    if (rc != 0)
	return PJ_ERESOLVE;

    orig_res = res;

    /* Enumerate each item in the result */
    for (i=0; i<*count && res; res=res->ai_next) {
	unsigned j;
	pj_bool_t duplicate_found = PJ_FALSE;

	/* Ignore unwanted address families */
	if (af!=PJ_AF_UNSPEC && res->ai_family != af)
	    continue;

	if (res->ai_socktype != pj_SOCK_DGRAM() &&
            res->ai_socktype != pj_SOCK_STREAM() &&
            /* It is possible that the result's sock type
             * is unspecified.
             */
            res->ai_socktype != 0)
        {
	        continue;
	}

	/* Add current address in the resulting list if there
	 * is no duplicates only. */
	for (j = 0; j < i; j++) {
	    if (!pj_sockaddr_cmp(&ai[j].ai_addr, res->ai_addr)) {
		duplicate_found = PJ_TRUE;
		break;
	    }
	}
	if (duplicate_found) {
	    continue;
	}

	/* Store canonical name (possibly truncating the name) */
	if (res->ai_canonname) {
	    pj_ansi_strncpy(ai[i].ai_canonname, res->ai_canonname,
			    sizeof(ai[i].ai_canonname));
	    ai[i].ai_canonname[sizeof(ai[i].ai_canonname)-1] = '\0';
	} else {
	    pj_ansi_strcpy(ai[i].ai_canonname, nodecopy);
	}

	/* Store address */
	PJ_ASSERT_ON_FAIL(res->ai_addrlen <= sizeof(pj_sockaddr), continue);
	pj_memcpy(&ai[i].ai_addr, res->ai_addr, res->ai_addrlen);
	PJ_SOCKADDR_RESET_LEN(&ai[i].ai_addr);

	/* Next slot */
	++i;
    }

    *count = i;

    freeaddrinfo(orig_res);

    /* Done */
    return (*count > 0? PJ_SUCCESS : PJ_ERESOLVE);
#endif

#else	/* PJ_SOCK_HAS_GETADDRINFO */
    pj_bool_t has_addr = PJ_FALSE;

    PJ_ASSERT_RETURN(count && *count, PJ_EINVAL);

#if PJ_WIN32_WINCE

    /* Check if nodename is IP address */
    pj_bzero(&ai[0], sizeof(ai[0]));
    if ((af==PJ_AF_INET || af==PJ_AF_UNSPEC) &&
	pj_inet_pton(PJ_AF_INET, nodename,
		     &ai[0].ai_addr.ipv4.sin_addr) == PJ_SUCCESS)
    {
	af = PJ_AF_INET;
	has_addr = PJ_TRUE;
    }
    else if ((af==PJ_AF_INET6 || af==PJ_AF_UNSPEC) &&
	     pj_inet_pton(PJ_AF_INET6, nodename,
			  &ai[0].ai_addr.ipv6.sin6_addr) == PJ_SUCCESS)
    {
	af = PJ_AF_INET6;
	has_addr = PJ_TRUE;
    }

    if (has_addr) {
	pj_str_t tmp;

	tmp.ptr = ai[0].ai_canonname;
	pj_strncpy_with_null(&tmp, nodename, PJ_MAX_HOSTNAME);
	ai[0].ai_addr.addr.sa_family = (pj_uint16_t)af;
	*count = 1;

	return PJ_SUCCESS;
    }

#else /* PJ_WIN32_WINCE */
    PJ_UNUSED_ARG(has_addr);
#endif

    if (af == PJ_AF_INET || af == PJ_AF_UNSPEC) {
	pj_hostent he;
	unsigned i, max_count;
	pj_status_t status;
	
	/* VC6 complains that "he" is uninitialized */
	#ifdef _MSC_VER
	pj_bzero(&he, sizeof(he));
	#endif

	status = pj_gethostbyname(nodename, &he);
	if (status != PJ_SUCCESS)
	    return status;

	max_count = *count;
	*count = 0;

	pj_bzero(ai, max_count * sizeof(pj_addrinfo));

	for (i=0; he.h_addr_list[i] && *count<max_count; ++i) {
	    pj_ansi_strncpy(ai[*count].ai_canonname, he.h_name,
			    sizeof(ai[*count].ai_canonname));
	    ai[*count].ai_canonname[sizeof(ai[*count].ai_canonname)-1] = '\0';

	    ai[*count].ai_addr.ipv4.sin_family = PJ_AF_INET;
	    pj_memcpy(&ai[*count].ai_addr.ipv4.sin_addr,
		      he.h_addr_list[i], he.h_length);
	    PJ_SOCKADDR_RESET_LEN(&ai[*count].ai_addr);

	    (*count)++;
	}

	return (*count > 0? PJ_SUCCESS : PJ_ERESOLVE);

    } else {
	/* IPv6 is not supported */
	*count = 0;

	return PJ_EIPV6NOTSUP;
    }
#endif	/* PJ_SOCK_HAS_GETADDRINFO */
}

