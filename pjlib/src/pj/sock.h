/* $Header: /pjproject/pjlib/src/pj/sock.h 7     5/28/05 9:59a Bennylp $ */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __PJ_SOCK_H__
#define __PJ_SOCK_H__

/**
 * @file sock.h
 * @brief Socket Abstraction.
 */

#include <pj/types.h>

PJ_BEGIN_DECL 

#if defined(PJ_WIN32) && PJ_WIN32==1
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <sys/select.h>
#  include <arpa/inet.h>
#  include <sys/ioctl.h>	/* FBIONBIO */
#  include <errno.h>
#  include <netdb.h>
#  include <unistd.h>
#endif

/**
 * @defgroup PJ_SOCK Socket Abstraction
 * @ingroup PJ_IO
 * @{
 */

/** 
 * Macro alias for AF_INET. 
 */
#define PJ_AF_INET	    AF_INET

/** 
 * Macro alias for SOCK_STREAM (only available if PJ_HAS_TCP != 0). 
 */
#if PJ_HAS_TCP
#  define PJ_SOCK_STREAM    SOCK_STREAM
#endif

/** 
 * Macro alias for SOCK_DGRAM. 
 */
#define PJ_SOCK_DGRAM	    SOCK_DGRAM

/**
 * Macro for invalid address value.
 */
#define PJ_INADDR_NONE	    INADDR_NONE

/**
 * Macro for INADDR_ANY
 */
#define PJ_INADDR_ANY	    INADDR_ANY

/*
 * Socket flags.
 */
#define PJ_SOCK_ASYNC	    1


/*
 * Socket options.
 */
#define PJ_FIONBIO	    FIONBIO

/*
 * Socket errors.
 */
#ifdef _WIN32
#define PJ_EWOULDBLOCK	    WSAEWOULDBLOCK
#define PJ_ECONNRESET	    WSAECONNRESET
#define PJ_EINPROGRESS	    WSAEINPROGRESS
#else
#define PJ_EWOULDBLOCK	    EWOULDBLOCK
#define PJ_EINPROGRESS	    EINPROGRESS
#define PJ_ECONNRESET	    ECONNRESET
#endif

/**
 * Invalid socket.
 */
#define PJ_INVALID_SOCKET   (-1)

/**
 * Type definition for sockaddr_in.
 */
typedef struct sockaddr_in pj_sockaddr_in;

/**
 * Type alias for fd_set.
 */
typedef fd_set pj_fdset_t;

/**
 * Clear all handles.
 */
#define PJ_FD_ZERO(a)	    FD_ZERO(a)

/**
 * Set handle membership.
 */
PJ_INLINE(void) PJ_FD_SET(pj_sock_t sock, pj_fdset_t *set)
{
#ifdef _MSC_VER
#  pragma warning(disable:4018)
#  pragma warning(disable:4389)	// sign/unsigned mismatch in comparison
#endif

    FD_SET(sock,set);

#ifdef _MSC_VER
#  pragma warning(default:4018)
#  pragma warning(default:4389)
#endif
}

/**
 * Check handle membership.
 */
#define PJ_FD_ISSET(s,a)    FD_ISSET(s,a)

/**
 * Clear handle membership.
 */
PJ_INLINE(void) PJ_FD_CLR(pj_sock_t sock, pj_fdset_t *set)
{
#ifdef _MSC_VER
#  pragma warning(disable:4018)
#  pragma warning(disable:4389)	// sign/unsigned mismatch in comparison
#endif

    FD_CLR(sock,set);

#ifdef _MSC_VER
#  pragma warning(default:4018)
#  pragma warning(default:4389)
#endif
}

#define pj_ntohs	ntohs
#define pj_ntohl	ntohl
#define pj_htons	htons
#define pj_htonl	htonl
#define pj_inet_ntoa	inet_ntoa


/**
 * Get the port number of an address.
 */
#define pj_sockaddr_get_port(addr)	pj_ntohs((addr)->sin_port)

/**
 * Get the address.
 */
#define pj_sockaddr_get_addr(addr)	pj_ntohl((addr)->sin_addr.s_addr)

/**
 * Set the port number.
 */
#define pj_sockaddr_set_port(addr,port)	((addr)->sin_port = pj_htons((unsigned short)port))

/**
 * Set the address.
 */
#define pj_sockaddr_set_addr(addr,ad)	((addr)->sin_addr.s_addr = pj_htonl(ad))

/**
 * Get the string address.
 */
#define pj_sockaddr_get_str_addr(addr)	inet_ntoa((addr)->sin_addr)

/**
 * Convert string representation to IP address.
 */
PJ_DECL(pj_uint32_t) pj_inet_addr(const pj_str_t *addr);

/**
 * Convert string representation to IP address.
 */
PJ_IDECL(pj_uint32_t) pj_inet_addr2(const char *addr);

/**
 * Set the address from string address, resolve the host if necessary.
 */
PJ_DECL(pj_status_t) pj_sockaddr_set_str_addr( pj_sockaddr_in *addr,
					       const pj_str_t *str_addr);

/**
 * Set the address from string address, resolve the host if necessary.
 */
PJ_DECL(pj_status_t) pj_sockaddr_set_str_addr2( pj_sockaddr_in *addr,
					 	const char *str_addr);

/**
 * Set the address and port.
 */
PJ_DECL(pj_status_t) pj_sockaddr_init( pj_sockaddr_in *addr,
				        const pj_str_t *str_addr,
				        int port);

PJ_DECL(pj_status_t) pj_sockaddr_init2( pj_sockaddr_in *addr,
				        const char *str_addr,
				        int port);

/**
 * Compare addresses.
 * @return zero if equal, -1 if addr1<addr2, 1 if addr1>addr2.
 */
PJ_DECL(int) pj_sockaddr_cmp( const pj_sockaddr_in *addr1,
			      const pj_sockaddr_in *addr2);

/**
 * Get system's host name.
 * @return Pointer to string
 */
PJ_DECL(const pj_str_t*) pj_gethostname(void);

/**
 * Get host's IP address.
 * @return host's IP address.
 */
PJ_DECL(pj_uint32_t) pj_gethostaddr(void);

/*
 *
 * SOCKET API.
 *
 */

/**
 * Create socket.
 * If PJ_SOCK_ASYNC is specified during creation, then for WINNT sockets, the
 * socket will be created with flag FILE_FLAG_OVERLAPPED. This flag doesn't 
 * have any effects on other types of socket.
 */
PJ_DECL(pj_sock_t) pj_sock_socket(int af, int type, int proto,
				  pj_uint32_t flag);

/**
 * Bind socket.
 */
PJ_IDECL(pj_status_t) pj_sock_bind(pj_sock_t sock, 
				   const pj_sockaddr_t *addr,
				   int len);

/**
 * Bind IP socket.
 */
PJ_DECL(pj_status_t) pj_sock_bind_in( pj_sock_t sock, 
				      pj_uint32_t addr,
				      pj_uint16_t port);

/**
 * Accept new connection (only available if PJ_HAS_TCP != 0).
 */
#if PJ_HAS_TCP
PJ_IDECL(pj_sock_t) pj_sock_accept(pj_sock_t sock,
				   pj_sockaddr_t *addr,
				   int *addrlen);
#endif

/**
 * Close socket.
 */
PJ_IDECL(pj_status_t) pj_sock_close(pj_sock_t sock);


/**
 * Connect socket.
 */
PJ_IDECL(pj_status_t) pj_sock_connect(pj_sock_t sock,
				      const pj_sockaddr_t *addr,
				      int namelen);

/**
 * Get peer address.
 */
PJ_IDECL(pj_status_t) pj_sock_getpeername(pj_sock_t sock,
					  pj_sockaddr_t *addr,
					  int *namelen);

/**
 * Get local address.
 */
PJ_IDECL(pj_status_t) pj_sock_getsockname(pj_sock_t sock,
					  pj_sockaddr_t *addr,
					  int *namelen);

/**
 * Get socket option.
 */
PJ_IDECL(pj_status_t) pj_sock_getsockopt(pj_sock_t sock,
					 int level,
					 int optname,
					 void *optval,
					 int *optlen);
/**
 * Set socket option.
 */
PJ_IDECL(pj_status_t) pj_sock_setsockopt(pj_sock_t sock,
					 int level,
					 int optname,
					 const void *optval,
					 int optlen);

/**
 * Set socket option.
 */
PJ_IDECL(pj_status_t) pj_sock_ioctl(pj_sock_t sock,
				    long cmd,
				    pj_uint32_t *val);

/**
 * Listen for incoming connection (only available if PJ_HAS_TCP != 0).
 */
#if PJ_HAS_TCP
PJ_IDECL(pj_status_t) pj_sock_listen(pj_sock_t sock,
				     int backlog);
#endif

/**
 * Receives data.
 */
PJ_IDECL(int) pj_sock_recv(pj_sock_t sock,
			   void *buf,
			   int len,
			   int flags);

/**
 * Receive data.
 */
PJ_IDECL(int) pj_sock_recvfrom(pj_sock_t sock,
			       void *buf,
			       int len,
			       int flags,
			       pj_sockaddr_t *from,
			       int *fromlen);

/**
 * Legacy select().
 */
PJ_DECL(int) pj_sock_select( int nfds,
			     pj_fdset_t *readfd,
			     pj_fdset_t *writefd,
			     pj_fdset_t *exfd,
			     const pj_time_val *timeout);

/**
 * Send data.
 */
PJ_IDECL(int) pj_sock_send(pj_sock_t sock,
			   const void *buf,
			   int len,
			   int flags);

/**
 * Send data.
 */
PJ_IDECL(int) pj_sock_sendto(pj_sock_t sock,
			     const void *buf,
			     int len,
			     int flags,
			     const pj_sockaddr_t *to,
			     int tolen);

/**
 * Shutdown socket (only available if PJ_HAS_TCP != 0).
 */
#if PJ_HAS_TCP
PJ_IDECL(pj_status_t) pj_sock_shutdown(pj_sock_t sock,
				       int how);
#endif

/**
 * Get last error of the network/socket operation.
 */
PJ_IDECL(pj_status_t) pj_sock_getlasterror(void);

/**
 * @}
 */

#if PJ_FUNCTIONS_ARE_INLINED
#  include <pj/sock_i.h>
#endif

PJ_END_DECL

#endif	/* __PJ_SOCK_H__ */

