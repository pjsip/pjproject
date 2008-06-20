/* $Id$ */
/* 
 * Copyright (C)2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pj/sock.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ip_helper.h>
#include <pj/os.h>
#include <pj/addr_resolv.h>
#include <pj/string.h>
#include <pj/compat/socket.h>


/*
 * Convert address string with numbers and dots to binary IP address.
 */ 
PJ_DEF(pj_in_addr) pj_inet_addr(const pj_str_t *cp)
{
    pj_in_addr addr;

    pj_inet_aton(cp, &addr);
    return addr;
}

/*
 * Convert address string with numbers and dots to binary IP address.
 */ 
PJ_DEF(pj_in_addr) pj_inet_addr2(const char *cp)
{
    pj_str_t str = pj_str((char*)cp);
    return pj_inet_addr(&str);
}

/*
 * Get text representation.
 */
PJ_DEF(char*) pj_inet_ntop2( int af, const void *src,
			     char *dst, int size)
{
    pj_status_t status;

    status = pj_inet_ntop(af, src, dst, size);
    return (status==PJ_SUCCESS)? dst : NULL;
}

/*
 * Print socket address.
 */
PJ_DEF(char*) pj_sockaddr_print( const pj_sockaddr_t *addr,
				 char *buf, int size,
				 unsigned flags)
{
    enum {
	WITH_PORT = 1,
	WITH_BRACKETS = 2
    };

    char txt[PJ_INET6_ADDRSTRLEN];
    char port[32];
    const pj_addr_hdr *h = (const pj_addr_hdr*)addr;
    char *bquote, *equote;
    pj_status_t status;

    status = pj_inet_ntop(h->sa_family, pj_sockaddr_get_addr(addr),
			  txt, sizeof(txt));
    if (status != PJ_SUCCESS)
	return "";

    if (h->sa_family != PJ_AF_INET6 || (flags & WITH_BRACKETS)==0) {
	bquote = ""; equote = "";
    } else {
	bquote = "["; equote = "]";
    }

    if (flags & WITH_PORT) {
	pj_ansi_snprintf(port, sizeof(port), ":%d",
			 pj_sockaddr_get_port(addr));
    } else {
	port[0] = '\0';
    }

    pj_ansi_snprintf(buf, size, "%s%s%s%s",
		     bquote, txt, equote, port);

    return buf;
}

/*
 * Set the IP address of an IP socket address from string address, 
 * with resolving the host if necessary. The string address may be in a
 * standard numbers and dots notation or may be a hostname. If hostname
 * is specified, then the function will resolve the host into the IP
 * address.
 */
PJ_DEF(pj_status_t) pj_sockaddr_in_set_str_addr( pj_sockaddr_in *addr,
					         const pj_str_t *str_addr)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(!str_addr || str_addr->slen < PJ_MAX_HOSTNAME, 
                     (addr->sin_addr.s_addr=PJ_INADDR_NONE, PJ_EINVAL));

    PJ_SOCKADDR_RESET_LEN(addr);
    addr->sin_family = AF_INET;
    pj_bzero(addr->sin_zero, sizeof(addr->sin_zero));

    if (str_addr && str_addr->slen) {
	addr->sin_addr = pj_inet_addr(str_addr);
	if (addr->sin_addr.s_addr == PJ_INADDR_NONE) {
    	    pj_hostent he;
	    pj_status_t rc;

	    rc = pj_gethostbyname(str_addr, &he);
	    if (rc == 0) {
		addr->sin_addr.s_addr = *(pj_uint32_t*)he.h_addr;
	    } else {
		addr->sin_addr.s_addr = PJ_INADDR_NONE;
		return rc;
	    }
	}

    } else {
	addr->sin_addr.s_addr = 0;
    }

    return PJ_SUCCESS;
}

/* Set address from a name */
PJ_DEF(pj_status_t) pj_sockaddr_set_str_addr(int af,
					     pj_sockaddr *addr,
					     const pj_str_t *str_addr)
{
    pj_status_t status;

    if (af == PJ_AF_INET) {
	return pj_sockaddr_in_set_str_addr(&addr->ipv4, str_addr);
    }

    PJ_ASSERT_RETURN(af==PJ_AF_INET6, PJ_EAFNOTSUP);

    /* IPv6 specific */

    addr->ipv6.sin6_family = PJ_AF_INET6;
    PJ_SOCKADDR_RESET_LEN(addr);

    if (str_addr && str_addr->slen) {
	status = pj_inet_pton(PJ_AF_INET6, str_addr, &addr->ipv6.sin6_addr);
	if (status != PJ_SUCCESS) {
    	    pj_addrinfo ai;
	    unsigned count = 1;

	    status = pj_getaddrinfo(PJ_AF_INET6, str_addr, &count, &ai);
	    if (status==PJ_SUCCESS) {
		pj_memcpy(&addr->ipv6.sin6_addr, &ai.ai_addr.ipv6.sin6_addr,
			  sizeof(pj_sockaddr_in6));
	    }
	}
    } else {
	status = PJ_SUCCESS;
    }

    return status;
}

/*
 * Set the IP address and port of an IP socket address.
 * The string address may be in a standard numbers and dots notation or 
 * may be a hostname. If hostname is specified, then the function will 
 * resolve the host into the IP address.
 */
PJ_DEF(pj_status_t) pj_sockaddr_in_init( pj_sockaddr_in *addr,
				         const pj_str_t *str_addr,
					 pj_uint16_t port)
{
    PJ_ASSERT_RETURN(addr, (addr->sin_addr.s_addr=PJ_INADDR_NONE, PJ_EINVAL));

    PJ_SOCKADDR_RESET_LEN(addr);
    addr->sin_family = PJ_AF_INET;
    pj_bzero(addr->sin_zero, sizeof(addr->sin_zero));
    pj_sockaddr_in_set_port(addr, port);
    return pj_sockaddr_in_set_str_addr(addr, str_addr);
}

/*
 * Initialize IP socket address based on the address and port info.
 */
PJ_DEF(pj_status_t) pj_sockaddr_init(int af, 
				     pj_sockaddr *addr,
				     const pj_str_t *cp,
				     pj_uint16_t port)
{
    pj_status_t status;

    if (af == PJ_AF_INET) {
	return pj_sockaddr_in_init(&addr->ipv4, cp, port);
    }

    /* IPv6 specific */
    PJ_ASSERT_RETURN(af==PJ_AF_INET6, PJ_EAFNOTSUP);

    pj_bzero(addr, sizeof(pj_sockaddr_in6));
    addr->addr.sa_family = PJ_AF_INET6;
    
    status = pj_sockaddr_set_str_addr(af, addr, cp);
    if (status != PJ_SUCCESS)
	return status;

    addr->ipv6.sin6_port = pj_htons(port);
    return PJ_SUCCESS;
}

/*
 * Compare two socket addresses.
 */
PJ_DEF(int) pj_sockaddr_cmp( const pj_sockaddr_t *addr1,
			     const pj_sockaddr_t *addr2)
{
    const pj_sockaddr *a1 = (const pj_sockaddr*) addr1;
    const pj_sockaddr *a2 = (const pj_sockaddr*) addr2;
    int port1, port2;
    int result;

    /* Compare address family */
    if (a1->addr.sa_family < a2->addr.sa_family)
	return -1;
    else if (a1->addr.sa_family > a2->addr.sa_family)
	return 1;

    /* Compare addresses */
    result = pj_memcmp(pj_sockaddr_get_addr(a1),
		       pj_sockaddr_get_addr(a2),
		       pj_sockaddr_get_addr_len(a1));
    if (result != 0)
	return result;

    /* Compare port number */
    port1 = pj_sockaddr_get_port(a1);
    port2 = pj_sockaddr_get_port(a2);

    if (port1 < port2)
	return -1;
    else if (port1 > port2)
	return 1;

    /* TODO:
     *	Do we need to compare flow label and scope id in IPv6? 
     */
    
    /* Looks equal */
    return 0;
}

/*
 * Get first IP address associated with the hostname.
 */
PJ_DEF(pj_in_addr) pj_gethostaddr(void)
{
    pj_sockaddr_in addr;
    const pj_str_t *hostname = pj_gethostname();

    pj_sockaddr_in_set_str_addr(&addr, hostname);
    return addr.sin_addr;
}

/*
 * Get port number of a pj_sockaddr_in
 */
PJ_DEF(pj_uint16_t) pj_sockaddr_in_get_port(const pj_sockaddr_in *addr)
{
    return pj_ntohs(addr->sin_port);
}

/*
 * Get the address part
 */
PJ_DEF(void*) pj_sockaddr_get_addr(const pj_sockaddr_t *addr)
{
    const pj_sockaddr *a = (const pj_sockaddr*)addr;

    PJ_ASSERT_RETURN(a->addr.sa_family == PJ_AF_INET ||
		     a->addr.sa_family == PJ_AF_INET6, NULL);

    if (a->addr.sa_family == PJ_AF_INET6)
	return (void*) &a->ipv6.sin6_addr;
    else
	return (void*) &a->ipv4.sin_addr;
}

/*
 * Check if sockaddr contains a non-zero address
 */
PJ_DEF(pj_bool_t) pj_sockaddr_has_addr(const pj_sockaddr_t *addr)
{
    const pj_sockaddr *a = (const pj_sockaddr*)addr;

    /* It's probably not wise to raise assertion here if
     * the address doesn't contain a valid address family, and
     * just return PJ_FALSE instead.
     * 
     * The reason is because application may need to distinguish 
     * these three conditions with sockaddr:
     *	a) sockaddr is not initialized. This is by convention
     *	   indicated by sa_family==0.
     *	b) sockaddr is initialized with zero address. This is
     *	   indicated with the address field having zero address.
     *	c) sockaddr is initialized with valid address/port.
     *
     * If we enable this assertion, then application will loose
     * the capability to specify condition a), since it will be
     * forced to always initialize sockaddr (even with zero address).
     * This may break some parts of upper layer libraries.
     */
    //PJ_ASSERT_RETURN(a->addr.sa_family == PJ_AF_INET ||
    //		     a->addr.sa_family == PJ_AF_INET6, PJ_EAFNOTSUP);

    if (a->addr.sa_family!=PJ_AF_INET && a->addr.sa_family!=PJ_AF_INET6) {
	return PJ_FALSE;
    } else if (a->addr.sa_family == PJ_AF_INET6) {
	pj_uint8_t zero[24];
	pj_bzero(zero, sizeof(zero));
	return pj_memcmp(a->ipv6.sin6_addr.s6_addr, zero, 
			 sizeof(pj_in6_addr)) != 0;
    } else
	return a->ipv4.sin_addr.s_addr != PJ_INADDR_ANY;
}

/*
 * Get port number
 */
PJ_DEF(pj_uint16_t) pj_sockaddr_get_port(const pj_sockaddr_t *addr)
{
    const pj_sockaddr *a = (const pj_sockaddr*) addr;

    PJ_ASSERT_RETURN(a->addr.sa_family == PJ_AF_INET ||
		     a->addr.sa_family == PJ_AF_INET6, (pj_uint16_t)0xFFFF);

    return pj_ntohs((pj_uint16_t)(a->addr.sa_family == PJ_AF_INET6 ?
				    a->ipv6.sin6_port : a->ipv4.sin_port));
}

/*
 * Get the length of the address part.
 */
PJ_DEF(unsigned) pj_sockaddr_get_addr_len(const pj_sockaddr_t *addr)
{
    const pj_sockaddr *a = (const pj_sockaddr*) addr;
    PJ_ASSERT_RETURN(a->addr.sa_family == PJ_AF_INET ||
		     a->addr.sa_family == PJ_AF_INET6, PJ_EAFNOTSUP);
    return a->addr.sa_family == PJ_AF_INET6 ?
	    sizeof(pj_in6_addr) : sizeof(pj_in_addr);
}

/*
 * Get socket address length.
 */
PJ_DEF(unsigned) pj_sockaddr_get_len(const pj_sockaddr_t *addr)
{
    const pj_sockaddr *a = (const pj_sockaddr*) addr;
    PJ_ASSERT_RETURN(a->addr.sa_family == PJ_AF_INET ||
		     a->addr.sa_family == PJ_AF_INET6, PJ_EAFNOTSUP);
    return a->addr.sa_family == PJ_AF_INET6 ?
	    sizeof(pj_sockaddr_in6) : sizeof(pj_sockaddr_in);
}

/*
 * Copy only the address part (sin_addr/sin6_addr) of a socket address.
 */
PJ_DEF(void) pj_sockaddr_copy_addr( pj_sockaddr *dst,
				    const pj_sockaddr *src)
{
    pj_memcpy(pj_sockaddr_get_addr(dst),
	      pj_sockaddr_get_addr(src),
	      pj_sockaddr_get_addr_len(src));
}

/*
 * Copy socket address.
 */
PJ_DEF(void) pj_sockaddr_cp(pj_sockaddr_t *dst, const pj_sockaddr_t *src)
{
    pj_memcpy(dst, src, pj_sockaddr_get_len(src));
}

/*
 * Set port number of pj_sockaddr_in
 */
PJ_DEF(void) pj_sockaddr_in_set_port(pj_sockaddr_in *addr, 
				     pj_uint16_t hostport)
{
    addr->sin_port = pj_htons(hostport);
}

/*
 * Set port number of pj_sockaddr
 */
PJ_DEF(pj_status_t) pj_sockaddr_set_port(pj_sockaddr *addr, 
					 pj_uint16_t hostport)
{
    int af = addr->addr.sa_family;

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EINVAL);

    if (af == PJ_AF_INET6)
	addr->ipv6.sin6_port = pj_htons(hostport);
    else
	addr->ipv4.sin_port = pj_htons(hostport);

    return PJ_SUCCESS;
}

/*
 * Get IPv4 address
 */
PJ_DEF(pj_in_addr) pj_sockaddr_in_get_addr(const pj_sockaddr_in *addr)
{
    pj_in_addr in_addr;
    in_addr.s_addr = pj_ntohl(addr->sin_addr.s_addr);
    return in_addr;
}

/*
 * Set IPv4 address
 */
PJ_DEF(void) pj_sockaddr_in_set_addr(pj_sockaddr_in *addr,
				     pj_uint32_t hostaddr)
{
    addr->sin_addr.s_addr = pj_htonl(hostaddr);
}

/* Resolve the IP address of local machine */
PJ_DEF(pj_status_t) pj_gethostip(int af, pj_sockaddr *addr)
{
    unsigned count;
    pj_addrinfo ai;
    pj_status_t status;


#ifdef _MSC_VER
    /* Get rid of "uninitialized he variable" with MS compilers */
    pj_bzero(&ai, sizeof(ai));
#endif

    addr->addr.sa_family = (pj_uint16_t)af;
    PJ_SOCKADDR_RESET_LEN(addr);

    /* Try with resolving local hostname first */
    count = 1;
    status = pj_getaddrinfo(af, pj_gethostname(), &count, &ai);
    if (status == PJ_SUCCESS) {
    	pj_assert(ai.ai_addr.addr.sa_family == (pj_uint16_t)af);
    	pj_sockaddr_copy_addr(addr, &ai.ai_addr);
    }


    /* If we end up with 127.0.0.0/8 or 0.0.0.0/8, resolve the IP
     * by getting the default interface to connect to some public host.
     * The 0.0.0.0/8 is a special IP class that doesn't seem to be
     * practically useful for our purpose.
     */
    if (status != PJ_SUCCESS || !pj_sockaddr_has_addr(addr) ||
	(af==PJ_AF_INET && (pj_ntohl(addr->ipv4.sin_addr.s_addr)>>24)==127) ||
	(af==PJ_AF_INET && (pj_ntohl(addr->ipv4.sin_addr.s_addr)>>24)==0))
    {
		status = pj_getdefaultipinterface(af, addr);
    }

    /* If failed, get the first available interface */
    if (status != PJ_SUCCESS) {
		pj_sockaddr itf[1];
		unsigned count = PJ_ARRAY_SIZE(itf);
	
		status = pj_enum_ip_interface(af, &count, itf);
		if (status == PJ_SUCCESS) {
		    pj_assert(itf[0].addr.sa_family == (pj_uint16_t)af);
		    pj_sockaddr_copy_addr(addr, &itf[0]);
		}
    }

    /* If else fails, returns loopback interface as the last resort */
    if (status != PJ_SUCCESS) {
		if (af==PJ_AF_INET) {
		    addr->ipv4.sin_addr.s_addr = pj_htonl (0x7f000001);
		} else {
		    pj_in6_addr *s6_addr;
	
		    s6_addr = (pj_in6_addr*) pj_sockaddr_get_addr(addr);
		    pj_bzero(s6_addr, sizeof(pj_in6_addr));
		    s6_addr->s6_addr[15] = 1;
		}
		status = PJ_SUCCESS;
    }

    return status;
}

/* Get the default IP interface */
PJ_DEF(pj_status_t) pj_getdefaultipinterface(int af, pj_sockaddr *addr)
{
    pj_sock_t fd;
    pj_str_t cp;
    pj_sockaddr a;
    int len;
    pj_uint8_t zero[64];
    pj_status_t status;

    addr->addr.sa_family = (pj_uint16_t)af;

    status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &fd);
    if (status != PJ_SUCCESS) {
	return status;
    }

    if (af == PJ_AF_INET) {
	cp = pj_str("1.1.1.1");
    } else {
	cp = pj_str("1::1");
    }
    status = pj_sockaddr_init(af, &a, &cp, 53);
    if (status != PJ_SUCCESS) {
	pj_sock_close(fd);
	return status;
    }

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

    /* Check that the address returned is not zero */
    pj_bzero(zero, sizeof(zero));
    if (pj_memcmp(pj_sockaddr_get_addr(&a), zero,
		  pj_sockaddr_get_addr_len(&a))==0)
    {
	return PJ_ENOTFOUND;
    }

    pj_sockaddr_copy_addr(addr, &a);

    /* Success */
    return PJ_SUCCESS;
}


/* Only need to implement these in DLL build */
#if defined(PJ_DLL)

PJ_DEF(pj_uint16_t) pj_AF_UNSPEC(void)
{
    return PJ_AF_UNSPEC;
}

PJ_DEF(pj_uint16_t) pj_AF_UNIX(void)
{
    return PJ_AF_UNIX;
}

PJ_DEF(pj_uint16_t) pj_AF_INET(void)
{
    return PJ_AF_INET;
}

PJ_DEF(pj_uint16_t) pj_AF_INET6(void)
{
    return PJ_AF_INET6;
}

PJ_DEF(pj_uint16_t) pj_AF_PACKET(void)
{
    return PJ_AF_PACKET;
}

PJ_DEF(pj_uint16_t) pj_AF_IRDA(void)
{
    return PJ_AF_IRDA;
}

PJ_DEF(int) pj_SOCK_STREAM(void)
{
    return PJ_SOCK_STREAM;
}

PJ_DEF(int) pj_SOCK_DGRAM(void)
{
    return PJ_SOCK_DGRAM;
}

PJ_DEF(int) pj_SOCK_RAW(void)
{
    return PJ_SOCK_RAW;
}

PJ_DEF(int) pj_SOCK_RDM(void)
{
    return PJ_SOCK_RDM;
}

PJ_DEF(pj_uint16_t) pj_SOL_SOCKET(void)
{
    return PJ_SOL_SOCKET;
}

PJ_DEF(pj_uint16_t) pj_SOL_IP(void)
{
    return PJ_SOL_IP;
}

PJ_DEF(pj_uint16_t) pj_SOL_TCP(void)
{
    return PJ_SOL_TCP;
}

PJ_DEF(pj_uint16_t) pj_SOL_UDP(void)
{
    return PJ_SOL_UDP;
}

PJ_DEF(pj_uint16_t) pj_SOL_IPV6(void)
{
    return PJ_SOL_IPV6;
}

PJ_DEF(int) pj_IP_TOS(void)
{
    return PJ_IP_TOS;
}

PJ_DEF(int) pj_IPTOS_LOWDELAY(void)
{
    return PJ_IPTOS_LOWDELAY;
}

PJ_DEF(int) pj_IPTOS_THROUGHPUT(void)
{
    return PJ_IPTOS_THROUGHPUT;
}

PJ_DEF(int) pj_IPTOS_RELIABILITY(void)
{
    return PJ_IPTOS_RELIABILITY;
}

PJ_DEF(int) pj_IPTOS_MINCOST(void)
{
    return PJ_IPTOS_MINCOST;
}

PJ_DEF(pj_uint16_t) pj_SO_TYPE(void)
{
    return PJ_SO_TYPE;
}

PJ_DEF(pj_uint16_t) pj_SO_RCVBUF(void)
{
    return PJ_SO_RCVBUF;
}

PJ_DEF(pj_uint16_t) pj_SO_SNDBUF(void)
{
    return PJ_SO_SNDBUF;
}

PJ_DEF(int) pj_MSG_OOB(void)
{
    return PJ_MSG_OOB;
}

PJ_DEF(int) pj_MSG_PEEK(void)
{
    return PJ_MSG_PEEK;
}

PJ_DEF(int) pj_MSG_DONTROUTE(void)
{
    return PJ_MSG_DONTROUTE;
}

#endif	/* PJ_DLL */

