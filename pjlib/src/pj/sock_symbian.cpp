/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pj/sock.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/os.h>
#include <pj/string.h>
#include <pj/unicode.h>

#include "os_symbian.h"


/*
 * Address families.
 */
const pj_uint16_t PJ_AF_UNIX	= 0xFFFF;
const pj_uint16_t PJ_AF_INET	= KAfInet;
const pj_uint16_t PJ_AF_INET6	= KAfInet6;
const pj_uint16_t PJ_AF_PACKET	= 0xFFFF;
const pj_uint16_t PJ_AF_IRDA	= 0xFFFF;

/*
 * Socket types conversion.
 * The values here are indexed based on pj_sock_type
 */
const pj_uint16_t PJ_SOCK_STREAM= KSockStream;
const pj_uint16_t PJ_SOCK_DGRAM	= KSockDatagram;
const pj_uint16_t PJ_SOCK_RAW	= 0xFFFF;
const pj_uint16_t PJ_SOCK_RDM	= 0xFFFF;

/* setsockop() is not really supported. */
const pj_uint16_t PJ_SOL_SOCKET	= 0xFFFF;
const pj_uint16_t PJ_SOL_IP	= 0xFFFF;
const pj_uint16_t PJ_SOL_TCP	= 0xFFFF;
const pj_uint16_t PJ_SOL_UDP	= 0xFFFF;
const pj_uint16_t PJ_SOL_IPV6	= 0xFFFF;

/* ioctl() is also not supported. */
const pj_uint16_t PJ_SO_TYPE    = 0xFFFF;
const pj_uint16_t PJ_SO_RCVBUF  = 0xFFFF;
const pj_uint16_t PJ_SO_SNDBUF  = 0xFFFF;


/////////////////////////////////////////////////////////////////////////////
//
// CPjSocket implementation.
// (declaration is in os_symbian.h)
//

CPjSocket::~CPjSocket()
{
    DestroyReader();
    sock_.Close();
}


// Create socket reader.
CPjSocketReader *CPjSocket::CreateReader(unsigned max_len)
{
    pj_assert(sockReader_ == NULL);
    return sockReader_ = CPjSocketReader::NewL(*this, max_len);
}

// Delete socket reader when it's not wanted.
void CPjSocket::DestroyReader() 
{
    if (sockReader_) {
	if (sockReader_->IsActive())
	    sockReader_->Cancel();
	delete sockReader_;
	sockReader_ = NULL;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// CPjSocketReader implementation
// (declaration in os_symbian.h)
//


CPjSocketReader::CPjSocketReader(CPjSocket &sock)
: CActive(EPriorityStandard), 
  sock_(sock), buffer_(NULL, 0), readCb_(NULL), key_(NULL)
{
}


void CPjSocketReader::ConstructL(unsigned max_len)
{
    TProtocolDesc aProtocol;
    TInt err;

    err = sock_.Socket().Info(aProtocol);
    User::LeaveIfError(err);

    isDatagram_ = (aProtocol.iSockType == KSockDatagram);

    TUint8 *ptr = new TUint8[max_len];
    buffer_.Set(ptr, 0, (TInt)max_len);
    CActiveScheduler::Add(this);
}

CPjSocketReader *CPjSocketReader::NewL(CPjSocket &sock, unsigned max_len)
{
    CPjSocketReader *self = new (ELeave) CPjSocketReader(sock);
    CleanupStack::PushL(self);
    self->ConstructL(max_len);
    CleanupStack::Pop(self);

    return self;
}


CPjSocketReader::~CPjSocketReader()
{
    const TUint8 *data = buffer_.Ptr();
    delete [] data;
}

void CPjSocketReader::StartRecv(void (*cb)(void *key), 
			        void *key, 
			        TDes8 *aDesc,
			        TUint flags)
{
    StartRecvFrom(cb, key, aDesc, flags, NULL);
}

void CPjSocketReader::StartRecvFrom(void (*cb)(void *key), 
				    void *key, 
				    TDes8 *aDesc,
				    TUint flags,
				    TSockAddr *fromAddr)
{
    readCb_ = cb;
    key_ = key;

    if (aDesc == NULL) aDesc = &buffer_;
    if (fromAddr == NULL) fromAddr = &recvAddr_;

    sock_.Socket().RecvFrom(*aDesc, *fromAddr, flags, iStatus);
    SetActive();
}

void CPjSocketReader::DoCancel()
{
    sock_.Socket().CancelRecv();
}

void CPjSocketReader::RunL()
{
    void (*old_cb)(void *key) = readCb_;
    void *old_key = key_;

    readCb_ = NULL;
    key_ = NULL;

    if (old_cb) {
	(*old_cb)(old_key);
    }
}

// Append data to aDesc, up to aDesc's maximum size.
// If socket is datagram based, buffer_ will be clared.
void CPjSocketReader::ReadData(TDes8 &aDesc, TInetAddr *addr)
{
    if (isDatagram_)
	aDesc.Zero();

    if (buffer_.Length() == 0)
	return;

    TInt size_to_copy = aDesc.MaxLength() - aDesc.Length();
    if (size_to_copy > buffer_.Length())
	size_to_copy = buffer_.Length();

    aDesc.Append(buffer_.Ptr(), size_to_copy);

    if (isDatagram_)
	buffer_.Zero();
    else
	buffer_.Delete(0, size_to_copy);

    if (addr)
	*addr = recvAddr_;
}



/////////////////////////////////////////////////////////////////////////////
//
// PJLIB's sock.h implementation
//

/*
 * Convert 16-bit value from network byte order to host byte order.
 */
PJ_DEF(pj_uint16_t) pj_ntohs(pj_uint16_t netshort)
{
#if PJ_IS_LITTLE_ENDIAN
    return pj_swap16(netshort);
#else
    return netshort;
#endif
}

/*
 * Convert 16-bit value from host byte order to network byte order.
 */
PJ_DEF(pj_uint16_t) pj_htons(pj_uint16_t hostshort)
{
#if PJ_IS_LITTLE_ENDIAN
    return pj_swap16(hostshort);
#else
    return hostshort;
#endif
}

/*
 * Convert 32-bit value from network byte order to host byte order.
 */
PJ_DEF(pj_uint32_t) pj_ntohl(pj_uint32_t netlong)
{
#if PJ_IS_LITTLE_ENDIAN
    return pj_swap32(netlong);
#else
    return netlong;
#endif
}

/*
 * Convert 32-bit value from host byte order to network byte order.
 */
PJ_DEF(pj_uint32_t) pj_htonl(pj_uint32_t hostlong)
{
#if PJ_IS_LITTLE_ENDIAN
    return pj_swap32(hostlong);
#else
    return netlong;
#endif
}

/*
 * Convert an Internet host address given in network byte order
 * to string in standard numbers and dots notation.
 */
PJ_DEF(char*) pj_inet_ntoa(pj_in_addr inaddr)
{
    static TBuf<20> str16;
    static char str8[20];

    /* (Symbian IP address is in host byte order) */
    TInetAddr temp_addr((TUint32)pj_ntohl(inaddr.s_addr), (TUint)0);
    temp_addr.Output(str16);
 
    return pj_unicode_to_ansi(str16.PtrZ(), str16.Length(),
			      str8, sizeof(str8));
}

/*
 * This function converts the Internet host address cp from the standard
 * numbers-and-dots notation into binary data and stores it in the structure
 * that inp points to. 
 */
PJ_DEF(int) pj_inet_aton(const pj_str_t *cp, struct pj_in_addr *inp)
{
    enum { MAXIPLEN = 16 };

    /* Initialize output with PJ_INADDR_NONE.
     * Some apps relies on this instead of the return value
     * (and anyway the return value is quite confusing!)
     */
    inp->s_addr = PJ_INADDR_NONE;

    /* Caution:
     *	this function might be called with cp->slen >= 16
     *  (i.e. when called with hostname to check if it's an IP addr).
     */
    PJ_ASSERT_RETURN(cp && cp->slen && inp, 0);
    if (cp->slen >= 16) {
	return 0;
    }

    char tempaddr8[MAXIPLEN];
    pj_memcpy(tempaddr8, cp->ptr, cp->slen);
    tempaddr8[cp->slen] = '\0';

    wchar_t tempaddr16[MAXIPLEN];
    pj_ansi_to_unicode(tempaddr8, pj_ansi_strlen(tempaddr8),
		       tempaddr16, sizeof(tempaddr16));

    TBuf<MAXIPLEN> ip_addr(tempaddr16);

    TInetAddr addr;
    addr.Init(KAfInet);
    if (addr.Input(ip_addr) == KErrNone) {
	/* Success (Symbian IP address is in host byte order) */
	inp->s_addr = pj_htonl(addr.Address());
	return 1;
    } else {
	/* Error */
	return 0;
    }
}

/*
 * Convert address string with numbers and dots to binary IP address.
 */ 
PJ_DEF(pj_in_addr) pj_inet_addr(const pj_str_t *cp)
{
    pj_in_addr addr;

    pj_inet_aton(cp, &addr);
    return addr;
}

/*
 * Convert address string with numbers and dots to binary IP address.
 */ 
PJ_DEF(pj_in_addr) pj_inet_addr2(const char *cp)
{
    pj_str_t str = pj_str((char*)cp);
    return pj_inet_addr(&str);
}

/*
 * Set the IP address of an IP socket address from string address, 
 * with resolving the host if necessary. The string address may be in a
 * standard numbers and dots notation or may be a hostname. If hostname
 * is specified, then the function will resolve the host into the IP
 * address.
 */
PJ_DEF(pj_status_t) pj_sockaddr_in_set_str_addr( pj_sockaddr_in *addr,
					         const pj_str_t *str_addr)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(!str_addr || str_addr->slen < PJ_MAX_HOSTNAME, 
                     (addr->sin_addr.s_addr=PJ_INADDR_NONE, PJ_EINVAL));

    addr->sin_family = PJ_AF_INET;
    pj_memset(addr->sin_zero, 0, sizeof(addr->sin_zero));

    if (str_addr && str_addr->slen) {
	addr->sin_addr = pj_inet_addr(str_addr);
	if (addr->sin_addr.s_addr == PJ_INADDR_NONE) {
    	    pj_hostent he;
	    pj_status_t rc;

	    rc = pj_gethostbyname(str_addr, &he);
	    if (rc == 0) {
		addr->sin_addr.s_addr = *(pj_uint32_t*)he.h_addr;
	    } else {
		addr->sin_addr.s_addr = PJ_INADDR_NONE;
		return rc;
	    }
	}

    } else {
	addr->sin_addr.s_addr = 0;
    }

    return PJ_SUCCESS;
}

/*
 * Set the IP address and port of an IP socket address.
 * The string address may be in a standard numbers and dots notation or 
 * may be a hostname. If hostname is specified, then the function will 
 * resolve the host into the IP address.
 */
PJ_DEF(pj_status_t) pj_sockaddr_in_init( pj_sockaddr_in *addr,
				         const pj_str_t *str_addr,
					 pj_uint16_t port)
{
    PJ_ASSERT_RETURN(addr, (addr->sin_addr.s_addr=PJ_INADDR_NONE, PJ_EINVAL));

    addr->sin_family = PJ_AF_INET;
    pj_memset(addr->sin_zero, 0, sizeof(addr->sin_zero));
    pj_sockaddr_in_set_port(addr, port);
    return pj_sockaddr_in_set_str_addr(addr, str_addr);
}

 
/*
 * Get hostname.
 */
PJ_DEF(const pj_str_t*) pj_gethostname(void)
{
    static char buf[PJ_MAX_HOSTNAME];
    static pj_str_t hostname;

    PJ_CHECK_STACK();

    if (hostname.ptr == NULL) {
	RHostResolver & resv = PjSymbianOS::Instance()->GetResolver();
	TRequestStatus reqStatus;
	THostName tmpName;

	resv.GetHostName(tmpName, reqStatus);
	User::WaitForRequest(reqStatus);

	hostname.ptr = pj_unicode_to_ansi(tmpName.Ptr(), tmpName.Length(),
					  buf, sizeof(buf));
	hostname.slen = tmpName.Length();
    }
    return &hostname;
}

/*
 * Get first IP address associated with the hostname.
 */
PJ_DEF(pj_in_addr) pj_gethostaddr(void)
{
    pj_sockaddr_in addr;
    const pj_str_t *hostname = pj_gethostname();

    pj_sockaddr_in_set_str_addr(&addr, hostname);
    return addr.sin_addr;
}


/*
 * Create new socket/endpoint for communication and returns a descriptor.
 */
PJ_DEF(pj_status_t) pj_sock_socket(int af, 
				   int type, 
				   int proto,
				   pj_sock_t *p_sock)
{
    TInt rc;

    PJ_CHECK_STACK();

    /* Sanity checks. */
    PJ_ASSERT_RETURN(p_sock!=NULL, PJ_EINVAL);

    /* Set proto if none is specified. */
    if (proto == 0) {
	if (type == PJ_SOCK_STREAM)
	    proto = KProtocolInetTcp;
	else if (type == PJ_SOCK_DGRAM)
	    proto = KProtocolInetUdp;
    }

    /* Create Symbian RSocket */
    RSocket rSock;
    rc = rSock.Open(PjSymbianOS::Instance()->SocketServ(), af, type, proto);
    if (rc != KErrNone)
	return PJ_RETURN_OS_ERROR(rc);


    /* Wrap Symbian RSocket into PJLIB's CPjSocket, and return to caller */
    CPjSocket *pjSock = new CPjSocket(rSock);
    *p_sock = (pj_sock_t)pjSock;

    return PJ_SUCCESS;
}


/*
 * Bind socket.
 */
PJ_DEF(pj_status_t) pj_sock_bind( pj_sock_t sock, 
				  const pj_sockaddr_t *addr,
				  int len)
{
    TInt rc;

    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(sock != 0, PJ_EINVAL);
    PJ_ASSERT_RETURN(addr && len == sizeof(pj_sockaddr_in), PJ_EINVAL);

    // Convert PJLIB's pj_sockaddr_in into Symbian's TInetAddr
    TInetAddr inetAddr;
    PjSymbianOS::pj2Addr(*(pj_sockaddr_in*)addr, inetAddr);

    // Get the RSocket instance
    RSocket &rSock = ((CPjSocket*)sock)->Socket();

    // Bind
    rc = rSock.Bind(inetAddr);

    return (rc==KErrNone) ? PJ_SUCCESS : PJ_RETURN_OS_ERROR(rc);
}


/*
 * Bind socket.
 */
PJ_DEF(pj_status_t) pj_sock_bind_in( pj_sock_t sock, 
				     pj_uint32_t addr32,
				     pj_uint16_t port)
{
    pj_sockaddr_in addr;

    PJ_CHECK_STACK();

    pj_bzero(&addr, sizeof(addr));
    addr.sin_family = PJ_AF_INET;
    addr.sin_addr.s_addr = pj_htonl(addr32);
    addr.sin_port = pj_htons(port);

    return pj_sock_bind(sock, &addr, sizeof(pj_sockaddr_in));
}


/*
 * Close socket.
 */
PJ_DEF(pj_status_t) pj_sock_close(pj_sock_t sock)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(sock != 0, PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)sock;

    // This will close the socket.
    delete pjSock;

    return PJ_SUCCESS;
}

/*
 * Get remote's name.
 */
PJ_DEF(pj_status_t) pj_sock_getpeername( pj_sock_t sock,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    PJ_CHECK_STACK();
    
    PJ_ASSERT_RETURN(sock && addr && namelen && 
		     *namelen>=sizeof(pj_sockaddr_in), PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)sock;
    RSocket &rSock = pjSock->Socket();

    // Socket must be connected.
    PJ_ASSERT_RETURN(pjSock->IsConnected(), PJ_EINVALIDOP);

    TInetAddr inetAddr;
    rSock.RemoteName(inetAddr);

    PjSymbianOS::Addr2pj(inetAddr, *(pj_sockaddr_in*)addr);
    *namelen = sizeof(pj_sockaddr_in);

    return PJ_SUCCESS;
}

/*
 * Get socket name.
 */
PJ_DEF(pj_status_t) pj_sock_getsockname( pj_sock_t sock,
					 pj_sockaddr_t *addr,
					 int *namelen)
{
    PJ_CHECK_STACK();
    
    PJ_ASSERT_RETURN(sock && addr && namelen && 
		     *namelen>=sizeof(pj_sockaddr_in), PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)sock;
    RSocket &rSock = pjSock->Socket();

    TInetAddr inetAddr;
    rSock.LocalName(inetAddr);

    PjSymbianOS::Addr2pj(inetAddr, *(pj_sockaddr_in*)addr);
    *namelen = sizeof(pj_sockaddr_in);

    return PJ_SUCCESS;
}

/*
 * Send data
 */
PJ_DEF(pj_status_t) pj_sock_send(pj_sock_t sock,
				 const void *buf,
				 pj_ssize_t *len,
				 unsigned flags)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(sock && buf && len, PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)sock;
    RSocket &rSock = pjSock->Socket();

    // send() should only be called to connected socket
    PJ_ASSERT_RETURN(pjSock->IsConnected(), PJ_EINVALIDOP);

    TPtrC8 data((const TUint8*)buf, (TInt)*len);
    TRequestStatus reqStatus;
    TSockXfrLength sentLen;

    rSock.Send(data, flags, reqStatus, sentLen);
    User::WaitForRequest(reqStatus);

    if (reqStatus.Int()==KErrNone) {
	//*len = (TInt) sentLen.Length();
	return PJ_SUCCESS;
    } else
	return PJ_RETURN_OS_ERROR(reqStatus.Int());
}


/*
 * Send data.
 */
PJ_DEF(pj_status_t) pj_sock_sendto(pj_sock_t sock,
				   const void *buf,
				   pj_ssize_t *len,
				   unsigned flags,
				   const pj_sockaddr_t *to,
				   int tolen)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(sock && buf && len, PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)sock;
    RSocket &rSock = pjSock->Socket();

    // Only supports AF_INET for now
    PJ_ASSERT_RETURN(tolen==sizeof(pj_sockaddr_in) && 
		     ((pj_sockaddr*)to)->addr.sa_family == PJ_AF_INET, 
		     PJ_EINVAL);

    TInetAddr inetAddr;
    PjSymbianOS::pj2Addr(*(pj_sockaddr_in*)to, inetAddr);

    TPtrC8 data((const TUint8*)buf, (TInt)*len);
    TRequestStatus reqStatus;
    TSockXfrLength sentLen;

    rSock.SendTo(data, inetAddr, flags, reqStatus, sentLen);
    User::WaitForRequest(reqStatus);

    if (reqStatus.Int()==KErrNone) {
	//For some reason TSockXfrLength is not returning correctly!
	//*len = (TInt) sentLen.Length();
	return PJ_SUCCESS;
    } else 
	return PJ_RETURN_OS_ERROR(reqStatus.Int());
}

/*
 * Receive data.
 */
PJ_DEF(pj_status_t) pj_sock_recv(pj_sock_t sock,
				 void *buf,
				 pj_ssize_t *len,
				 unsigned flags)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(sock && buf && len, PJ_EINVAL);
    PJ_ASSERT_RETURN(*len > 0, PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)sock;
    RSocket &rSock = pjSock->Socket();

    if (pjSock->Reader()) {
	CPjSocketReader *reader = pjSock->Reader();

	while (reader->IsActive() && !reader->HasData()) {
	    User::WaitForAnyRequest();
	}

	if (reader->HasData()) {
	    TPtr8 data((TUint8*)buf, (TInt)*len);
	    TInetAddr inetAddr;

	    reader->ReadData(data, &inetAddr);

	    *len = data.Length();
	    return PJ_SUCCESS;
	}
    }

    TRequestStatus reqStatus;
    TSockXfrLength recvLen;
    TPtr8 data((TUint8*)buf, (TInt)*len, (TInt)*len);

    rSock.Recv(data, flags, reqStatus, recvLen);
    User::WaitForRequest(reqStatus);

    if (reqStatus == KErrNone) {
	//*len = (TInt)recvLen.Length();
	*len = data.Length();
	return PJ_SUCCESS;
    } else {
	*len = -1;
	return PJ_RETURN_OS_ERROR(reqStatus.Int());
    }
}

/*
 * Receive data.
 */
PJ_DEF(pj_status_t) pj_sock_recvfrom(pj_sock_t sock,
				     void *buf,
				     pj_ssize_t *len,
				     unsigned flags,
				     pj_sockaddr_t *from,
				     int *fromlen)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(sock && buf && len && from && fromlen, PJ_EINVAL);
    PJ_ASSERT_RETURN(*len > 0, PJ_EINVAL);
    PJ_ASSERT_RETURN(*fromlen >= sizeof(pj_sockaddr_in), PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)sock;
    RSocket &rSock = pjSock->Socket();

    if (pjSock->Reader()) {
	CPjSocketReader *reader = pjSock->Reader();

	while (reader->IsActive() && !reader->HasData()) {
	    User::WaitForAnyRequest();
	}

	if (reader->HasData()) {
	    TPtr8 data((TUint8*)buf, (TInt)*len);
	    TInetAddr inetAddr;

	    reader->ReadData(data, &inetAddr);

	    *len = data.Length();

	    if (from && fromlen) {
		PjSymbianOS::Addr2pj(inetAddr, *(pj_sockaddr_in*)from);
		*fromlen = sizeof(pj_sockaddr_in);
	    }
	    return PJ_SUCCESS;
	}
    }

    TInetAddr inetAddr;
    TRequestStatus reqStatus;
    TSockXfrLength recvLen;
    TPtr8 data((TUint8*)buf, (TInt)*len, (TInt)*len);

    rSock.RecvFrom(data, inetAddr, flags, reqStatus, recvLen);
    User::WaitForRequest(reqStatus);

    if (reqStatus == KErrNone) {
	//*len = (TInt)recvLen.Length();
	*len = data.Length();
	*fromlen = sizeof(pj_sockaddr_in);
	PjSymbianOS::Addr2pj(inetAddr, *(pj_sockaddr_in*)from);
	return PJ_SUCCESS;
    } else {
	*len = -1;
	*fromlen = -1;
	return PJ_RETURN_OS_ERROR(reqStatus.Int());
    }
}

/*
 * Get socket option.
 */
PJ_DEF(pj_status_t) pj_sock_getsockopt( pj_sock_t sock,
					pj_uint16_t level,
					pj_uint16_t optname,
					void *optval,
					int *optlen)
{
    // Not supported for now.
    PJ_UNUSED_ARG(sock);
    PJ_UNUSED_ARG(level);
    PJ_UNUSED_ARG(optname);
    PJ_UNUSED_ARG(optval);
    PJ_UNUSED_ARG(optlen);
    return PJ_EINVALIDOP;
}

/*
 * Set socket option.
 */
PJ_DEF(pj_status_t) pj_sock_setsockopt( pj_sock_t sock,
					pj_uint16_t level,
					pj_uint16_t optname,
					const void *optval,
					int optlen)
{
    // Not supported for now.
    PJ_UNUSED_ARG(sock);
    PJ_UNUSED_ARG(level);
    PJ_UNUSED_ARG(optname);
    PJ_UNUSED_ARG(optval);
    PJ_UNUSED_ARG(optlen);
    return PJ_EINVALIDOP;
}

/*
 * Connect socket.
 */
PJ_DEF(pj_status_t) pj_sock_connect( pj_sock_t sock,
				     const pj_sockaddr_t *addr,
				     int namelen)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(sock && addr && namelen, PJ_EINVAL);
    PJ_ASSERT_RETURN(((pj_sockaddr*)addr)->addr.sa_family == PJ_AF_INET, 
		     PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)sock;
    RSocket &rSock = pjSock->Socket();

    TInetAddr inetAddr;
    TRequestStatus reqStatus;

    PjSymbianOS::pj2Addr(*(pj_sockaddr_in*)addr, inetAddr);

    rSock.Connect(inetAddr, reqStatus);
    User::WaitForRequest(reqStatus);

    if (reqStatus == KErrNone) {
	pjSock->SetConnected(true);
	return PJ_SUCCESS;
    } else {
	return PJ_RETURN_OS_ERROR(reqStatus.Int());
    }
}


/*
 * Shutdown socket.
 */
#if PJ_HAS_TCP
PJ_DEF(pj_status_t) pj_sock_shutdown( pj_sock_t sock,
				      int how)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(sock, PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)sock;
    RSocket &rSock = pjSock->Socket();

    RSocket::TShutdown aHow;
    if (how == PJ_SD_RECEIVE)
	aHow = RSocket::EStopInput;
    else if (how == PJ_SHUT_WR)
	aHow = RSocket::EStopOutput;
    else
	aHow = RSocket::ENormal;

    TRequestStatus reqStatus;

    rSock.Shutdown(aHow, reqStatus);
    User::WaitForRequest(reqStatus);

    if (reqStatus == KErrNone) {
	return PJ_SUCCESS;
    } else {
	return PJ_RETURN_OS_ERROR(reqStatus.Int());
    }
}

/*
 * Start listening to incoming connections.
 */
PJ_DEF(pj_status_t) pj_sock_listen( pj_sock_t sock,
				    int backlog)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(sock && backlog, PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)sock;
    RSocket &rSock = pjSock->Socket();

    TInt rc = rSock.Listen((TUint)backlog);

    if (rc == KErrNone) {
	return PJ_SUCCESS;
    } else {
	return PJ_RETURN_OS_ERROR(rc);
    }
}

/*
 * Accept incoming connections
 */
PJ_DEF(pj_status_t) pj_sock_accept( pj_sock_t serverfd,
				    pj_sock_t *newsock,
				    pj_sockaddr_t *addr,
				    int *addrlen)
{
    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(serverfd && newsock, PJ_EINVAL);

    CPjSocket *pjSock = (CPjSocket*)serverfd;
    RSocket &rSock = pjSock->Socket();

    // Create a 'blank' socket
    RSocket newSock;
    newSock.Open(PjSymbianOS::Instance()->SocketServ());

    // Call Accept()
    TRequestStatus reqStatus;

    rSock.Accept(newSock, reqStatus);
    User::WaitForRequest(reqStatus);

    if (reqStatus != KErrNone) {
	return PJ_RETURN_OS_ERROR(reqStatus.Int());
    }

    // Create PJ socket
    CPjSocket *newPjSock = new CPjSocket(newSock);
    newPjSock->SetConnected(true);

    *newsock = (pj_sock_t) newPjSock;

    if (addr && addrlen) {
	return pj_sock_getpeername(*newsock, addr, addrlen);
    }

    return PJ_SUCCESS;
}
#endif	/* PJ_HAS_TCP */


