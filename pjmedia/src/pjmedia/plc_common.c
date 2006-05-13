/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/plc.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>


static void* plc_replay_create(pj_pool_t*, unsigned c, unsigned f);
static void  plc_replay_save(void*, pj_int16_t*);
static void  plc_replay_generate(void*, pj_int16_t*);

extern void* pjmedia_plc_g711_create(pj_pool_t*, unsigned c, unsigned f);
extern void  pjmedia_plc_g711_save(void*, pj_int16_t*);
extern void  pjmedia_plc_g711_generate(void*, pj_int16_t*);


/**
 * This struct is used internally to represent a PLC backend.
 */
struct plc_alg
{
    void* (*plc_create)(pj_pool_t*, unsigned c, unsigned f);
    void  (*plc_save)(void*, pj_int16_t*);
    void  (*plc_generate)(void*, pj_int16_t*);
};


#if defined(PJMEDIA_HAS_G711_PLC) && PJMEDIA_HAS_G711_PLC!=0
static struct plc_alg plc_g711 =
{
    &pjmedia_plc_g711_create,
    &pjmedia_plc_g711_save,
    &pjmedia_plc_g711_generate
};
#endif


static struct plc_alg plc_replay =
{
    &plc_replay_create,
    &plc_replay_save,
    &plc_replay_generate
};


struct pjmedia_plc
{
    void	    *obj;
    struct plc_alg  *op;
};


/*
 * Create PLC session. This function will select the PLC algorithm to
 * use based on the arguments.
 */
PJ_DEF(pj_status_t) pjmedia_plc_create( pj_pool_t *pool,
					unsigned clock_rate,
					unsigned samples_per_frame,
					unsigned options,
					pjmedia_plc **p_plc)
{
    pjmedia_plc *plc;

    PJ_ASSERT_RETURN(pool && clock_rate && samples_per_frame && p_plc,
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);

    PJ_UNUSED_ARG(options);

    plc = pj_pool_zalloc(pool, sizeof(pjmedia_plc));

    if (0)
	;
#if defined(PJMEDIA_HAS_G711_PLC) && PJMEDIA_HAS_G711_PLC!=0
    else if (clock_rate == 8000)
	plc->op = &plc_g711;
#endif
    else
	plc->op = &plc_replay;

    plc->obj = plc->op->plc_create(pool, clock_rate, samples_per_frame);

    *p_plc = plc;

    return PJ_SUCCESS;
}


/*
 * Save a good frame to PLC.
 */
PJ_DEF(pj_status_t) pjmedia_plc_save( pjmedia_plc *plc,
				      pj_int16_t *frame )
{
    PJ_ASSERT_RETURN(plc && frame, PJ_EINVAL);
 
    plc->op->plc_save(plc->obj, frame);
    return PJ_SUCCESS;
}


/*
 * Generate a replacement for lost frame.
 */
PJ_DEF(pj_status_t) pjmedia_plc_generate( pjmedia_plc *plc,
					  pj_int16_t *frame )
{
    PJ_ASSERT_RETURN(plc && frame, PJ_EINVAL);
    
    plc->op->plc_generate(plc->obj, frame);
    return PJ_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
/*
 * Simple replay based plc
 */
struct replay_plc
{
    unsigned	size;
    unsigned	replay_cnt;
    pj_int16_t *frame;
};


static void* plc_replay_create(pj_pool_t *pool, unsigned clock_rate, 
			       unsigned samples_per_frame)
{
    struct replay_plc *o;

    PJ_UNUSED_ARG(clock_rate);

    o = pj_pool_alloc(pool, sizeof(struct replay_plc));
    o->size = samples_per_frame * 2;
    o->replay_cnt = 0;
    o->frame = pj_pool_zalloc(pool, o->size);

    return o;
}

static void plc_replay_save(void *plc, pj_int16_t *frame)
{
    struct replay_plc *o = plc;

    pj_memcpy(o->frame, frame, o->size);
    o->replay_cnt = 0;
}

static void plc_replay_generate(void *plc, pj_int16_t *frame)
{
    struct replay_plc *o = plc;
    unsigned i, count;
    pj_int16_t *samp;

    ++o->replay_cnt;

    if (o->replay_cnt < 16) {
	pj_memcpy(frame, o->frame, o->size);
    

	count = o->size / 2;
	samp = o->frame;
	for (i=0; i<count; ++i)
	    samp[i] = (pj_int16_t)(samp[i] >> 1);
    } else {
	pj_memset(frame, 0, o->size);
    }
}



