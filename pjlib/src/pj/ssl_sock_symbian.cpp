/* $Id$ */
/* 
 * Copyright (C) 2009 Teluu Inc. (http://www.teluu.com)
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
#include <pj/ssl_sock.h>
#include <pj/compat/socket.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/string.h>

#include "os_symbian.h"
#include <securesocket.h>
#include <x509cert.h>
#include <e32des8.h>

#define THIS_FILE "ssl_sock_symbian.cpp"

typedef void (*CPjSSLSocket_cb)(int err, void *key);

class CPjSSLSocketReader : public CActive
{
public:
    static CPjSSLSocketReader *NewL(CSecureSocket &sock) 
    {
	CPjSSLSocketReader *self = new (ELeave) 
				   CPjSSLSocketReader(sock);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
    }

    ~CPjSSLSocketReader() {
	Cancel();
    }

    /* Asynchronous read from the socket. */
    int Read(CPjSSLSocket_cb cb, void *key, TPtr8 &data, TUint flags)
    {
	PJ_ASSERT_RETURN(!IsActive(), PJ_EBUSY);
	
	cb_ = cb;
	key_ = key;
	sock_.RecvOneOrMore(data, iStatus, len_received_);
	SetActive();
	
	return PJ_EPENDING;
    }

private:
    CSecureSocket  	&sock_;
    CPjSSLSocket_cb	 cb_;
    void		*key_;
    TSockXfrLength  	 len_received_; /* not really useful? */

    void DoCancel() {
	sock_.CancelAll();
    }
    
    void RunL() {
	(*cb_)(iStatus.Int(), key_);
    }

    CPjSSLSocketReader(CSecureSocket &sock) : 
	CActive(0), sock_(sock), cb_(NULL), key_(NULL) 
    {}
    
    void ConstructL() {
	CActiveScheduler::Add(this);
    }
};

class CPjSSLSocket : public CActive
{
public:
    enum ssl_state {
	SSL_STATE_NULL,
	SSL_STATE_CONNECTING,
	SSL_STATE_HANDSHAKING,
	SSL_STATE_ESTABLISHED
    };
    
    static CPjSSLSocket *NewL(const TDesC8 &ssl_proto) {
	CPjSSLSocket *self = new (ELeave) CPjSSLSocket();
	CleanupStack::PushL(self);
	self->ConstructL(ssl_proto);
	CleanupStack::Pop(self);
	return self;
    }

    ~CPjSSLSocket() {
	Cancel();
	CleanupSubObjects();
    }

    int Connect(CPjSSLSocket_cb cb, void *key, const TInetAddr &local_addr, 
		const TInetAddr &rem_addr, 
		const TDesC8 &servername = TPtrC8(NULL,0));
    int Send(CPjSSLSocket_cb cb, void *key, const TDesC8 &aDesc, TUint flags);
    int SendSync(const TDesC8 &aDesc, TUint flags);

    CPjSSLSocketReader* GetReader();
    enum ssl_state GetState() const { return state_; }
    const TInetAddr* GetLocalAddr() const { return &local_addr_; }
    int GetCipher(TDes8 &cipher) const {
	if (securesock_)
	    return securesock_->CurrentCipherSuite(cipher);
	return KErrNotFound;
    }

private:
    enum ssl_state	 state_;
    pj_sock_t	    	 sock_;
    CSecureSocket  	*securesock_;
    bool	    	 is_connected_;
    CPjSSLSocketReader  *reader_;
    TBuf<32> 	    	 ssl_proto_;
    TInetAddr       	 rem_addr_;
    TPtrC8		 servername_;
    TInetAddr       	 local_addr_;
    TSockXfrLength 	 sent_len_;

    CPjSSLSocket_cb 	 cb_;
    void 	   	*key_;
    
    void DoCancel();
    void RunL();

    CPjSSLSocket() :
	CActive(0), state_(SSL_STATE_NULL), sock_(PJ_INVALID_SOCKET), 
	securesock_(NULL), 
	is_connected_(false), reader_(NULL),
	cb_(NULL), key_(NULL)
    {}
    
    void ConstructL(const TDesC8 &ssl_proto) {
	ssl_proto_.Copy(ssl_proto);
	CActiveScheduler::Add(this);
    }

    void CleanupSubObjects() {
	delete reader_;
	reader_ = NULL;
	if (securesock_) {
	    securesock_->Close();
	    delete securesock_;
	    securesock_ = NULL;
	}
	if (sock_ != PJ_INVALID_SOCKET) {
	    delete (CPjSocket*)sock_;
	    sock_ = PJ_INVALID_SOCKET;
	}	    
    }
};

int CPjSSLSocket::Connect(CPjSSLSocket_cb cb, void *key, 
			  const TInetAddr &local_addr, 
			  const TInetAddr &rem_addr,
			  const TDesC8 &servername)
{
    pj_status_t status;
    
    PJ_ASSERT_RETURN(state_ == SSL_STATE_NULL, PJ_EINVALIDOP);
    
    status = pj_sock_socket(rem_addr.Family(), pj_SOCK_STREAM(), 0, &sock_);
    if (status != PJ_SUCCESS)
	return status;

    RSocket &rSock = ((CPjSocket*)sock_)->Socket();

    local_addr_ = local_addr;
    
    if (!local_addr_.IsUnspecified()) {
	TInt err = rSock.Bind(local_addr_);
	if (err != KErrNone)
	    return PJ_RETURN_OS_ERROR(err);
    }
    
    cb_ = cb;
    key_ = key;
    rem_addr_ = rem_addr;
    servername_.Set(servername);
    state_ = SSL_STATE_CONNECTING;

    rSock.Connect(rem_addr_, iStatus);
    SetActive();
    
    rSock.LocalName(local_addr_);

    return PJ_EPENDING;
}

int CPjSSLSocket::Send(CPjSSLSocket_cb cb, void *key, const TDesC8 &aDesc, 
		       TUint flags)
{
    PJ_UNUSED_ARG(flags);

    PJ_ASSERT_RETURN(state_ == SSL_STATE_ESTABLISHED, PJ_EINVALIDOP);
    
    if (IsActive())
	return PJ_EBUSY;
    
    cb_ = cb;
    key_ = key;
    
    securesock_->Send(aDesc, iStatus, sent_len_);
    SetActive();
    
    return PJ_EPENDING;
}

int CPjSSLSocket::SendSync(const TDesC8 &aDesc, TUint flags)
{
    PJ_UNUSED_ARG(flags);

    PJ_ASSERT_RETURN(state_ == SSL_STATE_ESTABLISHED, PJ_EINVALIDOP);
    
    TRequestStatus reqStatus;
    securesock_->Send(aDesc, reqStatus, sent_len_);
    User::WaitForRequest(reqStatus);
    
    return PJ_RETURN_OS_ERROR(reqStatus.Int());
}

CPjSSLSocketReader* CPjSSLSocket::GetReader()
{
    PJ_ASSERT_RETURN(state_ == SSL_STATE_ESTABLISHED, NULL);
    
    if (reader_)
	return reader_;
    
    TRAPD(err,	reader_ = CPjSSLSocketReader::NewL(*securesock_));
    if (err != KErrNone)
	return NULL;
    
    return reader_;
}

void CPjSSLSocket::DoCancel()
{
    /* Operation to be cancelled depends on current state */
    switch (state_) {
    case SSL_STATE_CONNECTING:
	{
	    RSocket &rSock = ((CPjSocket*)sock_)->Socket();
	    rSock.CancelConnect();
	    
	    CleanupSubObjects();

	    state_ = SSL_STATE_NULL;
	}
	break;
    case SSL_STATE_HANDSHAKING:
	{
	    securesock_->CancelHandshake();
	    securesock_->Close();
	    
	    CleanupSubObjects();
	    
	    state_ = SSL_STATE_NULL;
	}
	break;
    case SSL_STATE_ESTABLISHED:
	securesock_->CancelSend();
	break;
    default:
	break;
    }
}

void CPjSSLSocket::RunL()
{
    switch (state_) {
    case SSL_STATE_CONNECTING:
	if (iStatus != KErrNone) {
	    CleanupSubObjects();
	    state_ = SSL_STATE_NULL;
	    /* Dispatch connect failure notification */
	    if (cb_) (*cb_)(iStatus.Int(), key_);
	} else {
	    RSocket &rSock = ((CPjSocket*)sock_)->Socket();

	    /* Get local addr */
	    rSock.LocalName(local_addr_);
	    
	    /* Prepare and start handshake */
	    securesock_ = CSecureSocket::NewL(rSock, ssl_proto_);
	    securesock_->SetDialogMode(EDialogModeAttended);
	    if (servername_.Length() > 0)
		securesock_->SetOpt(KSoSSLDomainName, KSolInetSSL,
				    servername_);
	    securesock_->FlushSessionCache();
	    securesock_->StartClientHandshake(iStatus);
	    SetActive();
	    state_ = SSL_STATE_HANDSHAKING;
	}
	break;
    case SSL_STATE_HANDSHAKING:
	if (iStatus == KErrNone) {
	    state_ = SSL_STATE_ESTABLISHED;
	} else {
	    state_ = SSL_STATE_NULL;
	    CleanupSubObjects();
	}
	/* Dispatch connect status notification */
	if (cb_) (*cb_)(iStatus.Int(), key_);
	break;
    case SSL_STATE_ESTABLISHED:
	/* Dispatch data sent notification */
	if (cb_) (*cb_)(iStatus.Int(), key_);
	break;
    default:
	pj_assert(0);
	break;
    }
}

typedef void (*CPjTimer_cb)(void *user_data);

class CPjTimer : public CActive 
{
public:
    CPjTimer(const pj_time_val *delay, CPjTimer_cb cb, void *user_data) : 
	CActive(0), cb_(cb), user_data_(user_data)
    {
	CActiveScheduler::Add(this);

	rtimer_.CreateLocal();
	pj_int32_t interval = PJ_TIME_VAL_MSEC(*delay) * 1000;
	if (interval < 0) {
	    interval = 0;
	}
	rtimer_.After(iStatus, interval);
	SetActive();
    }
    
    ~CPjTimer() { Cancel(); }
    
private:	
    RTimer		 rtimer_;
    CPjTimer_cb		 cb_;
    void		*user_data_;
    
    void RunL() { if (cb_) (*cb_)(user_data_); }
    void DoCancel() { rtimer_.Cancel(); }
};

/*
 * Structure of recv/read state.
 */
typedef struct read_state_t {
    TPtr8		*read_buf;
    TPtr8		*orig_buf;
    pj_uint32_t		 flags;    
} read_state_t;

/*
 * Structure of send/write data.
 */
typedef struct write_data_t {
    pj_size_t 	 	 len;
    pj_ioqueue_op_key_t	*key;
    pj_size_t 	 	 data_len;
    char		 data[1];
} write_data_t;

/*
 * Structure of send/write state.
 */
typedef struct write_state_t {
    char		*buf;
    pj_size_t		 max_len;    
    char		*start;
    pj_size_t		 len;
    write_data_t	*current_data;
    TPtrC8		 send_ptr;
} write_state_t;

/*
 * Secure socket structure definition.
 */
struct pj_ssl_sock_t
{
    pj_ssl_sock_cb	 cb;
    void		*user_data;
    
    pj_bool_t		 established;
    write_state_t	 write_state;
    read_state_t	 read_state;
    CPjTimer		*connect_timer;

    CPjSSLSocket   	*sock;
    int			 sock_af;
    int			 sock_type;
    pj_sockaddr		 local_addr;
    pj_sockaddr		 rem_addr;

    pj_ssl_sock_proto	 proto;
    pj_time_val		 timeout;
    pj_str_t		 ciphers;
    pj_str_t		 servername;
};


/*
 * Create SSL socket instance. 
 */
PJ_DEF(pj_status_t) pj_ssl_sock_create (pj_pool_t *pool,
					const pj_ssl_sock_param *param,
					pj_ssl_sock_t **p_ssock)
{
    pj_ssl_sock_t *ssock;

    PJ_ASSERT_RETURN(param->async_cnt == 1, PJ_EINVAL);
    PJ_ASSERT_RETURN(pool && param && p_ssock, PJ_EINVAL);

    /* Allocate secure socket */
    ssock = PJ_POOL_ZALLOC_T(pool, pj_ssl_sock_t);
    
    /* Allocate write buffer */
    ssock->write_state.buf = (char*)pj_pool_alloc(pool, 
						  param->send_buffer_size);
    ssock->write_state.max_len = param->send_buffer_size;
    ssock->write_state.start = ssock->write_state.buf;
    
    /* Init secure socket */
    ssock->sock_af = param->sock_af;
    ssock->sock_type = param->sock_type;
    ssock->cb = param->cb;
    ssock->user_data = param->user_data;
    pj_strdup_with_null(pool, &ssock->ciphers, &param->ciphers);
    pj_strdup_with_null(pool, &ssock->servername, &param->servername);

    /* Finally */
    *p_ssock = ssock;

    return PJ_SUCCESS;
}

/*
 * Set SSL socket credential.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_set_certificate(
					    pj_ssl_sock_t *ssock,
					    pj_pool_t *pool,
					    const pj_ssl_cert_t *cert)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(cert);
    return PJ_ENOTSUP;
}

/*
 * Close the SSL socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_close(pj_ssl_sock_t *ssock)
{
    PJ_ASSERT_RETURN(ssock, PJ_EINVAL);
    
    delete ssock->connect_timer;
    ssock->connect_timer = NULL;
    
    delete ssock->sock;
    ssock->sock = NULL;

    delete ssock->read_state.read_buf;
    delete ssock->read_state.orig_buf;
    ssock->read_state.read_buf = NULL;
    ssock->read_state.orig_buf = NULL;
    
    return PJ_SUCCESS;
}


/*
 * Associate arbitrary data with the SSL socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_set_user_data (pj_ssl_sock_t *ssock,
					       void *user_data)
{
    PJ_ASSERT_RETURN(ssock, PJ_EINVAL);
    
    ssock->user_data = user_data;
    
    return PJ_SUCCESS;
}
					       

/*
 * Retrieve the user data previously associated with this SSL
 * socket.
 */
PJ_DEF(void*) pj_ssl_sock_get_user_data(pj_ssl_sock_t *ssock)
{
    PJ_ASSERT_RETURN(ssock, NULL);
    
    return ssock->user_data;
}


/*
 * Retrieve the local address and port used by specified SSL socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_get_info (pj_ssl_sock_t *ssock,
					  pj_ssl_sock_info *info)
{
    const char *cipher_names[0x1B] = {
	"TLS_RSA_WITH_NULL_MD5",
	"TLS_RSA_WITH_NULL_SHA",
	"TLS_RSA_EXPORT_WITH_RC4_40_MD5",
	"TLS_RSA_WITH_RC4_128_MD5",
	"TLS_RSA_WITH_RC4_128_SHA",
	"TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5",
	"TLS_RSA_WITH_IDEA_CBC_SHA",
	"TLS_RSA_EXPORT_WITH_DES40_CBC_SHA",
	"TLS_RSA_WITH_DES_CBC_SHA",
	"TLS_RSA_WITH_3DES_EDE_CBC_SHA",
	"TLS_DH_DSS_EXPORT_WITH_DES40_CBC_SHA",
	"TLS_DH_DSS_WITH_DES_CBC_SHA",
	"TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA",
	"TLS_DH_RSA_EXPORT_WITH_DES40_CBC_SHA",
	"TLS_DH_RSA_WITH_DES_CBC_SHA",
	"TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA",
	"TLS_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA",
	"TLS_DHE_DSS_WITH_DES_CBC_SHA",
	"TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA",
	"TLS_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA",
	"TLS_DHE_RSA_WITH_DES_CBC_SHA",
	"TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA",
	"TLS_DH_anon_EXPORT_WITH_RC4_40_MD5",
	"TLS_DH_anon_WITH_RC4_128_MD5",
	"TLS_DH_anon_EXPORT_WITH_DES40_CBC_SHA",
	"TLS_DH_anon_WITH_DES_CBC_SHA",
	"TLS_DH_anon_WITH_3DES_EDE_CBC_SHA"
    };
    
    PJ_ASSERT_RETURN(ssock && info, PJ_EINVAL);
    
    pj_bzero(info, sizeof(*info));
    
    info->established = ssock->established;
    
    /* Local address */
    if (ssock->sock) {
	const TInetAddr* local_addr_ = ssock->sock->GetLocalAddr();
	int addrlen = sizeof(pj_sockaddr);
	pj_status_t status;
	
	status = PjSymbianOS::Addr2pj(*local_addr_, info->local_addr, &addrlen);
	if (status != PJ_SUCCESS)
	    return status;
    } else {
	pj_sockaddr_cp(&info->local_addr, &ssock->local_addr);
    }

    /* Remote address */
    pj_sockaddr_cp((pj_sockaddr_t*)&info->remote_addr, 
		   (pj_sockaddr_t*)&ssock->rem_addr);

    /* Cipher suite */
    if (info->established) {
	TBuf8<8> cipher;
	if (ssock->sock->GetCipher(cipher) == KErrNone) {
	    TLex8 lex(cipher);
	    TUint cipher_code = cipher[1];    
	    if (cipher_code>=1 && cipher_code<=0x1B)
		info->cipher = pj_str((char*)cipher_names[cipher_code-1]); 
	}
    }

    /* Protocol */
    info->proto = ssock->proto;
    
    return PJ_SUCCESS;
}


/*
 * Starts read operation on this SSL socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_read (pj_ssl_sock_t *ssock,
					    pj_pool_t *pool,
					    unsigned buff_size,
					    pj_uint32_t flags)
{
    PJ_ASSERT_RETURN(ssock && pool && buff_size, PJ_EINVAL);
    PJ_ASSERT_RETURN(ssock->established, PJ_EINVALIDOP);

    /* Reading is already started */
    if (ssock->read_state.orig_buf) {
	return PJ_SUCCESS;
    }

    void *readbuf[1];
    readbuf[0] = pj_pool_alloc(pool, buff_size);
    return pj_ssl_sock_start_read2(ssock, pool, buff_size, readbuf, flags);
}

static void read_cb(int err, void *key)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)key;
    pj_status_t status;

    status = (err == KErrNone)? PJ_SUCCESS : PJ_RETURN_OS_ERROR(err);

    /* Check connection status */
    if (err == KErrEof || !PjSymbianOS::Instance()->IsConnectionUp() ||
	!ssock->established) 
    {
	status = PJ_EEOF;
    }
    
    /* Notify data arrival */
    if (ssock->cb.on_data_read) {
	pj_size_t remainder = 0;
	char *data = (char*)ssock->read_state.orig_buf->Ptr();
	pj_size_t data_len = ssock->read_state.read_buf->Length() + 
			     ssock->read_state.read_buf->Ptr() -
			     ssock->read_state.orig_buf->Ptr();
	
	if (data_len > 0) {
	    /* Notify received data */
	    pj_bool_t ret = (*ssock->cb.on_data_read)(ssock, data, data_len, 
						      status, &remainder);
	    if (!ret) {
		/* We've been destroyed */
		return;
	    }
	    
	    /* Calculate available data for next READ operation */
	    if (remainder > 0) {
		pj_size_t data_maxlen = ssock->read_state.orig_buf->MaxLength();
		
		/* There is some data left unconsumed by application, we give
		 * smaller buffer for next READ operation.
		 */
		ssock->read_state.read_buf->Set((TUint8*)data+remainder, 0, 
					        data_maxlen - remainder);
	    } else {
		/* Give all buffer for next READ operation. 
		 */
		ssock->read_state.read_buf->Set(*ssock->read_state.orig_buf);
	    }
	}
    }

    if (status == PJ_SUCCESS) {
	/* Perform the "next" READ operation */
	CPjSSLSocketReader *reader = ssock->sock->GetReader(); 
	ssock->read_state.read_buf->SetLength(0);
	status = reader->Read(&read_cb, ssock, *ssock->read_state.read_buf, 
			      ssock->read_state.flags);
	if (status != PJ_EPENDING) {
	    /* Notify error */
	    (*ssock->cb.on_data_read)(ssock, NULL, 0, status, NULL);
	}
    }
    
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	/* Connection closed or something goes wrong */
	delete ssock->read_state.read_buf;
	delete ssock->read_state.orig_buf;
	ssock->read_state.read_buf = NULL;
	ssock->read_state.orig_buf = NULL;
	ssock->established = PJ_FALSE;
    }
}

/*
 * Same as #pj_ssl_sock_start_read(), except that the application
 * supplies the buffers for the read operation so that the acive socket
 * does not have to allocate the buffers.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_read2 (pj_ssl_sock_t *ssock,
					     pj_pool_t *pool,
					     unsigned buff_size,
					     void *readbuf[],
					     pj_uint32_t flags)
{
    PJ_ASSERT_RETURN(ssock && buff_size && readbuf, PJ_EINVAL);
    PJ_ASSERT_RETURN(ssock->established, PJ_EINVALIDOP);
    
    /* Return failure if access point is marked as down by app. */
    PJ_SYMBIAN_CHECK_CONNECTION();
    
    /* Reading is already started */
    if (ssock->read_state.orig_buf) {
	return PJ_SUCCESS;
    }
    
    PJ_UNUSED_ARG(pool);

    /* Get reader instance */
    CPjSSLSocketReader *reader = ssock->sock->GetReader();
    if (!reader)
	return PJ_ENOMEM;
    
    /* We manage two buffer pointers here:
     * 1. orig_buf keeps the orginal buffer address (and its max length).
     * 2. read_buf provides buffer for READ operation, mind that there may be
     *    some remainder data left by application.
     */
    ssock->read_state.read_buf = new TPtr8((TUint8*)readbuf[0], 0, buff_size);
    ssock->read_state.orig_buf = new TPtr8((TUint8*)readbuf[0], 0, buff_size);
    ssock->read_state.flags = flags;
    
    pj_status_t status;
    status = reader->Read(&read_cb, ssock, *ssock->read_state.read_buf, 
			  ssock->read_state.flags);
    
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	delete ssock->read_state.read_buf;
	delete ssock->read_state.orig_buf;
	ssock->read_state.read_buf = NULL;
	ssock->read_state.orig_buf = NULL;
	
	return status;
    }
    
    return PJ_SUCCESS;
}

/*
 * Same as pj_ssl_sock_start_read(), except that this function is used
 * only for datagram sockets, and it will trigger \a on_data_recvfrom()
 * callback instead.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_recvfrom (pj_ssl_sock_t *ssock,
						pj_pool_t *pool,
						unsigned buff_size,
						pj_uint32_t flags)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(buff_size);
    PJ_UNUSED_ARG(flags);
    return PJ_ENOTSUP;
}

/*
 * Same as #pj_ssl_sock_start_recvfrom() except that the recvfrom() 
 * operation takes the buffer from the argument rather than creating
 * new ones.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_recvfrom2 (pj_ssl_sock_t *ssock,
						 pj_pool_t *pool,
						 unsigned buff_size,
						 void *readbuf[],
						 pj_uint32_t flags)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(buff_size);
    PJ_UNUSED_ARG(readbuf);
    PJ_UNUSED_ARG(flags);
    return PJ_ENOTSUP;
}

static void send_cb(int err, void *key)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)key;
    write_state_t *st = &ssock->write_state;

    /* Check connection status */
    if (err != KErrNone || !PjSymbianOS::Instance()->IsConnectionUp() ||
	!ssock->established) 
    {
	ssock->established = PJ_FALSE;
	return;
    }

    /* Remove sent data from buffer */
    st->start += st->current_data->len;
    st->len -= st->current_data->len;

    /* Reset current outstanding send */
    st->current_data = NULL;

    /* Let's check if there is pending data to send */
    if (st->len) {
	write_data_t *wdata = (write_data_t*)st->start;
	pj_status_t status;
	
	st->send_ptr.Set((TUint8*)wdata->data, (TInt)wdata->data_len);
	st->current_data = wdata;
	status = ssock->sock->Send(&send_cb, ssock, st->send_ptr, 0);
	if (status != PJ_EPENDING) {
	    ssock->established = PJ_FALSE;
	    st->len = 0;
	    return;
	}
    }
}

/*
 * Send data using the socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_send (pj_ssl_sock_t *ssock,
				      pj_ioqueue_op_key_t *send_key,
				      const void *data,
				      pj_ssize_t *size,
				      unsigned flags)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(ssock && data && size, PJ_EINVAL);
    PJ_ASSERT_RETURN(ssock->write_state.max_len == 0 || 
		     ssock->write_state.max_len >= (pj_size_t)*size, 
		     PJ_ETOOSMALL);
    
    /* Check connection status */
    if (!PjSymbianOS::Instance()->IsConnectionUp() || !ssock->established) 
    {
	ssock->established = PJ_FALSE;
	return PJ_ECANCELLED;
    }

    write_state_t *st = &ssock->write_state;
    
    /* Synchronous mode */
    if (st->max_len == 0) {
	st->send_ptr.Set((TUint8*)data, (TInt)*size);
	return ssock->sock->SendSync(st->send_ptr, flags);
    }

    /* CSecureSocket only allows one outstanding send operation, so
     * we use buffering mechanism to allow application to perform send 
     * operations at any time.
     */
    
    pj_size_t avail_len = st->max_len - st->len;
    pj_size_t needed_len = *size + sizeof(write_data_t) - 1;
    
    /* Align needed_len to be multiplication of 4 */
    needed_len = ((needed_len + 3) >> 2) << 2; 

    /* Block until there is buffer slot available! */
    while (needed_len >= avail_len) {
	pj_symbianos_poll(-1, -1);
	avail_len = st->max_len - st->len;
    }

    /* Ok, make sure the new data will not get wrapped */
    if (st->start + st->len + needed_len > st->buf + st->max_len) {
	/* Align buffer left */
	pj_memmove(st->buf, st->start, st->len);
	st->start = st->buf;
    }
    
    /* Push back the send data into the buffer */
    write_data_t *wdata = (write_data_t*)(st->start + st->len);
    
    wdata->len = needed_len;
    wdata->key = send_key;
    wdata->data_len = (pj_size_t)*size;
    pj_memcpy(wdata->data, data, *size);
    st->len += needed_len;

    /* If no outstanding send, send it */
    if (st->current_data == NULL) {
	pj_status_t status;
	    
	wdata = (write_data_t*)st->start;
	st->current_data = wdata;
	st->send_ptr.Set((TUint8*)wdata->data, (TInt)wdata->data_len);
	status = ssock->sock->Send(&send_cb, ssock, st->send_ptr, flags);
	
	if (status != PJ_EPENDING) {
	    *size = -status;
	    return status;
	}
    }
    
    return PJ_SUCCESS;
}

/*
 * Send datagram using the socket.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_sendto (pj_ssl_sock_t *ssock,
					pj_ioqueue_op_key_t *send_key,
					const void *data,
					pj_ssize_t *size,
					unsigned flags,
					const pj_sockaddr_t *addr,
					int addr_len)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(send_key);
    PJ_UNUSED_ARG(data);
    PJ_UNUSED_ARG(size);
    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(addr);
    PJ_UNUSED_ARG(addr_len);
    return PJ_ENOTSUP;
}

/*
 * Starts asynchronous socket accept() operations on this SSL socket. 
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_accept (pj_ssl_sock_t *ssock,
					      pj_pool_t *pool,
					      const pj_sockaddr_t *local_addr,
					      int addr_len)
{
    PJ_UNUSED_ARG(ssock);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(local_addr);
    PJ_UNUSED_ARG(addr_len);
    
    return PJ_ENOTSUP;
}

static void connect_cb(int err, void *key)
{
    pj_ssl_sock_t *ssock = (pj_ssl_sock_t*)key;
    pj_status_t status;
    
    if (ssock->connect_timer) {
	delete ssock->connect_timer;
	ssock->connect_timer = NULL;
    }

    status = (err == KErrNone)? PJ_SUCCESS : PJ_RETURN_OS_ERROR(err);
    if (status == PJ_SUCCESS) {
	ssock->established = PJ_TRUE;
    } else {
	delete ssock->sock;
	ssock->sock = NULL;
    }
    
    if (ssock->cb.on_connect_complete) {
	pj_bool_t ret = (*ssock->cb.on_connect_complete)(ssock, status);
	if (!ret) {
	    /* We've been destroyed */
	    return;
	}
    }
}

static void connect_timer_cb(void *key)
{
    connect_cb(KErrTimedOut, key);
}

/*
 * Starts asynchronous socket connect() operation and SSL/TLS handshaking 
 * for this socket. Once the connection is done (either successfully or not),
 * the \a on_connect_complete() callback will be called.
 */
PJ_DEF(pj_status_t) pj_ssl_sock_start_connect (pj_ssl_sock_t *ssock,
					       pj_pool_t *pool,
					       const pj_sockaddr_t *localaddr,
					       const pj_sockaddr_t *remaddr,
					       int addr_len)
{
    CPjSSLSocket *sock = NULL;
    pj_status_t status;
    
    PJ_ASSERT_RETURN(ssock && pool && localaddr && remaddr && addr_len,
		     PJ_EINVAL);

    /* Check connection status */
    PJ_SYMBIAN_CHECK_CONNECTION();
    
    if (ssock->sock != NULL) {
	CPjSSLSocket::ssl_state state = ssock->sock->GetState();
	switch (state) {
	case CPjSSLSocket::SSL_STATE_ESTABLISHED:
	    return PJ_SUCCESS;
	default:
	    return PJ_EPENDING;
	}
    }

    /* Set SSL protocol */
    TPtrC8 proto;
    
    if (ssock->proto == PJ_SSL_SOCK_PROTO_DEFAULT)
	ssock->proto = PJ_SSL_SOCK_PROTO_TLS1;

    /* CSecureSocket only support TLS1.0 and SSL3.0 */
    switch(ssock->proto) {
    case PJ_SSL_SOCK_PROTO_TLS1:
	proto.Set((const TUint8*)"TLS1.0", 6);
	break;
    case PJ_SSL_SOCK_PROTO_SSL3:
	proto.Set((const TUint8*)"SSL3.0", 6);
	break;
    default:
	return PJ_ENOTSUP;
    }

    /* Prepare addresses */
    TInetAddr localaddr_, remaddr_;
    status = PjSymbianOS::pj2Addr(*(pj_sockaddr*)localaddr, addr_len, 
				  localaddr_);
    if (status != PJ_SUCCESS)
	return status;
    
    status = PjSymbianOS::pj2Addr(*(pj_sockaddr*)remaddr, addr_len,
				  remaddr_);
    if (status != PJ_SUCCESS)
	return status;

    pj_sockaddr_cp((pj_sockaddr_t*)&ssock->rem_addr, remaddr);

    /* Init SSL engine */
    TRAPD(err, sock = CPjSSLSocket::NewL(proto));
    if (err != KErrNone)
	return PJ_ENOMEM;
    
    if (ssock->timeout.sec != 0 || ssock->timeout.msec != 0) {
	ssock->connect_timer = new CPjTimer(&ssock->timeout, 
					    &connect_timer_cb, ssock);
    }
    
    /* Convert server name to Symbian descriptor */
    TPtrC8 servername_((TUint8*)ssock->servername.ptr, 
		       ssock->servername.slen);
    
    /* Try to connect */
    status = sock->Connect(&connect_cb, ssock, localaddr_, remaddr_,
			   servername_);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	delete sock;
	return status;
    }

    ssock->sock = sock;
    return status;
}

