/* $Header: /pjproject-0.3/pjlib/include/pj/ioqueue.h 10    10/29/05 11:29a Bennylp $ */

#ifndef __PJ_IOQUEUE_H__
#define __PJ_IOQUEUE_H__

/**
 * @file ioqueue.h
 * @brief I/O Dispatching Mechanism
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_IO Network I/O
 * @brief Network I/O
 * @ingroup PJ_OS
 *
 * This section contains API building blocks to perform network I/O and 
 * communications. If provides:
 *  - @ref PJ_SOCK
 *\n
 *    A highly portable socket abstraction, runs on all kind of
 *    network APIs such as standard BSD socket, Windows socket, Linux
 *    \b kernel socket, PalmOS networking API, etc.
 *
 *  - @ref pj_addr_resolve
 *\n
 *    Portable address resolution, which implements #pj_gethostbyname().
 *
 *  - @ref PJ_SOCK_SELECT
 *\n
 *    A portable \a select() like API (#pj_sock_select()) which can be
 *    implemented with various back-ends.
 *
 *  - @ref PJ_IOQUEUE
 *\n
 *    Framework for dispatching network events.
 *
 * For more information see the modules below.
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
 *
 * \section pj_ioqeuue_examples_sec Examples
 *
 * For some examples on how to use the I/O Queue, please see:
 *
 *  - \ref page_pjlib_ioqueue_tcp_test
 *  - \ref page_pjlib_ioqueue_udp_test
 *  - \ref page_pjlib_ioqueue_perf_test
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
     * @param sock          Newly connected socket.
     * @param status	    Zero if the operation completes successfully.
     */
    void (*on_accept_complete)(pj_ioqueue_key_t *key, pj_sock_t sock, 
                               int status);

    /**
     * This callback is called when #pj_ioqueue_connect completes.
     *
     * @param key	    The key.
     * @param status	    Zero if the operation completes successfully.
     */
    void (*on_connect_complete)(pj_ioqueue_key_t *key, int status);
} pj_ioqueue_callback;


/**
 * Types of I/O Queue operation.
 */
typedef enum pj_ioqueue_operation_e
{
    PJ_IOQUEUE_OP_NONE		= 0,	/**< No operation.          */
    PJ_IOQUEUE_OP_READ		= 1,	/**< read() operation.      */
    PJ_IOQUEUE_OP_RECV          = 2,    /**< recv() operation.      */
    PJ_IOQUEUE_OP_RECV_FROM	= 4,	/**< recvfrom() operation.  */
    PJ_IOQUEUE_OP_WRITE		= 8,	/**< write() operation.     */
    PJ_IOQUEUE_OP_SEND          = 16,   /**< send() operation.      */
    PJ_IOQUEUE_OP_SEND_TO	= 32,	/**< sendto() operation.    */
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
    PJ_IOQUEUE_OP_ACCEPT	= 64,	/**< accept() operation.    */
    PJ_IOQUEUE_OP_CONNECT	= 128,	/**< connect() operation.   */
#endif	/* PJ_HAS_TCP */
} pj_ioqueue_operation_e;


/**
 * Indicates that the I/O Queue should be created to handle reasonable
 * number of threads.
 */
#define PJ_IOQUEUE_DEFAULT_THREADS  0


/**
 * Create a new I/O Queue framework.
 *
 * @param pool		The pool to allocate the I/O queue structure. 
 * @param max_fd	The maximum number of handles to be supported, which 
 *			should not exceed PJ_IOQUEUE_MAX_HANDLES.
 * @param max_threads	The maximum number of threads that are allowed to
 *			operate on a single descriptor simultaneously. If
 *                      the value is zero, the framework will set it
 *                      to a reasonable value.
 * @param ioqueue	Pointer to hold the newly created I/O Queue.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_ioqueue_create( pj_pool_t *pool, 
					pj_size_t max_fd,
					int max_threads,
					pj_ioqueue_t **ioqueue);

/**
 * Destroy the I/O queue.
 *
 * @param ioque	        The I/O Queue to be destroyed.
 *
 * @return              PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pj_ioqueue_destroy( pj_ioqueue_t *ioque );

/**
 * Set the lock object to be used by the I/O Queue. This function can only
 * be called right after the I/O queue is created, before any handle is
 * registered to the I/O queue.
 *
 * Initially the I/O queue is created with non-recursive mutex protection. 
 * Applications can supply alternative lock to be used by calling this 
 * function.
 *
 * @param ioque         The ioqueue instance.
 * @param lock          The lock to be used by the ioqueue.
 * @param auto_delete   In non-zero, the lock will be deleted by the ioqueue.
 *
 * @return              PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_set_lock( pj_ioqueue_t *ioque, 
					  pj_lock_t *lock,
					  pj_bool_t auto_delete );

/**
 * Register a socket to the I/O queue framework. 
 * When a socket is registered to the IOQueue, it may be modified to use
 * non-blocking IO. If it is modified, there is no guarantee that this 
 * modification will be restored after the socket is unregistered.
 *
 * @param pool	    To allocate the resource for the specified handle, 
 *		    which must be valid until the handle/key is unregistered 
 *		    from I/O Queue.
 * @param ioque	    The I/O Queue.
 * @param sock	    The socket.
 * @param user_data User data to be associated with the key, which can be
 *		    retrieved later.
 * @param cb	    Callback to be called when I/O operation completes. 
 * @param key	    Pointer to receive the returned key.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
					       pj_ioqueue_t *ioque,
					       pj_sock_t sock,
					       void *user_data,
					       const pj_ioqueue_callback *cb,
					       pj_ioqueue_key_t **key);

/**
 * Unregister a handle from the I/O Queue framework.
 *
 * @param ioque     The I/O Queue.
 * @param key	    The key that uniquely identifies the handle, which is 
 *                  returned from the function #pj_ioqueue_register_sock()
 *                  or other registration functions.
 *
 * @return          PJ_SUCCESS on success or the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_t *ioque,
					    pj_ioqueue_key_t *key );


/**
 * Get user data associated with the I/O Queue key.
 *
 * @param key	    The key previously associated with the socket/handle with
 *		    #pj_ioqueue_register_sock() (or other registration 
 *                  functions).
 *
 * @return          The user data associated with the key, or NULL on error
 *                  of if no data is associated with the key during 
 *                  registration.
 */
PJ_DECL(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key );


#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
/**
 * Instruct I/O Queue to wait for incoming connections on the specified 
 * listening socket. This function will return
 * immediately (i.e. non-blocking) regardless whether some data has been 
 * transfered. If the function can't complete immediately, and the caller will
 * be notified about the completion when it calls pj_ioqueue_poll().
 *
 * @param ioqueue   The I/O Queue
 * @param key	    The key which registered to the server socket.
 * @param sock	    Argument which contain pointer to receive 
 *                  the socket for the incoming connection.
 * @param local	    Optional argument which contain pointer to variable to 
 *                  receive local address.
 * @param remote    Optional argument which contain pointer to variable to 
 *                  receive the remote address.
 * @param addrlen   On input, contains the length of the buffer for the
 *		    address, and on output, contains the actual length of the
 *		    address. This argument is optional.
 * @return
 *  - PJ_SUCCESS    If there's a connection available immediately, which 
 *                  in this case the callback should have been called before 
 *                  the function returns.
 *  - PJ_EPENDING   If accept is queued, or 
 *  - non-zero      which indicates the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_accept( pj_ioqueue_t *ioqueue,
					pj_ioqueue_key_t *key,
					pj_sock_t *sock,
					pj_sockaddr_t *local,
					pj_sockaddr_t *remote,
					int *addrlen );

/**
 * Initiate non-blocking socket connect. If the socket can NOT be connected
 * immediately, the result will be reported during poll.
 *
 * @param ioqueue   The ioqueue
 * @param key	    The key associated with TCP socket
 * @param addr	    The remote address.
 * @param addrlen   The remote address length.
 *
 * @return
 *  - PJ_SUCCESS    If socket is connected immediately, which in this case 
 *                  the callback should have been called.
 *  - PJ_EPENDING   If operation is queued, or 
 *  - non-zero      Indicates the error code.
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
 *  - (<0) if error occured during polling. Callback will NOT be called.
 *  - (>1) to indicate numbers of events. Callbacks have been called.
 */
PJ_DECL(int) pj_ioqueue_poll( pj_ioqueue_t *ioque,
			      const pj_time_val *timeout);

/**
 * Instruct the I/O Queue to read from the specified handle. This function
 * returns immediately (i.e. non-blocking) regardless whether some data has 
 * been transfered. If the operation can't complete immediately, caller will 
 * be notified about the completion when it calls pj_ioqueue_poll().
 *
 * @param ioque	    The I/O Queue.
 * @param key	    The key that uniquely identifies the handle.
 * @param buffer    The buffer to hold the read data. The caller MUST make sure
 *		    that this buffer remain valid until the framework completes
 *		    reading the handle.
 * @param buflen    The maximum size to be read.
 *
 * @return
 *  - PJ_SUCCESS    If immediate data has been received. In this case, the 
 *		    callback must have been called before this function 
 *		    returns, and no pending operation is scheduled.
 *  - PJ_EPENDING   If the operation has been queued.
 *  - non-zero      The return value indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_read( pj_ioqueue_t *ioque,
				      pj_ioqueue_key_t *key,
				      void *buffer,
				      pj_size_t buflen);


/**
 * This function behaves similarly as #pj_ioqueue_read(), except that it is
 * normally called for socket.
 *
 * @param ioque	    The I/O Queue.
 * @param key	    The key that uniquely identifies the handle.
 * @param buffer    The buffer to hold the read data. The caller MUST make sure
 *		    that this buffer remain valid until the framework completes
 *		    reading the handle.
 * @param buflen    The maximum size to be read.
 * @param flags     Recv flag.
 *
 * @return
 *  - PJ_SUCCESS    If immediate data has been received. In this case, the 
 *		    callback must have been called before this function 
 *		    returns, and no pending operation is scheduled.
 *  - PJ_EPENDING   If the operation has been queued.
 *  - non-zero      The return value indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_recv( pj_ioqueue_t *ioque,
				      pj_ioqueue_key_t *key,
				      void *buffer,
				      pj_size_t buflen,
				      unsigned flags );

/**
 * This function behaves similarly as #pj_ioqueue_read(), except that it is
 * normally called for socket, and the remote address will also be returned
 * along with the data. Caller MUST make sure that both buffer and addr
 * remain valid until the framework completes reading the data.
 *
 * @param ioque	    The I/O Queue.
 * @param key	    The key that uniquely identifies the handle.
 * @param buffer    The buffer to hold the read data. The caller MUST make sure
 *		    that this buffer remain valid until the framework completes
 *		    reading the handle.
 * @param buflen    The maximum size to be read.
 * @param flags     Recv flag.
 * @param addr      Pointer to buffer to receive the address, or NULL.
 * @param addrlen   On input, specifies the length of the address buffer.
 *                  On output, it will be filled with the actual length of
 *                  the address.
 *
 * @return
 *  - PJ_SUCCESS    If immediate data has been received. In this case, the 
 *		    callback must have been called before this function 
 *		    returns, and no pending operation is scheduled.
 *  - PJ_EPENDING   If the operation has been queued.
 *  - non-zero      The return value indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_recvfrom( pj_ioqueue_t *ioque,
					  pj_ioqueue_key_t *key,
					  void *buffer,
					  pj_size_t buflen,
                                          unsigned flags,
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
 *  - PJ_SUCCESS    If data was immediately written.
 *  - PJ_EPENDING   If the operation has been queued.
 *  - non-zero      The return value indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_write( pj_ioqueue_t *ioque,
				       pj_ioqueue_key_t *key,
				       const void *data,
				       pj_size_t datalen);

/**
 * This function behaves similarly as #pj_ioqueue_write(), except that
 * pj_sock_send() (or equivalent) will be called to send the data.
 *
 * @param ioque	    the I/O Queue.
 * @param key	    the key that identifies the handle.
 * @param data	    the data to send. Caller MUST make sure that this buffer 
 *		    remains valid until the write operation completes.
 * @param datalen   the length of the data.
 * @param flags     send flags.
 *
 * @return
 *  - PJ_SUCCESS    If data was immediately written.
 *  - PJ_EPENDING   If the operation has been queued.
 *  - non-zero      The return value indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_send( pj_ioqueue_t *ioque,
				      pj_ioqueue_key_t *key,
				      const void *data,
				      pj_size_t datalen,
				      unsigned flags );


/**
 * This function behaves similarly as #pj_ioqueue_write(), except that
 * pj_sock_sendto() (or equivalent) will be called to send the data.
 *
 * @param ioque	    the I/O Queue.
 * @param key	    the key that identifies the handle.
 * @param data	    the data to send. Caller MUST make sure that this buffer 
 *		    remains valid until the write operation completes.
 * @param datalen   the length of the data.
 * @param flags     send flags.
 * @param addr      remote address.
 * @param addrlen   remote address length.
 *
 * @return
 *  - PJ_SUCCESS    If data was immediately written.
 *  - PJ_EPENDING   If the operation has been queued.
 *  - non-zero      The return value indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_sendto( pj_ioqueue_t *ioque,
					pj_ioqueue_key_t *key,
					const void *data,
					pj_size_t datalen,
                                        unsigned flags,
					const pj_sockaddr_t *addr,
					int addrlen);


/**
 * !}
 */

PJ_END_DECL

#endif	/* __PJ_IOQUEUE_H__ */

