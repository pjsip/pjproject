/* $Id$
 *
 */
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

#ifndef __PJ_FIFOBUF_H__
#define __PJ_FIFOBUF_H__

#include <pj/types.h>

PJ_BEGIN_DECL

typedef struct pj_fifobuf_t pj_fifobuf_t;
struct pj_fifobuf_t
{
    char *first, *last;
    char *ubegin, *uend;
    int full;
};

PJ_DECL(void)	     pj_fifobuf_init (pj_fifobuf_t *fb, void *buffer, unsigned size);
PJ_DECL(unsigned)    pj_fifobuf_max_size (pj_fifobuf_t *fb);
PJ_DECL(void*)	     pj_fifobuf_alloc (pj_fifobuf_t *fb, unsigned size);
PJ_DECL(pj_status_t) pj_fifobuf_unalloc (pj_fifobuf_t *fb, void *buf);
PJ_DECL(pj_status_t) pj_fifobuf_free (pj_fifobuf_t *fb, void *buf);

PJ_END_DECL

#endif	/* __PJ_FIFOBUF_H__ */

