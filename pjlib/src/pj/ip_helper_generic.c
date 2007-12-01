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
#include <pj/ip_helper.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>
#include <pj/compat/socket.h>

static pj_status_t dummy_enum_ip_interface(int af,
					   unsigned *p_cnt,
					   pj_sockaddr ifs[])
{
    pj_status_t status;

    PJ_ASSERT_RETURN(p_cnt && *p_cnt > 0 && ifs, PJ_EINVAL);

    pj_bzero(ifs, sizeof(ifs[0]) * (*p_cnt));

    /* Just get one default route */
    status = pj_getdefaultipinterface(af, &ifs[0]);
    if (status != PJ_SUCCESS)
	return status;

    *p_cnt = 1;
    return PJ_SUCCESS;
}

#ifdef SIOCGIFCONF
static pj_status_t sock_enum_ip_interface(int af,
					  unsigned *p_cnt,
					  pj_sockaddr ifs[])
{
    pj_sock_t sock;
    char buf[512];
    struct ifconf ifc;
    struct ifreq *ifr;
    int i, count;
    pj_status_t status;

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EINVAL);
    
    status = pj_sock_socket(af, PJ_SOCK_DGRAM, 0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    /* Query available interfaces */
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;

    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
	int oserr = pj_get_netos_error();
	pj_sock_close(sock);
	return PJ_RETURN_OS_ERROR(oserr);
    }

    /* Done with socket */
    pj_sock_close(sock);

    /* Interface interfaces */
    ifr = (struct ifreq*) ifc.ifc_req;
    count = ifc.ifc_len / sizeof(struct ifreq);
    if (count > *p_cnt)
	count = *p_cnt;
    else
	*p_cnt = count;
    for (i=0; i<count; ++i) {
	struct ifreq *itf = &ifr[i];
	struct sockaddr *ad = itf->ifr_addr;
	
	ifs[i].addr.sa_family = ad->sa_family;
	pj_memcpy(pj_sockaddr_get_addr(&ifs[i]),
		  pj_sockaddr_get_addr(ad),
		  pj_sockaddr_get_addr_len(ad));
    }

    return PJ_SUCCESS;
}
#endif /* SIOCGIFCONF */

/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(int af,
					 unsigned *p_cnt,
					 pj_sockaddr ifs[])
{
#ifdef SIOCGIFCONF
    if (sock_enum_ip_interface(af, p_cnt, ifs) == PJ_SUCCESS)
	return PJ_SUCCESS;
#endif
    return dummy_enum_ip_interface(af, p_cnt, ifs);
}

/*
 * Enumerate the IP routing table for this host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_route(unsigned *p_cnt,
				     pj_ip_route_entry routes[])
{
    pj_sockaddr itf;
    pj_status_t status;

    PJ_ASSERT_RETURN(p_cnt && *p_cnt > 0 && routes, PJ_EINVAL);

    pj_bzero(routes, sizeof(routes[0]) * (*p_cnt));

    /* Just get one default route */
    status = pj_getdefaultipinterface(PJ_AF_INET, &itf);
    if (status != PJ_SUCCESS)
	return status;
    
    routes[0].ipv4.if_addr.s_addr = itf.ipv4.sin_addr.s_addr;
    routes[0].ipv4.dst_addr.s_addr = 0;
    routes[0].ipv4.mask.s_addr = 0;
    *p_cnt = 1;

    return PJ_SUCCESS;
}

