/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/string.h,v 1.1 2005/12/02 20:02:30 nn Exp $ */
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

#ifndef __PJ_STRING_H__
#define __PJ_STRING_H__

/**
 * @file string.h
 * @brief PJLIB String Operations.
 */

#include <pj/types.h>
#include <string.h>

#if defined(PJ_WIN32) && PJ_WIN32==1
#  define strcasecmp	stricmp
#  define strncasecmp	strnicmp
#  define snprintf	_snprintf
#  define vsnprintf	_vsnprintf	
#elif defined(PJ_WIN32_WINCE)
//#  define strcasecmp	stricmp
//#  define strncasecmp	strnicmp
#  define snprintf	_snprintf
#  define vsnprintf	_vsnprintf	
#else
#  define stricmp	strcasecmp
#  define strnicmp	strncasecmp
#endif


PJ_BEGIN_DECL

/**
 * @defgroup PJ_PSTR String Operations
 * @ingroup PJ_DS
 * @{
 */

/**
 * Create string initializer from a normal C string.
 *
 * @param str	Null terminated string to be stored.
 *
 * @return pj_str_t.
 */
PJ_IDECL(pj_str_t) pj_str(char *str);

/**
 * Set the pointer and length to the specified value.
 *
 * @param str	    the string.
 * @param ptr	    pointer to set.
 * @param length    length to set.
 *
 * @return the string.
 */
PJ_IDECL(pj_str_t*) pj_strset( pj_str_t *str, char *ptr, pj_size_t length);

/**
 * Set the pointer and length of the string to the source string, which
 * must be NULL terminated.
 *
 * @param str	    the string.
 * @param src	    pointer to set.
 *
 * @return the string.
 */
PJ_IDECL(pj_str_t*) pj_strset2( pj_str_t *str, char *src);

/**
 * Set the pointer and the length of the string.
 *
 * @param str	    The target string.
 * @param begin	    The start of the string.
 * @param end	    The end of the string.
 *
 * @return the target string.
 */
PJ_IDECL(pj_str_t*) pj_strset3( pj_str_t *str, char *begin, char *end );

/**
 * Assign string.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 *
 * @return the target string.
 */
PJ_IDECL(pj_str_t*) pj_strassign( pj_str_t *dst, pj_str_t *src );

/**
 * Copy string contents.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 *
 * @return the target string.
 */
PJ_IDECL(pj_str_t*) pj_strcpy(pj_str_t *dst, const pj_str_t *src);

/**
 * Copy string contents.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 *
 * @return the target string.
 */
PJ_IDECL(pj_str_t*) pj_strcpy2(pj_str_t *dst, const char *src);

/**
 * Duplicate string.
 *
 * @param pool	    The pool.
 * @param dst	    The string result.
 * @param src	    The string to duplicate.
 *
 * @return the string result.
 */
PJ_IDECL(pj_str_t*) pj_strdup(pj_pool_t *pool,
			      pj_str_t *dst,
			      const pj_str_t *src);

/**
 * Duplicate string and NULL terminate the destination string.
 *
 * @param pool
 * @param dst
 * @param src
 */
PJ_IDECL(pj_str_t*) pj_strdup_with_null(pj_pool_t *pool,
					pj_str_t *dst,
					const pj_str_t *src);

/**
 * Duplicate string.
 *
 * @param pool	    The pool.
 * @param dst	    The string result.
 * @param src	    The string to duplicate.
 *
 * @return the string result.
 */
PJ_IDECL(pj_str_t*) pj_strdup2(pj_pool_t *pool,
			       pj_str_t *dst,
			       const char *src);

/**
 * Duplicate string.
 *
 * @param pool	    The pool.
 * @param src	    The string to duplicate.
 *
 * @return the string result.
 */
PJ_IDECL(pj_str_t) pj_strdup3(pj_pool_t *pool, const char *src);

/**
 * Return the length of the string.
 *
 * @param str	    The string.
 *
 * @return the length of the string.
 */
PJ_IDECL(pj_size_t) pj_strlen( const pj_str_t *str );

/**
 * Return the pointer to the string data.
 *
 * @param str	    The string.
 *
 * @return the pointer to the string buffer.
 */
PJ_IDECL(const char*) pj_strbuf( const pj_str_t *str );

/**
 * Compare strings. 
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strcmp( const pj_str_t *str1, const pj_str_t *str2);

/**
 * Compare strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strcmp2( const pj_str_t *str1, const char *str2 );

/**
 * Compare strings. 
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strncmp( const pj_str_t *str1, const pj_str_t *str2, 
			  pj_size_t len);

/**
 * Compare strings. 
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strncmp2( const pj_str_t *str1, const char *str2, 
			   pj_size_t len);

/**
 * Perform lowercase comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_stricmp( const pj_str_t *str1, const pj_str_t *str2);

/**
 * Perform lowercase comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_stricmp2( const pj_str_t *str1, const char *str2);

/**
 * Perform lowercase comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strnicmp( const pj_str_t *str1, const pj_str_t *str2, 
			   pj_size_t len);

/**
 * Perform lowercase comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
PJ_IDECL(int) pj_strnicmp2( const pj_str_t *str1, const char *str2, 
			    pj_size_t len);

/**
 * Concatenate strings.
 *
 * @param dst	    The destination string.
 * @param src	    The source string.
 */
PJ_IDECL(void) pj_strcat(pj_str_t *dst, const pj_str_t *src);

/**
 * Finds a character in a string.
 *
 * @param str	    The string.
 * @param chr	    The character to find.
 *
 * @return the pointer to first character found, or NULL.
 */
PJ_IDECL(char*) pj_strchr( pj_str_t *str, int chr);

/**
 * Remove (trim) leading whitespaces from the string.
 *
 * @param str	    The string.
 *
 * @return the string.
 */
PJ_DECL(pj_str_t*) pj_strltrim( pj_str_t *str );

/**
 * Remove (trim) the trailing whitespaces from the string.
 *
 * @param str	    The string.
 *
 * @return the string.
 */
PJ_DECL(pj_str_t*) pj_strrtrim( pj_str_t *str );

/**
 * Remove (trim) leading and trailing whitespaces from the string.
 *
 * @param str	    The string.
 *
 * @return the string.
 */
PJ_IDECL(pj_str_t*) pj_strtrim( pj_str_t *str );

/**
 * Create a randomize string with the specified length. The string is NOT
 * guaranteed to be globally unique, but it will be unique only in one 
 * instance of the program.
 *
 * @param str	    the string to store the result.
 * @param length    the length of the random string to generate.
 *
 * @return the string.
 */
PJ_DECL(pj_str_t*) pj_create_random_string(pj_str_t *str, int length);

/**
 * Convert string to unsigned integer.
 *
 * @param str	the string.
 *
 * @return the unsigned integer.
 */
PJ_DECL(unsigned long) pj_strtoul(const pj_str_t *str);

/**
 * Utility to convert unsigned integer to string.
 *
 * @param val	    the unsigned integer
 * @param buf	    the buffer
 *
 * @return the number of characters written
 */
PJ_DECL(int) pj_utoa(unsigned val, char *buf);

/**
 * Wrapper for rand().
 *
 * @return	    integer.
 */
PJ_DECL(int) pj_rand(void);

/**
 * snprintf.
 * @return the number of characters printed.
 */
#if defined(PJ_WIN32) && !defined(PJ_WIN32_WINCE)
#   define pj_vsnprintf _vsnprintf 
#   define pj_snprintf _snprintf
#else
#   define pj_vsnprintf vsnprintf
#   define pj_snprintf snprintf
#endif

/**
 * Fill the memory location with value.
 *
 * @param dst	    The destination buffer.
 * @param c	    Character to set.
 * @param size	    The number of characters.
 *
 * @return the value of dst.
 */
PJ_IDECL(void*) pj_memset(void *dst, int c, pj_size_t size);

/**
 * Copy buffer.
 *
 * @param dst	    The destination buffer.
 * @param src	    The source buffer.
 * @param size	    The size to copy.
 *
 * @return the destination buffer.
 */
PJ_IDECL(void*) pj_memcpy(void *dst, const void *src, pj_size_t size);

/**
 * Move memory.
 *
 * @param dst	    The destination buffer.
 * @param src	    The source buffer.
 * @param size	    The size to copy.
 *
 * @return the destination buffer.
 */
PJ_IDECL(void*) pj_memmove(void *dst, const void *src, pj_size_t size);

/**
 * Compare buffers.
 *
 * @param buf1	    The first buffer.
 * @param buf2	    The second buffer.
 * @param size	    The size to compare.
 *
 * @return negative, zero, or positive value.
 */
PJ_IDECL(int) pj_memcmp(const void *buf1, const void *buf2, pj_size_t size);

/**
 * Find character in the buffer.
 *
 * @param buf	    The buffer.
 * @param c	    The character to find.
 * @param size	    The size to check.
 *
 * @return the pointer to location where the character is found, or NULL if
 *         not found.
 */
PJ_IDECL(void*) pj_memchr(const void *buf, int c, pj_size_t size);

/**
 * @}
 */

#if PJ_FUNCTIONS_ARE_INLINED
#  include <pj/string_i.h>
#endif

PJ_END_DECL

#endif	/* __PJ_STRING_H__ */

