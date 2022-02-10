/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/ctype.h>
#include <pj/rand.h>
#include <pj/os.h>
#include <pj/errno.h>
#include <pj/limits.h>

#if PJ_FUNCTIONS_ARE_INLINED==0
#  include <pj/string_i.h>
#endif


PJ_DEF(pj_ssize_t) pj_strspn(const pj_str_t *str, const pj_str_t *set_char)
{
    pj_ssize_t i, j, count = 0;
    for (i = 0; i < str->slen; i++) {
	if (count != i) 
	    break;

	for (j = 0; j < set_char->slen; j++) {
	    if (str->ptr[i] == set_char->ptr[j])
		count++;
	}
    }
    return count;
}


PJ_DEF(pj_ssize_t) pj_strspn2(const pj_str_t *str, const char *set_char)
{
    pj_ssize_t i, j, count = 0;
    for (i = 0; i < str->slen; i++) {
	if (count != i)
	    break;

	for (j = 0; set_char[j] != 0; j++) {
	    if (str->ptr[i] == set_char[j])
		count++;
	}
    }
    return count;
}


PJ_DEF(pj_ssize_t) pj_strcspn(const pj_str_t *str, const pj_str_t *set_char)
{
    pj_ssize_t i, j;
    for (i = 0; i < str->slen; i++) {
	for (j = 0; j < set_char->slen; j++) {
	    if (str->ptr[i] == set_char->ptr[j])
		return i;
	}
    }
    return i;
}


PJ_DEF(pj_ssize_t) pj_strcspn2(const pj_str_t *str, const char *set_char)
{
    pj_ssize_t i, j;
    for (i = 0; i < str->slen; i++) {
	for (j = 0; set_char[j] != 0; j++) {
	    if (str->ptr[i] == set_char[j])
		return i;
	}
    }
    return i;
}


PJ_DEF(pj_ssize_t) pj_strtok(const pj_str_t *str, const pj_str_t *delim,
			     pj_str_t *tok, pj_size_t start_idx)
{    
    pj_ssize_t str_idx;

    pj_assert(str->slen >= 0);
    pj_assert(delim->slen >= 0);

    tok->slen = 0;
    if ((str->slen <= 0) || ((pj_size_t)str->slen < start_idx)) {
	return str->slen;
    }
    
    tok->ptr = str->ptr + start_idx;
    tok->slen = str->slen - start_idx;

    str_idx = pj_strspn(tok, delim);
    if (start_idx+str_idx == (pj_size_t)str->slen) {
	return str->slen;
    }    
    tok->ptr += str_idx;
    tok->slen -= str_idx;

    tok->slen = pj_strcspn(tok, delim);
    return start_idx + str_idx;
}


PJ_DEF(pj_ssize_t) pj_strtok2(const pj_str_t *str, const char *delim,
			       pj_str_t *tok, pj_size_t start_idx)
{
    pj_ssize_t str_idx;

    pj_assert(str->slen >= 0);

    tok->slen = 0;
    if ((str->slen <= 0) || ((pj_size_t)str->slen < start_idx)) {
	return str->slen;
    }

    tok->ptr = str->ptr + start_idx;
    tok->slen = str->slen - start_idx;

    str_idx = pj_strspn2(tok, delim);
    if (start_idx + str_idx == (pj_size_t)str->slen) {
	return str->slen;
    }
    tok->ptr += str_idx;
    tok->slen -= str_idx;

    tok->slen = pj_strcspn2(tok, delim);
    return start_idx + str_idx;
}


PJ_DEF(char*) pj_strstr(const pj_str_t *str, const pj_str_t *substr)
{
    const char *s, *ends;

    PJ_ASSERT_RETURN(str->slen >= 0 && substr->slen >= 0, NULL);

    /* Check if the string is empty */
    if (str->slen <= 0)
    	return NULL;

    /* Special case when substr is empty */
    if (substr->slen <= 0) {
	return (char*)str->ptr;
    }

    s = str->ptr;
    ends = str->ptr + str->slen - substr->slen;
    for (; s<=ends; ++s) {
	if (pj_ansi_strncmp(s, substr->ptr, substr->slen)==0)
	    return (char*)s;
    }
    return NULL;
}


PJ_DEF(char*) pj_stristr(const pj_str_t *str, const pj_str_t *substr)
{
    const char *s, *ends;

    PJ_ASSERT_RETURN(str->slen >= 0 && substr->slen >= 0, NULL);

    /* Check if the string is empty */
    if (str->slen <= 0)
    	return NULL;

    /* Special case when substr is empty */
    if (substr->slen == 0) {
	return (char*)str->ptr;
    }

    s = str->ptr;
    ends = str->ptr + str->slen - substr->slen;
    for (; s<=ends; ++s) {
	if (pj_ansi_strnicmp(s, substr->ptr, substr->slen)==0)
	    return (char*)s;
    }
    return NULL;
}


PJ_DEF(pj_str_t*) pj_strltrim( pj_str_t *str )
{
    char *end = str->ptr + str->slen;
    register char *p = str->ptr;
 
    pj_assert(str->slen >= 0);
 
    while (p < end && pj_isspace(*p))
	++p;
    str->slen -= (p - str->ptr);
    str->ptr = p;
    return str;
}

PJ_DEF(pj_str_t*) pj_strrtrim( pj_str_t *str )
{
    char *end = str->ptr + str->slen;
    register char *p = end - 1;

    pj_assert(str->slen >= 0);

    while (p >= str->ptr && pj_isspace(*p))
        --p;
    str->slen -= ((end - p) - 1);
    return str;
}

PJ_DEF(char*) pj_create_random_string(char *str, pj_size_t len)
{
    unsigned i;
    char *p = str;

    PJ_CHECK_STACK();

    for (i=0; i<len/8; ++i) {
	pj_uint32_t val = pj_rand();
	pj_val_to_hex_digit( (val & 0xFF000000) >> 24, p+0 );
	pj_val_to_hex_digit( (val & 0x00FF0000) >> 16, p+2 );
	pj_val_to_hex_digit( (val & 0x0000FF00) >>  8, p+4 );
	pj_val_to_hex_digit( (val & 0x000000FF) >>  0, p+6 );
	p += 8;
    }
    for (i=i * 8; i<len; ++i) {
	*p++ = pj_hex_digits[ pj_rand() & 0x0F ];
    }
    return str;
}

PJ_DEF(long) pj_strtol(const pj_str_t *str)
{
    PJ_CHECK_STACK();

    if (str->slen > 0 && (str->ptr[0] == '+' || str->ptr[0] == '-')) {
        pj_str_t s;
        
        s.ptr = str->ptr + 1;
        s.slen = str->slen - 1;
        return (str->ptr[0] == '-'? -(long)pj_strtoul(&s) : pj_strtoul(&s));
    } else
        return pj_strtoul(str);
}


PJ_DEF(pj_status_t) pj_strtol2(const pj_str_t *str, long *value)
{
    pj_str_t s;
    unsigned long retval = 0;
    pj_bool_t is_negative = PJ_FALSE;
    int rc = 0;

    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(str->slen >= 0, PJ_EINVAL);

    if (!str || !value) {
        return PJ_EINVAL;
    }

    s = *str;
    pj_strltrim(&s);

    if (s.slen == 0)
        return PJ_EINVAL;

    if (s.ptr[0] == '+' || s.ptr[0] == '-') {
        is_negative = (s.ptr[0] == '-');
        s.ptr += 1;
        s.slen -= 1;
    }

    rc = pj_strtoul3(&s, &retval, 10);
    if (rc == PJ_EINVAL) {
        return rc;
    } else if (rc != PJ_SUCCESS) {
        *value = is_negative ? PJ_MINLONG : PJ_MAXLONG;
        return is_negative ? PJ_ETOOSMALL : PJ_ETOOBIG;
    }

    if (retval > PJ_MAXLONG && !is_negative) {
        *value = PJ_MAXLONG;
        return PJ_ETOOBIG;
    }

    if (retval > (PJ_MAXLONG + 1UL) && is_negative) {
        *value = PJ_MINLONG;
        return PJ_ETOOSMALL;
    }

    *value = is_negative ? -(long)retval : retval;

    return PJ_SUCCESS;
}

PJ_DEF(unsigned long) pj_strtoul(const pj_str_t *str)
{
    unsigned long value;
    unsigned i;

    PJ_CHECK_STACK();

    pj_assert(str->slen >= 0);

    value = 0;
    for (i=0; i<(unsigned)str->slen; ++i) {
	if (!pj_isdigit(str->ptr[i]))
	    break;
	value = value * 10 + (str->ptr[i] - '0');
    }
    return value;
}

PJ_DEF(unsigned long) pj_strtoul2(const pj_str_t *str, pj_str_t *endptr,
				  unsigned base)
{
    unsigned long value;
    unsigned i;

    PJ_CHECK_STACK();

    pj_assert(str->slen >= 0);

    value = 0;
    if (base <= 10) {
	for (i=0; i<(unsigned)str->slen; ++i) {
	    unsigned c = (str->ptr[i] - '0');
	    if (c >= base)
		break;
	    value = value * base + c;
	}
    } else if (base == 16) {
	for (i=0; i<(unsigned)str->slen; ++i) {
	    if (!pj_isxdigit(str->ptr[i]))
		break;
	    value = value * 16 + pj_hex_digit_to_val(str->ptr[i]);
	}
    } else {
	pj_assert(!"Unsupported base");
	i = 0;
	value = 0xFFFFFFFFUL;
    }

    if (endptr) {
	endptr->ptr = str->ptr + i;
	endptr->slen = (str->slen < 0)? 0: (str->slen - i);
    }

    return value;
}

PJ_DEF(pj_status_t) pj_strtoul3(const pj_str_t *str, unsigned long *value,
				unsigned base)
{
    pj_str_t s;
    unsigned i;

    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(str->slen >= 0, PJ_EINVAL);

    if (!str || !value) {
        return PJ_EINVAL;
    }

    s = *str;
    pj_strltrim(&s);

    if (s.slen == 0 || s.ptr[0] < '0' ||
	(base <= 10 && (unsigned)s.ptr[0] > ('0' - 1) + base) ||
	(base == 16 && !pj_isxdigit(s.ptr[0])))
    {
        return PJ_EINVAL;
    }

    *value = 0;
    if (base <= 10) {
	for (i=0; i<(unsigned)s.slen; ++i) {
	    unsigned c = s.ptr[i] - '0';
	    if (s.ptr[i] < '0' || (unsigned)s.ptr[i] > ('0' - 1) + base) {
		break;
	    }
	    if (*value > PJ_MAXULONG / base) {
		*value = PJ_MAXULONG;
		return PJ_ETOOBIG;
	    }

	    *value *= base;
	    if ((PJ_MAXULONG - *value) < c) {
		*value = PJ_MAXULONG;
		return PJ_ETOOBIG;
	    }
	    *value += c;
	}
    } else if (base == 16) {
	for (i=0; i<(unsigned)s.slen; ++i) {
	    unsigned c = pj_hex_digit_to_val(s.ptr[i]);
	    if (!pj_isxdigit(s.ptr[i]))
		break;

	    if (*value > PJ_MAXULONG / base) {
		*value = PJ_MAXULONG;
		return PJ_ETOOBIG;
	    }
	    *value *= base;
	    if ((PJ_MAXULONG - *value) < c) {
		*value = PJ_MAXULONG;
		return PJ_ETOOBIG;
	    }
	    *value += c;
	}
    } else {
	pj_assert(!"Unsupported base");
	return PJ_EINVAL;
    }
    return PJ_SUCCESS;
}

PJ_DEF(float) pj_strtof(const pj_str_t *str)
{
    pj_str_t part;
    char *pdot;
    float val;

    pj_assert(str->slen >= 0);

    if (str->slen <= 0)
	return 0;

    pdot = (char*)pj_memchr(str->ptr, '.', str->slen);
    part.ptr = str->ptr;
    part.slen = pdot ? pdot - str->ptr : str->slen;

    if (part.slen)
	val = (float)pj_strtol(&part);
    else
	val = 0;

    if (pdot) {
	part.ptr = pdot + 1;
	part.slen = (str->ptr + str->slen - pdot - 1);
	if (part.slen) {
	    pj_str_t endptr;
	    float fpart, fdiv;
	    int i;
	    fpart = (float)pj_strtoul2(&part, &endptr, 10);
	    fdiv = 1.0;
	    for (i=0; i<(part.slen - endptr.slen); ++i)
		    fdiv = fdiv * 10;
	    if (val >= 0)
		val += (fpart / fdiv);
	    else
		val -= (fpart / fdiv);
	}
    }
    return val;
}

PJ_DEF(int) pj_utoa(unsigned long val, char *buf)
{
    return pj_utoa_pad(val, buf, 0, 0);
}

PJ_DEF(int) pj_utoa_pad( unsigned long val, char *buf, int min_dig, int pad)
{
    char *p;
    int len;

    PJ_CHECK_STACK();

    p = buf;
    do {
        unsigned long digval = (unsigned long) (val % 10);
        val /= 10;
        *p++ = (char) (digval + '0');
    } while (val > 0);

    len = (int)(p-buf);
    while (len < min_dig) {
	*p++ = (char)pad;
	++len;
    }
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
