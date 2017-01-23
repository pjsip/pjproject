/* $Id$ */
/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/assert.h>

#define THIS_FILE   "audiodev.c"


/* extern functions to create factories */
#if PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO
pjmedia_aud_dev_factory* pjmedia_pa_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_COREAUDIO
pjmedia_aud_dev_factory* pjmedia_coreaudio_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_ALSA
pjmedia_aud_dev_factory* pjmedia_alsa_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_OPENSL
pjmedia_aud_dev_factory* pjmedia_opensl_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_ANDROID_JNI
pjmedia_aud_dev_factory* pjmedia_android_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_BB10
pjmedia_aud_dev_factory* pjmedia_bb10_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_WMME
pjmedia_aud_dev_factory* pjmedia_wmme_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_BDIMAD
pjmedia_aud_dev_factory* pjmedia_bdimad_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_SYMB_VAS
pjmedia_aud_dev_factory* pjmedia_symb_vas_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_SYMB_APS
pjmedia_aud_dev_factory* pjmedia_aps_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_SYMB_MDA
pjmedia_aud_dev_factory* pjmedia_symb_mda_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_WASAPI
pjmedia_aud_dev_factory* pjmedia_wasapi_factory(pj_pool_factory *pf);
#endif

#if PJMEDIA_AUDIO_DEV_HAS_NULL_AUDIO
pjmedia_aud_dev_factory* pjmedia_null_audio_factory(pj_pool_factory *pf);
#endif


/* API: Initialize the audio subsystem. */
PJ_DEF(pj_status_t) pjmedia_aud_subsys_init(pj_pool_factory *pf)
{
    unsigned i;
    pj_status_t status;
    pjmedia_aud_subsys *aud_subsys = pjmedia_get_aud_subsys();

    /* Allow init() to be called multiple times as long as there is matching
     * number of shutdown().
     */
    if (aud_subsys->init_count++ != 0) {
	return PJ_SUCCESS;
    }

    /* Register error subsystem */
    status = pj_register_strerror(PJMEDIA_AUDIODEV_ERRNO_START,
				  PJ_ERRNO_SPACE_SIZE,
				  &pjmedia_audiodev_strerror);
    pj_assert(status == PJ_SUCCESS);

    /* Init */
    aud_subsys->pf = pf;
    aud_subsys->drv_cnt = 0;
    aud_subsys->dev_cnt = 0;

    /* Register creation functions */
#if PJMEDIA_AUDIO_DEV_HAS_OPENSL
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_opensl_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_ANDROID_JNI
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_android_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_BB10
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_bb10_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_ALSA
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_alsa_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_COREAUDIO
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_coreaudio_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_pa_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_WMME
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_wmme_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_BDIMAD
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_bdimad_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_SYMB_VAS
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_symb_vas_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_SYMB_APS
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_aps_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_SYMB_MDA
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_symb_mda_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_WASAPI
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_wasapi_factory;
#endif
#if PJMEDIA_AUDIO_DEV_HAS_NULL_AUDIO
    aud_subsys->drv[aud_subsys->drv_cnt++].create = &pjmedia_null_audio_factory;
#endif

    /* Initialize each factory and build the device ID list */
    for (i=0; i<aud_subsys->drv_cnt; ++i) {
	status = pjmedia_aud_driver_init(i, PJ_FALSE);
	if (status != PJ_SUCCESS) {
	    pjmedia_aud_driver_deinit(i);
	    continue;
	}
    }

    return aud_subsys->dev_cnt ? PJ_SUCCESS : status;
}

/* API: register an audio device factory to the audio subsystem. */
PJ_DEF(pj_status_t)
pjmedia_aud_register_factory(pjmedia_aud_dev_factory_create_func_ptr adf)
{
    pj_status_t status;
    pjmedia_aud_subsys *aud_subsys = pjmedia_get_aud_subsys();

    if (aud_subsys->init_count == 0)
	return PJMEDIA_EAUD_INIT;

    aud_subsys->drv[aud_subsys->drv_cnt].create = adf;
    status = pjmedia_aud_driver_init(aud_subsys->drv_cnt, PJ_FALSE);
    if (status == PJ_SUCCESS) {
	aud_subsys->drv_cnt++;
    } else {
	pjmedia_aud_driver_deinit(aud_subsys->drv_cnt);
    }

    return status;
}

/* API: unregister an audio device factory from the audio subsystem. */
PJ_DEF(pj_status_t)
pjmedia_aud_unregister_factory(pjmedia_aud_dev_factory_create_func_ptr adf)
{
    unsigned i, j;
    pjmedia_aud_subsys *aud_subsys = pjmedia_get_aud_subsys();

    if (aud_subsys->init_count == 0)
	return PJMEDIA_EAUD_INIT;

    for (i=0; i<aud_subsys->drv_cnt; ++i) {
	pjmedia_aud_driver *drv = &aud_subsys->drv[i];

	if (drv->create == adf) {
	    for (j = drv->start_idx; j < drv->start_idx + drv->dev_cnt; j++)
	    {
		aud_subsys->dev_list[j] = (pj_uint32_t)PJMEDIA_AUD_INVALID_DEV;
	    }

	    pjmedia_aud_driver_deinit(i);
	    return PJ_SUCCESS;
	}
    }

    return PJMEDIA_EAUD_ERR;
}

/* API: get the pool factory registered to the audio subsystem. */
PJ_DEF(pj_pool_factory*) pjmedia_aud_subsys_get_pool_factory(void)
{
    pjmedia_aud_subsys *aud_subsys = pjmedia_get_aud_subsys();
    return aud_subsys->pf;
}

/* API: Shutdown the audio subsystem. */
PJ_DEF(pj_status_t) pjmedia_aud_subsys_shutdown(void)
{
    unsigned i;
    pjmedia_aud_subsys *aud_subsys = pjmedia_get_aud_subsys();

    /* Allow shutdown() to be called multiple times as long as there is matching
     * number of init().
     */
    if (aud_subsys->init_count == 0) {
	return PJ_SUCCESS;
    }
    --aud_subsys->init_count;

    if (aud_subsys->init_count == 0) {
	for (i=0; i<aud_subsys->drv_cnt; ++i) {
	    pjmedia_aud_driver_deinit(i);
	}

	aud_subsys->pf = NULL;
    }
    return PJ_SUCCESS;
}
