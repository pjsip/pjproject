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
#include <pj/hash.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/pool.h>
#include <pj/os.h>
#include <pj/ctype.h>
#include <pj/assert.h>

/**
 * The hash multiplier used to calculate hash value.
 */
#define PJ_HASH_MULTIPLIER      33


struct pj_hash_entry
{
    struct pj_hash_entry *next;
    void *key;
    pj_uint32_t hash;
    pj_uint32_t keylen;
    void *value;
};


struct pj_hash_table_t
{
    pj_hash_entry     **table;
    unsigned            count, rows;
    pj_hash_iterator_t  iterator;
};



/*
 * Keyed table hash (SipHash-2-4) for hash-flooding resistance. Only the hash
 * TABLE bucketing uses this; pj_hash_calc() below stays plain djb2.
 */
#if defined(PJ_HASH_TABLE_USE_SIPHASH) && PJ_HASH_TABLE_USE_SIPHASH!=0 && \
    defined(PJ_HAS_INT64) && PJ_HAS_INT64!=0
#  define HASH_USE_SIPHASH   1
#else
#  define HASH_USE_SIPHASH   0
#endif

#if HASH_USE_SIPHASH
#include <pj/guid.h>
#include <pj/rand.h>

/* Per-process SipHash key, derived once from a mix of entropy sources. */
static pj_uint64_t sip_k0, sip_k1;
static pj_bool_t   sip_key_ready;

#define SIP_U64(hi,lo)  (((pj_uint64_t)(hi) << 32) | (pj_uint64_t)(lo))
#define SIP_ROTL(x,b)   (pj_uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define SIP_ROUND \
    do { \
        v0 += v1; v1 = SIP_ROTL(v1,13); v1 ^= v0; v0 = SIP_ROTL(v0,32); \
        v2 += v3; v3 = SIP_ROTL(v3,16); v3 ^= v2; \
        v0 += v3; v3 = SIP_ROTL(v3,21); v3 ^= v0; \
        v2 += v1; v1 = SIP_ROTL(v1,17); v1 ^= v2; v2 = SIP_ROTL(v2,32); \
    } while (0)

/* SipHash-2-4 keyed hash over the (optionally lowercased) input, returning the
 * full 64-bit result. Kept independent of the process key so it can be checked
 * against canonical SipHash-2-4 test vectors (see pj_hash_test_siphash24()).
 */
static pj_uint64_t siphash24(pj_uint64_t k0, pj_uint64_t k1,
                             const void *data, unsigned len, pj_bool_t lower)
{
    pj_uint64_t v0, v1, v2, v3, m, b;
    const pj_uint8_t *in = (const pj_uint8_t*)data;
    unsigned i, left = len & 7;
    const pj_uint8_t *end = in + (len - left);

    v0 = SIP_U64(0x736f6d65, 0x70736575) ^ k0;
    v1 = SIP_U64(0x646f7261, 0x6e646f6d) ^ k1;
    v2 = SIP_U64(0x6c796765, 0x6e657261) ^ k0;
    v3 = SIP_U64(0x74656462, 0x79746573) ^ k1;

    for (; in != end; in += 8) {
        m = 0;
        for (i = 0; i < 8; ++i) {
            pj_uint8_t c = lower? (pj_uint8_t)pj_tolower(in[i]) : in[i];
            m |= (pj_uint64_t)c << (8*i);
        }
        v3 ^= m; SIP_ROUND; SIP_ROUND; v0 ^= m;
    }

    b = (pj_uint64_t)len << 56;
    for (i = 0; i < left; ++i) {
        pj_uint8_t c = lower? (pj_uint8_t)pj_tolower(in[i]) : in[i];
        b |= (pj_uint64_t)c << (8*i);
    }
    v3 ^= b; SIP_ROUND; SIP_ROUND; v0 ^= b;

    v2 ^= 0xff;
    SIP_ROUND; SIP_ROUND; SIP_ROUND; SIP_ROUND;

    return v0 ^ v1 ^ v2 ^ v3;
}

/* Unit-test hook (prototype lives only in the pjlib-test private header, not in
 * any public header): run the keyed core with an explicit key so tests can
 * verify canonical SipHash-2-4 vectors.
 */
PJ_DECL(pj_uint64_t) pj_hash_test_siphash24(pj_uint64_t k0, pj_uint64_t k1,
                                            const void *data, unsigned len);
PJ_DEF(pj_uint64_t) pj_hash_test_siphash24(pj_uint64_t k0, pj_uint64_t k1,
                                           const void *data, unsigned len)
{
    return siphash24(k0, k1, data, len, PJ_FALSE);
}

/* Fold a buffer of raw entropy bytes into the 16-byte key material. */
static void mix_entropy(pj_uint8_t k[16], unsigned *idx,
                        const void *data, unsigned len)
{
    const pj_uint8_t *p = (const pj_uint8_t*)data;
    unsigned i;
    for (i = 0; i < len; ++i) {
        unsigned j = (*idx) & 15;
        k[j] = (pj_uint8_t)(k[j] * 33 + p[i]);
        ++(*idx);
    }
}

static void init_sip_key(void)
{
    pj_enter_critical_section();
    if (!sip_key_ready) {
        pj_uint8_t k[16];
        char guidbuf[PJ_GUID_MAX_LENGTH];
        pj_str_t guid;
        pj_timestamp ts;
        pj_uint32_t u32;
        void *ptrs[2];
        unsigned n, idx = 0;

        /* Combine whatever entropy is portably available without an OS CSPRNG:
         * fresh GUID(s) (real OS entropy on proper UUID backends), a high-
         * resolution timestamp, the process id, address-space layout (ASLR)
         * via a couple of pointer values, and pj_rand(). No single source is
         * strong on its own, but together they make the per-process key hard
         * to predict remotely even on the weak guid_simple GUID backend; the
         * strongest entropy still comes from an OS UUID backend.
         */
        pj_bzero(k, sizeof(k));

        for (n = 0; n < 2; ++n) {
            guid.ptr = guidbuf;
            pj_generate_unique_string(&guid);
            mix_entropy(k, &idx, guidbuf, (unsigned)guid.slen);
        }
        if (pj_get_timestamp(&ts) == PJ_SUCCESS)
            mix_entropy(k, &idx, &ts, sizeof(ts));
        u32 = pj_getpid();
        mix_entropy(k, &idx, &u32, sizeof(u32));
        ptrs[0] = (void*)k;         /* stack address (ASLR) */
        ptrs[1] = (void*)&sip_k0;   /* data-segment address (ASLR) */
        mix_entropy(k, &idx, ptrs, sizeof(ptrs));
        for (n = 0; n < 4; ++n) {
            u32 = (pj_uint32_t)pj_rand();
            mix_entropy(k, &idx, &u32, sizeof(u32));
        }

        sip_k0 = SIP_U64(((pj_uint32_t)k[0]<<24)|(k[1]<<16)|(k[2]<<8)|k[3],
                         ((pj_uint32_t)k[4]<<24)|(k[5]<<16)|(k[6]<<8)|k[7]);
        sip_k1 = SIP_U64(((pj_uint32_t)k[8]<<24)|(k[9]<<16)|(k[10]<<8)|k[11],
                         ((pj_uint32_t)k[12]<<24)|(k[13]<<16)|(k[14]<<8)|k[15]);
        sip_key_ready = PJ_TRUE;
    }
    pj_leave_critical_section();
}

/* SipHash-2-4 over the (optionally lowercased) key with the process key,
 * folded to 32 bits for the bucket index. */
static pj_uint32_t calc_table_hash(const void *key, unsigned len,
                                   pj_bool_t lower)
{
    pj_uint64_t b;

    if (!sip_key_ready)
        init_sip_key();

    b = siphash24(sip_k0, sip_k1, key, len, lower);
    return (pj_uint32_t)b ^ (pj_uint32_t)(b >> 32);
}
#endif  /* HASH_USE_SIPHASH */


PJ_DEF(pj_uint32_t) pj_hash_calc(pj_uint32_t hash, const void *key,
                                 unsigned keylen)
{
    PJ_CHECK_STACK();

    if (keylen==PJ_HASH_KEY_STRING) {
        const pj_uint8_t *p = (const pj_uint8_t*)key;
        for ( ; *p; ++p ) {
            hash = (hash * PJ_HASH_MULTIPLIER) + *p;
        }
    } else {
        const pj_uint8_t *p = (const pj_uint8_t*)key,
                              *end = p + keylen;
        for ( ; p!=end; ++p) {
            hash = (hash * PJ_HASH_MULTIPLIER) + *p;
        }
    }
    return hash;
}

PJ_DEF(pj_uint32_t) pj_hash_calc_tolower( pj_uint32_t hval,
                                          char *result,
                                          const pj_str_t *key)
{
    long i;

    for (i=0; i<key->slen; ++i) {
        int lower = pj_tolower(key->ptr[i]);
        if (result)
            result[i] = (char)lower;

        hval = hval * PJ_HASH_MULTIPLIER + lower;
    }

    return hval;
}


PJ_DEF(pj_hash_table_t*) pj_hash_create(pj_pool_t *pool, unsigned size)
{
    pj_hash_table_t *h;
    unsigned table_size;
    
    /* Check that PJ_HASH_ENTRY_BUF_SIZE is correct. */
    PJ_ASSERT_RETURN(sizeof(pj_hash_entry)<=PJ_HASH_ENTRY_BUF_SIZE, NULL);

    h = PJ_POOL_ALLOC_T(pool, pj_hash_table_t);
    h->count = 0;

#if HASH_USE_SIPHASH
    /* Ensure the per-process keyed-hash key is initialized before the table
     * is used (safe: pj_init() has set up the critical section by now).
     */
    init_sip_key();
#endif

    PJ_LOG( 6, ("hashtbl", "hash table %p created from pool %s", h, pj_pool_getobjname(pool)));

    /* size must be 2^n - 1.
       round-up the size to this rule, except when size is 2^n, then size
       will be round-down to 2^n-1.
     */
    table_size = 8;
    do {
        table_size <<= 1;
    } while (table_size < size && table_size <= ((unsigned)-1 >> 1));
    /* The upper bound (half the max value of table_size) stops before the
     * left shift would overflow the unsigned table_size, which would loop
     * forever. For an unreasonably large 'size' the table is capped at the
     * largest representable power of two rather than growing unbounded.
     */
    table_size -= 1;
    
    h->rows = table_size;
    h->table = (pj_hash_entry**)
               pj_pool_calloc(pool, table_size+1, sizeof(pj_hash_entry*));
    return h;
}

static pj_hash_entry **find_entry( pj_pool_t *pool, pj_hash_table_t *ht, 
                                   const void *key, unsigned keylen,
                                   void *val, pj_uint32_t *hval,
                                   void *entry_buf, pj_bool_t lower)
{
    pj_uint32_t hash;
    pj_hash_entry **p_entry, *entry;

#if HASH_USE_SIPHASH
    /* Bucketing always uses the keyed table hash. The caller-supplied *hval is
     * a djb2 value (from pj_hash_calc), which is not comparable to the keyed
     * hash, so it is not trusted for bucketing here.
     */
    if (keylen==PJ_HASH_KEY_STRING)
        keylen = (unsigned)pj_ansi_strlen((const char*)key);
    hash = calc_table_hash(key, keylen, lower);

    /* Honor the public hval out-contract: fill a zero (uncomputed) hval with
     * the deterministic djb2 value, matching pj_hash_calc(), so get-then-set
     * patterns keep working. A nonzero value is left untouched: it is a
     * caller-owned value that some code relies on as stable (e.g. pjsip
     * compares dialog tag_hval directly).
     */
    if (hval && *hval == 0) {
        if (lower) {
            pj_str_t k;
            k.ptr = (char*)key;
            k.slen = keylen;
            *hval = pj_hash_calc_tolower(0, NULL, &k);
        } else {
            *hval = pj_hash_calc(0, key, keylen);
        }
    }
#else
    if (hval && *hval != 0) {
        hash = *hval;
        if (keylen==PJ_HASH_KEY_STRING) {
            keylen = (unsigned)pj_ansi_strlen((const char*)key);
        }
    } else {
        /* This slightly differs with pj_hash_calc() because we need
         * to get the keylen when keylen is PJ_HASH_KEY_STRING.
         */
        hash=0;
        if (keylen==PJ_HASH_KEY_STRING) {
            const pj_uint8_t *p = (const pj_uint8_t*)key;
            for ( ; *p; ++p ) {
                if (lower)
                    hash = hash * PJ_HASH_MULTIPLIER + pj_tolower(*p);
                else 
                    hash = hash * PJ_HASH_MULTIPLIER + *p;
            }
            keylen = (unsigned)(p - (const unsigned char*)key);
        } else {
            const pj_uint8_t *p = (const pj_uint8_t*)key,
                                  *end = p + keylen;
            for ( ; p!=end; ++p) {
                if (lower)
                    hash = hash * PJ_HASH_MULTIPLIER + pj_tolower(*p);
                else
                    hash = hash * PJ_HASH_MULTIPLIER + *p;
            }
        }

        /* Report back the computed hash. */
        if (hval)
            *hval = hash;
    }
#endif  /* HASH_USE_SIPHASH */

    /* scan the linked list */
    for (p_entry = &ht->table[hash & ht->rows], entry=*p_entry; 
         entry; 
         p_entry = &entry->next, entry = *p_entry)
    {
        if (entry->hash==hash && entry->keylen==keylen &&
            ((lower && pj_ansi_strnicmp((const char*)entry->key,
                                        (const char*)key, keylen)==0) ||
             (!lower && pj_memcmp(entry->key, key, keylen)==0)))
        {
            break;
        }
    }

    if (entry || val==NULL)
        return p_entry;

    /* Entry not found, create a new one. 
     * If entry_buf is specified, use it. Otherwise allocate from pool.
     */
    if (entry_buf) {
        entry = (pj_hash_entry*)entry_buf;
    } else {
        /* Pool must be specified! */
        PJ_ASSERT_RETURN(pool != NULL, NULL);

        entry = PJ_POOL_ALLOC_T(pool, pj_hash_entry);
        PJ_LOG(6, ("hashtbl", 
                   "%p: New p_entry %p created, pool used=%lu, cap=%lu", 
                   ht, entry,  (unsigned long)pj_pool_get_used_size(pool),
                   (unsigned long)pj_pool_get_capacity(pool)));
    }
    entry->next = NULL;
    entry->hash = hash;
    if (pool) {
        entry->key = pj_pool_alloc(pool, keylen);
        pj_memcpy(entry->key, key, keylen);
    } else {
        entry->key = (void*)key;
    }
    entry->keylen = keylen;
    entry->value = val;
    *p_entry = entry;
    
    ++ht->count;
    
    return p_entry;
}

PJ_DEF(void *) pj_hash_get( pj_hash_table_t *ht,
                            const void *key, unsigned keylen,
                            pj_uint32_t *hval)
{
    pj_hash_entry *entry;
    entry = *find_entry( NULL, ht, key, keylen, NULL, hval, NULL, PJ_FALSE);
    return entry ? entry->value : NULL;
}

PJ_DEF(void *) pj_hash_get_lower( pj_hash_table_t *ht,
                                  const void *key, unsigned keylen,
                                  pj_uint32_t *hval)
{
    pj_hash_entry *entry;
    entry = *find_entry( NULL, ht, key, keylen, NULL, hval, NULL, PJ_TRUE);
    return entry ? entry->value : NULL;
}

static void hash_set( pj_pool_t *pool, pj_hash_table_t *ht,
                      const void *key, unsigned keylen, pj_uint32_t hval,
                      void *value, void *entry_buf, pj_bool_t lower )
{
    pj_hash_entry **p_entry;

    p_entry = find_entry( pool, ht, key, keylen, value, &hval, entry_buf,
                          lower);
    if (*p_entry) {
        if (value == NULL) {
            /* delete entry */
            PJ_LOG(6, ("hashtbl", "%p: p_entry %p deleted", ht, *p_entry));
            *p_entry = (*p_entry)->next;
            --ht->count;
            
        } else {
            /* overwrite */
            (*p_entry)->value = value;
            PJ_LOG(6, ("hashtbl", "%p: p_entry %p value set to %p", ht, 
                       *p_entry, value));
        }
    }
}

PJ_DEF(void) pj_hash_set( pj_pool_t *pool, pj_hash_table_t *ht,
                          const void *key, unsigned keylen, pj_uint32_t hval,
                          void *value )
{
    hash_set(pool, ht, key, keylen, hval, value, NULL, PJ_FALSE);
}

PJ_DEF(void) pj_hash_set_lower( pj_pool_t *pool, pj_hash_table_t *ht,
                                const void *key, unsigned keylen,
                                pj_uint32_t hval, void *value )
{
    hash_set(pool, ht, key, keylen, hval, value, NULL, PJ_TRUE);
}

PJ_DEF(void) pj_hash_set_np( pj_hash_table_t *ht,
                             const void *key, unsigned keylen, 
                             pj_uint32_t hval, pj_hash_entry_buf entry_buf, 
                             void *value)
{
    hash_set(NULL, ht, key, keylen, hval, value, (void *)entry_buf, PJ_FALSE);
}

PJ_DEF(void) pj_hash_set_np_lower( pj_hash_table_t *ht,
                                   const void *key, unsigned keylen,
                                   pj_uint32_t hval,
                                   pj_hash_entry_buf entry_buf,
                                   void *value)
{
    hash_set(NULL, ht, key, keylen, hval, value, (void *)entry_buf, PJ_TRUE);
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

    for (; it->index <= ht->rows; ++it->index) {
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

    for (++it->index; it->index <= ht->rows; ++it->index) {
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

    for (i=0; i<=ht->rows; ++i) {
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


