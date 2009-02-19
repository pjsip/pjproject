/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#include <pjmedia-audiodev/audiodev_imp.h>
#include <pj/errno.h>

/*
 * The Device ID seen by application and driver is different. 
 *
 * At application level, device ID is a 32bit value. The high 16bit contains
 * the factory ID, and the low 16bit contains the device index in the 
 * specified factory. The device ID may also be -1 to denote default device.
 *
 * At driver level, device ID is a 16bit unsigned integer index.
 */
#define MAKE_DEV_ID(f_id, index)   (((f_id & 0xFFFF) << 16) | (index & 0xFFFF))
#define GET_INDEX(dev_id)	   ((dev_id) & 0xFFFF)
#define GET_FID(dev_id)		   ((dev_id) >> 16)
#define DEFAULT_DEV_ID		    0


/* extern functions to create factories */
pjmedia_aud_dev_factory* pjmedia_pa_factory(pj_pool_factory *pf);
pjmedia_aud_dev_factory* pjmedia_wmme_factory(pj_pool_factory *pf);

#define MAX_DRIVERS	16

/* The audio subsystem */
static struct aud_subsys
{
    unsigned	     init_count;
    pj_pool_factory *pf;
    unsigned	     factory_cnt;

    struct factory
    {
	pjmedia_aud_dev_factory*   (*create)(pj_pool_factory*);
	pjmedia_aud_dev_factory	    *f;

    } factories[MAX_DRIVERS];

} aud_subsys;



/* API: Initialize the audio subsystem. */
PJ_DEF(pj_status_t) pjmedia_aud_subsys_init(pj_pool_factory *pf)
{
    unsigned i;
    pj_status_t status = PJ_ENOMEM;

    /* Allow init() to be called multiple times as long as there is matching
     * number of shutdown().
     */
    if (aud_subsys.init_count++ != 0) {
	return PJ_SUCCESS;
    }

    aud_subsys.pf = pf;
    aud_subsys.factory_cnt = 0;

    aud_subsys.factories[aud_subsys.factory_cnt++].create = &pjmedia_pa_factory;
    aud_subsys.factories[aud_subsys.factory_cnt++].create = &pjmedia_wmme_factory;

    for (i=0; i<aud_subsys.factory_cnt; ++i) {
	pjmedia_aud_dev_factory *f;

	f = (*aud_subsys.factories[i].create)(pf);
	if (!f)
	    continue;

	status = f->op->init(f);
	if (status != PJ_SUCCESS) {
	    f->op->destroy(f);
	    continue;
	}

	aud_subsys.factories[i].f = f;
	aud_subsys.factories[i].f->internal.id = i;
    }

    return aud_subsys.factory_cnt ? PJ_SUCCESS : status;
}

/* API: get the pool factory registered to the audio subsystem. */
PJ_DEF(pj_pool_factory*) pjmedia_aud_subsys_get_pool_factory(void)
{
    return aud_subsys.pf;
}

/* API: Shutdown the audio subsystem. */
PJ_DEF(pj_status_t) pjmedia_aud_subsys_shutdown(void)
{
    unsigned i;

    /* Allow shutdown() to be called multiple times as long as there is matching
     * number of init().
     */
    if (aud_subsys.init_count == 0) {
	return PJ_SUCCESS;
    }
    --aud_subsys.init_count;

    for (i=0; i<aud_subsys.factory_cnt; ++i) {
	pjmedia_aud_dev_factory *f = aud_subsys.factories[i].f;

	if (!f)
	    continue;

	f->op->destroy(f);
	aud_subsys.factories[i].f = NULL;
    }

    return PJ_SUCCESS;
}

/* API: Get the number of sound devices installed in the system. */
PJ_DEF(unsigned) pjmedia_aud_dev_count(void)
{
    unsigned i, count = 0;

    for (i=0; i<aud_subsys.factory_cnt; ++i) {
	pjmedia_aud_dev_factory *f = aud_subsys.factories[i].f;

	if (!f)
	    continue;

	count += f->op->get_dev_count(f);
    }

    return count;
}

/* API: Enumerate device ID's. */
PJ_DEF(unsigned) pjmedia_aud_dev_enum(unsigned max_count,
				      pjmedia_aud_dev_id ids[])
{
    unsigned i, count = 0;

    for (i=0; i<aud_subsys.factory_cnt && count < max_count; ++i) {
	pjmedia_aud_dev_factory *f = aud_subsys.factories[i].f;
	unsigned j, fcount;

	if (!f)
	    continue;

	fcount = f->op->get_dev_count(f);
	for (j=0; j<fcount && count<max_count; ++j) {
	    ids[count++] = MAKE_DEV_ID(i, j);
	}
    }

    return count;
}


/* API: Get device information. */
PJ_DEF(pj_status_t) pjmedia_aud_dev_get_info(pjmedia_aud_dev_id id,
					     pjmedia_aud_dev_info *info)
{
    pjmedia_aud_dev_factory *f;
    int f_id, index;

    if (id == PJMEDIA_AUD_DEV_DEFAULT_ID)
	id = DEFAULT_DEV_ID;

    f_id = GET_FID(id);
    index = GET_INDEX(id);

    if (f_id < 0 || f_id >= (int)aud_subsys.factory_cnt)
	return PJMEDIA_EAUD_INVDEV;

    f = aud_subsys.factories[f_id].f;
    if (f == NULL)
	return PJMEDIA_EAUD_INVDEV;

    return f->op->get_dev_info(f, index, info);
}

/* API: Initialize the audio device parameters with default values for the
 * specified device.
 */
PJ_DEF(pj_status_t) pjmedia_aud_dev_default_param(pjmedia_aud_dev_id id,
						  pjmedia_aud_dev_param *param)
{
    pjmedia_aud_dev_factory *f;
    int f_id, index;
    pj_status_t status;

    if (id == PJMEDIA_AUD_DEV_DEFAULT_ID)
	id = DEFAULT_DEV_ID;

    f_id = GET_FID(id);
    index = GET_INDEX(id);

    if (f_id < 0 || f_id >= (int)aud_subsys.factory_cnt)
	return PJMEDIA_EAUD_INVDEV;

    f = aud_subsys.factories[f_id].f;
    if (f == NULL)
	return PJMEDIA_EAUD_INVDEV;

    status = f->op->default_param(f, index, param);
    if (status != PJ_SUCCESS)
	return status;

    /* Normalize device IDs */
    if (param->rec_id != PJMEDIA_AUD_DEV_DEFAULT_ID)
	param->rec_id = MAKE_DEV_ID(f_id, param->rec_id);
    if (param->play_id != PJMEDIA_AUD_DEV_DEFAULT_ID)
	param->play_id = MAKE_DEV_ID(f_id, param->play_id);

    return PJ_SUCCESS;
}

/* API: Open audio stream object using the specified parameters. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_create(const pjmedia_aud_dev_param *p,
					      pjmedia_aud_rec_cb rec_cb,
					      pjmedia_aud_play_cb play_cb,
					      void *user_data,
					      pjmedia_aud_stream **p_aud_strm)
{
    pjmedia_aud_dev_factory *f;
    pjmedia_aud_dev_param param;
    int f_id;
    pj_status_t status;

    /* Must make copy of param because we're changing device ID */
    pj_memcpy(&param, p, sizeof(param));

    /* Set default device */
    if (param.rec_id == PJMEDIA_AUD_DEV_DEFAULT_ID) 
	param.rec_id = DEFAULT_DEV_ID;
    if (param.play_id == PJMEDIA_AUD_DEV_DEFAULT_ID) 
	param.play_id = DEFAULT_DEV_ID;
    
    if (param.dir & PJMEDIA_DIR_CAPTURE)
	f_id = GET_FID(param.rec_id);
    else
	f_id = GET_FID(param.play_id);

    if (f_id < 0 || f_id >= (int)aud_subsys.factory_cnt)
	return PJMEDIA_EAUD_INVDEV;
    
    /* Normalize device id's */
    param.rec_id = GET_INDEX(param.rec_id);
    param.play_id = GET_INDEX(param.play_id);

    f = aud_subsys.factories[f_id].f;
    if (f == NULL)
	return PJMEDIA_EAUD_INVDEV;

    status = f->op->create_stream(f, &param, rec_cb, play_cb,
				  user_data, p_aud_strm);
    if (status != PJ_SUCCESS)
	return status;

    (*p_aud_strm)->factory = f;
    return PJ_SUCCESS;
}

/* API: Get the running parameters for the specified audio stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_get_param(pjmedia_aud_stream *strm,
						 pjmedia_aud_dev_param *param)
{
    pj_status_t status;

    status = strm->op->get_param(strm, param);
    if (status != PJ_SUCCESS)
	return status;

    /* Normalize device id's */
    if (param->rec_id != PJMEDIA_AUD_DEV_DEFAULT_ID)
	param->rec_id = MAKE_DEV_ID(strm->factory->internal.id, param->rec_id);
    if (param->play_id != PJMEDIA_AUD_DEV_DEFAULT_ID)
	param->play_id = MAKE_DEV_ID(strm->factory->internal.id, param->play_id);

    return PJ_SUCCESS;
}

/* API: Get the value of a specific capability of the audio stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_get_cap(pjmedia_aud_stream *strm,
					       pjmedia_aud_dev_cap cap,
					       void *value)
{
    return strm->op->get_cap(strm, cap, value);
}

/* API: Set the value of a specific capability of the audio stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_set_cap(pjmedia_aud_stream *strm,
					       pjmedia_aud_dev_cap cap,
					       const void *value)
{
    return strm->op->set_cap(strm, cap, value);
}

/* API: Start the stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_start(pjmedia_aud_stream *strm)
{
    return strm->op->start(strm);
}

/* API: Stop the stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_stop(pjmedia_aud_stream *strm)
{
    return strm->op->stop(strm);
}

/* API: Destroy the stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_destroy(pjmedia_aud_stream *strm)
{
    return strm->op->destroy(strm);
}


