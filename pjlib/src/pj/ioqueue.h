/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/ioqueue.h,v 1.1 2005/12/02 20:02:29 nn Exp $ */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __PJ_IOQUEUE_H__
#define __PJ_IOQUEUE_H__

/**
 * @file ioqueue.h
 * @brief I/O Dispatching Mechanism
 */

//Can't include sock.h now (FD_SETSIZE declaration).
//#include <pj/sock.h>

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_IO (Asynchronous) Input Output
 * @brief (Asynchronous) Input Output
 * @ingroup PJ_OS
 */

/**
 * @defgroup PJ_IOQUEUE I/O Event Dispatching Queue
 * @ingroup PJ_IO
 * @{
 *
 * This file provides abstraction for various event dispatching mechanisms. 
 * The interfaces for event dispatching vary alot, even in a single
 * operating system. The abstraction here hopefully is suitable for most of
 * the event dispatching available.
 *
 * Currently, the I/O Queue supports:
 * - select(), as the common denominator, but the least efficient.
 * - I/O Completion ports in Windows NT/2000/XP, which is the most efficient
 *      way to dispatch events in Windows NT based OSes, and most importantly,
 *      it doesn't have the limit on how many handles to monitor. And it works
 *      with files (not only sockets) as well.
 */

/**
 * @bug For any given socket, only one thread can issue one type of
 *	operation.
 */

 /**
  * This structure describes the callbacks to be called when I/O operation
  * completes.
  */
typedef struct pj_ioqueue_callback
{
    /**
     * This callback is called when #pj_ioqueue_read or #pj_ioqueue_recvfrom
     * completes.
     *
     * @param key	    The key.
     * @param bytes_read    The size of data that has just been read.
     */
    void (*on_read_complete)(pj_ioqueue_key_t *key, pj_ssize_t bytes_read);

    /**
     * This callback is called when #pj_ioqueue_write or #pj_ioqueue_sendto
     * completes.
     *
     * @param key	    The key.
     * @param bytes_read    The size of data that has just been read.
     */
    void (*on_write_complete)(pj_ioqueue_key_t *key, pj_ssize_t bytes_sent);

    /**
     * This callback is called when #pj_ioqueue_accept completes.
     *
     * @param key	    The key.
     * @param status	    Zero if the operation completes successfully.
     */
    void (*on_accept_complete)(pj_ioqueue_key_t *key, int status);

    /**
     * This callback is called when #pj_ioqueue_connect completes.
     *
     * @param key	    The key.
     * @param status	    Zero if the operation completes successfully.
     */
    void (*on_connect_complete)(pj_ioqueue_key_t *key, int status);
} pj_ioqueue_callback;


/**
 * Error value returned by I/O operations to indicate that the operation
 * can't complete immediately and will complete later.
 */
#define PJ_IOQUEUE_PENDING	(-2)

/**
 * Types of I/O Queue operation.
 */
typedef enum pj_ioqueue_operation_e
{
    PJ_IOQUEUE_OP_NONE		= 0,	/**< No operation. */
    PJ_IOQUEUE_OP_READ		= 1,	/**< read() operation. */
    PJ_IOQUEUE_OP_RECV_FROM	= 2,	/**< recvfrom() operation. */
    PJ_IOQUEUE_OP_WRITE		= 4,	/**< write() operation. */
    PJ_IOQUEUE_OP_SEND_TO	= 8,	/**< sendto() operation. */
#if PJ_HAS_TCP
    PJ_IOQUEUE_OP_ACCEPT	= 16,	/**< accept() operation. */
    PJ_IOQUEUE_OP_CONNECT	= 32,	/**< connect() operation. */
#endif
} pj_ioqueue_operation_e;


/**
 * Create a new I/O Queue framework.
 *
 * @param pool	    the pool to allocate the I/O queue structure. 
 * @param max_fd    the maximum number of handles to be supported, which should
 *		    not exceed PJ_IOQUEUE_MAX_HANDLES.
 *
 * @return the I/O queue structure, or NULL upon error.
 */
PJ_DECL(pj_ioqueue_t*) pj_ioqueue_create( pj_pool_t *pool, 
					  pj_size_t max_fd );

/**
 * Destroy the I/O queue.
 *
 * @param ioque	    the I/O Queue
 *
 * @return PJ_OK (zero) if success.
 */
PJ_DECL(pj_status_t) pj_ioqueue_destroy( pj_ioqueue_t *ioque );


/**
 * Register a handle to the I/O queue framework. 
 * For some types of handles, the handle attributes will be modified so
 * that it fits in to the framework. For example, for socket, the socket will
 * be modified to use NONBLOCKING I/O.
 *
 * @param pool	    to allocate the resource for the specified handle, 
 *		    which must be valid until the handle/key is unregistered 
 *		    from I/O Queue.
 * @param ioque	    the I/O Queue.
 * @param hnd	    the OS handle to be registered.
 * @param user_data User data to be associated with the key, which can be
 *		    retrieved later.
 * @param cb	    Callback to be called when I/O operation completes. 
 *
 * @return the key to identify the handle in the AIO framework, or NULL
 *	   upon error.
 */
PJ_DECL(pj_ioqueue_key_t*) pj_ioqueue_register(	pj_pool_t *pool,
						pj_ioqueue_t *ioque,
						pj_oshandle_t hnd,
						void *user_data,
						const pj_ioqueue_callback *cb);

/**
 * Unregister a handle from the I/O Queue framework.
 *
 * @param ioque the I/O Queue.
 * @param key	the key that uniquely identifies the handle, which is returned
 *		from the function pj_ioqueue_register().
 *
 * @return zero on success.
 */
PJ_DECL(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_t *ioque,
					    pj_ioqueue_key_t *key );


/**
 * Get user data associated with the I/O Queue key.
 *
 * @param key	the key previously associated with the socket/handle with
 *		pj_ioqueue_register().
 *
 * @return the user data.
 */
PJ_DECL(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key );


#if PJ_HAS_TCP
/**
 * Instruct I/O Queue to wait for incoming connections on the specified 
 * listening socket. This function will return
 * immediately (i.e. non-blocking) regardless whether some data has been 
 * transfered. If the function can't complete immediately, and the caller will
 * be notified about the completion when it calls pj_ioqueue_poll().
 *
 * @param ioqueue   the I/O Queue
 * @param key	    the key which registered to the server socket.
 * @param sock	    variable to receive the socket for the incoming connection.
 * @param local	    variable to receive local address.
 * @param remote    variable to receive the remote address.
 * @param addrlen   on input, contains the length of the buffer for the
 *		    address, and on output, contains the actual length of the
 *		    address.
 * @return
 *  - zero if there's a connection available immediately, which in this case
 *    the callback should have been called before the function returns.
 *  - PJ_IOQUEUE_PENDING if accept is queued, or 
 *  - (-1) on error.
 */
PJ_DECL(pj_status_t) pj_ioqueue_accept( pj_ioqueue_t *ioqueue,
					pj_ioqueue_key_t *key,
					pj_sock_t *sock,
					pj_sockaddr_t *local,
					pj_sockaddr_t *remote,
					int *addrlen);

/**
 * Initiate non-blocking socket connect. If the socket can NOT be connected
 * immediately, the result will be reported during poll.
 *
 * @param ioqueue   the ioqueue
 * @param key	    the key associated with TCP socket
 * @param addr	    the remote address.
 * @param addrlen   the remote address length.
 *
 * @return
 *  - zero if socket is connected immediately, which in this case the callback
 *    should have been called.
 *  - PJ_IOQUEUE_PENDING if operation is queued, or 
 *  - (-1) on error.
 */
PJ_DECL(pj_status_t) pj_ioqueue_connect( pj_ioqueue_t *ioqueue,
					 pj_ioqueue_key_t *key,
					 const pj_sockaddr_t *addr,
					 int addrlen );

#endif	/* PJ_HAS_TCP */

/**
 * Poll the I/O Queue for completed events.
 *
 * @param ioque		the I/O Queue.
 * @param timeout	polling timeout, or NULL if the thread wishes to wait
 *			indefinetely for the event.
 *
 * @return 
 *  - zero if timed out (no event).
 *  - (-1) if error occured during polling. Callback will NOT be called.
 *  - (1)  if there's an event. Callback will be called.
 */
PJ_DECL(int) pj_ioqueue_poll( pj_ioqueue_t *ioque,
			      const pj_time_val *timeout);

/**
 * Instruct the I/O Queue to read from the specified handle. This function
 * returns immediately (i.e. non-blocking) regardless whether some data has 
 * been transfered. If the operation can't complete immediately, caller will 
 * be notified about the completion when it calls pj_ioqueue_poll().
 *
 * @param ioque	    the I/O Queue.
 * @param key	    the key that uniquely identifies the handle.
 * @param buffer    the buffer to hold the read data. The caller MUST make sure
 *		    that this buffer remain valid until the framework completes
 *		    reading the handle.
 * @param buflen    the maximum size to be read.
 *
 * @return
 *  - zero or positive number to indicate the number of bytes has been
 *		    read, and in this case the operation was not queued.
 *  - (-1) on error, which in this case operation was not queued.
 *  - PJ_IOQUEUE_PENDING if the operation has been queued.
 */
PJ_DECL(int) pj_ioqueue_read( pj_ioqueue_t *ioque,
			      pj_ioqueue_key_t *key,
			      void *buffer,
			      pj_size_t buflen);


/**
 * This function behaves similarly as pj_ioqueue_read(), except that it is
 * normally called for socket, and the remote address will also be returned
 * along with the data. Caller MUST make sure that both buffer and addr
 * remain valid until the framework completes reading the data.
 *
 * @see ::pj_ioqueue_read
 */
PJ_DECL(int) pj_ioqueue_recvfrom( pj_ioqueue_t *ioque,
				  pj_ioqueue_key_t *key,
				  void *buffer,
				  pj_size_t buflen,
				  pj_sockaddr_t *addr,
				  int *addrlen);

/**
 * Instruct the I/O Queue to write to the handle. This function will return
 * immediately (i.e. non-blocking) regardless whether some data has been 
 * transfered. If the function can't complete immediately, and the caller will
 * be notified about the completion when it calls pj_ioqueue_poll().
 *
 * @param ioque	    the I/O Queue.
 * @param key	    the key that identifies the handle.
 * @param data	    the data to send. Caller MUST make sure that this buffer 
 *		    remains valid until the write operation completes.
 * @param datalen   the length of the data.
 *
 * @return
 *  - zero or positive number to indicate the number of bytes has been
 *	    written, and in this case the operation was not queued.
 *  - (-1) on error, which in this case operation was not queued.
 *  - PJ_IOQUEUE_PENDING if the operation has been queued.
 */
PJ_DECL(int) pj_ioqueue_write( pj_ioqueue_t *ioque,
				pj_ioqueue_key_t *key,
				const void *data,
				pj_size_t datalen);

/**
 * This function behaves similarly as pj_ioqueue_write(), except that
 * pj_sock_sendto() (or equivalent) will be called to send the data.
 *
 * @see pj_ioqueue_write
 */
PJ_DECL(int) pj_ioqueue_sendto( pj_ioqueue_t *ioque,
				pj_ioqueue_key_t *key,
				const void *data,
				pj_size_t datalen,
				const pj_sockaddr_t *addr,
				int addrlen);


/**
 * !}
 */

PJ_END_DECL

#endif	/* __PJ_IOQUEUE_H__ */

