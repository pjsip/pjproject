/* $Header: /pjproject/pjlib/src/pj++/string.hpp 2     2/24/05 11:23a Bennylp $ */
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
#ifndef __PJPP_STRING_H__
#define __PJPP_STRING_H__

#include <pj/string.h>
#include <pj++/pool.hpp>

class PJ_String : public pj_str_t
{
public:
    PJ_String() 
    { 
	pj_assert(sizeof(PJ_String) == sizeof(pj_str_t));
	ptr=NULL; slen=0; 
    }

    explicit PJ_String(char *str) 
    { 
	set(str);
    }

    PJ_String(PJ_Pool *pool, const char *src)
    {
	set(pool, src);
    }

    explicit PJ_String(pj_str_t *s)
    {
	set(s);
    }

    PJ_String(PJ_Pool *pool, const pj_str_t *s)
    {
	set(pool, s);
    }

    explicit PJ_String(PJ_String &rhs)
    {
	set(rhs);
    }

    PJ_String(PJ_Pool *pool, const PJ_String &rhs)
    {
	set(pool, rhs);
    }

    PJ_String(char *str, pj_size_t len)
    {
	set(str, len);
    }

    PJ_String(char *begin, char *end)
    {
	pj_strset3(this, begin, end);
    }

    pj_size_t length() const
    {
	return pj_strlen(this);
    }

    pj_size_t size() const
    {
	return length();
    }

    const char *buf() const
    {
	return ptr;
    }

    void set(char *str)
    {
	pj_strset2(this, str);
    }

    void set(PJ_Pool *pool, const char *s)
    {
	pj_strdup2(pool->pool_(), this, s);
    }

    void set(pj_str_t *s)
    {
	pj_strassign(this, s);
    }

    void set(PJ_Pool *pool, const pj_str_t *s)
    {
	pj_strdup(pool->pool_(), this, s);
    }

    void set(char *str, pj_size_t len)
    {
	pj_strset(this, str, len);
    }

    void set(char *begin, char *end)
    {
	pj_strset3(this, begin, end);
    }

    void set(PJ_String &rhs)
    {
	pj_strassign(this, &rhs);
    }

    void set(PJ_Pool *pool, const PJ_String *s)
    {
	pj_strdup(pool->pool_(), this, s);
    }

    void set(PJ_Pool *pool, const PJ_String &s)
    {
	pj_strdup(pool->pool_(), this, &s);
    }

    void strcpy(const pj_str_t *s)
    {
	pj_strcpy(this, s);
    }

    void strcpy(const PJ_String &rhs)
    {
	pj_strcpy(this, &rhs);
    }

    void strcpy(const char *s)
    {
	pj_strcpy2(this, s);
    }

    int strcmp(const char *s) const
    {
	return pj_strcmp2(this, s);
    }

    int strcmp(const pj_str_t *s) const
    {
	return pj_strcmp(this, s);
    }

    int strcmp(const PJ_String &rhs) const
    {
	return pj_strcmp(this, &rhs);
    }

    int strncmp(const char *s, pj_size_t len) const
    {
	return pj_strncmp2(this, s, len);
    }

    int strncmp(const pj_str_t *s, pj_size_t len) const
    {
	return pj_strncmp(this, s, len);
    }

    int strncmp(const PJ_String &rhs, pj_size_t len) const
    {
	return pj_strncmp(this, &rhs, len);
    }

    int stricmp(const char *s) const
    {
	return pj_stricmp2(this, s);
    }

    int stricmp(const pj_str_t *s) const
    {
	return pj_stricmp(this, s);
    }

    int stricmp(const PJ_String &rhs) const
    {
	return stricmp(&rhs);
    }

    int strnicmp(const char *s, pj_size_t len) const
    {
	return pj_strnicmp2(this, s, len);
    }

    int strnicmp(const pj_str_t *s, pj_size_t len) const
    {
	return pj_strnicmp(this, s, len);
    }

    int strnicmp(const PJ_String &rhs, pj_size_t len) const
    {
	return strnicmp(&rhs, len);
    }

    bool operator==(const char *s) const
    {
	return strcmp(s) == 0;
    }

    bool operator==(const pj_str_t *s) const
    {
	return strcmp(s) == 0;
    }

    bool operator==(const PJ_String &rhs) const
    {
	return pj_strcmp(this, &rhs) == 0;
    }

    char *strchr(int chr)
    {
	return pj_strchr(this, chr);
    }

    char *find(int chr)
    {
	return strchr(chr);
    }

    void strcat(const PJ_String &rhs)
    {
	pj_strcat(this, &rhs);
    }

    void ltrim()
    {
	pj_strltrim(this);
    }

    void rtrim()
    {
	pj_strrtrim(this);
    }

    void trim()
    {
	pj_strtrim(this);
    }

    unsigned long toul() const
    {
	return pj_strtoul(this);
    }

private:
    //PJ_String(const PJ_String &rhs) {}
    void operator=(const PJ_String &rhs) { pj_assert(false); }
};

#endif	/* __PJPP_STRING_H__ */
