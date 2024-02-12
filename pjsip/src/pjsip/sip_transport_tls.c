/* 
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
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

#include <pjsip/sip_transport_tls.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pj/compat/socket.h>
#include <pj/addr_resolv.h>
#include <pj/ssl_sock.h>
#include <pj/assert.h>
#include <pj/hash.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>

#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0

#define THIS_FILE       "sip_transport_tls.c"

#define MAX_ASYNC_CNT   16
#define POOL_LIS_INIT   512
#define POOL_LIS_INC    512
#define POOL_TP_INIT    512
#define POOL_TP_INC     512

struct tls_listener;
struct tls_transport;

/*
 * Definition of TLS/SSL transport listener, and it's descendant of
 * pjsip_tpfactory.
 */
struct tls_listener
{
    pjsip_tpfactory          factory;
    pj_bool_t                is_registered;
    pjsip_endpoint          *endpt;
    pjsip_tpmgr             *tpmgr;
    pj_ssl_sock_t           *ssock;
    pj_sockaddr              bound_addr;
    pj_ssl_cert_t           *cert;
    pjsip_tls_setting        tls_setting;    
    unsigned                 async_cnt;    

    /* Group lock to be used by TLS transport and ioqueue key */
    pj_grp_lock_t           *grp_lock;
};


/*
 * This structure is used to keep delayed transmit operation in a list.
 * A delayed transmission occurs when application sends tx_data when
 * the TLS connect/establishment is still in progress. These delayed
 * transmission will be "flushed" once the socket is connected (either
 * successfully or with errors).
 */
struct delayed_tdata
{
    PJ_DECL_LIST_MEMBER(struct delayed_tdata);
    pjsip_tx_data_op_key    *tdata_op_key;
    pj_time_val              timeout;
};


/*
 * TLS/SSL transport, and it's descendant of pjsip_transport.
 */
struct tls_transport
{
    pjsip_transport          base;
    pj_bool_t                is_server;
    pj_str_t                 remote_name;

    pj_bool_t                is_registered;
    pj_bool_t                is_closing;
    pj_status_t              close_reason;
    pj_ssl_sock_t           *ssock;
    pj_bool_t                has_pending_connect;
    pj_bool_t                verify_server;

    /* Keep-alive timer. */
    pj_timer_entry           ka_timer;
    pj_time_val              last_activity;
    pjsip_tx_data_op_key     ka_op_key;
    pj_str_t                 ka_pkt;

    /* TLS transport can only have  one rdata!
     * Otherwise chunks of incoming PDU may be received on different
     * buffer.
     */
    pjsip_rx_data            rdata;

    /* Pending transmission list. */
    struct delayed_tdata     delayed_list;

    /* Group lock to be used by TLS transport and ioqueue key */
    pj_grp_lock_t           *grp_lock;

    /* Verify callback. */
    pj_bool_t(*on_verify_cb)(const pjsip_tls_on_verify_param *param);
};


/****************************************************************************
 * PROTOTYPES
 */

/* This callback is called when pending accept() operation completes. */
static pj_bool_t on_accept_complete2(pj_ssl_sock_t *ssock,
                                    pj_ssl_sock_t *new_ssock,
                                    const pj_sockaddr_t *src_addr,
                                    int src_addr_len,
                                    pj_status_t status);

/* Callback on incoming data */
static pj_bool_t on_data_read(pj_ssl_sock_t *ssock,
                              void *data,
                              pj_size_t size,
                              pj_status_t status,
                              pj_size_t *remainder);

/* Callback when packet is sent */
static pj_bool_t on_data_sent(pj_ssl_sock_t *ssock,
                              pj_ioqueue_op_key_t *send_key,
                              pj_ssize_t sent);

static pj_bool_t on_verify_cb(pj_ssl_sock_t *ssock, pj_bool_t is_server);

/* This callback is called by transport manager to destroy listener */
static pj_status_t lis_destroy(pjsip_tpfactory *factory);

/* Clean up listener resources (group lock handler) */
static void lis_on_destroy(void *arg);

/* This callback is called by transport manager to create transport */
static pj_status_t lis_create_transport(pjsip_tpfactory *factory,
                                        pjsip_tpmgr *mgr,
                                        pjsip_endpoint *endpt,
                                        const pj_sockaddr *rem_addr,
                                        int addr_len,
                                        pjsip_tx_data *tdata,
                                        pjsip_transport **transport);

/* Common function to create and initialize transport */
static pj_status_t tls_create(struct tls_listener *listener,
                              pj_pool_t *pool,
                              pj_ssl_sock_t *ssock, 
                              pj_bool_t is_server,
                              const pj_sockaddr *local,
                              const pj_sockaddr *remote,
                              const pj_str_t *remote_name,
                              pj_grp_lock_t *glock,
                              struct tls_transport **p_tls);


/* Clean up TLS resources */
static void tls_on_destroy(void *arg);

static void wipe_buf(pj_str_t *buf);


static void tls_perror(const char *sender, const char *title,
                       pj_status_t status, pj_str_t *remote_name)
{
    PJ_PERROR(3,(sender, status, "%s: [code=%d]%s%.*s", title, status,
        remote_name ? " peer: " : "", remote_name ? (int)remote_name->slen : 0,
        remote_name ? remote_name->ptr : ""));
}


static void sockaddr_to_host_port( pj_pool_t *pool,
                                   pjsip_host_port *host_port,
                                   const pj_sockaddr *addr )
{
    host_port->host.ptr = (char*) pj_pool_alloc(pool, PJ_INET6_ADDRSTRLEN+4);
    pj_sockaddr_print(addr, host_port->host.ptr, PJ_INET6_ADDRSTRLEN+4, 0);
    host_port->host.slen = pj_ansi_strlen(host_port->host.ptr);
    host_port->port = pj_sockaddr_get_port(addr);
}


static pj_uint32_t ssl_get_proto(pjsip_ssl_method ssl_method, pj_uint32_t proto)
{
    pj_uint32_t out_proto;

    if (proto)
        return proto;

    if (ssl_method == PJSIP_SSL_UNSPECIFIED_METHOD)
        ssl_method = PJSIP_SSL_DEFAULT_METHOD;

    switch(ssl_method) {
    case PJSIP_SSLV2_METHOD:
        out_proto = PJ_SSL_SOCK_PROTO_SSL2;
        break;
    case PJSIP_SSLV3_METHOD:
        out_proto = PJ_SSL_SOCK_PROTO_SSL3;
        break;
    case PJSIP_TLSV1_METHOD:
        out_proto = PJ_SSL_SOCK_PROTO_TLS1;
        break;
    case PJSIP_TLSV1_1_METHOD:
        out_proto = PJ_SSL_SOCK_PROTO_TLS1_1;
        break;
    case PJSIP_TLSV1_2_METHOD:
        out_proto = PJ_SSL_SOCK_PROTO_TLS1_2;
        break;
    case PJSIP_TLSV1_3_METHOD:
        out_proto = PJ_SSL_SOCK_PROTO_TLS1_3;
        break;
    case PJSIP_SSLV23_METHOD:
        out_proto = PJ_SSL_SOCK_PROTO_SSL23;
        break;
    default:
        out_proto = PJ_SSL_SOCK_PROTO_DEFAULT;
        break;
    }   
    return out_proto;
}


static void tls_init_shutdown(struct tls_transport *tls, pj_status_t status)
{
    pjsip_tp_state_callback state_cb;

    if (tls->close_reason == PJ_SUCCESS)
        tls->close_reason = status;

    if (tls->base.is_shutdown || tls->base.is_destroying)
        return;

    /* Prevent immediate transport destroy by application, as transport
     * state notification callback may be stacked and transport instance
     * must remain valid at any point in the callback.
     */
    pjsip_transport_add_ref(&tls->base);

    /* Notify application of transport disconnected state */
    state_cb = pjsip_tpmgr_get_state_cb(tls->base.tpmgr);
    if (state_cb) {
        pjsip_transport_state_info state_info;
        pjsip_tls_state_info tls_info;
        pj_ssl_sock_info ssl_info;
        
        /* Init transport state info */
        pj_bzero(&state_info, sizeof(state_info));
        state_info.status = tls->close_reason;

        if (tls->ssock && 
            pj_ssl_sock_get_info(tls->ssock, &ssl_info) == PJ_SUCCESS)
        {
            pj_bzero(&tls_info, sizeof(tls_info));
            tls_info.ssl_sock_info = &ssl_info;
            state_info.ext_info = &tls_info;
        }

        (*state_cb)(&tls->base, PJSIP_TP_STATE_DISCONNECTED, &state_info);
    }

    /* check again */
    if (tls->base.is_shutdown || tls->base.is_destroying) {
        pjsip_transport_dec_ref(&tls->base);
        return;
    }

    /* We can not destroy the transport since high level objects may
     * still keep reference to this transport. So we can only 
     * instruct transport manager to gracefully start the shutdown
     * procedure for this transport.
     */
    pjsip_transport_shutdown(&tls->base);

    /* Now, it is ok to destroy the transport. */
    pjsip_transport_dec_ref(&tls->base);
}


/****************************************************************************
 * The TLS listener/transport factory.
 */


static void set_ssock_param(pj_ssl_sock_param *ssock_param,
    struct tls_listener *listener)
{
    int af, sip_ssl_method;
    pj_uint32_t sip_ssl_proto;

    /* Build SSL socket param */
    af = pjsip_transport_type_get_af(listener->factory.type);
    pj_ssl_sock_param_default(ssock_param);
    ssock_param->sock_af = af;
    ssock_param->cb.on_accept_complete2 = &on_accept_complete2;
    if (listener->tls_setting.on_verify_cb)
        ssock_param->cb.on_verify_cb = &on_verify_cb;

    ssock_param->async_cnt = listener->async_cnt;
    ssock_param->ioqueue = pjsip_endpt_get_ioqueue(listener->endpt);
    ssock_param->timer_heap = pjsip_endpt_get_timer_heap(listener->endpt);
    ssock_param->require_client_cert = listener->tls_setting.require_client_cert;
    ssock_param->timeout = listener->tls_setting.timeout;
    ssock_param->user_data = listener;
    ssock_param->verify_peer = PJ_FALSE; /* avoid SSL socket closing the socket
                                          * due to verification error */
    if (ssock_param->send_buffer_size < PJSIP_MAX_PKT_LEN)
        ssock_param->send_buffer_size = PJSIP_MAX_PKT_LEN;
    if (ssock_param->read_buffer_size < PJSIP_MAX_PKT_LEN)
        ssock_param->read_buffer_size = PJSIP_MAX_PKT_LEN;
    ssock_param->ciphers_num = listener->tls_setting.ciphers_num;
    ssock_param->ciphers = listener->tls_setting.ciphers;
    ssock_param->curves_num = listener->tls_setting.curves_num;
    ssock_param->curves = listener->tls_setting.curves;
    ssock_param->sigalgs = listener->tls_setting.sigalgs;
    ssock_param->entropy_type = listener->tls_setting.entropy_type;
    ssock_param->entropy_path = listener->tls_setting.entropy_path;
    ssock_param->reuse_addr = listener->tls_setting.reuse_addr;
    ssock_param->qos_type = listener->tls_setting.qos_type;
    ssock_param->qos_ignore_error = listener->tls_setting.qos_ignore_error;
    pj_memcpy(&ssock_param->qos_params, &listener->tls_setting.qos_params,
              sizeof(ssock_param->qos_params));

    ssock_param->sockopt_ignore_error =
                                    listener->tls_setting.sockopt_ignore_error;

    ssock_param->enable_renegotiation =
                                    listener->tls_setting.enable_renegotiation;
    /* Copy the sockopt */
    if (listener->tls_setting.sockopt_params.cnt > 0) {
        pj_memcpy(&ssock_param->sockopt_params, 
                  &listener->tls_setting.sockopt_params,
                  sizeof(listener->tls_setting.sockopt_params));
    }

    sip_ssl_method = listener->tls_setting.method;
    sip_ssl_proto = listener->tls_setting.proto;
    ssock_param->proto = ssl_get_proto(sip_ssl_method, sip_ssl_proto);
}

static void update_bound_addr(struct tls_listener *listener,
                              const pj_sockaddr *local)
{
    pj_sockaddr *listener_addr = &listener->factory.local_addr;
    int af = pjsip_transport_type_get_af(listener->factory.type);

    /* Bind address may be different than factory.local_addr because
     * factory.local_addr will be resolved.
     */
    if (local) {
        pj_sockaddr_cp(&listener->bound_addr, local);
    } else {
        pj_sockaddr_init(af, &listener->bound_addr, NULL, 0);
    }
    pj_sockaddr_cp(listener_addr, &listener->bound_addr);    
}

static pj_status_t update_factory_addr(struct tls_listener *listener,
                                       const pjsip_host_port *addr_name)
{
    pj_status_t status = PJ_SUCCESS;
    pj_sockaddr *listener_addr = &listener->factory.local_addr;

    if (addr_name && addr_name->host.slen) {
        pj_sockaddr tmp;
        pj_uint16_t af = (pj_uint16_t)
                         pjsip_transport_type_get_af(listener->factory.type);

        tmp.addr.sa_family = af;

        /* Validate IP address only */
        if (pj_inet_pton(af, &addr_name->host, pj_sockaddr_get_addr(&tmp)) ==
            PJ_SUCCESS)
        {
            /* Verify that address given in a_name (if any) is valid */
            status = pj_sockaddr_init(af, &tmp, &addr_name->host,
                                      (pj_uint16_t)addr_name->port);
            if (status != PJ_SUCCESS || !pj_sockaddr_has_addr(&tmp) ||
                (af == pj_AF_INET() &&
                 tmp.ipv4.sin_addr.s_addr == PJ_INADDR_NONE))
            {
                /* Invalid address */
                return PJ_EINVAL;
            }
        }

        /* Copy the address */
        listener->factory.addr_name = *addr_name;
        pj_strdup(listener->factory.pool, &listener->factory.addr_name.host,
                  &addr_name->host);
        listener->factory.addr_name.port = addr_name->port;

    }
    else {
        /* No published address is given, use the bound address */

        /* If the address returns 0.0.0.0, use the default
        * interface address as the transport's address.
        */
        if (!pj_sockaddr_has_addr(listener_addr)) {
            pj_sockaddr hostip;

            status = pj_gethostip(listener->bound_addr.addr.sa_family,
                                  &hostip);
            if (status != PJ_SUCCESS)
                return status;

            pj_sockaddr_copy_addr(listener_addr, &hostip);
        }

        /* Save the address name */
        sockaddr_to_host_port(listener->factory.pool,
                              &listener->factory.addr_name, listener_addr);
    }

    /* If port is zero, get the bound port */
    if (listener->factory.addr_name.port == 0) {
        listener->factory.addr_name.port = pj_sockaddr_get_port(listener_addr);
    }

    pj_ansi_snprintf(listener->factory.obj_name,
                     sizeof(listener->factory.obj_name),
                     "tlstp:%d", listener->factory.addr_name.port);
    return status;
}

static void update_transport_info(struct tls_listener *listener)
{
    enum { INFO_LEN = 100 };
    char local_addr[PJ_INET6_ADDRSTRLEN + 10];
    char pub_addr[PJ_INET6_ADDRSTRLEN + 10];
    int len;
    pj_sockaddr *listener_addr = &listener->factory.local_addr;

    if (listener->factory.info == NULL) {
        listener->factory.info = (char*)pj_pool_alloc(listener->factory.pool,
                                                      INFO_LEN);
    }
    pj_sockaddr_print(listener_addr, local_addr, sizeof(local_addr), 3);
    pj_addr_str_print(&listener->factory.addr_name.host, 
                      listener->factory.addr_name.port, pub_addr, 
                      sizeof(pub_addr), 1);
    len = pj_ansi_snprintf(
            listener->factory.info, INFO_LEN, "tls %s [published as %s]",
            local_addr, pub_addr);
    PJ_CHECK_TRUNC_STR(len, listener->factory.info, INFO_LEN);

    if (listener->ssock) {
        char addr[PJ_INET6_ADDRSTRLEN+10];

        PJ_LOG(4, (listener->factory.obj_name,
               "SIP TLS listener is ready for incoming connections at %s",
               pj_addr_str_print(&listener->factory.addr_name.host,
                                 listener->factory.addr_name.port, addr,
                                 sizeof(addr), 1)));
    } else {
        PJ_LOG(4, (listener->factory.obj_name, "SIP TLS is ready "
               "(client only)"));
    }
}


/*
 * This is the public API to create, initialize, register, and start the
 * TLS listener.
 */
PJ_DEF(pj_status_t) pjsip_tls_transport_start(pjsip_endpoint *endpt,
    const pjsip_tls_setting *opt,
    const pj_sockaddr_in *local_in,
    const pjsip_host_port *a_name,
    unsigned async_cnt,
    pjsip_tpfactory **p_factory)
{
    pj_sockaddr local;

    if (local_in)
        pj_sockaddr_cp(&local, local_in);

    return pjsip_tls_transport_start2(endpt, opt, (local_in ? &local : NULL),
                                      a_name, async_cnt, p_factory);
}


PJ_DEF(pj_status_t) pjsip_tls_transport_lis_start(pjsip_tpfactory *factory,
                                                const pj_sockaddr *local,
                                                const pjsip_host_port *a_name)
{
    pj_status_t status = PJ_SUCCESS;
    pj_ssl_sock_param ssock_param, newsock_param;
    struct tls_listener *listener = (struct tls_listener *)factory;
    pj_sockaddr *listener_addr = &listener->factory.local_addr;

    if (listener->ssock)
        return PJ_SUCCESS;

    set_ssock_param(&ssock_param, listener);
    update_bound_addr(listener, local);
    ssock_param.grp_lock = listener->grp_lock;

    /* Create SSL socket */
    status = pj_ssl_sock_create(listener->factory.pool, &ssock_param, 
                                &listener->ssock);
    if (status != PJ_SUCCESS)
        return status;

    if (listener->cert) {
        status = pj_ssl_sock_set_certificate(listener->ssock, 
                                       listener->factory.pool, listener->cert);
        if (status != PJ_SUCCESS)
            return status;
    }

    /* Start accepting incoming connections. Note that some TLS/SSL
     * backends may not support for SSL socket server.
     */    
    pj_memcpy(&newsock_param, &ssock_param, sizeof(newsock_param));
    newsock_param.async_cnt = 1;
    newsock_param.cb.on_data_read = &on_data_read;
    newsock_param.cb.on_data_sent = &on_data_sent;
    status = pj_ssl_sock_start_accept2(listener->ssock, listener->factory.pool,
                            (pj_sockaddr_t*)listener_addr,
                            pj_sockaddr_get_len((pj_sockaddr_t*)listener_addr),
                            &newsock_param);

    if (status == PJ_SUCCESS || status == PJ_EPENDING) {
        pj_ssl_sock_info info;  

        /* Retrieve the bound address */
        status = pj_ssl_sock_get_info(listener->ssock, &info);
        if (status == PJ_SUCCESS)
            pj_sockaddr_cp(listener_addr, (pj_sockaddr_t*)&info.local_addr);

    }
    status = update_factory_addr(listener, a_name);
    if (status != PJ_SUCCESS)
        return status;

    update_transport_info(listener);

    return status;    
}


PJ_DEF(pj_status_t) pjsip_tls_transport_start2( pjsip_endpoint *endpt,
                                                const pjsip_tls_setting *opt,
                                                const pj_sockaddr *local,
                                                const pjsip_host_port *a_name,
                                                unsigned async_cnt,
                                                pjsip_tpfactory **p_factory)
{        
    pj_pool_t *pool;
    pj_bool_t is_ipv6;    
    struct tls_listener *listener;    
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && async_cnt, PJ_EINVAL);

    is_ipv6 = (local && local->addr.sa_family == pj_AF_INET6());    

    pool = pjsip_endpt_create_pool(endpt, "tlstp", POOL_LIS_INIT, 
                                   POOL_LIS_INC);
    PJ_ASSERT_RETURN(pool, PJ_ENOMEM);

    listener = PJ_POOL_ZALLOC_T(pool, struct tls_listener);
    listener->factory.pool = pool;
    if (is_ipv6)
        listener->factory.type = PJSIP_TRANSPORT_TLS6;
    else
        listener->factory.type = PJSIP_TRANSPORT_TLS;
    listener->factory.type_name = (char*)
                pjsip_transport_get_type_name(listener->factory.type);
    listener->factory.flag = 
                pjsip_transport_get_flag_from_type(listener->factory.type);
    listener->endpt = endpt;

    pj_ansi_strxcpy(listener->factory.obj_name, "tlstp",
                    sizeof(listener->factory.obj_name));
    if (is_ipv6)
        pj_ansi_strxcat(listener->factory.obj_name, "6",
                        sizeof(listener->factory.obj_name));

    if (opt)
        pjsip_tls_setting_copy(pool, &listener->tls_setting, opt);
    else
        pjsip_tls_setting_default(&listener->tls_setting);

    status = pj_lock_create_recursive_mutex(pool, listener->factory.obj_name,
                                            &listener->factory.lock);
    if (status != PJ_SUCCESS)
        goto on_error;

    if (async_cnt > MAX_ASYNC_CNT) 
        async_cnt = MAX_ASYNC_CNT;

    listener->async_cnt = async_cnt;    

    /* Create group lock */
    status = pj_grp_lock_create(pool, NULL, &listener->grp_lock);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Setup group lock handler */
    pj_grp_lock_add_ref(listener->grp_lock);
    pj_grp_lock_add_handler(listener->grp_lock, pool, listener,
                            &lis_on_destroy);

    /* Check if certificate/CA list for SSL socket is set */
    if (listener->tls_setting.cert_file.slen ||
        listener->tls_setting.ca_list_file.slen ||
        listener->tls_setting.ca_list_path.slen || 
        listener->tls_setting.privkey_file.slen) 
    {
        status = pj_ssl_cert_load_from_files2(pool,
                        &listener->tls_setting.ca_list_file,
                        &listener->tls_setting.ca_list_path,
                        &listener->tls_setting.cert_file,
                        &listener->tls_setting.privkey_file,
                        &listener->tls_setting.password,
                        &listener->cert);
        if (status != PJ_SUCCESS)
            goto on_error;
    } else if (listener->tls_setting.ca_buf.slen ||
               listener->tls_setting.cert_buf.slen||
               listener->tls_setting.privkey_buf.slen)
    {
        status = pj_ssl_cert_load_from_buffer(pool,
                        &listener->tls_setting.ca_buf,
                        &listener->tls_setting.cert_buf,
                        &listener->tls_setting.privkey_buf,
                        &listener->tls_setting.password,
                        &listener->cert);
        if (status != PJ_SUCCESS)
            goto on_error;    
    }

    /* Register to transport manager */
    listener->endpt = endpt;
    listener->tpmgr = pjsip_endpt_get_tpmgr(endpt);
    listener->factory.create_transport2 = lis_create_transport;
    listener->factory.destroy = lis_destroy;

#if !(defined(PJSIP_TLS_TRANSPORT_DONT_CREATE_LISTENER) && \
    PJSIP_TLS_TRANSPORT_DONT_CREATE_LISTENER != 0)
    /* Start listener. */
    status = pjsip_tls_transport_lis_start(&listener->factory, local, a_name);
    if (status != PJ_SUCCESS)
        goto on_error;
#else
    update_bound_addr(listener, local);
    /* If published host/IP is specified, then use that address as the
     * listener advertised address.
     */
    status = update_factory_addr(listener, a_name);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Set transport info. */
    update_transport_info(listener);
#endif

    listener->is_registered = PJ_TRUE;
    status = pjsip_tpmgr_register_tpfactory(listener->tpmgr,
                                            &listener->factory);
    if (status != PJ_SUCCESS) {
        listener->is_registered = PJ_FALSE;
        goto on_error;
    }

    /* Return the pointer to user */
    if (p_factory) *p_factory = &listener->factory;

    return PJ_SUCCESS;

on_error:
    lis_destroy(&listener->factory);
    return status;
}


/* Clean up listener resources */
static void lis_on_destroy(void *arg)
{
    struct tls_listener *listener = (struct tls_listener*)arg;

    if (listener->cert) {
        pj_ssl_cert_wipe_keys(listener->cert);
        listener->cert = NULL;
    }

    if (listener->factory.lock) {
        pj_lock_destroy(listener->factory.lock);
        listener->factory.lock = NULL;
    }

    if (listener->factory.pool) {
        PJ_LOG(4,(listener->factory.obj_name,  "SIP TLS transport destroyed"));
        pj_pool_secure_release(&listener->factory.pool);
    }
}


static void lis_close(struct tls_listener *listener)
{
    if (listener->is_registered) {
        pjsip_tpmgr_unregister_tpfactory(listener->tpmgr, &listener->factory);
        listener->is_registered = PJ_FALSE;
    }

    if (listener->ssock) {
        pj_ssl_sock_close(listener->ssock);
        listener->ssock = NULL;
    }
}


/* This callback is called by transport manager to destroy listener */
static pj_status_t lis_destroy(pjsip_tpfactory *factory)
{
    struct tls_listener *listener = (struct tls_listener *)factory;

    lis_close(listener);

    if (listener->grp_lock) {
        pj_grp_lock_t *grp_lock = listener->grp_lock;
        listener->grp_lock = NULL;
        pj_grp_lock_dec_ref(grp_lock);
        /* Listener may have been deleted at this point */
    } else {
        lis_on_destroy(listener);
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_tls_transport_restart(pjsip_tpfactory *factory,
                                                const pj_sockaddr *local,
                                                const pjsip_host_port *a_name)
{
    pj_status_t status = PJ_SUCCESS;
    struct tls_listener *listener = (struct tls_listener *)factory;

    lis_close(listener);

    status = pjsip_tls_transport_lis_start(factory, local, a_name);
    if (status != PJ_SUCCESS) { 
        tls_perror(listener->factory.obj_name, 
                   "Unable to start listener after closing it", status, NULL);

        return status;
    }
    
    status = pjsip_tpmgr_register_tpfactory(listener->tpmgr,
                                            &listener->factory);
    if (status != PJ_SUCCESS) {
        tls_perror(listener->factory.obj_name,
                    "Unable to register the transport listener", status, NULL);

        listener->is_registered = PJ_FALSE;     
    } else {
        listener->is_registered = PJ_TRUE;      
    }    

    return status;
}


/***************************************************************************/
/*
 * TLS Transport
 */

/*
 * Prototypes.
 */
/* Called by transport manager to send message */
static pj_status_t tls_send_msg(pjsip_transport *transport, 
                                pjsip_tx_data *tdata,
                                const pj_sockaddr_t *rem_addr,
                                int addr_len,
                                void *token,
                                pjsip_transport_callback callback);

/* Called by transport manager to shutdown */
static pj_status_t tls_shutdown(pjsip_transport *transport);

/* Called by transport manager to destroy transport */
static pj_status_t tls_destroy_transport(pjsip_transport *transport);

/* Utility to destroy transport */
static pj_status_t tls_destroy(pjsip_transport *transport,
                               pj_status_t reason);

/* Callback when connect completes */
static pj_bool_t on_connect_complete(pj_ssl_sock_t *ssock,
                                     pj_status_t status);

/* TLS keep-alive timer callback */
static void tls_keep_alive_timer(pj_timer_heap_t *th, pj_timer_entry *e);

/*
 * Common function to create TLS transport, called when pending accept() and
 * pending connect() complete.
 */
static pj_status_t tls_create( struct tls_listener *listener,
                               pj_pool_t *pool,
                               pj_ssl_sock_t *ssock,
                               pj_bool_t is_server,
                               const pj_sockaddr *local,
                               const pj_sockaddr *remote,
                               const pj_str_t *remote_name,
                               pj_grp_lock_t *glock,
                               struct tls_transport **p_tls)
{
    struct tls_transport *tls;
    const pj_str_t ka_pkt = PJSIP_TLS_KEEP_ALIVE_DATA;
    char print_addr[PJ_INET6_ADDRSTRLEN+10];
    pj_status_t status;
    

    PJ_ASSERT_RETURN(listener && ssock && local && remote && p_tls, PJ_EINVAL);


    if (pool == NULL) {
        pool = pjsip_endpt_create_pool(listener->endpt, "tls",
                                       POOL_TP_INIT, POOL_TP_INC);
        PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);
    }    

    /*
     * Create and initialize basic transport structure.
     */
    tls = PJ_POOL_ZALLOC_T(pool, struct tls_transport);
    tls->is_server = is_server;
    tls->verify_server = listener->tls_setting.verify_server;
    pj_list_init(&tls->delayed_list);
    tls->base.pool = pool;

    pj_ansi_snprintf(tls->base.obj_name, PJ_MAX_OBJ_NAME, 
                     (is_server ? "tlss%p" :"tlsc%p"), tls);

    status = pj_atomic_create(pool, 0, &tls->base.ref_cnt);
    if (status != PJ_SUCCESS) {
        goto on_error;
    }

    status = pj_lock_create_recursive_mutex(pool, "tls", &tls->base.lock);
    if (status != PJ_SUCCESS) {
        goto on_error;
    }

    if (remote_name)
        pj_strdup(pool, &tls->remote_name, remote_name);

    tls->base.key.type = listener->factory.type;
    pj_sockaddr_cp(&tls->base.key.rem_addr, remote);
    tls->base.type_name = (char*)pjsip_transport_get_type_name(
                                   (pjsip_transport_type_e)tls->base.key.type);
    tls->base.flag = pjsip_transport_get_flag_from_type(
                                   (pjsip_transport_type_e)tls->base.key.type);

    tls->base.info = (char*) pj_pool_alloc(pool, 64);
    pj_ansi_snprintf(tls->base.info, 64, "%s to %s",
                     tls->base.type_name,
                     pj_sockaddr_print(remote, print_addr,
                                       sizeof(print_addr), 3));


    tls->base.addr_len = pj_sockaddr_get_len(remote);
    tls->base.dir = is_server? PJSIP_TP_DIR_INCOMING : PJSIP_TP_DIR_OUTGOING;
    
    /* Set initial local address */
    if (!pj_sockaddr_has_addr(local)) {
        pj_sockaddr_cp(&tls->base.local_addr,
                       &listener->factory.local_addr);
    } else {
        pj_sockaddr_cp(&tls->base.local_addr, local);
    }
    
    sockaddr_to_host_port(pool, &tls->base.local_name, &tls->base.local_addr);
    if (tls->remote_name.slen) {
        tls->base.remote_name.host = tls->remote_name;
        tls->base.remote_name.port = pj_sockaddr_get_port(remote);
    } else {
        sockaddr_to_host_port(pool, &tls->base.remote_name, remote);
    }

    tls->base.endpt = listener->endpt;
    tls->base.tpmgr = listener->tpmgr;
    tls->base.send_msg = &tls_send_msg;
    tls->base.do_shutdown = &tls_shutdown;
    tls->base.destroy = &tls_destroy_transport;
    tls->base.factory = &listener->factory;
    tls->base.initial_timeout = listener->tls_setting.initial_timeout;

    tls->ssock = ssock;
    tls->on_verify_cb = listener->tls_setting.on_verify_cb;

    /* Set up the group lock */
    tls->grp_lock = tls->base.grp_lock = glock;
    pj_grp_lock_add_ref(tls->grp_lock);
    pj_grp_lock_add_handler(tls->grp_lock, pool, tls, &tls_on_destroy);

    /* Register transport to transport manager */
    status = pjsip_transport_register(listener->tpmgr, &tls->base);
    if (status != PJ_SUCCESS) {
        goto on_error;
    }

    tls->is_registered = PJ_TRUE;

    /* Initialize keep-alive timer */
    tls->ka_timer.user_data = (void*)tls;
    tls->ka_timer.cb = &tls_keep_alive_timer;
    pj_ioqueue_op_key_init(&tls->ka_op_key.key, sizeof(pj_ioqueue_op_key_t));
    pj_strdup(tls->base.pool, &tls->ka_pkt, &ka_pkt);
    
    /* Done setting up basic transport. */
    *p_tls = tls;

    PJ_LOG(4,(tls->base.obj_name, "TLS %s transport created",
              (tls->is_server ? "server" : "client")));

    return PJ_SUCCESS;

on_error:
    if (tls->grp_lock && pj_grp_lock_get_ref(tls->grp_lock))
        tls_destroy(&tls->base, status);
    else
        tls_on_destroy(tls);

    return status;
}


/* Flush all delayed transmision once the socket is connected. */
static void tls_flush_pending_tx(struct tls_transport *tls)
{
    pj_time_val now;

    pj_gettickcount(&now);
    pj_lock_acquire(tls->base.lock);
    while (!pj_list_empty(&tls->delayed_list)) {
        struct delayed_tdata *pending_tx;
        pjsip_tx_data *tdata;
        pj_ioqueue_op_key_t *op_key;
        pj_ssize_t size;
        pj_status_t status;

        pending_tx = tls->delayed_list.next;
        pj_list_erase(pending_tx);

        tdata = pending_tx->tdata_op_key->tdata;
        op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

        if (pending_tx->timeout.sec > 0 &&
            PJ_TIME_VAL_GT(now, pending_tx->timeout))
        {
            pj_lock_release(tls->base.lock);
            on_data_sent(tls->ssock, op_key, -PJ_ETIMEDOUT);
            pj_lock_acquire(tls->base.lock);
            continue;
        }

        /* send! */
        size = tdata->buf.cur - tdata->buf.start;
        status = pj_ssl_sock_send(tls->ssock, op_key, tdata->buf.start, 
                                  &size, 0);

        if (status != PJ_EPENDING) {
            pj_lock_release(tls->base.lock);
            on_data_sent(tls->ssock, op_key, size);
            pj_lock_acquire(tls->base.lock);
        }
    }
    pj_lock_release(tls->base.lock);
}


/* Called by transport manager to destroy transport */
static pj_status_t tls_destroy_transport(pjsip_transport *transport)
{
    struct tls_transport *tls = (struct tls_transport*)transport;

    /* Transport would have been unregistered by now since this callback
     * is called by transport manager.
     */
    tls->is_registered = PJ_FALSE;

    return tls_destroy(transport, tls->close_reason);
}


/* Clean up TLS resources */
static void tls_on_destroy(void *arg)
{
    struct tls_transport *tls = (struct tls_transport*)arg;

    if (tls->rdata.tp_info.pool) {
        pj_pool_secure_release(&tls->rdata.tp_info.pool);
    }

    if (tls->base.lock) {
        pj_lock_destroy(tls->base.lock);
        tls->base.lock = NULL;
    }

    if (tls->base.ref_cnt) {
        pj_atomic_destroy(tls->base.ref_cnt);
        tls->base.ref_cnt = NULL;
    }

    if (tls->base.pool) {
        if (tls->close_reason != PJ_SUCCESS) {
            char errmsg[PJ_ERR_MSG_SIZE];

            pj_strerror(tls->close_reason, errmsg, sizeof(errmsg));
            PJ_LOG(4,(tls->base.obj_name, 
                      "TLS transport destroyed with reason %d: %s", 
                      tls->close_reason, errmsg));

        } else {

            PJ_LOG(4,(tls->base.obj_name, 
                      "TLS transport destroyed normally"));

        }
        pj_pool_secure_release(&tls->base.pool);
    }
}

/* Destroy TLS transport */
static pj_status_t tls_destroy(pjsip_transport *transport, 
                               pj_status_t reason)
{
    struct tls_transport *tls = (struct tls_transport*)transport;

    if (tls->close_reason == 0)
        tls->close_reason = reason;

    if (tls->is_registered) {
        tls->is_registered = PJ_FALSE;
        pjsip_transport_destroy(transport);

        /* pjsip_transport_destroy will recursively call this function
         * again.
         */
        return PJ_SUCCESS;
    }

    /* Mark transport as closing */
    tls->is_closing = PJ_TRUE;

    /* Stop keep-alive timer. */
    if (tls->ka_timer.id) {
        pjsip_endpt_cancel_timer(tls->base.endpt, &tls->ka_timer);
        tls->ka_timer.id = PJ_FALSE;
    }

    /* Cancel all delayed transmits */
    while (!pj_list_empty(&tls->delayed_list)) {
        struct delayed_tdata *pending_tx;
        pj_ioqueue_op_key_t *op_key;

        pending_tx = tls->delayed_list.next;
        pj_list_erase(pending_tx);

        op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

        on_data_sent(tls->ssock, op_key, -reason);
    }

    if (tls->ssock) {
        pj_ssl_sock_close(tls->ssock);
        tls->ssock = NULL;
    }

    if (tls->grp_lock) {
        pj_grp_lock_t *grp_lock = tls->grp_lock;
        tls->grp_lock = NULL;
        pj_grp_lock_dec_ref(grp_lock);
        /* Transport may have been deleted at this point */
    }

    return PJ_SUCCESS;
}


/*
 * This utility function creates receive data buffers and start
 * asynchronous recv() operations from the socket. It is called after
 * accept() or connect() operation complete.
 */
static pj_status_t tls_start_read(struct tls_transport *tls)
{
    pj_pool_t *pool;
    pj_uint32_t size;
    pj_sockaddr *rem_addr;
    void *readbuf[1];
    pj_status_t status;

    /* Init rdata */
    pool = pjsip_endpt_create_pool(tls->base.endpt,
                                   "rtd%p",
                                   PJSIP_POOL_RDATA_LEN,
                                   PJSIP_POOL_RDATA_INC);
    if (!pool) {
        tls_perror(tls->base.obj_name, "Unable to create pool", PJ_ENOMEM,
                   NULL);
        return PJ_ENOMEM;
    }

    tls->rdata.tp_info.pool = pool;

    tls->rdata.tp_info.transport = &tls->base;
    tls->rdata.tp_info.tp_data = tls;
    tls->rdata.tp_info.op_key.rdata = &tls->rdata;
    pj_ioqueue_op_key_init(&tls->rdata.tp_info.op_key.op_key, 
                           sizeof(pj_ioqueue_op_key_t));

    tls->rdata.pkt_info.src_addr = tls->base.key.rem_addr;
    tls->rdata.pkt_info.src_addr_len = sizeof(tls->rdata.pkt_info.src_addr);
    rem_addr = &tls->base.key.rem_addr;
    pj_sockaddr_print(rem_addr, tls->rdata.pkt_info.src_name,
                          sizeof(tls->rdata.pkt_info.src_name), 0);
    tls->rdata.pkt_info.src_port = pj_sockaddr_get_port(rem_addr);

    size = sizeof(tls->rdata.pkt_info.packet);
    readbuf[0] = tls->rdata.pkt_info.packet;
    status = pj_ssl_sock_start_read2(tls->ssock, tls->base.pool, size,
                                     readbuf, 0);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
        PJ_PERROR(4, (tls->base.obj_name, status,
                     "pj_ssl_sock_start_read() error"));
        return status;
    }

    return PJ_SUCCESS;
}


/* This callback is called by transport manager for the TLS factory
 * to create outgoing transport to the specified destination.
 */
static pj_status_t lis_create_transport(pjsip_tpfactory *factory,
                                        pjsip_tpmgr *mgr,
                                        pjsip_endpoint *endpt,
                                        const pj_sockaddr *rem_addr,
                                        int addr_len,
                                        pjsip_tx_data *tdata,
                                        pjsip_transport **p_transport)
{
    struct tls_listener *listener;
    struct tls_transport *tls;
    int sip_ssl_method;
    pj_uint32_t sip_ssl_proto;
    pj_pool_t *pool;
    pj_grp_lock_t *glock;
    pj_ssl_sock_t *ssock;
    pj_ssl_sock_param ssock_param;
    pj_sockaddr local_addr;
    pj_str_t remote_name;
    pj_status_t status;

    /* Sanity checks */
    PJ_ASSERT_RETURN(factory && mgr && endpt && rem_addr &&
                     addr_len && p_transport, PJ_EINVAL);

    /* Check that address is a sockaddr_in or sockaddr_in6*/
    PJ_ASSERT_RETURN((rem_addr->addr.sa_family == pj_AF_INET() &&
                      addr_len == sizeof(pj_sockaddr_in)) ||
                     (rem_addr->addr.sa_family == pj_AF_INET6() &&
                      addr_len == sizeof(pj_sockaddr_in6)), PJ_EINVAL);


    listener = (struct tls_listener*)factory;

    pool = pjsip_endpt_create_pool(listener->endpt, "tls",
                                   POOL_TP_INIT, POOL_TP_INC);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    /* Get remote host name from tdata */
    if (tdata)
        remote_name = tdata->dest_info.name;
    else
        pj_bzero(&remote_name, sizeof(remote_name));

    /* Build SSL socket param */
    pj_ssl_sock_param_default(&ssock_param);
    ssock_param.sock_af = (factory->type & PJSIP_TRANSPORT_IPV6) ?
                            pj_AF_INET6() : pj_AF_INET();
    ssock_param.cb.on_connect_complete = &on_connect_complete;
    ssock_param.cb.on_data_read = &on_data_read;
    ssock_param.cb.on_data_sent = &on_data_sent;
    if (listener->tls_setting.on_verify_cb)
        ssock_param.cb.on_verify_cb = &on_verify_cb;
    ssock_param.async_cnt = 1;
    ssock_param.ioqueue = pjsip_endpt_get_ioqueue(listener->endpt);
    ssock_param.timer_heap = pjsip_endpt_get_timer_heap(listener->endpt);
    ssock_param.server_name = remote_name;
    ssock_param.timeout = listener->tls_setting.timeout;
    ssock_param.user_data = NULL; /* pending, must be set later */
    ssock_param.verify_peer = PJ_FALSE; /* avoid SSL socket closing the socket
                                         * due to verification error */
    if (ssock_param.send_buffer_size < PJSIP_MAX_PKT_LEN)
        ssock_param.send_buffer_size = PJSIP_MAX_PKT_LEN;
    if (ssock_param.read_buffer_size < PJSIP_MAX_PKT_LEN)
        ssock_param.read_buffer_size = PJSIP_MAX_PKT_LEN;
    ssock_param.ciphers_num = listener->tls_setting.ciphers_num;
    ssock_param.ciphers = listener->tls_setting.ciphers;
    ssock_param.curves_num = listener->tls_setting.curves_num;
    ssock_param.curves = listener->tls_setting.curves;
    ssock_param.sigalgs = listener->tls_setting.sigalgs;
    ssock_param.entropy_type = listener->tls_setting.entropy_type;
    ssock_param.entropy_path = listener->tls_setting.entropy_path;
    ssock_param.qos_type = listener->tls_setting.qos_type;
    ssock_param.qos_ignore_error = listener->tls_setting.qos_ignore_error;
    pj_memcpy(&ssock_param.qos_params, &listener->tls_setting.qos_params,
              sizeof(ssock_param.qos_params));

    ssock_param.sockopt_ignore_error = 
                                     listener->tls_setting.sockopt_ignore_error;

    ssock_param.enable_renegotiation = listener->tls_setting.enable_renegotiation;
    /* Copy the sockopt */
    if (listener->tls_setting.sockopt_params.cnt > 0) {
        pj_memcpy(&ssock_param.sockopt_params, 
                  &listener->tls_setting.sockopt_params,
                  sizeof(listener->tls_setting.sockopt_params));
    }

    sip_ssl_method = listener->tls_setting.method;
    sip_ssl_proto = listener->tls_setting.proto;
    ssock_param.proto = ssl_get_proto(sip_ssl_method, sip_ssl_proto);

    /* Create group lock */
    status = pj_grp_lock_create(pool, NULL, &glock);
    if (status != PJ_SUCCESS)
        return status;

    ssock_param.grp_lock = glock;
    status = pj_ssl_sock_create(pool, &ssock_param, &ssock);
    if (status != PJ_SUCCESS) {
        pj_grp_lock_destroy(glock);
        return status;
    }

    /* Apply SSL certificate */
    if (listener->cert) {
        status = pj_ssl_sock_set_certificate(ssock, pool, listener->cert);
        if (status != PJ_SUCCESS) {
            pj_grp_lock_destroy(glock);
            return status;
        }
    }

    /* Initially set bind address to listener's bind address */
    pj_sockaddr_init(listener->bound_addr.addr.sa_family,
                     &local_addr, NULL, 0);
    pj_sockaddr_copy_addr(&local_addr, &listener->bound_addr);

    /* Create the transport descriptor */
    status = tls_create(listener, pool, ssock, PJ_FALSE, &local_addr, 
                        rem_addr, &remote_name, glock, &tls);
    if (status != PJ_SUCCESS)
        return status;

    /* Set the "pending" SSL socket user data */
    pj_ssl_sock_set_user_data(tls->ssock, tls);

    /* Start asynchronous connect() operation */
    tls->has_pending_connect = PJ_TRUE;
    status = pj_ssl_sock_start_connect(tls->ssock, tls->base.pool, 
                                       (pj_sockaddr_t*)&local_addr,
                                       (pj_sockaddr_t*)rem_addr,
                                       addr_len);
    if (status == PJ_SUCCESS) {
        on_connect_complete(tls->ssock, PJ_SUCCESS);
    } else if (status != PJ_EPENDING) {
        tls_destroy(&tls->base, status);
        return status;
    }

    if (tls->has_pending_connect) {
        pj_ssl_sock_info info;
        char local_addr_buf[PJ_INET6_ADDRSTRLEN+10];
        char remote_addr_buf[PJ_INET6_ADDRSTRLEN+10];

        /* Update local address, just in case local address currently set is 
         * different now that asynchronous connect() is started.
         */

        /* Retrieve the bound address */
        status = pj_ssl_sock_get_info(tls->ssock, &info);
        if (status == PJ_SUCCESS) {
            pj_uint16_t new_port;

            new_port = pj_sockaddr_get_port((pj_sockaddr_t*)&info.local_addr);

            if (pj_sockaddr_has_addr((pj_sockaddr_t*)&info.local_addr)) {
                /* Update sockaddr */
                pj_sockaddr_cp((pj_sockaddr_t*)&tls->base.local_addr,
                               (pj_sockaddr_t*)&info.local_addr);
            } else if (new_port && new_port != pj_sockaddr_get_port(
                                        (pj_sockaddr_t*)&tls->base.local_addr))
            {
                /* Update port only */
                pj_sockaddr_set_port(&tls->base.local_addr, 
                                     new_port);
            }

            sockaddr_to_host_port(tls->base.pool, &tls->base.local_name,
                                  &tls->base.local_addr);
        }

        PJ_LOG(4,(tls->base.obj_name, 
                  "TLS transport %s is connecting to %s...",
                  pj_addr_str_print(&tls->base.local_name.host, 
                                    tls->base.local_name.port, 
                                    local_addr_buf, sizeof(local_addr_buf), 1),
                  pj_addr_str_print(&tls->base.remote_name.host, 
                                tls->base.remote_name.port, 
                                remote_addr_buf, sizeof(remote_addr_buf), 1)));
    }

    /* Done */
    *p_transport = &tls->base;

    return PJ_SUCCESS;
}


static pj_bool_t on_accept_complete2(pj_ssl_sock_t *ssock,
                                     pj_ssl_sock_t *new_ssock,
                                     const pj_sockaddr_t *src_addr,
                                     int src_addr_len, 
                                     pj_status_t accept_status)
{    
    struct tls_listener *listener;
    struct tls_transport *tls;
    pj_ssl_sock_info ssl_info;
    char addr[PJ_INET6_ADDRSTRLEN+10];
    pjsip_tp_state_callback state_cb;
    pj_sockaddr tmp_src_addr;
    pj_bool_t is_shutdown;
    pj_status_t status;
    char addr_buf[PJ_INET6_ADDRSTRLEN+10];        

    PJ_UNUSED_ARG(src_addr_len);

    listener = (struct tls_listener*) pj_ssl_sock_get_user_data(ssock);
    if (!listener) {
        /* Listener already destroyed, e.g: after TCP accept but before SSL
         * handshake is completed.
         */
        if (new_ssock && accept_status == PJ_SUCCESS) {
            /* Close the SSL socket if the accept op is successful */
            PJ_LOG(4,(THIS_FILE,
                      "Incoming TLS connection from %s (sock=%p) is discarded "
                      "because listener is already destroyed",
                      pj_sockaddr_print(src_addr, addr, sizeof(addr), 3),
                      new_ssock));

            pj_ssl_sock_close(new_ssock);
        }

        return PJ_FALSE;
    }

    if (accept_status != PJ_SUCCESS) {
        if (listener->tls_setting.on_accept_fail_cb) {
            pjsip_tls_on_accept_fail_param param;
            pj_ssl_sock_info ssi;

            pj_bzero(&param, sizeof(param));
            param.status = accept_status;
            param.local_addr = &listener->factory.local_addr;
            param.remote_addr = src_addr;
            if (new_ssock &&
                pj_ssl_sock_get_info(new_ssock, &ssi) == PJ_SUCCESS)
            {
                param.last_native_err = ssi.last_native_err;
            }

            (*listener->tls_setting.on_accept_fail_cb) (&param);
        }

        return PJ_FALSE;
    }

    PJ_ASSERT_RETURN(new_ssock, PJ_TRUE);

    if (!listener->is_registered) {
        pj_ssl_sock_close(new_ssock);

        if (listener->tls_setting.on_accept_fail_cb) {
            pjsip_tls_on_accept_fail_param param;
            pj_bzero(&param, sizeof(param));
            param.status = PJSIP_TLS_EACCEPT;
            param.local_addr = &listener->factory.local_addr;
            param.remote_addr = src_addr;
            (*listener->tls_setting.on_accept_fail_cb) (&param);
        }
        return PJ_FALSE;
    }   

    PJ_LOG(4,(listener->factory.obj_name, 
              "TLS listener %s: got incoming TLS connection "
              "from %s, sock=%p",
              pj_addr_str_print(&listener->factory.addr_name.host, 
                                listener->factory.addr_name.port, addr_buf, 
                                sizeof(addr_buf), 1),
              pj_sockaddr_print(src_addr, addr, sizeof(addr), 3),
              new_ssock));

    /* Retrieve SSL socket info, close the socket if this is failed
     * as the SSL socket info availability is rather critical here.
     */
    status = pj_ssl_sock_get_info(new_ssock, &ssl_info);
    if (status != PJ_SUCCESS) {
        pj_ssl_sock_close(new_ssock);

        if (listener->tls_setting.on_accept_fail_cb) {
            pjsip_tls_on_accept_fail_param param;
            pj_bzero(&param, sizeof(param));
            param.status = status;
            param.local_addr = &listener->factory.local_addr;
            param.remote_addr = src_addr;
            (*listener->tls_setting.on_accept_fail_cb) (&param);
        }
        return PJ_TRUE;
    }

    /* Copy to larger buffer, just in case */
    pj_bzero(&tmp_src_addr, sizeof(tmp_src_addr));
    pj_sockaddr_cp(&tmp_src_addr, src_addr);

    /* 
     * Incoming connection!
     * Create TLS transport for the new socket.
     */
    status = tls_create( listener, NULL, new_ssock, PJ_TRUE,
                         &ssl_info.local_addr, &tmp_src_addr, NULL,
                         ssl_info.grp_lock, &tls);
    
    if (status != PJ_SUCCESS) {
        pj_ssl_sock_close(new_ssock);

        if (listener->tls_setting.on_accept_fail_cb) {
            pjsip_tls_on_accept_fail_param param;
            pj_bzero(&param, sizeof(param));
            param.status = status;
            param.local_addr = &listener->factory.local_addr;
            param.remote_addr = src_addr;
            (*listener->tls_setting.on_accept_fail_cb) (&param);
        }
        return PJ_TRUE;
    }

    /* Set the "pending" SSL socket user data */
    pj_ssl_sock_set_user_data(new_ssock, tls);

    /* Prevent immediate transport destroy as application may access it 
     * (getting info, etc) in transport state notification callback.
     */
    pjsip_transport_add_ref(&tls->base);

    /* If there is verification error and verification is mandatory, shutdown
     * and destroy the transport.
     */
    if (ssl_info.verify_status && listener->tls_setting.verify_client) {
        if (tls->close_reason == PJ_SUCCESS) 
            tls->close_reason = PJSIP_TLS_ECERTVERIF;
        pjsip_transport_shutdown(&tls->base);
    }
    /* Notify transport state to application */
    state_cb = pjsip_tpmgr_get_state_cb(tls->base.tpmgr);
    if (state_cb) {
        pjsip_transport_state_info state_info;
        pjsip_tls_state_info tls_info;
        pjsip_transport_state tp_state;

        /* Init transport state info */
        pj_bzero(&tls_info, sizeof(tls_info));
        pj_bzero(&state_info, sizeof(state_info));
        tls_info.ssl_sock_info = &ssl_info;
        state_info.ext_info = &tls_info;

        /* Set transport state based on verification status */
        if (ssl_info.verify_status && listener->tls_setting.verify_client)
        {
            tp_state = PJSIP_TP_STATE_DISCONNECTED;
            state_info.status = PJSIP_TLS_ECERTVERIF;
        } else {
            tp_state = PJSIP_TP_STATE_CONNECTED;
            state_info.status = PJ_SUCCESS;
        }

        (*state_cb)(&tls->base, tp_state, &state_info);
    }

    /* Release transport reference. If transport is shutting down, it may
     * get destroyed here.
     */
    is_shutdown = tls->base.is_shutdown;
    pjsip_transport_dec_ref(&tls->base);
    if (is_shutdown)
        return PJ_TRUE;

    /* Start keep-alive timer */
    if (pjsip_cfg()->tls.keep_alive_interval) {
        pj_time_val delay = {0};
        delay.sec = pjsip_cfg()->tls.keep_alive_interval;
        pjsip_endpt_schedule_timer(listener->endpt,
                                   &tls->ka_timer,
                                   &delay);
        tls->ka_timer.id = PJ_TRUE;
        pj_gettimeofday(&tls->last_activity);
    }

    status = tls_start_read(tls);
    if (status != PJ_SUCCESS) {
        PJ_LOG(3,(tls->base.obj_name, "New transport cancelled"));
        tls_init_shutdown(tls, status);
        tls_destroy(&tls->base, status);
    }

    return PJ_TRUE;
}

/*
 * This callback is called by SSL socket when pending accept() operation
 * has completed.
 */
//static pj_bool_t on_accept_complete(pj_ssl_sock_t *ssock,
//                                  pj_ssl_sock_t *new_ssock,
//                                  const pj_sockaddr_t *src_addr,
//                                  int src_addr_len)
//{
//    PJ_UNUSED_ARG(src_addr_len);
//}


/* 
 * Callback from ioqueue when packet is sent.
 */
static pj_bool_t on_data_sent(pj_ssl_sock_t *ssock,
                              pj_ioqueue_op_key_t *op_key,
                              pj_ssize_t bytes_sent)
{
    struct tls_transport *tls = (struct tls_transport*) 
                                pj_ssl_sock_get_user_data(ssock);
    pjsip_tx_data_op_key *tdata_op_key = (pjsip_tx_data_op_key*)op_key;

    /* Note that op_key may be the op_key from keep-alive, thus
     * it will not have tdata etc.
     */

    tdata_op_key->tdata = NULL;

    if (tdata_op_key->callback) {
        /*
         * Notify sip_transport.c that packet has been sent.
         */
        if (bytes_sent == 0)
            bytes_sent = -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);

        tdata_op_key->callback(&tls->base, tdata_op_key->token, bytes_sent);

        /* Mark last activity time */
        pj_gettimeofday(&tls->last_activity);

    }

    /* Check for error/closure */
    if (bytes_sent <= 0) {
        pj_status_t status;

        PJ_LOG(5,(tls->base.obj_name, "TLS send() error, sent=%ld", 
                  bytes_sent));

        status = (bytes_sent == 0) ? PJ_RETURN_OS_ERROR(OSERR_ENOTCONN) :
                                     (pj_status_t)-bytes_sent;

        tls_init_shutdown(tls, status);

        return PJ_FALSE;
    }
    
    return PJ_TRUE;
}

static pj_bool_t on_verify_cb(pj_ssl_sock_t* ssock, pj_bool_t is_server)
{
    pj_bool_t(*verify_cb)(const pjsip_tls_on_verify_param * param) = NULL;

    if (is_server) {
        struct tls_listener* tls;

        tls = (struct tls_listener*)pj_ssl_sock_get_user_data(ssock);
        verify_cb = tls->tls_setting.on_verify_cb;
    } else {
        struct tls_transport* tls;

        tls = (struct tls_transport*)pj_ssl_sock_get_user_data(ssock);
        verify_cb = tls->on_verify_cb;
    }

    if (verify_cb) {
        pjsip_tls_on_verify_param param;
        pj_ssl_sock_info info;

        pj_bzero(&param, sizeof(param));
        pj_ssl_sock_get_info(ssock, &info);

        param.local_addr = &info.local_addr;
        param.remote_addr = &info.remote_addr;
        param.local_cert_info = info.local_cert_info;
        param.remote_cert_info = info.remote_cert_info;
        param.tp_dir = is_server?PJSIP_TP_DIR_INCOMING:PJSIP_TP_DIR_OUTGOING;
        param.ssock = ssock;
        
        return (*verify_cb)(&param);
    }
    return PJ_TRUE;
}


/* 
 * This callback is called by transport manager to send SIP message 
 */
static pj_status_t tls_send_msg(pjsip_transport *transport, 
                                pjsip_tx_data *tdata,
                                const pj_sockaddr_t *rem_addr,
                                int addr_len,
                                void *token,
                                pjsip_transport_callback callback)
{
    struct tls_transport *tls = (struct tls_transport*)transport;
    pj_ssize_t size;
    pj_bool_t delayed = PJ_FALSE;
    pj_status_t status = PJ_SUCCESS;

    /* Sanity check */
    PJ_ASSERT_RETURN(transport && tdata, PJ_EINVAL);

    /* Check that there's no pending operation associated with the tdata */
    PJ_ASSERT_RETURN(tdata->op_key.tdata == NULL, PJSIP_EPENDINGTX);
    
    /* Check the address is supported */
    PJ_ASSERT_RETURN(rem_addr && (addr_len==sizeof(pj_sockaddr_in) ||
                                  addr_len==sizeof(pj_sockaddr_in6)),
                     PJ_EINVAL);

    /* Init op key. */
    tdata->op_key.tdata = tdata;
    tdata->op_key.token = token;
    tdata->op_key.callback = callback;

    /* If asynchronous connect() has not completed yet, just put the
     * transmit data in the pending transmission list since we can not
     * use the socket yet.
     */
    if (tls->has_pending_connect) {

        /*
         * Looks like connect() is still in progress. Check again (this time
         * with holding the lock) to be sure.
         */
        pj_lock_acquire(tls->base.lock);

        if (tls->has_pending_connect) {
            struct delayed_tdata *delayed_tdata;

            /*
             * connect() is still in progress. Put the transmit data to
             * the delayed list.
             * Starting from #1583 (https://github.com/pjsip/pjproject/issues/1583),
             * we also add timeout value for the transmit data. When the
             * connect() is completed, the timeout value will be checked to
             * determine whether the transmit data needs to be sent.
             */
            delayed_tdata = PJ_POOL_ZALLOC_T(tdata->pool, 
                                             struct delayed_tdata);
            delayed_tdata->tdata_op_key = &tdata->op_key;
            if (tdata->msg && tdata->msg->type == PJSIP_REQUEST_MSG) {
                pj_gettickcount(&delayed_tdata->timeout);
                delayed_tdata->timeout.msec += pjsip_cfg()->tsx.td;
                pj_time_val_normalize(&delayed_tdata->timeout);
            }

            pj_list_push_back(&tls->delayed_list, delayed_tdata);
            status = PJ_EPENDING;

            /* Prevent pj_ioqueue_send() to be called below */
            delayed = PJ_TRUE;
        }

        pj_lock_release(tls->base.lock);
    } 
    
    if (!delayed) {
        /*
         * Transport is ready to go. Send the packet to ioqueue to be
         * sent asynchronously.
         */
        size = tdata->buf.cur - tdata->buf.start;
        status = pj_ssl_sock_send(tls->ssock, 
                                    (pj_ioqueue_op_key_t*)&tdata->op_key,
                                    tdata->buf.start, &size, 0);

        if (status != PJ_EPENDING) {
            /* Not pending (could be immediate success or error) */
            tdata->op_key.tdata = NULL;

            /* Shutdown transport on closure/errors */
            if (size <= 0) {

                PJ_LOG(5,(tls->base.obj_name, "TLS send() error, sent=%ld", 
                          size));

                if (status == PJ_SUCCESS) 
                    status = PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);

                tls_init_shutdown(tls, status);
            }
        }
    }

    return status;
}


/* 
 * This callback is called by transport manager to shutdown transport.
 */
static pj_status_t tls_shutdown(pjsip_transport *transport)
{
    struct tls_transport *tls = (struct tls_transport*)transport;
    
    /* Stop keep-alive timer. */
    if (tls->ka_timer.id) {
        pjsip_endpt_cancel_timer(tls->base.endpt, &tls->ka_timer);
        tls->ka_timer.id = PJ_FALSE;
    }

    return PJ_SUCCESS;
}


/* 
 * Callback from ioqueue that an incoming data is received from the socket.
 */
static pj_bool_t on_data_read(pj_ssl_sock_t *ssock,
                              void *data,
                              pj_size_t size,
                              pj_status_t status,
                              pj_size_t *remainder)
{
    enum { MAX_IMMEDIATE_PACKET = 10 };
    struct tls_transport *tls;
    pjsip_rx_data *rdata;

    PJ_UNUSED_ARG(data);

    tls = (struct tls_transport*) pj_ssl_sock_get_user_data(ssock);
    rdata = &tls->rdata;

    /* Don't do anything if transport is closing. */
    if (tls->is_closing) {
        tls->is_closing++;
        return PJ_FALSE;
    }

    /* Houston, we have packet! Report the packet to transport manager
     * to be parsed.
     */
    if (status == PJ_SUCCESS) {
        pj_size_t size_eaten;

        /* Mark this as an activity */
        pj_gettimeofday(&tls->last_activity);

        pj_assert((void*)rdata->pkt_info.packet == data);

        /* Init pkt_info part. */
        rdata->pkt_info.len = size;
        rdata->pkt_info.zero = 0;
        pj_gettimeofday(&rdata->pkt_info.timestamp);

        /* Report to transport manager.
         * The transport manager will tell us how many bytes of the packet
         * have been processed (as valid SIP message).
         */
        size_eaten = 
            pjsip_tpmgr_receive_packet(rdata->tp_info.transport->tpmgr, 
                                       rdata);

        pj_assert(size_eaten <= (pj_size_t)rdata->pkt_info.len);

        /* Move unprocessed data to the front of the buffer */
        *remainder = size - size_eaten;
        if (*remainder > 0 && *remainder != size) {
            pj_memmove(rdata->pkt_info.packet,
                       rdata->pkt_info.packet + size_eaten,
                       *remainder);
        }

    } else {

        /* Transport is closed */
        PJ_LOG(4,(tls->base.obj_name, "TLS connection closed"));

        tls_init_shutdown(tls, status);

        return PJ_FALSE;

    }

    /* Reset pool. */
    pj_pool_reset(rdata->tp_info.pool);

    return PJ_TRUE;
}


/* 
 * Callback from ioqueue when asynchronous connect() operation completes.
 */
static pj_bool_t on_connect_complete(pj_ssl_sock_t *ssock,
                                     pj_status_t status)
{
    struct tls_transport *tls;
    pj_ssl_sock_info ssl_info;
    pj_sockaddr addr, *tp_addr;
    pjsip_tp_state_callback state_cb;
    pj_bool_t is_shutdown;
    char local_addr_buf[PJ_INET6_ADDRSTRLEN+10];
    char remote_addr_buf[PJ_INET6_ADDRSTRLEN+10];

    tls = (struct tls_transport*) pj_ssl_sock_get_user_data(ssock);

    /* If transport is being shutdown/destroyed, proceed as error connect.
     * Note that it is important to notify application via on_data_sent()
     * as otherwise the transport reference counter may never reach zero
     * (see #1898).
     */
    if ((tls->base.is_shutdown || tls->base.is_destroying) &&
        status == PJ_SUCCESS)
    {
        status = PJ_ECANCELLED;
    }

    /* Check connect() status */
    if (status != PJ_SUCCESS) {

        tls_perror(tls->base.obj_name, "TLS connect() error", status,
                   &tls->remote_name);

        /* Cancel all delayed transmits */
        while (!pj_list_empty(&tls->delayed_list)) {
            struct delayed_tdata *pending_tx;
            pj_ioqueue_op_key_t *op_key;

            pending_tx = tls->delayed_list.next;
            pj_list_erase(pending_tx);

            op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

            on_data_sent(tls->ssock, op_key, -status);
        }

        goto on_error;
    }

    /* Retrieve SSL socket info, shutdown the transport if this is failed
     * as the SSL socket info availability is rather critical here.
     */
    status = pj_ssl_sock_get_info(tls->ssock, &ssl_info);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Update (again) local address, just in case local address currently
     * set is different now that the socket is connected (could happen
     * on some systems, like old Win32 probably?).
     */
    tp_addr = &tls->base.local_addr;
    pj_sockaddr_cp((pj_sockaddr_t*)&addr, 
                   (pj_sockaddr_t*)&ssl_info.local_addr);
    if (pj_sockaddr_cmp(tp_addr, &addr) != 0) {
        pj_sockaddr_cp(tp_addr, &addr);
        sockaddr_to_host_port(tls->base.pool, &tls->base.local_name,
                              tp_addr);
    }

    /* Server identity verification based on server certificate. */
    if (ssl_info.remote_cert_info->version) {
        pj_str_t *remote_name;
        pj_ssl_cert_info *serv_cert = ssl_info.remote_cert_info;
        pj_bool_t matched = PJ_FALSE;
        unsigned i;

        /* Remote name may be hostname or IP address */
        if (tls->remote_name.slen)
            remote_name = &tls->remote_name;
        else
            remote_name = &tls->base.remote_name.host;

        /* Start matching remote name with SubjectAltName fields of 
         * server certificate.
         */
        for (i = 0; i < serv_cert->subj_alt_name.cnt && !matched; ++i) {
            pj_str_t *cert_name = &serv_cert->subj_alt_name.entry[i].name;

            switch (serv_cert->subj_alt_name.entry[i].type) {
            case PJ_SSL_CERT_NAME_DNS:
            case PJ_SSL_CERT_NAME_IP:
                matched = !pj_stricmp(remote_name, cert_name);
                break;
            case PJ_SSL_CERT_NAME_URI:
                if (pj_strnicmp2(cert_name, "sip:", 4) == 0 ||
                    pj_strnicmp2(cert_name, "sips:", 5) == 0)
                {
                    pj_str_t host_part;
                    char *p;

                    p = pj_strchr(cert_name, ':') + 1;
                    pj_strset(&host_part, p, cert_name->slen - 
                                             (p - cert_name->ptr));
                    matched = !pj_stricmp(remote_name, &host_part);
                }
                break;
            default:
                break;
            }
        }
        
        /* When still not matched or no SubjectAltName fields in server
         * certificate, try with Common Name of Subject field.
         */
        if (!matched) {
            matched = !pj_stricmp(remote_name, &serv_cert->subject.cn);
        }

        if (!matched) {
            if (pj_strnicmp2(&serv_cert->subject.cn, "*.", 2) == 0) {
                PJ_LOG(1,(tls->base.obj_name,
                    "RFC 5922 (section 7.2) does not allow TLS wildcard "
                        "certificates. Advise your SIP provider, please!"));
            }
            ssl_info.verify_status |= PJ_SSL_CERT_EIDENTITY_NOT_MATCH;
        }
    }

    /* Prevent immediate transport destroy as application may access it 
     * (getting info, etc) in transport state notification callback.
     */
    pjsip_transport_add_ref(&tls->base);

    /* If there is verification error and verification is mandatory, shutdown
     * and destroy the transport.
     */
    if (ssl_info.verify_status && tls->verify_server) {
        if (tls->close_reason == PJ_SUCCESS) 
            tls->close_reason = PJSIP_TLS_ECERTVERIF;
        pjsip_transport_shutdown(&tls->base);
    }

    /* Notify transport state to application */
    state_cb = pjsip_tpmgr_get_state_cb(tls->base.tpmgr);
    if (state_cb) {
        pjsip_transport_state_info state_info;
        pjsip_tls_state_info tls_info;
        pjsip_transport_state tp_state;

        /* Init transport state info */
        pj_bzero(&state_info, sizeof(state_info));
        pj_bzero(&tls_info, sizeof(tls_info));
        state_info.ext_info = &tls_info;
        tls_info.ssl_sock_info = &ssl_info;

        /* Set transport state based on verification status */
        if (ssl_info.verify_status && tls->verify_server)
        {
            tp_state = PJSIP_TP_STATE_DISCONNECTED;
            state_info.status = PJSIP_TLS_ECERTVERIF;
        } else {
            tp_state = PJSIP_TP_STATE_CONNECTED;
            state_info.status = PJ_SUCCESS;
        }

        (*state_cb)(&tls->base, tp_state, &state_info);
    }

    /* Release transport reference. If transport is shutting down, it may
     * get destroyed here.
     */
    is_shutdown = tls->base.is_shutdown;
    pjsip_transport_dec_ref(&tls->base);
    if (is_shutdown) {
        status = tls->close_reason;
        tls_perror(tls->base.obj_name, "TLS connect() error", status, 
                   &tls->remote_name);

        /* Cancel all delayed transmits */
        while (!pj_list_empty(&tls->delayed_list)) {
            struct delayed_tdata *pending_tx;
            pj_ioqueue_op_key_t *op_key;

            pending_tx = tls->delayed_list.next;
            pj_list_erase(pending_tx);

            op_key = (pj_ioqueue_op_key_t*)pending_tx->tdata_op_key;

            on_data_sent(tls->ssock, op_key, -status);
        }

        return PJ_FALSE;
    }


    /* Mark that pending connect() operation has completed. */
    tls->has_pending_connect = PJ_FALSE;

    PJ_LOG(4,(tls->base.obj_name, 
              "TLS transport %s is connected to %s",
              pj_addr_str_print(&tls->base.local_name.host, 
                                tls->base.local_name.port, local_addr_buf, 
                                sizeof(local_addr_buf), 1),
              pj_addr_str_print(&tls->base.remote_name.host, 
                                tls->base.remote_name.port, remote_addr_buf, 
                                sizeof(remote_addr_buf), 1)));

    /* Start pending read */
    status = tls_start_read(tls);
    if (status != PJ_SUCCESS)
        goto on_error;

    /* Flush all pending send operations */
    tls_flush_pending_tx(tls);

    /* Start keep-alive timer */
    if (pjsip_cfg()->tls.keep_alive_interval) {
        pj_time_val delay = {0};            
        delay.sec = pjsip_cfg()->tls.keep_alive_interval;
        pjsip_endpt_schedule_timer(tls->base.endpt, &tls->ka_timer, 
                                   &delay);
        tls->ka_timer.id = PJ_TRUE;
        pj_gettimeofday(&tls->last_activity);
    }

    return PJ_TRUE;

on_error:
    tls_init_shutdown(tls, status);

    return PJ_FALSE;
}


/* Transport keep-alive timer callback */
static void tls_keep_alive_timer(pj_timer_heap_t *th, pj_timer_entry *e)
{
    struct tls_transport *tls = (struct tls_transport*) e->user_data;
    pj_time_val delay;
    pj_time_val now;
    pj_ssize_t size;
    pj_status_t status;
    char addr[PJ_INET6_ADDRSTRLEN+10];    

    PJ_UNUSED_ARG(th);

    tls->ka_timer.id = PJ_TRUE;

    pj_gettimeofday(&now);
    PJ_TIME_VAL_SUB(now, tls->last_activity);

    if (now.sec > 0 && now.sec < pjsip_cfg()->tls.keep_alive_interval) {
        /* There has been activity, so don't send keep-alive */
        delay.sec = pjsip_cfg()->tls.keep_alive_interval - now.sec;
        delay.msec = 0;

        pjsip_endpt_schedule_timer(tls->base.endpt, &tls->ka_timer, 
                                   &delay);
        tls->ka_timer.id = PJ_TRUE;
        return;
    }

    PJ_LOG(5,(tls->base.obj_name, "Sending %d byte(s) keep-alive to %s", 
              (int)tls->ka_pkt.slen, 
              pj_addr_str_print(&tls->base.remote_name.host, 
                                tls->base.remote_name.port, addr, 
                                sizeof(addr), 1)));

    /* Send the data */
    size = tls->ka_pkt.slen;
    status = pj_ssl_sock_send(tls->ssock, &tls->ka_op_key.key,
                              tls->ka_pkt.ptr, &size, 0);

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
        tls_perror(tls->base.obj_name, 
                   "Error sending keep-alive packet", status,
                   &tls->remote_name);

        tls_init_shutdown(tls, status);
        return;
    }

    /* Register next keep-alive */
    delay.sec = pjsip_cfg()->tls.keep_alive_interval;
    delay.msec = 0;

    pjsip_endpt_schedule_timer(tls->base.endpt, &tls->ka_timer, 
                               &delay);
    tls->ka_timer.id = PJ_TRUE;
}


static void wipe_buf(pj_str_t *buf)
{
    volatile char *p = buf->ptr;
    pj_ssize_t len = buf->slen;
    while (len--) *p++ = 0;
    buf->slen = 0;
}

/*
 * Wipe out certificates and keys in the TLS setting buffer.
 */
PJ_DEF(void) pjsip_tls_setting_wipe_keys(pjsip_tls_setting *opt)
{
    wipe_buf(&opt->ca_list_file);
    wipe_buf(&opt->ca_list_path);
    wipe_buf(&opt->cert_file);
    wipe_buf(&opt->privkey_file);
    wipe_buf(&opt->password);
    wipe_buf(&opt->sigalgs);
    wipe_buf(&opt->entropy_path);
    wipe_buf(&opt->ca_buf);
    wipe_buf(&opt->cert_buf);
    wipe_buf(&opt->privkey_buf);    
}

#endif /* PJSIP_HAS_TLS_TRANSPORT */
