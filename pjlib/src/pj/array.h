/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/array.h,v 1.1 2005/12/02 20:02:28 nn Exp $ */
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
PJ_IDECL(void) pj_array_insert(void *array,
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
PJ_IDECL(void) pj_array_erase(void *array,
			      unsigned elem_size,
			      unsigned count,
			      unsigned pos);

/**
 * Search the first value in the array according to matching function.
 *
 * @param array	    the array.
 * @param elem_size the individual size of the element.
 * @param count	    the number of elements.
 * @param matching  the matching function, which MUST return PJ_OK if 
 *		    the specified element match.
 * @param result    the pointer to the value found.
 *
 * @return PJ_OK (zero) if value is found.
 */
PJ_IDECL(pj_status_t) pj_array_search(const void *array, 
				      unsigned elem_size, 
				      unsigned count, 
				      pj_status_t (*matching)(const void *value),
				      void **result);

/**
 * @}
 */

PJ_END_DECL


#if PJ_FUNCTIONS_ARE_INLINED
#   include <pj/array_i.h>
#endif

#endif	/* __PJ_ARRAY_H__ */

