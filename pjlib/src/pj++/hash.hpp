/* $Header: /pjproject/pjlib/src/pj++/hash.hpp 5     8/24/05 10:29a Bennylp $ */
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
#ifndef __PJPP_HASH_H__
#define __PJPP_HASH_H__

#include <pj++/types.hpp>
#include <pj/hash.h>

class PJ_Hash_Table
{
public:
    class iterator
    {
    public:
	iterator() {}
	explicit iterator(pj_hash_table_t *h, pj_hash_iterator_t *i) : ht_(h), it_(i) {}
	iterator(const iterator &rhs) : ht_(rhs.ht_), it_(rhs.it_) {}
	void operator++() { it_ = pj_hash_next(ht_, it_); }
	bool operator==(const iterator &rhs) { return ht_ == rhs.ht_ && it_ == rhs.it_; }
	iterator & operator=(const iterator &rhs) { ht_=rhs.ht_; it_=rhs.it_; return *this; }
    private:
	pj_hash_table_t *ht_;
	pj_hash_iterator_t it_val_;
	pj_hash_iterator_t *it_;

	friend class PJ_Hash_Table;
    };

    static PJ_Hash_Table *create(PJ_Pool *pool, unsigned size)
    {
	return (PJ_Hash_Table*) pj_hash_create(pool->pool_(), size);
    }

    static pj_uint32_t calc(pj_uint32_t initial_hval, const void *key, unsigned keylen)
    {
	return pj_hash_calc(initial_hval, key, keylen);
    }

    pj_hash_table_t *hash_table_()
    {
	return (pj_hash_table_t*)this;
    }

    void *get(const void *key, unsigned keylen)
    {
	return pj_hash_get(this->hash_table_(), key, keylen);
    }

    void set(PJ_Pool *pool, const void *key, unsigned keylen, void *value)
    {
	pj_hash_set(pool->pool_(), this->hash_table_(), key, keylen, value);
    }

    unsigned count()
    {
	return pj_hash_count(this->hash_table_());
    }

    iterator begin()
    {
	iterator it(this->hash_table_(), NULL);
	it.it_ = pj_hash_first(this->hash_table_(), &it.it_val_);
	return it;
    }

    iterator end()
    {
	return iterator(this->hash_table_(), NULL);
    }
};

#endif	/* __PJPP_HASH_H__ */
