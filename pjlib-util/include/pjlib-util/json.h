/* $Id$ */
/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJLIB_UTIL_JSON_H__
#define __PJLIB_UTIL_JSON_H__


/**
 * @file json.h
 * @brief PJLIB JSON Implementation
 */

#include <pj/types.h>
#include <pj/list.h>
#include <pj/pool.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_JSON JSON Writer and Loader
 * @ingroup PJ_FILE_FMT
 * @{
 * This API implements JSON file format according to RFC 4627. It can be used
 * to parse, write, and manipulate JSON documents.
 */

/**
 * Type of JSON value.
 */
typedef enum pj_json_val_type
{
    PJ_JSON_VAL_NULL,		/**< Null value (null)			*/
    PJ_JSON_VAL_BOOL,		/**< Boolean value (true, false)	*/
    PJ_JSON_VAL_NUMBER,		/**< Numeric (float or fixed point)	*/
    PJ_JSON_VAL_STRING,		/**< Literal string value.		*/
    PJ_JSON_VAL_ARRAY,		/**< Array				*/
    PJ_JSON_VAL_OBJ		/**< Object.				*/
} pj_json_val_type;

/* Forward declaration for JSON element */
typedef struct pj_json_elem pj_json_elem;

/**
 * JSON list to store child elements.
 */
typedef struct pj_json_list
{
    PJ_DECL_LIST_MEMBER(pj_json_elem);
} pj_json_list;

/**
 * This represents JSON element. A JSON element is basically a name/value
 * pair, where the name is a string and the value can be one of null, boolean
 * (true and false constants), number, string, array (containing zero or more
 * elements), or object. An object can be viewed as C struct, that is a
 * compound element containing other elements, each having name/value pair.
 */
struct pj_json_elem
{
    PJ_DECL_LIST_MEMBER(pj_json_elem);
    pj_str_t		name;		/**< ELement name.		*/
    pj_json_val_type	type;		/**< Element type.		*/
    union
    {
	pj_bool_t	is_true;	/**< Boolean value.		*/
	float		num;		/**< Number value.		*/
	pj_str_t	str;		/**< String value.		*/
	pj_json_list	children;	/**< Object and array children	*/
    } value;				/**< Element value.		*/
};

/**
 * Structure to be specified to pj_json_parse() to be filled with additional
 * info when parsing failed.
 */
typedef struct pj_json_err_info
{
    unsigned	line;		/**< Line location of the error		*/
    unsigned	col;		/**< Column location of the error	*/
    int		err_char;	/**< The offending character.		*/
} pj_json_err_info;

/**
 * Type of function callback to write JSON document in pj_json_writef().
 *
 * @param s		The string to be written to the document.
 * @param size		The length of the string
 * @param user_data	User data that was specified to pj_json_writef()
 *
 * @return		If the callback returns non-PJ_SUCCESS, it will
 * 			stop the pj_json_writef() function and this error
 * 			will be returned to caller.
 */
typedef pj_status_t (*pj_json_writer)(const char *s,
				      unsigned size,
				      void *user_data);

/**
 * Initialize null element.
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 */
PJ_DECL(void) pj_json_elem_null(pj_json_elem *el, pj_str_t *name);

/**
 * Initialize boolean element with the specified value.
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 * @param val		The value.
 */
PJ_DECL(void) pj_json_elem_bool(pj_json_elem *el, pj_str_t *name,
                                pj_bool_t val);

/**
 * Initialize number element with the specified value.
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 * @param val		The value.
 */
PJ_DECL(void) pj_json_elem_number(pj_json_elem *el, pj_str_t *name,
                                  float val);

/**
 * Initialize string element with the specified value.
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 * @param val		The value.
 */
PJ_DECL(void) pj_json_elem_string(pj_json_elem *el, pj_str_t *name,
                                  pj_str_t *val);

/**
 * Initialize element as an empty array
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 */
PJ_DECL(void) pj_json_elem_array(pj_json_elem *el, pj_str_t *name);

/**
 * Initialize element as an empty object
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 */
PJ_DECL(void) pj_json_elem_obj(pj_json_elem *el, pj_str_t *name);

/**
 * Add an element to an object or array.
 *
 * @param el		The object or array element.
 * @param child		Element to be added to the object or array.
 */
PJ_DECL(void) pj_json_elem_add(pj_json_elem *el, pj_json_elem *child);

/**
 * Parse a JSON document in the buffer. The buffer MUST be NULL terminated,
 * or if not then it must have enough size to put the NULL character.
 *
 * @param pool		The pool to allocate memory for creating elements.
 * @param buffer	String buffer containing JSON document.
 * @param size		Size of the document.
 * @param err_info	Optional structure to be filled with info when
 * 			parsing failed.
 *
 * @return		The root element from the document.
 */
PJ_DECL(pj_json_elem*) pj_json_parse(pj_pool_t *pool,
                                     char *buffer,
                                     unsigned *size,
                                     pj_json_err_info *err_info);

/**
 * Write the specified element to the string buffer.
 *
 * @param elem		The element to be written.
 * @param buffer	Output buffer.
 * @param size		On input, it must be set to the size of the buffer.
 * 			Upon successful return, this will be set to
 * 			the length of the written string.
 *
 * @return		PJ_SUCCESS on success or the appropriate error.
 */
PJ_DECL(pj_status_t)   pj_json_write(const pj_json_elem *elem,
                                     char *buffer, unsigned *size);

/**
 * Incrementally write the element to arbitrary medium using the specified
 * callback to write the document chunks.
 *
 * @param elem		The element to be written.
 * @param writer	Callback function which will be called to write
 * 			text chunks.
 * @param user_data	Arbitrary user data which will be given back when
 * 			calling the callback.
 *
 * @return		PJ_SUCCESS on success or the appropriate error.
 */
PJ_DECL(pj_status_t)   pj_json_writef(const pj_json_elem *elem,
                                      pj_json_writer writer,
                                      void *user_data);

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJLIB_UTIL_JSON_H__ */
