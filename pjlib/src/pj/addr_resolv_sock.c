/* $Id$
 */
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
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/string.h>
#include <pj/compat/socket.h>
#include <pj/errno.h>


PJ_DEF(pj_status_t) pj_gethostbyname(const pj_str_t *hostname, pj_hostent *phe)
{
    struct hostent *he;
    char copy[PJ_MAX_HOSTNAME];

    pj_assert(hostname && hostname ->slen < PJ_MAX_HOSTNAME);
    
    if (hostname->slen >= PJ_MAX_HOSTNAME)
	return PJ_ENAMETOOLONG;

    pj_memcpy(copy, hostname->ptr, hostname->slen);
    copy[ hostname->slen ] = '\0';

    he = gethostbyname(copy);
    if (!he)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    phe->h_name = he->h_name;
    phe->h_aliases = he->h_aliases;
    phe->h_addrtype = he->h_addrtype;
    phe->h_length = he->h_length;
    phe->h_addr_list = he->h_addr_list;

    return PJ_SUCCESS;
}

