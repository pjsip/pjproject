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
#include <pjmedia/mediamgr.h>
#include <pj/sock.h>
#include <pj/pool.h>
#include <pj/string.h>

PJ_DECL(pj_status_t) g711_init_factory (pj_codec_factory *factory, pj_pool_t *pool);
PJ_DECL(pj_status_t) g711_deinit_factory (pj_codec_factory *factory);

/** Concrete declaration of media manager. */
struct pj_med_mgr_t
{
    /** Pool. */
    pj_pool_t		 *pool;

    /** Pool factory. */
    pj_pool_factory	 *pf;

    /** Codec manager. */
    pj_codec_mgr	  codec_mgr;
};

/**
 * Initialize and get the instance of media manager.
 */
PJ_DEF(pj_med_mgr_t*) pj_med_mgr_create ( pj_pool_factory *pf)
{
    pj_pool_t *pool;
    pj_med_mgr_t *mm;
    pj_codec_factory *cf;
    pj_status_t status;

    pool = pj_pool_create(pf, "mediamgr", 512, 512, NULL);
    if (!pool)
	return NULL;

    mm = pj_pool_calloc(pool, 1, sizeof(struct pj_med_mgr_t));
    mm->pool = pool;
    mm->pf = pf;

    /* Sound */
    pj_snd_init(pf);

    /* Init codec manager. */
    status = pj_codec_mgr_init(&mm->codec_mgr);
    if (status != 0) {
	pj_snd_deinit();
	goto on_error;
    }

    /* Init and register G.711 codec. */
    cf = pj_pool_alloc (mm->pool, sizeof(pj_codec_factory));

    status = g711_init_factory (cf, mm->pool);
    if (status != 0) {
	pj_snd_deinit();
	return NULL;
    }

    status = pj_codec_mgr_register_factory (&mm->codec_mgr, cf);
    if (status != 0) 
	return NULL;

    return mm;

on_error:
    pj_pool_release(pool);
    return NULL;
}

/**
 * Get the codec manager instance.
 */
PJ_DEF(pj_codec_mgr*) pj_med_mgr_get_codec_mgr (pj_med_mgr_t *mgr)
{
    return &mgr->codec_mgr;
}

/**
 * Deinitialize media manager.
 */
PJ_DEF(pj_status_t) pj_med_mgr_destroy (pj_med_mgr_t *mgr)
{
    pj_snd_deinit();
    pj_pool_release (mgr->pool);
    return 0;
}

/**
 * Get pool factory.
 */
PJ_DEF(pj_pool_factory*) pj_med_mgr_get_pool_factory (pj_med_mgr_t *mgr)
{
    return mgr->pf;
}
