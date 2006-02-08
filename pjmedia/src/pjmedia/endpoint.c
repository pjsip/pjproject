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
#include <pjmedia/endpoint.h>
#include <pjmedia/errno.h>
#include <pjmedia/sdp.h>
#include <pj/sock.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/os.h>


#define THIS_FILE   "endpoint.c"

static const pj_str_t STR_AUDIO = { "audio", 5};
static const pj_str_t STR_VIDEO = { "video", 5};
static const pj_str_t STR_IN = { "IN", 2 };
static const pj_str_t STR_IP4 = { "IP4", 3};
static const pj_str_t STR_RTP_AVP = { "RTP/AVP", 7 };
static const pj_str_t STR_SDP_NAME = { "pjmedia", 7 };
static const pj_str_t STR_SENDRECV = { "sendrecv", 8 };


PJ_DECL(pj_status_t) g711_init_factory (pjmedia_codec_factory *factory, pj_pool_t *pool);
PJ_DECL(pj_status_t) g711_deinit_factory (pjmedia_codec_factory *factory);


/** Concrete declaration of media endpoint. */
struct pjmedia_endpt
{
    /** Pool. */
    pj_pool_t		 *pool;

    /** Pool factory. */
    pj_pool_factory	 *pf;

    /** Codec manager. */
    pjmedia_codec_mgr	  codec_mgr;
};

/**
 * Initialize and get the instance of media endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_endpt_create(pj_pool_factory *pf,
					 pjmedia_endpt **p_endpt)
{
    pj_pool_t *pool;
    pjmedia_endpt *endpt;
    pjmedia_codec_factory *factory;
    pj_status_t status;

    PJ_ASSERT_RETURN(pf && p_endpt, PJ_EINVAL);

    pool = pj_pool_create(pf, "med-ept", 512, 512, NULL);
    if (!pool)
	return PJ_ENOMEM;

    endpt = pj_pool_zalloc(pool, sizeof(struct pjmedia_endpt));
    endpt->pool = pool;
    endpt->pf = pf;

    /* Sound */
    pj_snd_init(pf);

    /* Init codec manager. */
    status = pjmedia_codec_mgr_init(&endpt->codec_mgr);
    if (status != PJ_SUCCESS) {
	pj_snd_deinit();
	goto on_error;
    }

    /* Init and register G.711 codec. */
    factory = pj_pool_alloc (endpt->pool, sizeof(pjmedia_codec_factory));

    status = g711_init_factory (factory, endpt->pool);
    if (status != PJ_SUCCESS) {
	pj_snd_deinit();
	goto on_error;
    }

    status = pjmedia_codec_mgr_register_factory (&endpt->codec_mgr, factory);
    if (status != PJ_SUCCESS)  {
	pj_snd_deinit();
	goto on_error;
    }

    *p_endpt = endpt;
    return PJ_SUCCESS;

on_error:
    pj_pool_release(pool);
    return status;
}

/**
 * Get the codec manager instance.
 */
PJ_DEF(pjmedia_codec_mgr*) pjmedia_endpt_get_codec_mgr(pjmedia_endpt *endpt)
{
    return &endpt->codec_mgr;
}

/**
 * Deinitialize media endpoint.
 */
PJ_DEF(pj_status_t) pjmedia_endpt_destroy (pjmedia_endpt *endpt)
{
    PJ_ASSERT_RETURN(endpt, PJ_EINVAL);

    endpt->pf = NULL;

    pj_snd_deinit();
    pj_pool_release (endpt->pool);

    return PJ_SUCCESS;
}

/**
 * Create pool.
 */
PJ_DEF(pj_pool_t*) pjmedia_endpt_create_pool( pjmedia_endpt *endpt,
					      const char *name,
					      pj_size_t initial,
					      pj_size_t increment)
{
    pj_assert(endpt != NULL);

    return pj_pool_create(endpt->pf, name, initial, increment, NULL);
}

/**
 * Create a SDP session description that describes the endpoint
 * capability.
 */
PJ_DEF(pj_status_t) pjmedia_endpt_create_sdp( pjmedia_endpt *endpt,
					      pj_pool_t *pool,
					      unsigned stream_cnt,
					      const pjmedia_sock_info sock_info[],
					      pjmedia_sdp_session **p_sdp )
{
    pj_time_val tv;
    unsigned i;
    pjmedia_sdp_session *sdp;
    pjmedia_sdp_media *m;
    pjmedia_sdp_attr *attr;

    PJ_ASSERT_RETURN(endpt && pool && p_sdp && stream_cnt, PJ_EINVAL);


    /* Create and initialize basic SDP session */
    sdp = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_session));

    pj_gettimeofday(&tv);
    sdp->origin.user = pj_str("-");
    sdp->origin.version = sdp->origin.id = tv.sec + 2208988800UL;
    sdp->origin.net_type = STR_IN;
    sdp->origin.addr_type = STR_IP4;
    sdp->origin.addr = *pj_gethostname();
    sdp->name = STR_SDP_NAME;

    /* Since we only support one media stream at present, put the
     * SDP connection line in the session level.
     */
    sdp->conn = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_conn));
    sdp->conn->net_type = STR_IN;
    sdp->conn->addr_type = STR_IP4;
    pj_strdup2(pool, &sdp->conn->addr, 
	       pj_inet_ntoa(sock_info[0].rtp_addr_name.sin_addr));


    /* SDP time and attributes. */
    sdp->time.start = sdp->time.stop = 0;
    sdp->attr_count = 0;

    /* Create media stream 0: */

    sdp->media_count = 1;
    m = pj_pool_zalloc (pool, sizeof(pjmedia_sdp_media));
    sdp->media[0] = m;

    /* Standard media info: */
    pj_strdup(pool, &m->desc.media, &STR_AUDIO);
    m->desc.port = pj_ntohs(sock_info[0].rtp_addr_name.sin_port);
    m->desc.port_count = 1;
    pj_strdup (pool, &m->desc.transport, &STR_RTP_AVP);

    /* Add format and rtpmap for each codec. */
    m->desc.fmt_count = 0;
    m->attr_count = 0;

    for (i=0; i<endpt->codec_mgr.codec_cnt; ++i) {

	pjmedia_codec_info *codec_info = &endpt->codec_mgr.codecs[i];
	pjmedia_sdp_rtpmap rtpmap;
	pjmedia_sdp_attr *attr;
	pj_str_t *fmt = &m->desc.fmt[m->desc.fmt_count++];

	fmt->ptr = pj_pool_alloc(pool, 8);
	fmt->slen = pj_utoa(codec_info->pt, fmt->ptr);

	rtpmap.pt = *fmt;
	rtpmap.clock_rate = codec_info->sample_rate;
	rtpmap.enc_name = codec_info->encoding_name;
	rtpmap.param.slen = 0;

	pjmedia_sdp_rtpmap_to_attr(pool, &rtpmap, &attr);
	m->attr[m->attr_count++] = attr;

    }

    /* Add sendrect attribute. */
    attr = pj_pool_zalloc(pool, sizeof(pjmedia_sdp_attr));
    attr->name = STR_SENDRECV;
    m->attr[m->attr_count++] = attr;


    /* Done */
    *p_sdp = sdp;

    return PJ_SUCCESS;

}

