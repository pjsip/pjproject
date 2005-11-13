/* $Id$
 *
 */
/* 
 * PJMEDIA - Multimedia over IP Stack 
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
#include <pjmedia/codec.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/log.h>

#define THIS_FILE   "codec.c"

static void enum_all_codecs (pj_codec_mgr *cm)
{
    pj_codec_factory *cf;

    cf = cm->factory_list.next;
    cm->codec_cnt = 0;
    while (cf != &cm->factory_list) {
	pj_codec_id temp[PJ_CODEC_MGR_MAX_CODECS];
	int i, cnt;

	cnt = cf->op->enum_codecs (cf, PJ_CODEC_MGR_MAX_CODECS, temp);
	if (cnt > PJ_CODEC_MGR_MAX_CODECS) {
	    pj_assert(0);
	    PJ_LOG(4, (THIS_FILE, "Too many codecs reported by factory"));
	    cnt = PJ_CODEC_MGR_MAX_CODECS;
	}

	for (i=0; i<cnt && cm->codec_cnt < PJ_CODEC_MGR_MAX_CODECS; ++i) {
	    cm->codecs[cm->codec_cnt++] = temp[i];
	}

	cf = cf->next;
    }
}

PJ_DEF(pj_status_t) pj_codec_mgr_init (pj_codec_mgr *mgr)
{
    pj_list_init (&mgr->factory_list);
    mgr->codec_cnt = 0;
    return 0;
}

PJ_DEF(pj_status_t) pj_codec_mgr_register_factory (pj_codec_mgr *mgr,
						   pj_codec_factory *factory)
{
    pj_list_insert_before (&mgr->factory_list, factory);
    enum_all_codecs (mgr);
    return 0;
}

PJ_DEF(void) pj_codec_mgr_unregister_factory (pj_codec_mgr *mgr, pj_codec_factory *factory)
{
    PJ_UNUSED_ARG(mgr)
    pj_list_erase(factory);
    enum_all_codecs (mgr);
}

PJ_DEF(unsigned)
pj_codec_mgr_enum_codecs (pj_codec_mgr *mgr, unsigned count, const pj_codec_id *codecs[])
{
    unsigned i;

    if (count > mgr->codec_cnt)
	count = mgr->codec_cnt;

    for (i=0; i<count; ++i)
	codecs[i] = &mgr->codecs[i];

    return mgr->codec_cnt;
}

PJ_DEF(pj_codec*) pj_codec_mgr_alloc_codec (pj_codec_mgr *mgr, const struct pj_codec_id *id)
{
    pj_codec_factory *factory = mgr->factory_list.next;
    while (factory != &mgr->factory_list) {
	if ( (*factory->op->match_id)(factory, id) == 0 ) {
	    pj_codec *codec = (*factory->op->alloc_codec)(factory, id);
	    if (codec != NULL)
		return codec;
	}
	factory = factory->next;
    }
    return NULL;
}

PJ_DEF(void) pj_codec_mgr_dealloc_codec (pj_codec_mgr *mgr, pj_codec *codec)
{
    PJ_UNUSED_ARG(mgr)
    (*codec->factory->op->dealloc_codec)(codec->factory, codec);
}

