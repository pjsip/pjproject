/* $Header: /pjproject/pjlib/src/pj/os.h 6     5/05/05 11:34p Bennylp $ */
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

/**
 * Log the last error message.
 * @param format format of string message preceeding the error.
 */
PJ_DECL(void) pj_perror(const char *src, const char *format, ...);

/**
 * Wrapper for pj_perror.
 * Sample usage: PJ_PERROR((__FILE__, "bind() to addr %s error", addr));
 */
#define PJ_PERROR(ARGS)   pj_perror ARGS

/**
 * Get error/status of the last operation.
 * @return OS dependent error code.
 */
PJ_DECL(pj_status_t) pj_getlasterror(void);


///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJ_THREAD Threads
 * @ingroup PJ_OS
 * @{
 */

/**
 * Thread creation flags:
 * - PJ_THREAD_SUSPENDED: specify that the thread should be created suspended.
 * -
 */
typedef enum pj_thread_create_flags
{
    PJ_THREAD_SUSPENDED = 1
} pj_thread_create_flags;


/**
 * Type of thread entry function.
 */
typedef void *(PJ_THREAD_FUNC pj_thread_proc)(void*);

/**
 * Thread structure, to thread's state when the thread is created by external
 * or native API. 
 */
typedef pj_uint8_t pj_thread_desc[32];

/**
 * Get process ID.
 * @return process ID.
 */
PJ_DECL(pj_uint32_t) pj_getpid(void);

/**
 * Create a new thread.
 * @param pool 
 *	       The memory pool from which the thread record will be allocated 
 *             from.
 * @param thread_name 
 *		The optional name to be assigned to the thread.
 * @param proc 
 *		Thread entry function.
 * @param arg  
 *	        Argument to be passed to the thread entry function.
 * @param stack_size 
 *		The size of the stack for the new thread, or ZERO for the
 *	        default stack size.
 * @param stack_ptr
 *		Optional pointer to the buffer to be used by the thread, or
 *		NULL if the caller doesn't wish to supply the buffer. Not
 *		all operating systems support this feature. If the OS doesn't
 *		support this feature, then this argument must be NULL.
 * @param flags
 *		Flags for thread creation, which is bitmask combination from
 *		enum pj_thread_create_flags.
 *
 * @return
 *  The handle for the new thread, or NULL if the thread can NOT be created.
 */
PJ_DECL(pj_thread_t*) pj_thread_create( pj_pool_t *pool, const char *thread_name,
				        pj_thread_proc *proc, void *arg,
				        pj_size_t stack_size, void *stack_ptr, 
				        unsigned flags);

/**
 * Register a thread that was created by external or native API to PJLIB.
 * This function must be called in the context of the thread being registered.
 * When the thread is created by external function or API call,
 * it must be 'registered' to PJLIB using pj_thread_register(), so that it can
 * cooperate with PJLIB's framework. During registration, some data needs to
 * be maintained, and this data must remain available during the thread's 
 * lifetime.
 * @param thread_name
 *	    The optional name to be assigned to the thread.
 * @param desc
 *	    Thread descriptor, which must be available throughout the lifetime
 *	    of the thread.
 * @return
 *  The handle for the thread.
 */
PJ_DECL(pj_thread_t*) pj_thread_register (const char *thread_name,
					  pj_thread_desc desc);

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
 * Join thread.
 * This function will block the caller thread until the specified thread exits.
 *
 * @param thread    The thread handle.
 *
 * @return zero on success.
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
 * Put the current thread to sleep for the specified microseconds.
 *
 * @param usec Microseconds delay.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_thread_usleep(unsigned usec);

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJ_TLS Thread Local Storage.
 * @ingroup PJ_OS
 * @{
 */

/** 
 * Allocate thread local storage index. The initial value of the variable at
 * the index is zero.
 *
 * @return the index, or -1 when an index can not be allocated.
 */
PJ_DECL(long) pj_thread_local_alloc(void);

/**
 * Deallocate thread local variable.
 *
 * @param index the variable index.
 */
PJ_DECL(void) pj_thread_local_free(long index);

/**
 * Set the value of thread local variable.
 *
 * @param index	    the index of the variable.
 * @param value	    the value.
 */
PJ_DECL(void) pj_thread_local_set(long index, void *value);

/**
 * Get the value of thread local variable.
 *
 * @param index the index of the variable.
 * @return the value.
 */
PJ_DECL(void*) pj_thread_local_get(long index);


/**
 * @}
 */


///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJ_ATOMIC Atomic Operations.
 * @ingroup PJ_OS
 * @{
 */


/**
 * Create atomic variable.
 *
 * @param pool	    the pool.
 * @param initial   the initial value of the atomic variable.
 *
 * @return the atomic variable, or NULL if failed.
 */
PJ_DECL(pj_atomic_t*) pj_atomic_create( pj_pool_t *pool, long initial );

/**
 * Destroy atomic variable.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return PJ_OK if success.
 */
PJ_DECL(pj_status_t) pj_atomic_destroy( pj_atomic_t *atomic_var );

/**
 * Set the value of an atomic type, and return the previous value.
 *
 * @param atomic_var	the atomic variable.
 * @param value		value to be set to the variable.
 *
 * @return the previous value of the variable.
 */
PJ_DECL(long) pj_atomic_set(pj_atomic_t *atomic_var, long value);

/**
 * Get the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return the value of the atomic variable.
 */
PJ_DECL(long) pj_atomic_get(pj_atomic_t *atomic_var);

/**
 * Increment the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return the result.
 */
PJ_DECL(long) pj_atomic_inc(pj_atomic_t *atomic_var);

/**
 * Decrement the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return the result.
 */
PJ_DECL(long) pj_atomic_dec(pj_atomic_t *atomic_var);

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJ_MUTEX Mutexes.
 * @ingroup PJ_OS
 * @{
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
    PJ_MUTEX_RECURSE,
} pj_mutex_type_e;

/**
 * Create mutex.
 *
 * @param pool	    the pool.
 * @param name	    Name to be associated with the mutex (for debugging).
 * @param type	    The type of the mutex, of type \a pj_mutex_type_e.
 *
 * @return the mutex, or NULL if failed.
 */
PJ_DECL(pj_mutex_t*) pj_mutex_create(pj_pool_t *pool, const char *name,
				     int type);

/**
 * Acquire mutex lock.
 *
 * @param mutex the mutex.
 * @return PJ_OK (zero) if success.
 */
PJ_DECL(pj_status_t) pj_mutex_lock(pj_mutex_t *mutex);

/**
 * Release mutex lock.
 *
 * @param mutex the mutex.
 * @return PJ_OK (zero) if success.
 */
PJ_DECL(pj_status_t) pj_mutex_unlock(pj_mutex_t *mutex);

/**
 * Try to acquire mutex lock.
 *
 * @param mutex the mutex.
 * @return PJ_OK (zero) if success, or -1 if lock can not be acquired.
 */
PJ_DECL(pj_status_t) pj_mutex_trylock(pj_mutex_t *mutex);

/**
 * Destroy mutex.
 *
 * @param mutex the mutex.
 * @return PJ_OK (zero) if success.
 */
PJ_DECL(pj_status_t) pj_mutex_destroy(pj_mutex_t *mutex);

#if PJ_DEBUG
/**
 * Determine whether calling thread is owning the mutex (only available when
 * PJ_DEBUG is set).
 * @param mutex the mutex.
 * @return non-zero if yes.
 */
PJ_DECL(pj_status_t) pj_mutex_is_locked(pj_mutex_t *mutex);
#endif

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJ_CRIT_SEC Critical sections.
 * @ingroup PJ_OS
 * @{
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

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJ_SEM Semaphores.
 * @ingroup PJ_OS
 * @{
 */

/**
 * Create semaphore.
 *
 * @param pool	    The pool.
 * @param name	    Name to be assigned to the semaphore (for logging purpose)
 * @param initial   The initial count of the semaphore.
 * @param max	    The maximum count of the semaphore.
 *
 * @return semaphore handle, or NULL if failed.
 */
PJ_DECL(pj_sem_t*) pj_sem_create(pj_pool_t *pool, const char *name,
				 unsigned initial, unsigned max);

/**
 * Wait for semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_sem_wait(pj_sem_t *sem);

/**
 * Try wait for semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_sem_trywait(pj_sem_t *sem);

/**
 * Release semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_sem_post(pj_sem_t *sem);

/**
 * Destroy semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_sem_destroy(pj_sem_t *sem);

/**
 * @}
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJ_EVENT Event Object.
 * @ingroup PJ_OS
 * @{
 */

/**
 * Create event object.
 *
 * @param pool		The pool.
 * @param name		The name of the event object (for logging purpose).
 * @param manual_reset	Specify whether the event is manual-reset
 * @param initial	Specify the initial state of the event object.
 *
 * @return event handle, or NULL if failed.
 */
PJ_DECL(pj_event_t*) pj_event_create(pj_pool_t *pool, const char *name,
				     pj_bool_t manual_reset, pj_bool_t initial);

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


///////////////////////////////////////////////////////////////////////////////
/**
 * @addtogroup PJ_TIME Time Data Type and Manipulation.
 * @ingroup PJ_OS
 * @{
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

///////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////
/*
 * High resolution timer.
 */
#if PJ_HAS_HIGH_RES_TIMER

/**
 * This structure represents high resolution (64bit) time value. The meaning
 * of the value depends on the resolution of the timer used.
 */
typedef union pj_hr_timestamp
{
    struct
    {
#if PJ_IS_LITTLE_ENDIAN
	pj_uint32_t lo;
	pj_uint32_t hi;
#else
#   error "You've got to see and fix this!"
#endif
    } u32;

#if PJ_HAS_INT64
    pj_uint64_t u64;
#endif
} pj_hr_timestamp;

/**
 * Acquire high resolution timer value.
 *
 * @param ts	High resolution timer value.
 */
PJ_INLINE(void) pj_hr_gettimestamp(pj_hr_timestamp *ts)
{
#if defined(_MSC_VER) && defined(PJ_HAS_PENTIUM) && PJ_HAS_PENTIUM != 0
    __asm 
    {
	RDTSC
	MOV EBX, DWORD PTR [ts]
	MOV DWORD ptr [EBX]+4, EDX
	MOV DWORD ptr [EBX], EAX
    }
#elif defined(__GNUC__) && defined(PJ_HAS_PENTIUM) && PJ_HAS_PENTIUM != 0
    __asm__ __volatile__ ( "rdtsc"
			   : "=a" (ts->u32.lo), "=d" (ts->u32.hi));
#elif defined(PJ_WIN32)
    ts->hi = 0;
    ts->lo = GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts->hi = tv.tv_sec;
    ts->lo = tv.tv_usec;
#endif
}


#endif	/* PJ_HAS_HIGH_RES_TIMER */


///////////////////////////////////////////////////////////////////////////////
/*
 * Private functions.
 */
pj_status_t pj_thread_init(void);


PJ_END_DECL

#endif

