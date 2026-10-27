/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>

/* Zephyr's socket layer has neither getifaddrs() nor the interface ioctls,
 * and a connected UDP socket reports no local address, so the default route
 * trick of ip_helper_generic.c does not work either. Ask the network stack.
 */
#include <zephyr/net/net_if.h>

struct if_enum
{
    int          af;
    unsigned     max;
    unsigned     cnt;
    pj_sockaddr *ifs;
};

static void addr_cb(struct net_if *iface, struct net_if_addr *ifaddr,
                    void *user_data)
{
    struct if_enum *e = (struct if_enum*)user_data;
    pj_sockaddr *out = &e->ifs[e->cnt];

    PJ_UNUSED_ARG(iface);

    if (e->cnt >= e->max || ifaddr->addr_state != NET_ADDR_PREFERRED)
        return;

    pj_bzero(out, sizeof(*out));
    out->addr.sa_family = (pj_uint16_t)e->af;
    if (e->af == PJ_AF_INET) {
        pj_memcpy(&out->ipv4.sin_addr, &ifaddr->address.in_addr,
                  sizeof(out->ipv4.sin_addr));
#if PJ_IP_HELPER_IGNORE_LOOPBACK_IF
        if ((pj_ntohl(out->ipv4.sin_addr.s_addr) >> 24) == 127)
            return;
#endif
    } else {
        pj_memcpy(&out->ipv6.sin6_addr, &ifaddr->address.in6_addr,
                  sizeof(out->ipv6.sin6_addr));
    }
    ++e->cnt;
}

static void if_cb(struct net_if *iface, void *user_data)
{
    struct if_enum *e = (struct if_enum*)user_data;

    if (!net_if_is_up(iface))
        return;
    if (e->af == PJ_AF_INET)
        net_if_ipv4_addr_foreach(iface, &addr_cb, e);
#if defined(PJ_HAS_IPV6) && PJ_HAS_IPV6 != 0
    else
        net_if_ipv6_addr_foreach(iface, &addr_cb, e);
#endif
}

static pj_status_t if_enum_by_af(int af, unsigned *p_cnt, pj_sockaddr ifs[])
{
    struct if_enum e;

    e.af = af;
    e.max = *p_cnt;
    e.cnt = 0;
    e.ifs = ifs;
    net_if_foreach(&if_cb, &e);

    *p_cnt = e.cnt;
    return e.cnt ? PJ_SUCCESS : PJ_ENOTFOUND;
}

/*
 * Enumerate the local IP interface currently active in the host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_interface(int af,
                                         unsigned *p_cnt,
                                         pj_sockaddr ifs[])
{
    unsigned start;
    pj_status_t status;

    PJ_ASSERT_RETURN(p_cnt && *p_cnt > 0 && ifs, PJ_EINVAL);
    pj_bzero(ifs, sizeof(ifs[0]) * (*p_cnt));

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

PJ_DEF(pj_status_t) pj_enum_ip_interface2( const pj_enum_ip_option *opt,
                                           unsigned *p_cnt,
                                           pj_sockaddr ifs[])
{
    pj_enum_ip_option opt_;

    if (opt)
        opt_ = *opt;
    else
        pj_enum_ip_option_default(&opt_);

    if (opt_.af != pj_AF_INET() && opt_.omit_deprecated_ipv6)
        return PJ_ENOTSUP;

    return pj_enum_ip_interface(opt_.af, p_cnt, ifs);
}

/*
 * Enumerate the IP routing table for this host.
 */
PJ_DEF(pj_status_t) pj_enum_ip_route(unsigned *p_cnt,
                                     pj_ip_route_entry routes[])
{
    pj_sockaddr itf;
    unsigned cnt = 1;
    pj_status_t status;

    PJ_ASSERT_RETURN(p_cnt && *p_cnt > 0 && routes, PJ_EINVAL);

    pj_bzero(routes, sizeof(routes[0]) * (*p_cnt));

    /* Just one default route, through the first interface */
    status = if_enum_by_af(PJ_AF_INET, &cnt, &itf);
    if (status != PJ_SUCCESS)
        return status;

    routes[0].ipv4.if_addr.s_addr = itf.ipv4.sin_addr.s_addr;
    routes[0].ipv4.dst_addr.s_addr = 0;
    routes[0].ipv4.mask.s_addr = 0;
    *p_cnt = 1;

    return PJ_SUCCESS;
}
