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
 * FIFO buffer or circular buffer.
 */
typedef struct pj_fifobuf_t
{
    /** The start of the buffer */
    char *first;

    /** The end of the buffer */
    char *last;

    /** The start of empty area in the buffer */
    char *ubegin;

    /** The end of empty area in the buffer */
    char *uend;

    /** Full flag when ubegin==uend */
    int full;

} pj_fifobuf_t;


/**
 * Initialize the circular buffer by giving it a buffer and size.
 * 
 * @param fb        The fifobuf/circular buffer
 * @param buffer    Buffer to be used to allocate/free chunks of memory from by
 *                  the circular buffer.
 * @param size      The size of the buffer.
 */
PJ_DECL(void) pj_fifobuf_init(pj_fifobuf_t *fb, void *buffer, unsigned size);

/**
 * Returns the capacity (initial size) of the buffer.
 *
 * @param fb        The fifobuf/circular buffer
 * 
 * @return          Capacity in bytes.
 */
PJ_DECL(unsigned) pj_fifobuf_capacity(pj_fifobuf_t *fb);

/**
 * Returns maximum buffer size that can be allocated from the circular buffer.
 *
 * @param fb        The fifobuf/circular buffer
 *
 * @return          Free size in bytes
 */
PJ_DECL(unsigned) pj_fifobuf_available_size(pj_fifobuf_t *fb);

/**
 * Allocate a buffer from the circular buffer.
 *
 * @param fb        The fifobuf/circular buffer
 * @param size      Size to allocate
 * 
 * @return          Allocated buffer or NULL if the buffer cannot be allocated.
 */
PJ_DECL(void*) pj_fifobuf_alloc(pj_fifobuf_t *fb, unsigned size);

/**
 * Free up space used by the last allocated buffer. For example, if you
 * allocated ptr0, ptr1, and ptr2, this function is used to free ptr2.
 *
 * @param fb        The fifobuf/circular buffer
 * @param buf       The buffer to be freed. This is the pointer returned by 
 *                  pj_fifobuf_alloc()
 * 
 * @return          PJ_SUCCESS or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_fifobuf_unalloc(pj_fifobuf_t *fb, void *buf);

/**
 * Free up space used by the earliest allocated buffer. For example, if you
 * allocated ptr0, ptr1, and ptr2, this function is used to free ptr0.
 *
 * @param fb        The fifobuf/circular buffer
 * @param buf       The buffer to be freed. This is the pointer returned by 
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

