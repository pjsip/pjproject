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
#include <pj/log.h>
#include <pj/string.h>

#define THIS_FILE   "audiodev.c"

/* Capability names */
static struct cap_info
{
    const char *name;
    const char *info;
} cap_infos[] = 
{
    {"ext-fmt",	    "Extended/non-PCM format"},
    {"latency-in",  "Input latency/buffer size setting"},
    {"latency-out", "Output latency/buffer size setting"},
    {"vol-in",	    "Input volume setting"},
    {"vol-out",	    "Output volume setting"},
    {"meter-in",    "Input meter"},
    {"meter-out",   "Output meter"},
    {"route-in",    "Input routing"},
    {"route-out",   "Output routing"},
    {"aec",	    "Accoustic echo cancellation"},
    {"aec-tail",    "Tail length setting for AEC"},
    {"vad",	    "Voice activity detection"},
    {"cng",	    "Comfort noise generation"},
    {"plg",	    "Packet loss concealment"}
};


/*
 * The device index seen by application and driver is different. 
 *
 * At application level, device index is index to global list of device.
 * At driver level, device index is index to device list on that particular
 * factory only.
 */
#define MAKE_DEV_ID(f_id, index)   (((f_id & 0xFFFF) << 16) | (index & 0xFFFF))
#define GET_INDEX(dev_id)	   ((dev_id) & 0xFFFF)
#define GET_FID(dev_id)		   ((dev_id) >> 16)
#define DEFAULT_DEV_ID		    0


/* extern functions to create factories */
pjmedia_aud_dev_factory* pjmedia_pa_factory(pj_pool_factory *pf);
pjmedia_aud_dev_factory* pjmedia_wmme_factory(pj_pool_factory *pf);

#define MAX_DRIVERS	16
#define MAX_DEVS	64

/* typedef for factory creation function */
typedef pjmedia_aud_dev_factory*  (*create_func_ptr)(pj_pool_factory*);

/* driver structure */
struct driver
{
    create_func_ptr	     create;	/* Creation function.		    */
    pjmedia_aud_dev_factory *f;		/* Factory instance.		    */
    char		     name[32];	/* Driver name			    */
    unsigned		     dev_cnt;	/* Number of devices		    */
    unsigned		     start_idx;	/* Start index in global list	    */
};

/* The audio subsystem */
static struct aud_subsys
{
    unsigned	     init_count;	/* How many times init() is called  */
    pj_pool_factory *pf;		/* The pool factory.		    */

    unsigned	     drv_cnt;		/* Number of drivers.		    */
    struct driver    drv[MAX_DRIVERS];	/* Array of drivers.		    */

    unsigned	     dev_cnt;		/* Total number of devices.	    */
    pj_uint32_t	     dev_list[MAX_DEVS];/* Array of device IDs.		    */

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
    aud_subsys.drv_cnt = 0;
    aud_subsys.dev_cnt = 0;

    /* Register creation functions */
    aud_subsys.drv[aud_subsys.drv_cnt++].create = &pjmedia_pa_factory;
    aud_subsys.drv[aud_subsys.drv_cnt++].create = &pjmedia_wmme_factory;

    /* Initialize each factory and build the device ID list */
    for (i=0; i<aud_subsys.drv_cnt; ++i) {
	pjmedia_aud_dev_factory *f;
	pjmedia_aud_dev_info info;
	unsigned j, dev_cnt;

	/* Create the factory */
	f = (*aud_subsys.drv[i].create)(pf);
	if (!f)
	    continue;

	/* Call factory->init() */
	status = f->op->init(f);
	if (status != PJ_SUCCESS) {
	    f->op->destroy(f);
	    continue;
	}

	/* Build device list */
	dev_cnt = f->op->get_dev_count(f);
	if (dev_cnt == 0) {
	    f->op->destroy(f);
	    continue;
	}

	/* Get one device info */
	status = f->op->get_dev_info(f, 0, &info);
	if (status != PJ_SUCCESS) {
	    f->op->destroy(f);
	    continue;
	}

	/* Register the factory */
	aud_subsys.drv[i].f = f;
	aud_subsys.drv[i].f->internal.id = i;
	aud_subsys.drv[i].start_idx = aud_subsys.dev_cnt;
	pj_ansi_strncpy(aud_subsys.drv[i].name, info.driver,
			sizeof(aud_subsys.drv[i].name));
	aud_subsys.drv[i].name[sizeof(aud_subsys.drv[i].name)-1] = '\0';

	/* Register devices */
	if (aud_subsys.dev_cnt + dev_cnt > MAX_DEVS) {
	    PJ_LOG(4,(THIS_FILE, "%d device(s) cannot be registered because"
				  " there are too many sound devices",
				  aud_subsys.dev_cnt + dev_cnt - MAX_DEVS));
	    dev_cnt = MAX_DEVS - aud_subsys.dev_cnt;
	}
	for (j=0; j<dev_cnt; ++j) {
	    aud_subsys.dev_list[aud_subsys.dev_cnt++] = MAKE_DEV_ID(i, j);
	}

    }

    return aud_subsys.drv_cnt ? PJ_SUCCESS : status;
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

    for (i=0; i<aud_subsys.drv_cnt; ++i) {
	pjmedia_aud_dev_factory *f = aud_subsys.drv[i].f;

	if (!f)
	    continue;

	f->op->destroy(f);
	aud_subsys.drv[i].f = NULL;
    }

    return PJ_SUCCESS;
}

/* API: get capability name/info */
PJ_DEF(const char*) pjmedia_aud_dev_cap_name(pjmedia_aud_dev_cap cap,
					     const char **p_desc)
{
    char *desc;
    unsigned i;

    if (p_desc==NULL) p_desc = &desc;

    for (i=0; i<PJ_ARRAY_SIZE(cap_infos); ++i) {
	if ((1 << i)==cap)
	    break;
    }

    if (i==32) {
	*p_desc = "??";
	return "??";
    }

    *p_desc = cap_infos[i].info;
    return cap_infos[i].name;
}

/* API: Get the number of sound devices installed in the system. */
PJ_DEF(unsigned) pjmedia_aud_dev_count(void)
{
    return aud_subsys.dev_cnt;
}

/* Internal: lookup device id */
static pj_status_t lookup_dev(pjmedia_aud_dev_index id,
			      pjmedia_aud_dev_factory **p_f,
			      unsigned *p_local_index)
{
    int f_id, index;

    if (id == PJMEDIA_AUD_DEV_DEFAULT)
	id = DEFAULT_DEV_ID;

    PJ_ASSERT_RETURN(id>=0 && id<(int)aud_subsys.dev_cnt, 
		     PJMEDIA_EAUD_INVDEV);

    f_id = GET_FID(aud_subsys.dev_list[id]);
    index = GET_INDEX(aud_subsys.dev_list[id]);

    if (f_id < 0 || f_id >= (int)aud_subsys.drv_cnt)
	return PJMEDIA_EAUD_INVDEV;

    if (index < 0 || index >= (int)aud_subsys.drv[f_id].dev_cnt)
	return PJMEDIA_EAUD_INVDEV;

    *p_f = aud_subsys.drv[f_id].f;
    *p_local_index = (unsigned)index;

    return PJ_SUCCESS;

}

/* Internal: convert local index to global device index */
static pj_status_t make_global_index(pjmedia_aud_dev_factory *f,
				     pjmedia_aud_dev_index *id)
{
    unsigned f_id = f->internal.id;

    if (*id == PJMEDIA_AUD_DEV_DEFAULT)
	return PJ_SUCCESS;

    /* Check that factory still exists */
    PJ_ASSERT_RETURN(f, PJ_EBUG);

    /* Check that device index is valid */
    PJ_ASSERT_RETURN(*id>=0 && *id<(int)aud_subsys.drv[f_id].dev_cnt, PJ_EBUG);

    *id += aud_subsys.drv[f_id].start_idx;
    return PJ_SUCCESS;
}

/* API: Get device information. */
PJ_DEF(pj_status_t) pjmedia_aud_dev_get_info(pjmedia_aud_dev_index id,
					     pjmedia_aud_dev_info *info)
{
    pjmedia_aud_dev_factory *f;
    unsigned index;
    pj_status_t status;

    PJ_ASSERT_RETURN(info, PJ_EINVAL);

    status = lookup_dev(id, &f, &index);
    if (status != PJ_SUCCESS)
	return status;

    return f->op->get_dev_info(f, index, info);
}

/* API: find device */
PJ_DEF(pj_status_t) pjmedia_aud_dev_lookup( const char *drv_name,
					    const char *dev_name,
					    pjmedia_aud_dev_index *id)
{
    pjmedia_aud_dev_factory *f = NULL;
    unsigned i, j;

    PJ_ASSERT_RETURN(drv_name && dev_name && id, PJ_EINVAL);

    for (i=0; i<aud_subsys.drv_cnt; ++i) {
	if (!pj_ansi_stricmp(drv_name, aud_subsys.drv[i].name)) {
	    f = aud_subsys.drv[i].f;
	    break;
	}
    }

    if (!f)
	return PJ_ENOTFOUND;

    for (j=0; j<aud_subsys.drv[i].dev_cnt; ++j) {
	pjmedia_aud_dev_info info;
	pj_status_t status;

	status = f->op->get_dev_info(f, j, &info);
	if (status != PJ_SUCCESS)
	    return status;

	if (!pj_ansi_stricmp(dev_name, info.name))
	    break;
    }

    if (j==aud_subsys.drv[i].dev_cnt)
	return PJ_ENOTFOUND;

    *id = j;
    make_global_index(f, id);

    return PJ_SUCCESS;
}

/* API: Initialize the audio device parameters with default values for the
 * specified device.
 */
PJ_DEF(pj_status_t) pjmedia_aud_dev_default_param(pjmedia_aud_dev_index id,
						  pjmedia_aud_param *param)
{
    pjmedia_aud_dev_factory *f;
    unsigned index;
    pj_status_t status;

    PJ_ASSERT_RETURN(param, PJ_EINVAL);

    status = lookup_dev(id, &f, &index);
    if (status != PJ_SUCCESS)
	return status;

    status = f->op->default_param(f, index, param);
    if (status != PJ_SUCCESS)
	return status;

    /* Normalize device IDs */
    make_global_index(f, &param->rec_id);
    make_global_index(f, &param->play_id);

    return PJ_SUCCESS;
}

/* API: Open audio stream object using the specified parameters. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_create(const pjmedia_aud_param *prm,
					      pjmedia_aud_rec_cb rec_cb,
					      pjmedia_aud_play_cb play_cb,
					      void *user_data,
					      pjmedia_aud_stream **p_aud_strm)
{
    pjmedia_aud_dev_factory *rec_f=NULL, *play_f=NULL, *f=NULL;
    pjmedia_aud_param param;
    pj_status_t status;

    PJ_ASSERT_RETURN(prm && prm->dir && p_aud_strm, PJ_EINVAL);

    /* Must make copy of param because we're changing device ID */
    pj_memcpy(&param, prm, sizeof(param));

    /* Normalize rec_id */
    if (param.dir & PJMEDIA_DIR_CAPTURE) {
	unsigned index;

	status = lookup_dev(param.rec_id, &rec_f, &index);
	if (status != PJ_SUCCESS)
	    return status;

	param.rec_id = index;
	f = rec_f;
    }

    /* Normalize play_id */
    if (param.dir & PJMEDIA_DIR_PLAYBACK) {
	unsigned index;

	status = lookup_dev(param.play_id, &play_f, &index);
	if (status != PJ_SUCCESS)
	    return status;

	param.play_id = index;
	f = play_f;

	/* For now, rec_id and play_id must belong to the same factory */
	PJ_ASSERT_RETURN(rec_f == play_f, PJ_EINVAL);
    }

    
    /* Create the stream */
    status = f->op->create_stream(f, &param, rec_cb, play_cb,
				  user_data, p_aud_strm);
    if (status != PJ_SUCCESS)
	return status;

    /* Assign factory id to the stream */
    (*p_aud_strm)->factory_id = f->internal.id;
    return PJ_SUCCESS;
}

/* API: Get the running parameters for the specified audio stream. */
PJ_DEF(pj_status_t) pjmedia_aud_stream_get_param(pjmedia_aud_stream *strm,
						 pjmedia_aud_param *param)
{
    pj_status_t status;

    status = strm->op->get_param(strm, param);
    if (status != PJ_SUCCESS)
	return status;

    /* Normalize device id's */
    make_global_index(aud_subsys.drv[strm->factory_id].f, &param->rec_id);
    make_global_index(aud_subsys.drv[strm->factory_id].f, &param->play_id);

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


