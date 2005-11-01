/* $Header: /pjproject-0.3/pjlib/src/pj/array.c 5     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/array.c $
 * 
 * 5     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 4     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#include <pj/array.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/errno.h>

PJ_DEF(void) pj_array_insert( void *array,
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

PJ_DEF(void) pj_array_erase( void *array,
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

PJ_DEF(pj_status_t) pj_array_find( const void *array, 
				   unsigned elem_size, 
				   unsigned count, 
				   pj_status_t (*matching)(const void *value),
				   void **result)
{
    unsigned i;
    const char *char_array = array;
    for (i=0; i<count; ++i) {
	if ( (*matching)(char_array) == PJ_SUCCESS) {
	    if (result) {
		*result = (void*)char_array;
	    }
	    return PJ_SUCCESS;
	}
	char_array += elem_size;
    }
    return PJ_ENOTFOUND;
}

