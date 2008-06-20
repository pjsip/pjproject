/* $Id$ */
/* 
 * Copyright (C)2003-2008 Benny Prijono <benny@prijono.org>
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
#ifndef __PJ_POOL_ALT_H__
#define __PJ_POOL_ALT_H__

#define __PJ_POOL_H__


typedef struct pj_pool_t pj_pool_t;


/**
 * The type for function to receive callback from the pool when it is unable
 * to allocate memory. The elegant way to handle this condition is to throw
 * exception, and this is what is expected by most of this library 
 * components.
 */
typedef void pj_pool_callback(pj_pool_t *pool, pj_size_t size);

/**
 * This constant denotes the exception number that will be thrown by default
 * memory factory policy when memory allocation fails.
 */
extern int PJ_NO_MEMORY_EXCEPTION;



/*
 * Declare all pool API as macro that calls the implementation
 * function.
 */
#define pj_pool_create(fc,nm,init,inc,cb)   \
	pj_pool_create_imp(__FILE__, __LINE__, fc, nm, init, inc, cb)

#define pj_pool_release(pool)		    pj_pool_release_imp(pool)
#define pj_pool_getobjname(pool)	    pj_pool_getobjname_imp(pool)
#define pj_pool_reset(pool)		    pj_pool_reset_imp(pool)
#define pj_pool_get_capacity(pool)	    pj_pool_get_capacity_imp(pool)
#define pj_pool_get_used_size(pool)	    pj_pool_get_used_size_imp(pool)
#define pj_pool_alloc(pool,sz)		    \
	pj_pool_alloc_imp(__FILE__, __LINE__, pool, sz)

#define pj_pool_calloc(pool,cnt,elem)	    \
	pj_pool_calloc_imp(__FILE__, __LINE__, pool, cnt, elem)

#define pj_pool_zalloc(pool,sz)		    \
	pj_pool_zalloc_imp(__FILE__, __LINE__, pool, sz)



/*
 * Declare prototypes for pool implementation API.
 */

/* Create pool */
PJ_DECL(pj_pool_t*) pj_pool_create_imp(const char *file, int line,
				       void *factory,
				       const char *name,
				       pj_size_t initial_size,
				       pj_size_t increment_size,
				       pj_pool_callback *callback);

/* Release pool */
PJ_DECL(void) pj_pool_release_imp(pj_pool_t *pool);

/* Get pool name */
PJ_DECL(const char*) pj_pool_getobjname_imp(pj_pool_t *pool);

/* Reset pool */
PJ_DECL(void) pj_pool_reset_imp(pj_pool_t *pool);

/* Get capacity */
PJ_DECL(pj_size_t) pj_pool_get_capacity_imp(pj_pool_t *pool);

/* Get total used size */
PJ_DECL(pj_size_t) pj_pool_get_used_size_imp(pj_pool_t *pool);

/* Allocate memory from the pool */
PJ_DECL(void*) pj_pool_alloc_imp(const char *file, int line, 
				 pj_pool_t *pool, pj_size_t sz);

/* Allocate memory from the pool and zero the memory */
PJ_DECL(void*) pj_pool_calloc_imp(const char *file, int line, 
				  pj_pool_t *pool, unsigned cnt, 
				  unsigned elemsz);

/* Allocate memory from the pool and zero the memory */
PJ_DECL(void*) pj_pool_zalloc_imp(const char *file, int line, 
				  pj_pool_t *pool, pj_size_t sz);


typedef struct pj_pool_factory
{
    int dummy;
} pj_pool_factory;

typedef struct pj_caching_pool 
{
    pj_pool_factory factory;
} pj_caching_pool;


#define pj_caching_pool_init( cp, pol, mac)
#define pj_caching_pool_destroy(cp)
#define pj_pool_factory_dump(pf, detail)


#endif	/* __PJ_POOL_ALT_H__ */

