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
#include <pj/sock.h>
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/compat/socket.h>
#include <pj/addr_resolv.h>
#include <pj/errno.h>
#include <pj/unicode.h>

#define THIS_FILE	"sock_bsd.c"

/*
 * Address families conversion.
 * The values here are indexed based on pj_addr_family.
 */
const pj_uint16_t PJ_AF_UNSPEC	= AF_UNSPEC;
const pj_uint16_t PJ_AF_UNIX	= AF_UNIX;
const pj_uint16_t PJ_AF_INET	= AF_INET;
const pj_uint16_t PJ_AF_INET6	= AF_INET6;
#ifdef AF_PACKET
const pj_uint16_t PJ_AF_PACKET	= AF_PACKET;
#else
const pj_uint16_t PJ_AF_PACKET	= 0xFFFF;
#endif
#ifdef AF_IRDA
const pj_uint16_t PJ_AF_IRDA	= AF_IRDA;
#else
const pj_uint16_t PJ_AF_IRDA	= 0xFFFF;
#endif

/*
 * Socket types conversion.
 * The values here are indexed based on pj_sock_type
 */
const pj_uint16_t PJ_SOCK_STREAM= SOCK_STREAM;
const pj_uint16_t PJ_SOCK_DGRAM	= SOCK_DGRAM;
const pj_uint16_t PJ_SOCK_RAW	= SOCK_RAW;
const pj_uint16_t PJ_SOCK_RDM	= SOCK_RDM;

/*
 * Socket level values.
 */
const pj_uint16_t PJ_SOL_SOCKET	= SOL_SOCKET;
#ifdef SOL_IP
const pj_uint16_t PJ_SOL_IP	= SOL_IP;
#elif (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_WIN64) && PJ_WIN64) || \
      (defined (IPPROTO_IP))
const pj_uint16_t PJ_SOL_IP	= IPPROTO_IP;
#else
const pj_uint16_t PJ_SOL_IP	= 0;
#endif /* SOL_IP */

#if defined(SOL_TCP)
const pj_uint16_t PJ_SOL_TCP	= SOL_TCP;
#elif defined(IPPROTO_TCP)
const pj_uint16_t PJ_SOL_TCP	= IPPROTO_TCP;
#elif (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_WIN64) && PJ_WIN64)
const pj_uint16_t PJ_SOL_TCP	= IPPROTO_TCP;
#else
const pj_uint16_t PJ_SOL_TCP	= 6;
#endif /* SOL_TCP */

#ifdef SOL_UDP
const pj_uint16_t PJ_SOL_UDP	= SOL_UDP;
#elif defined(IPPROTO_UDP)
const pj_uint16_t PJ_SOL_UDP	= IPPROTO_UDP;
#elif (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_WIN64) && PJ_WIN64)
const pj_uint16_t PJ_SOL_UDP	= IPPROTO_UDP;
#else
const pj_uint16_t PJ_SOL_UDP	= 17;
#endif /* SOL_UDP */

#ifdef SOL_IPV6
const pj_uint16_t PJ_SOL_IPV6	= SOL_IPV6;
#elif (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_WIN64) && PJ_WIN64)
#   if defined(IPPROTO_IPV6) || (_WIN32_WINNT >= 0x0501)
	const pj_uint16_t PJ_SOL_IPV6	= IPPROTO_IPV6;
#   else
	const pj_uint16_t PJ_SOL_IPV6	= 41;
#   endif
#else
const pj_uint16_t PJ_SOL_IPV6	= 41;
#endif /* SOL_IPV6 */

/* IP_TOS */
#ifdef IP_TOS
const pj_uint16_t PJ_IP_TOS	= IP_TOS;
#else
const pj_uint16_t PJ_IP_TOS	= 1;
#endif


/* TOS settings (declared in netinet/ip.h) */
#ifdef IPTOS_LOWDELAY
const pj_uint16_t PJ_IPTOS_LOWDELAY	= IPTOS_LOWDELAY;
#else
const pj_uint16_t PJ_IPTOS_LOWDELAY	= 0x10;
#endif
#ifdef IPTOS_THROUGHPUT
const pj_uint16_t PJ_IPTOS_THROUGHPUT	= IPTOS_THROUGHPUT;
#else
const pj_uint16_t PJ_IPTOS_THROUGHPUT	= 0x08;
#endif
#ifdef IPTOS_RELIABILITY
const pj_uint16_t PJ_IPTOS_RELIABILITY	= IPTOS_RELIABILITY;
#else
const pj_uint16_t PJ_IPTOS_RELIABILITY	= 0x04;
#endif
#ifdef IPTOS_MINCOST
const pj_uint16_t PJ_IPTOS_MINCOST	= IPTOS_MINCOST;
#else
const pj_uint16_t PJ_IPTOS_MINCOST	= 0x02;
#endif


/* IPV6_TCLASS */
#ifdef IPV6_TCLASS
const pj_uint16_t PJ_IPV6_TCLASS = IPV6_TCLASS;
#else
const pj_uint16_t PJ_IPV6_TCLASS = 0xFFFF;
#endif


/* optname values. */
const pj_uint16_t PJ_SO_TYPE    = SO_TYPE;
const pj_uint16_t PJ_SO_RCVBUF  = SO_RCVBUF;
const pj_uint16_t PJ_SO_SNDBUF  = SO_SNDBUF;
const pj_uint16_t PJ_TCP_NODELAY= TCP_NODELAY;
const pj_uint16_t PJ_SO_REUSEADDR= SO_REUSEADDR;
#ifdef SO_NOSIGPIPE
const pj_uint16_t PJ_SO_NOSIGPIPE = SO_NOSIGPIPE;
#else
const pj_uint16_t PJ_SO_NOSIGPIPE = 0xFFFF;
#endif
#if defined(SO_PRIORITY)
const pj_uint16_t PJ_SO_PRIORITY = SO_PRIORITY;
#else
/* This is from Linux, YMMV */
const pj_uint16_t PJ_SO_PRIORITY = 12;
#endif

/* Multicasting is not supported e.g. in PocketPC 2003 SDK */
#ifdef IP_MULTICAST_IF
const pj_uint16_t PJ_IP_MULTICAST_IF    = IP_MULTICAST_IF;
const pj_uint16_t PJ_IP_MULTICAST_TTL   = IP_MULTICAST_TTL;
const pj_uint16_t PJ_IP_MULTICAST_LOOP  = IP_MULTICAST_LOOP;
const pj_uint16_t PJ_IP_ADD_MEMBERSHIP  = IP_ADD_MEMBERSHIP;
const pj_uint16_t PJ_IP_DROP_MEMBERSHIP = IP_DROP_MEMBERSHIP;
#else
const pj_uint16_t PJ_IP_MULTICAST_IF    = 0xFFFF;
const pj_uint16_t PJ_IP_MULTICAST_TTL   = 0xFFFF;
const pj_uint16_t PJ_IP_MULTICAST_LOOP  = 0xFFFF;
const pj_uint16_t PJ_IP_ADD_MEMBERSHIP  = 0xFFFF;
const pj_uint16_t PJ_IP_DROP_MEMBERSHIP = 0xFFFF;
#endif

/* recv() and send() flags */
const int PJ_MSG_OOB		= MSG_OOB;
const int PJ_MSG_PEEK		= MSG_PEEK;
const int PJ_MSG_DONTROUTE	= MSG_DONTROUTE;


#if 0
static void CHECK_ADDR_LEN(const pj_sockaddr *addr, int len)
{
    pj_sockaddr *a = (pj_sockaddr*)addr;
    pj_assert((a->addr.sa_family==PJ_AF_INET && len==sizeof(pj_sockaddr_in)) ||
	      (a->addr.sa_family==PJ_AF_INET6 && len==sizeof(pj_sockaddr_in6)));

}
#else
#define CHECK_ADDR_LEN(addr,len)
#endif

/*
 * Convert 16-bit value from network byte order to host byte order.
 */
PJ_DEF(pj_uint16_t) pj_ntohs(pj_uint16_t netshort)
{
    return ntohs(netshort);
}

/*
 * Convert 16-bit value from host byte order to network byte order.
 */
PJ_DEF(pj_uint16_t) pj_htons(pj_uint16_t hostshort)
{
    return htons(hostshort);
}

/*
 * Convert 32-bit value from network byte order to host byte order.
 */
PJ_DEF(pj_uint32_t) pj_ntohl(pj_uint32_t netlong)
{
    return ntohl(netlong);
}

/*
 * Convert 32-bit value from host byte order to network byte order.
 */
PJ_DEF(pj_uint32_t) pj_htonl(pj_uint32_t hostlong)
{
    return htonl(hostlong);
}

/*
 * Convert an Internet host address given in network byte order
 * to string in standard numbers and dots notation.
 */
PJ_DEF(char*) pj_inet_ntoa(pj_in_addr inaddr)
{
#if 0
    return inet_ntoa(*(struct in_addr*)&inaddr);
#else
    struct in_addr addr;
    //addr.s_addr = inaddr.s_addr;
    pj_memcpy(&addr, &inaddr, sizeof(addr));
    return inet_ntoa(addr);
#endif
}

/*
 * This function converts the Internet host address cp from the standard
 * numbers-and-dots notation into binary data and stores it in the structure
 * that inp points to. 
 */
PJ_DEF(int) pj_inet_aton(const pj_str_t *cp, pj_in_addr *inp)
{
    char tempaddr[PJ_INET_ADDRSTRLEN];

    /* Initialize output with PJ_INADDR_NONE.
     * Some apps relies on this instead of the return value
     * (and anyway the return value is quite confusing!)
     */
    inp->s_addr = PJ_INADDR_NONE;

    /* Caution:
     *	this function might be called with cp->slen >= 16
     *  (i.e. when called with hostname to check if it's an IP addr).
     */
    PJ_ASSERT_RETURN(cp && cp->slen && inp, 0);
    if (cp->slen >= PJ_INET_ADDRSTRLEN) {
	return 0;
    }

    pj_memcpy(tempaddr, cp->ptr, cp->slen);
    tempaddr[cp->slen] = '\0';

#if defined(PJ_SOCK_HAS_INET_ATON) && PJ_SOCK_HAS_INET_ATON != 0
    return inet_aton(tempaddr, (struct in_addr*)inp);
#else
    inp->s_addr = inet_addr(tempaddr);
    return inp->s_addr == PJ_INADDR_NONE ? 0 : 1;
#endif
}

/*
 * Convert text to IPv4/IPv6 address.
 */
PJ_DEF(pj_status_t) pj_inet_pton(int af, const pj_str_t *src, void *dst)
{
    char tempaddr[PJ_INET6_ADDRSTRLEN];

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EAFNOTSUP);
    PJ_ASSERT_RETURN(src && src->slen && dst, PJ_EINVAL);

    /* Initialize output with PJ_IN_ADDR_NONE for IPv4 (to be 
     * compatible with pj_inet_aton()
     */
    if (af==PJ_AF_INET) {
	((pj_in_addr*)dst)->s_addr = PJ_INADDR_NONE;
    }

    /* Caution:
     *	this function might be called with cp->slen >= 46
     *  (i.e. when called with hostname to check if it's an IP addr).
     */
    if (src->slen >= PJ_INET6_ADDRSTRLEN) {
	return PJ_ENAMETOOLONG;
    }

    pj_memcpy(tempaddr, src->ptr, src->slen);
    tempaddr[src->slen] = '\0';

#if defined(PJ_SOCK_HAS_INET_PTON) && PJ_SOCK_HAS_INET_PTON != 0
    /*
     * Implementation using inet_pton()
     */
    if (inet_pton(af, tempaddr, dst) != 1) {
	pj_status_t status = pj_get_netos_error();
	if (status == PJ_SUCCESS)
	    status = PJ_EUNKNOWN;

	return status;
    }

    return PJ_SUCCESS;

#elif defined(PJ_WIN32) || defined(PJ_WIN64) || defined(PJ_WIN32_WINCE)
    /*
     * Implementation on Windows, using WSAStringToAddress().
     * Should also work on Unicode systems.
     */
    {
	PJ_DECL_UNICODE_TEMP_BUF(wtempaddr,PJ_INET6_ADDRSTRLEN)
	pj_sockaddr sock_addr;
	int addr_len = sizeof(sock_addr);
	int rc;

	sock_addr.addr.sa_family = (pj_uint16_t)af;
	rc = WSAStringToAddress(
		PJ_STRING_TO_NATIVE(tempaddr,wtempaddr,sizeof(wtempaddr)), 
		af, NULL, (LPSOCKADDR)&sock_addr, &addr_len);
	if (rc != 0) {
	    /* If you get rc 130022 Invalid argument (WSAEINVAL) with IPv6,
	     * check that you have IPv6 enabled (install it in the network
	     * adapter).
	     */
	    pj_status_t status = pj_get_netos_error();
	    if (status == PJ_SUCCESS)
		status = PJ_EUNKNOWN;

	    return status;
	}

	if (sock_addr.addr.sa_family == PJ_AF_INET) {
	    pj_memcpy(dst, &sock_addr.ipv4.sin_addr, 4);
	    return PJ_SUCCESS;
	} else if (sock_addr.addr.sa_family == PJ_AF_INET6) {
	    pj_memcpy(dst, &sock_addr.ipv6.sin6_addr, 16);
	    return PJ_SUCCESS;
	} else {
	    pj_assert(!"Shouldn't happen");
	    return PJ_EBUG;
	}
    }
#elif !defined(PJ_HAS_IPV6) || PJ_HAS_IPV6==0
    /* IPv6 support is disabled, just return error without raising assertion */
    return PJ_EIPV6NOTSUP;
#else
    pj_assert(!"Not supported");
    return PJ_EIPV6NOTSUP;
#endif
}

/*
 * Convert IPv4/IPv6 address to text.
 */
PJ_DEF(pj_status_t) pj_inet_ntop(int af, const void *src,
				 char *dst, int size)

{
    PJ_ASSERT_RETURN(src && dst && size, PJ_EINVAL);

    *dst = '\0';

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EAFNOTSUP);

#if defined(PJ_SOCK_HAS_INET_NTOP) && PJ_SOCK_HAS_INET_NTOP != 0
    /*
     * Implementation using inet_ntop()
     */
    if (inet_ntop(af, src, dst, size) == NULL) {
	pj_status_t status = pj_get_netos_error();
	if (status == PJ_SUCCESS)
	    status = PJ_EUNKNOWN;

	return status;
    }

    return PJ_SUCCESS;

#elif defined(PJ_WIN32) || defined(PJ_WIN64) || defined(PJ_WIN32_WINCE)
    /*
     * Implementation on Windows, using WSAAddressToString().
     * Should also work on Unicode systems.
     */
    {
	PJ_DECL_UNICODE_TEMP_BUF(wtempaddr,PJ_INET6_ADDRSTRLEN)
	pj_sockaddr sock_addr;
	DWORD addr_len, addr_str_len;
	int rc;

	pj_bzero(&sock_addr, sizeof(sock_addr));
	sock_addr.addr.sa_family = (pj_uint16_t)af;
	if (af == PJ_AF_INET) {
	    if (size < PJ_INET_ADDRSTRLEN)
		return PJ_ETOOSMALL;
	    pj_memcpy(&sock_addr.ipv4.sin_addr, src, 4);
	    addr_len = sizeof(pj_sockaddr_in);
	    addr_str_len = PJ_INET_ADDRSTRLEN;
	} else if (af == PJ_AF_INET6) {
	    if (size < PJ_INET6_ADDRSTRLEN)
		return PJ_ETOOSMALL;
	    pj_memcpy(&sock_addr.ipv6.sin6_addr, src, 16);
	    addr_len = sizeof(pj_sockaddr_in6);
	    addr_str_len = PJ_INET6_ADDRSTRLEN;
	} else {
	    pj_assert(!"Unsupported address family");
	    return PJ_EAFNOTSUP;
	}

#if PJ_NATIVE_STRING_IS_UNICODE
	rc = WSAAddressToString((LPSOCKADDR)&sock_addr, addr_len,
				NULL, wtempaddr, &addr_str_len);
	if (rc == 0) {
	    pj_unicode_to_ansi(wtempaddr, wcslen(wtempaddr), dst, size);
	}
#else
	rc = WSAAddressToString((LPSOCKADDR)&sock_addr, addr_len,
				NULL, dst, &addr_str_len);
#endif

	if (rc != 0) {
	    pj_status_t status = pj_get_netos_error();
	    if (status == PJ_SUCCESS)
		status = PJ_EUNKNOWN;

	    return status;
	}

	return PJ_SUCCESS;
    }

#elif !defined(PJ_HAS_IPV6) || PJ_HAS_IPV6==0
    /* IPv6 support is disabled, just return error without raising assertion */
    return PJ_EIPV6NOTSUP;
#else
    pj_assert(!"Not supported");
    return PJ_EIPV6NOTSUP;
#endif
}

/*
 * Get hostname.
 */
PJ_DEF(const pj_str_t*) pj_gethostname(void)
{
    static char buf[PJ_MAX_HOSTNAME];
    static pj_str_t hostname;

    PJ_CHECK_STACK();

    if (hostname.ptr == NULL) {
	hostname.ptr = buf;
	if (gethostname(buf, sizeof(buf)) != 0) {
	    hostname.ptr[0] = '\0';
	    hostname.slen = 0;
	} else {
            hostname.slen = strlen(buf);
	}
    }
    return &hostname;
}

#if defined(PJ_WIN32) || defined(PJ_WIN64)
/*
 * Create new socket/endpoint for communication and returns a descriptor.
 */
PJ_DEF(pj_status_t) pj_sock_socket(int af, 
				   int type, 
				   int proto,
				   pj_sock_t *sock)
{
    PJ_CHECK_STACK();

    /* Sanity checks. */
    PJ_ASSERT_RETURN(sock!=NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN((SOCKET)PJ_INVALID_SOCKET==INVALID_SOCKET, 
                     (*sock=PJ_INVALID_SOCKET, PJ_EINVAL));

    *sock = WSASocket(af, type, proto, NULL, 0, WSA_FLAG_OVERLAPPED);

    if (*sock == PJ_INVALID_SOCKET) 
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    
#if PJ_SOCK_DISABLE_WSAECONNRESET && \
    (!defined(PJ_WIN32_WINCE) || PJ_WIN32_WINCE==0)

#ifndef SIO_UDP_CONNRESET
    #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
#endif

    /* Disable WSAECONNRESET for UDP.
     * See https://trac.pjsip.org/repos/ticket/1197
     */
    if (type==PJ_SOCK_DGRAM) {
	DWORD dwBytesReturned = 0;
	BOOL bNewBehavior = FALSE;
	DWORD rc;

	rc = WSAIoctl(*sock, SIO_UDP_CONNRESET,
		      &bNewBehavior, sizeof(bNewBehavior),
		      NULL, 0, &dwBytesReturned,
		      NULL, NULL);

	if (rc==SOCKET_ERROR) {
	    // Ignored..
	}
    }
#endif

    return PJ_SUCCESS;
}

#else
/*
 * Create new socket/endpoint for communication and returns a descriptor.
 */
PJ_DEF(pj_status_t) pj_sock_socket(int af, 
				   int type, 
				   int proto, 
				   pj_sock_t *sock)
{

    PJ_CHECK_STACK();

    /* Sanity checks. */
    PJ_ASSERT_RETURN(sock!=NULL, PJ_EINVAL);
    PJ_ASSERT_RETURN(PJ_INVALID_SOCKET==-1, 
                     (*sock=PJ_INVALID_SOCKET, PJ_EINVAL));
    
    *sock = socket(af, type, proto);
    if (*sock == PJ_INVALID_SOCKET)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else {
	pj_int32_t val = 1;
	if (type == pj_SOCK_STREAM()) {
	    pj_sock_setsockopt(*sock, pj_SOL_SOCKET(), pj_SO_NOSIGPIPE(),
			       &val, sizeof(val));
	}
#if defined(PJ_SOCK_HAS_IPV6_V6ONLY) && PJ_SOCK_HAS_IPV6_V6ONLY != 0
	if (af == PJ_AF_INET6) {
	    pj_sock_setsockopt(*sock, PJ_SOL_IPV6, IPV6_V6ONLY,
			       &val, sizeof(val));
	}
#endif
#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
	if (type == pj_SOCK_DGRAM()) {
	    pj_sock_setsockopt(*sock, pj_SOL_SOCKET(), SO_NOSIGPIPE, 
			       &val, sizeof(val));
	}
#endif
	return PJ_SUCCESS;
    }
}
#endif

/*
 * Bind socket.
 */
PJ_DEF(pj_status_t) pj_sock_bind( pj_sock_t sock, 
				  const pj_sockaddr_t *addr,
				  int len)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(addr && len >= (int)sizeof(struct sockaddr_in), PJ_EINVAL);

    CHECK_ADDR_LEN(addr, len);

    if (bind(sock, (struct sockaddr*)addr, len) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}


/*
 * Bind socket.
 */
PJ_DEF(pj_status_t) pj_sock_bind_in( pj_sock_t sock, 
				     pj_uint32_t addr32,
				     pj_uint16_t port)
{
    pj_sockaddr_in addr;

    PJ_CHECK_STACK();

    PJ_SOCKADDR_SET_LEN(&addr, sizeof(pj_sockaddr_in));
    addr.sin_family = PJ_AF_INET;
    pj_bzero(addr.sin_zero_pad, sizeof(addr.sin_zero_pad));
    addr.sin_addr.s_addr = pj_htonl(addr32);
    addr.sin_port = pj_htons(port);

    return pj_sock_bind(sock, &addr, sizeof(pj_sockaddr_in));
}


/*
 * Close socket.
 */
PJ_DEF(pj_status_t) pj_sock_close(pj_sock_t sock)
{
    int rc;

    PJ_CHECK_STACK();
#if defined(PJ_WIN32) && PJ_WIN32!=0 || \
    defined(PJ_WIN64) && PJ_WIN64 != 0 || \
    defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
    rc = closesocket(sock);
#else
    rc = close(sock);
#endif

    if (rc != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Get remote's name.
 */
PJ_DEF(pj_status_t) pj_sock_getpeername( pj_sock_t sock,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    PJ_CHECK_STACK();
    if (getpeername(sock, (struct sockaddr*)addr, (socklen_t*)namelen) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else {
	PJ_SOCKADDR_RESET_LEN(addr);
	return PJ_SUCCESS;
    }
}

/*
 * Get socket name.
 */
PJ_DEF(pj_status_t) pj_sock_getsockname( pj_sock_t sock,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    PJ_CHECK_STACK();
    if (getsockname(sock, (struct sockaddr*)addr, (socklen_t*)namelen) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else {
	PJ_SOCKADDR_RESET_LEN(addr);
	return PJ_SUCCESS;
    }
}

/*
 * Send data
 */
PJ_DEF(pj_status_t) pj_sock_send(pj_sock_t sock,
				 const void *buf,
				 pj_ssize_t *len,
				 unsigned flags)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(len, PJ_EINVAL);

#ifdef MSG_NOSIGNAL
    /* Suppress SIGPIPE. See https://trac.pjsip.org/repos/ticket/1538 */
    flags |= MSG_NOSIGNAL;
#endif

    *len = send(sock, (const char*)buf, (int)(*len), flags);

    if (*len < 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}


/*
 * Send data.
 */
PJ_DEF(pj_status_t) pj_sock_sendto(pj_sock_t sock,
				   const void *buf,
				   pj_ssize_t *len,
				   unsigned flags,
				   const pj_sockaddr_t *to,
				   int tolen)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(len, PJ_EINVAL);
    
    CHECK_ADDR_LEN(to, tolen);

    *len = sendto(sock, (const char*)buf, (int)(*len), flags, 
		  (const struct sockaddr*)to, tolen);

    if (*len < 0) 
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else 
	return PJ_SUCCESS;
}

/*
 * Receive data.
 */
PJ_DEF(pj_status_t) pj_sock_recv(pj_sock_t sock,
				 void *buf,
				 pj_ssize_t *len,
				 unsigned flags)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(buf && len, PJ_EINVAL);

    *len = recv(sock, (char*)buf, (int)(*len), flags);

    if (*len < 0) 
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Receive data.
 */
PJ_DEF(pj_status_t) pj_sock_recvfrom(pj_sock_t sock,
				     void *buf,
				     pj_ssize_t *len,
				     unsigned flags,
				     pj_sockaddr_t *from,
				     int *fromlen)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(buf && len, PJ_EINVAL);

    *len = recvfrom(sock, (char*)buf, (int)(*len), flags, 
		    (struct sockaddr*)from, (socklen_t*)fromlen);

    if (*len < 0) 
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else {
        if (from) {
            PJ_SOCKADDR_RESET_LEN(from);
        }
	return PJ_SUCCESS;
    }
}

/*
 * Get socket option.
 */
PJ_DEF(pj_status_t) pj_sock_getsockopt( pj_sock_t sock,
					pj_uint16_t level,
					pj_uint16_t optname,
					void *optval,
					int *optlen)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(optval && optlen, PJ_EINVAL);

    if (getsockopt(sock, level, optname, (char*)optval, (socklen_t*)optlen)!=0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Set socket option.
 */
PJ_DEF(pj_status_t) pj_sock_setsockopt( pj_sock_t sock,
					pj_uint16_t level,
					pj_uint16_t optname,
					const void *optval,
					int optlen)
{
    int status;
    PJ_CHECK_STACK();
    
#if (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_SUNOS) && PJ_SUNOS)
    /* Some opt may still need int value (e.g:SO_EXCLUSIVEADDRUSE in win32). */
    status = setsockopt(sock, 
		     level, 
		     ((optname&0xff00)==0xff00)?(int)optname|0xffff0000:optname, 		     
		     (const char*)optval, optlen);
#else
    status = setsockopt(sock, level, optname, (const char*)optval, optlen);
#endif

    if (status != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Set socket option.
 */
PJ_DEF(pj_status_t) pj_sock_setsockopt_params( pj_sock_t sockfd,
					       const pj_sockopt_params *params)
{
    unsigned int i = 0;
    pj_status_t retval = PJ_SUCCESS;
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(params, PJ_EINVAL);
    
    for (;i<params->cnt && i<PJ_MAX_SOCKOPT_PARAMS;++i) {
	pj_status_t status = pj_sock_setsockopt(sockfd, 
					(pj_uint16_t)params->options[i].level,
					(pj_uint16_t)params->options[i].optname,
					params->options[i].optval, 
					params->options[i].optlen);
	if (status != PJ_SUCCESS) {
	    retval = status;
	    PJ_PERROR(4,(THIS_FILE, status,
			 "Warning: error applying sock opt %d",
			 params->options[i].optname));
	}
    }

    return retval;
}

/*
 * Connect socket.
 */
PJ_DEF(pj_status_t) pj_sock_connect( pj_sock_t sock,
				     const pj_sockaddr_t *addr,
				     int namelen)
{
    PJ_CHECK_STACK();
    if (connect(sock, (struct sockaddr*)addr, namelen) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}


/*
 * Shutdown socket.
 */
#if PJ_HAS_TCP
PJ_DEF(pj_status_t) pj_sock_shutdown( pj_sock_t sock,
				      int how)
{
    PJ_CHECK_STACK();
    if (shutdown(sock, how) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Start listening to incoming connections.
 */
PJ_DEF(pj_status_t) pj_sock_listen( pj_sock_t sock,
				    int backlog)
{
    PJ_CHECK_STACK();
    if (listen(sock, backlog) != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}

/*
 * Accept incoming connections
 */
PJ_DEF(pj_status_t) pj_sock_accept( pj_sock_t serverfd,
				    pj_sock_t *newsock,
				    pj_sockaddr_t *addr,
				    int *addrlen)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(newsock != NULL, PJ_EINVAL);

#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
    if (addr) {
	PJ_SOCKADDR_SET_LEN(addr, *addrlen);
    }
#endif
    
    *newsock = accept(serverfd, (struct sockaddr*)addr, (socklen_t*)addrlen);
    if (*newsock==PJ_INVALID_SOCKET)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else {
	
#if defined(PJ_SOCKADDR_HAS_LEN) && PJ_SOCKADDR_HAS_LEN!=0
	if (addr) {
	    PJ_SOCKADDR_RESET_LEN(addr);
	}
#endif
	    
	return PJ_SUCCESS;
    }
}
#endif	/* PJ_HAS_TCP */


