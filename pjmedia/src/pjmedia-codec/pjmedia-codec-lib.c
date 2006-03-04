/* $Id$ */
/* 
 * Copyright (C)2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjmedia-codec.h>

/* Include factories: */
#include <pjmedia-codec/config.h>
#include <pjmedia-codec/gsm.h>
#include <pjmedia-codec/speex.h>


static pjmedia_endpt *the_endpt;
static struct codec_list
{
    pj_status_t (*init)(pjmedia_endpt*);
    pj_status_t (*deinit)(void);
} codec_list[] = 
{

#if PJMEDIA_CODEC_HAS_GSM
    { &pjmedia_codec_gsm_init, &pjmedia_codec_gsm_deinit},
#endif

#if PJMEDIA_CODEC_HAS_SPEEX
    { &pjmedia_codec_speex_init_default, &pjmedia_codec_speex_deinit},
#endif

    { NULL, NULL }
};

/*
 * Initialize pjmedia-codec library, and register all codec factories
 * in this library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_init(pjmedia_endpt *endpt)
{
    pj_status_t status;
    unsigned i;

    the_endpt = endpt;

    for (i=0; codec_list[i].init; ++i) {
	status = (*codec_list[i].init)(the_endpt);
	if (status != PJ_SUCCESS)
	    return status;
    }
    return PJ_SUCCESS;
}


/*
 * Deinitialize pjmedia-codec library, and unregister all codec factories
 * in this library.
 */
PJ_DEF(pj_status_t) pjmedia_codec_deinit(void)
{
    pj_status_t status;
    unsigned i;

    for (i=0; codec_list[i].init; ++i) {
	status = (*codec_list[i].deinit)();
	if (status != PJ_SUCCESS)
	    return status;
    }

    return PJ_SUCCESS;
}




