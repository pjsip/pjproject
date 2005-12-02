/* $Header: /pjproject/pjlib/src/pj/sock_i.h 3     5/24/05 12:14a Bennylp $ */
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


#include <pj/string.h>

PJ_IDEF(pj_uint32_t) pj_inet_addr2(const char *addr)
{
    return inet_addr(addr);
}

PJ_IDEF(pj_status_t) pj_sock_bind( pj_sock_t sock, 
				  const pj_sockaddr_t *addr,
				  int len)
{
    return bind(sock, (struct sockaddr*)addr, len);
}


PJ_IDEF(pj_status_t) pj_sock_close(pj_sock_t sock)
{
#ifdef PJ_WIN32
    return closesocket(sock);
#else
    return close(sock);
#endif
}

PJ_IDEF(pj_status_t) pj_sock_getpeername( pj_sock_t sock,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    return getpeername(sock, (struct sockaddr*)addr, namelen);
}

PJ_IDEF(pj_status_t) pj_sock_getsockname( pj_sock_t sock,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    return getsockname(sock, (struct sockaddr*)addr, namelen);
}

PJ_IDEF(pj_status_t) pj_sock_getsockopt( pj_sock_t sock,
					int level,
					int optname,
					void *optval,
					int *optlen)
{
    return getsockopt(sock, level, optname, (char*)optval, optlen);
}

PJ_IDEF(int) pj_sock_send( pj_sock_t sock,
			  const void *buf,
			  int len,
			  int flags)
{
    return send(sock, (const char*)buf, len, flags);
}


PJ_IDEF(int) pj_sock_sendto( pj_sock_t sock,
			    const void *buf,
			    int len,
			    int flags,
			    const pj_sockaddr_t *to,
			    int tolen)
{
    return sendto(sock, (const char*)buf, len, flags, 
		  (const struct sockaddr*)to, tolen);
}

PJ_IDEF(int) pj_sock_recv( pj_sock_t sock,
			  void *buf,
			  int len,
			  int flags)
{
    return recv(sock, (char*)buf, len, flags);
}

PJ_IDEF(int) pj_sock_recvfrom( pj_sock_t sock,
			      void *buf,
			      int len,
			      int flags,
			      pj_sockaddr_t *from,
			      int *fromlen)
{
    return recvfrom(sock, (char*)buf, len, flags, 
		    (struct sockaddr*)from, fromlen);
}

PJ_IDEF(pj_status_t) pj_sock_setsockopt( pj_sock_t sock,
					int level,
					int optname,
					const void *optval,
					int optlen)
{
    return setsockopt(sock, level, optname, (const char*)optval, optlen);
}

#if PJ_HAS_TCP
PJ_IDEF(pj_status_t) pj_sock_shutdown(pj_sock_t sock,
					int how)
{
    return shutdown(sock, how);
}

PJ_IDEF(pj_status_t) pj_sock_listen( pj_sock_t sock,
				    int backlog)
{
    return listen(sock, backlog);
}

PJ_IDEF(pj_status_t) pj_sock_connect( pj_sock_t sock,
				     const pj_sockaddr_t *addr,
				     int namelen)
{
    return connect(sock, (struct sockaddr*)addr, namelen);
}

PJ_IDEF(pj_sock_t) pj_sock_accept( pj_sock_t sock,
				  pj_sockaddr_t *addr,
				  int *addrlen)
{
    return accept(sock, (struct sockaddr*)addr, addrlen);
}
#endif	/* PJ_HAS_TCP */

PJ_IDEF(pj_status_t) pj_sock_ioctl( pj_sock_t sock,
				   long cmd,
				   pj_uint32_t *val)
{
#if defined(PJ_WIN32) && PJ_WIN32==1
    return ioctlsocket(sock, cmd, (unsigned long*)val);
#else
    return ioctl(sock, cmd, (unsigned long*)val);
#endif
}

