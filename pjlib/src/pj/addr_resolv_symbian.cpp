/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pj/errno.h>
#include <pj/ip_helper.h>
#include <pj/sock.h>
#include <pj/string.h>
#include <pj/unicode.h>

#include "os_symbian.h"
 


// PJLIB API: resolve hostname
PJ_DEF(pj_status_t) pj_gethostbyname(const pj_str_t *name, pj_hostent *he)
{
    static pj_addrinfo ai;
    static char *aliases[2];
    static char *addrlist[2];
    unsigned count = 1;
    pj_status_t status;
    
    status = pj_getaddrinfo(PJ_AF_INET, name, &count, &ai);
    if (status != PJ_SUCCESS)
    	return status;
    
    aliases[0] = ai.ai_canonname;
    aliases[1] = NULL;
    
    addrlist[0] = (char*) &ai.ai_addr.ipv4.sin_addr;
    addrlist[1] = NULL;
    
    pj_bzero(he, sizeof(*he));
    he->h_name = aliases[0];
    he->h_aliases = aliases;
    he->h_addrtype = PJ_AF_INET;
    he->h_length = 4;
    he->h_addr_list = addrlist;
    
    return PJ_SUCCESS;
}


// Resolve for specific address family
static pj_status_t getaddrinfo_by_af(int af, const pj_str_t *name,
				     unsigned *count, pj_addrinfo ai[]) 
{
    unsigned i;
    pj_status_t status;
    
    PJ_ASSERT_RETURN(name && count && ai, PJ_EINVAL);

    // Get resolver for the specified address family
    RHostResolver &resv = PjSymbianOS::Instance()->GetResolver(af);

    // Convert name to Unicode
    wchar_t name16[PJ_MAX_HOSTNAME];
    pj_ansi_to_unicode(name->ptr, name->slen, name16, PJ_ARRAY_SIZE(name16));
    TPtrC16 data((const TUint16*)name16);

    // Resolve!
    TNameEntry nameEntry;
    TRequestStatus reqStatus;
    
    resv.GetByName(data, nameEntry, reqStatus);
    User::WaitForRequest(reqStatus);
    
    // Iterate each result
    i = 0;
    while (reqStatus == KErrNone && i < *count) {
    	
	// Get the resolved TInetAddr
	TInetAddr inetAddr(nameEntry().iAddr);
	int addrlen;

	// Ignore if this is not the same address family
	if (inetAddr.Family() != (unsigned)af) {
	    resv.Next(nameEntry, reqStatus);
	    User::WaitForRequest(reqStatus);
	    continue;
	}
	
	// Convert the official address to ANSI.
	pj_unicode_to_ansi((const wchar_t*)nameEntry().iName.Ptr(), 
			   nameEntry().iName.Length(),
		       	   ai[i].ai_canonname, sizeof(ai[i].ai_canonname));

	// Convert IP address
	addrlen = sizeof(ai[i].ai_addr);
	status = PjSymbianOS::Addr2pj(inetAddr, ai[i].ai_addr, &addrlen);
	if (status != PJ_SUCCESS)
	    return status;
	
	// Next
	++i;
	resv.Next(nameEntry, reqStatus);
	User::WaitForRequest(reqStatus);
    }

    *count = i;
    return PJ_SUCCESS;
}

/* Resolve IPv4/IPv6 address */
PJ_DEF(pj_status_t) pj_getaddrinfo(int af, const pj_str_t *nodename,
				   unsigned *count, pj_addrinfo ai[]) 
{
    unsigned start;
    pj_status_t status;
    
    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6 || af==PJ_AF_UNSPEC,
    		     PJ_EAFNOTSUP);
    PJ_ASSERT_RETURN(nodename && count && *count && ai, PJ_EINVAL);
    
    start = 0;
    
    if (af==PJ_AF_INET6 || af==PJ_AF_UNSPEC) {
        unsigned max = *count;
    	status = getaddrinfo_by_af(PJ_AF_INET6, nodename, 
    				   &max, &ai[start]);
    	if (status == PJ_SUCCESS) {
    	    (*count) -= max;
    	    start += max;
    	}
    }
    
    if (af==PJ_AF_INET || af==PJ_AF_UNSPEC) {
        unsigned max = *count;
    	status = getaddrinfo_by_af(PJ_AF_INET, nodename, 
    				   &max, &ai[start]);
    	if (status == PJ_SUCCESS) {
    	    (*count) -= max;
    	    start += max;
    	}
    }
    
    *count = start;
    
    if (*count) {
    	return PJ_SUCCESS;
    } else {
    	return status!=PJ_SUCCESS ? status : PJ_ENOTFOUND;
    }
}

