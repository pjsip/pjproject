/* $Header: /pjproject/pjlib/src/pj++/os.hpp 2     2/24/05 11:23a Bennylp $ */
#ifndef __PJPP_OS_H__
#define __PJPP_OS_H__

#include <pj/os.h>
#include <pj++/types.hpp>
#include <pj++/pool.hpp>

class PJ_Thread
{
public:
    enum Flags
    {
	FLAG_SUSPENDED = PJ_THREAD_SUSPENDED
    };

    static PJ_Thread *create( PJ_Pool *pool, const char *thread_name,
			      pj_thread_proc *proc, void *arg,
			      pj_size_t stack_size, void *stack_ptr, 
			      unsigned flags)
    {
	return (PJ_Thread*) pj_thread_create( pool->pool_(), thread_name, proc, arg, stack_size, stack_ptr, flags);
    }

    static PJ_Thread *register_current_thread(const char *name, pj_thread_desc desc)
    {
	return (PJ_Thread*) pj_thread_register(name, desc);
    }

    static PJ_Thread *get_current_thread()
    {
	return (PJ_Thread*) pj_thread_this();
    }

    static pj_status_t sleep(unsigned msec)
    {
	return pj_thread_sleep(msec);
    }

    static pj_status_t usleep(unsigned usec)
    {
	return pj_thread_usleep(usec);
    }

    pj_thread_t *pj_thread_t_()
    {
	return (pj_thread_t*)this;
    }

    const char *get_name()
    {
	return pj_thread_get_name( this->pj_thread_t_() );
    }

    pj_status_t resume()
    {
	return pj_thread_resume( this->pj_thread_t_() );
    }

    pj_status_t join()
    {
	return pj_thread_join( this->pj_thread_t_() );
    }

    pj_status_t destroy()
    {
	return pj_thread_destroy( this->pj_thread_t_() );
    }
};


class PJ_Thread_Local
{
public:
    static PJ_Thread_Local *alloc()
    {
	long index = pj_thread_local_alloc();
	return index < 0 ? NULL : (PJ_Thread_Local*)index;
    }
    void free()
    {
	pj_thread_local_free( this->tls_() );
    }

    long tls_() const
    {
	return (long)this;
    }

    void set(void *value)
    {
	pj_thread_local_set( this->tls_(), value );
    }

    void *get()
    {
	return pj_thread_local_get( this->tls_() );
    }
};


class PJ_Atomic
{
public:
    static PJ_Atomic *create(PJ_Pool *pool, long initial)
    {
	return (PJ_Atomic*) pj_atomic_create(pool->pool_(), initial);
    }

    pj_atomic_t *pj_atomic_t_()
    {
	return (pj_atomic_t*)this;
    }

    pj_status_t destroy()
    {
	return pj_atomic_destroy( this->pj_atomic_t_() );
    }

    long set(long val)
    {
	return pj_atomic_set( this->pj_atomic_t_(), val);
    }

    long get()
    {
	return pj_atomic_get( this->pj_atomic_t_() );
    }

    long inc()
    {
	return pj_atomic_inc( this->pj_atomic_t_() );
    }

    long dec()
    {
	return pj_atomic_dec( this->pj_atomic_t_() );
    }
};


class PJ_Mutex
{
public:
    enum Type
    {
	DEFAULT = PJ_MUTEX_DEFAULT,
	SIMPLE = PJ_MUTEX_SIMPLE,
	RECURSE = PJ_MUTEX_RECURSE,
    };

    static PJ_Mutex *create( PJ_Pool *pool, const char *name, Type type)
    {
	return (PJ_Mutex*) pj_mutex_create( pool->pool_(), name, type);
    }

    pj_mutex_t *pj_mutex_()
    {
	return (pj_mutex_t*)this;
    }

    pj_status_t destroy()
    {
	return pj_mutex_destroy( this->pj_mutex_() );
    }

    pj_status_t lock()
    {
	return pj_mutex_lock( this->pj_mutex_() );
    }

    pj_status_t unlock()
    {
	return pj_mutex_unlock( this->pj_mutex_() );
    }

    pj_status_t trylock()
    {
	return pj_mutex_trylock( this->pj_mutex_() );
    }

#if PJ_DEBUG
    pj_status_t is_locked()
    {
	return pj_mutex_is_locked( this->pj_mutex_() );
    }
#endif
};


class PJ_Semaphore
{
public:
    static PJ_Semaphore *create( PJ_Pool *pool, const char *name, unsigned initial, unsigned max)
    {
	return (PJ_Semaphore*) pj_sem_create( pool->pool_(), name, initial, max);
    }

    pj_sem_t *pj_sem_t_()
    {
	return (pj_sem_t*)this;
    }

    pj_status_t destroy()
    {
	return pj_sem_destroy(this->pj_sem_t_());
    }

    pj_status_t wait()
    {
	return pj_sem_wait(this->pj_sem_t_());
    }

    pj_status_t lock()
    {
	return wait();
    }

    pj_status_t trywait()
    {
	return pj_sem_trywait(this->pj_sem_t_());
    }

    pj_status_t trylock()
    {
	return trywait();
    }

    pj_status_t post()
    {
	return pj_sem_post(this->pj_sem_t_());
    }

    pj_status_t unlock()
    {
	return post();
    }
};


class PJ_Event
{
public:
    static PJ_Event *create( PJ_Pool *pool, const char *name, bool manual_reset, bool initial)
    {
	return (PJ_Event*) pj_event_create(pool->pool_(), name, manual_reset, initial);
    }

    pj_event_t *pj_event_t_()
    {
	return (pj_event_t*)this;
    }

    pj_status_t destroy()
    {
	return pj_event_destroy(this->pj_event_t_());
    }

    pj_status_t wait()
    {
	return pj_event_wait(this->pj_event_t_());
    }

    pj_status_t trywait()
    {
	return pj_event_trywait(this->pj_event_t_());
    }

    pj_status_t set()
    {
	return pj_event_set(this->pj_event_t_());
    }

    pj_status_t pulse()
    {
	return pj_event_pulse(this->pj_event_t_());
    }

    pj_status_t reset()
    {
	return pj_event_reset(this->pj_event_t_());
    }
};

class PJ_OS
{
public:
    static pj_status_t gettimeofday( PJ_Time_Val *tv )
    {
	return pj_gettimeofday(tv);
    }

    static pj_status_t time_decode( const PJ_Time_Val *tv, pj_parsed_time *pt )
    {
	return pj_time_decode(tv, pt);
    }

    static pj_status_t time_encode(const pj_parsed_time *pt, PJ_Time_Val *tv)
    {
	return pj_time_encode(pt, tv);
    }

    static pj_status_t time_local_to_gmt( PJ_Time_Val *tv )
    {
	return pj_time_local_to_gmt( tv );
    }

    static pj_status_t time_gmt_to_local( PJ_Time_Val *tv) 
    {
	return pj_time_gmt_to_local( tv );
    }
};


inline pj_status_t PJ_Time_Val::gettimeofday()
{
    return PJ_OS::gettimeofday(this);
}

inline pj_parsed_time PJ_Time_Val::decode()
{
    pj_parsed_time pt;
    PJ_OS::time_decode(this, &pt);
    return pt;
}

inline pj_status_t PJ_Time_Val::encode(const pj_parsed_time *pt)
{
    return PJ_OS::time_encode(pt, this);
}

inline pj_status_t PJ_Time_Val::to_gmt()
{
    return PJ_OS::time_local_to_gmt(this);
}

inline pj_status_t PJ_Time_Val::to_local()
{
    return PJ_OS::time_gmt_to_local(this);
}

#endif	/* __PJPP_OS_H__ */
