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
#include <pj/ioqueue.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/list.h>
#include <pj/lock.h>
#include <pj/pool.h>
#include <pj/string.h>

#include "os_symbian.h"

class CIoqueueCallback;

/*
 * IO Queue structure.
 */
struct pj_ioqueue_t
{
    int		     eventCount;
};


/////////////////////////////////////////////////////////////////////////////
// Class to encapsulate asynchronous socket operation.
//
class CIoqueueCallback : public CActive
{
public:
    static CIoqueueCallback* NewL(pj_ioqueue_t *ioqueue,
				  pj_ioqueue_key_t *key, 
				  pj_sock_t sock, 
				  const pj_ioqueue_callback *cb, 
				  void *user_data);

    //
    // Start asynchronous recv() operation
    //
    pj_status_t StartRead(pj_ioqueue_op_key_t *op_key, 
			  void *buf, pj_ssize_t *size, unsigned flags,
			  pj_sockaddr_t *addr, int *addrlen);

    //
    // Start asynchronous accept() operation.
    //
    pj_status_t StartAccept(pj_ioqueue_op_key_t *op_key,
			    pj_sock_t *new_sock,
			    pj_sockaddr_t *local,
			    pj_sockaddr_t *remote,
			    int *addrlen );

    //
    // Completion callback.
    //
    void RunL();

    //
    // CActive's DoCancel()
    //
    void DoCancel();

    //
    // Cancel operation and call callback.
    //
    void CancelOperation(pj_ioqueue_op_key_t *op_key, 
			 pj_ssize_t bytes_status);

    //
    // Accessors
    //
    void* get_user_data() const
    {
	return user_data_;
    }
    void set_user_data(void *user_data)
    {
	user_data_ = user_data;
    }
    pj_ioqueue_op_key_t *get_op_key() const
    {
	return pending_data_.common_.op_key_;
    }
    CPjSocket* get_pj_socket()
    {
	return sock_;
    }

private:
    // Type of pending operation.
    enum Type {
	TYPE_NONE,
	TYPE_READ,
	TYPE_ACCEPT,
    };

    // Static data.
    pj_ioqueue_t		*ioqueue_;
    pj_ioqueue_key_t		*key_;
    CPjSocket			*sock_;
    pj_ioqueue_callback		 cb_;
    void			*user_data_;

    // Symbian data.
    TPtr8			 aBufferPtr_;
    TInetAddr			 aAddress_;

    // Application data.
    Type			 type_;

    union Pending_Data
    {
	struct Common
	{
	    pj_ioqueue_op_key_t	*op_key_;
	} common_;

	struct Pending_Read
	{
	    pj_ioqueue_op_key_t	*op_key_;
	    pj_sockaddr_t	*addr_;
	    int			*addrlen_;
	} read_;

	struct Pending_Accept
	{
	    pj_ioqueue_op_key_t *op_key_;
	    pj_sock_t		*new_sock_;
	    pj_sockaddr_t	*local_;
	    pj_sockaddr_t	*remote_;
	    int			*addrlen_;
	} accept_;
    };

    union Pending_Data		 pending_data_;
    RSocket			blank_sock_;

    CIoqueueCallback(pj_ioqueue_t *ioqueue,
		     pj_ioqueue_key_t *key, pj_sock_t sock, 
		     const pj_ioqueue_callback *cb, void *user_data)
    : CActive(CActive::EPriorityStandard),
	  ioqueue_(ioqueue), key_(key), sock_((CPjSocket*)sock), 
	  user_data_(user_data), aBufferPtr_(NULL, 0), type_(TYPE_NONE)
    {
    	pj_memcpy(&cb_, cb, sizeof(*cb));
    }


    void ConstructL()
    {
	CActiveScheduler::Add(this);
    }
    
    void HandleReadCompletion();
    CPjSocket *HandleAcceptCompletion();
};


CIoqueueCallback* CIoqueueCallback::NewL(pj_ioqueue_t *ioqueue,
					 pj_ioqueue_key_t *key, 
					 pj_sock_t sock, 
					 const pj_ioqueue_callback *cb, 
					 void *user_data)
{
    CIoqueueCallback *self = new CIoqueueCallback(ioqueue, key, sock, 
						  cb, user_data);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);

    return self;
}


//
// Start asynchronous recv() operation
//
pj_status_t CIoqueueCallback::StartRead(pj_ioqueue_op_key_t *op_key, 
					void *buf, pj_ssize_t *size, 
					unsigned flags,
					pj_sockaddr_t *addr, int *addrlen)
{
    PJ_ASSERT_RETURN(IsActive()==false, PJ_EBUSY);
    PJ_ASSERT_RETURN(pending_data_.common_.op_key_==NULL, PJ_EBUSY);

    flags &= ~PJ_IOQUEUE_ALWAYS_ASYNC;

    pending_data_.read_.op_key_ = op_key;
    pending_data_.read_.addr_ = addr;
    pending_data_.read_.addrlen_ = addrlen;

    aBufferPtr_.Set((TUint8*)buf, 0, (TInt)*size);

    type_ = TYPE_READ;
    if (addr && addrlen) {
	sock_->Socket().RecvFrom(aBufferPtr_, aAddress_, flags, iStatus);
    } else {
	aAddress_.SetAddress(0);
	aAddress_.SetPort(0);
	sock_->Socket().Recv(aBufferPtr_, flags, iStatus);
    }

    SetActive();
    return PJ_EPENDING;
}


//
// Start asynchronous accept() operation.
//
pj_status_t CIoqueueCallback::StartAccept(pj_ioqueue_op_key_t *op_key,
					  pj_sock_t *new_sock,
					  pj_sockaddr_t *local,
					  pj_sockaddr_t *remote,
					  int *addrlen )
{
    PJ_ASSERT_RETURN(IsActive()==false, PJ_EBUSY);
    PJ_ASSERT_RETURN(pending_data_.common_.op_key_==NULL, PJ_EBUSY);

    pending_data_.accept_.op_key_ = op_key;
    pending_data_.accept_.new_sock_ = new_sock;
    pending_data_.accept_.local_ = local;
    pending_data_.accept_.remote_ = remote;
    pending_data_.accept_.addrlen_ = addrlen;

    // Create blank socket
    blank_sock_.Open(PjSymbianOS::Instance()->SocketServ());

    type_ = TYPE_ACCEPT;
    sock_->Socket().Accept(blank_sock_, iStatus);

    SetActive();
    return PJ_EPENDING;
}


//
// Handle asynchronous RecvFrom() completion
//
void CIoqueueCallback::HandleReadCompletion() 
{
    if (pending_data_.read_.addr_) {
	PjSymbianOS::Addr2pj(aAddress_, 
			     *(pj_sockaddr_in*)pending_data_.read_.addr_);
	pending_data_.read_.addr_ = NULL;
    }
    if (pending_data_.read_.addrlen_) {
	*pending_data_.read_.addrlen_ = sizeof(pj_sockaddr_in);
	pending_data_.read_.addrlen_ = NULL;
    }
	
    pending_data_.read_.op_key_ = NULL;
}


//
// Handle asynchronous Accept() completion.
//
CPjSocket *CIoqueueCallback::HandleAcceptCompletion() 
{
	CPjSocket *pjNewSock = new CPjSocket(blank_sock_);

	if (pending_data_.accept_.new_sock_) {
	    *pending_data_.accept_.new_sock_ = (pj_sock_t)pjNewSock;
	    pending_data_.accept_.new_sock_ = NULL;
	}

	if (pending_data_.accept_.local_) {
	    TInetAddr aAddr;
	    pj_sockaddr_in *ptr_sockaddr;

	    blank_sock_.LocalName(aAddr);
	    ptr_sockaddr = (pj_sockaddr_in*)pending_data_.accept_.local_;
	    PjSymbianOS::Addr2pj(aAddr, *ptr_sockaddr);
	    pending_data_.accept_.local_ = NULL;
	}

	if (pending_data_.accept_.remote_) {
	    TInetAddr aAddr;
	    pj_sockaddr_in *ptr_sockaddr;

	    blank_sock_.RemoteName(aAddr);
	    ptr_sockaddr = (pj_sockaddr_in*)pending_data_.accept_.remote_;
	    PjSymbianOS::Addr2pj(aAddr, *ptr_sockaddr);
	    pending_data_.accept_.remote_ = NULL;
	}

	if (pending_data_.accept_.addrlen_) {
	    *pending_data_.accept_.addrlen_ = sizeof(pj_sockaddr_in);
	    pending_data_.accept_.addrlen_ = NULL;
	}
	
	return pjNewSock;
}


//
// Completion callback.
//
void CIoqueueCallback::RunL()
{
    Type cur_type = type_;

    type_ = TYPE_NONE;

    if (cur_type == TYPE_READ) {
	//
	// Completion of asynchronous RecvFrom()
	//

	/* Clear op_key (save it to temp variable first!) */
	pj_ioqueue_op_key_t	*op_key = pending_data_.read_.op_key_;
	pending_data_.read_.op_key_ = NULL;

	// Handle failure condition
	if (iStatus != KErrNone) {
	    if (cb_.on_read_complete) {
	    	cb_.on_read_complete( key_, op_key, 
				      -PJ_RETURN_OS_ERROR(iStatus.Int()));
	    }
	    return;
	}

	HandleReadCompletion();

	/* Call callback */
	if (cb_.on_read_complete) {
	    cb_.on_read_complete(key_, op_key, aBufferPtr_.Length());
	}

    } else if (cur_type == TYPE_ACCEPT) {
	//
	// Completion of asynchronous Accept()
	//
	
	/* Clear op_key (save it to temp variable first!) */
	pj_ioqueue_op_key_t	*op_key = pending_data_.read_.op_key_;
	pending_data_.read_.op_key_ = NULL;

	// Handle failure condition
	if (iStatus != KErrNone) {
	    if (pending_data_.accept_.new_sock_)
		*pending_data_.accept_.new_sock_ = PJ_INVALID_SOCKET;
	    
	    if (cb_.on_accept_complete) {
	    	cb_.on_accept_complete( key_, op_key, PJ_INVALID_SOCKET,
				        -PJ_RETURN_OS_ERROR(iStatus.Int()));
	    }
	    return;
	}

	CPjSocket *pjNewSock = HandleAcceptCompletion();
	
	// Call callback.
	if (cb_.on_accept_complete) {
	    cb_.on_accept_complete( key_, op_key, (pj_sock_t)pjNewSock, 
				    PJ_SUCCESS);
	}
    }

    ioqueue_->eventCount++;
}

//
// CActive's DoCancel()
//
void CIoqueueCallback::DoCancel()
{
    if (type_ == TYPE_READ)
	sock_->Socket().CancelRecv();
    else if (type_ == TYPE_ACCEPT)
	sock_->Socket().CancelAccept();

    type_ = TYPE_NONE;
}

//
// Cancel operation and call callback.
//
void CIoqueueCallback::CancelOperation(pj_ioqueue_op_key_t *op_key, 
				       pj_ssize_t bytes_status)
{
    Type cur_type = type_;

    Cancel();

    if (cur_type == TYPE_READ) {
    	if (cb_.on_read_complete)
    	    cb_.on_read_complete(key_, op_key, bytes_status);
    } else if (cur_type == TYPE_ACCEPT)
	;
}


/////////////////////////////////////////////////////////////////////////////
/*
 * IO Queue key structure.
 */
struct pj_ioqueue_key_t
{
    CIoqueueCallback	*cbObj;
};


/*
 * Return the name of the ioqueue implementation.
 */
PJ_DEF(const char*) pj_ioqueue_name(void)
{
    return "ioqueue-symbian";
}


/*
 * Create a new I/O Queue framework.
 */
PJ_DEF(pj_status_t) pj_ioqueue_create(	pj_pool_t *pool, 
					pj_size_t max_fd,
					pj_ioqueue_t **p_ioqueue)
{
    pj_ioqueue_t *ioq;

    PJ_UNUSED_ARG(max_fd);

    ioq = PJ_POOL_ZALLOC_T(pool, pj_ioqueue_t);
    *p_ioqueue = ioq;
    return PJ_SUCCESS;
}


/*
 * Destroy the I/O queue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_destroy( pj_ioqueue_t *ioq )
{
    PJ_UNUSED_ARG(ioq);
    return PJ_SUCCESS;
}


/*
 * Set the lock object to be used by the I/O Queue. 
 */
PJ_DEF(pj_status_t) pj_ioqueue_set_lock( pj_ioqueue_t *ioq, 
					 pj_lock_t *lock,
					 pj_bool_t auto_delete )
{
    /* Don't really need lock for now */
    PJ_UNUSED_ARG(ioq);
    
    if (auto_delete) {
	pj_lock_destroy(lock);
    }

    return PJ_SUCCESS;
}


/*
 * Register a socket to the I/O queue framework. 
 */
PJ_DEF(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
					      pj_ioqueue_t *ioq,
					      pj_sock_t sock,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **p_key )
{
    pj_ioqueue_key_t *key;

    key = PJ_POOL_ZALLOC_T(pool, pj_ioqueue_key_t);
    key->cbObj = CIoqueueCallback::NewL(ioq, key, sock, cb, user_data);

    *p_key = key;
    return PJ_SUCCESS;
}

/*
 * Unregister from the I/O Queue framework. 
 */
PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_key_t *key )
{
    if (key == NULL || key->cbObj == NULL)
	return PJ_SUCCESS;

    // Cancel pending async object
    if (key->cbObj && key->cbObj->IsActive()) {
	key->cbObj->Cancel();
    }

    // Close socket.
    key->cbObj->get_pj_socket()->Socket().Close();
    delete key->cbObj->get_pj_socket();

    // Delete async object.
    if (key->cbObj) {
	delete key->cbObj;
	key->cbObj = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Get user data associated with an ioqueue key.
 */
PJ_DEF(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key )
{
    return key->cbObj->get_user_data();
}


/*
 * Set or change the user data to be associated with the file descriptor or
 * handle or socket descriptor.
 */
PJ_DEF(pj_status_t) pj_ioqueue_set_user_data( pj_ioqueue_key_t *key,
                                              void *user_data,
                                              void **old_data)
{
    if (old_data)
	*old_data = key->cbObj->get_user_data();
    key->cbObj->set_user_data(user_data);

    return PJ_SUCCESS;
}


/*
 * Initialize operation key.
 */
PJ_DEF(void) pj_ioqueue_op_key_init( pj_ioqueue_op_key_t *op_key,
				     pj_size_t size )
{
    pj_bzero(op_key, size);
}


/*
 * Check if operation is pending on the specified operation key.
 */
PJ_DEF(pj_bool_t) pj_ioqueue_is_pending( pj_ioqueue_key_t *key,
                                         pj_ioqueue_op_key_t *op_key )
{
    return key->cbObj->get_op_key()==op_key &&
	   key->cbObj->IsActive();
}


/*
 * Post completion status to the specified operation key and call the
 * appropriate callback. 
 */
PJ_DEF(pj_status_t) pj_ioqueue_post_completion( pj_ioqueue_key_t *key,
                                                pj_ioqueue_op_key_t *op_key,
                                                pj_ssize_t bytes_status )
{
    if (pj_ioqueue_is_pending(key, op_key)) {
	key->cbObj->CancelOperation(op_key, bytes_status);
    }
    return PJ_SUCCESS;
}


#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
/**
 * Instruct I/O Queue to accept incoming connection on the specified 
 * listening socket.
 */
PJ_DEF(pj_status_t) pj_ioqueue_accept( pj_ioqueue_key_t *key,
                                       pj_ioqueue_op_key_t *op_key,
				       pj_sock_t *new_sock,
				       pj_sockaddr_t *local,
				       pj_sockaddr_t *remote,
				       int *addrlen )
{
    
    return key->cbObj->StartAccept(op_key, new_sock, local, remote, addrlen);
}


/*
 * Initiate non-blocking socket connect.
 */
PJ_DEF(pj_status_t) pj_ioqueue_connect( pj_ioqueue_key_t *key,
					const pj_sockaddr_t *addr,
					int addrlen )
{
    PJ_ASSERT_RETURN(addrlen == sizeof(pj_sockaddr_in), PJ_EINVAL);

    RSocket &rSock = key->cbObj->get_pj_socket()->Socket();
    TInetAddr inetAddr;
    PjSymbianOS::pj2Addr(*(const pj_sockaddr_in*)addr, inetAddr);
    TRequestStatus reqStatus;

    // We don't support async connect for now.
    PJ_TODO(IOQUEUE_SUPPORT_ASYNC_CONNECT);

    rSock.Connect(inetAddr, reqStatus);
    User::WaitForRequest(reqStatus);

    if (reqStatus == KErrNone)
	return PJ_SUCCESS;

    return PJ_RETURN_OS_ERROR(reqStatus.Int());
}


#endif	/* PJ_HAS_TCP */

/*
 * Poll the I/O Queue for completed events.
 */
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioq,
			     const pj_time_val *timeout)
{
    /* Polling is not necessary on Symbian, since all async activities
     * are registered to active scheduler.
     */
    PJ_UNUSED_ARG(ioq);
    PJ_UNUSED_ARG(timeout);
    return 0;
}


/*
 * Instruct the I/O Queue to read from the specified handle.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recv( pj_ioqueue_key_t *key,
                                     pj_ioqueue_op_key_t *op_key,
				     void *buffer,
				     pj_ssize_t *length,
				     pj_uint32_t flags )
{
    // Clear flag
    flags &= ~PJ_IOQUEUE_ALWAYS_ASYNC;
    return key->cbObj->StartRead(op_key, buffer, length, flags, NULL, NULL);
}


/*
 * This function behaves similarly as #pj_ioqueue_recv(), except that it is
 * normally called for socket, and the remote address will also be returned
 * along with the data.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recvfrom( pj_ioqueue_key_t *key,
                                         pj_ioqueue_op_key_t *op_key,
					 void *buffer,
					 pj_ssize_t *length,
                                         pj_uint32_t flags,
					 pj_sockaddr_t *addr,
					 int *addrlen)
{
    if (key->cbObj->IsActive())
	return PJ_EBUSY;

    // Clear flag
    flags &= ~PJ_IOQUEUE_ALWAYS_ASYNC;
    return key->cbObj->StartRead(op_key, buffer, length, flags, addr, addrlen);
}


/*
 * Instruct the I/O Queue to write to the handle.
 */
PJ_DEF(pj_status_t) pj_ioqueue_send( pj_ioqueue_key_t *key,
                                     pj_ioqueue_op_key_t *op_key,
				     const void *data,
				     pj_ssize_t *length,
				     pj_uint32_t flags )
{
    TRequestStatus reqStatus;
    TPtrC8 aBuffer((const TUint8*)data, (TInt)*length);
    TSockXfrLength aLen;
    
    PJ_UNUSED_ARG(op_key);

    // Forcing pending operation is not supported.
    PJ_ASSERT_RETURN((flags & PJ_IOQUEUE_ALWAYS_ASYNC)==0, PJ_EINVAL);

    // Clear flag
    flags &= ~PJ_IOQUEUE_ALWAYS_ASYNC;

    key->cbObj->get_pj_socket()->Socket().Send(aBuffer, flags, reqStatus, aLen);
    User::WaitForRequest(reqStatus);

    if (reqStatus.Int() != KErrNone)
	return PJ_RETURN_OS_ERROR(reqStatus.Int());

    //At least in UIQ Emulator, aLen.Length() reports incorrect length
    //for UDP (some newlc.com users seem to have reported this too).
    //*length = aLen.Length();
    return PJ_SUCCESS;
}


/*
 * Instruct the I/O Queue to write to the handle.
 */
PJ_DEF(pj_status_t) pj_ioqueue_sendto( pj_ioqueue_key_t *key,
                                       pj_ioqueue_op_key_t *op_key,
				       const void *data,
				       pj_ssize_t *length,
                                       pj_uint32_t flags,
				       const pj_sockaddr_t *addr,
				       int addrlen)
{
    TRequestStatus reqStatus;
    TPtrC8 aBuffer;
    TInetAddr inetAddr;
    TSockXfrLength aLen;
    
    PJ_UNUSED_ARG(op_key);

    // Forcing pending operation is not supported.
    PJ_ASSERT_RETURN((flags & PJ_IOQUEUE_ALWAYS_ASYNC)==0, PJ_EINVAL);

    // Must be pj_sockaddr_in for now.
    PJ_ASSERT_RETURN(addrlen == sizeof(pj_sockaddr_in), PJ_EINVAL);

    // Clear flag
    flags &= ~PJ_IOQUEUE_ALWAYS_ASYNC;

    aBuffer.Set((const TUint8*)data, (TInt)*length);
    PjSymbianOS::pj2Addr(*(const pj_sockaddr_in*)addr, inetAddr);
    CPjSocket *pjSock = key->cbObj->get_pj_socket();

    pjSock->Socket().SendTo(aBuffer, inetAddr, flags, reqStatus, aLen);
    User::WaitForRequest(reqStatus);

    if (reqStatus.Int() != KErrNone)
	return PJ_RETURN_OS_ERROR(reqStatus.Int());

    //At least in UIQ Emulator, aLen.Length() reports incorrect length
    //for UDP (some newlc.com users seem to have reported this too).
    //*length = aLen.Length();
    return PJ_SUCCESS;
}

