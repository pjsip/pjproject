/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjsip/sip_transport.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/string.h>


/* Only build when PJSIP_HAS_TLS_TRANSPORT is enabled */
#if defined(PJSIP_HAS_TLS_TRANSPORT) && PJSIP_HAS_TLS_TRANSPORT!=0


/* OpenSSL headers */

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef _MSC_VER
# ifdef _DEBUG
#  pragma comment( lib, "libeay32MTd")
#  pragma comment( lib, "ssleay32MTd")
#else
#  pragma comment( lib, "libeay32MT")
#  pragma comment( lib, "ssleay32MT")
# endif
#endif

/**
 * @hideinitializer
 * Unable to read SSL certificate file.
 */
#define PJSIP_TLS_ECERTFILE	PJ_EUNKNOWN
/**
 * @hideinitializer
 * Unable to read SSL private key file.
 */
#define PJSIP_TLS_EKEYFILE	PJ_EUNKNOWN
/**
 * @hideinitializer
 * Unable to list SSL CA list.
 */
#define PJSIP_TLS_ECALIST	PJ_EUNKNOWN
/**
 * @hideinitializer
 * SSL connect() error
 */
#define PJSIP_TLS_ECONNECT	PJ_EUNKNOWN
/**
 * @hideinitializer
 * Error sending SSL data
 */
#define PJSIP_TLS_ESEND		PJ_EUNKNOWN


#define THIS_FILE	"tls_ssl"


/*
 * TLS transport factory/listener.
 */
struct tls_listener
{
    pjsip_tpfactory  base;
    pjsip_endpoint  *endpt;
    pjsip_tpmgr	    *tpmgr;

    pj_bool_t	     is_registered;

    SSL_CTX	    *ctx;
    pj_str_t	     password;
};


/*
 * TLS transport.
 */
struct tls_transport
{
    pjsip_transport  base;

    pj_sock_t	     sock;
    SSL		    *ssl;
    BIO		    *bio;

    pjsip_rx_data    rdata;
    pj_bool_t	     quitting;
    pj_thread_t	    *thread;
};


/*
 * TLS factory callbacks.
 */
static pj_status_t lis_create_transport(pjsip_tpfactory *factory,
					pjsip_tpmgr *mgr,
					pjsip_endpoint *endpt,
					const pj_sockaddr *rem_addr,
					int addr_len,
					pjsip_transport **transport);
static pj_status_t lis_destroy(pjsip_tpfactory *factory);


/*
 * TLS transport callback.
 */
static pj_status_t tls_tp_send_msg(pjsip_transport *transport, 
				   pjsip_tx_data *tdata,
				   const pj_sockaddr_t *rem_addr,
				   int addr_len,
				   void *token,
				   void (*callback)(pjsip_transport *transport,
						    void *token, 
						    pj_ssize_t sent_bytes));
static pj_status_t tls_tp_do_shutdown(pjsip_transport *transport);
static pj_status_t tls_tp_destroy(pjsip_transport *transport);




/*
 * Static vars.
 */
static int tls_init_count;

/* ssl_perror() */
#if 0
#define ssl_perror(level,obj,title)	\
{ \
    unsigned long ssl_err = ERR_get_error(); \
    char errmsg[200]; \
    ERR_error_string_n(ssl_err, errmsg, sizeof(errmsg)); \
    PJ_LOG(level,(obj, "%s: %s", title, errmsg)); \
}
#elif 1
struct err_data
{
    int lvl;
    const char *snd;
    const char *ttl;
};

static int ssl_print_err_cb(const char *str, size_t len, void *u)
{
    struct err_data *e = (struct err_data *)u;
    switch (e->lvl) {
    case 1:
	PJ_LOG(1,(e->snd, "%s: %.*s", e->ttl, len-1, str));
	break;
    case 2:
	PJ_LOG(2,(e->snd, "%s: %.*s", e->ttl, len-1, str));
	break;
    case 3:
	PJ_LOG(3,(e->snd, "%s: %.*s", e->ttl, len-1, str));
	break;
    default:
	PJ_LOG(4,(e->snd, "%s: %.*s", e->ttl, len-1, str));
	break;
    }
    return len;
}

static void ssl_perror(int level, const char *sender, const char *title)
{
    struct err_data e;
    e.lvl = level; e.snd = sender; e.ttl = title;
    ERR_print_errors_cb(&ssl_print_err_cb, &e);
    ERR_print_errors_fp(stderr);
}
#else
static void ssl_perror(int level, const char *sender, const char *title)
{
    static BIO *bio_err;

    if (!bio_err) {
	bio_err = BIO_new_fp(stderr,BIO_NOCLOSE);
    }
    ERR_print_errors(bio_err);
}

#endif


/* Initialize OpenSSL */
static pj_status_t init_openssl(void)
{
    if (++tls_init_count != 1)
	return PJ_SUCCESS;

    SSL_library_init();
    SSL_load_error_strings();

    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();

    return PJ_SUCCESS;
}

/* Shutdown OpenSSL */
static void shutdown_openssl(void)
{
    if (--tls_init_count != 0)
	return;
}

/* SSL password callback. */
static int password_cb(char *buf, int num, int rwflag, void *user_data)
{
    struct tls_listener *lis = user_data;

    PJ_UNUSED_ARG(rwflag);

    if(num < lis->password.slen+1)
	return 0;
    
    pj_memcpy(buf, lis->password.ptr, lis->password.slen);
    return lis->password.slen;
}


/* Create and initialize new SSL context */
static pj_status_t initialize_ctx(struct tls_listener *lis,
				  const char *keyfile, 
				  const char *ca_list_file,
				  SSL_CTX **p_ctx)
{
    SSL_METHOD *meth;
    SSL_CTX *ctx;
        
    /* Create SSL context*/
    meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);
    
    /* Load our keys and certificates */
    if(!(SSL_CTX_use_certificate_chain_file(ctx, keyfile))) {
	ssl_perror(2, lis->base.obj_name, 
		   "Error loading keys and certificate file");
	SSL_CTX_free(ctx);
	return PJSIP_TLS_EKEYFILE;
    }

    /* Set password callback */
    SSL_CTX_set_default_passwd_cb(ctx, password_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ctx, lis);

    if(!(SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM))) {
	ssl_perror(2, lis->base.obj_name, "Error loading private key file");
	SSL_CTX_free(ctx);
	return PJSIP_TLS_EKEYFILE;
    }
    
    /* Load the CAs we trust*/
    if(!(SSL_CTX_load_verify_locations(ctx, ca_list_file, 0))) {
	ssl_perror(2, lis->base.obj_name, 
		   "Error loading/verifying CA list file");
	SSL_CTX_free(ctx);
	return PJSIP_TLS_ECALIST;
    }

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    SSL_CTX_set_verify_depth(ctx,1);
#endif
    
    *p_ctx = ctx;
    return PJ_SUCCESS;
}

/* Destroy SSL context */
static void destroy_ctx(SSL_CTX *ctx)
{
    SSL_CTX_free(ctx);
}


/* Check that the common name matches the host name*/
#if 0
static void check_cert(SSL *ssl, char *host)
{
    X509 *peer;
    char peer_CN[256];
    
    if(SSL_get_verify_result(ssl)!=X509_V_OK)
	berr_exit("Certificate doesn't verify");
    
    /* Check the cert chain. The chain length is automatically checked 
     * by OpenSSL when we set the verify depth in the ctx 
     */
    
    /* Check the common name */
    peer = SSL_get_peer_certificate(ssl);
    X509_NAME_get_text_by_NID( X509_get_subject_name(peer),
			       NID_commonName, peer_CN, 256);

    if(strcasecmp(peer_CN,host)) {
	err_exit("Common name doesn't match host name");
    }
}
#endif



/*
 * Public function to create TLS listener.
 */
PJ_DEF(pj_status_t) pjsip_tls_transport_start(pjsip_endpoint *endpt,
					      const pj_str_t *prm_keyfile,
					      const pj_str_t *prm_password,
					      const pj_str_t *prm_ca_list_file,
					      const pj_sockaddr_in *local,
					      const pjsip_host_port *a_name,
					      unsigned async_cnt,
					      pjsip_tpfactory **p_factory)
{
    struct tls_listener *lis = NULL;
    pj_pool_t *pool = NULL;
    char str_keyfile[256], *keyfile;
    char str_ca_list_file[256], *ca_list_file;
    char str_password[128], *password;
    pj_status_t status;

    PJ_LOG(5,(THIS_FILE, "Creating TLS listener"));

    /* Sanity check */
    PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

    /* Unused arguments */
    PJ_UNUSED_ARG(async_cnt);

#define COPY_STRING(dstbuf, dst, src)	\
    if (src) {	\
	PJ_ASSERT_RETURN(src->slen < sizeof(dstbuf), PJ_ENAMETOOLONG); \
	pj_memcpy(dstbuf, src->ptr, src->slen); \
	dstbuf[src->slen] = '\0'; \
	dst = dstbuf; \
    } else { \
	dst = NULL; \
    }

    /* Copy strings */
    COPY_STRING(str_keyfile, keyfile, prm_keyfile);
    COPY_STRING(str_ca_list_file, ca_list_file, prm_ca_list_file);
    COPY_STRING(str_password, password, prm_password);

    /* Verify that address given in a_name (if any) is valid */
    if (a_name && a_name->host.slen) {
	pj_sockaddr_in tmp;

	status = pj_sockaddr_in_init(&tmp, &a_name->host, 
				     (pj_uint16_t)a_name->port);
	if (status != PJ_SUCCESS || tmp.sin_addr.s_addr == PJ_INADDR_ANY ||
	    tmp.sin_addr.s_addr == PJ_INADDR_NONE)
	{
	    /* Invalid address */
	    return PJ_EINVAL;
	}
    }

    /* Initialize OpenSSL */
    status = init_openssl();
    if (status != PJ_SUCCESS)
	return status;


    /* Create the listener struct. */
    pool = pjsip_endpt_create_pool(endpt, "tlslis", 4000, 4000);
    lis = pj_pool_zalloc(pool, sizeof(*lis));
    lis->base.pool = pool;

    /* Save password */
    pj_strdup2_with_null(pool, &lis->password, password);


    /* Create OpenSSL context */
    status = initialize_ctx(lis, keyfile, ca_list_file, &lis->ctx);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Initialize listener. */
    pj_ansi_snprintf(lis->base.obj_name, sizeof(lis->base.obj_name), 
		     "%s", "tlslis");
    pj_lock_create_recursive_mutex(pool, "tlslis", &lis->base.lock);
    lis->base.type = PJSIP_TRANSPORT_TLS;
    lis->base.type_name = "tls";
    lis->base.flag = pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_TLS);
    lis->base.create_transport = &lis_create_transport;
    lis->base.destroy = &lis_destroy;

    /* Keep endpoint and transport manager instance */
    lis->endpt = endpt;
    lis->tpmgr = pjsip_endpt_get_tpmgr(endpt);

    /* Determine the exported name */
    if (a_name) {
	pj_strdup(pool, &lis->base.addr_name.host, &a_name->host);
	lis->base.addr_name.port = a_name->port;
    } else {
	pj_in_addr ip_addr;
	const char *str_ip_addr;

	/* Get default IP interface for the host */
	status = pj_gethostip(&ip_addr);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Set publicized host */
	str_ip_addr = pj_inet_ntoa(ip_addr);
	pj_strdup2(pool, &lis->base.addr_name.host, str_ip_addr);

	/* Set publicized port */
	if (local) {
	    lis->base.addr_name.port = pj_ntohs(local->sin_port);
	} else {
	    lis->base.addr_name.port = 
		pjsip_transport_get_default_port_for_type(PJSIP_TRANSPORT_TLS);
	}
    }

#if 0
    if (local) {
	pj_memcpy(&lis->base.local_addr, local, sizeof(pj_sockaddr_in));
	pj_strdup2(pool, &lis->base.addr_name.host, 
		   pj_inet_ntoa(((pj_sockaddr_in*)local)->sin_addr));
	lis->base.addr_name.port = pj_ntohs(((pj_sockaddr_in*)local)->sin_port);
    } else {
	int port;
	port = pjsip_transport_get_default_port_for_type(PJSIP_TRANSPORT_TLS);
	pj_sockaddr_in_init(&lis->base.local_addr, NULL, port);
	pj_strdup(pool, &lis->base.addr_name.host, pj_gethostname());
	lis->base.addr_name.port = port;
    }
#endif


    /* Register listener to transport manager. */
    status = pjsip_tpmgr_register_tpfactory(lis->tpmgr, &lis->base);
    if (status != PJ_SUCCESS)
	goto on_error;

    lis->is_registered = PJ_TRUE;


    /* Done */
    if (p_factory)
	*p_factory = &lis->base;

    PJ_LOG(4,(lis->base.obj_name, "TLS listener started at %.*s;%d",
	      (int)lis->base.addr_name.host.slen,
	      lis->base.addr_name.host.ptr,
	      lis->base.addr_name.port));
    return PJ_SUCCESS;

on_error:
    if (lis) {
	lis_destroy(&lis->base);
    } else if (pool) {
	pj_pool_release(pool);
	shutdown_openssl();
    } else {
	shutdown_openssl();
    }

    return status;
}


/* Transport worker thread */
static int PJ_THREAD_FUNC tls_worker_thread(void *arg)
{
    struct tls_transport *tls_tp = (struct tls_transport *)arg;
    pjsip_rx_data *rdata = &tls_tp->rdata;

    while (!tls_tp->quitting) {
	int len;
	pj_size_t size_eaten;

	/* Start blocking read to SSL socket */
	len = SSL_read(tls_tp->ssl, 
		       rdata->pkt_info.packet + rdata->pkt_info.len,
		       sizeof(rdata->pkt_info.packet) - rdata->pkt_info.len);


	switch (SSL_get_error(tls_tp->ssl, len)) {
        case SSL_ERROR_NONE:
	    rdata->pkt_info.len += len;
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
	    if (size_eaten>0 && size_eaten<(pj_size_t)rdata->pkt_info.len) {
		pj_memmove(rdata->pkt_info.packet,
			   rdata->pkt_info.packet + size_eaten,
			   rdata->pkt_info.len - size_eaten);
	    }
	    
	    rdata->pkt_info.len -= size_eaten;
	    break;

        case SSL_ERROR_ZERO_RETURN:
	    pjsip_transport_shutdown(&tls_tp->base);
	    goto done;

        case SSL_ERROR_SYSCALL:
	    PJ_LOG(2,(tls_tp->base.obj_name, "SSL Error: Premature close"));
	    pjsip_transport_shutdown(&tls_tp->base);
	    goto done;

        default:
	    PJ_LOG(2,(tls_tp->base.obj_name, "SSL read problem"));
	    pjsip_transport_shutdown(&tls_tp->base);
	    goto done;
	}

    }

done:
    return 0;
}


/*
 * Create a new TLS transport. The TLS role can be a server or a client,
 * depending on whether socket is valid.
 */
static pj_status_t tls_create_transport(struct tls_listener *lis,
				        pj_sock_t sock,
				        const pj_sockaddr_in *rem_addr,
				        struct tls_transport **p_tp)
{
    struct tls_transport *tls_tp = NULL;
    pj_pool_t *pool = NULL;
    char dst_str[80];
    int len;
    pj_status_t status;

    /* Build remote address */
    PJ_ASSERT_RETURN(rem_addr->sin_family==PJ_AF_INET, PJ_EINVAL);

    /* sock must not be zero (should be either a valid socket or
     * PJ_INVALID_SOCKET.
     */
    PJ_ASSERT_RETURN(sock==PJ_INVALID_SOCKET || sock > 0, PJ_EINVAL);

    /* 
     * Create the transport 
     */
    pool = pjsip_endpt_create_pool(lis->endpt, "tls", 4000, 4000);
    tls_tp = pj_pool_zalloc(pool, sizeof(*tls_tp));
    tls_tp->sock = sock;
    tls_tp->base.pool = pool;

    len = pj_ansi_snprintf(tls_tp->base.obj_name, 
			   sizeof(tls_tp->base.obj_name),
			   "tls%p", tls_tp);
    if (len < 1 || len >= sizeof(tls_tp->base.obj_name)) {
	status = PJ_ENAMETOOLONG;
	goto on_error;
    }

    /* Print destination address. */
    len = pj_ansi_snprintf(dst_str, sizeof(dst_str), "%s:%d",
			   pj_inet_ntoa(rem_addr->sin_addr), 
			   pj_ntohs(rem_addr->sin_port));

    PJ_LOG(5,(lis->base.obj_name, "Creating TLS transport to %s", dst_str));


    /* Transport info */
    tls_tp->base.endpt = lis->endpt;
    tls_tp->base.tpmgr = lis->tpmgr;
    tls_tp->base.type_name = (char*)pjsip_transport_get_type_name(PJSIP_TRANSPORT_TLS);
    tls_tp->base.flag = pjsip_transport_get_flag_from_type(PJSIP_TRANSPORT_TLS);
    tls_tp->base.info = pj_pool_alloc(pool, len + 5);
    pj_ansi_snprintf(tls_tp->base.info, len + 5, "TLS:%s", dst_str);

    /* Reference counter */
    status = pj_atomic_create(pool, 0, &tls_tp->base.ref_cnt);
    if (status != PJ_SUCCESS)
	goto on_error;
    
    /* Lock */
    status = pj_lock_create_recursive_mutex(pool, "tls", &tls_tp->base.lock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Transport key */
    tls_tp->base.key.type = PJSIP_TRANSPORT_TLS;
    pj_memcpy(&tls_tp->base.key.rem_addr, rem_addr, sizeof(*rem_addr));

    pj_strdup(pool, &tls_tp->base.local_name.host, &lis->base.addr_name.host);
    tls_tp->base.local_name.port = lis->base.addr_name.port;

    pj_strdup2(pool, &tls_tp->base.remote_name.host, dst_str);
    tls_tp->base.remote_name.port = pj_ntohs(rem_addr->sin_port);

    /* Initialize transport callback */
    tls_tp->base.send_msg = &tls_tp_send_msg;
    tls_tp->base.do_shutdown = &tls_tp_do_shutdown;
    tls_tp->base.destroy = &tls_tp_destroy;


    /* Connect SSL */
    if (sock == PJ_INVALID_SOCKET) {

	/* Create socket */
	status = pj_sock_socket(PJ_AF_INET, PJ_SOCK_STREAM, 0, &sock);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Save the socket */
	tls_tp->sock = sock;

	/* TODO: asynchronous connect() */
	PJ_TODO(TLS_ASYNC_CONNECT);

	/* Connect socket */
	status = pj_sock_connect(sock, rem_addr, sizeof(*rem_addr));
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Create SSL object and BIO */
	tls_tp->ssl = SSL_new(lis->ctx);
	tls_tp->bio = BIO_new_socket(sock, BIO_NOCLOSE);
	SSL_set_bio(tls_tp->ssl, tls_tp->bio, tls_tp->bio);

	/* Connect SSL */
	if (SSL_connect(tls_tp->ssl) <= 0) {
	    ssl_perror(4, tls_tp->base.obj_name, "SSL_connect() error");
	    status = PJSIP_TLS_ECONNECT;
	    goto on_error;
	}

	/* TODO: check server cert. */
	PJ_TODO(TLS_CHECK_SERVER_CERT);
#if 0
	check_cert(ssl,host);
#endif

    } else {
	/*
	 * This is a server side TLS socket.
	 */
	PJ_TODO(TLS_IMPLEMENT_SERVER);
	status = PJ_ENOTSUP;
	goto on_error;
    }

    /* Initialize local address */
    status = pj_sock_getsockname(tls_tp->sock, &tls_tp->base.local_addr,
				 &tls_tp->base.addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;


    /* 
     * Create rdata 
     */
    pool = pjsip_endpt_create_pool(lis->endpt,
				   "rtd%p",
				   PJSIP_POOL_RDATA_LEN,
				   PJSIP_POOL_RDATA_INC);
    if (!pool) {
	status = PJ_ENOMEM;
	goto on_error;
    }
    tls_tp->rdata.tp_info.pool = pool;

    /*
     * Initialize rdata
     */
    tls_tp->rdata.tp_info.transport = &tls_tp->base;
    tls_tp->rdata.tp_info.tp_data = tls_tp;
    tls_tp->rdata.tp_info.op_key.rdata = &tls_tp->rdata;
    pj_ioqueue_op_key_init(&tls_tp->rdata.tp_info.op_key.op_key, 
			   sizeof(pj_ioqueue_op_key_t));

    tls_tp->rdata.pkt_info.src_addr = tls_tp->base.key.rem_addr;
    tls_tp->rdata.pkt_info.src_addr_len = sizeof(pj_sockaddr_in);
    rem_addr = (pj_sockaddr_in*) &tls_tp->base.key.rem_addr;
    pj_ansi_strcpy(tls_tp->rdata.pkt_info.src_name,
		   pj_inet_ntoa(rem_addr->sin_addr));
    tls_tp->rdata.pkt_info.src_port = pj_ntohs(rem_addr->sin_port);

    /* Create worker thread to receive packets */
    status = pj_thread_create(pool, "tlsthread", &tls_worker_thread,
			      tls_tp, PJ_THREAD_DEFAULT_STACK_SIZE, 0, 
			      &tls_tp->thread);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Done */
    *p_tp = tls_tp;

    PJ_LOG(4,(tls_tp->base.obj_name, "TLS transport created, remote=%s", 
	      dst_str));

    return PJ_SUCCESS;

on_error:
    if (tls_tp)
	tls_tp_destroy(&tls_tp->base);
    else if (pool) {
	pj_pool_release(pool);
	if (sock != PJ_INVALID_SOCKET) pj_sock_close(sock);
    }
    return status;
}


/*
 * Callback from transport manager to create a new (outbound) TLS transport.
 */
static pj_status_t lis_create_transport(pjsip_tpfactory *factory,
					pjsip_tpmgr *mgr,
					pjsip_endpoint *endpt,
					const pj_sockaddr *rem_addr,
					int addr_len,
					pjsip_transport **transport)
{
    pj_status_t status;
    struct tls_transport *tls_tp;

    /* Check address */
    PJ_ASSERT_RETURN(rem_addr->sa_family == PJ_AF_INET &&
		     addr_len == sizeof(pj_sockaddr_in), PJ_EINVAL);


    PJ_UNUSED_ARG(mgr);
    PJ_UNUSED_ARG(endpt);
    /* addr_len is not used on Release build */
    PJ_UNUSED_ARG(addr_len);

    /* Create TLS transport */
    status = tls_create_transport((struct tls_listener*)factory, 
				  PJ_INVALID_SOCKET,
				  (const pj_sockaddr_in*)rem_addr,
				  &tls_tp);
    if (status != PJ_SUCCESS)
	return status;

    /* Done */
    *transport = &tls_tp->base;
    return PJ_SUCCESS;
}

/*
 * Callback from transport manager to destroy TLS listener.
 */
static pj_status_t lis_destroy(pjsip_tpfactory *factory)
{
    struct tls_listener *lis = (struct tls_listener *) factory;

    PJ_LOG(4,(factory->obj_name, "TLS listener shutting down.."));

    if (lis->is_registered) {
	pjsip_tpmgr_unregister_tpfactory(lis->tpmgr, &lis->base);
	lis->is_registered = PJ_FALSE;
    }

    if (lis->base.lock) {
	pj_lock_destroy(lis->base.lock);
	lis->base.lock = NULL;
    }

    if (lis->ctx) {
	destroy_ctx(lis->ctx);
	lis->ctx = NULL;
    }

    if (lis->base.pool) {
	pj_pool_t *pool = lis->base.pool;
	lis->base.pool = NULL;
	pj_pool_release(pool);
    }

    /* Shutdown OpenSSL */
    shutdown_openssl();

    return PJ_SUCCESS;
}


/*
 * Function to be called by transport manager to send SIP message.
 */
static pj_status_t tls_tp_send_msg(pjsip_transport *transport, 
				   pjsip_tx_data *tdata,
				   const pj_sockaddr_t *rem_addr,
				   int addr_len,
				   void *token,
				   void (*callback)(pjsip_transport *transport,
						    void *token, 
						    pj_ssize_t sent_bytes))
{
    struct tls_transport *tls_tp = (struct tls_transport*) transport;

    /* This is a connection oriented protocol, so rem_addr is not used */
    PJ_UNUSED_ARG(rem_addr);
    PJ_UNUSED_ARG(addr_len);

    /* Write to TLS */
    if (BIO_write(tls_tp->bio, tdata->buf.start	, 
		  tdata->buf.cur - tdata->buf.start) <= 0)
    {
	if(! BIO_should_retry(tls_tp->bio)) {
	    ssl_perror(4, transport->obj_name, "SSL send error");
	    return PJSIP_TLS_ESEND;
	}

	/* Do something to handle the retry */
    }

    /* Data written immediately, no need to call callback */
    PJ_UNUSED_ARG(callback);
    PJ_UNUSED_ARG(token);

    return PJ_SUCCESS;
}


/*
 * Instruct the transport to initiate graceful shutdown procedure.
 */
static pj_status_t tls_tp_do_shutdown(pjsip_transport *transport)
{
    /* Nothing to do for TLS */
    PJ_UNUSED_ARG(transport);
    return PJ_SUCCESS;
}

/*
 * Forcefully destroy this transport.
 */
static pj_status_t tls_tp_destroy(pjsip_transport *transport)
{
    struct tls_transport *tls_tp = (struct tls_transport*) transport;

    if (tls_tp->thread) {
	tls_tp->quitting = PJ_TRUE;
	SSL_shutdown(tls_tp->ssl);

	pj_thread_join(tls_tp->thread);
	pj_thread_destroy(tls_tp->thread);
	tls_tp->thread = NULL;
    }

    if (tls_tp->ssl) {
	int rc;
	rc = SSL_shutdown(tls_tp->ssl);
	if (rc == 0) {
	    pj_sock_shutdown(tls_tp->sock, PJ_SD_BOTH);
	    SSL_shutdown(tls_tp->ssl);
	}

	SSL_free(tls_tp->ssl);
	tls_tp->ssl = NULL;
	tls_tp->bio = NULL;
	tls_tp->sock = PJ_INVALID_SOCKET;

    } else if (tls_tp->sock != PJ_INVALID_SOCKET) {
	pj_sock_close(tls_tp->sock);
	tls_tp->sock = PJ_INVALID_SOCKET;
    }

    if (tls_tp->base.lock) {
	pj_lock_destroy(tls_tp->base.lock);
	tls_tp->base.lock = NULL;
    }

    if (tls_tp->base.ref_cnt) {
	pj_atomic_destroy(tls_tp->base.ref_cnt);
	tls_tp->base.ref_cnt = NULL;
    }

    if (tls_tp->base.pool) {
	pj_pool_t *pool = tls_tp->base.pool;
	tls_tp->base.pool = NULL;
	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}


#endif	/* PJSIP_HAS_TLS_TRANSPORT */

