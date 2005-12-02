/* $Header: /pjproject/pjlib/src/pj/os_unix.c 5     6/24/05 4:38p Bennylp $ */
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
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/guid.h>
#include <stddef.h>
#include <sys/timeb.h>
#include <time.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/time.h>
#define __USE_GNU
#include <pthread.h>

#define THIS_FILE   "osunix"

struct pj_thread_t
{
    char	    obj_name[PJ_MAX_OBJ_NAME];
    pthread_t	    thread;
    pj_thread_proc *proc;
    void	   *arg;
};

struct pj_atomic_t
{
    pj_mutex_t	*mutex;
    long	 value;
};

struct pj_mutex_t
{
    pthread_mutex_t     mutex;
    char		obj_name[PJ_MAX_OBJ_NAME];
#if PJ_DEBUG
    int		        nesting_level;
    pj_thread_t	       *owner;
#endif
};


struct pj_sem_t
{
    sem_t		sem;
    char		obj_name[PJ_MAX_OBJ_NAME];
};

struct pj_event_t
{
    char		obj_name[PJ_MAX_OBJ_NAME];
};

#if PJ_HAS_THREADS
static pj_thread_t main_thread;
static int thread_tls_id;
static pj_mutex_t critical_section;
#endif


#if !PJ_HAS_THREADS
#define MAX_THREADS 16
static int tls_flag[MAX_THREADS];
static void *tls[MAX_THREADS];
#endif

static pj_status_t init_mutex(pj_mutex_t *mutex, const char *name, int type);

PJ_DEF(pj_status_t) pj_init(void)
{
    char dummy_guid[PJ_GUID_LENGTH];
    pj_str_t guid;

    PJ_LOG(5, ("pj_init", "Initializing PJ Library.."));

    /* Init this thread's TLS. */
#if PJ_HAS_THREADS
    if (pj_thread_init() != 0) {
	PJ_LOG(1, ("pj_init", "Thread initialization has returned an error"));
	return -1;
    }

    /* Critical section. */
    if (init_mutex(&critical_section, "critsec", PJ_MUTEX_SIMPLE) != 0)
	return -1;

#endif
    
    /* Init random seed. */
    srand( clock() );

    /* Startup GUID. */
    guid.ptr = dummy_guid;
    pj_generate_unique_string( &guid );


    return PJ_OK;
}

PJ_DEF(pj_uint32_t) pj_getpid(void)
{
    return getpid();
}

PJ_DEF(void) pj_perror(const char *src, const char *format, ...)
{
    char title[80];
    va_list marker;

    va_start(marker, format);
    vsnprintf(title, sizeof(title), format, marker);
    va_end(marker);

    /* Do not do title[len]=X here. 
     * len may be greater than sizeof(title) according to C99.
     */

    PJ_LOG(1, (src, "%s: (errno=%d) %s", title, errno, strerror(errno)));
}

PJ_DEF(pj_status_t) pj_getlasterror(void)
{
    return errno;
}

PJ_DEF(pj_thread_t*) pj_thread_register (const char *cstr_thread_name,
					 pj_thread_desc desc)
{
#if PJ_HAS_THREADS
    pj_thread_t *thread = (pj_thread_t *)desc;
    pj_str_t thread_name = pj_str((char*)cstr_thread_name);

    /* Size sanity check. */
    if (sizeof(pj_thread_desc) < sizeof(pj_thread_t)) {
	pj_assert(!"Not enough pj_thread_desc size!");
	return NULL;
    }

    /* If a thread descriptor has been registered before, just return it. */
    if (pj_thread_local_get (thread_tls_id) != 0) {
	return (pj_thread_t*)pj_thread_local_get (thread_tls_id);
    }

    /* Initialize and set the thread entry. */
    pj_memset(desc, 0, sizeof(pj_thread_desc));
    thread->thread = pthread_self();

    if (cstr_thread_name && pj_strlen(&thread_name) < sizeof(thread->obj_name)-1)
	sprintf(thread->obj_name, cstr_thread_name, thread->thread);
    else
	sprintf(thread->obj_name, "thr%p", (void*)thread->thread);
    
    pj_thread_local_set(thread_tls_id, thread);

    return thread;
#else
    pj_thread_t *thread = (pj_thread_t*)desc;
    return thread;
#endif
}


pj_status_t pj_thread_init(void)
{
#if PJ_HAS_THREADS
    pj_memset(&main_thread, 0, sizeof(main_thread));
    main_thread.thread = pthread_self();
    sprintf(main_thread.obj_name, "thr%p", &main_thread);

    thread_tls_id = pj_thread_local_alloc();
    if (thread_tls_id == -1) {
	return -1;
    }

    pj_thread_local_set(thread_tls_id, &main_thread);
    return PJ_OK;
#else
    PJ_LOG(2,(THIS_FILE, "Thread init error. Threading is not enabled!"));
    return -1;
#endif
}

#if PJ_HAS_THREADS
static void *thread_main(void *param)
{
    pj_thread_t *rec = param;
    void *result;

    PJ_LOG(6,(rec->obj_name, "Thread started"));

    pj_thread_local_set(thread_tls_id, rec);
    result = (*rec->proc)(rec->arg);

    PJ_LOG(6,(rec->obj_name, "Thread quitting"));
    return result;
}
#endif

PJ_DEF(pj_thread_t*) pj_thread_create(pj_pool_t *pool, const char *thread_name,
				      pj_thread_proc *proc, void *arg,
				      pj_size_t stack_size, void *stack, 
				      unsigned flags)
{
#if PJ_HAS_THREADS
    pj_thread_t *rec;

    /* We don't implement stack setting for now.. */
    PJ_TODO(IMPLEMENT_THREAD_STACK);    

    /* We don't implement suspended state for threads */
    if (flags & PJ_THREAD_SUSPENDED) {
	PJ_LOG(2, (THIS_FILE, "Unsupported: PJ_THREAD_SUSPENDED flag in pj_thread_create()"));
	return NULL;
    }

    /* Create thread record and assign name for the thread */
    rec = (struct pj_thread_t*) pj_pool_calloc(pool, 1, sizeof(pj_thread_t));
    if (!rec) {
	return NULL;
    }
    /* Set name. */
    if (!thread_name) {
	thread_name = "thr%p";
    }
    if (strchr(thread_name, '%')) {
	pj_snprintf(rec->obj_name, PJ_MAX_OBJ_NAME, thread_name, rec);
    } else {
	strncpy(rec->obj_name, thread_name, PJ_MAX_OBJ_NAME);
	rec->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
    }

    PJ_LOG(6, (rec->obj_name, "Thread created"));

    /* Create the thread. */
    rec->proc = proc;
    rec->arg = arg;
    if (pthread_create( &rec->thread, NULL, thread_main, rec) != 0) {
	PJ_LOG(2, (THIS_FILE, "Error creating thread"));
	return NULL;
    }

    return rec;
#else
    PJ_LOG(2,(THIS_FILE, "Unable to create thread. Threading is not enabled!"));
    return NULL;
#endif
}

PJ_DEF(const char*) pj_thread_get_name(pj_thread_t *p)
{
#if PJ_HAS_THREADS
    pj_thread_t *rec = (pj_thread_t*)p;
    return rec->obj_name;
#else
    return "";
#endif
}

PJ_DEF(pj_status_t) pj_thread_resume(pj_thread_t *p)
{
    PJ_LOG(2, (THIS_FILE, "Unsupported: pj_thread_resume()"));
    return -1;
}

PJ_DEF(pj_thread_t*) pj_thread_this(void)
{
#if PJ_HAS_THREADS
    pj_thread_t *rec = pj_thread_local_get(thread_tls_id);
    pj_assert(rec != NULL);
    return rec;
#else
    PJ_LOG(2, (THIS_FILE, "pj_thread_this() unsupported: thread is not enabled!"));
    return 0;
#endif
}

PJ_DEF(pj_status_t) pj_thread_join(pj_thread_t *p)
{
#if PJ_HAS_THREADS
    pj_thread_t *rec = (pj_thread_t *)p;
    void *ret;

    PJ_LOG(6, (pj_thread_this()->obj_name, "Joining thread %s", p->obj_name));
    return pthread_join( rec->thread, &ret);
#else
    PJ_LOG(2, (THIS_FILE, "pj_thread_join() unsupported: thread is not enabled!"));
    return 0;
#endif
}

PJ_DEF(pj_status_t) pj_thread_destroy(pj_thread_t *p)
{
    return PJ_OK;
}

PJ_DEF(pj_status_t) pj_thread_sleep(unsigned msec)
{
    return usleep(msec * 1000);
}

///////////////////////////////////////////////////////////////////////////////
PJ_DEF(pj_atomic_t*) pj_atomic_create( pj_pool_t *pool, long value)
{
    pj_atomic_t *t = pj_pool_calloc(pool, 1, sizeof(pj_atomic_t));
    if (!t)
	return NULL;
#if PJ_HAS_THREADS
    t->mutex = pj_mutex_create(pool, "atm%p", PJ_MUTEX_SIMPLE);
    if (t->mutex == NULL)
	return NULL;
#endif
    t->value = value;
    return t;
}

PJ_DEF(pj_status_t) pj_atomic_destroy( pj_atomic_t *atomic_var )
{
#if PJ_HAS_THREADS
    return pj_mutex_destroy( atomic_var->mutex );
#else
    return 0;
#endif
}

PJ_DEF(long) pj_atomic_set(pj_atomic_t *atomic_var, long value)
{
    long oldval;
    
#if PJ_HAS_THREADS
    pj_mutex_lock( atomic_var->mutex );
#endif
    oldval = atomic_var->value;
    atomic_var->value = value;
#if PJ_HAS_THREADS
    pj_mutex_unlock( atomic_var->mutex);
#endif 
    return oldval;
}

PJ_DEF(long) pj_atomic_get(pj_atomic_t *atomic_var)
{
    long oldval;
    
#if PJ_HAS_THREADS
    pj_mutex_lock( atomic_var->mutex );
#endif
    oldval = atomic_var->value;
#if PJ_HAS_THREADS
    pj_mutex_unlock( atomic_var->mutex);
#endif
    return oldval;
}

PJ_DEF(long) pj_atomic_inc(pj_atomic_t *atomic_var)
{
    long newval;

#if PJ_HAS_THREADS
    pj_mutex_lock( atomic_var->mutex );
#endif
    newval = atomic_var->value++;
#if PJ_HAS_THREADS
    pj_mutex_unlock( atomic_var->mutex);
#endif
    return newval;
}

PJ_DEF(long) pj_atomic_dec(pj_atomic_t *atomic_var)
{
    long newval;

#if PJ_HAS_THREADS
    pj_mutex_lock( atomic_var->mutex );
#endif
    newval = atomic_var->value--;
#if PJ_HAS_THREADS
    pj_mutex_unlock( atomic_var->mutex);
#endif
    return newval;
}


///////////////////////////////////////////////////////////////////////////////
PJ_DEF(long) pj_thread_local_alloc(void)
{
#if PJ_HAS_THREADS
    pthread_key_t key;

    pj_assert( sizeof(pthread_key_t) <= sizeof(long));
    if (pthread_key_create(&key, NULL))
	return -1;
    return key;
#else
    int i;
    for (i=0; i<MAX_THREADS; ++i) {
	if (tls_flag[i] == 0)
	    break;
    }
    if (i == MAX_THREADS) {
	PJ_LOG(2,(THIS_FILE, "Unable to allocate thread local: too many threads!"));
	return -1;
    }
    tls_flag[i] = 1;
    tls[i] = NULL;
    return i;
#endif
}

PJ_DEF(void) pj_thread_local_free(long index)
{
#if PJ_HAS_THREADS
    pthread_key_delete(index);
#else
    tls_flag[index] = 0;
#endif
}

PJ_DEF(void) pj_thread_local_set(long index, void *value)
{
#if PJ_HAS_THREADS
    pthread_setspecific(index, value);
#else
    pj_assert(index >= 0 && index < MAX_THREADS);
    tls[index] = value;
#endif
}

PJ_DEF(void*) pj_thread_local_get(long index)
{
#if PJ_HAS_THREADS
    return pthread_getspecific(index);
#else
    pj_assert(index >= 0 && index < MAX_THREADS);
    return tls[index];
#endif
}

///////////////////////////////////////////////////////////////////////////////
PJ_DEF(void) pj_enter_critical_section(void)
{
#if PJ_HAS_THREADS
    pj_mutex_lock(&critical_section);
#endif
}

PJ_DEF(void) pj_leave_critical_section(void)
{
#if PJ_HAS_THREADS
    pj_mutex_unlock(&critical_section);
#endif
}


///////////////////////////////////////////////////////////////////////////////
static pj_status_t init_mutex(pj_mutex_t *mutex, const char *name, int type)
{
#if PJ_HAS_THREADS
    PJ_UNUSED_ARG(type)

    if (type == PJ_MUTEX_SIMPLE) {
	pthread_mutex_t the_mutex = PTHREAD_MUTEX_INITIALIZER;
	mutex->mutex = the_mutex;
    } else {
	pthread_mutex_t the_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	mutex->mutex = the_mutex;
    }
    
#if PJ_DEBUG
    /* Set owner. */
    mutex->nesting_level = 0;
    mutex->owner = NULL;
#endif

    /* Set name. */
    if (!name) {
	name = "mtx%p";
    }
    if (strchr(name, '%')) {
	pj_snprintf(mutex->obj_name, PJ_MAX_OBJ_NAME, name, mutex);
    } else {
	strncpy(mutex->obj_name, name, PJ_MAX_OBJ_NAME);
	mutex->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
    }

    PJ_LOG(6, (mutex->obj_name, "Mutex created"));
    return 0;
#else /* PJ_HAS_THREADS */
    return 0;
#endif
}

PJ_DEF(pj_mutex_t*) pj_mutex_create(pj_pool_t *pool, const char *name, int type)
{
#if PJ_HAS_THREADS
    pj_mutex_t *mutex = pj_pool_alloc(pool, sizeof(*mutex));
    if (init_mutex(mutex, name, type) == 0)
	return mutex;
    else
	return NULL;
#else /* PJ_HAS_THREADS */
    return (pj_mutex_t*)1;
#endif
}

PJ_DEF(pj_status_t) pj_mutex_lock(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    pj_status_t status;

    PJ_LOG(6,(mutex->obj_name, "Mutex: thread %s is waiting", 
				pj_thread_this()->obj_name));

    status = pthread_mutex_lock( &mutex->mutex );

    PJ_LOG(6,(mutex->obj_name, 
	      (status==PJ_OK ? "Mutex acquired by thread %s" : "FAILED by %s"),
	      pj_thread_this()->obj_name));

#if PJ_DEBUG
    if (status == PJ_OK) {
	mutex->owner = pj_thread_this();
	++mutex->nesting_level;
    }
#endif

    return status;
#else	/* PJ_HAS_THREADS */
    pj_assert( mutex == (pj_mutex_t*)1 );
    return 0;
#endif
}

PJ_DEF(pj_status_t) pj_mutex_unlock(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    pj_status_t status;

#if PJ_DEBUG
    pj_assert(mutex->owner == pj_thread_this());
    if (--mutex->nesting_level == 0) {
	mutex->owner = NULL;
    }
#endif

    PJ_LOG(6,(mutex->obj_name, "Mutex released by thread %s", 
				pj_thread_this()->obj_name));

    status = pthread_mutex_unlock( &mutex->mutex );
    return status;
#else /* PJ_HAS_THREADS */
    pj_assert( mutex == (pj_mutex_t*)1 );
    return 0;
#endif
}

PJ_DEF(pj_status_t) pj_mutex_trylock(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    pj_status_t status;

    status = pthread_mutex_trylock( &mutex->mutex );

    if (status==PJ_OK) {
	PJ_LOG(6,(mutex->obj_name, "Mutex acquired by thread %s", 
				  pj_thread_this()->obj_name));

#if PJ_DEBUG
	mutex->owner = pj_thread_this();
	++mutex->nesting_level;
#endif
    }
    return status;
#else	/* PJ_HAS_THREADS */
    pj_assert( mutex == (pj_mutex_t*)1);
    return 0;
#endif
}

PJ_DEF(pj_status_t) pj_mutex_destroy(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    PJ_LOG(6,(mutex->obj_name, "Mutex destroyed"));
    return pthread_mutex_destroy( &mutex->mutex );
#else
    pj_assert( mutex == (pj_mutex_t*)1 );
    return 0;
#endif
}

#if PJ_DEBUG
PJ_DEF(pj_status_t) pj_mutex_is_locked(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    return mutex->owner == pj_thread_this();
#else
    return 1;
#endif
}
#endif

///////////////////////////////////////////////////////////////////////////////
PJ_DEF(pj_sem_t*) pj_sem_create(pj_pool_t *pool, const char *name,
				unsigned initial, unsigned max)
{
#if PJ_HAS_THREADS
    pj_sem_t *sem;

    sem = pj_pool_alloc(pool, sizeof(*sem));    

    if (sem_init( &sem->sem, 0, initial) != 0) {
	PJ_LOG(3, (THIS_FILE, "Error creating semaphore"));
	return NULL;
    }
    
    /* Set name. */
    if (!name) {
	name = "sem%p";
    }
    if (strchr(name, '%')) {
	pj_snprintf(sem->obj_name, PJ_MAX_OBJ_NAME, name, sem);
    } else {
	strncpy(sem->obj_name, name, PJ_MAX_OBJ_NAME);
	sem->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
    }

    PJ_LOG(6, (sem->obj_name, "Semaphore created"));
    return sem;
#else
    return (pj_sem_t*)1;
#endif
}

PJ_DEF(pj_status_t) pj_sem_wait(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    int result;

    PJ_LOG(6, (sem->obj_name, "Semaphore: thread %s is waiting", 
			      pj_thread_this()->obj_name));

    result = sem_wait( &sem->sem );
    
    if (result == 0) {
	PJ_LOG(6, (sem->obj_name, "Semaphore acquired by thread %s", 
				  pj_thread_this()->obj_name));
    } else {
	PJ_LOG(6, (sem->obj_name, "Semaphore: thread %s FAILED to acquire", 
				  pj_thread_this()->obj_name));
    }

    return result;
#else
    pj_assert( sem == (pj_sem_t*) 1 );
    return 0;
#endif
}

PJ_DEF(pj_status_t) pj_sem_trywait(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    int result;

    result = sem_trywait( &sem->sem );
    
    if (result == 0) {
	PJ_LOG(6, (sem->obj_name, "Semaphore acquired by thread %s", 
				  pj_thread_this()->obj_name));
    } 
    return result;
#else
    pj_assert( sem == (pj_sem_t*)1 );
    return 0;
#endif
}

PJ_DEF(pj_status_t) pj_sem_post(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    PJ_LOG(6, (sem->obj_name, "Semaphore released by thread %s",
			      pj_thread_this()->obj_name));
    return sem_post( &sem->sem );
#else
    pj_assert( sem == (pj_sem_t*) 1);
    return 0;
#endif
}

PJ_DEF(pj_status_t) pj_sem_destroy(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    PJ_LOG(6, (sem->obj_name, "Semaphore destroyed by thread %s",
			      pj_thread_this()->obj_name));
    return sem_destroy( &sem->sem );
#else
    pj_assert( sem == (pj_sem_t*) 1 );
    return 0;
#endif
}

///////////////////////////////////////////////////////////////////////////////
PJ_DEF(pj_event_t*) pj_event_create(pj_pool_t *pool, const char *name,
				    pj_bool_t manual_reset, pj_bool_t initial)
{
    PJ_LOG(3, (THIS_FILE, "pj_event_create(): operation is not supported!"));
    return NULL;
}

PJ_DEF(pj_status_t) pj_event_wait(pj_event_t *event)
{
    return -1;
}

PJ_DEF(pj_status_t) pj_event_trywait(pj_event_t *event)
{
    return -1;
}

PJ_DEF(pj_status_t) pj_event_set(pj_event_t *event)
{
    return -1;
}

PJ_DEF(pj_status_t) pj_event_pulse(pj_event_t *event)
{
    return -1;
}

PJ_DEF(pj_status_t) pj_event_reset(pj_event_t *event)
{
    return -1;
}

PJ_DEF(pj_status_t) pj_event_destroy(pj_event_t *event)
{
    return -1;
}

///////////////////////////////////////////////////////////////////////////////

PJ_DEF(pj_status_t) pj_gettimeofday(pj_time_val *tv)
{
    struct timeval t;
    int status = gettimeofday(&t, NULL);
    tv->sec = t.tv_sec;
    tv->msec = t.tv_usec / 1000;
    return status;
}

PJ_DEF(pj_status_t) pj_time_decode(const pj_time_val *tv, pj_parsed_time *pt)
{
    struct tm *local_time;

    local_time = localtime((time_t*)&tv->sec);

    pt->year = local_time->tm_year;
    pt->mon = local_time->tm_mon;
    pt->day = local_time->tm_mday;
    pt->hour = local_time->tm_hour;
    pt->min = local_time->tm_min;
    pt->sec = local_time->tm_sec;
    pt->wday = local_time->tm_wday;
    pt->yday = local_time->tm_yday;
    pt->msec = tv->msec;

    return PJ_OK;
}

/**
 * Encode parsed time to time value.
 */
PJ_DEF(pj_status_t) pj_time_encode(const pj_parsed_time *pt, pj_time_val *tv);

/**
 * Convert local time to GMT.
 */
PJ_DEF(pj_status_t) pj_time_local_to_gmt(pj_time_val *tv);

/**
 * Convert GMT to local time.
 */
PJ_DEF(pj_status_t) pj_time_gmt_to_local(pj_time_val *tv);


///////////////////////////////////////////////////////////////////////////////
/*
 * Terminal
 */

/**
 * Set terminal color.
 */
PJ_DEF(pj_status_t) pj_term_set_color(pj_color_t color)
{
    return -1;
}

/**
 * Get current terminal foreground color.
 */
PJ_DEF(pj_color_t) pj_term_get_color(void)
{
    return 0;
}

