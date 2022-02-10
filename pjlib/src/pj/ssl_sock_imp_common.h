/* 
 * Copyright (C) 2019-2019 Teluu Inc. (http://www.teluu.com)
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

#ifndef __SSL_SOCK_IMP_COMMON_H__
#define __SSL_SOCK_IMP_COMMON_H__

#include <pj/activesock.h>
#include <pj/timer.h>

/*
 * SSL/TLS state enumeration.
 */
enum ssl_state {
    SSL_STATE_NULL,
    SSL_STATE_HANDSHAKING,
    SSL_STATE_ESTABLISHED,
    SSL_STATE_ERROR
};

/*
 * Internal timer types.
 */
enum timer_id
{
    TIMER_NONE,
    TIMER_HANDSHAKE_TIMEOUT,
    TIMER_CLOSE
};

/*
 * Structure of SSL socket read buffer.
 */
typedef struct read_data_t
{
    void		 *data;
    pj_size_t		  len;
} read_data_t;

/*
 * Structure of SSL socket write data.
 */
typedef struct write_data_t {
    PJ_DECL_LIST_MEMBER(struct write_data_t);
    pj_ioqueue_op_key_t	 key;
    pj_size_t 	 	 record_len;
    pj_ioqueue_op_key_t	*app_key;
    pj_size_t 	 	 plain_data_len;
    pj_size_t 	 	 data_len;
    unsigned		 flags;
    union {
	char		 content[1];
	const char	*ptr;
    } data;
} write_data_t;

/*
 * Structure of SSL socket write buffer (circular buffer).
 */
typedef struct send_buf_t {
    char		*buf;
    pj_size_t		 max_len;    
    char		*start;
    pj_size_t		 len;
} send_buf_t;

/* Circular buffer object */
typedef struct circ_buf_t {
    pj_ssl_sock_t *owner;  /* owner of the circular buffer */
    pj_size_t      cap;    /* maximum number of elements (must be power of 2) */
    pj_size_t      readp;  /* index of oldest element */
    pj_size_t      writep; /* index at which to write new element  */
    pj_size_t      size;   /* number of elements */
    pj_uint8_t    *buf;    /* data buffer */
    pj_pool_t     *pool;   /* where new allocations will take place */
} circ_buf_t;

/*
 * Secure socket structure definition.
 */
struct pj_ssl_sock_t
{
    pj_pool_t		 *pool;
    pj_pool_t		 *info_pool; /* this is for certificate chain 
				      * information allocation. Don't use for 
				      * other purposes. */
    pj_ssl_sock_t	 *parent;
    pj_ssl_sock_param	  param;
    pj_ssl_sock_param	  newsock_param;
    pj_ssl_cert_t	 *cert;
    
    pj_ssl_cert_info	  local_cert_info;
    pj_ssl_cert_info	  remote_cert_info;

    pj_bool_t		  is_server;
    enum ssl_state	  ssl_state;
    pj_ioqueue_op_key_t	  handshake_op_key;
    pj_ioqueue_op_key_t	  shutdown_op_key;
    pj_timer_entry	  timer;
    pj_status_t		  verify_status;

    pj_bool_t		  is_closing;
    unsigned long	  last_err;

    pj_sock_t		  sock;
    pj_activesock_t	 *asock;

    pj_sockaddr		  local_addr;
    pj_sockaddr		  rem_addr;
    int			  addr_len;
    
    pj_bool_t		  read_started;
    pj_size_t		  read_size;
    pj_uint32_t		  read_flags;
    void		**asock_rbuf;
    read_data_t		 *ssock_rbuf;

    write_data_t	  write_pending;/* list of pending write to ssl */
    write_data_t	  write_pending_empty; /* cache for write_pending   */
    pj_bool_t		  flushing_write_pend; /* flag of flushing is ongoing*/
    send_buf_t		  send_buf;
    write_data_t 	  send_buf_pending; /* send buffer is full but some
					     * data is queuing in wbio.     */
    write_data_t	  send_pending;	/* list of pending write to network */
    pj_lock_t		 *write_mutex;	/* protect write BIO and send_buf   */

    circ_buf_t            circ_buf_input;
    pj_lock_t            *circ_buf_input_mutex;

    circ_buf_t            circ_buf_output;
    pj_lock_t            *circ_buf_output_mutex;
};


/*
 * Certificate/credential structure definition.
 */
struct pj_ssl_cert_t
{
    pj_str_t CA_file;
    pj_str_t CA_path;
    pj_str_t cert_file;
    pj_str_t privkey_file;
    pj_str_t privkey_pass;

    /* Certificate buffer. */
    pj_ssl_cert_buffer CA_buf;
    pj_ssl_cert_buffer cert_buf;
    pj_ssl_cert_buffer privkey_buf;
};

/* ssl available ciphers */
static unsigned ssl_cipher_num;
static struct ssl_ciphers_t {
    pj_ssl_cipher    id;
    const char	    *name;
} ssl_ciphers[PJ_SSL_SOCK_MAX_CIPHERS];

/* ssl available curves */
static unsigned ssl_curves_num;
static struct ssl_curves_t {
    pj_ssl_curve    id;
    const char	    *name;
} ssl_curves[PJ_SSL_SOCK_MAX_CURVES];

/*
 *******************************************************************
 * I/O functions.
 *******************************************************************
 */

static pj_bool_t io_empty(pj_ssl_sock_t *ssock, circ_buf_t *cb);
static pj_size_t io_size(pj_ssl_sock_t *ssock, circ_buf_t *cb);
static void io_read(pj_ssl_sock_t *ssock, circ_buf_t *cb,
		    pj_uint8_t *dst, pj_size_t len);
static pj_status_t io_write(pj_ssl_sock_t *ssock, circ_buf_t *cb,
                            const pj_uint8_t *src, pj_size_t len);

static write_data_t* alloc_send_data(pj_ssl_sock_t *ssock, pj_size_t len);
static void free_send_data(pj_ssl_sock_t *ssock, write_data_t *wdata);
static pj_status_t flush_delayed_send(pj_ssl_sock_t *ssock);

#ifdef SSL_SOCK_IMP_USE_CIRC_BUF
/*
 *******************************************************************
 * Circular buffer functions.
 *******************************************************************
 */

static pj_status_t circ_init(pj_pool_factory *factory,
                             circ_buf_t *cb, pj_size_t cap);
static void circ_deinit(circ_buf_t *cb);
static pj_bool_t circ_empty(const circ_buf_t *cb);
static pj_size_t circ_size(const circ_buf_t *cb);
static void circ_read(circ_buf_t *cb, pj_uint8_t *dst, pj_size_t len);
static pj_status_t circ_write(circ_buf_t *cb,
                              const pj_uint8_t *src, pj_size_t len);

inline static pj_bool_t io_empty(pj_ssl_sock_t *ssock, circ_buf_t *cb)
{
    return circ_empty(cb);
}
inline static pj_size_t io_size(pj_ssl_sock_t *ssock, circ_buf_t *cb)
{
    return circ_size(cb);
}
inline static void io_reset(pj_ssl_sock_t *ssock, circ_buf_t *cb) {}
inline static void io_read(pj_ssl_sock_t *ssock, circ_buf_t *cb,
		    	   pj_uint8_t *dst, pj_size_t len)
{
    return circ_read(cb, dst, len);
}
inline static pj_status_t io_write(pj_ssl_sock_t *ssock, circ_buf_t *cb,
                            const pj_uint8_t *src, pj_size_t len)
{
    return circ_write(cb, src, len);
}

#endif


/*
 *******************************************************************
 * The below functions must be implemented by SSL backend.
 *******************************************************************
 */

/* Allocate SSL backend struct */
static pj_ssl_sock_t *ssl_alloc(pj_pool_t *pool);
/* Create and initialize new SSL context and instance */
static pj_status_t ssl_create(pj_ssl_sock_t *ssock);
/* Destroy SSL context and instance */
static void ssl_destroy(pj_ssl_sock_t *ssock);
/* Reset SSL socket state */
static void ssl_reset_sock_state(pj_ssl_sock_t *ssock);

/* Ciphers and certs */
static void ssl_ciphers_populate();
static pj_ssl_cipher ssl_get_cipher(pj_ssl_sock_t *ssock);
static void ssl_update_certs_info(pj_ssl_sock_t *ssock);

/* SSL session functions */
static void ssl_set_state(pj_ssl_sock_t *ssock, pj_bool_t is_server);
static void ssl_set_peer_name(pj_ssl_sock_t *ssock);

static pj_status_t ssl_do_handshake(pj_ssl_sock_t *ssock);
static pj_status_t ssl_renegotiate(pj_ssl_sock_t *ssock);
static pj_status_t ssl_read(pj_ssl_sock_t *ssock, void *data, int *size);
static pj_status_t ssl_write(pj_ssl_sock_t *ssock, const void *data,
			     pj_ssize_t size, int *nwritten);

#ifdef SSL_SOCK_IMP_USE_OWN_NETWORK

static void ssl_close_sockets(pj_ssl_sock_t *ssock);

static pj_status_t network_send(pj_ssl_sock_t *ssock,
				pj_ioqueue_op_key_t *send_key,
				const void *data,
				pj_ssize_t *size,
				unsigned flags);
static pj_status_t network_start_read(pj_ssl_sock_t *ssock,
				      unsigned async_count,
				      unsigned buff_size,
				      void *readbuf[],
				      pj_uint32_t flags);
static pj_status_t network_start_accept(pj_ssl_sock_t *ssock,
			 		pj_pool_t *pool,
			  		const pj_sockaddr_t *localaddr,
			  		int addr_len,
			  		const pj_ssl_sock_param *newsock_param);
static pj_status_t network_start_connect(pj_ssl_sock_t *ssock,
		       pj_ssl_start_connect_param *connect_param);
static pj_status_t network_setup_connection(pj_ssl_sock_t *ssock,
					    void *connection);
static pj_status_t network_get_localaddr(pj_ssl_sock_t *ssock,
					 pj_sockaddr_t *addr,
					 int *namelen);

#endif

#endif /* __SSL_SOCK_IMP_COMMON_H__ */
