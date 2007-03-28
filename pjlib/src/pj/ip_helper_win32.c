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
#include <pj/config.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* PMIB_ICMP_EX is not declared in VC6, causing error.
 * But EVC4, which also claims to be VC6, does have it! 
 */
#if defined(_MSC_VER) && _MSC_VER==1200 && !defined(PJ_WIN32_WINCE)
#   define PMIB_ICMP_EX void*
#endif
#include <Iphlpapi.h>

#include <pj/ip_helper.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>


/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(unsigned *p_cnt,
					 pj_in_addr ifs[])
{
    /* Provide enough buffer or otherwise it will fail with 
     * error 22 ("Not Enough Buffer") error.
     */
    MIB_IPADDRTABLE ipTabBuff[4];
    MIB_IPADDRTABLE *pTab;
    ULONG tabSize;
    unsigned i, count;
    DWORD rc;

    PJ_ASSERT_RETURN(p_cnt && ifs, PJ_EINVAL);

    pTab = ipTabBuff;

    /* Get IP address table */
    tabSize = sizeof(ipTabBuff);
    rc = GetIpAddrTable(ipTabBuff, &tabSize, FALSE);
    if (rc != NO_ERROR)
	return PJ_RETURN_OS_ERROR(rc);

    /* Reset result */
    pj_bzero(ifs, sizeof(ifs[0]) * (*p_cnt));

    /* Now fill out the entries */
    count = (pTab->dwNumEntries < *p_cnt) ? pTab->dwNumEntries : *p_cnt;
    *p_cnt = 0;
    for (i=0; i<count; ++i) {
	/* Some Windows returns 0.0.0.0! */
	if (pTab->table[i].dwAddr == 0)
	    continue;
	ifs[*p_cnt].s_addr = pTab->table[i].dwAddr;
	(*p_cnt)++;
    }

    return PJ_SUCCESS;

}


/*
 * Enumerate the IP routing table for this host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_route(unsigned *p_cnt,
				     pj_ip_route_entry routes[])
{
    MIB_IPADDRTABLE ipTabBuff[4];
    MIB_IPADDRTABLE *pIpTab;
    MIB_IPFORWARDTABLE rtabBuff[4];
    MIB_IPFORWARDTABLE *prTab;
    ULONG tabSize;
    unsigned i, count;
    DWORD rc;

    PJ_ASSERT_RETURN(p_cnt && routes, PJ_EINVAL);

    pIpTab = ipTabBuff;
    prTab = rtabBuff;

    /* First get IP address table */
    tabSize = sizeof(ipTabBuff);
    rc = GetIpAddrTable(ipTabBuff, &tabSize, FALSE);
    if (rc != NO_ERROR)
	return PJ_RETURN_OS_ERROR(rc);

    /* Next get IP route table */
    tabSize = sizeof(rtabBuff);
    rc = GetIpForwardTable(rtabBuff, &tabSize, 1);
    if (rc != NO_ERROR)
	return PJ_RETURN_OS_ERROR(rc);

    /* Reset routes */
    pj_bzero(routes, sizeof(routes[0]) * (*p_cnt));

    /* Now fill out the route entries */
    count = (prTab->dwNumEntries < *p_cnt) ? prTab->dwNumEntries : *p_cnt;
    *p_cnt = 0;
    for (i=0; i<count; ++i) {
	unsigned j;

	/* Find interface entry */
	for (j=0; j<pIpTab->dwNumEntries; ++j) {
	    if (pIpTab->table[j].dwIndex == prTab->table[i].dwForwardIfIndex)
		break;
	}

	if (j==pIpTab->dwNumEntries)
	    continue;	/* Interface not found */

	routes[*p_cnt].ipv4.if_addr.s_addr = pIpTab->table[j].dwAddr;
	routes[*p_cnt].ipv4.dst_addr.s_addr = prTab->table[i].dwForwardDest;
	routes[*p_cnt].ipv4.mask.s_addr = prTab->table[i].dwForwardMask;

	(*p_cnt)++;
    }

    return PJ_SUCCESS;
}



