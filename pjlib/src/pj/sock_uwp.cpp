/* $Id$ */
/* 
 * Copyright (C) 2016 Teluu Inc. (http://www.teluu.com)
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
#include <pj/math.h>
#include <pj/os.h>
#include <pj/string.h>
#include <pj/unicode.h>
#include <pj/compat/socket.h>

#include <ppltasks.h>
#include <string>

#include "sock_uwp.h"

#define THIS_FILE	"sock_uwp.cpp"

 /*
 * Address families conversion.
 * The values here are indexed based on pj_addr_family.
 */
const pj_uint16_t PJ_AF_UNSPEC	= AF_UNSPEC;
const pj_uint16_t PJ_AF_UNIX	= AF_UNIX;
const pj_uint16_t PJ_AF_INET	= AF_INET;
const pj_uint16_t PJ_AF_INET6	= AF_INET6;
#ifdef AF_PACKET
const pj_uint16_t PJ_AF_PACKET	= AF_PACKET;
#else
const pj_uint16_t PJ_AF_PACKET	= 0xFFFF;
#endif
#ifdef AF_IRDA
const pj_uint16_t PJ_AF_IRDA	= AF_IRDA;
#else
const pj_uint16_t PJ_AF_IRDA	= 0xFFFF;
#endif

/*
* Socket types conversion.
* The values here are indexed based on pj_sock_type
*/
const pj_uint16_t PJ_SOCK_STREAM= SOCK_STREAM;
const pj_uint16_t PJ_SOCK_DGRAM	= SOCK_DGRAM;
const pj_uint16_t PJ_SOCK_RAW	= SOCK_RAW;
const pj_uint16_t PJ_SOCK_RDM	= SOCK_RDM;

/*
* Socket level values.
*/
const pj_uint16_t PJ_SOL_SOCKET	= SOL_SOCKET;
#ifdef SOL_IP
const pj_uint16_t PJ_SOL_IP	= SOL_IP;
#elif (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_WIN64) && PJ_WIN64) 
const pj_uint16_t PJ_SOL_IP	= IPPROTO_IP;
#else
const pj_uint16_t PJ_SOL_IP	= 0;
#endif /* SOL_IP */

#if defined(SOL_TCP)
const pj_uint16_t PJ_SOL_TCP	= SOL_TCP;
#elif defined(IPPROTO_TCP)
const pj_uint16_t PJ_SOL_TCP	= IPPROTO_TCP;
#elif (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_WIN64) && PJ_WIN64)
const pj_uint16_t PJ_SOL_TCP	= IPPROTO_TCP;
#else
const pj_uint16_t PJ_SOL_TCP	= 6;
#endif /* SOL_TCP */

#ifdef SOL_UDP
const pj_uint16_t PJ_SOL_UDP	= SOL_UDP;
#elif defined(IPPROTO_UDP)
const pj_uint16_t PJ_SOL_UDP	= IPPROTO_UDP;
#elif (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_WIN64) && PJ_WIN64)
const pj_uint16_t PJ_SOL_UDP	= IPPROTO_UDP;
#else
const pj_uint16_t PJ_SOL_UDP	= 17;
#endif /* SOL_UDP */

#ifdef SOL_IPV6
const pj_uint16_t PJ_SOL_IPV6	= SOL_IPV6;
#elif (defined(PJ_WIN32) && PJ_WIN32) || (defined(PJ_WIN64) && PJ_WIN64)
#   if defined(IPPROTO_IPV6) || (_WIN32_WINNT >= 0x0501)
const pj_uint16_t PJ_SOL_IPV6	= IPPROTO_IPV6;
#   else
const pj_uint16_t PJ_SOL_IPV6	= 41;
#   endif
#else
const pj_uint16_t PJ_SOL_IPV6	= 41;
#endif /* SOL_IPV6 */

/* IP_TOS */
#ifdef IP_TOS
const pj_uint16_t PJ_IP_TOS	= IP_TOS;
#else
const pj_uint16_t PJ_IP_TOS	= 1;
#endif


/* TOS settings (declared in netinet/ip.h) */
#ifdef IPTOS_LOWDELAY
const pj_uint16_t PJ_IPTOS_LOWDELAY	= IPTOS_LOWDELAY;
#else
const pj_uint16_t PJ_IPTOS_LOWDELAY	= 0x10;
#endif
#ifdef IPTOS_THROUGHPUT
const pj_uint16_t PJ_IPTOS_THROUGHPUT	= IPTOS_THROUGHPUT;
#else
const pj_uint16_t PJ_IPTOS_THROUGHPUT	= 0x08;
#endif
#ifdef IPTOS_RELIABILITY
const pj_uint16_t PJ_IPTOS_RELIABILITY	= IPTOS_RELIABILITY;
#else
const pj_uint16_t PJ_IPTOS_RELIABILITY	= 0x04;
#endif
#ifdef IPTOS_MINCOST
const pj_uint16_t PJ_IPTOS_MINCOST	= IPTOS_MINCOST;
#else
const pj_uint16_t PJ_IPTOS_MINCOST	= 0x02;
#endif


/* optname values. */
const pj_uint16_t PJ_SO_TYPE    = SO_TYPE;
const pj_uint16_t PJ_SO_RCVBUF  = SO_RCVBUF;
const pj_uint16_t PJ_SO_SNDBUF  = SO_SNDBUF;
const pj_uint16_t PJ_TCP_NODELAY= TCP_NODELAY;
const pj_uint16_t PJ_SO_REUSEADDR= SO_REUSEADDR;
#ifdef SO_NOSIGPIPE
const pj_uint16_t PJ_SO_NOSIGPIPE = SO_NOSIGPIPE;
#else
const pj_uint16_t PJ_SO_NOSIGPIPE = 0xFFFF;
#endif
#if defined(SO_PRIORITY)
const pj_uint16_t PJ_SO_PRIORITY = SO_PRIORITY;
#else
/* This is from Linux, YMMV */
const pj_uint16_t PJ_SO_PRIORITY = 12;
#endif

/* Multicasting is not supported e.g. in PocketPC 2003 SDK */
#ifdef IP_MULTICAST_IF
const pj_uint16_t PJ_IP_MULTICAST_IF    = IP_MULTICAST_IF;
const pj_uint16_t PJ_IP_MULTICAST_TTL   = IP_MULTICAST_TTL;
const pj_uint16_t PJ_IP_MULTICAST_LOOP  = IP_MULTICAST_LOOP;
const pj_uint16_t PJ_IP_ADD_MEMBERSHIP  = IP_ADD_MEMBERSHIP;
const pj_uint16_t PJ_IP_DROP_MEMBERSHIP = IP_DROP_MEMBERSHIP;
#else
const pj_uint16_t PJ_IP_MULTICAST_IF    = 0xFFFF;
const pj_uint16_t PJ_IP_MULTICAST_TTL   = 0xFFFF;
const pj_uint16_t PJ_IP_MULTICAST_LOOP  = 0xFFFF;
const pj_uint16_t PJ_IP_ADD_MEMBERSHIP  = 0xFFFF;
const pj_uint16_t PJ_IP_DROP_MEMBERSHIP = 0xFFFF;
#endif

/* recv() and send() flags */
const int PJ_MSG_OOB		= MSG_OOB;
const int PJ_MSG_PEEK		= MSG_PEEK;
const int PJ_MSG_DONTROUTE	= MSG_DONTROUTE;



using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Networking;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;


ref class PjUwpSocketDatagramRecvHelper sealed
{
internal:
    PjUwpSocketDatagramRecvHelper(PjUwpSocket* uwp_sock_) :
	uwp_sock(uwp_sock_), avail_data_len(0), recv_args(nullptr)
    {
	recv_wait = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	event_token = uwp_sock->datagram_sock->MessageReceived += 
	    ref new TypedEventHandler<DatagramSocket^, 
				      DatagramSocketMessageReceivedEventArgs^>
		    (this, &PjUwpSocketDatagramRecvHelper::OnMessageReceived);
    }

    void OnMessageReceived(DatagramSocket ^sender,
			   DatagramSocketMessageReceivedEventArgs ^args)
    {
	try {
	    recv_args = args;
	    avail_data_len = args->GetDataReader()->UnconsumedBufferLength;

	    // Notify application asynchronously
	    concurrency::create_task([this]()
	    {
		if (uwp_sock->on_read) {
		    if (!pj_thread_is_registered())
			pj_thread_register("MsgReceive", thread_desc, 
					   &rec_thread);

		    (tp)(*uwp_sock->read_userdata)
		    (*uwp_sock->on_read)(uwp_sock, avail_data_len);
		}
	    });

	    WaitForSingleObjectEx(recv_wait, INFINITE, false);
	} catch (Exception^ e) {}
    }

    pj_status_t ReadDataIfAvailable(void *buf, pj_ssize_t *len,
				    pj_sockaddr_t *from)
    {
	if (avail_data_len <= 0)
	    return PJ_ENOTFOUND;

	if (*len < avail_data_len)
	    return PJ_ETOOSMALL;

	// Read data
	auto reader = recv_args->GetDataReader();
	auto buffer = reader->ReadBuffer(avail_data_len);
	unsigned char *p;
	GetRawBufferFromIBuffer(buffer, &p);
	pj_memcpy((void*) buf, p, avail_data_len);
	*len = avail_data_len;

	// Get source address
	wstr_addr_to_sockaddr(recv_args->RemoteAddress->CanonicalName->Data(),
			      recv_args->RemotePort->Data(), from);

	// finally
	avail_data_len = 0;
	SetEvent(recv_wait);

	return PJ_SUCCESS;
    }

private:

    ~PjUwpSocketDatagramRecvHelper()
    {
	if (uwp_sock->datagram_sock)
	    uwp_sock->datagram_sock->MessageReceived -= event_token;

	SetEvent(recv_wait);
	CloseHandle(recv_wait);
    }

    PjUwpSocket* uwp_sock;
    DatagramSocketMessageReceivedEventArgs^ recv_args;
    EventRegistrationToken event_token;
    HANDLE recv_wait;
    int avail_data_len;
    pj_thread_desc thread_desc;
    pj_thread_t *rec_thread;
};


ref class PjUwpSocketListenerHelper sealed
{
internal:
    PjUwpSocketListenerHelper(PjUwpSocket* uwp_sock_) :
	uwp_sock(uwp_sock_), conn_args(nullptr)
    {
	conn_wait = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
	event_token = uwp_sock->listener_sock->ConnectionReceived +=
	    ref new TypedEventHandler<StreamSocketListener^, StreamSocketListenerConnectionReceivedEventArgs^>
	    (this, &PjUwpSocketListenerHelper::OnConnectionReceived);
    }

    void OnConnectionReceived(StreamSocketListener ^sender,
	StreamSocketListenerConnectionReceivedEventArgs ^args)
    {
	try {
	    conn_args = args;

	    // Notify application asynchronously
	    concurrency::create_task([this]()
	    {
		if (uwp_sock->on_accept) {
		    if (!pj_thread_is_registered())
			pj_thread_register("ConnReceive", thread_desc, 
					   &listener_thread);

		    (*uwp_sock->on_accept)(uwp_sock, PJ_SUCCESS);
		}
	    });

	    WaitForSingleObjectEx(conn_wait, INFINITE, false);
	} catch (Exception^ e) {}
    }

    pj_status_t GetAcceptedSocket(StreamSocket^ stream_sock)
    {
	if (conn_args == nullptr)
	    return PJ_ENOTFOUND;

	stream_sock = conn_args->Socket;

	// finally
	conn_args = nullptr;
	SetEvent(conn_wait);

	return PJ_SUCCESS;
    }

private:

    ~PjUwpSocketListenerHelper()
    {
	if (uwp_sock->listener_sock)
	    uwp_sock->listener_sock->ConnectionReceived -= event_token;

	SetEvent(conn_wait);
	CloseHandle(conn_wait);
    }

    PjUwpSocket* uwp_sock;
    StreamSocketListenerConnectionReceivedEventArgs^ conn_args;
    EventRegistrationToken event_token;
    HANDLE conn_wait;

    pj_thread_desc thread_desc;
    pj_thread_t *listener_thread;
};


PjUwpSocket::PjUwpSocket(int af_, int type_, int proto_) :
    af(af_), type(type_), proto(proto_),
    sock_type(SOCKTYPE_UNKNOWN), sock_state(SOCKSTATE_NULL),
    is_blocking(PJ_TRUE), is_busy_sending(PJ_FALSE)
{
    pj_sockaddr_init(pj_AF_INET(), &local_addr, NULL, 0);
    pj_sockaddr_init(pj_AF_INET(), &remote_addr, NULL, 0);
}

PjUwpSocket::~PjUwpSocket()
{}

PjUwpSocket* PjUwpSocket::CreateAcceptSocket(Windows::Networking::Sockets::StreamSocket^ stream_sock_)
{
    PjUwpSocket *new_sock = new PjUwpSocket(pj_AF_INET(), pj_SOCK_STREAM(), 0);
    new_sock->stream_sock = stream_sock_;
    new_sock->sock_type = SOCKTYPE_STREAM;
    new_sock->sock_state = SOCKSTATE_CONNECTED;
    new_sock->socket_reader = ref new DataReader(new_sock->stream_sock->InputStream);
    new_sock->socket_writer = ref new DataWriter(new_sock->stream_sock->OutputStream);
    new_sock->send_buffer = ref new Buffer(SEND_BUFFER_SIZE);
    new_sock->is_blocking = is_blocking;

    // Update local & remote address
    wstr_addr_to_sockaddr(stream_sock_->Information->RemoteAddress->CanonicalName->Data(),
			  stream_sock_->Information->RemotePort->Data(),
			  &new_sock->remote_addr);
    wstr_addr_to_sockaddr(stream_sock_->Information->LocalAddress->CanonicalName->Data(),
			  stream_sock_->Information->LocalPort->Data(),
			  &new_sock->local_addr);

    return new_sock;
}


pj_status_t PjUwpSocket::InitSocket(enum PjUwpSocketType sock_type_)
{
    PJ_ASSERT_RETURN(sock_type_ > SOCKTYPE_UNKNOWN, PJ_EINVAL);

    sock_type = sock_type_;
    if (sock_type == SOCKTYPE_LISTENER) {
	listener_sock = ref new StreamSocketListener();
    } else if (sock_type == SOCKTYPE_STREAM) {
	stream_sock = ref new StreamSocket();
    } else if (sock_type == SOCKTYPE_DATAGRAM) {
	datagram_sock = ref new DatagramSocket();
    } else {
	pj_assert(!"Invalid socket type");
	return PJ_EINVAL;
    }

    if (sock_type == SOCKTYPE_DATAGRAM || sock_type == SOCKTYPE_STREAM) {
	send_buffer = ref new Buffer(SEND_BUFFER_SIZE);
    }

    sock_state = SOCKSTATE_INITIALIZED;

    return PJ_SUCCESS;
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
    return ntohs(netshort);
}

/*
 * Convert 16-bit value from host byte order to network byte order.
 */
PJ_DEF(pj_uint16_t) pj_htons(pj_uint16_t hostshort)
{
    return htons(hostshort);
}

/*
 * Convert 32-bit value from network byte order to host byte order.
 */
PJ_DEF(pj_uint32_t) pj_ntohl(pj_uint32_t netlong)
{
    return ntohl(netlong);
}

/*
 * Convert 32-bit value from host byte order to network byte order.
 */
PJ_DEF(pj_uint32_t) pj_htonl(pj_uint32_t hostlong)
{
    return htonl(hostlong);
}

/*
 * Convert an Internet host address given in network byte order
 * to string in standard numbers and dots notation.
 */
PJ_DEF(char*) pj_inet_ntoa(pj_in_addr inaddr)
{
    return inet_ntoa(*(struct in_addr*)&inaddr);
}

/*
 * This function converts the Internet host address cp from the standard
 * numbers-and-dots notation into binary data and stores it in the structure
 * that inp points to. 
 */
PJ_DEF(int) pj_inet_aton(const pj_str_t *cp, struct pj_in_addr *inp)
{
    char tempaddr[PJ_INET_ADDRSTRLEN];

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
    if (cp->slen >= PJ_INET_ADDRSTRLEN) {
	return 0;
    }

    pj_memcpy(tempaddr, cp->ptr, cp->slen);
    tempaddr[cp->slen] = '\0';

#if defined(PJ_SOCK_HAS_INET_ATON) && PJ_SOCK_HAS_INET_ATON != 0
    return inet_aton(tempaddr, (struct in_addr*)inp);
#else
    inp->s_addr = inet_addr(tempaddr);
    return inp->s_addr == PJ_INADDR_NONE ? 0 : 1;
#endif
}

/*
 * Convert text to IPv4/IPv6 address.
 */
PJ_DEF(pj_status_t) pj_inet_pton(int af, const pj_str_t *src, void *dst)
{
    char tempaddr[PJ_INET6_ADDRSTRLEN];

    PJ_ASSERT_RETURN(af == PJ_AF_INET || af == PJ_AF_INET6, PJ_EAFNOTSUP);
    PJ_ASSERT_RETURN(src && src->slen && dst, PJ_EINVAL);

    /* Initialize output with PJ_IN_ADDR_NONE for IPv4 (to be
    * compatible with pj_inet_aton()
    */
    if (af == PJ_AF_INET) {
	((pj_in_addr*)dst)->s_addr = PJ_INADDR_NONE;
    }

    /* Caution:
    *	this function might be called with cp->slen >= 46
    *  (i.e. when called with hostname to check if it's an IP addr).
    */
    if (src->slen >= PJ_INET6_ADDRSTRLEN) {
	return PJ_ENAMETOOLONG;
    }

    pj_memcpy(tempaddr, src->ptr, src->slen);
    tempaddr[src->slen] = '\0';

#if defined(PJ_SOCK_HAS_INET_PTON) && PJ_SOCK_HAS_INET_PTON != 0
    /*
    * Implementation using inet_pton()
    */
    if (inet_pton(af, tempaddr, dst) != 1) {
	pj_status_t status = pj_get_netos_error();
	if (status == PJ_SUCCESS)
	    status = PJ_EUNKNOWN;

	return status;
    }

    return PJ_SUCCESS;

#elif defined(PJ_WIN32) || defined(PJ_WIN64) || defined(PJ_WIN32_WINCE)
    /*
    * Implementation on Windows, using WSAStringToAddress().
    * Should also work on Unicode systems.
    */
    {
	PJ_DECL_UNICODE_TEMP_BUF(wtempaddr, PJ_INET6_ADDRSTRLEN)
	    pj_sockaddr sock_addr;
	int addr_len = sizeof(sock_addr);
	int rc;

	sock_addr.addr.sa_family = (pj_uint16_t)af;
	rc = WSAStringToAddress(
	    PJ_STRING_TO_NATIVE(tempaddr, wtempaddr, sizeof(wtempaddr)),
	    af, NULL, (LPSOCKADDR)&sock_addr, &addr_len);
	if (rc != 0) {
	    /* If you get rc 130022 Invalid argument (WSAEINVAL) with IPv6,
	    * check that you have IPv6 enabled (install it in the network
	    * adapter).
	    */
	    pj_status_t status = pj_get_netos_error();
	    if (status == PJ_SUCCESS)
		status = PJ_EUNKNOWN;

	    return status;
	}

	if (sock_addr.addr.sa_family == PJ_AF_INET) {
	    pj_memcpy(dst, &sock_addr.ipv4.sin_addr, 4);
	    return PJ_SUCCESS;
	}
	else if (sock_addr.addr.sa_family == PJ_AF_INET6) {
	    pj_memcpy(dst, &sock_addr.ipv6.sin6_addr, 16);
	    return PJ_SUCCESS;
	}
	else {
	    pj_assert(!"Shouldn't happen");
	    return PJ_EBUG;
	}
    }
#elif !defined(PJ_HAS_IPV6) || PJ_HAS_IPV6==0
    /* IPv6 support is disabled, just return error without raising assertion */
    return PJ_EIPV6NOTSUP;
#else
    pj_assert(!"Not supported");
    return PJ_EIPV6NOTSUP;
#endif
}

/*
 * Convert IPv4/IPv6 address to text.
 */
PJ_DEF(pj_status_t) pj_inet_ntop(int af, const void *src,
				 char *dst, int size)

{
    PJ_ASSERT_RETURN(src && dst && size, PJ_EINVAL);
    PJ_ASSERT_RETURN(af == PJ_AF_INET || af == PJ_AF_INET6, PJ_EAFNOTSUP);

    *dst = '\0';

#if defined(PJ_SOCK_HAS_INET_NTOP) && PJ_SOCK_HAS_INET_NTOP != 0
    /*
    * Implementation using inet_ntop()
    */
    if (inet_ntop(af, src, dst, size) == NULL) {
	pj_status_t status = pj_get_netos_error();
	if (status == PJ_SUCCESS)
	    status = PJ_EUNKNOWN;

	return status;
    }

    return PJ_SUCCESS;

#elif defined(PJ_WIN32) || defined(PJ_WIN64) || defined(PJ_WIN32_WINCE)
    /*
    * Implementation on Windows, using WSAAddressToString().
    * Should also work on Unicode systems.
    */
    {
	PJ_DECL_UNICODE_TEMP_BUF(wtempaddr, PJ_INET6_ADDRSTRLEN)
	    pj_sockaddr sock_addr;
	DWORD addr_len, addr_str_len;
	int rc;

	pj_bzero(&sock_addr, sizeof(sock_addr));
	sock_addr.addr.sa_family = (pj_uint16_t)af;
	if (af == PJ_AF_INET) {
	    if (size < PJ_INET_ADDRSTRLEN)
		return PJ_ETOOSMALL;
	    pj_memcpy(&sock_addr.ipv4.sin_addr, src, 4);
	    addr_len = sizeof(pj_sockaddr_in);
	    addr_str_len = PJ_INET_ADDRSTRLEN;
	}
	else if (af == PJ_AF_INET6) {
	    if (size < PJ_INET6_ADDRSTRLEN)
		return PJ_ETOOSMALL;
	    pj_memcpy(&sock_addr.ipv6.sin6_addr, src, 16);
	    addr_len = sizeof(pj_sockaddr_in6);
	    addr_str_len = PJ_INET6_ADDRSTRLEN;
	}
	else {
	    pj_assert(!"Unsupported address family");
	    return PJ_EAFNOTSUP;
	}

#if PJ_NATIVE_STRING_IS_UNICODE
	rc = WSAAddressToString((LPSOCKADDR)&sock_addr, addr_len,
	    NULL, wtempaddr, &addr_str_len);
	if (rc == 0) {
	    pj_unicode_to_ansi(wtempaddr, wcslen(wtempaddr), dst, size);
	}
#else
	rc = WSAAddressToString((LPSOCKADDR)&sock_addr, addr_len,
	    NULL, dst, &addr_str_len);
#endif

	if (rc != 0) {
	    pj_status_t status = pj_get_netos_error();
	    if (status == PJ_SUCCESS)
		status = PJ_EUNKNOWN;

	    return status;
	}

	return PJ_SUCCESS;
    }

#elif !defined(PJ_HAS_IPV6) || PJ_HAS_IPV6==0
    /* IPv6 support is disabled, just return error without raising assertion */
    return PJ_EIPV6NOTSUP;
#else
    pj_assert(!"Not supported");
    return PJ_EIPV6NOTSUP;
#endif
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
	hostname.ptr = buf;
	if (gethostname(buf, sizeof(buf)) != 0) {
	    hostname.ptr[0] = '\0';
	    hostname.slen = 0;
	}
	else {
	    hostname.slen = strlen(buf);
	}
    }
    return &hostname;
}

/*
 * Create new socket/endpoint for communication and returns a descriptor.
 */
PJ_DEF(pj_status_t) pj_sock_socket(int af, 
				   int type, 
				   int proto,
				   pj_sock_t *p_sock)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(p_sock!=NULL, PJ_EINVAL);

    PjUwpSocket *s = new PjUwpSocket(af, type, proto);
    
    /* Init UDP socket here */
    if (type == pj_SOCK_DGRAM()) {
	s->InitSocket(SOCKTYPE_DATAGRAM);
    }

    *p_sock = (pj_sock_t)s;
    return PJ_SUCCESS;
}


/*
 * Bind socket.
 */
PJ_DEF(pj_status_t) pj_sock_bind( pj_sock_t sock, 
				  const pj_sockaddr_t *addr,
				  int len)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(sock, PJ_EINVAL);
    PJ_ASSERT_RETURN(addr && len>=(int)sizeof(pj_sockaddr_in), PJ_EINVAL);

    PjUwpSocket *s = (PjUwpSocket*)sock;

    if (s->sock_state > SOCKSTATE_INITIALIZED)
	return PJ_EINVALIDOP;

    pj_sockaddr_cp(&s->local_addr, addr);

    /* Bind now if this is UDP. But if it is TCP, unfortunately we don't
     * know yet whether it is SocketStream or Listener!
     */
    if (s->type == pj_SOCK_DGRAM()) {
	HRESULT err = 0;

	try {
	    concurrency::create_task([s, addr]() {
		HostName ^hostname;
		int port;
		sockaddr_to_hostname_port(addr, hostname, &port);
		if (pj_sockaddr_has_addr(addr)) {
		    s->datagram_sock->BindEndpointAsync(hostname, 
							port.ToString());
		} else if (pj_sockaddr_get_port(addr) != 0) {
		    s->datagram_sock->BindServiceNameAsync(port.ToString());
		}
	    }).then([s, &err](concurrency::task<void> t)
	    {
		try {
		    t.get();
		    s->sock_state = SOCKSTATE_CONNECTED;
		} catch (Exception^ e) {
		    err = e->HResult;
		}
	    }).get();
	} catch (Exception^ e) {
	    err = e->HResult;
	}

	return (err? PJ_RETURN_OS_ERROR(err) : PJ_SUCCESS);
    }

    return PJ_SUCCESS;
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
    PJ_ASSERT_RETURN(sock, PJ_EINVAL);

    if (sock == PJ_INVALID_SOCKET)
	return PJ_SUCCESS;

    PjUwpSocket *s = (PjUwpSocket*)sock;
    delete s;

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
		     *namelen>=(int)sizeof(pj_sockaddr_in), PJ_EINVAL);

    PjUwpSocket *s = (PjUwpSocket*)sock;

    pj_sockaddr_cp(addr, &s->remote_addr);
    *namelen = pj_sockaddr_get_len(&s->remote_addr);

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
		     *namelen>=(int)sizeof(pj_sockaddr_in), PJ_EINVAL);

    PjUwpSocket *s = (PjUwpSocket*)sock;

    pj_sockaddr_cp(addr, &s->local_addr);
    *namelen = pj_sockaddr_get_len(&s->local_addr);

    return PJ_SUCCESS;
}


static pj_status_t sock_send_imp(PjUwpSocket *s, const void *buf,
				 pj_ssize_t *len)
{
    if (s->is_busy_sending)
	return PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK);

    if (*len > (pj_ssize_t)s->send_buffer->Capacity)
	return PJ_ETOOBIG;

    CopyToIBuffer((unsigned char*)buf, *len, s->send_buffer);
    s->send_buffer->Length = *len;
    s->socket_writer->WriteBuffer(s->send_buffer);

    if (s->is_blocking) {
	pj_status_t status = PJ_SUCCESS;
	concurrency::cancellation_token_source cts;
	auto cts_token = cts.get_token();
	auto t = concurrency::create_task(s->socket_writer->StoreAsync(),
					  cts_token);
	*len = cancel_after_timeout(t, cts, (unsigned int)WRITE_TIMEOUT).
	    then([cts_token, &status](concurrency::task<unsigned int> t_)
	{
	    int sent = 0;
	    try {
		if (cts_token.is_canceled())
		    status = PJ_ETIMEDOUT;
		else
		    sent = t_.get();
	    } catch (Exception^ e) {
		status = PJ_RETURN_OS_ERROR(e->HResult);
	    }
	    return sent;
	}).get();

	return status;
    } 

    s->is_busy_sending = true;
    concurrency::create_task(s->socket_writer->StoreAsync()).
	then([s](concurrency::task<unsigned int> t_)
    {
	try {
	    unsigned int l = t_.get();
	    s->is_busy_sending = false;

	    // invoke callback
	    if (s->on_write) {
		(*s->on_write)(s, l);
	    }
	} catch (Exception^ e) {
	    s->is_busy_sending = false;
	    if (s->sock_type == SOCKTYPE_STREAM)
		s->sock_state = SOCKSTATE_DISCONNECTED;

	    // invoke callback
	    if (s->on_write) {
		(*s->on_write)(s, -PJ_RETURN_OS_ERROR(e->HResult));
	    }
	}
    });

    return PJ_EPENDING;
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
    PJ_UNUSED_ARG(flags);

    PjUwpSocket *s = (PjUwpSocket*)sock;

    if ((s->sock_type!=SOCKTYPE_STREAM && s->sock_type!=SOCKTYPE_DATAGRAM) ||
	(s->sock_state!=SOCKSTATE_CONNECTED))
    {
	return PJ_EINVALIDOP;
    }
 
    /* Sending for SOCKTYPE_DATAGRAM is implemented in pj_sock_sendto() */
    if (s->sock_type == SOCKTYPE_DATAGRAM) {
	return pj_sock_sendto(sock, buf, len, flags, &s->remote_addr,
			      pj_sockaddr_get_len(&s->remote_addr));
    }

    return sock_send_imp(s, buf, len);
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
    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(tolen);
    
    PjUwpSocket *s = (PjUwpSocket*)sock;

    if (s->sock_type != SOCKTYPE_DATAGRAM ||
	s->sock_state < SOCKSTATE_INITIALIZED)
    {
	return PJ_EINVALIDOP;
    }

    if (s->is_busy_sending)
	return PJ_STATUS_FROM_OS(OSERR_EWOULDBLOCK);

    if (*len > (pj_ssize_t)s->send_buffer->Capacity)
	return PJ_ETOOBIG;

    HostName ^hostname;
    int port;
    sockaddr_to_hostname_port(to, hostname, &port);

    concurrency::cancellation_token_source cts;
    auto cts_token = cts.get_token();
    auto t = concurrency::create_task(
		s->datagram_sock->GetOutputStreamAsync(
		    hostname, port.ToString()), cts_token);
    pj_status_t status = PJ_SUCCESS;

    cancel_after_timeout(t, cts, (unsigned int)WRITE_TIMEOUT).
	then([s, cts_token, &status](concurrency::task<IOutputStream^> t_)
    {
	try {
	    if (cts_token.is_canceled()) {
		status = PJ_ETIMEDOUT;
	    } else {
		IOutputStream^ outstream = t_.get();
		s->socket_writer = ref new DataWriter(outstream);
	    }
	} catch (Exception^ e) {
	    status = PJ_RETURN_OS_ERROR(e->HResult);
	}
    }).get();

    if (status != PJ_SUCCESS)
	return status;

    status = sock_send_imp(s, buf, len);

    if ((status == PJ_SUCCESS || status == PJ_EPENDING) &&
	s->sock_state < SOCKSTATE_CONNECTED)
    {
	s->sock_state = SOCKSTATE_CONNECTED;
    }

    return status;
}


static int consume_read_buffer(PjUwpSocket *s, void *buf, int max_len)
{
    if (s->socket_reader->UnconsumedBufferLength == 0)
	return 0;

    int read_len = PJ_MIN((int)s->socket_reader->UnconsumedBufferLength,
			  max_len);
    IBuffer^ buffer = s->socket_reader->ReadBuffer(read_len);
    read_len = buffer->Length;
    CopyFromIBuffer((unsigned char*)buf, read_len, buffer);
    return read_len;
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
    PJ_ASSERT_RETURN(sock && len && *len > 0, PJ_EINVAL);
    
    PJ_UNUSED_ARG(flags);

    PjUwpSocket *s = (PjUwpSocket*)sock;

    /* Only for TCP, at least for now! */
    if (s->sock_type == SOCKTYPE_DATAGRAM)
	return PJ_ENOTSUP;

    if (s->sock_type != SOCKTYPE_STREAM ||
	s->sock_state != SOCKSTATE_CONNECTED)
    {
	return PJ_EINVALIDOP;
    }

    /* First check if there is already some data in the read buffer */
    int avail_len = consume_read_buffer(s, buf, *len);
    if (avail_len > 0) {
	*len = avail_len;
	return PJ_SUCCESS;
    }

    /* Start sync read */
    if (s->is_blocking) {
	pj_status_t status = PJ_SUCCESS;
	concurrency::cancellation_token_source cts;
	auto cts_token = cts.get_token();
	auto t = concurrency::create_task(s->socket_reader->LoadAsync(*len), cts_token);
	*len = cancel_after_timeout(t, cts, READ_TIMEOUT)
		    .then([s, len, buf, cts_token, &status](concurrency::task<unsigned int> t_)
	{
	    try {
		if (cts_token.is_canceled()) {
		    status = PJ_ETIMEDOUT;
		    return 0;
		}
		t_.get();
	    } catch (Exception^) {
		status = PJ_ETIMEDOUT;
		return 0;
	    }

	    *len = consume_read_buffer(s, buf, *len);
	    return (int)*len;
	}).get();

	return status;
    }

    /* Start async read */
    int read_len = *len;
    concurrency::create_task(s->socket_reader->LoadAsync(read_len))
	.then([s, &read_len](concurrency::task<unsigned int> t_)
    {
	try {
	    // catch any exception
	    t_.get();

	    // invoke callback
	    read_len = PJ_MIN((int)s->socket_reader->UnconsumedBufferLength,
			      read_len);
	    if (read_len > 0 && s->on_read) {
		(*s->on_read)(s, read_len);
	    }
	} catch (Exception^ e) {
	    // invoke callback
	    if (s->on_read) {
		(*s->on_read)(s, -PJ_RETURN_OS_ERROR(e->HResult));
	    }
	    return 0;
	}

	return (int)read_len;
    });

    return PJ_EPENDING;
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
    PJ_ASSERT_RETURN(*fromlen >= (int)sizeof(pj_sockaddr_in), PJ_EINVAL);

    PJ_UNUSED_ARG(flags);

    PjUwpSocket *s = (PjUwpSocket*)sock;

    if (s->sock_type != SOCKTYPE_DATAGRAM ||
	s->sock_state < SOCKSTATE_INITIALIZED)
    {
	return PJ_EINVALIDOP;
    }

    /* Start receive, if not yet */
    if (s->datagram_recv_helper == nullptr) {
	s->datagram_recv_helper = ref new PjUwpSocketDatagramRecvHelper(s);
    }

    /* Try to read any available data first */
    pj_status_t status = s->datagram_recv_helper->
					ReadDataIfAvailable(buf, len, from);
    if (status != PJ_ENOTFOUND)
	return status;

    /* Start sync read */
    if (s->is_blocking) {
	while (status == PJ_ENOTFOUND && s->sock_state <= SOCKSTATE_CONNECTED)
	{
	    status = s->datagram_recv_helper->
					ReadDataIfAvailable(buf, len, from);
	    pj_thread_sleep(100);
	}
	return PJ_SUCCESS;
    }

    /* For async read, just return PJ_EPENDING */
    return PJ_EPENDING;
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
    return PJ_ENOTSUP;
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
    return PJ_ENOTSUP;
}


/*
* Set socket option.
*/
PJ_DEF(pj_status_t) pj_sock_setsockopt_params( pj_sock_t sockfd,
    const pj_sockopt_params *params)
{
    unsigned int i = 0;
    pj_status_t retval = PJ_SUCCESS;
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(params, PJ_EINVAL);

    for (;i<params->cnt && i<PJ_MAX_SOCKOPT_PARAMS;++i) {
	pj_status_t status = pj_sock_setsockopt(sockfd, 
				    (pj_uint16_t)params->options[i].level,
				    (pj_uint16_t)params->options[i].optname,
				    params->options[i].optval, 
				    params->options[i].optlen);
	if (status != PJ_SUCCESS) {
	    retval = status;
	    PJ_PERROR(4,(THIS_FILE, status,
			 "Warning: error applying sock opt %d",
			 params->options[i].optname));
	}
    }

    return retval;
}

static pj_status_t tcp_bind(PjUwpSocket *s)
{
    /* If no bound address is set, just return */
    if (!pj_sockaddr_has_addr(&s->local_addr) &&
	pj_sockaddr_get_port(&s->local_addr)==0)
    {
	return PJ_SUCCESS;
    }

    HRESULT err = 0;    

    try {
	concurrency::create_task([s]() {
	    HostName ^hostname;
	    int port;
	    sockaddr_to_hostname_port(&s->local_addr, hostname, &port);

	    s->listener_sock->BindEndpointAsync(hostname, 
						port.ToString());
	}).then([s, &err](concurrency::task<void> t)
	{
	    try {
		t.get();
		s->sock_state = SOCKSTATE_CONNECTED;
	    } catch (Exception^ e) {
		err = e->HResult;
	    }
	}).get();
    } catch (Exception^ e) {
	err = e->HResult;
    }

    return (err? PJ_RETURN_OS_ERROR(err) : PJ_SUCCESS);
}


/*
 * Connect socket.
 */
PJ_DEF(pj_status_t) pj_sock_connect( pj_sock_t sock,
				     const pj_sockaddr_t *addr,
				     int namelen)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(sock && addr, PJ_EINVAL);

    PJ_UNUSED_ARG(namelen);

    PjUwpSocket *s = (PjUwpSocket*)sock;
    pj_status_t status;

    pj_sockaddr_cp(&s->remote_addr, addr);

    /* UDP */

    if (s->sock_type == SOCKTYPE_DATAGRAM) {
	if (s->sock_state != SOCKSTATE_INITIALIZED)
	    return PJ_EINVALIDOP;
	
	HostName ^hostname;
	int port;
	sockaddr_to_hostname_port(addr, hostname, &port);

	try {
	    concurrency::create_task(s->datagram_sock->ConnectAsync
						   (hostname, port.ToString()))
		.then([s](concurrency::task<void> t_)
	    {
		try {
		    t_.get();
		} catch (Exception^ ex) 
		{
		
		}
	    }).get();
	} catch (Exception^) {
	    return PJ_EUNKNOWN;
	}

	// Update local & remote address
	wstr_addr_to_sockaddr(s->datagram_sock->Information->RemoteAddress->CanonicalName->Data(),
			      s->datagram_sock->Information->RemotePort->Data(),
			      &s->remote_addr);
	wstr_addr_to_sockaddr(s->datagram_sock->Information->LocalAddress->CanonicalName->Data(),
			      s->datagram_sock->Information->LocalPort->Data(),
			      &s->local_addr);

	s->sock_state = SOCKSTATE_CONNECTED;
	
	return PJ_SUCCESS;
    }

    /* TCP */

    /* Init stream socket now */
    s->InitSocket(SOCKTYPE_STREAM);

    pj_sockaddr_cp(&s->remote_addr, addr);
    wstr_addr_to_sockaddr(s->stream_sock->Information->LocalAddress->CanonicalName->Data(),
			  s->stream_sock->Information->LocalPort->Data(),
			  &s->local_addr);

    /* Perform any pending bind */
    status = tcp_bind(s);
    if (status != PJ_SUCCESS)
	return status;

    char tmp[PJ_INET6_ADDRSTRLEN];
    wchar_t wtmp[PJ_INET6_ADDRSTRLEN];
    pj_sockaddr_print(addr, tmp, PJ_INET6_ADDRSTRLEN, 0);
    pj_ansi_to_unicode(tmp, pj_ansi_strlen(tmp), wtmp,
		       PJ_INET6_ADDRSTRLEN);
    auto host = ref new HostName(ref new String(wtmp));
    int port = pj_sockaddr_get_port(addr);

    auto t = concurrency::create_task(s->stream_sock->ConnectAsync
	     (host, port.ToString(), SocketProtectionLevel::PlainSocket))
	     .then([=](concurrency::task<void> t_)
    {
	try {
	    t_.get();
	    s->socket_reader = ref new DataReader(s->stream_sock->InputStream);
	    s->socket_writer = ref new DataWriter(s->stream_sock->OutputStream);

	    // Update local & remote address
	    wstr_addr_to_sockaddr(s->stream_sock->Information->RemoteAddress->CanonicalName->Data(),
				  s->stream_sock->Information->RemotePort->Data(),
				  &s->remote_addr);
	    wstr_addr_to_sockaddr(s->stream_sock->Information->LocalAddress->CanonicalName->Data(),
				  s->stream_sock->Information->LocalPort->Data(),
				  &s->local_addr);

	    s->sock_state = SOCKSTATE_CONNECTED;

	    if (!s->is_blocking && s->on_connect) {
		(*s->on_connect)(s, PJ_SUCCESS);
	    }
	    return (pj_status_t)PJ_SUCCESS;
	} catch (Exception^ ex) {
	    SocketErrorStatus status = SocketError::GetStatus(ex->HResult);

	    switch (status)
	    {
	    case SocketErrorStatus::UnreachableHost:
		break;
	    case SocketErrorStatus::ConnectionTimedOut:
		break;
	    case SocketErrorStatus::ConnectionRefused:
		break;
	    default:
		break;
	    }

	    if (!s->is_blocking && s->on_connect) {
		(*s->on_connect)(s, PJ_EUNKNOWN);
	    }

	    return (pj_status_t)PJ_EUNKNOWN;
	}
    });

    if (!s->is_blocking)
	return PJ_EPENDING;

    try {
	status = t.get();
    } catch (Exception^) {
	return PJ_EUNKNOWN;
    }
    return status;
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
    PJ_UNUSED_ARG(how);

    return pj_sock_close(sock);
}

/*
 * Start listening to incoming connections.
 */
PJ_DEF(pj_status_t) pj_sock_listen( pj_sock_t sock,
				    int backlog)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(backlog);
    PJ_ASSERT_RETURN(sock, PJ_EINVAL);

    PjUwpSocket *s = (PjUwpSocket*)sock;
    pj_status_t status;

    /* Init listener socket now */
    s->InitSocket(SOCKTYPE_LISTENER);

    /* Perform any pending bind */
    status = tcp_bind(s);
    if (status != PJ_SUCCESS)
	return status;

    /* Start listen */
    if (s->listener_helper == nullptr) {
	s->listener_helper = ref new PjUwpSocketListenerHelper(s);
    }

    return PJ_SUCCESS;
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

    PjUwpSocket *s = (PjUwpSocket*)serverfd;

    if (s->sock_type != SOCKTYPE_LISTENER ||
	s->sock_state != SOCKSTATE_INITIALIZED)
    {
	return PJ_EINVALIDOP;
    }

    StreamSocket^ accepted_sock;
    pj_status_t status = s->listener_helper->GetAcceptedSocket(accepted_sock);
    if (status == PJ_ENOTFOUND)
	return PJ_EPENDING;

    if (status != PJ_SUCCESS)
	return status;

    PjUwpSocket *new_sock = s->CreateAcceptSocket(accepted_sock);

    pj_sockaddr_cp(addr, &new_sock->remote_addr);
    *addrlen = pj_sockaddr_get_len(&new_sock->remote_addr);
    *newsock = (pj_sock_t)new_sock;

    return PJ_SUCCESS;
}
#endif	/* PJ_HAS_TCP */
