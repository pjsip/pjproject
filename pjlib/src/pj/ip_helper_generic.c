/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/ip_helper.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>
#include <pj/compat/socket.h>
#include <pj/sock.h>


#if defined(PJ_LINUX) && PJ_LINUX!=0
/* The following headers are used to get DEPRECATED addresses */
#include <arpa/inet.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

/* Set to 1 to enable tracing */
#if 0
#   include <pj/log.h>
#   define THIS_FILE	"ip_helper_generic.c"
#   define TRACE_(exp)	PJ_LOG(5,exp)
    static const char *get_os_errmsg(void)
    {
	static char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(pj_get_os_error(), errmsg, sizeof(errmsg));
	return errmsg;
    }
    static const char *get_addr(void *addr)
    {
	static char txt[PJ_INET6_ADDRSTRLEN];
	struct sockaddr *ad = (struct sockaddr*)addr;
	if (ad->sa_family != PJ_AF_INET && ad->sa_family != PJ_AF_INET6)
	    return "?";
	return pj_inet_ntop2(ad->sa_family, pj_sockaddr_get_addr(ad), 
			     txt, sizeof(txt));
    }
#else
#   define TRACE_(exp)
#endif


#if 0
    /* dummy */

#elif defined(PJ_HAS_IFADDRS_H) && PJ_HAS_IFADDRS_H != 0 && \
      defined(PJ_HAS_NET_IF_H) && PJ_HAS_NET_IF_H != 0
/* Using getifaddrs() is preferred since it can work with both IPv4 and IPv6 */
static pj_status_t if_enum_by_af(int af,
				 unsigned *p_cnt,
				 pj_sockaddr ifs[])
{
    struct ifaddrs *ifap = NULL, *it;
    unsigned max;

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EINVAL);
    
    TRACE_((THIS_FILE, "Starting interface enum with getifaddrs() for af=%d",
	    af));

    if (getifaddrs(&ifap) != 0) {
	TRACE_((THIS_FILE, " getifarrds() failed: %s", get_os_errmsg()));
	return PJ_RETURN_OS_ERROR(pj_get_netos_error());
    }

    it = ifap;
    max = *p_cnt;
    *p_cnt = 0;
    for (; it!=NULL && *p_cnt < max; it = it->ifa_next) {
	struct sockaddr *ad = it->ifa_addr;

	TRACE_((THIS_FILE, " checking %s", it->ifa_name));

	if ((it->ifa_flags & IFF_UP)==0) {
	    TRACE_((THIS_FILE, "  interface is down"));
	    continue; /* Skip when interface is down */
	}

	if ((it->ifa_flags & IFF_RUNNING)==0) {
	    TRACE_((THIS_FILE, "  interface is not running"));
	    continue; /* Skip when interface is not running */
	}

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
	if (it->ifa_flags & IFF_LOOPBACK) {
	    TRACE_((THIS_FILE, "  loopback interface"));
	    continue; /* Skip loopback interface */
	}
#endif

	if (ad==NULL) {
	    TRACE_((THIS_FILE, "  NULL address ignored"));
	    continue; /* reported to happen on Linux 2.6.25.9 
			 with ppp interface */
	}

	if (ad->sa_family != af) {
	    TRACE_((THIS_FILE, "  address %s ignored (af=%d)", 
		    get_addr(ad), ad->sa_family));
	    continue; /* Skip when interface is down */
	}

	/* Ignore 192.0.0.0/29 address.
	 * Ref: https://datatracker.ietf.org/doc/html/rfc7335#section-4
	 */
	if (af==pj_AF_INET() &&
	    (pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 4) ==
	     201326592) /* 0b1100000000000000000000000000 which is
	                   192.0.0.0 >> 4 */
	{
	    TRACE_((THIS_FILE, "  address %s ignored (192.0.0.0/29 class)",
		    get_addr(ad), ad->sa_family));
	    continue;
	}

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if (af==pj_AF_INET() &&
	    (pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
	{
	    TRACE_((THIS_FILE, "  address %s ignored (0.0.0.0/8 class)", 
		    get_addr(ad), ad->sa_family));
	    continue;
	}

	TRACE_((THIS_FILE, "  address %s (af=%d) added at index %d", 
		get_addr(ad), ad->sa_family, *p_cnt));

	pj_bzero(&ifs[*p_cnt], sizeof(ifs[0]));
	pj_memcpy(&ifs[*p_cnt], ad, pj_sockaddr_get_len(ad));
	PJ_SOCKADDR_RESET_LEN(&ifs[*p_cnt]);
	(*p_cnt)++;
    }

    freeifaddrs(ifap);
    TRACE_((THIS_FILE, "done, found %d address(es)", *p_cnt));
    return (*p_cnt != 0) ? PJ_SUCCESS : PJ_ENOTFOUND;
}

#elif defined(SIOCGIFCONF) && \
      defined(PJ_HAS_NET_IF_H) && PJ_HAS_NET_IF_H != 0

/* Note: this does not work with IPv6 */
static pj_status_t if_enum_by_af(int af,
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
    
    TRACE_((THIS_FILE, "Starting interface enum with SIOCGIFCONF for af=%d",
	    af));

    status = pj_sock_socket(af, PJ_SOCK_DGRAM, 0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    /* Query available interfaces */
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;

    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
	int oserr = pj_get_netos_error();
	TRACE_((THIS_FILE, " ioctl(SIOCGIFCONF) failed: %s", get_os_errmsg()));
	pj_sock_close(sock);
	return PJ_RETURN_OS_ERROR(oserr);
    }

    /* Interface interfaces */
    ifr = (struct ifreq*) ifc.ifc_req;
    count = ifc.ifc_len / sizeof(struct ifreq);
    if (count > *p_cnt)
	count = *p_cnt;

    *p_cnt = 0;
    for (i=0; i<count; ++i) {
	struct ifreq *itf = &ifr[i];
        struct ifreq iff = *itf;
	struct sockaddr *ad = &itf->ifr_addr;
	
	TRACE_((THIS_FILE, " checking interface %s", itf->ifr_name));

	/* Skip address with different family */
	if (ad->sa_family != af) {
	    TRACE_((THIS_FILE, "  address %s (af=%d) ignored",
		    get_addr(ad), (int)ad->sa_family));
	    continue;
	}

        if (ioctl(sock, SIOCGIFFLAGS, &iff) != 0) {
	    TRACE_((THIS_FILE, "  ioctl(SIOCGIFFLAGS) failed: %s",
		    get_os_errmsg()));
	    continue;	/* Failed to get flags, continue */
	}

	if ((iff.ifr_flags & IFF_UP)==0) {
	    TRACE_((THIS_FILE, "  interface is down"));
	    continue; /* Skip when interface is down */
	}

	if ((iff.ifr_flags & IFF_RUNNING)==0) {
	    TRACE_((THIS_FILE, "  interface is not running"));
	    continue; /* Skip when interface is not running */
	}

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
	if (iff.ifr_flags & IFF_LOOPBACK) {
	    TRACE_((THIS_FILE, "  loopback interface"));
	    continue; /* Skip loopback interface */
	}
#endif

	/* Ignore 192.0.0.0/29 address.
	 * Ref: https://datatracker.ietf.org/doc/html/rfc7335#section-4
	 */
	if (af==pj_AF_INET() &&
	    (pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 4) ==
	     201326592) /* 0b1100000000000000000000000000 which is
	     		   192.0.0.0 >> 4 */
	{
	    TRACE_((THIS_FILE, "  address %s ignored (192.0.0.0/29 class)",
		    get_addr(ad), ad->sa_family));
	    continue;
	}

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if (af==pj_AF_INET() &&
	    (pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
	{
	    TRACE_((THIS_FILE, "  address %s ignored (0.0.0.0/8 class)", 
		    get_addr(ad), ad->sa_family));
	    continue;
	}

	TRACE_((THIS_FILE, "  address %s (af=%d) added at index %d", 
		get_addr(ad), ad->sa_family, *p_cnt));

	pj_bzero(&ifs[*p_cnt], sizeof(ifs[0]));
	pj_memcpy(&ifs[*p_cnt], ad, pj_sockaddr_get_len(ad));
	PJ_SOCKADDR_RESET_LEN(&ifs[*p_cnt]);
	(*p_cnt)++;
    }

    /* Done with socket */
    pj_sock_close(sock);

    TRACE_((THIS_FILE, "done, found %d address(es)", *p_cnt));
    return (*p_cnt != 0) ? PJ_SUCCESS : PJ_ENOTFOUND;
}


#elif defined(PJ_HAS_NET_IF_H) && PJ_HAS_NET_IF_H != 0
/* Note: this does not work with IPv6 */
static pj_status_t if_enum_by_af(int af, unsigned *p_cnt, pj_sockaddr ifs[])
{
    struct if_nameindex *if_list;
    struct ifreq ifreq;
    pj_sock_t sock;
    unsigned i, max_count;
    pj_status_t status;

    PJ_ASSERT_RETURN(af==PJ_AF_INET || af==PJ_AF_INET6, PJ_EINVAL);

    TRACE_((THIS_FILE, "Starting if_nameindex() for af=%d", af));

    status = pj_sock_socket(af, PJ_SOCK_DGRAM, 0, &sock);
    if (status != PJ_SUCCESS)
	return status;

    if_list = if_nameindex();
    if (if_list == NULL)
	return PJ_ENOTFOUND;

    max_count = *p_cnt;
    *p_cnt = 0;
    for (i=0; if_list[i].if_index && *p_cnt<max_count; ++i) {
	struct sockaddr *ad;
	int rc;

	strncpy(ifreq.ifr_name, if_list[i].if_name, IFNAMSIZ);

	TRACE_((THIS_FILE, " checking interface %s", ifreq.ifr_name));

	if ((rc=ioctl(sock, SIOCGIFFLAGS, &ifreq)) != 0) {
	    TRACE_((THIS_FILE, "  ioctl(SIOCGIFFLAGS) failed: %s",
		    get_os_errmsg()));
	    continue;	/* Failed to get flags, continue */
	}

	if ((ifreq.ifr_flags & IFF_UP)==0) {
	    TRACE_((THIS_FILE, "  interface is down"));
	    continue; /* Skip when interface is down */
	}

        if ((ifreq.ifr_flags & IFF_RUNNING)==0) {
	    TRACE_((THIS_FILE, "  interface is not running"));
	    continue; /* Skip when interface is not running */
	}

#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
	if (ifreq.ifr_flags & IFF_LOOPBACK) {
	    TRACE_((THIS_FILE, "  loopback interface"));
	    continue; /* Skip loopback interface */
	}
#endif

	/* Note: SIOCGIFADDR does not work for IPv6! */
	if ((rc=ioctl(sock, SIOCGIFADDR, &ifreq)) != 0) {
	    TRACE_((THIS_FILE, "  ioctl(SIOCGIFADDR) failed: %s",
		    get_os_errmsg()));
	    continue;	/* Failed to get address, continue */
	}

	ad = (struct sockaddr*) &ifreq.ifr_addr;

	if (ad->sa_family != af) {
	    TRACE_((THIS_FILE, "  address %s family %d ignored", 
			       get_addr(&ifreq.ifr_addr),
			       ifreq.ifr_addr.sa_family));
	    continue;	/* Not address family that we want, continue */
	}

	/* Ignore 192.0.0.0/29 address.
	 * Ref: https://datatracker.ietf.org/doc/html/rfc7335#section-4
	 */
	if (af==pj_AF_INET() &&
	    (pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 4) ==
	     201326592) /* 0b1100000000000000000000000000 which is
	     		   192.0.0.0 >> 4 */
	{
	    TRACE_((THIS_FILE, "  address %s ignored (192.0.0.0/29 class)",
		    get_addr(ad), ad->sa_family));
	    continue;
	}

	/* Ignore 0.0.0.0/8 address. This is a special address
	 * which doesn't seem to have practical use.
	 */
	if (af==pj_AF_INET() &&
	    (pj_ntohl(((pj_sockaddr_in*)ad)->sin_addr.s_addr) >> 24) == 0)
	{
	    TRACE_((THIS_FILE, "  address %s ignored (0.0.0.0/8 class)", 
		    get_addr(ad), ad->sa_family));
	    continue;
	}

	/* Got an address ! */
	TRACE_((THIS_FILE, "  address %s (af=%d) added at index %d", 
		get_addr(ad), ad->sa_family, *p_cnt));

	pj_bzero(&ifs[*p_cnt], sizeof(ifs[0]));
	pj_memcpy(&ifs[*p_cnt], ad, pj_sockaddr_get_len(ad));
	PJ_SOCKADDR_RESET_LEN(&ifs[*p_cnt]);
	(*p_cnt)++;
    }

    if_freenameindex(if_list);
    pj_sock_close(sock);

    TRACE_((THIS_FILE, "done, found %d address(es)", *p_cnt));
    return (*p_cnt != 0) ? PJ_SUCCESS : PJ_ENOTFOUND;
}

#else
static pj_status_t if_enum_by_af(int af,
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
#endif /* SIOCGIFCONF */

/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(int af,
					 unsigned *p_cnt,
					 pj_sockaddr ifs[])
{
    unsigned start;
    pj_status_t status;

    start = 0;
    if (af==PJ_AF_INET6 || af==PJ_AF_UNSPEC) {
	unsigned max = *p_cnt;
	status = if_enum_by_af(PJ_AF_INET6, &max, &ifs[start]);
	if (status == PJ_SUCCESS) {
	    start += max;
	    (*p_cnt) -= max;
	}
    }

    if (af==PJ_AF_INET || af==PJ_AF_UNSPEC) {
	unsigned max = *p_cnt;
	status = if_enum_by_af(PJ_AF_INET, &max, &ifs[start]);
	if (status == PJ_SUCCESS) {
	    start += max;
	    (*p_cnt) -= max;
	}
    }

    *p_cnt = start;

    return (*p_cnt != 0) ? PJ_SUCCESS : PJ_ENOTFOUND;
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

static pj_status_t get_ipv6_deprecated(unsigned *count, pj_sockaddr addr[])
{
#if defined(PJ_LINUX) && PJ_LINUX!=0
    struct {
        struct nlmsghdr        nlmsg_info;
        struct ifaddrmsg    ifaddrmsg_info;
    } netlink_req;

    long pagesize = sysconf(_SC_PAGESIZE);
    if (!pagesize)
        pagesize = 4096; /* Assume pagesize is 4096 if sysconf() failed */

    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    bzero(&netlink_req, sizeof(netlink_req));

    netlink_req.nlmsg_info.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    netlink_req.nlmsg_info.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    netlink_req.nlmsg_info.nlmsg_type = RTM_GETADDR;
    netlink_req.nlmsg_info.nlmsg_pid = getpid();
    netlink_req.ifaddrmsg_info.ifa_family = AF_INET6;

    int rtn = send(fd, &netlink_req, netlink_req.nlmsg_info.nlmsg_len, 0);
    if (rtn < 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

    char read_buffer[pagesize];
    size_t idx = 0;

    while(1) {
	bzero(read_buffer, pagesize);
	int read_size = recv(fd, read_buffer, pagesize, 0);
	if (read_size < 0)
	    return PJ_RETURN_OS_ERROR(pj_get_native_netos_error());

	struct nlmsghdr *nlmsg_ptr = (struct nlmsghdr *) read_buffer;
	int nlmsg_len = read_size;

	if (nlmsg_len < sizeof (struct nlmsghdr))
	    return PJ_ETOOSMALL;

	if (nlmsg_ptr->nlmsg_type == NLMSG_DONE)
	    break;

	for(; NLMSG_OK(nlmsg_ptr, nlmsg_len);
	    nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, nlmsg_len))
	{
	    struct ifaddrmsg *ifaddrmsg_ptr;
	    struct rtattr *rtattr_ptr;
	    int ifaddrmsg_len;

	    ifaddrmsg_ptr = (struct ifaddrmsg*)NLMSG_DATA(nlmsg_ptr);

	    if (ifaddrmsg_ptr->ifa_flags & IFA_F_DEPRECATED ||
		ifaddrmsg_ptr->ifa_flags & IFA_F_TENTATIVE)
	    {
		rtattr_ptr = (struct rtattr*)IFA_RTA(ifaddrmsg_ptr);
		ifaddrmsg_len = IFA_PAYLOAD(nlmsg_ptr);

		for(;RTA_OK(rtattr_ptr, ifaddrmsg_len);
		    rtattr_ptr = RTA_NEXT(rtattr_ptr, ifaddrmsg_len))
		{
		    switch(rtattr_ptr->rta_type) {
		    case IFA_ADDRESS:
			// Check if addr can contains more data
			if (idx >= *count)
			    break;
			// Store deprecated IP
			char deprecatedAddr[PJ_INET6_ADDRSTRLEN];
			inet_ntop(ifaddrmsg_ptr->ifa_family,
				  RTA_DATA(rtattr_ptr),
				  deprecatedAddr,
				  sizeof(deprecatedAddr));
			pj_str_t pj_addr_str;
			pj_cstr(&pj_addr_str, deprecatedAddr);
			pj_sockaddr_init(pj_AF_INET6(), &addr[idx],
					 &pj_addr_str, 0);
			++idx;
		    default:
			break;
		    }
		}
	    }
	}
    }

    close(fd);
    *count = idx;

    return PJ_SUCCESS;
#else
    *count = 0;
    return PJ_ENOTSUP;
#endif
}


/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface2( const pj_enum_ip_option *opt,
					   unsigned *p_cnt,
					   pj_sockaddr ifs[])
{
    pj_enum_ip_option opt_;

    if (opt)
	opt_ = *opt;
    else
	pj_enum_ip_option_default(&opt_);

    if (opt_.af != pj_AF_INET() && opt_.omit_deprecated_ipv6) {
	pj_sockaddr addrs[*p_cnt];
	pj_sockaddr deprecatedAddrs[*p_cnt];
	unsigned deprecatedCount = *p_cnt;
	unsigned cnt = 0;
	int i;
	pj_status_t status;

	status = get_ipv6_deprecated(&deprecatedCount, deprecatedAddrs);
	if (status != PJ_SUCCESS)
	    return status;

	status = pj_enum_ip_interface(opt_.af, p_cnt, addrs);
	if (status != PJ_SUCCESS)
	    return status;

	for (i = 0; i < *p_cnt; ++i) {
	    int j;
	    
	    ifs[cnt++] = addrs[i];

	    if (addrs[i].addr.sa_family != pj_AF_INET6())
		continue;

	    for (j = 0; j < deprecatedCount; ++j) {
		if (pj_sockaddr_cmp(&addrs[i], &deprecatedAddrs[j]) == 0) {
		    cnt--;
		    break;
		}
	    }
	}

	*p_cnt = cnt;
	return *p_cnt ? PJ_SUCCESS : PJ_ENOTFOUND;
    }

    return pj_enum_ip_interface(opt_.af, p_cnt, ifs);
}
