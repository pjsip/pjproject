/* $Id$ */
#ifndef __PJPP_LOCK_H__
#define __PJPP_LOCK_H__

#include <pj++/types.hpp>
#include <pj/lock.h>
#include <pj++/pool.hpp>

//////////////////////////////////////////////////////////////////////////////
// Lock object.
//
class Pj_Lock : public Pj_Object
{
public:
    //
    // Constructor.
    //
    explicit Pj_Lock(pj_lock_t *lock)
        : lock_(lock)
    {
    }

    //
    // Destructor.
    //
    ~Pj_Lock()
    {
        if (lock_)
            pj_lock_destroy(lock_);
    }

    //
    // Get pjlib compatible lock object.
    //
    pj_lock_t *pj_lock_t_()
    {
        return lock_;
    }

    //
    // acquire lock.
    //
    pj_status_t acquire()
    {
        return pj_lock_acquire(lock_);
    }

    //
    // release lock,.
    //
    pj_status_t release()
    {
        return pj_lock_release(lock_);
    }

protected:
    pj_lock_t *lock_;
};


//////////////////////////////////////////////////////////////////////////////
// Null lock object.
//
class Pj_Null_Lock : public Pj_Lock
{
public:
    //
    // Default constructor.
    //
    explicit Pj_Null_Lock(Pj_Pool *pool, const char *name = NULL)
        : Pj_Lock(NULL)
    {
        pj_lock_create_null_mutex(pool->pool_(), name, &lock_);
    }
};

//////////////////////////////////////////////////////////////////////////////
// Simple mutex lock object.
//
class Pj_Simple_Mutex_Lock : public Pj_Lock
{
public:
    //
    // Default constructor.
    //
    explicit Pj_Simple_Mutex_Lock(Pj_Pool *pool, const char *name = NULL)
        : Pj_Lock(NULL)
    {
        pj_lock_create_simple_mutex(pool->pool_(), name, &lock_);
    }
};

//////////////////////////////////////////////////////////////////////////////
// Recursive mutex lock object.
//
class Pj_Recursive_Mutex_Lock : public Pj_Lock
{
public:
    //
    // Default constructor.
    //
    explicit Pj_Recursive_Mutex_Lock(Pj_Pool *pool, const char *name = NULL)
        : Pj_Lock(NULL)
    {
        pj_lock_create_recursive_mutex(pool->pool_(), name, &lock_);
    }
};

//////////////////////////////////////////////////////////////////////////////
// Semaphore lock object.
//
class Pj_Semaphore_Lock : public Pj_Lock
{
public:
    //
    // Default constructor.
    //
    explicit Pj_Semaphore_Lock(Pj_Pool *pool, 
                               unsigned max=PJ_MAXINT32,
                               unsigned initial=0,
                               const char *name=NULL)
        : Pj_Lock(NULL)
    {
        pj_lock_create_semaphore(pool->pool_(), name, initial, max, &lock_);
    }
};



#endif	/* __PJPP_LOCK_H__ */

