/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/config.h>

#include <pj/ip_helper.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>

/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(int af,
					 unsigned *p_cnt,
					 pj_sockaddr ifs[])
{
    PJ_UNUSED_ARG(af);
    PJ_UNUSED_ARG(ifs);

    PJ_ASSERT_RETURN(p_cnt, PJ_EINVAL);

    *p_cnt = 0;

    return PJ_ENOTSUP;
}

/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface2( const pj_enum_ip_option *opt,
					   unsigned *p_cnt,
					   pj_sockaddr ifs[])
{
    PJ_UNUSED_ARG(opt);
    PJ_UNUSED_ARG(ifs);

    PJ_ASSERT_RETURN(p_cnt, PJ_EINVAL);

    *p_cnt = 0;

    return PJ_ENOTSUP;
}




/*
 * Enumerate the IP routing table for this host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_route(unsigned *p_cnt,
				     pj_ip_route_entry routes[])
{
    PJ_UNUSED_ARG(routes);
    PJ_ASSERT_RETURN(p_cnt, PJ_EINVAL);

    *p_cnt = 0;

    return PJ_ENOTSUP;
}