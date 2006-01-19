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
#include <pj/sock.h>
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/compat/socket.h>
#include <pj/addr_resolv.h>
#include <pj/errno.h>

/*
 * Address families conversion.
 * The values here are indexed based on pj_addr_family-0xFF00.
 */
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
 * The values here are indexed based on pj_sock_type-0xFF00
 */
const pj_uint16_t PJ_SOCK_STREAM	= SOCK_STREAM;
const pj_uint16_t PJ_SOCK_DGRAM	= SOCK_DGRAM;
const pj_uint16_t PJ_SOCK_RAW	= SOCK_RAW;
const pj_uint16_t PJ_SOCK_RDM	= SOCK_RDM;

/*
 * Socket level values.
 */
const pj_uint16_t PJ_SOL_SOCKET	= SOL_SOCKET;
#ifdef SOL_IP
const pj_uint16_t PJ_SOL_IP	= SOL_IP;
#else
const pj_uint16_t PJ_SOL_IP	= 0xFFFF;
#endif /* SOL_IP */
#if defined(SOL_TCP)
const pj_uint16_t PJ_SOL_TCP	= SOL_TCP;
#elif defined(IPPROTO_TCP)
const pj_uint16_t PJ_SOL_TCP	= IPPROTO_TCP;
#endif /* SOL_TCP */
#ifdef SOL_UDP
const pj_uint16_t PJ_SOL_UDP	= SOL_UDP;
#else
const pj_uint16_t PJ_SOL_UDP	= 0xFFFF;
#endif
#ifdef SOL_IPV6
const pj_uint16_t PJ_SOL_IPV6	= SOL_IPV6;
#else
const pj_uint16_t PJ_SOL_IPV6	= 0xFFFF;
#endif

/* optname values. */
const pj_uint16_t PJ_SO_TYPE    = SO_TYPE;
const pj_uint16_t PJ_SO_RCVBUF  = SO_RCVBUF;
const pj_uint16_t PJ_SO_SNDBUF  = SO_SNDBUF;


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
#if !defined(PJ_LINUX) && !defined(PJ_LINUX_KERNEL)
    return inet_ntoa(*(struct in_addr*)&inaddr);
#else
    struct in_addr addr;
    addr.s_addr = inaddr.s_addr;
    return inet_ntoa(addr);
#endif
}

/*
 * This function converts the Internet host address cp from the standard
 * numbers-and-dots notation into binary data and stores it in the structure
 * that inp points to. 
 */
PJ_DEF(int) pj_inet_aton(const pj_str_t *cp, struct pj_in_addr *inp)
{
    char tempaddr[16];

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
    if (cp->slen >= 16) {
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
 * Convert address string with numbers and dots to binary IP address.
 */ 
PJ_DEF(pj_in_addr) pj_inet_addr(const pj_str_t *cp)
{
    pj_in_addr addr;

    pj_inet_aton(cp, &addr);
    return addr;
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

    addr->sin_family = AF_INET;

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

    addr->sin_family = PJ_AF_INET;
    pj_sockaddr_in_set_port(addr, port);
    return pj_sockaddr_in_set_str_addr(addr, str_addr);
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


#if defined(PJ_WIN32)
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
    PJ_ASSERT_RETURN((unsigned)PJ_INVALID_SOCKET==INVALID_SOCKET, 
                     (*sock=PJ_INVALID_SOCKET, PJ_EINVAL));

    *sock = WSASocket(af, type, proto, NULL, 0, WSA_FLAG_OVERLAPPED);

    if (*sock == PJ_INVALID_SOCKET) 
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
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
    else 
	return PJ_SUCCESS;
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

    PJ_ASSERT_RETURN(addr && len > 0, PJ_EINVAL);

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

    addr.sin_family = PJ_AF_INET;
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
    else
	return PJ_SUCCESS;
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
    else
	return PJ_SUCCESS;
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

    *len = send(sock, (const char*)buf, *len, flags);

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

    *len = sendto(sock, (const char*)buf, *len, flags, 
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

    *len = recv(sock, (char*)buf, *len, flags);

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
    PJ_ASSERT_RETURN(from && fromlen, (*len=-1, PJ_EINVAL));

    *len = recvfrom(sock, (char*)buf, *len, flags, 
		    (struct sockaddr*)from, (socklen_t*)fromlen);

    if (*len < 0) 
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
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
    PJ_CHECK_STACK();
    if (setsockopt(sock, level, optname, (const char*)optval, optlen) != 0)
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
 * Accept incoming connections
 */
PJ_DEF(pj_status_t) pj_sock_accept( pj_sock_t serverfd,
				    pj_sock_t *newsock,
				    pj_sockaddr_t *addr,
				    int *addrlen)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(newsock != NULL, PJ_EINVAL);

    *newsock = accept(serverfd, (struct sockaddr*)addr, (socklen_t*)addrlen);
    if (*newsock==PJ_INVALID_SOCKET)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());
    else
	return PJ_SUCCESS;
}
#endif	/* PJ_HAS_TCP */


