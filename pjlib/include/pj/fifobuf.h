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
#ifndef __PJ_FIFOBUF_H__
#define __PJ_FIFOBUF_H__

/**
 * @file fifobuf.h
 * @brief Circular buffer
 */
/**
 * @defgroup PJ_FIFOBUF Circular buffer
 * @ingroup PJ_DS
 * @{
 */

#include <pj/types.h>

PJ_BEGIN_DECL


/**
 * A FIFO buffer provides chunks of memory to the application with first in
 * first out policy (or more correctly, first out first in). The fifobuf is
 * created by providing it with a fixed buffer. After that, application may
 * request chunks of memory from this buffer. When the app is done with a
 * chunk of memory, it must return that chunk back to the fifobuf, with the
 * requirement that the oldest allocated chunk must be returned first.
 */
typedef struct pj_fifobuf_t
{
    /** The start of the buffer */
    char *first;

    /** The end of the buffer */
    char *last;

    /** The start of used area in the buffer */
    char *ubegin;

    /** The end of used area in the buffer */
    char *uend;

    /** Full flag when ubegin==uend */
    int full;

} pj_fifobuf_t;


/**
 * Initialize the fifobuf by giving it a buffer and size.
 * 
 * @param fb        The fifobuf
 * @param buffer    Buffer to be used to allocate/free chunks of memory from by
 *                  the fifo buffer.
 * @param size      The size of the buffer.
 */
PJ_DECL(void) pj_fifobuf_init(pj_fifobuf_t *fb, void *buffer, unsigned size);

/**
 * Returns the capacity (initial size) of the buffer.
 *
 * @param fb        The fifobuf
 * 
 * @return          Capacity in bytes.
 */
PJ_DECL(unsigned) pj_fifobuf_capacity(pj_fifobuf_t *fb);

/**
 * Returns maximum size of memory chunk that can be allocated from the buffer.
 *
 * @param fb        The fifobuf
 *
 * @return          Size in bytes
 */
PJ_DECL(unsigned) pj_fifobuf_available_size(pj_fifobuf_t *fb);

/**
 * Allocate a chunk of memory from the fifobuf.
 *
 * @param fb        The fifobuf
 * @param size      Size to allocate
 * 
 * @return          Allocated buffer or NULL if the buffer cannot be allocated
 */
PJ_DECL(void*) pj_fifobuf_alloc(pj_fifobuf_t *fb, unsigned size);

/**
 * Return the space used by the earliest allocated memory chunk back to the
 * fifobuf. For example, if app previously allocated ptr0, ptr1, and ptr2
 * (in that order), then pj_fifobuf_free() can only be called with ptr0 as
 * parameter. Subsequent pj_fifobuf_free() must be called with ptr1, and
 * the next one with ptr2, and so on.
 * 
 * @param fb        The fifobuf
 * @param buf       Pointer to memory chunk previously returned by
 *                  pj_fifobuf_alloc()
 * 
 * @return          PJ_SUCCESS or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_fifobuf_free(pj_fifobuf_t *fb, void *buf);


PJ_END_DECL

/**
 * @}   // PJ_FIFOBUF group
 */

#endif  /* __PJ_FIFOBUF_H__ */

