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
#ifndef __PJ_CTYPE_H__
#define __PJ_CTYPE_H__

/**
 * @file ctype.h
 * @brief C type helper macros.
 */

#include <pj/compat/ctype.h>

/**
 * @defgroup pj_ctype ctype - Character Type
 * @ingroup PJ_MISC
 * @{
 *
 * This module contains several inline functions/macros for testing or
 * manipulating character types. It is provided in PJLIB because PJLIB
 * must not depend to LIBC.
 */

/** 
 * Returns a non-zero value if either isalpha or isdigit is true for c.
 * @param c     The integer character to test.
 * @return      Non-zero value if either isalpha or isdigit is true for c.
 */
PJ_INLINE(int) pj_isalnum(int c) { return isalnum(c); }

/** 
 * Returns a non-zero value if c is a particular representation of an 
 * alphabetic character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of an 
 *              alphabetic character.
 */
PJ_INLINE(int) pj_isalpha(int c) { return isalpha(c); }

/** 
 * Returns a non-zero value if c is a particular representation of an 
 * ASCII character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              an ASCII character.
 */
PJ_INLINE(int) pj_isascii(int c) { return isascii(c); }

/** 
 * Returns a non-zero value if c is a particular representation of 
 * a decimal-digit character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a decimal-digit character.
 */
PJ_INLINE(int) pj_isdigit(int c) { return isdigit(c); }

/** 
 * Returns a non-zero value if c is a particular representation of 
 * a space character (0x09 - 0x0D or 0x20).
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a space character (0x09 - 0x0D or 0x20).
 */
PJ_INLINE(int) pj_isspace(int c) { return isspace(c); }

/** 
 * Returns a non-zero value if c is a particular representation of 
 * a lowercase character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a lowercase character.
 */
PJ_INLINE(int) pj_islower(int c) { return islower(c); }


/** 
 * Returns a non-zero value if c is a particular representation of 
 * a uppercase character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a uppercase character.
 */
PJ_INLINE(int) pj_isupper(int c) { return isupper(c); }

/**
 * Returns a non-zero value if c is a particular representation of 
 * an hexadecimal digit character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              an hexadecimal digit character.
 */
PJ_INLINE(int) pj_isxdigit(int c){ return isxdigit(c); }

/**
 * Returns a non-zero value if c is a either a space (' ') or horizontal
 * tab ('\\t') character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a either a space (' ') or horizontal
 *              tab ('\\t') character.
 */
PJ_INLINE(int) pj_isblank(int c) { return isblank(c); }

/**
 * Converts character to lowercase.
 * @param c     The integer character to convert.
 * @return      Lowercase character of c.
 */
PJ_INLINE(int) pj_tolower(int c) { return tolower(c); }

/**
 * Converts character to uppercase.
 * @param c     The integer character to convert.
 * @return      Uppercase character of c.
 */
PJ_INLINE(int) pj_toupper(int c) { return toupper(c); }

/** @} */

#endif	/* __PJ_CTYPE_H__ */

