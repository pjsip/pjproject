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
#ifndef __PJ_OS_H__
#define __PJ_OS_H__

/**
 * @file os.h
 * @brief OS dependent functions
 */
#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_OS Operating System Dependent Functionality.
 * @ingroup PJ
 */


/* **************************************************************************/
/**
 * @defgroup PJ_THREAD Threads
 * @ingroup PJ_OS
 * @{
 * This module provides multithreading API.
 *
 * \section pj_thread_examples_sec Examples
 *
 * For examples, please see:
 *  - \ref page_pjlib_thread_test
 *  - \ref page_pjlib_sleep_test
 *
 */

/**
 * Thread creation flags:
 * - PJ_THREAD_SUSPENDED: specify that the thread should be created suspended.
 */
typedef enum pj_thread_create_flags
{
    PJ_THREAD_SUSPENDED = 1
} pj_thread_create_flags;


/**
 * Type of thread entry function.
 */
typedef int (PJ_THREAD_FUNC pj_thread_proc)(void*);

/**
 * Size of thread struct.
 */
#if !defined(PJ_THREAD_DESC_SIZE)
#   define PJ_THREAD_DESC_SIZE	    (16)
#endif

/**
 * Thread structure, to thread's state when the thread is created by external
 * or native API. 
 */
typedef long pj_thread_desc[PJ_THREAD_DESC_SIZE];

/**
 * Get process ID.
 * @return process ID.
 */
PJ_DECL(pj_uint32_t) pj_getpid(void);

/**
 * Create a new thread.
 *
 * @param pool          The memory pool from which the thread record 
 *                      will be allocated from.
 * @param thread_name   The optional name to be assigned to the thread.
 * @param proc          Thread entry function.
 * @param arg           Argument to be passed to the thread entry function.
 * @param stack_size    The size of the stack for the new thread, or ZERO or
 *                      PJ_THREAD_DEFAULT_STACK_SIZE to let the 
 *		        library choose the reasonable size for the stack. 
 *                      For some systems, the stack will be allocated from 
 *                      the pool, so the pool must have suitable capacity.
 * @param flags         Flags for thread creation, which is bitmask combination 
 *                      from enum pj_thread_create_flags.
 * @param thread        Pointer to hold the newly created thread.
 *
 * @return	        PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_thread_create(  pj_pool_t *pool, 
                                        const char *thread_name,
				        pj_thread_proc *proc, 
                                        void *arg,
				        pj_size_t stack_size, 
                                        unsigned flags,
					pj_thread_t **thread );

/**
 * Register a thread that was created by external or native API to PJLIB.
 * This function must be called in the context of the thread being registered.
 * When the thread is created by external function or API call,
 * it must be 'registered' to PJLIB using pj_thread_register(), so that it can
 * cooperate with PJLIB's framework. During registration, some data needs to
 * be maintained, and this data must remain available during the thread's 
 * lifetime.
 *
 * @param thread_name   The optional name to be assigned to the thread.
 * @param desc          Thread descriptor, which must be available throughout 
 *                      the lifetime of the thread.
 * @param thread        Pointer to hold the created thread handle.
 *
 * @return              PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_thread_register ( const char *thread_name,
					  pj_thread_desc desc,
					  pj_thread_t **thread);

/**
 * Get thread name.
 *
 * @param thread    The thread handle.
 *
 * @return Thread name as null terminated string.
 */
PJ_DECL(const char*) pj_thread_get_name(pj_thread_t *thread);

/**
 * Resume a suspended thread.
 *
 * @param thread    The thread handle.
 *
 * @return zero on success.
 */
PJ_DECL(pj_status_t) pj_thread_resume(pj_thread_t *thread);

/**
 * Get the current thread.
 *
 * @return Thread handle of current thread.
 */
PJ_DECL(pj_thread_t*) pj_thread_this(void);

/**
 * Join thread, and block the caller thread until the specified thread exits.
 * If the specified thread has already been dead, or it does not exist,
 * the function will return immediately with successfull status.
 *
 * @param thread    The thread handle.
 *
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_thread_join(pj_thread_t *thread);


/**
 * Destroy thread and release resources allocated for the thread.
 * However, the memory allocated for the pj_thread_t itself will only be released
 * when the pool used to create the thread is destroyed.
 *
 * @param thread    The thread handle.
 *
 * @return zero on success.
 */
PJ_DECL(pj_status_t) pj_thread_destroy(pj_thread_t *thread);


/**
 * Put the current thread to sleep for the specified miliseconds.
 *
 * @param msec Miliseconds delay.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_thread_sleep(unsigned msec);

/**
 * @def PJ_CHECK_STACK()
 * PJ_CHECK_STACK() macro is used to check the sanity of the stack.
 * The OS implementation may check that no stack overflow occurs, and
 * it also may collect statistic about stack usage.
 */
#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK!=0

#  define PJ_CHECK_STACK() pj_thread_check_stack(__FILE__, __LINE__)

/** @internal
 * The implementation of stack checking. 
 */
PJ_DECL(void) pj_thread_check_stack(const char *file, int line);

/** @internal
 * Get maximum stack usage statistic. 
 */
PJ_DECL(pj_uint32_t) pj_thread_get_stack_max_usage(pj_thread_t *thread);

/** @internal
 * Dump thread stack status. 
 */
PJ_DECL(pj_status_t) pj_thread_get_stack_info(pj_thread_t *thread,
					      const char **file,
					      int *line);
#else

#  define PJ_CHECK_STACK()
/** pj_thread_get_stack_max_usage() for the thread */
#  define pj_thread_get_stack_max_usage(thread)	    0
/** pj_thread_get_stack_info() for the thread */
#  define pj_thread_get_stack_info(thread,f,l)	    (*(f)="",*(l)=0)
#endif	/* PJ_OS_HAS_CHECK_STACK */

/**
 * @}
 */

/* **************************************************************************/
/**
 * @defgroup PJ_TLS Thread Local Storage.
 * @ingroup PJ_OS
 * @{
 */

/** 
 * Allocate thread local storage index. The initial value of the variable at
 * the index is zero.
 *
 * @param index	    Pointer to hold the return value.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_thread_local_alloc(long *index);

/**
 * Deallocate thread local variable.
 *
 * @param index	    The variable index.
 */
PJ_DECL(void) pj_thread_local_free(long index);

/**
 * Set the value of thread local variable.
 *
 * @param index	    The index of the variable.
 * @param value	    The value.
 */
PJ_DECL(pj_status_t) pj_thread_local_set(long index, void *value);

/**
 * Get the value of thread local variable.
 *
 * @param index	    The index of the variable.
 * @return	    The value.
 */
PJ_DECL(void*) pj_thread_local_get(long index);


/**
 * @}
 */


/* **************************************************************************/
/**
 * @defgroup PJ_ATOMIC Atomic Variables
 * @ingroup PJ_OS
 * @{
 *
 * This module provides API to manipulate atomic variables.
 *
 * \section pj_atomic_examples_sec Examples
 *
 * For some example codes, please see:
 *  - @ref page_pjlib_atomic_test
 */


/**
 * Create atomic variable.
 *
 * @param pool	    The pool.
 * @param initial   The initial value of the atomic variable.
 * @param atomic    Pointer to hold the atomic variable upon return.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_atomic_create( pj_pool_t *pool, 
				       pj_atomic_value_t initial,
				       pj_atomic_t **atomic );

/**
 * Destroy atomic variable.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pj_atomic_destroy( pj_atomic_t *atomic_var );

/**
 * Set the value of an atomic type, and return the previous value.
 *
 * @param atomic_var	the atomic variable.
 * @param value		value to be set to the variable.
 */
PJ_DECL(void) pj_atomic_set( pj_atomic_t *atomic_var, 
			     pj_atomic_value_t value);

/**
 * Get the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return the value of the atomic variable.
 */
PJ_DECL(pj_atomic_value_t) pj_atomic_get(pj_atomic_t *atomic_var);

/**
 * Increment the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 */
PJ_DECL(void) pj_atomic_inc(pj_atomic_t *atomic_var);

/**
 * Increment the value of an atomic type and get the result.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return              The incremented value.
 */
PJ_DECL(pj_atomic_value_t) pj_atomic_inc_and_get(pj_atomic_t *atomic_var);

/**
 * Decrement the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 */
PJ_DECL(void) pj_atomic_dec(pj_atomic_t *atomic_var);

/**
 * Decrement the value of an atomic type and get the result.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return              The decremented value.
 */
PJ_DECL(pj_atomic_value_t) pj_atomic_dec_and_get(pj_atomic_t *atomic_var);

/**
 * Add a value to an atomic type.
 *
 * @param atomic_var	The atomic variable.
 * @param value		Value to be added.
 */
PJ_DECL(void) pj_atomic_add( pj_atomic_t *atomic_var,
			     pj_atomic_value_t value);

/**
 * Add a value to an atomic type and get the result.
 *
 * @param atomic_var	The atomic variable.
 * @param value		Value to be added.
 *
 * @return              The result after the addition.
 */
PJ_DECL(pj_atomic_value_t) pj_atomic_add_and_get( pj_atomic_t *atomic_var,
			                          pj_atomic_value_t value);

/**
 * @}
 */

/* **************************************************************************/
/**
 * @defgroup PJ_MUTEX Mutexes.
 * @ingroup PJ_OS
 * @{
 *
 * Mutex manipulation. Alternatively, application can use higher abstraction
 * for lock objects, which provides uniform API for all kinds of lock 
 * mechanisms, including mutex. See @ref PJ_LOCK for more information.
 */

/**
 * Mutex types:
 *  - PJ_MUTEX_DEFAULT: default mutex type, which is system dependent.
 *  - PJ_MUTEX_SIMPLE: non-recursive mutex.
 *  - PJ_MUTEX_RECURSIVE: recursive mutex.
 */
typedef enum pj_mutex_type_e
{
    PJ_MUTEX_DEFAULT,
    PJ_MUTEX_SIMPLE,
    PJ_MUTEX_RECURSE
} pj_mutex_type_e;


/**
 * Create mutex of the specified type.
 *
 * @param pool	    The pool.
 * @param name	    Name to be associated with the mutex (for debugging).
 * @param type	    The type of the mutex, of type #pj_mutex_type_e.
 * @param mutex	    Pointer to hold the returned mutex instance.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_create(pj_pool_t *pool, 
                                     const char *name,
				     int type, 
                                     pj_mutex_t **mutex);

/**
 * Create simple, non-recursive mutex.
 * This function is a simple wrapper for #pj_mutex_create to create 
 * non-recursive mutex.
 *
 * @param pool	    The pool.
 * @param name	    Mutex name.
 * @param mutex	    Pointer to hold the returned mutex instance.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_create_simple( pj_pool_t *pool, const char *name,
					     pj_mutex_t **mutex );

/**
 * Create recursive mutex.
 * This function is a simple wrapper for #pj_mutex_create to create 
 * recursive mutex.
 *
 * @param pool	    The pool.
 * @param name	    Mutex name.
 * @param mutex	    Pointer to hold the returned mutex instance.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_create_recursive( pj_pool_t *pool,
					        const char *name,
						pj_mutex_t **mutex );

/**
 * Acquire mutex lock.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_lock(pj_mutex_t *mutex);

/**
 * Release mutex lock.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_unlock(pj_mutex_t *mutex);

/**
 * Try to acquire mutex lock.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code if the
 *		    lock couldn't be acquired.
 */
PJ_DECL(pj_status_t) pj_mutex_trylock(pj_mutex_t *mutex);

/**
 * Destroy mutex.
 *
 * @param mutex	    Te mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_destroy(pj_mutex_t *mutex);

/**
 * Determine whether calling thread is owning the mutex (only available when
 * PJ_DEBUG is set).
 * @param mutex	    The mutex.
 * @return	    Non-zero if yes.
 */
#if defined(PJ_DEBUG) && PJ_DEBUG != 0
   PJ_DECL(pj_bool_t) pj_mutex_is_locked(pj_mutex_t *mutex);
#else
#  define pj_mutex_is_locked(mutex)	    1
#endif

/**
 * @}
 */

/* **************************************************************************/
/**
 * @defgroup PJ_RW_MUTEX Reader/Writer Mutex
 * @ingroup PJ_OS
 * @{
 * Reader/writer mutex is a classic synchronization object where multiple
 * readers can acquire the mutex, but only a single writer can acquire the 
 * mutex.
 */

/**
 * Opaque declaration for reader/writer mutex.
 * Reader/writer mutex is a classic synchronization object where multiple
 * readers can acquire the mutex, but only a single writer can acquire the 
 * mutex.
 */
typedef struct pj_rwmutex_t pj_rwmutex_t;

/**
 * Create reader/writer mutex.
 *
 * @param pool	    Pool to allocate memory for the mutex.
 * @param name	    Name to be assigned to the mutex.
 * @param mutex	    Pointer to receive the newly created mutex.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_create(pj_pool_t *pool, const char *name,
				       pj_rwmutex_t **mutex);

/**
 * Lock the mutex for reading.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_lock_read(pj_rwmutex_t *mutex);

/**
 * Lock the mutex for writing.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_lock_write(pj_rwmutex_t *mutex);

/**
 * Release read lock.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_unlock_read(pj_rwmutex_t *mutex);

/**
 * Release write lock.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_unlock_write(pj_rwmutex_t *mutex);

/**
 * Destroy reader/writer mutex.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_destroy(pj_rwmutex_t *mutex);


/**
 * @}
 */


/* **************************************************************************/
/**
 * @defgroup PJ_CRIT_SEC Critical sections.
 * @ingroup PJ_OS
 * @{
 * Critical section protection can be used to protect regions where:
 *  - mutual exclusion protection is needed.
 *  - it's rather too expensive to create a mutex.
 *  - the time spent in the region is very very brief.
 *
 * Critical section is a global object, and it prevents any threads from
 * entering any regions that are protected by critical section once a thread
 * is already in the section.
 *
 * Critial section is \a not recursive!
 *
 * Application <b>MUST NOT</b> call any functions that may cause current
 * thread to block (such as allocating memory, performing I/O, locking mutex,
 * etc.) while holding the critical section.
 */
/**
 * Enter critical section.
 */
PJ_DECL(void) pj_enter_critical_section(void);

/**
 * Leave critical section.
 */
PJ_DECL(void) pj_leave_critical_section(void);

/**
 * @}
 */

/* **************************************************************************/
#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0
/**
 * @defgroup PJ_SEM Semaphores.
 * @ingroup PJ_OS
 * @{
 *
 * This module provides abstraction for semaphores, where available.
 */

/**
 * Create semaphore.
 *
 * @param pool	    The pool.
 * @param name	    Name to be assigned to the semaphore (for logging purpose)
 * @param initial   The initial count of the semaphore.
 * @param max	    The maximum count of the semaphore.
 * @param sem	    Pointer to hold the semaphore created.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_create( pj_pool_t *pool, 
                                    const char *name,
				    unsigned initial, 
                                    unsigned max,
				    pj_sem_t **sem);

/**
 * Wait for semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_wait(pj_sem_t *sem);

/**
 * Try wait for semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_trywait(pj_sem_t *sem);

/**
 * Release semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_post(pj_sem_t *sem);

/**
 * Destroy semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_destroy(pj_sem_t *sem);

/**
 * @}
 */
#endif	/* PJ_HAS_SEMAPHORE */


/* **************************************************************************/
#if defined(PJ_HAS_EVENT_OBJ) && PJ_HAS_EVENT_OBJ != 0
/**
 * @defgroup PJ_EVENT Event Object.
 * @ingroup PJ_OS
 * @{
 *
 * This module provides abstraction to event object (e.g. Win32 Event) where
 * available. Event objects can be used for synchronization among threads.
 */

/**
 * Create event object.
 *
 * @param pool		The pool.
 * @param name		The name of the event object (for logging purpose).
 * @param manual_reset	Specify whether the event is manual-reset
 * @param initial	Specify the initial state of the event object.
 * @param event		Pointer to hold the returned event object.
 *
 * @return event handle, or NULL if failed.
 */
PJ_DECL(pj_status_t) pj_event_create(pj_pool_t *pool, const char *name,
				     pj_bool_t manual_reset, pj_bool_t initial,
				     pj_event_t **event);

/**
 * Wait for event to be signaled.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_wait(pj_event_t *event);

/**
 * Try wait for event object to be signalled.
 *
 * @param event The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_trywait(pj_event_t *event);

/**
 * Set the event object state to signaled. For auto-reset event, this 
 * will only release the first thread that are waiting on the event. For
 * manual reset event, the state remains signaled until the event is reset.
 * If there is no thread waiting on the event, the event object state 
 * remains signaled.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_set(pj_event_t *event);

/**
 * Set the event object to signaled state to release appropriate number of
 * waiting threads and then reset the event object to non-signaled. For
 * manual-reset event, this function will release all waiting threads. For
 * auto-reset event, this function will only release one waiting thread.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_pulse(pj_event_t *event);

/**
 * Set the event object state to non-signaled.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_reset(pj_event_t *event);

/**
 * Destroy the event object.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_destroy(pj_event_t *event);

/**
 * @}
 */
#endif	/* PJ_HAS_EVENT_OBJ */

/* **************************************************************************/
/**
 * @addtogroup PJ_TIME Time Data Type and Manipulation.
 * @ingroup PJ_OS
 * @{
 * This module provides API for manipulating time.
 *
 * \section pj_time_examples_sec Examples
 *
 * For examples, please see:
 *  - \ref page_pjlib_sleep_test
 */

/**
 * Get current time of day in local representation.
 *
 * @param tv	Variable to store the result.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_gettimeofday(pj_time_val *tv);


/**
 * Parse time value into date/time representation.
 *
 * @param tv	The time.
 * @param pt	Variable to store the date time result.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_time_decode(const pj_time_val *tv, pj_parsed_time *pt);

/**
 * Encode date/time to time value.
 *
 * @param pt	The date/time.
 * @param tv	Variable to store time value result.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_time_encode(const pj_parsed_time *pt, pj_time_val *tv);

/**
 * Convert local time to GMT.
 *
 * @param tv	Time to convert.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_time_local_to_gmt(pj_time_val *tv);

/**
 * Convert GMT to local time.
 *
 * @param tv	Time to convert.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_time_gmt_to_local(pj_time_val *tv);

/**
 * @}
 */

/* **************************************************************************/
#if defined(PJ_TERM_HAS_COLOR) && PJ_TERM_HAS_COLOR != 0

/**
 * @defgroup PJ_TERM Terminal
 * @ingroup PJ_OS
 * @{
 */

/**
 * Set current terminal color.
 *
 * @param color	    The RGB color.
 *
 * @return zero on success.
 */
PJ_DECL(pj_status_t) pj_term_set_color(pj_color_t color);

/**
 * Get current terminal foreground color.
 *
 * @return RGB color.
 */
PJ_DECL(pj_color_t) pj_term_get_color(void);

/**
 * @}
 */

#endif	/* PJ_TERM_HAS_COLOR */

/* **************************************************************************/
/**
 * @defgroup PJ_TIMESTAMP High Resolution Timestamp
 * @ingroup PJ_OS
 * @{
 *
 * PJLIB provides <b>High Resolution Timestamp</b> API to access highest 
 * resolution timestamp value provided by the platform. The API is usefull
 * to measure precise elapsed time, and can be used in applications such
 * as profiling.
 *
 * The timestamp value is represented in cycles, and can be related to
 * normal time (in seconds or sub-seconds) using various functions provided.
 *
 * \section pj_timestamp_examples_sec Examples
 *
 * For examples, please see:
 *  - \ref page_pjlib_sleep_test
 *  - \ref page_pjlib_timestamp_test
 */

/*
 * High resolution timer.
 */
#if defined(PJ_HAS_HIGH_RES_TIMER) && PJ_HAS_HIGH_RES_TIMER != 0

/**
 * Acquire high resolution timer value. The time value are stored
 * in cycles.
 *
 * @param ts	    High resolution timer value.
 * @return	    PJ_SUCCESS or the appropriate error code.
 *
 * @see pj_get_timestamp_freq().
 */
PJ_DECL(pj_status_t) pj_get_timestamp(pj_timestamp *ts);

/**
 * Get high resolution timer frequency, in cycles per second.
 *
 * @param freq	    Timer frequency, in cycles per second.
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq);

/**
 * Add timestamp t2 to t1.
 * @param t1	    t1.
 * @param t2	    t2.
 */
PJ_INLINE(void) pj_add_timestamp(pj_timestamp *t1, const pj_timestamp *t2)
{
#if PJ_HAS_INT64
    t1->u64 += t2->u64;
#else
    pj_uint32_t old = t1->u32.lo;
    t1->u32.hi += t2->u32.hi;
    t1->u32.lo += t2->u32.lo;
    if (t1->u32.lo < old)
	++t1->u32.hi;
#endif
}

/**
 * Substract timestamp t2 from t1.
 * @param t1	    t1.
 * @param t2	    t2.
 */
PJ_INLINE(void) pj_sub_timestamp(pj_timestamp *t1, const pj_timestamp *t2)
{
#if PJ_HAS_INT64
    t1->u64 -= t2->u64;
#else
    t1->u32.hi -= t2->u32.hi;
    if (t1->u32.lo >= t2->u32.lo)
	t1->u32.lo -= t2->u32.lo;
    else {
	t1->u32.lo -= t2->u32.lo;
	--t1->u32.hi;
    }
#endif
}

/**
 * Calculate the elapsed time, and store it in pj_time_val.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time as #pj_time_val.
 *
 * @see pj_elapsed_usec(), pj_elapsed_cycle(), pj_elapsed_nanosec()
 */
PJ_DECL(pj_time_val) pj_elapsed_time( const pj_timestamp *start,
                                      const pj_timestamp *stop );

/**
 * Calculate the elapsed time as 32-bit miliseconds.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in milisecond.
 *
 * @see pj_elapsed_time(), pj_elapsed_cycle(), pj_elapsed_nanosec()
 */
PJ_DECL(pj_uint32_t) pj_elapsed_msec( const pj_timestamp *start,
                                      const pj_timestamp *stop );

/**
 * Calculate the elapsed time in 32-bit microseconds.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in microsecond.
 *
 * @see pj_elapsed_time(), pj_elapsed_cycle(), pj_elapsed_nanosec()
 */
PJ_DECL(pj_uint32_t) pj_elapsed_usec( const pj_timestamp *start,
                                      const pj_timestamp *stop );

/**
 * Calculate the elapsed time in 32-bit nanoseconds.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in nanoseconds.
 *
 * @see pj_elapsed_time(), pj_elapsed_cycle(), pj_elapsed_usec()
 */
PJ_DECL(pj_uint32_t) pj_elapsed_nanosec( const pj_timestamp *start,
                                         const pj_timestamp *stop );

/**
 * Calculate the elapsed time in 32-bit cycles.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in cycles.
 *
 * @see pj_elapsed_usec(), pj_elapsed_time(), pj_elapsed_nanosec()
 */
PJ_DECL(pj_uint32_t) pj_elapsed_cycle( const pj_timestamp *start,
                                       const pj_timestamp *stop );


#endif	/* PJ_HAS_HIGH_RES_TIMER */

/** @} */


/* **************************************************************************/
/**
 * Internal PJLIB function to initialize the threading subsystem.
 * @return          PJ_SUCCESS or the appropriate error code.
 */
pj_status_t pj_thread_init(void);


PJ_END_DECL

#endif  /* __PJ_OS_H__ */

