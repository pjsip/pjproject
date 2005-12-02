/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/sock.c,v 1.1 2005/12/02 20:02:30 nn Exp $ */
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
#include <pj/sock.h>
#include <pj/pool.h>
#if !PJ_WIN32_WINCE
#include <errno.h>
#endif

#if !PJ_FUNCTIONS_ARE_INLINED
#  include <pj/sock_i.h>
#endif

#if defined(PJ_WIN32_WINNT) || PJ_WIN32_WINNT >= 0x0350

PJ_DEF(pj_sock_t) pj_sock_socket( int af, int type, int proto,
				  pj_uint32_t flag)
{
    SOCKET sock;

    if (flag & PJ_SOCK_ASYNC) {
	flag = WSA_FLAG_OVERLAPPED;
	sock = WSASocket(af, type, proto, NULL, 0, flag);
    } else {
	flag = 0;
	sock = socket(af, type, proto);
    }

    return sock;
}

#else

PJ_DEF(pj_sock_t) pj_sock_socket(int af, int type, int proto,
				 pj_uint32_t flag)
{
    int sock;

    sock = socket(af, type, proto);
    if (sock && (flag & PJ_SOCK_ASYNC)) {
	pj_uint32_t val=1;
	if (pj_sock_ioctl(sock, PJ_FIONBIO, &val)) {
	    pj_sock_close(sock);
	    return -1;
	}
    }
    return sock;
}

#endif


PJ_DEF(const pj_str_t*) pj_gethostname(void)
{
    static char buf[PJ_MAX_HOSTNAME];
    static pj_str_t hostname;

    if (hostname.ptr == NULL) {
	hostname.ptr = buf;
	if (gethostname(buf, sizeof(buf)) != 0) {
	    hostname.ptr[0] = '\0';
	    hostname.slen = 0;
	    return &hostname;
	}
	hostname.slen = strlen(buf);
    }
    return &hostname;
}

PJ_DEF(pj_uint32_t) pj_gethostaddr(void)
{
    pj_sockaddr_in addr;
    const pj_str_t *hostname = pj_gethostname();

    if (pj_sockaddr_set_str_addr(&addr, hostname) != 0)
	return 0;

    return addr.sin_addr.s_addr;
}

PJ_DEF(pj_status_t) pj_sock_bind_in( pj_sock_t sock, 
				     pj_uint32_t addr32,
				     pj_uint16_t port)
{
    pj_sockaddr_in addr;
    pj_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = pj_htonl(addr32);
    addr.sin_port = pj_htons(port);
    return bind(sock, (struct sockaddr*)&addr, sizeof(addr));
}

PJ_DEF(int) pj_sock_select( int nfds,
			    pj_fdset_t *readfd,
			    pj_fdset_t *writefd,
			    pj_fdset_t *exfd,
			    const pj_time_val *timeout)
{
    struct timeval *tv_timeout = NULL, tv;
    if (timeout) {
	tv.tv_sec = timeout->sec;
	tv.tv_usec = timeout->msec * 1000;
	tv_timeout = &tv;
    }
    return select(nfds, readfd, writefd, exfd, tv_timeout);
}

PJ_DEF(pj_status_t) pj_sock_getlasterror(void)
{
#if defined(PJ_WIN32) && (PJ_WIN32==1 || PJ_WIN32==2)
    return WSAGetLastError();
#else
    return errno;
#endif
}


PJ_DEF(pj_uint32_t) pj_inet_addr(const pj_str_t *addr)
{
    char hostname[PJ_MAX_HOSTNAME];
    
    if (addr->slen >= PJ_MAX_HOSTNAME)
	return PJ_INADDR_NONE;

    pj_memcpy(hostname, addr->ptr, addr->slen);
    hostname[ addr->slen ] = '\0';

    return pj_inet_addr2(hostname);
}

PJ_DEF(pj_status_t) pj_sockaddr_set_str_addr2( pj_sockaddr_in *addr,
					       const char *hostname)
{
    addr->sin_family = AF_INET;
    if (hostname) {
	addr->sin_addr.s_addr = pj_inet_addr2(hostname);
	if (addr->sin_addr.s_addr == PJ_INADDR_NONE) {
	    struct hostent *he;

	    he = gethostbyname(hostname);
	    if (he) {
		addr->sin_addr.s_addr = *(pj_uint32_t*)he->h_addr;
	    } else {
		return -1;
	    }
	}
    } else {
	addr->sin_addr.s_addr = 0;
    }
    return 0;
}

PJ_DEF(pj_status_t) pj_sockaddr_set_str_addr( pj_sockaddr_in *addr,
					      const pj_str_t *str_addr)
{
    if (str_addr && str_addr->slen) {
	char hostname[PJ_MAX_HOSTNAME];
	if (str_addr->slen >= PJ_MAX_HOSTNAME)
	    return -1;
	pj_memcpy(hostname, str_addr->ptr, str_addr->slen);
	hostname[ str_addr->slen ] = '\0';

	return pj_sockaddr_set_str_addr2(addr, hostname);
    } else {
	return pj_sockaddr_set_str_addr2(addr, NULL);
    }
}

PJ_DEF(pj_status_t) pj_sockaddr_init( pj_sockaddr_in *addr,
				       const pj_str_t *str_addr,
				       int port)
{
    addr->sin_family = PJ_AF_INET;
    pj_sockaddr_set_port(addr, port);
    return pj_sockaddr_set_str_addr(addr, str_addr);
}

PJ_DEF(pj_status_t) pj_sockaddr_init2( pj_sockaddr_in *addr,
				       const char *str_addr,
				       int port)
{
    addr->sin_family = PJ_AF_INET;
    pj_sockaddr_set_port(addr, port);
    return pj_sockaddr_set_str_addr2(addr, str_addr);
}

PJ_DEF(int) pj_sockaddr_cmp( const pj_sockaddr_in *addr1,
			     const pj_sockaddr_in *addr2)
{
    unsigned a1 = pj_ntohl(addr1->sin_addr.s_addr);
    unsigned a2 = pj_ntohl(addr2->sin_addr.s_addr);

    if (a1<a2)
	return -1;
    if (a1>a2)
	return 1;

    a1 = pj_ntohs(addr1->sin_port);
    a2 = pj_ntohs(addr2->sin_port);
    if (a1<a2)
	return -1;
    if (a1>a2)
	return 1;

    return 0;
}


