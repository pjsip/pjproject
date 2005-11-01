/* $Id$
 *
 */
#ifndef __PJ_ARRAY_H__
#define __PJ_ARRAY_H__

/**
 * @file array.h
 * @brief PJLIB Array helper.
 */
#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_ARRAY Array helper.
 * @ingroup PJ_DS
 * @{
 *
 * This module provides helper to manipulate array of elements of any size.
 * It provides most used array operations such as insert, erase, and search.
 */

/**
 * Insert value to the array at the given position, and rearrange the
 * remaining nodes after the position.
 *
 * @param array	    the array.
 * @param elem_size the size of the individual element.
 * @param count	    the current number of elements in the array.
 * @param pos	    the position where the new element is put.
 * @param value	    the value to copy to the new element.
 */
PJ_DECL(void) pj_array_insert( void *array,
			       unsigned elem_size,
			       unsigned count,
			       unsigned pos,
			       const void *value);

/**
 * Erase a value from the array at given position, and rearrange the remaining
 * elements post the erased element.
 *
 * @param array	    the array.
 * @param elem_size the size of the individual element.
 * @param count	    the current number of elements in the array.
 * @param pos	    the index/position to delete.
 */
PJ_DECL(void) pj_array_erase( void *array,
			      unsigned elem_size,
			      unsigned count,
			      unsigned pos);

/**
 * Search the first value in the array according to matching function.
 *
 * @param array	    the array.
 * @param elem_size the individual size of the element.
 * @param count	    the number of elements.
 * @param matching  the matching function, which MUST return PJ_SUCCESS if 
 *		    the specified element match.
 * @param result    the pointer to the value found.
 *
 * @return	    PJ_SUCCESS if value is found, otherwise the error code.
 */
PJ_DECL(pj_status_t) pj_array_find(   const void *array, 
				      unsigned elem_size, 
				      unsigned count, 
				      pj_status_t (*matching)(const void *value),
				      void **result);

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJ_ARRAY_H__ */

