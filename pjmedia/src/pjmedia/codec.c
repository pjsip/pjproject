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
#include <pjmedia/codec.h>
#include <pjmedia/errno.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/log.h>

#define THIS_FILE   "codec.c"

/*
 * Reinitialize array of supported codecs.
 */
static void enum_all_codecs (pjmedia_codec_mgr *mgr)
{
    pjmedia_codec_factory *factory;

    mgr->codec_cnt = 0;

    factory = mgr->factory_list.next;
    while (factory != &mgr->factory_list) {
	unsigned count;
	pj_status_t status;

	count = PJ_ARRAY_SIZE(mgr->codecs) - mgr->codec_cnt;
	status = factory->op->enum_info(factory, &count, 
					mgr->codecs+mgr->codec_cnt);
	if (status == PJ_SUCCESS)
	    mgr->codec_cnt += count;

	factory = factory->next;
    }
}

/*
 * Initialize codec manager.
 */
PJ_DEF(pj_status_t) pjmedia_codec_mgr_init (pjmedia_codec_mgr *mgr)
{
    PJ_ASSERT_RETURN(mgr, PJ_EINVAL);

    pj_list_init (&mgr->factory_list);
    mgr->codec_cnt = 0;

    return PJ_SUCCESS;
}

/*
 * Register a codec factory.
 */
PJ_DEF(pj_status_t) 
pjmedia_codec_mgr_register_factory( pjmedia_codec_mgr *mgr,
				    pjmedia_codec_factory *factory)
{
    PJ_ASSERT_RETURN(mgr && factory, PJ_EINVAL);

    pj_list_push_back(&mgr->factory_list, factory);
    enum_all_codecs (mgr);

    return PJ_SUCCESS;
}

/*
 * Unregister a codec factory.
 */
PJ_DEF(pj_status_t) 
pjmedia_codec_mgr_unregister_factory(pjmedia_codec_mgr *mgr, 
				     pjmedia_codec_factory *factory)
{

    PJ_ASSERT_RETURN(mgr && factory, PJ_EINVAL);

    /* Factory must be registered. */
    PJ_ASSERT_RETURN(pj_list_find_node(&mgr->factory_list, factory)==factory,
		     PJ_ENOTFOUND);


    pj_list_erase(factory);
    enum_all_codecs (mgr);

    return PJ_SUCCESS;
}

/*
 * Enum all codecs.
 */
PJ_DEF(pj_status_t)
pjmedia_codec_mgr_enum_codecs(pjmedia_codec_mgr *mgr, 
			      unsigned *count, 
			      pjmedia_codec_info codecs[])
{
    PJ_ASSERT_RETURN(mgr && count && codecs, PJ_EINVAL);

    if (*count > mgr->codec_cnt)
	*count = mgr->codec_cnt;
    
    pj_memcpy(codecs, mgr->codecs, *count * sizeof(pjmedia_codec_info));

    return PJ_SUCCESS;
}

/*
 * Allocate one codec.
 */
PJ_DEF(pj_status_t) pjmedia_codec_mgr_alloc_codec(pjmedia_codec_mgr *mgr, 
						  const pjmedia_codec_info *info,
						  pjmedia_codec **p_codec)
{
    pjmedia_codec_factory *factory;
    pj_status_t status;

    PJ_ASSERT_RETURN(mgr && info && p_codec, PJ_EINVAL);

    *p_codec = NULL;

    factory = mgr->factory_list.next;
    while (factory != &mgr->factory_list) {

	if ( (*factory->op->test_alloc)(factory, info) == PJ_SUCCESS ) {

	    status = (*factory->op->alloc_codec)(factory, info, p_codec);
	    if (status == PJ_SUCCESS)
		return PJ_SUCCESS;

	}

	factory = factory->next;
    }


    return PJMEDIA_CODEC_EUNSUP;
}

/*
 * Dealloc codec.
 */
PJ_DEF(pj_status_t) pjmedia_codec_mgr_dealloc_codec(pjmedia_codec_mgr *mgr, 
						    pjmedia_codec *codec)
{
    PJ_ASSERT_RETURN(mgr && codec, PJ_EINVAL);

    return (*codec->factory->op->dealloc_codec)(codec->factory, codec);
}

