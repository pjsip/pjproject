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
#include <pj/sock_qos.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/string.h>
#include <pj/compat/socket.h>

/* This is the implementation of QoS with BSD socket's setsockopt(),
 * using Darwin-specific SO_NET_SERVICE_TYPE if available, and IP_TOS/
 * IPV6_TCLASS as fallback.
 */
#if defined(PJ_QOS_IMPLEMENTATION) && PJ_QOS_IMPLEMENTATION==PJ_QOS_DARWIN

#include <sys/socket.h>

#ifdef SO_NET_SERVICE_TYPE
static pj_status_t sock_set_net_service_type(pj_sock_t sock, int val)
{
    pj_status_t status;

    status = pj_sock_setsockopt(sock, pj_SOL_SOCKET(), SO_NET_SERVICE_TYPE,
                                &val, sizeof(val));
    if (status == PJ_STATUS_FROM_OS(OSERR_ENOPROTOOPT))
        status = PJ_ENOTSUP;

    return status;
}
#endif

static pj_status_t sock_set_net_service_type_type(pj_sock_t sock,
                                                  pj_qos_type type)
{
#ifdef SO_NET_SERVICE_TYPE
    int val = NET_SERVICE_TYPE_BE;

    switch (type) {
        case PJ_QOS_TYPE_BEST_EFFORT:
            val = NET_SERVICE_TYPE_BE;
            break;
        case PJ_QOS_TYPE_BACKGROUND:
            val = NET_SERVICE_TYPE_BK;
            break;
        case PJ_QOS_TYPE_SIGNALLING:
            val = NET_SERVICE_TYPE_SIG;
            break;          
        case PJ_QOS_TYPE_VIDEO:
            val = NET_SERVICE_TYPE_VI;
            break;
        case PJ_QOS_TYPE_VOICE:
        case PJ_QOS_TYPE_CONTROL:
        default:
            val = NET_SERVICE_TYPE_VO;
            break;
    }

    return sock_set_net_service_type(sock, val);
#else
    return PJ_ENOTSUP;
#endif
}

static pj_status_t sock_set_net_service_type_params(pj_sock_t sock,
                                                    pj_qos_params *param)
{
#ifdef SO_NET_SERVICE_TYPE
    pj_status_t status;
    int val = -1;

    PJ_ASSERT_RETURN(param, PJ_EINVAL);

    /*
     * Sources:
     *  - IETF draft-szigeti-tsvwg-ieee-802-11e-01
     *  - iOS 10 SDK, sys/socket.h
     */
    if (val == -1 && param->flags & PJ_QOS_PARAM_HAS_DSCP) {
        if (param->dscp_val == 0) /* DF */
            val = NET_SERVICE_TYPE_BE;
        else if (param->dscp_val < 0x10) /* CS1, AF11, AF12, AF13 */
            val = NET_SERVICE_TYPE_BK;
        else if (param->dscp_val == 0x10) /* CS2 */
            val = NET_SERVICE_TYPE_OAM;
        else if (param->dscp_val < 0x18) /* AF21, AF22, AF23 */
            val = NET_SERVICE_TYPE_RD;
        else if (param->dscp_val < 0x20) /* CS3, AF31, AF32, AF33 */
            val = NET_SERVICE_TYPE_AV;
        else if (param->dscp_val == 0x20) /* CS4 */
            val = NET_SERVICE_TYPE_RD;
        else if (param->dscp_val < 0x28) /* AF41, AF42, AF43 */
            val = NET_SERVICE_TYPE_VI;
        else if (param->dscp_val == 0x28) /* CS5 */
            val = NET_SERVICE_TYPE_SIG;
        else
            val = NET_SERVICE_TYPE_VO; /* VOICE-ADMIT, EF, CS6, etc. */
    }

    if (val == -1 && param->flags & PJ_QOS_PARAM_HAS_WMM) {
        switch (param->wmm_prio) {
            case PJ_QOS_WMM_PRIO_BULK_EFFORT:
                val = NET_SERVICE_TYPE_BE;
                break;
            case PJ_QOS_WMM_PRIO_BULK:
                val = NET_SERVICE_TYPE_BK;
                break;
            case PJ_QOS_WMM_PRIO_VIDEO:
                val = NET_SERVICE_TYPE_VI;
                break;
            case PJ_QOS_WMM_PRIO_VOICE:
                val = NET_SERVICE_TYPE_VO;
                break;
        }
    }

    if (val == -1) {
        pj_qos_type type;

        status = pj_qos_get_type(param, &type);

        if (status == PJ_SUCCESS)
            return sock_set_net_service_type_type(sock, type);

        val = NET_SERVICE_TYPE_BE;
    }

    return sock_set_net_service_type(sock, val);
#else
    return PJ_ENOTSUP;
#endif
}

static pj_status_t sock_set_ip_ds(pj_sock_t sock, pj_qos_params *param)
{
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(param, PJ_EINVAL);

    if (param->flags & PJ_QOS_PARAM_HAS_DSCP) {
        /* We need to know if the socket is IPv4 or IPv6 */
        pj_sockaddr sa;
        int salen = sizeof(salen);

        /* Value is dscp_val << 2 */
        int val = (param->dscp_val << 2);

        status = pj_sock_getsockname(sock, &sa, &salen);
        if (status != PJ_SUCCESS)
            return status;

        if (sa.addr.sa_family == pj_AF_INET()) {
            /* In IPv4, the DS field goes in the TOS field */
            status = pj_sock_setsockopt(sock, pj_SOL_IP(), pj_IP_TOS(),
                                        &val, sizeof(val));
        } else if (sa.addr.sa_family == pj_AF_INET6()) {
            /* In IPv6, the DS field goes in the Traffic Class field */
            status = pj_sock_setsockopt(sock, pj_SOL_IPV6(),
                                        pj_IPV6_TCLASS(),
                                        &val, sizeof(val));
        } else
            status = PJ_EINVAL;

        if (status != PJ_SUCCESS) {
            param->flags &= ~(PJ_QOS_PARAM_HAS_DSCP);
        }
    }

    return status;
}

PJ_DEF(pj_status_t) pj_sock_set_qos_params(pj_sock_t sock,
                                           pj_qos_params *param)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(param, PJ_EINVAL);

    /* No op? */
    if (!param->flags)
        return PJ_SUCCESS;

    /* Clear prio field since we don't support it */
    param->flags &= ~(PJ_QOS_PARAM_HAS_SO_PRIO);

    /* Try SO_NET_SERVICE_TYPE */
    status = sock_set_net_service_type_params(sock, param);
    if (status == PJ_SUCCESS)
        return status;

    if (status != PJ_ENOTSUP) {
        /* SO_NET_SERVICE_TYPE sets both DSCP and WMM */
        param->flags &= ~(PJ_QOS_PARAM_HAS_DSCP);
        param->flags &= ~(PJ_QOS_PARAM_HAS_WMM);
        return status;
    }

    /* Fall back to IP_TOS/IPV6_TCLASS */
    return sock_set_ip_ds(sock, param);
}

PJ_DEF(pj_status_t) pj_sock_set_qos_type(pj_sock_t sock,
                                         pj_qos_type type)
{
    pj_status_t status;
    pj_qos_params param;

    /* Try SO_NET_SERVICE_TYPE */
    status = sock_set_net_service_type_type(sock, type);
    if (status == PJ_SUCCESS || status != PJ_ENOTSUP)
        return status;

    /* Fall back to IP_TOS/IPV6_TCLASS */
    status = pj_qos_get_params(type, &param);
    if (status != PJ_SUCCESS)
        return status;

    return sock_set_ip_ds(sock, &param);
}

#ifdef SO_NET_SERVICE_TYPE
static pj_status_t sock_get_net_service_type(pj_sock_t sock, int *p_val)
{
    pj_status_t status;
    int optlen = sizeof(*p_val);

    PJ_ASSERT_RETURN(p_val, PJ_EINVAL);

    status = pj_sock_getsockopt(sock, pj_SOL_SOCKET(), SO_NET_SERVICE_TYPE,
                                p_val, &optlen);
    if (status == PJ_STATUS_FROM_OS(OSERR_ENOPROTOOPT))
        status = PJ_ENOTSUP;

    return status;
}
#endif

static pj_status_t sock_get_net_service_type_type(pj_sock_t sock,
                                                  pj_qos_type *p_type)
{
#ifdef SO_NET_SERVICE_TYPE
    pj_status_t status;
    int val;

    PJ_ASSERT_RETURN(p_type, PJ_EINVAL);

    status = sock_get_net_service_type(sock, &val);
    if (status == PJ_SUCCESS) {
        switch (val) {
            default:
            case NET_SERVICE_TYPE_BE:
                *p_type = PJ_QOS_TYPE_BEST_EFFORT;
                break;
            case NET_SERVICE_TYPE_BK:
                *p_type = PJ_QOS_TYPE_BACKGROUND;
                break;
            case NET_SERVICE_TYPE_SIG:
                *p_type = PJ_QOS_TYPE_SIGNALLING;
                break;      
            case NET_SERVICE_TYPE_VI:
            case NET_SERVICE_TYPE_RV:
            case NET_SERVICE_TYPE_AV:
            case NET_SERVICE_TYPE_OAM:
            case NET_SERVICE_TYPE_RD:
                *p_type = PJ_QOS_TYPE_VIDEO;
                break;
            case NET_SERVICE_TYPE_VO:
                *p_type = PJ_QOS_TYPE_VOICE;
                break;
        }
    }

    return status;
#else
    return PJ_ENOTSUP;
#endif
}

static pj_status_t sock_get_net_service_type_params(pj_sock_t sock,
                                                    pj_qos_params *p_param)
{
#ifdef SO_NET_SERVICE_TYPE
    pj_status_t status;
    int val;

    PJ_ASSERT_RETURN(p_param, PJ_EINVAL);

    status = sock_get_net_service_type(sock, &val);
    if (status == PJ_SUCCESS) {
        pj_bzero(p_param, sizeof(*p_param));

        /* Note: these are just educated guesses, chosen for symmetry with
         * sock_set_net_service_type_params: we can't know the actual values
         * chosen by the OS, or even if DSCP/WMM are used at all.
         *
         * The source for mapping DSCP to WMM is:
         *  - IETF draft-szigeti-tsvwg-ieee-802-11e-01
         */
        switch (val) {
            default:
            case NET_SERVICE_TYPE_BE:
                p_param->flags = PJ_QOS_PARAM_HAS_DSCP | PJ_QOS_PARAM_HAS_WMM;
                p_param->dscp_val = 0; /* DF */
                p_param->wmm_prio = PJ_QOS_WMM_PRIO_BULK_EFFORT; /* AC_BE */
                break;
            case NET_SERVICE_TYPE_BK:
                p_param->flags = PJ_QOS_PARAM_HAS_DSCP | PJ_QOS_PARAM_HAS_WMM;
                p_param->dscp_val = 0x08; /* CS1 */
                p_param->wmm_prio = PJ_QOS_WMM_PRIO_BULK; /* AC_BK */
                break;
            case NET_SERVICE_TYPE_SIG:
                p_param->flags = PJ_QOS_PARAM_HAS_DSCP | PJ_QOS_PARAM_HAS_WMM;
                p_param->dscp_val = 0x28; /* CS5 */
                p_param->wmm_prio = PJ_QOS_WMM_PRIO_VIDEO; /* AC_VI */
                break;
            case NET_SERVICE_TYPE_VI:
                p_param->flags = PJ_QOS_PARAM_HAS_DSCP | PJ_QOS_PARAM_HAS_WMM;
                p_param->dscp_val = 0x22; /* AF41 */
                p_param->wmm_prio = PJ_QOS_WMM_PRIO_VIDEO; /* AC_VI */
                break;
            case NET_SERVICE_TYPE_VO:
                p_param->flags = PJ_QOS_PARAM_HAS_DSCP | PJ_QOS_PARAM_HAS_WMM;
                p_param->dscp_val = 0x30; /* CS6 */
                p_param->wmm_prio = PJ_QOS_WMM_PRIO_VOICE; /* AC_VO */
                break;
            case NET_SERVICE_TYPE_RV:
                p_param->flags = PJ_QOS_PARAM_HAS_DSCP | PJ_QOS_PARAM_HAS_WMM;
                p_param->dscp_val = 0x22; /* AF41 */
                p_param->wmm_prio = PJ_QOS_WMM_PRIO_VIDEO; /* AC_VI */
                break;
            case NET_SERVICE_TYPE_AV:
                p_param->flags = PJ_QOS_PARAM_HAS_DSCP | PJ_QOS_PARAM_HAS_WMM;
                p_param->dscp_val = 0x18; /* CS3 */
                p_param->wmm_prio = PJ_QOS_WMM_PRIO_VIDEO; /* AC_VI */
                break;
            case NET_SERVICE_TYPE_OAM:
                p_param->flags = PJ_QOS_PARAM_HAS_DSCP | PJ_QOS_PARAM_HAS_WMM;
                p_param->dscp_val = 0x10; /* CS2 */
                p_param->wmm_prio = PJ_QOS_WMM_PRIO_BULK_EFFORT; /* AC_BE */
                break;
            case NET_SERVICE_TYPE_RD:
                p_param->flags = PJ_QOS_PARAM_HAS_DSCP | PJ_QOS_PARAM_HAS_WMM;
                p_param->dscp_val = 0x20; /* CS4 */
                p_param->wmm_prio = PJ_QOS_WMM_PRIO_VIDEO; /* AC_VI */
                break;
        }
    }

    return status;
#else
    return PJ_ENOTSUP;
#endif
}

PJ_DEF(pj_status_t) pj_sock_get_qos_params(pj_sock_t sock,
                                           pj_qos_params *p_param)
{
    pj_status_t status;
    int val, optlen;
    pj_sockaddr sa;
    int salen = sizeof(salen);

    PJ_ASSERT_RETURN(p_param, PJ_EINVAL);

    pj_bzero(p_param, sizeof(*p_param));

    /* Try SO_NET_SERVICE_TYPE */
    status = sock_get_net_service_type_params(sock, p_param);
    if (status != PJ_ENOTSUP)
        return status;

    /* Fall back to IP_TOS/IPV6_TCLASS */
    status = pj_sock_getsockname(sock, &sa, &salen);
    if (status != PJ_SUCCESS)
        return status;

    optlen = sizeof(val);
    if (sa.addr.sa_family == pj_AF_INET()) {
        status = pj_sock_getsockopt(sock, pj_SOL_IP(), pj_IP_TOS(),
                                    &val, &optlen);
    } else if (sa.addr.sa_family == pj_AF_INET6()) {
        status = pj_sock_getsockopt(sock, pj_SOL_IPV6(), pj_IPV6_TCLASS(),
                                    &val, &optlen);
    } else
        status = PJ_EINVAL;
    if (status == PJ_SUCCESS) {
        p_param->flags |= PJ_QOS_PARAM_HAS_DSCP;
        p_param->dscp_val = (pj_uint8_t)(val >> 2);
    }

    return status;
}

PJ_DEF(pj_status_t) pj_sock_get_qos_type(pj_sock_t sock,
                                         pj_qos_type *p_type)
{
    pj_qos_params param;
    pj_status_t status;

    PJ_ASSERT_RETURN(p_type, PJ_EINVAL);

    status = sock_get_net_service_type_type(sock, p_type);
    if (status != PJ_ENOTSUP)
        return status;

    status = pj_sock_get_qos_params(sock, &param);
    if (status != PJ_SUCCESS)
        return status;

    return pj_qos_get_type(&param, p_type);
}

#endif  /* PJ_QOS_IMPLEMENTATION */
