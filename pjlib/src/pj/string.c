/* $Header: /cvs/pjproject-0.2.9.3/pjlib/src/pj/string.c,v 1.1 2005/12/02 20:02:30 nn Exp $ */
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
#include <pj/pool.h>
#include <ctype.h>	/* isspace() */
#include <stdlib.h>	/* rand() */

#if defined(PJ_WIN32_WINCE)

int strncasecmp( const char *s1, const char *s2, size_t count ) {
	int i;
	char *r1 = s1;
	char *r2 = s2;

	for (i=0; i<count; i++) {
		if (*r1=='\0' && *r2=='\0')
			break;
		if (*r1=='\0' || *r2=='\0')
			return -1;
		if (tolower(*r1)!=tolower(*r2)) {
			return -1;
		}
		r1++;
		r2++;
	}
	
	return 0;
}

int strcasecmp( const char *s1, const char *s2 )
{
	int i = 0;
	char *r1 = s1;
	char *r2 = s2;

	while (1) {
		if (*r1=='\0' && *r2=='\0')
			break;
		if (*r1=='\0' || *r2=='\0')
			return -1;
		if (tolower(*r1)!=tolower(*r2)) {
			return -1;
		}
		r1++;
		r2++;
		i++;
		if (i>1024) return -1;
	}
	
	return 0;
}

#endif

#if PJ_FUNCTIONS_ARE_INLINED==0
#  include <pj/string_i.h>
#endif

#ifdef PJ_WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <objbase.h>
#  include <winsock2.h>
#endif

static char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
		     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

PJ_DEF(pj_str_t*) pj_strltrim( pj_str_t *str )
{
    register char *p = str->ptr;
    while (isspace(*p))
	++p;
    str->slen -= (p - str->ptr);
    str->ptr = p;
    return str;
}

PJ_DEF(pj_str_t*) pj_strrtrim( pj_str_t *str )
{
    char *end = str->ptr + str->slen;
    register char *p = end - 1;
    while (p >= str->ptr && isspace(*p))
        --p;
    str->slen -= ((end - p) - 1);
    return str;
}

PJ_INLINE(void) pj_val_to_hex_digit(unsigned value, char *p)
{
    *p++ = hex[ (value & 0xF0) >> 4 ];
    *p++ = hex[ (value & 0x0F) ];
}

PJ_DEF(pj_str_t*) pj_create_random_string(pj_str_t *str, int len)
{
    int i;
    char *p = str->ptr;

    for (i=0; i<len/8; ++i) {
	unsigned val = pj_rand();
	pj_val_to_hex_digit( (val & 0xFF000000) >> 24, p+0 );
	pj_val_to_hex_digit( (val & 0x00FF0000) >> 16, p+2 );
	pj_val_to_hex_digit( (val & 0x0000FF00) >>  8, p+4 );
	pj_val_to_hex_digit( (val & 0x000000FF) >>  0, p+6 );
	p += 8;
    }
    for (i=i * 8; i<len; ++i) {
	*p++ = hex[ pj_rand() & 0x0F ];
    }
    str->slen = len;
    return str;
}


PJ_DEF(unsigned long) pj_strtoul(const pj_str_t *str)
{
    unsigned long value;
    unsigned i;

    value = 0;
    for (i=0; i<(unsigned)str->slen; ++i) {
	value = value * 10 + (str->ptr[i] - '0');
    }
    return value;
}

PJ_DEF(int) pj_utoa(unsigned val, char *buf)
{
    char *p;
    int len;

    p = buf;
    do {
        unsigned digval = (unsigned) (val % 10);
        val /= 10;
        *p++ = (char) (digval + '0');
    } while (val > 0);

    len = p-buf;
    *p-- = '\0';

    do {
        char temp = *p;
        *p = *buf;
        *buf = temp;
        --p;
        ++buf;
    } while (buf < p);

    return len;
}

PJ_DEF(int) pj_rand()
{
    return rand();
}
