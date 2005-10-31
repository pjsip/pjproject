/* $Header: /pjproject-0.3/pjlib/src/pj/os_core_unix.c 11    10/29/05 10:27p Bennylp $ */
/* 
 * $Log: /pjproject-0.3/pjlib/src/pj/os_core_unix.c $ 
 * 
 * 11    10/29/05 10:27p Bennylp
 * Fixed misc warnings.
 * 
 * 10    10/29/05 11:51a Bennylp
 * Version 0.3-pre2.
 * 
 * 9     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 *
 */
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/guid.h>
#include <pj/compat/sprintf.h>
#include <pj/except.h>
#include <pj/errno.h>

#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0
#  include <semaphore.h>
#endif

#include <unistd.h>	    // getpid()
#include <errno.h>	    // errno

#define __USE_GNU
#include <pthread.h>

#define THIS_FILE   "osunix"

struct pj_thread_t
{
    char	    obj_name[PJ_MAX_OBJ_NAME];
    pthread_t	    thread;
    pj_thread_proc *proc;
    void	   *arg;

    pj_mutex_t	   *suspended_mutex;

#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK!=0
    pj_uint32_t	    stk_size;
    pj_uint32_t	    stk_max_usage;
    char	   *stk_start;
    const char	   *caller_file;
    int		    caller_line;
#endif
};

struct pj_atomic_t
{
    pj_mutex_t	       *mutex;
    pj_atomic_value_t	value;
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

#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0
struct pj_sem_t
{
    sem_t		sem;
    char		obj_name[PJ_MAX_OBJ_NAME];
};
#endif /* PJ_HAS_SEMAPHORE */

#if defined(PJ_HAS_EVENT_OBJ) && PJ_HAS_EVENT_OBJ != 0
struct pj_event_t
{
    char		obj_name[PJ_MAX_OBJ_NAME];
};
#endif	/* PJ_HAS_EVENT_OBJ */


#if PJ_HAS_THREADS
    static pj_thread_t main_thread;
    static long thread_tls_id;
    static pj_mutex_t critical_section;
#else
#   define MAX_THREADS 32
    static int tls_flag[MAX_THREADS];
    static void *tls[MAX_THREADS];
#endif

static pj_status_t init_mutex(pj_mutex_t *mutex, const char *name, int type);

/*
 * pj_init(void).
 * Init PJLIB!
 */
PJ_DEF(pj_status_t) pj_init(void)
{
    char dummy_guid[PJ_GUID_MAX_LENGTH];
    pj_str_t guid;
    pj_status_t rc;

    PJ_LOG(5, ("pj_init", "Initializing PJ Library.."));

#if PJ_HAS_THREADS
    /* Init this thread's TLS. */
    if ((rc=pj_thread_init()) != 0) {
	return rc;
    }

    /* Critical section. */
    if ((rc=init_mutex(&critical_section, "critsec", PJ_MUTEX_SIMPLE)) != 0)
	return rc;

#endif

    /* Initialize exception ID for the pool. 
     * Must do so after critical section is configured.
     */
    rc = pj_exception_id_alloc("PJLIB/No memory", &PJ_NO_MEMORY_EXCEPTION);
    if (rc != PJ_SUCCESS)
        return rc;
    
    /* Init random seed. */
    pj_srand( clock() );

    /* Startup GUID. */
    guid.ptr = dummy_guid;
    pj_generate_unique_string( &guid );

    /* Initialize exception ID for the pool. 
     * Must do so after critical section is configured.
     */
    rc = pj_exception_id_alloc("PJLIB/No memory", &PJ_NO_MEMORY_EXCEPTION);
    if (rc != PJ_SUCCESS)
        return rc;

    /* Startup timestamp */
#if defined(PJ_HAS_HIGH_RES_TIMER) && PJ_HAS_HIGH_RES_TIMER != 0
    {
	pj_timestamp dummy_ts;
	if ((rc=pj_get_timestamp(&dummy_ts)) != 0) {
	    return rc;
	}
    }
#endif   

    return PJ_SUCCESS;
}

/*
 * pj_getpid(void)
 */
PJ_DEF(pj_uint32_t) pj_getpid(void)
{
    PJ_CHECK_STACK();
    return getpid();
}

/*
 * pj_thread_register(..)
 */
PJ_DEF(pj_status_t) pj_thread_register ( const char *cstr_thread_name,
					 pj_thread_desc desc,
					 pj_thread_t **ptr_thread)
{
#if PJ_HAS_THREADS
    char stack_ptr;
    pj_thread_t *thread = (pj_thread_t *)desc;
    pj_str_t thread_name = pj_str((char*)cstr_thread_name);

    /* Size sanity check. */
    if (sizeof(pj_thread_desc) < sizeof(pj_thread_t)) {
	pj_assert(!"Not enough pj_thread_desc size!");
	return PJ_EBUG;
    }

    /* If a thread descriptor has been registered before, just return it. */
    if (pj_thread_local_get (thread_tls_id) != 0) {
	*ptr_thread = (pj_thread_t*)pj_thread_local_get (thread_tls_id);
	return PJ_SUCCESS;
    }

    /* Initialize and set the thread entry. */
    pj_memset(desc, 0, sizeof(pj_thread_desc));
    thread->thread = pthread_self();

    if(cstr_thread_name && pj_strlen(&thread_name) < sizeof(thread->obj_name)-1)
	pj_sprintf(thread->obj_name, cstr_thread_name, thread->thread);
    else
	pj_sprintf(thread->obj_name, "thr%p", (void*)thread->thread);
    
    pj_thread_local_set(thread_tls_id, thread);

#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK!=0
    thread->stk_start = &stack_ptr;
    thread->stk_size = 0xFFFFFFFFUL;
    thread->stk_max_usage = 0;
#else
    stack_ptr = '\0';
#endif

    *ptr_thread = thread;
    return PJ_SUCCESS;
#else
    pj_thread_t *thread = (pj_thread_t*)desc;
    *ptr_thread = thread;
    return SUCCESS;
#endif
}

/*
 * pj_thread_init(void)
 */
pj_status_t pj_thread_init(void)
{
#if PJ_HAS_THREADS
    pj_status_t rc;
    pj_thread_t *dummy;

    rc = pj_thread_local_alloc(&thread_tls_id );
    if (rc != PJ_SUCCESS) {
	return rc;
    }
    return pj_thread_register("thr%p", (pj_uint8_t*)&main_thread, &dummy);
#else
    PJ_LOG(2,(THIS_FILE, "Thread init error. Threading is not enabled!"));
    return PJ_EINVALIDOP;
#endif
}

#if PJ_HAS_THREADS
/*
 * thread_main()
 *
 * This is the main entry for all threads.
 */
static void *thread_main(void *param)
{
    pj_thread_t *rec = param;
    void *result;

#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK!=0
    rec->stk_start = (char*)&rec;
#endif

    /* Set current thread id. */
    pj_thread_local_set(thread_tls_id, rec);

    /* Check if suspension is required. */
    if (rec->suspended_mutex)
	pj_mutex_lock(rec->suspended_mutex);

    PJ_LOG(6,(rec->obj_name, "Thread started"));

    /* Call user's entry! */
    result = (void*) (*rec->proc)(rec->arg);

    /* Done. */
    PJ_LOG(6,(rec->obj_name, "Thread quitting"));
    return result;
}
#endif

/*
 * pj_thread_create(...)
 */
PJ_DEF(pj_status_t) pj_thread_create( pj_pool_t *pool, 
				      const char *thread_name,
				      pj_thread_proc *proc, 
				      void *arg,
				      pj_size_t stack_size, 
				      unsigned flags,
				      pj_thread_t **ptr_thread)
{
#if PJ_HAS_THREADS
    pj_thread_t *rec;
    int rc;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(pool && proc && ptr_thread, PJ_EINVAL);

    /* Create thread record and assign name for the thread */
    rec = (struct pj_thread_t*) pj_pool_zalloc(pool, sizeof(pj_thread_t));
    if (!rec)
	return PJ_ENOMEM;
    
    /* Set name. */
    if (!thread_name) 
	thread_name = "thr%p";
    
    if (strchr(thread_name, '%')) {
	pj_snprintf(rec->obj_name, PJ_MAX_OBJ_NAME, thread_name, rec);
    } else {
	strncpy(rec->obj_name, thread_name, PJ_MAX_OBJ_NAME);
	rec->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
    }

#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK!=0
    rec->stk_size = stack_size ? stack_size : 0xFFFFFFFFUL;
    rec->stk_max_usage = 0;
#endif

    /* Emulate suspended thread with mutex. */
    if (flags & PJ_THREAD_SUSPENDED) {
	rc = pj_mutex_create_simple(pool, NULL, &rec->suspended_mutex);
	if (rc != PJ_SUCCESS)
	    return rc;

	pj_mutex_lock(rec->suspended_mutex);
    } else {
	pj_assert(rec->suspended_mutex == NULL);
    }
    
    PJ_LOG(6, (rec->obj_name, "Thread created"));

    /* Create the thread. */
    rec->proc = proc;
    rec->arg = arg;
    rc = pthread_create( &rec->thread, NULL, thread_main, rec);
    if (rc != 0)
	return PJ_RETURN_OS_ERROR(rc);

    *ptr_thread = rec;
    return PJ_SUCCESS;
#else
    pj_assert(!"Threading is disabled!");
    return PJ_EINVALIDOP;
#endif
}

/*
 * pj_thread-get_name()
 */
PJ_DEF(const char*) pj_thread_get_name(pj_thread_t *p)
{
#if PJ_HAS_THREADS
    pj_thread_t *rec = (pj_thread_t*)p;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(p, "");

    return rec->obj_name;
#else
    return "";
#endif
}

/*
 * pj_thread_resume()
 */
PJ_DEF(pj_status_t) pj_thread_resume(pj_thread_t *p)
{
    pj_status_t rc;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(p, PJ_EINVAL);

    rc = pj_mutex_unlock(p->suspended_mutex);

    return rc;
}

/*
 * pj_thread_this()
 */
PJ_DEF(pj_thread_t*) pj_thread_this(void)
{
#if PJ_HAS_THREADS
    pj_thread_t *rec = pj_thread_local_get(thread_tls_id);
    pj_assert(rec != NULL);

    /*
     * MUST NOT check stack because this function is called
     * by PJ_CHECK_STACK() itself!!!
     *
     */

    return rec;
#else
    pj_assert(!"Threading is not enabled!");
    return NULL;
#endif
}

/*
 * pj_thread_join()
 */
PJ_DEF(pj_status_t) pj_thread_join(pj_thread_t *p)
{
#if PJ_HAS_THREADS
    pj_thread_t *rec = (pj_thread_t *)p;
    void *ret;
    int result;

    PJ_CHECK_STACK();

    PJ_LOG(6, (pj_thread_this()->obj_name, "Joining thread %s", p->obj_name));
    result = pthread_join( rec->thread, &ret);

    if (result == 0)
	return PJ_SUCCESS;
    else
	return PJ_RETURN_OS_ERROR(result);
#else
    PJ_CHECK_STACK();
    pj_assert(!"No multithreading support!");
    return PJ_EINVALIDOP;
#endif
}

/*
 * pj_thread_destroy()
 */
PJ_DEF(pj_status_t) pj_thread_destroy(pj_thread_t *p)
{
    /* This function is used to destroy thread handle in other platforms.
     * I suppose there's nothing to do here..
     */
    PJ_CHECK_STACK();
    return PJ_SUCCESS;
}

/*
 * pj_thread_sleep()
 */
PJ_DEF(pj_status_t) pj_thread_sleep(unsigned msec)
{
    PJ_CHECK_STACK();
    return usleep(msec * 1000);
}

#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK!=0
/*
 * pj_thread_check_stack()
 * Implementation for PJ_CHECK_STACK()
 */
PJ_DEF(void) pj_thread_check_stack(const char *file, int line)
{
    char stk_ptr;
    pj_uint32_t usage;
    pj_thread_t *thread = pj_thread_this();

    /* Calculate current usage. */
    usage = (&stk_ptr > thread->stk_start) ? &stk_ptr - thread->stk_start :
		thread->stk_start - &stk_ptr;

    /* Assert if stack usage is dangerously high. */
    pj_assert("STACK OVERFLOW!! " && (usage <= thread->stk_size - 128));

    /* Keep statistic. */
    if (usage > thread->stk_max_usage) {
	thread->stk_max_usage = usage;
	thread->caller_file = file;
	thread->caller_line = line;
    }
}

/*
 * pj_thread_get_stack_max_usage()
 */
PJ_DEF(pj_uint32_t) pj_thread_get_stack_max_usage(pj_thread_t *thread)
{
    return thread->stk_max_usage;
}

/*
 * pj_thread_get_stack_info()
 */
PJ_DEF(pj_status_t) pj_thread_get_stack_info( pj_thread_t *thread,
					      const char **file,
					      int *line )
{
    pj_assert(thread);

    *file = thread->caller_file;
    *line = thread->caller_line;
    return 0;
}

#endif	/* PJ_OS_HAS_CHECK_STACK */

///////////////////////////////////////////////////////////////////////////////
/*
 * pj_atomic_create()
 */
PJ_DEF(pj_status_t) pj_atomic_create( pj_pool_t *pool, 
				      pj_atomic_value_t initial,
				      pj_atomic_t **ptr_atomic)
{
    pj_status_t rc;
    pj_atomic_t *atomic_var = pj_pool_calloc(pool, 1, sizeof(pj_atomic_t));
    if (!atomic_var)
	return PJ_ENOMEM;
    
#if PJ_HAS_THREADS
    rc = pj_mutex_create(pool, "atm%p", PJ_MUTEX_SIMPLE, &atomic_var->mutex);
    if (rc != PJ_SUCCESS)
	return rc;
#endif
    atomic_var->value = initial;

    *ptr_atomic = atomic_var;
    return PJ_SUCCESS;
}

/*
 * pj_atomic_destroy()
 */
PJ_DEF(pj_status_t) pj_atomic_destroy( pj_atomic_t *atomic_var )
{
    PJ_ASSERT_RETURN(atomic_var, PJ_EINVAL);
#if PJ_HAS_THREADS
    return pj_mutex_destroy( atomic_var->mutex );
#else
    return 0;
#endif
}

/*
 * pj_atomic_set()
 */
PJ_DEF(pj_atomic_value_t) pj_atomic_set(pj_atomic_t *atomic_var, 
					pj_atomic_value_t value)
{
    pj_atomic_value_t oldval;
    
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(atomic_var, 0);

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

/*
 * pj_atomic_get()
 */
PJ_DEF(pj_atomic_value_t) pj_atomic_get(pj_atomic_t *atomic_var)
{
    pj_atomic_value_t oldval;
    
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(atomic_var, 0);

#if PJ_HAS_THREADS
    pj_mutex_lock( atomic_var->mutex );
#endif
    oldval = atomic_var->value;
#if PJ_HAS_THREADS
    pj_mutex_unlock( atomic_var->mutex);
#endif
    return oldval;
}

/*
 * pj_atomic_inc()
 */
PJ_DEF(pj_atomic_value_t) pj_atomic_inc(pj_atomic_t *atomic_var)
{
    pj_atomic_value_t newval;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(atomic_var, 0);

#if PJ_HAS_THREADS
    pj_mutex_lock( atomic_var->mutex );
#endif
    newval = ++atomic_var->value;
#if PJ_HAS_THREADS
    pj_mutex_unlock( atomic_var->mutex);
#endif
    return newval;
}

/*
 * pj_atomic_dec()
 */
PJ_DEF(pj_atomic_value_t) pj_atomic_dec(pj_atomic_t *atomic_var)
{
    pj_atomic_value_t newval;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(atomic_var, 0);

#if PJ_HAS_THREADS
    pj_mutex_lock( atomic_var->mutex );
#endif
    newval = --atomic_var->value;
#if PJ_HAS_THREADS
    pj_mutex_unlock( atomic_var->mutex);
#endif
    return newval;
}


///////////////////////////////////////////////////////////////////////////////
/*
 * pj_thread_local_alloc()
 */
PJ_DEF(pj_status_t) pj_thread_local_alloc(long *p_index)
{
#if PJ_HAS_THREADS
    pthread_key_t key;
    int rc;

    PJ_ASSERT_RETURN(p_index != NULL, PJ_EINVAL);

    pj_assert( sizeof(pthread_key_t) <= sizeof(long));
    if ((rc=pthread_key_create(&key, NULL)) != 0)
	return PJ_RETURN_OS_ERROR(rc);

    *p_index = key;
    return PJ_SUCCESS;
#else
    int i;
    for (i=0; i<MAX_THREADS; ++i) {
	if (tls_flag[i] == 0)
	    break;
    }
    if (i == MAX_THREADS) 
	return PJ_ETOOMANY;
    
    tls_flag[i] = 1;
    tls[i] = NULL;

    *p_index = i;
    return PJ_SUCCESS;
#endif
}

/*
 * pj_thread_local_free()
 */
PJ_DEF(void) pj_thread_local_free(long index)
{
    PJ_CHECK_STACK();
#if PJ_HAS_THREADS
    pthread_key_delete(index);
#else
    tls_flag[index] = 0;
#endif
}

/*
 * pj_thread_local_set()
 */
PJ_DEF(void) pj_thread_local_set(long index, void *value)
{
    //Can't check stack because this function is called in the
    //beginning before main thread is initialized.
    //PJ_CHECK_STACK();
#if PJ_HAS_THREADS
    pthread_setspecific(index, value);
#else
    pj_assert(index >= 0 && index < MAX_THREADS);
    tls[index] = value;
#endif
}

PJ_DEF(void*) pj_thread_local_get(long index)
{
    //Can't check stack because this function is called
    //by PJ_CHECK_STACK() itself!!!
    //PJ_CHECK_STACK();
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
    PJ_UNUSED_ARG(type);

    PJ_CHECK_STACK();

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
    return PJ_SUCCESS;
#else /* PJ_HAS_THREADS */
    return PJ_SUCCESS;
#endif
}

/*
 * pj_mutex_create()
 */
PJ_DEF(pj_status_t) pj_mutex_create(pj_pool_t *pool, 
				    const char *name, 
				    int type,
				    pj_mutex_t **ptr_mutex)
{
#if PJ_HAS_THREADS
    pj_status_t rc;
    pj_mutex_t *mutex;

    PJ_ASSERT_RETURN(pool && ptr_mutex, PJ_EINVAL);

    mutex = pj_pool_alloc(pool, sizeof(*mutex));
    if (!mutex) return PJ_ENOMEM;

    if ((rc=init_mutex(mutex, name, type)) != PJ_SUCCESS)
	return rc;
    
    *ptr_mutex = mutex;
    return PJ_SUCCESS;
#else /* PJ_HAS_THREADS */
    return (pj_mutex_t*)1;
#endif
}

/*
 * pj_mutex_create_simple()
 */
PJ_DEF(pj_status_t) pj_mutex_create_simple( pj_pool_t *pool, 
                                            const char *name,
					    pj_mutex_t **mutex )
{
    return pj_mutex_create(pool, name, PJ_MUTEX_SIMPLE, mutex);
}

/*
 * pj_mutex_create_recursive()
 */
PJ_DEF(pj_status_t) pj_mutex_create_recursive( pj_pool_t *pool,
					       const char *name,
					       pj_mutex_t **mutex )
{
    return pj_mutex_create(pool, name, PJ_MUTEX_RECURSE, mutex);
}

/*
 * pj_mutex_lock()
 */
PJ_DEF(pj_status_t) pj_mutex_lock(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    pj_status_t status;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);

    PJ_LOG(6,(mutex->obj_name, "Mutex: thread %s is waiting", 
				pj_thread_this()->obj_name));

    status = pthread_mutex_lock( &mutex->mutex );

    PJ_LOG(6,(mutex->obj_name, 
	      (status==0 ? "Mutex acquired by thread %s" : "FAILED by %s"),
	      pj_thread_this()->obj_name));

#if PJ_DEBUG
    if (status == PJ_SUCCESS) {
	mutex->owner = pj_thread_this();
	++mutex->nesting_level;
    }
#endif

    if (status == 0)
	return PJ_SUCCESS;
    else
	return PJ_RETURN_OS_ERROR(status);
#else	/* PJ_HAS_THREADS */
    pj_assert( mutex == (pj_mutex_t*)1 );
    return PJ_SUCCESS;
#endif
}

/*
 * pj_mutex_unlock()
 */
PJ_DEF(pj_status_t) pj_mutex_unlock(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    pj_status_t status;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);

#if PJ_DEBUG
    pj_assert(mutex->owner == pj_thread_this());
    if (--mutex->nesting_level == 0) {
	mutex->owner = NULL;
    }
#endif

    PJ_LOG(6,(mutex->obj_name, "Mutex released by thread %s", 
				pj_thread_this()->obj_name));

    status = pthread_mutex_unlock( &mutex->mutex );
    if (status == 0)
	return PJ_SUCCESS;
    else
	return PJ_RETURN_OS_ERROR(status);

#else /* PJ_HAS_THREADS */
    pj_assert( mutex == (pj_mutex_t*)1 );
    return PJ_SUCCESS;
#endif
}

/*
 * pj_mutex_trylock()
 */
PJ_DEF(pj_status_t) pj_mutex_trylock(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    int status;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);

    status = pthread_mutex_trylock( &mutex->mutex );

    if (status==0) {
	PJ_LOG(6,(mutex->obj_name, "Mutex acquired by thread %s", 
				  pj_thread_this()->obj_name));

#if PJ_DEBUG
	mutex->owner = pj_thread_this();
	++mutex->nesting_level;
#endif
    }
    
    if (status==0)
	return PJ_SUCCESS;
    else
	return PJ_RETURN_OS_ERROR(status);
#else	/* PJ_HAS_THREADS */
    pj_assert( mutex == (pj_mutex_t*)1);
    return PJ_SUCCESS;
#endif
}

/*
 * pj_mutex_destroy()
 */
PJ_DEF(pj_status_t) pj_mutex_destroy(pj_mutex_t *mutex)
{
    int status;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);

#if PJ_HAS_THREADS
    PJ_LOG(6,(mutex->obj_name, "Mutex destroyed"));
    status = pthread_mutex_destroy( &mutex->mutex );
    if (status == 0)
	return PJ_SUCCESS;
    else
	return PJ_RETURN_OS_ERROR(status);
#else
    pj_assert( mutex == (pj_mutex_t*)1 );
    status = PJ_SUCCESS;
    return status;
#endif
}

#if PJ_DEBUG
PJ_DEF(pj_bool_t) pj_mutex_is_locked(pj_mutex_t *mutex)
{
#if PJ_HAS_THREADS
    return mutex->owner == pj_thread_this();
#else
    return 1;
#endif
}
#endif

///////////////////////////////////////////////////////////////////////////////
#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0

/*
 * pj_sem_create()
 */
PJ_DEF(pj_status_t) pj_sem_create( pj_pool_t *pool, 
				   const char *name,
				   unsigned initial, 
				   unsigned max,
				   pj_sem_t **ptr_sem)
{
#if PJ_HAS_THREADS
    pj_sem_t *sem;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(pool != NULL && ptr_sem != NULL, PJ_EINVAL);

    sem = pj_pool_alloc(pool, sizeof(*sem));    
    if (!sem) return PJ_ENOMEM;

    if (sem_init( &sem->sem, 0, initial) != 0) 
	return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
    
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

    *ptr_sem = sem;
    return PJ_SUCCESS;
#else
    *ptr_sem = (pj_sem_t*)1;
    return PJ_SUCCESS;
#endif
}

/*
 * pj_sem_wait()
 */
PJ_DEF(pj_status_t) pj_sem_wait(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    int result;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(sem, PJ_EINVAL);

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

    if (result == 0)
	return PJ_SUCCESS;
    else
	return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
#else
    pj_assert( sem == (pj_sem_t*) 1 );
    return PJ_SUCCESS;
#endif
}

/*
 * pj_sem_trywait()
 */
PJ_DEF(pj_status_t) pj_sem_trywait(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    int result;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(sem, PJ_EINVAL);

    result = sem_trywait( &sem->sem );
    
    if (result == 0) {
	PJ_LOG(6, (sem->obj_name, "Semaphore acquired by thread %s", 
				  pj_thread_this()->obj_name));
    } 
    if (result == 0)
	return PJ_SUCCESS;
    else
	return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
#else
    pj_assert( sem == (pj_sem_t*)1 );
    return PJ_SUCCESS;
#endif
}

/*
 * pj_sem_post()
 */
PJ_DEF(pj_status_t) pj_sem_post(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    int result;
    PJ_LOG(6, (sem->obj_name, "Semaphore released by thread %s",
			      pj_thread_this()->obj_name));
    result = sem_post( &sem->sem );

    if (result == 0)
	return PJ_SUCCESS;
    else
	return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
#else
    pj_assert( sem == (pj_sem_t*) 1);
    return PJ_SUCCESS;
#endif
}

/*
 * pj_sem_destroy()
 */
PJ_DEF(pj_status_t) pj_sem_destroy(pj_sem_t *sem)
{
#if PJ_HAS_THREADS
    int result;

    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(sem, PJ_EINVAL);

    PJ_LOG(6, (sem->obj_name, "Semaphore destroyed by thread %s",
			      pj_thread_this()->obj_name));
    result = sem_destroy( &sem->sem );

    if (result == 0)
	return PJ_SUCCESS;
    else
	return PJ_RETURN_OS_ERROR(pj_get_native_os_error());
#else
    pj_assert( sem == (pj_sem_t*) 1 );
    return PJ_SUCCESS;
#endif
}

#endif	/* PJ_HAS_SEMAPHORE */

///////////////////////////////////////////////////////////////////////////////
#if defined(PJ_HAS_EVENT_OBJ) && PJ_HAS_EVENT_OBJ != 0

/*
 * pj_event_create()
 */
PJ_DEF(pj_status_t) pj_event_create(pj_pool_t *pool, const char *name,
				    pj_bool_t manual_reset, pj_bool_t initial,
				    pj_event_t **ptr_event)
{
    pj_assert(!"Not supported!");
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(name);
    PJ_UNUSED_ARG(manual_reset);
    PJ_UNUSED_ARG(initial);
    PJ_UNUSED_ARG(ptr_event);
    return PJ_EINVALIDOP;
}

/*
 * pj_event_wait()
 */
PJ_DEF(pj_status_t) pj_event_wait(pj_event_t *event)
{
    PJ_UNUSED_ARG(event);
    return PJ_EINVALIDOP;
}

/*
 * pj_event_trywait()
 */
PJ_DEF(pj_status_t) pj_event_trywait(pj_event_t *event)
{
    PJ_UNUSED_ARG(event);
    return PJ_EINVALIDOP;
}

/*
 * pj_event_set()
 */
PJ_DEF(pj_status_t) pj_event_set(pj_event_t *event)
{
    PJ_UNUSED_ARG(event);
    return PJ_EINVALIDOP;
}

/*
 * pj_event_pulse()
 */
PJ_DEF(pj_status_t) pj_event_pulse(pj_event_t *event)
{
    PJ_UNUSED_ARG(event);
    return PJ_EINVALIDOP;
}

/*
 * pj_event_reset()
 */
PJ_DEF(pj_status_t) pj_event_reset(pj_event_t *event)
{
    PJ_UNUSED_ARG(event);
    return PJ_EINVALIDOP;
}

/*
 * pj_event_destroy()
 */
PJ_DEF(pj_status_t) pj_event_destroy(pj_event_t *event)
{
    PJ_UNUSED_ARG(event);
    return PJ_EINVALIDOP;
}

#endif	/* PJ_HAS_EVENT_OBJ */

///////////////////////////////////////////////////////////////////////////////
#if defined(PJ_TERM_HAS_COLOR) && PJ_TERM_HAS_COLOR != 0
/*
 * Terminal
 */

/**
 * Set terminal color.
 */
PJ_DEF(pj_status_t) pj_term_set_color(pj_color_t color)
{
    PJ_UNUSED_ARG(color);
    return PJ_EINVALIDOP;
}

/**
 * Get current terminal foreground color.
 */
PJ_DEF(pj_color_t) pj_term_get_color(void)
{
    return 0;
}

#endif	/* PJ_TERM_HAS_COLOR */

