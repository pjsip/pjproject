/* $Header: /pjproject-0.3/pjlib/src/pj/hash.c 8     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/hash.c $
 * 
 * 8     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 7     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#include <pj/hash.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/os.h>

/**
 * The hash multiplier used to calculate hash value.
 */
#define PJ_HASH_MULTIPLIER	33


struct pj_hash_entry
{
    struct pj_hash_entry *next;
    const void *key;
    pj_uint32_t hash;
    pj_uint32_t keylen;
    void *value;
};


struct pj_hash_table_t
{
    pj_hash_entry     **table;
    unsigned		count, rows;
    pj_hash_iterator_t	iterator;
};



PJ_DEF(pj_uint32_t) pj_hash_calc(pj_uint32_t hash, const void *key, unsigned keylen)
{
    PJ_CHECK_STACK();

    if (keylen==PJ_HASH_KEY_STRING) {
	const unsigned char *p = key;
	for ( ; *p; ++p ) {
	    hash = hash * PJ_HASH_MULTIPLIER + *p;
	}
	keylen = p - (const unsigned char*)key;
    } else {
	const unsigned char *p = key,
			    *end = p + keylen;
	for ( ; p!=end; ++p) {
	    hash = hash * PJ_HASH_MULTIPLIER + *p;
	}
    }
    return hash;
}


PJ_DEF(pj_hash_table_t*) pj_hash_create(pj_pool_t *pool, unsigned size)
{
    pj_hash_table_t *h;
    unsigned table_size;
    
    h = pj_pool_alloc(pool, sizeof(pj_hash_table_t));
    h->count = 0;

    PJ_LOG( 5, ("hashtbl", "hash table %p created from pool %s", h, pj_pool_getobjname(pool)));

    /* size must be 2^n - 1.
       round-up the size to this rule, except when size is 2^n, then size
       will be round-down to 2^n-1.
     */
    table_size = 8;
    do {
	table_size <<= 1;    
    } while (table_size <= size);
    table_size -= 1;
    
    h->rows = table_size;
    h->table = pj_pool_calloc(pool, table_size+1, sizeof(pj_hash_entry*));
    return h;
}

static pj_hash_entry **find_entry( pj_pool_t *pool, pj_hash_table_t *ht, 
				   const void *key, unsigned keylen,
				   void *val)
{
    pj_uint32_t hash;
    pj_hash_entry **p_entry, *entry;

    hash=0;
    if (keylen==PJ_HASH_KEY_STRING) {
	const unsigned char *p = key;
	for ( ; *p; ++p ) {
	    hash = hash * PJ_HASH_MULTIPLIER + *p;
	}
	keylen = p - (const unsigned char*)key;
    } else {
	const unsigned char *p = key,
			    *end = p + keylen;
	for ( ; p!=end; ++p) {
	    hash = hash * PJ_HASH_MULTIPLIER + *p;
	}
    }

    /* scan the linked list */
    for (p_entry = &ht->table[hash & ht->rows], entry=*p_entry; 
	 entry; 
	 p_entry = &entry->next, entry = *p_entry)
    {
	if (entry->hash==hash && entry->keylen==keylen &&
	    memcmp(entry->key, key, keylen)==0) 
	{
	    break;	
	}
    }

    if (entry || val==NULL)
	return p_entry;

    /* create a new entry */
    entry = pj_pool_alloc(pool, sizeof(pj_hash_entry));
    PJ_LOG(5, ("hashtbl", "%p: New p_entry %p created, pool used=%u, cap=%u", ht, entry, 
			  pj_pool_get_used_size(pool), pj_pool_get_capacity(pool)));
    entry->next = NULL;
    entry->hash = hash;
    entry->key = key;
    entry->keylen = keylen;
    entry->value = val;
    *p_entry = entry;
    
    ++ht->count;
    
    return p_entry;
}

PJ_DEF(void *) pj_hash_get( pj_hash_table_t *ht,
			    const void *key, unsigned keylen )
{
    pj_hash_entry *entry;
    entry = *find_entry( NULL, ht, key, keylen, NULL);
    return entry ? entry->value : NULL;
}

PJ_DEF(void) pj_hash_set( pj_pool_t *pool, pj_hash_table_t *ht,
			  const void *key, unsigned keylen,
			  void *value )
{
    pj_hash_entry **p_entry;

    p_entry = find_entry( pool, ht, key, keylen, value );
    if (*p_entry) {
	if (value == NULL) {
	    /* delete entry */
	    PJ_LOG(5, ("hashtbl", "%p: p_entry %p deleted", ht, *p_entry));
	    *p_entry = (*p_entry)->next;
	    --ht->count;
	    
	} else {
	    /* overwrite */
	    (*p_entry)->value = value;
	    PJ_LOG(5, ("hashtbl", "%p: p_entry %p value set to %p", ht, *p_entry, value));
	}
    }
}

PJ_DEF(unsigned) pj_hash_count( pj_hash_table_t *ht )
{
    return ht->count;
}

PJ_DEF(pj_hash_iterator_t*) pj_hash_first( pj_hash_table_t *ht,
					   pj_hash_iterator_t *it )
{
    it->index = 0;
    it->entry = NULL;

    for (; it->index < ht->rows; ++it->index) {
	it->entry = ht->table[it->index];
	if (it->entry) {
	    break;
	}
    }

    return it->entry ? it : NULL;
}

PJ_DEF(pj_hash_iterator_t*) pj_hash_next( pj_hash_table_t *ht, 
					  pj_hash_iterator_t *it )
{
    it->entry = it->entry->next;
    if (it->entry) {
	return it;
    }

    for (++it->index; it->index < ht->rows; ++it->index) {
	it->entry = ht->table[it->index];
	if (it->entry) {
	    break;
	}
    }

    return it->entry ? it : NULL;
}

PJ_DEF(void*) pj_hash_this( pj_hash_table_t *ht, pj_hash_iterator_t *it )
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(ht);
    return it->entry->value;
}

#if 0
void pj_hash_dump_collision( pj_hash_table_t *ht )
{
    unsigned min=0xFFFFFFFF, max=0;
    unsigned i;
    char line[120];
    int len, totlen = 0;

    for (i=0; i<ht->rows; ++i) {
	unsigned count = 0;    
	pj_hash_entry *entry = ht->table[i];
	while (entry) {
	    ++count;
	    entry = entry->next;
	}
	if (count < min)
	    min = count;
	if (count > max)
	    max = count;
	len = pj_snprintf( line+totlen, sizeof(line)-totlen, "%3d:%3d ", i, count);
	if (len < 1)
	    break;
	totlen += len;

	if ((i+1) % 10 == 0) {
	    line[totlen] = '\0';
	    PJ_LOG(4,(__FILE__, line));
	}
    }

    PJ_LOG(4,(__FILE__,"Count: %d, min: %d, max: %d\n", ht->count, min, max));
}
#endif


