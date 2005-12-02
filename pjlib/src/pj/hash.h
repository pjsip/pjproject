/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/hash.h,v 1.1 2005/12/02 20:02:29 nn Exp $ */
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

#ifndef __PJ_HASH_H__
#define __PJ_HASH_H__

/**
 * @file hash.h
 * @brief Hash Table.
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_HASH Hash Table
 * @ingroup PJ_DS
 * @{
 * A hash table is a dictionary in which keys are mapped to array positions by
 * hash functions. Having the keys of more than one item map to the same 
 * position is called a collision. In this library, we will chain the nodes
 * that have the same key in a list.
 */

/**
 * If this constant is used as keylen, then the key is interpreted as
 * NULL terminated string.
 */
#define PJ_HASH_KEY_STRING	((unsigned)-1)

/**
 * The hash multiplier used to calculate hash value.
 */
#define PJ_HASH_MULTIPLIER	33

/**
 * This is the function that is used by the hash table to calculate hash value
 * of the specified key.
 *
 * @param hval	    the initial hash value, or zero.
 * @param key	    the key to calculate.
 * @param keylen    the length of the key, or PJ_HASH_KEY_STRING to treat 
 *		    the key as null terminated string.
 *
 * @return the hash value.
 */
PJ_DECL(pj_uint32_t) pj_hash_calc(pj_uint32_t hval, 
				  const void *key, unsigned keylen);


/**
 * Create a hash table with the specified 'bucket' size.
 *
 * @param pool	the pool from which the hash table will be allocated from.
 * @param size	the bucket size, which will be round-up to the nearest 2^n+1
 *
 * @return the hash table.
 */
PJ_DECL(pj_hash_table_t*) pj_hash_create(pj_pool_t *pool, unsigned size);


/**
 * Get the value associated with the specified key.
 *
 * @param ht	    the hash table.
 * @param key	    the key to look for.
 * @param keylen    the length of the key, or PJ_HASH_KEY_STRING to use the
 *		    string length of the key.
 *
 * @return the value associated with the key, or NULL if the key is not found.
 */
PJ_DECL(void *) pj_hash_get( pj_hash_table_t *ht,
			     const void *key, unsigned keylen );


/**
 * Associate/disassociate a value with the specified key.
 *
 * @param pool	    the pool to allocate the new entry if a new entry has to be
 *		    created.
 * @param ht	    the hash table.
 * @param key	    the key.
 * @param keylen    the length of the key, or PJ_HASH_KEY_STRING to use the 
 *		    string length of the key.
 * @param value	    value to be associated, or NULL to delete the entry with
 *		    the specified key.
 */
PJ_DECL(void) pj_hash_set( pj_pool_t *pool, pj_hash_table_t *ht,
			   const void *key, unsigned keylen,
			   void *value );

/**
 * Get the total number of entries in the hash table.
 *
 * @param ht	the hash table.
 *
 * @return the number of entries in the hash table.
 */
PJ_DECL(unsigned) pj_hash_count( pj_hash_table_t *ht );


/**
 * Get the iterator to the first element in the hash table. 
 *
 * @param ht	the hash table.
 * @param it	the iterator for iterating hash elements.
 *
 * @return the iterator to the hash element, or NULL if no element presents.
 */
PJ_DECL(pj_hash_iterator_t*) pj_hash_first( pj_hash_table_t *ht,
					    pj_hash_iterator_t *it );


/**
 * Get the next element from the iterator.
 *
 * @param ht	the hash table.
 * @param it	the hash iterator.
 *
 * @return the next iterator, or NULL if there's no more element.
 */
PJ_DECL(pj_hash_iterator_t*) pj_hash_next( pj_hash_table_t *ht, 
					   pj_hash_iterator_t *it );

/**
 * Get the value associated with a hash iterator.
 *
 * @param ht	the hash table.
 * @param it	the hash iterator.
 *
 * @return the value associated with the current element in iterator.
 */
PJ_DECL(void*) pj_hash_this( pj_hash_table_t *ht,
			     pj_hash_iterator_t *it );


/**
 * @}
 */

PJ_END_DECL

#endif


