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

#include <pj/list.h>

/* See if we use pool's alternate API.
 * The alternate API is used e.g. to implement pool debugging.
 */
#if PJ_HAS_POOL_ALT_API
#  include <pj/pool_alt.h>
#endif


#ifndef __PJ_POOL_H__
#define __PJ_POOL_H__

/**
 * @file pool.h
 * @brief Memory Pool.
 */

PJ_BEGIN_DECL

/**
 * @defgroup PJ_POOL_GROUP Memory Pool Management
 * @ingroup PJ
 * @brief
 * Memory pool management provides API to allocate and deallocate memory from
 * memory pool and to manage and establish policy for pool creation and
 * destruction in pool factory.
 *
 * \section PJ_POOL_FACTORY_SEC Pool Factory
 * See: \ref PJ_POOL_FACTORY "Pool Factory"
 *
 * A memory pool must be created through a factory. A factory not only provides
 * generic interface functions to create and release pool, but also provides 
 * strategy to manage the life time of pools. One sample implementation, 
 * \a pj_caching_pool, can be set to keep the pools released by application for
 * future use as long as the total memory is below the limit.
 * 
 * The pool factory interface declared in PJLIB is designed to be extensible.
 * Application can define its own strategy by creating it's own pool factory
 * implementation, and this strategy can be used even by existing library
 * without recompilation.
 *
 *
 * \section PJ_POOL_POLICY_SEC Pool Factory Policy
 * See: \ref PJ_POOL_FACTORY "Pool Factory Policy"
 *
 * A pool factory only defines functions to create and release pool and how
 * to manage pools, but the rest of the functionalities are controlled by
 * policy. A pool policy defines:
 *  - how memory block is allocated and deallocated (the default implementation
 *    allocates and deallocate memory by calling malloc() and free()).
 *  - callback to be called when memory allocation inside a pool fails (the
 *    default implementation will throw PJ_NO_MEMORY_EXCEPTION exception).
 *  - concurrency when creating and releasing pool from/to the factory.
 *
 * A pool factory can be given different policy during creation to make
 * it behave differently. For example, caching pool factory can be configured
 * to allocate and deallocate from a static/contiguous/preallocated memory 
 * instead of using malloc()/free().
 * 
 * What strategy/factory and what policy to use is not defined by PJLIB, but
 * instead is left to application to make use whichever is most efficient for
 * itself.
 *
 *
 * \section PJ_POOL_POOL_SEC The Pool
 * See: \ref PJ_POOL "Pool"
 *
 * The memory pool is an opaque object created by pool factory.
 * Application uses this object to request a memory chunk, by calling
 * #pj_pool_alloc or #pj_pool_calloc. When the application has finished using
 * the pool, it must call #pj_pool_release to free all the chunks previously
 * allocated and release the pool back to the factory.
 *
 * \section PJ_POOL_THREADING_SEC More on Threading Policies:
 * - By design, memory allocation from a pool is not thread safe. We assumed 
 *   that a pool will be owned by an object, and thread safety should be 
 *   handled by that object. Thus these functions are not thread safe: 
 *	- #pj_pool_alloc, 
 *	- #pj_pool_calloc, 
 *	- and other pool statistic functions.
 * - Threading in the pool factory is decided by the policy set for the
 *   factory when it was created.
 *
 * \section PJ_POOL_EXAMPLES_SEC Examples
 *
 * For some sample codes on how to use the pool, please see:
 *  - @ref page_pjlib_pool_test
 */

/**
 * @defgroup PJ_POOL Memory Pool.
 * @ingroup PJ_POOL_GROUP
 * @brief
 * A memory pool is initialized with an initial amount of memory, which is
 * called a block. Pool can be configured to dynamically allocate more memory 
 * blocks when it runs out of memory. Subsequent memory allocations by user 
 * will use up portions of these block. 
 * The pool doesn't keep track of individual memory allocations
 * by user, and the user doesn't have to free these indidual allocations. This
 * makes memory allocation simple and very fast. All the memory allocated from
 * the pool will be destroyed when the pool itself is destroyed.
 * @{
 */

/**
 * The type for function to receive callback from the pool when it is unable
 * to allocate memory. The elegant way to handle this condition is to throw
 * exception, and this is what is expected by most of this library 
 * components.
 */
typedef void pj_pool_callback(pj_pool_t *pool, pj_size_t size);

/**
 * This class, which is used internally by the pool, describes a single 
 * block of memory from which user memory allocations will be allocated from.
 */
typedef struct pj_pool_block
{
    PJ_DECL_LIST_MEMBER(struct pj_pool_block);  /**< List's prev and next.  */
    unsigned char    *buf;                      /**< Start of buffer.       */
    unsigned char    *cur;                      /**< Current alloc ptr.     */
    unsigned char    *end;                      /**< End of buffer.         */
} pj_pool_block;


/**
 * This structure describes the memory pool. Only implementors of pool factory
 * need to care about the contents of this structure.
 */
struct pj_pool_t
{
    PJ_DECL_LIST_MEMBER(struct pj_pool_t);  /**< Standard list elements.    */

    /** Pool name */
    char	    obj_name[PJ_MAX_OBJ_NAME];

    /** Pool factory. */
    pj_pool_factory *factory;

    /** Data put by factory */
    void	    *factory_data;

    /** Current capacity allocated by the pool. */
    pj_size_t	    capacity;

    /** Size of memory block to be allocated when the pool runs out of memory */
    pj_size_t	    increment_size;

    /** List of memory blocks allcoated by the pool. */
    pj_pool_block   block_list;

    /** The callback to be called when the pool is unable to allocate memory. */
    pj_pool_callback *callback;

};


/**
 * Guidance on how much memory required for initial pool administrative data.
 */
#define PJ_POOL_SIZE	        (sizeof(struct pj_pool_t))

/** 
 * Pool memory alignment (must be power of 2). 
 */
#ifndef PJ_POOL_ALIGNMENT
#   define PJ_POOL_ALIGNMENT    4
#endif

/**
 * Create a new pool from the pool factory. This wrapper will call create_pool
 * member of the pool factory.
 *
 * @param factory	    The pool factory.
 * @param name		    The name to be assigned to the pool. The name should 
 *			    not be longer than PJ_MAX_OBJ_NAME (32 chars), or 
 *			    otherwise it will be truncated.
 * @param initial_size	    The size of initial memory blocks taken by the pool.
 *			    Note that the pool will take 68+20 bytes for 
 *			    administrative area from this block.
 * @param increment_size    the size of each additional blocks to be allocated
 *			    when the pool is running out of memory. If user 
 *			    requests memory which is larger than this size, then 
 *			    an error occurs.
 *			    Note that each time a pool allocates additional block, 
 *			    it needs PJ_POOL_SIZE more to store some 
 *			    administrative info.
 * @param callback	    Callback to be called when error occurs in the pool.
 *			    If this value is NULL, then the callback from pool
 *			    factory policy will be used.
 *			    Note that when an error occurs during pool creation, 
 *			    the callback itself is not called. Instead, NULL 
 *			    will be returned.
 *
 * @return                  The memory pool, or NULL.
 */
PJ_IDECL(pj_pool_t*) pj_pool_create(pj_pool_factory *factory, 
				    const char *name,
				    pj_size_t initial_size, 
				    pj_size_t increment_size,
				    pj_pool_callback *callback);

/**
 * Release the pool back to pool factory.
 *
 * @param pool	    Memory pool.
 */
PJ_IDECL(void) pj_pool_release( pj_pool_t *pool );

/**
 * Get pool object name.
 *
 * @param pool the pool.
 *
 * @return pool name as NULL terminated string.
 */
PJ_IDECL(const char *) pj_pool_getobjname( const pj_pool_t *pool );

/**
 * Reset the pool to its state when it was initialized.
 * This means that if additional blocks have been allocated during runtime, 
 * then they will be freed. Only the original block allocated during 
 * initialization is retained. This function will also reset the internal 
 * counters, such as pool capacity and used size.
 *
 * @param pool the pool.
 */
PJ_DECL(void) pj_pool_reset( pj_pool_t *pool );


/**
 * Get the pool capacity, that is, the system storage that have been allocated
 * by the pool, and have been used/will be used to allocate user requests.
 * There's no guarantee that the returned value represent a single
 * contiguous block, because the capacity may be spread in several blocks.
 *
 * @param pool	the pool.
 *
 * @return the capacity.
 */
PJ_IDECL(pj_size_t) pj_pool_get_capacity( pj_pool_t *pool );

/**
 * Get the total size of user allocation request.
 *
 * @param pool	the pool.
 *
 * @return the total size.
 */
PJ_IDECL(pj_size_t) pj_pool_get_used_size( pj_pool_t *pool );

/**
 * Allocate storage with the specified size from the pool.
 * If there's no storage available in the pool, then the pool can allocate more
 * blocks if the increment size is larger than the requested size.
 *
 * @param pool	    the pool.
 * @param size	    the requested size.
 *
 * @return pointer to the allocated memory.
 */
PJ_IDECL(void*) pj_pool_alloc( pj_pool_t *pool, pj_size_t size);

/**
 * Allocate storage  from the pool, and initialize it to zero.
 * This function behaves like pj_pool_alloc(), except that the storage will
 * be initialized to zero.
 *
 * @param pool	    the pool.
 * @param count	    the number of elements in the array.
 * @param elem	    the size of individual element.
 *
 * @return pointer to the allocated memory.
 */
PJ_IDECL(void*) pj_pool_calloc( pj_pool_t *pool, pj_size_t count, 
				pj_size_t elem);


/**
 * @def pj_pool_zalloc(pj_pool_t *pool, pj_size_t size)
 * Allocate storage from the pool and initialize it to zero.
 *
 * @param pool	    The pool.
 * @param size	    The size to be allocated.
 *
 * @return	    Pointer to the allocated memory.
 */
#define pj_pool_zalloc(pool, size)  pj_pool_calloc(pool, 1, size)


/**
 * @}	// PJ_POOL
 */

///////////////////////////////////////////////////////////////////////////////
/**
 * @defgroup PJ_POOL_FACTORY Pool Factory and Policy.
 * @ingroup PJ_POOL_GROUP
 * @brief
 * Pool factory declares an interface to create and destroy pool. There may
 * be several strategies for pool creation, and these strategies should 
 * implement the interface defined by pool factory.
 *
 * \section PJ_POOL_FACTORY_ITF Pool Factory Interface
 * The pool factory defines the following interface:
 *  - \a policy: the memory pool factory policy.
 *  - \a create_pool(): create a new memory pool.
 *  - \a release_pool(): release memory pool back to factory.
 *
 * \section PJ_POOL_FACTORY_POL Pool Factory Policy.
 * The pool factory policy controls the behaviour of memory factories, and
 * defines the following interface:
 *  - \a block_alloc(): allocate memory block from backend memory mgmt/system.
 *  - \a block_free(): free memory block back to backend memory mgmt/system.
 * @{
 */

/* We unfortunately don't have support for factory policy options as now,
   so we keep this commented at the moment.
enum PJ_POOL_FACTORY_OPTION
{
    PJ_POOL_FACTORY_SERIALIZE = 1
};
*/

/**
 * This structure declares pool factory interface.
 */
typedef struct pj_pool_factory_policy
{
    /**
     * Allocate memory block (for use by pool). This function is called
     * by memory pool to allocate memory block.
     * 
     * @param factory	Pool factory.
     * @param size	The size of memory block to allocate.
     *
     * @return		Memory block.
     */
    void* (*block_alloc)(pj_pool_factory *factory, pj_size_t size);

    /**
     * Free memory block.
     *
     * @param factory	Pool factory.
     * @param mem	Memory block previously allocated by block_alloc().
     * @param size	The size of memory block.
     */
    void (*block_free)(pj_pool_factory *factory, void *mem, pj_size_t size);

    /**
     * Default callback to be called when memory allocation fails.
     */
    pj_pool_callback *callback;

    /**
     * Option flags.
     */
    unsigned flags;

} pj_pool_factory_policy;

/**
 * This constant denotes the exception number that will be thrown by default
 * memory factory policy when memory allocation fails.
 */
extern int PJ_NO_MEMORY_EXCEPTION;

/**
 * This global variable points to default memory pool factory policy.
 * The behaviour of the default policy is:
 *  - block allocation and deallocation use malloc() and free().
 *  - callback will raise PJ_NO_MEMORY_EXCEPTION exception.
 *  - access to pool factory is not serialized (i.e. not thread safe).
 */
extern pj_pool_factory_policy pj_pool_factory_default_policy;

/**
 * This structure contains the declaration for pool factory interface.
 */
struct pj_pool_factory
{
    /**
     * Memory pool policy.
     */
    pj_pool_factory_policy policy;

    /**
    * Create a new pool from the pool factory.
    *
    * @param factory	The pool factory.
    * @param name	the name to be assigned to the pool. The name should 
    *			not be longer than PJ_MAX_OBJ_NAME (32 chars), or 
    *			otherwise it will be truncated.
    * @param initial_size the size of initial memory blocks taken by the pool.
    *			Note that the pool will take 68+20 bytes for 
    *			administrative area from this block.
    * @param increment_size the size of each additional blocks to be allocated
    *			when the pool is running out of memory. If user 
    *			requests memory which is larger than this size, then 
    *			an error occurs.
    *			Note that each time a pool allocates additional block, 
    *			it needs 20 bytes (equal to sizeof(pj_pool_block)) to 
    *			store some administrative info.
    * @param callback	Cllback to be called when error occurs in the pool.
    *			Note that when an error occurs during pool creation, 
    *			the callback itself is not called. Instead, NULL 
    *			will be returned.
    *
    * @return the memory pool, or NULL.
    */
    pj_pool_t*	(*create_pool)( pj_pool_factory *factory,
				const char *name,
				pj_size_t initial_size, 
				pj_size_t increment_size,
				pj_pool_callback *callback);

    /**
     * Release the pool to the pool factory.
     *
     * @param factory	The pool factory.
     * @param pool	The pool to be released.
    */
    void (*release_pool)( pj_pool_factory *factory, pj_pool_t *pool );

    /**
     * Dump pool status to log.
     *
     * @param factory	The pool factory.
     */
    void (*dump_status)( pj_pool_factory *factory, pj_bool_t detail );
};

/**
 * This function is intended to be used by pool factory implementors.
 * @param factory           Pool factory.
 * @param name              Pool name.
 * @param initial_size      Initial size.
 * @param increment_size    Increment size.
 * @param callback          Callback.
 * @return                  The pool object, or NULL.
 */
PJ_DECL(pj_pool_t*) pj_pool_create_int(	pj_pool_factory *factory, 
					const char *name,
					pj_size_t initial_size, 
					pj_size_t increment_size,
					pj_pool_callback *callback);

/**
 * This function is intended to be used by pool factory implementors.
 * @param pool              The pool.
 * @param name              Pool name.
 * @param increment_size    Increment size.
 * @param callback          Callback function.
 */
PJ_DECL(void) pj_pool_init_int( pj_pool_t *pool, 
				const char *name,
				pj_size_t increment_size,
				pj_pool_callback *callback);

/**
 * This function is intended to be used by pool factory implementors.
 * @param pool      The memory pool.
 */
PJ_DECL(void) pj_pool_destroy_int( pj_pool_t *pool );


/**
 * Dump pool factory state.
 * @param pf	    The pool factory.
 * @param detail    Detail state required.
 */
PJ_INLINE(void) pj_pool_factory_dump( pj_pool_factory *pf,
				      pj_bool_t detail )
{
    (*pf->dump_status)(pf, detail);
}

/**
 *  @}	// PJ_POOL_FACTORY
 */

///////////////////////////////////////////////////////////////////////////////

/**
 * @defgroup PJ_CACHING_POOL Caching Pool Factory.
 * @ingroup PJ_POOL_GROUP
 * @brief
 * Caching pool is one sample implementation of pool factory where the
 * factory can reuse memory to create a pool. Application defines what the 
 * maximum memory the factory can hold, and when a pool is released the
 * factory decides whether to destroy the pool or to keep it for future use.
 * If the total amount of memory in the internal cache is still within the
 * limit, the factory will keep the pool in the internal cache, otherwise the
 * pool will be destroyed, thus releasing the memory back to the system.
 *
 * @{
 */

/**
 * Number of unique sizes, to be used as index to the free list.
 * Each pool in the free list is organized by it's size.
 */
#define PJ_CACHING_POOL_ARRAY_SIZE	16

/**
 * Declaration for caching pool. Application doesn't normally need to
 * care about the contents of this struct, it is only provided here because
 * application need to define an instance of this struct (we can not allocate
 * the struct from a pool since there is no pool factory yet!).
 */
struct pj_caching_pool 
{
    /** Pool factory interface, must be declared first. */
    pj_pool_factory factory;

    /** Current factory's capacity, i.e. number of bytes that are allocated
     *  and available for application in this factory. The factory's
     *  capacity represents the size of all pools kept by this factory
     *  in it's free list, which will be returned to application when it
     *  requests to create a new pool.
     */
    pj_size_t	    capacity;

    /** Maximum size that can be held by this factory. Once the capacity
     *  has exceeded @a max_capacity, further #pj_pool_release() will
     *  flush the pool. If the capacity is still below the @a max_capacity,
     *  #pj_pool_release() will save the pool to the factory's free list.
     */
    pj_size_t       max_capacity;

    /**
     * Number of pools currently held by applications. This number gets
     * incremented everytime #pj_pool_create() is called, and gets
     * decremented when #pj_pool_release() is called.
     */
    pj_size_t       used_count;

    /**
     * Lists of pools in the cache, indexed by pool size.
     */
    pj_list	    free_list[PJ_CACHING_POOL_ARRAY_SIZE];

    /**
     * List of pools currently allocated by applications.
     */
    pj_list	    used_list;

    /**
     * Internal pool.
     */
    pj_pool_t	   *pool;

    /**
     * Mutex.
     */
    pj_mutex_t	   *mutex;
};



/**
 * Initialize caching pool.
 *
 * @param ch_pool	The caching pool factory to be initialized.
 * @param policy	Pool factory policy.
 * @param max_capacity	The total capacity to be retained in the cache. When
 *			the pool is returned to the cache, it will be kept in
 *			recycling list if the total capacity of pools in this
 *			list plus the capacity of the pool is still below this
 *			value.
 */
PJ_DECL(void) pj_caching_pool_init( pj_caching_pool *ch_pool, 
				    const pj_pool_factory_policy *policy,
				    pj_size_t max_capacity);


/**
 * Destroy caching pool, and release all the pools in the recycling list.
 *
 * @param ch_pool	The caching pool.
 */
PJ_DECL(void) pj_caching_pool_destroy( pj_caching_pool *ch_pool );

/**
 * @}	// PJ_CACHING_POOL
 */

#  if PJ_FUNCTIONS_ARE_INLINED
#    include "pool_i.h"
#  endif

PJ_END_DECL
    
#endif	/* __PJ_POOL_H__ */

