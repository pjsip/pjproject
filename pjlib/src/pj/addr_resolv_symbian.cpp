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
    PJ_ASSERT_RETURN(name && he, PJ_EINVAL);

    RHostResolver &resv = PjSymbianOS::Instance()->GetResolver();

    // Convert name to Unicode
    wchar_t name16[PJ_MAX_HOSTNAME];
    pj_ansi_to_unicode(name->ptr, name->slen, name16, PJ_ARRAY_SIZE(name16));
    TPtrC16 data((const TUint16*)name16);

    // Resolve!
    TNameEntry nameEntry;
    TRequestStatus reqStatus;
    
    resv.GetByName(data, nameEntry, reqStatus);
    User::WaitForRequest(reqStatus);
    
    if (reqStatus != KErrNone)
	return PJ_RETURN_OS_ERROR(reqStatus.Int());

    // Get the resolved TInetAddr
    // This doesn't work, see Martin email on 28/3/2007:
    // const TNameRecord &rec = (const TNameRecord&) nameEntry;
    // TInetAddr inetAddr(rec.iAddr);
    TInetAddr inetAddr(nameEntry().iAddr);

    //
    // This where we keep static variables.
    // These should be kept in TLS probably, to allow multiple threads
    // to call pj_gethostbyname() without interfering each other.
    // But again, we don't support threads in Symbian!
    //
    static char resolved_name[PJ_MAX_HOSTNAME];
    static char *no_aliases[1];
    static pj_in_addr *addr_list[2];
    static pj_sockaddr_in resolved_addr;

    // Convert the official address to ANSI.
    pj_unicode_to_ansi((const wchar_t*)nameEntry().iName.Ptr(), nameEntry().iName.Length(),
		       resolved_name, sizeof(resolved_name));

    // Convert IP address
    
    PjSymbianOS::Addr2pj(inetAddr, resolved_addr);
    addr_list[0] = &resolved_addr.sin_addr;

    // Return hostent
    he->h_name = resolved_name;
    he->h_aliases = no_aliases;
    he->h_addrtype = pj_AF_INET();
    he->h_length = 4;
    he->h_addr_list = (char**) addr_list;

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
		addr->s_addr = pj_htonl(0x7f000001);
	    }
	}
    }

    return status;
}


