/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#include <pj/config.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* PMIB_ICMP_EX is not declared in VC6, causing error.
 * But EVC4, which also claims to be VC6, does have it! 
 */
#if defined(_MSC_VER) && _MSC_VER==1200 && !defined(PJ_WIN32_WINCE)
#   define PMIB_ICMP_EX void*
#endif
#include <winsock2.h>

/* If you encounter error "Cannot open include file: 'Iphlpapi.h' here,
 * you need to install newer Platform SDK. Presumably you're using
 * Microsoft Visual Studio 6?
 */
#include <Iphlpapi.h>

#include <pj/ip_helper.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>

typedef DWORD (WINAPI *PFN_GetIpAddrTable)(PMIB_IPADDRTABLE pIpAddrTable, 
					   PULONG pdwSize, 
					   BOOL bOrder);
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6!=0
typedef DWORD (WINAPI *PFN_GetAdapterAddresses)(ULONG Family,
					        ULONG Flags,
					        PVOID Reserved,
					        PIP_ADAPTER_ADDRESSES AdapterAddresses,
					        PULONG SizePointer);
#endif	/* PJ_HAS_IPV6 */
typedef DWORD (WINAPI *PFN_GetIpForwardTable)(PMIB_IPFORWARDTABLE pIpForwardTable,
					      PULONG pdwSize, 
					      BOOL bOrder);
typedef DWORD (WINAPI *PFN_GetIfEntry)(PMIB_IFROW pIfRow);

static HANDLE s_hDLL;
static PFN_GetIpAddrTable s_pfnGetIpAddrTable;
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6!=0
    static PFN_GetAdapterAddresses s_pfnGetAdapterAddresses;
#endif	/* PJ_HAS_IPV6 */
static PFN_GetIpForwardTable s_pfnGetIpForwardTable;
static PFN_GetIfEntry s_pfnGetIfEntry;


static void unload_iphlp_module(void)
{
    FreeLibrary(s_hDLL);
    s_hDLL = NULL;
    s_pfnGetIpAddrTable = NULL;
    s_pfnGetIpForwardTable = NULL;
    s_pfnGetIfEntry = NULL;
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6!=0
    s_pfnGetAdapterAddresses = NULL;
#endif
}

static FARPROC GetIpHlpApiProc(char *lpProcName)
{
    if(NULL == s_hDLL) {
	s_hDLL = LoadLibrary(PJ_T("IpHlpApi"));
	if(NULL != s_hDLL) {
	    pj_atexit(&unload_iphlp_module);
	}
    }
	
    if(NULL != s_hDLL)
	return GetProcAddress(s_hDLL, lpProcName);
    
    return NULL;
}

static DWORD MyGetIpAddrTable(PMIB_IPADDRTABLE pIpAddrTable, 
			      PULONG pdwSize, 
			      BOOL bOrder)
{
    if(NULL == s_pfnGetIpAddrTable) {
	s_pfnGetIpAddrTable = (PFN_GetIpAddrTable) 
	    GetIpHlpApiProc("GetIpAddrTable");
    }
    
    if(NULL != s_pfnGetIpAddrTable) {
	return s_pfnGetIpAddrTable(pIpAddrTable, pdwSize, bOrder);
    }
    
    return ERROR_NOT_SUPPORTED;
}

#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6!=0
static DWORD MyGetAdapterAddresses(ULONG Family,
				   ULONG Flags,
				   PVOID Reserved,
				   PIP_ADAPTER_ADDRESSES AdapterAddresses,
				   PULONG SizePointer)
{
    if(NULL == s_pfnGetAdapterAddresses) {
	s_pfnGetAdapterAddresses = (PFN_GetAdapterAddresses) 
	    GetIpHlpApiProc("GetAdapterAddresses");
    }
    
    if(NULL != s_pfnGetAdapterAddresses) {
	return s_pfnGetAdapterAddresses(Family, Flags, Reserved,
					AdapterAddresses, SizePointer);
    }
    
    return ERROR_NOT_SUPPORTED;
}
#endif	/* PJ_HAS_IPV6 */

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
static DWORD MyGetIfEntry(MIB_IFROW *pIfRow)
{
    if(NULL == s_pfnGetIfEntry) {
	s_pfnGetIfEntry = (PFN_GetIfEntry) 
	    GetIpHlpApiProc("GetIfEntry");
    }
    
    if(NULL != s_pfnGetIfEntry) {
	return s_pfnGetIfEntry(pIfRow);
    }
    
    return ERROR_NOT_SUPPORTED;
}
#endif


static DWORD MyGetIpForwardTable(PMIB_IPFORWARDTABLE pIpForwardTable, 
				 PULONG pdwSize, 
				 BOOL bOrder)
{
    if(NULL == s_pfnGetIpForwardTable) {
	s_pfnGetIpForwardTable = (PFN_GetIpForwardTable) 
	    GetIpHlpApiProc("GetIpForwardTable");
    }
    
    if(NULL != s_pfnGetIpForwardTable) {
	return s_pfnGetIpForwardTable(pIpForwardTable, pdwSize, bOrder);
    }
    
    return ERROR_NOT_SUPPORTED;
}

/* Enumerate local IP interface using GetIpAddrTable()
 * for IPv4 addresses only.
 */
static pj_status_t enum_ipv4_interface(unsigned *p_cnt,
				       pj_sockaddr ifs[])
{
    /* Provide enough buffer or otherwise it will fail with 
     * error 22 ("Not Enough Buffer") error.
     */
    char ipTabBuff[1024];
    MIB_IPADDRTABLE *pTab;
    ULONG tabSize;
    unsigned i, count;
    DWORD rc = NO_ERROR;

    PJ_ASSERT_RETURN(p_cnt && ifs, PJ_EINVAL);

    pTab = (MIB_IPADDRTABLE*)ipTabBuff;

    /* Get IP address table */
    tabSize = sizeof(ipTabBuff);

    rc = MyGetIpAddrTable(pTab, &tabSize, FALSE);
    if (rc != NO_ERROR)
	return PJ_RETURN_OS_ERROR(rc);

    /* Reset result */
    pj_bzero(ifs, sizeof(ifs[0]) * (*p_cnt));

    /* Now fill out the entries */
    count = (pTab->dwNumEntries < *p_cnt) ? pTab->dwNumEntries : *p_cnt;
    *p_cnt = 0;
    for (i=0; i<count; ++i) {
	MIB_IFROW ifRow;

	/* Ignore 0.0.0.0 address (interface is down?) */
	if (pTab->table[i].dwAddr == 0)
	    continue;

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if ((pj_ntohl(pTab->table[i].dwAddr) >> 24) == 0)
	    continue;

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
	/* Investigate the type of this interface */
	pj_bzero(&ifRow, sizeof(ifRow));
	ifRow.dwIndex = pTab->table[i].dwIndex;
	if (MyGetIfEntry(&ifRow) != 0)
	    continue;

	if (ifRow.dwType == MIB_IF_TYPE_LOOPBACK)
	    continue;
#endif

	ifs[*p_cnt].ipv4.sin_family = PJ_AF_INET;
	ifs[*p_cnt].ipv4.sin_addr.s_addr = pTab->table[i].dwAddr;
	(*p_cnt)++;
    }

    return (*p_cnt) ? PJ_SUCCESS : PJ_ENOTFOUND;
}


/* Enumerate local IP interface using GetAdapterAddresses(),
 * which works for both IPv4 and IPv6.
 */
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6!=0
static pj_status_t enum_ipv4_ipv6_interface(int af,
					    unsigned *p_cnt,
					    pj_sockaddr ifs[])
{
    pj_uint8_t buffer[1024];
    IP_ADAPTER_ADDRESSES *adapter = (IP_ADAPTER_ADDRESSES*)buffer;
    ULONG size = sizeof(buffer);
    unsigned i;
    DWORD rc;

    rc = MyGetAdapterAddresses(af, 0, NULL, adapter, &size);
    if (rc != ERROR_SUCCESS)
	return PJ_RETURN_OS_ERROR(rc);

    for (i=0; i<*p_cnt && adapter; ++i, adapter = adapter->Next) {
	SOCKET_ADDRESS *pAddr = &adapter->FirstUnicastAddress->Address;
	ifs[i].addr.sa_family = pAddr->lpSockaddr->sa_family;
	pj_memcpy(&ifs[i], pAddr->lpSockaddr, pAddr->iSockaddrLength);
    }

    return PJ_SUCCESS;
}
#endif


/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(int af,
					 unsigned *p_cnt,
					 pj_sockaddr ifs[])
{
    pj_status_t status = -1;

    PJ_ASSERT_RETURN(p_cnt && ifs, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==PJ_AF_UNSPEC || af==PJ_AF_INET || af==PJ_AF_INET6,
		     PJ_EAFNOTSUP);

#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6!=0
    status = enum_ipv4_ipv6_interface(af, p_cnt, ifs);
    if (status != PJ_SUCCESS && (af==PJ_AF_INET || af==PJ_AF_UNSPEC))
	status = enum_ipv4_interface(p_cnt, ifs);
    return status;
#else
    if (af==PJ_AF_INET6)
	return PJ_EIPV6NOTSUP;
    else if (af != PJ_AF_INET && af != PJ_AF_UNSPEC)
	return PJ_EAFNOTSUP;

    status = enum_ipv4_interface(p_cnt, ifs);
    return status;
#endif
}

/*
 * Enumerate the IP routing table for this host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_route(unsigned *p_cnt,
				     pj_ip_route_entry routes[])
{
    char ipTabBuff[1024];
    MIB_IPADDRTABLE *pIpTab;
    char rtabBuff[1024];
    MIB_IPFORWARDTABLE *prTab;
    ULONG tabSize;
    unsigned i, count;
    DWORD rc = NO_ERROR;

    PJ_ASSERT_RETURN(p_cnt && routes, PJ_EINVAL);

    pIpTab = (MIB_IPADDRTABLE *)ipTabBuff;
    prTab = (MIB_IPFORWARDTABLE *)rtabBuff;

    /* First get IP address table */
    tabSize = sizeof(ipTabBuff);
    rc = MyGetIpAddrTable(pIpTab, &tabSize, FALSE);
    if (rc != NO_ERROR)
	return PJ_RETURN_OS_ERROR(rc);

    /* Next get IP route table */
    tabSize = sizeof(rtabBuff);

    rc = MyGetIpForwardTable(prTab, &tabSize, 1);
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

