/* $Id$
 *
 */
#ifndef __PJ_EQUEUE_H__
#define __PJ_EQUEUE_H__

/**
 * @file equeue.h
 * @brief Event Queue
 */
#include <pj/types.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJ_EQUEUE Event Queue
 * @brief Event Queue
 * @ingroup PJ_OS
 * @{
 */


/**
 * Opaque data type for Event Queue.
 */
typedef struct pj_equeue_t pj_equeue_t;

/**
 * Opaque data type for Event Queue key.
 */
typedef struct pj_equeue_key_t pj_equeue_key_t;


/**
 * This structure describes the callbacks to be called when I/O operation
 * completes.
 */
typedef struct pj_io_callback
{
    /**
     * This callback is called when #pj_equeue_read, #pj_equeue_recv or 
     * #pj_equeue_recvfrom completes.
     *
     * @param key	    The key.
     * @param bytes_read    The size of data that has just been read.
     */
    void (*on_read_complete)(pj_equeue_key_t *key, pj_ssize_t bytes_read);

    /**
     * This callback is called when #pj_equeue_write, #pj_equeue_send, or
     * #pj_equeue_sendto completes.
     *
     * @param key	    The key.
     * @param bytes_read    The size of data that has just been written.
     */
    void (*on_write_complete)(pj_equeue_key_t *key, pj_ssize_t bytes_sent);

    /**
     * This callback is called when #pj_equeue_accept completes.
     *
     * @param key	    The key.
     * @param status	    Zero if the operation completes successfully.
     */
    void (*on_accept_complete)(pj_equeue_key_t *key, int status);

    /**
     * This callback is called when #pj_equeue_connect completes.
     *
     * @param key	    The key.
     * @param status	    Zero if the operation completes successfully.
     */
    void (*on_connect_complete)(pj_equeue_key_t *key, int status);

} pj_io_callback;

/**
 * Event Queue options.
 */
typedef struct pj_equeue_options
{
    /** Maximum number of threads that are allowed to access Event Queue
     *  simulteneously.
     */
    unsigned	nb_threads;

    /** If non-zero, then no mutex protection will be used. */
    pj_bool_t	no_lock;

    /** Interval of the busy loop inside the event queue.
     *  The time resolution here determines the accuracy of the
     *  timer in the Event Queue.
     */
    pj_time_val	poll_interval;

} pj_equeue_options;


/**
 * Error value returned by I/O operations to indicate that the operation
 * can't complete immediately and will complete later.
 */
#define PJ_EQUEUE_PENDING   (-2)

/**
 * Types of Event Queue operation.
 */
typedef enum pj_equeue_op
{
    PJ_EQUEUE_OP_NONE		= 0,	/**< No operation.	    */
    PJ_EQUEUE_OP_READ		= 1,	/**< read() operation.	    */
    PJ_EQUEUE_OP_RECV_FROM	= 2,	/**< recvfrom() operation.  */
    PJ_EQUEUE_OP_WRITE		= 4,	/**< write() operation.	    */
    PJ_EQUEUE_OP_SEND_TO	= 8,	/**< sendto() operation.    */
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
    PJ_EQUEUE_OP_ACCEPT		= 16,	/**< accept() operation.    */
    PJ_EQUEUE_OP_CONNECT	= 32,	/**< connect() operation.   */
#endif	/* PJ_HAS_TCP */
} pj_equeue_op;



/**
 * Initialize Event Queue options with default values.
 *
 * @param options   Event Queue options.
 */
PJ_DECL(void) pj_equeue_options_init(pj_equeue_options *options);

/**
 * Create a new Event Queue framework.
 *
 * @param pool	    The pool to allocate the event queue structure.
 * @param options   Event queue options, or if NULL is given, then
 *		    default options will be used.
 * @param equeue    Pointer to receive event queue structure.
 *
 * @return	    zero on success.
 */
PJ_DECL(pj_status_t) pj_equeue_create( pj_pool_t *pool, 
				       const pj_equeue_options *options,
				       pj_equeue_t **equeue);

/**
 * Get the first instance of Event Queue, or NULL if no Event Queue
 * instance has been created in the application.
 *
 * @return	    The first instance of Event Queue created, or NULL.
 */
PJ_DECL(pj_equeue_t*) pj_equeue_instance(void);

/**
 * Destroy the Event Queue.
 *
 * @param equeue    The Event Queue instance to be destroyed.
 */
PJ_DECL(pj_status_t) pj_equeue_destroy( pj_equeue_t *equeue );

/**
 * Customize the lock object that is used by the Event Queue.
 *
 * @param equeue    The Event Queue instance.
 * @param lock	    The lock object.
 * @param auto_del  If non-zero, the lock will be destroyed by
 *		    Event Queue.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pj_equeue_set_lock( pj_equeue_t *equeue,
					 pj_lock_t *lock, 
					 pj_bool_t auto_del);

/**
 * Associate an Event Queue key to particular handle. The key is also
 * associated with the callback and user data, which will be used by
 * the Event Queue framework when signalling event back to application.
 *
 * @param pool	    To allocate the resource for the specified handle, which
 *		    must be valid until the handle/key is unregistered
 *		    from Event Queue.
 * @param equeue    The Event Queue.
 * @param hnd	    The OS handle to be registered, which can be a socket
 *		    descriptor (pj_sock_t), file descriptor, etc.
 * @param cb	    Callback to be called when I/O operation completes. 
 * @param user_data User data to be associated with the key.
 * @param key	    Pointer to receive the key.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pj_equeue_register( pj_pool_t *pool,
					 pj_equeue_t *equeue,
					 pj_oshandle_t hnd,
					 pj_io_callback *cb,
					 void *user_data,
					 pj_equeue_key_t **key);

/**
 * Retrieve user data associated with a key.
 *
 * @param key	    The Event Queue key.
 *
 * @return	    User data associated with the key.
 */
PJ_DECL(void*) pj_equeue_get_user_data( pj_equeue_key_t *key );


/**
 * Unregister Event Queue key from the Event Queue.
 *
 * @param equeue    The Event Queue.
 * @param key	    The key.
 *
 * @return	    Zero on success.
 */
PJ_DECL(pj_status_t) pj_equeue_unregister( pj_equeue_t *equeue,
					   pj_equeue_key_t *key);

/**
 * Instruct the Event Queue to read from the specified handle. This function
 * returns immediately (i.e. non-blocking) regardless whether some data has 
 * been transfered. If the operation can't complete immediately, caller will 
 * be notified about the completion when it calls pj_equeue_poll().
 *
 * @param key	    The key that uniquely identifies the handle.
 * @param buffer    The buffer to hold the read data. The caller MUST make sure
 *		    that this buffer remain valid until the framework completes
 *		    reading the handle.
 * @param size	    The maximum size to be read.
 *
 * @return
 *  - zero or positive number to indicate the number of bytes has been
 *		    read, and in this case the operation was not queued.
 *  - (-1) on error, which in this case operation was not queued.
 *  - PJ_EQUEUE_PENDING if the operation has been queued.
 */
PJ_DECL(pj_ssize_t) pj_equeue_read( pj_equeue_key_t *key,
				    void *buffer,
				    pj_size_t size);

/**
 * Start recv() operation on the specified handle.
 *
 * @see ::pj_ioqueue_read
 */
PJ_DECL(pj_ssize_t) pj_equeue_recv( pj_equeue_key_t *key,
				    void *buf,
				    pj_size_t size,
				    unsigned flags);

/**
 * Start recvfrom() operation on the specified handle.
 *
 * @see ::pj_equeue_read
 */
PJ_DECL(pj_ssize_t) pj_equeue_recvfrom( pj_equeue_key_t *key,
					void *buf,
					pj_size_t size,
					unsigned flags,
					pj_sockaddr_t *addr,
					int *addrlen );

/**
 * Write.
 */
PJ_DECL(pj_ssize_t) pj_equeue_write( pj_equeue_key_t *key,
				     const void *buf,
				     pj_size_t size);

/**
 * Send.
 */
PJ_DECL(pj_ssize_t) pj_equeue_send( pj_equeue_key_t *key,
				    const void *buf,
				    pj_size_t size,
				    unsigned flags);

/**
 * Sendto.
 */
PJ_DECL(pj_ssize_t) pj_equeue_sendto( pj_equeue_key_t *key,
				      const void *buf,
				      pj_size_t size,
				      unsigned flags,
				      const pj_sockaddr_t *addr,
				      int addrlen);

/**
 * Schedule timer.
 */
PJ_DECL(pj_status_t) pj_equeue_schedule_timer( pj_equeue_t *equeue,
					       const pj_time_val *timeout,
					       pj_timer_entry *entry);

/**
 * Cancel timer.
 */
PJ_DECL(pj_status_t) pj_equeue_cancel_timer( pj_equeue_t *equeue,
					     pj_timer_entry *entry);

/**
 * Poll for events.
 */
PJ_DECL(pj_status_t) pj_equeue_poll( pj_equeue_t *equeue,
				     const pj_time_val *timeout );

/**
 * Run.
 */
PJ_DECL(pj_status_t) pj_equeue_run( pj_equeue_t *equeue );

/**
 * Stop all running threads.
 */
PJ_DECL(pj_status_t) pj_equeue_stop( pj_equeue_t *equeue );


/** @} */

PJ_END_DECL

#endif	/* __PJ_EQUEUE_H__ */
