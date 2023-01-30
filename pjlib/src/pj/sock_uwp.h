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
#pragma once


#include <pj/assert.h>
#include <pj/sock.h>
#include <pj/string.h>
#include <pj/unicode.h>


enum {
    READ_TIMEOUT        = 60 * 1000,
    WRITE_TIMEOUT       = 60 * 1000,
    SEND_BUFFER_SIZE    = 128 * 1024,
};

enum PjUwpSocketType {
    SOCKTYPE_UNKNOWN, SOCKTYPE_LISTENER,
    SOCKTYPE_STREAM, SOCKTYPE_DATAGRAM
};

enum PjUwpSocketState {
    SOCKSTATE_NULL, SOCKSTATE_INITIALIZED, SOCKSTATE_CONNECTING,
    SOCKSTATE_CONNECTED, SOCKSTATE_DISCONNECTED, SOCKSTATE_ERROR
};

ref class PjUwpSocketDatagramRecvHelper;
ref class PjUwpSocketListenerHelper;
class PjUwpSocket;


typedef struct PjUwpSocketCallback
{
    void (*on_read)(PjUwpSocket *s, int bytes_read);
    void (*on_write)(PjUwpSocket *s, int bytes_sent);
    void (*on_accept)(PjUwpSocket *s);
    void (*on_connect)(PjUwpSocket *s, pj_status_t status);
} PjUwpSocketCallback;


/*
 * UWP Socket Wrapper.
 */
class PjUwpSocket
{
public:
    PjUwpSocket(int af_, int type_, int proto_);
    virtual ~PjUwpSocket();
    pj_status_t InitSocket(enum PjUwpSocketType sock_type_);
    void DeinitSocket();

    void* GetUserData() { return user_data; }
    void SetNonBlocking(const PjUwpSocketCallback *cb_, void *user_data_)
    {
        is_blocking = PJ_FALSE;
        cb=*cb_;
        user_data = user_data_;
    }

    enum PjUwpSocketType GetType() { return sock_type; }
    enum PjUwpSocketState GetState() { return sock_state; }

    pj_sockaddr* GetLocalAddr() { return &local_addr; }
    pj_sockaddr* GetRemoteAddr() { return &remote_addr; }


    pj_status_t Bind(const pj_sockaddr_t *addr = NULL);
    pj_status_t Send(const void *buf, pj_ssize_t *len);
    pj_status_t SendTo(const void *buf, pj_ssize_t *len, const pj_sockaddr_t *to);
    pj_status_t Recv(void *buf, pj_ssize_t *len);
    pj_status_t RecvFrom(void *buf, pj_ssize_t *len, pj_sockaddr_t *from);
    pj_status_t Connect(const pj_sockaddr_t *addr);
    pj_status_t Listen();
    pj_status_t Accept(PjUwpSocket **new_sock);

    void (*on_read)(PjUwpSocket *s, int bytes_read);
    void (*on_write)(PjUwpSocket *s, int bytes_sent);
    void (*on_accept)(PjUwpSocket *s, pj_status_t status);
    void (*on_connect)(PjUwpSocket *s, pj_status_t status);

private:
    PjUwpSocket* CreateAcceptSocket(Windows::Networking::Sockets::StreamSocket^ stream_sock_);
    pj_status_t SendImp(const void *buf, pj_ssize_t *len);
    int ConsumeReadBuffer(void *buf, int max_len);

    int af;
    int type;
    int proto;
    pj_sockaddr local_addr;
    pj_sockaddr remote_addr;
    pj_bool_t is_blocking;
    pj_bool_t has_pending_bind;
    pj_bool_t has_pending_send;
    pj_bool_t has_pending_recv;
    void *user_data;
    PjUwpSocketCallback cb;

    enum PjUwpSocketType sock_type;
    enum PjUwpSocketState sock_state;
    Windows::Networking::Sockets::DatagramSocket^ datagram_sock;
    Windows::Networking::Sockets::StreamSocket^ stream_sock;
    Windows::Networking::Sockets::StreamSocketListener^ listener_sock;
    
    /* Helper objects */
    PjUwpSocketDatagramRecvHelper^ dgram_recv_helper;
    PjUwpSocketListenerHelper^ listener_helper;

    Windows::Storage::Streams::DataReader^ socket_reader;
    Windows::Storage::Streams::DataWriter^ socket_writer;
    Windows::Storage::Streams::IBuffer^ send_buffer;

    friend PjUwpSocketDatagramRecvHelper;
    friend PjUwpSocketListenerHelper;
};


//////////////////////////////////
// Misc


inline pj_status_t wstr_addr_to_sockaddr(const wchar_t *waddr,
                                         const wchar_t *wport,
                                         pj_sockaddr_t *sockaddr)
{
#if 0
    char tmp_str_buf[PJ_INET6_ADDRSTRLEN+1];
    pj_assert(wcslen(waddr) < sizeof(tmp_str_buf));
    pj_unicode_to_ansi(waddr, wcslen(waddr), tmp_str_buf, sizeof(tmp_str_buf));
    pj_str_t remote_host;
    pj_strset(&remote_host, tmp_str_buf, pj_ansi_strlen(tmp_str_buf));
    pj_sockaddr_parse(pj_AF_UNSPEC(), 0, &remote_host, (pj_sockaddr*)sockaddr);
    pj_sockaddr_set_port((pj_sockaddr*)sockaddr,  (pj_uint16_t)_wtoi(wport));

    return PJ_SUCCESS;
#endif
    char tmp_str_buf[PJ_INET6_ADDRSTRLEN+1];
    pj_assert(wcslen(waddr) < sizeof(tmp_str_buf));
    pj_unicode_to_ansi(waddr, wcslen(waddr), tmp_str_buf, sizeof(tmp_str_buf));
    pj_str_t remote_host;
    pj_strset(&remote_host, tmp_str_buf, pj_ansi_strlen(tmp_str_buf));
    pj_sockaddr *addr = (pj_sockaddr*)sockaddr;
    pj_bool_t got_addr = PJ_FALSE;

    if (pj_inet_pton(PJ_AF_INET, &remote_host, &addr->ipv4.sin_addr)
        == PJ_SUCCESS)
    {
        addr->addr.sa_family = PJ_AF_INET;
        got_addr = PJ_TRUE;
    } else if (pj_inet_pton(PJ_AF_INET6, &remote_host, &addr->ipv6.sin6_addr)
        == PJ_SUCCESS)
    {
        addr->addr.sa_family = PJ_AF_INET6;
        got_addr = PJ_TRUE;
    }
    if (!got_addr)
        return PJ_EINVAL;

    pj_sockaddr_set_port(addr, (pj_uint16_t)_wtoi(wport));
    return PJ_SUCCESS;
}


inline pj_status_t sockaddr_to_hostname_port(const pj_sockaddr_t *sockaddr,
                                             Windows::Networking::HostName ^&hostname,
                                             int *port)
{
    char tmp[PJ_INET6_ADDRSTRLEN];
    wchar_t wtmp[PJ_INET6_ADDRSTRLEN];
    pj_sockaddr_print(sockaddr, tmp, PJ_INET6_ADDRSTRLEN, 0);
    pj_ansi_to_unicode(tmp, pj_ansi_strlen(tmp), wtmp,
        PJ_INET6_ADDRSTRLEN);
    hostname = ref new Windows::Networking::HostName(ref new Platform::String(wtmp));
    *port = pj_sockaddr_get_port(sockaddr);

    return PJ_SUCCESS;
}


/* Buffer helper */

#include <Robuffer.h>
#include <wrl/client.h>

inline Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> GetBufferByteAccess(Windows::Storage::Streams::IBuffer^ buffer)
{
    auto pUnk = reinterpret_cast<IUnknown*>(buffer);

    Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> comBuff;
    pUnk->QueryInterface(__uuidof(Windows::Storage::Streams::IBufferByteAccess), (void**)comBuff.ReleaseAndGetAddressOf());

    return comBuff;
}


inline void GetRawBufferFromIBuffer(Windows::Storage::Streams::IBuffer^ buffer, unsigned char** pbuffer)
{
    Platform::Object^ obj = buffer;
    Microsoft::WRL::ComPtr<IInspectable> insp(reinterpret_cast<IInspectable*>(obj));
    Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> bufferByteAccess;
    insp.As(&bufferByteAccess);
    bufferByteAccess->Buffer(pbuffer);
}

inline void CopyToIBuffer(unsigned char* buffSource, unsigned int copyByteCount, Windows::Storage::Streams::IBuffer^ buffer, unsigned int writeStartPos = 0)
{
    auto bufferLen = buffer->Capacity;
    assert(copyByteCount <= bufferLen);

    unsigned char* pBuffer;

    GetRawBufferFromIBuffer(buffer, &pBuffer);

    memcpy(pBuffer + writeStartPos, buffSource, copyByteCount);
}

inline void CopyFromIBuffer(unsigned char* buffDestination, unsigned int copyByteCount, Windows::Storage::Streams::IBuffer^ buffer, unsigned int readStartPos = 0)
{
    assert(copyByteCount <= buffer->Capacity);

    unsigned char* pBuffer;

    GetRawBufferFromIBuffer(buffer, &pBuffer);

    memcpy(buffDestination, pBuffer + readStartPos, copyByteCount);
}


/* PPL helper */

#include <ppltasks.h>
#include <agents.h>

// Creates a task that completes after the specified delay, in ms.
inline concurrency::task<void> complete_after(unsigned int timeout)
{
    // A task completion event that is set when a timer fires.
    concurrency::task_completion_event<void> tce;

    // Create a non-repeating timer.
    auto fire_once = new concurrency::timer<int>(timeout, 0, nullptr, false);
    // Create a call object that sets the completion event after the timer fires.
    auto callback = new concurrency::call<int>([tce](int)
    {
        tce.set();
    });

    // Connect the timer to the callback and start the timer.
    fire_once->link_target(callback);
    fire_once->start();

    // Create a task that completes after the completion event is set.
    concurrency::task<void> event_set(tce);

    // Create a continuation task that cleans up resources and 
    // and return that continuation task. 
    return event_set.then([callback, fire_once]()
    {
        delete callback;
        delete fire_once;
    });
}

// Cancels the provided task after the specifed delay, if the task 
// did not complete. 
template<typename T>
inline concurrency::task<T> cancel_after_timeout(concurrency::task<T> t, concurrency::cancellation_token_source cts, unsigned int timeout)
{
    // Create a task that returns true after the specified task completes.
    concurrency::task<bool> success_task = t.then([](T)
    {
        return true;
    });
    // Create a task that returns false after the specified timeout.
    concurrency::task<bool> failure_task = complete_after(timeout).then([]
    {
        return false;
    });

    // Create a continuation task that cancels the overall task  
    // if the timeout task finishes first. 
    return (failure_task || success_task).then([t, cts](bool success)
    {
        if (!success)
        {
            // Set the cancellation token. The task that is passed as the 
            // t parameter should respond to the cancellation and stop 
            // as soon as it can.
            cts.cancel();
        }

        // Return the original task. 
        return t;
    });
}
