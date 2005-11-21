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

PJ_IDEF(pj_str_t) pj_str(char *str)
{
    pj_str_t dst;
    dst.ptr = str;
    dst.slen = str ? pj_native_strlen(str) : 0;
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strdup(pj_pool_t *pool,
			      pj_str_t *dst,
			      const pj_str_t *src)
{
    if (src->slen) {
	dst->ptr = (char*)pj_pool_alloc(pool, src->slen);
	pj_memcpy(dst->ptr, src->ptr, src->slen);
    }
    dst->slen = src->slen;
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strdup_with_null( pj_pool_t *pool,
					pj_str_t *dst,
					const pj_str_t *src)
{
    if (src->slen) {
	dst->ptr = (char*)pj_pool_alloc(pool, src->slen+1);
	pj_memcpy(dst->ptr, src->ptr, src->slen);
    } else {
	dst->ptr = (char*)pj_pool_alloc(pool, 1);
    }
    dst->slen = src->slen;
    dst->ptr[dst->slen] = '\0';
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strdup2(pj_pool_t *pool,
			      pj_str_t *dst,
			      const char *src)
{
    dst->slen = src ? pj_native_strlen(src) : 0;
    if (dst->slen) {
	dst->ptr = (char*)pj_pool_alloc(pool, dst->slen);
	pj_memcpy(dst->ptr, src, dst->slen);
    } else {
	dst->ptr = NULL;
    }
    return dst;
}


PJ_IDEF(pj_str_t) pj_strdup3(pj_pool_t *pool, const char *src)
{
    pj_str_t temp;
    pj_strdup2(pool, &temp, src);
    return temp;
}

PJ_IDEF(pj_str_t*) pj_strassign( pj_str_t *dst, pj_str_t *src )
{
    dst->ptr = src->ptr;
    dst->slen = src->slen;
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strcpy(pj_str_t *dst, const pj_str_t *src)
{
    dst->slen = src->slen;
    if (src->slen > 0)
	pj_memcpy(dst->ptr, src->ptr, src->slen);
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strcpy2(pj_str_t *dst, const char *src)
{
    dst->slen = src ? pj_native_strlen(src) : 0;
    if (dst->slen > 0)
	pj_memcpy(dst->ptr, src, dst->slen);
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strncpy( pj_str_t *dst, const pj_str_t *src, 
			       pj_ssize_t max)
{
    if (max > src->slen) max = src->slen;
    pj_memcpy(dst->ptr, src->ptr, max);
    dst->slen = max;
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strncpy_with_null( pj_str_t *dst, const pj_str_t *src,
					 pj_ssize_t max)
{
    if (max <= src->slen)
	max = max-1;
    else
	max = src->slen;

    pj_memcpy(dst->ptr, src->ptr, max);
    dst->ptr[max] = '\0';
    dst->slen = max;
    return dst;
}


PJ_IDEF(int) pj_strcmp( const pj_str_t *str1, const pj_str_t *str2)
{
    pj_ssize_t diff;

    diff = str1->slen - str2->slen;
    if (diff) {
	return (int)diff;
    } else if (str1->ptr && str1->slen) {
	return pj_native_strncmp(str1->ptr, str2->ptr, str1->slen);
    } else {
	return 0;
    }
}

PJ_IDEF(int) pj_strncmp( const pj_str_t *str1, const pj_str_t *str2, 
			 pj_size_t len)
{
    return (str1->ptr && str2->ptr) ? 
	    pj_native_strncmp(str1->ptr, str2->ptr, len) :
	    (str1->ptr == str2->ptr ? 0 : 1);
}

PJ_IDEF(int) pj_strncmp2( const pj_str_t *str1, const char *str2, 
			  pj_size_t len)
{
    return (str1->ptr && str2) ? pj_native_strncmp(str1->ptr, str2, len) :
	   (str1->ptr==str2 ? 0 : 1);
}

PJ_IDEF(int) pj_strcmp2( const pj_str_t *str1, const char *str2 )
{
    return pj_strncmp2( str1, str2, str1->slen);
}

PJ_IDEF(int) pj_stricmp( const pj_str_t *str1, const pj_str_t *str2)
{
    pj_ssize_t diff;

    diff = str1->slen - str2->slen;
    if (diff) {
	return (int)diff;
    } else {
	return pj_native_strnicmp(str1->ptr, str2->ptr, str1->slen);
    }
}

PJ_IDEF(int) pj_stricmp2( const pj_str_t *str1, const char *str2)
{
    return (str1->ptr && str2) ? 
	    pj_native_strnicmp(str1->ptr, str2, str1->slen) :
	    (str1->ptr==str2 ? 0 : 1);
}

PJ_IDEF(int) pj_strnicmp( const pj_str_t *str1, const pj_str_t *str2, 
			  pj_size_t len)
{
    return (str1->ptr && str2->ptr) ? 
	    pj_native_strnicmp(str1->ptr, str2->ptr, len) :
	    (str1->ptr == str2->ptr ? 0 : 1);
}

PJ_IDEF(int) pj_strnicmp2( const pj_str_t *str1, const char *str2, 
			   pj_size_t len)
{
    return (str1->ptr && str2) ? 
	    pj_native_strnicmp(str1->ptr, str2, len) :
	    (str1->ptr == str2 ? 0 : 1);
}

PJ_IDEF(void) pj_strcat(pj_str_t *dst, const pj_str_t *src)
{
    if (src->slen) {
	pj_memcpy(dst->ptr + dst->slen, src->ptr, src->slen);
	dst->slen += src->slen;
    }
}

PJ_IDEF(pj_str_t*) pj_strtrim( pj_str_t *str )
{
    pj_strltrim(str);
    pj_strrtrim(str);
    return str;
}

