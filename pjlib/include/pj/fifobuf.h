/* $Id$
 *
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

