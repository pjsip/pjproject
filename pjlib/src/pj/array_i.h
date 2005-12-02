/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/array_i.h,v 1.1 2005/12/02 20:02:28 nn Exp $ */
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


#include <pj/string.h>

PJ_IDEF(void) pj_array_insert(void *array,
			      unsigned elem_size,
			      unsigned count,
			      unsigned pos,
			      const void *value)
{
    if (count && pos < count-1) {
	pj_memmove( (char*)array + (pos+1)*elem_size,
		    (char*)array + pos*elem_size,
		    (count-pos)*elem_size);
    }
    pj_memmove((char*)array + pos*elem_size, value, elem_size);
}

PJ_IDEF(void) pj_array_erase(void *array,
			     unsigned elem_size,
			     unsigned count,
			     unsigned pos)
{
    pj_assert(count != 0);
    if (pos < count-1) {
	pj_memmove( (char*)array + pos*elem_size,
		    (char*)array + (pos+1)*elem_size,
		    (count-pos-1)*elem_size);
    }
}

PJ_IDEF(pj_status_t) pj_array_find(const void *array, 
				   unsigned elem_size, 
				   unsigned count, 
				   pj_status_t (*matching)(const void *value),
				   void **result)
{
    unsigned i;
    const char *char_array = array;
    for (i=0; i<count; ++i) {
	if ( (*matching)(char_array) == PJ_OK) {
	    if (result) {
		*result = (void*)char_array;
	    }
	    return PJ_OK;
	}
	char_array += elem_size;
    }
    return -1;
}

