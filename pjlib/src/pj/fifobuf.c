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
#include <pj/fifobuf.h>
#include <pj/errno.h>
#include <pj/log.h>
#include <pj/assert.h>
#include <pj/os.h>
#include <pj/string.h>

#define THIS_FILE   "fifobuf"

/* Size of the per-chunk header, which holds the chunk's total size.
 *
 * It doubles as the allocator's alignment: the payload starts immediately
 * after the header and every chunk occupies a multiple of SZ, so a payload
 * is exactly as aligned as fifobuf->first (which pj_fifobuf_init() aligns).
 * It must be a power of two, at least sizeof(unsigned) so the header fits,
 * and large enough for any object the caller stores in the buffer, hence
 * pointer sized, matching PJ_POOL_ALIGNMENT.
 */
#define SZ  (sizeof(void*) < sizeof(unsigned) ? sizeof(unsigned) \
                                              : sizeof(void*))

/* Round up/down to a multiple of SZ */
#define ALIGN_UP(x)     (((x) + SZ - 1) & ~(SZ - 1))
#define ALIGN_DN(x)     ((x) & ~(SZ - 1))

/* put and get size at arbitrary, possibly unaligned location */
PJ_INLINE(void) put_size(void *ptr, unsigned size)
{
    pj_memcpy(ptr, &size, sizeof(size));
}

PJ_INLINE(unsigned) get_size(const void *ptr)
{
    unsigned size;
    pj_memcpy(&size, ptr, sizeof(size));
    return size;
}

PJ_DEF(void) pj_fifobuf_init (pj_fifobuf_t *fifobuf, void *buffer, unsigned size)
{
    PJ_CHECK_STACK();

    PJ_LOG(6, (THIS_FILE, 
               "fifobuf_init fifobuf=%p buffer=%p, size=%d", 
               fifobuf, buffer, size));

    /* Align the start, so that every chunk payload is aligned too. The
     * padding is taken out of the usable space.
     */
    fifobuf->first = (char*)ALIGN_UP((pj_size_t)buffer);
    fifobuf->last = (char*)buffer + size;
    if (fifobuf->last < fifobuf->first)
        fifobuf->last = fifobuf->first;
    fifobuf->ubegin = fifobuf->uend = fifobuf->first;
    fifobuf->full = (fifobuf->last==fifobuf->first);
}

PJ_DEF(unsigned) pj_fifobuf_capacity (pj_fifobuf_t *fifobuf)
{
    unsigned cap = (unsigned)ALIGN_DN((pj_size_t)(fifobuf->last -
                                                  fifobuf->first));
    return (cap > SZ) ? cap-SZ : 0;
}

PJ_DEF(unsigned) pj_fifobuf_available_size (pj_fifobuf_t *fifobuf)
{
    PJ_CHECK_STACK();

    if (fifobuf->full)
        return 0;

    if (fifobuf->uend >= fifobuf->ubegin) {
        unsigned s;
        unsigned s1 = (unsigned)(fifobuf->last - fifobuf->uend);
        unsigned s2 = (unsigned)(fifobuf->ubegin - fifobuf->first);
        if (s1 <= SZ)
            s = s2;
        else if (s2 <= SZ)
            s = s1;
        else
            s = s1<s2 ? s2 : s1;

        s = (unsigned)ALIGN_DN((pj_size_t)s);
        return (s>SZ) ? s-SZ : 0;
    } else {
        unsigned s = (unsigned)ALIGN_DN((pj_size_t)(fifobuf->ubegin -
                                                    fifobuf->uend));
        return (s>SZ) ? s-SZ : 0;
    }
}

PJ_DEF(void*) pj_fifobuf_alloc (pj_fifobuf_t *fifobuf, unsigned size)
{
    unsigned available;
    char *start;
    unsigned total;

    PJ_CHECK_STACK();

    /* Chunks occupy a multiple of SZ so that the chunk after this one, and
     * hence its payload, stays aligned.
     */
    total = (unsigned)ALIGN_UP((pj_size_t)size + SZ);

    if (fifobuf->full) {
        PJ_LOG(6, (THIS_FILE, 
                   "fifobuf_alloc fifobuf=%p, size=%d: full!", 
                   fifobuf, size));
        return NULL;
    }

    /* try to allocate from the end part of the fifo */
    if (fifobuf->uend >= fifobuf->ubegin) {
        /* If we got here, then first <= ubegin <= uend <= last, and
         * the the buffer layout is like this:
         *
         *      <--free0---> <--- used --> <-free1->
         *     |            |#############|         |
         *     ^            ^             ^         ^
         *   first       ubegin          uend      last
         *
         * where the size of free0, used, and/or free1 may be zero.
         */
        available = (unsigned)(fifobuf->last - fifobuf->uend);
        if (available >= total) {
            char *ptr = fifobuf->uend;
            fifobuf->uend += total;
            if (fifobuf->uend == fifobuf->last)
                fifobuf->uend = fifobuf->first;
            if (fifobuf->uend == fifobuf->ubegin)
                fifobuf->full = 1;
            put_size(ptr, total);
            ptr += SZ;

            PJ_LOG(6, (THIS_FILE, 
                       "fifobuf_alloc fifobuf=%p, size=%d: returning %p, p1=%p, p2=%p",
                       fifobuf, size, ptr, fifobuf->ubegin, fifobuf->uend));
            return ptr;
        }
    }

    /* If we got here, then either there is not enough space in free1 above,
     * or the the buffer layout is like this:
     *
     *     <-used0-> <--free--> <- used1 ->
     *    |#########|          |###########|
     *    ^         ^          ^           ^
     *   first    uend       ubegin       last
     * 
     * where the size of used0, used1, and/or free may be zero.
     */
    start = (fifobuf->uend <= fifobuf->ubegin) ? fifobuf->uend : fifobuf->first;
    available = (unsigned)(fifobuf->ubegin - start);
    if (available >= total) {
        char *ptr = start;
        fifobuf->uend = start + total;
        if (fifobuf->uend == fifobuf->ubegin)
            fifobuf->full = 1;
        put_size(ptr, total);
        ptr += SZ;

        PJ_LOG(6, (THIS_FILE, 
                   "fifobuf_alloc fifobuf=%p, size=%d: returning %p, p1=%p, p2=%p", 
                   fifobuf, size, ptr, fifobuf->ubegin, fifobuf->uend));
        return ptr;
    }

    PJ_LOG(6, (THIS_FILE, 
               "fifobuf_alloc fifobuf=%p, size=%d: no space left! p1=%p, p2=%p", 
               fifobuf, size, fifobuf->ubegin, fifobuf->uend));
    return NULL;
}

PJ_DEF(pj_status_t) pj_fifobuf_free (pj_fifobuf_t *fifobuf, void *buf)
{
    char *ptr = (char*)buf;
    char *end;
    unsigned sz;

    PJ_CHECK_STACK();

    ptr -= SZ;
    if (ptr < fifobuf->first || ptr >= fifobuf->last) {
        pj_assert(!"Invalid pointer to free");
        return PJ_EINVAL;
    }

    if (ptr != fifobuf->ubegin && ptr != fifobuf->first) {
        pj_assert(!"Invalid free() sequence!");
        return PJ_EINVAL;
    }

    end = (fifobuf->uend > fifobuf->ubegin) ? fifobuf->uend : fifobuf->last;
    sz = get_size(ptr);
    if (ptr+sz > end) {
        pj_assert(!"Invalid size!");
        return PJ_EINVAL;
    }

    fifobuf->ubegin = ptr + sz;

    /* Rollover */
    if (fifobuf->ubegin == fifobuf->last)
        fifobuf->ubegin = fifobuf->first;

    /* Reset if fifobuf is empty */
    if (fifobuf->ubegin == fifobuf->uend)
        fifobuf->ubegin = fifobuf->uend = fifobuf->first;

    fifobuf->full = 0;

    PJ_LOG(6, (THIS_FILE, 
               "fifobuf_free fifobuf=%p, ptr=%p, size=%d, p1=%p, p2=%p", 
               fifobuf, buf, sz, fifobuf->ubegin, fifobuf->uend));

    return 0;
}
