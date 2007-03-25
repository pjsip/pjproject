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

/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(unsigned *p_cnt,
					 pj_in_addr ifs[])
{
    pj_status_t status;

    PJ_ASSERT_RETURN(p_cnt && *p_cnt > 0 && ifs, PJ_EINVAL);

    pj_bzero(ifs, sizeof(ifs[0]) * (*p_cnt));

    /* Just get one default route */
    status = pj_gethostip(&ifs[0]);
    if (status != PJ_SUCCESS)
	return status;

    *p_cnt = 1;
    return PJ_SUCCESS;
}

/*
 * Enumerate the IP routing table for this host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_route(unsigned *p_cnt,
				     pj_ip_route_entry routes[])
{
    pj_status_t status;

    PJ_ASSERT_RETURN(p_cnt && *p_cnt > 0 && routes, PJ_EINVAL);

    pj_bzero(routes, sizeof(routes[0]) * (*p_cnt));

    /* Just get one default route */
    status = pj_gethostip(&routes[0].ipv4.if_addr);
    if (status != PJ_SUCCESS)
	return status;

    routes[0].ipv4.dst_addr.s_addr = 0;
    routes[0].ipv4.mask.s_addr = 0;
    *p_cnt = 1;

    return PJ_SUCCESS;
}

