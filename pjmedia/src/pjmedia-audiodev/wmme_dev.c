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
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/string.h>
#include <pj/unicode.h>
#ifdef _MSC_VER
#   pragma warning(push, 3)
#endif

#include <windows.h>
#include <mmsystem.h>

#ifdef _MSC_VER
#   pragma warning(pop)
#endif

#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
#   pragma comment(lib, "Coredll.lib")
#elif defined(_MSC_VER)
#   pragma comment(lib, "winmm.lib")
#endif


#define THIS_FILE			"wmme_dev.c"
#define BITS_PER_SAMPLE			16
#define BYTES_PER_SAMPLE		(BITS_PER_SAMPLE/8)


/* WMME device info */
struct wmme_dev_info
{
    pjmedia_aud_dev_info	 info;
    unsigned			 deviceId;
};

/* WMME factory */
struct wmme_factory
{
    pjmedia_aud_dev_factory	 base;
    pj_pool_t			*pool;
    pj_pool_factory		*pf;

    unsigned			 dev_count;
    struct wmme_dev_info	*dev_info;
};


/* Individual WMME capture/playback stream descriptor */
struct wmme_channel
{
    union
    {
	HWAVEIN   In;
	HWAVEOUT  Out;
    } hWave;

    WAVEHDR      *WaveHdr;
    HANDLE        hEvent;
    DWORD         dwBufIdx;
    DWORD         dwMaxBufIdx;
    unsigned	  latency_ms;
    pj_timestamp  timestamp;
};


/* Sound stream. */
struct wmme_stream
{
    pjmedia_aud_stream	 base;
    pjmedia_dir          dir;               /**< Sound direction.      */
    int                  play_id;           /**< Playback dev id.      */
    int                  rec_id;            /**< Recording dev id.     */
    pj_pool_t           *pool;              /**< Memory pool.          */

    pjmedia_aud_rec_cb   rec_cb;            /**< Capture callback.     */
    pjmedia_aud_play_cb  play_cb;           /**< Playback callback.    */
    void                *user_data;         /**< Application data.     */

    struct wmme_channel  play_strm;         /**< Playback stream.      */
    struct wmme_channel  rec_strm;          /**< Capture stream.       */

    void    		*buffer;	    /**< Temp. frame buffer.   */
    unsigned             clock_rate;        /**< Clock rate.           */
    unsigned             samples_per_frame; /**< Samples per frame.    */
    unsigned             bits_per_sample;   /**< Bits per sample.      */
    unsigned             channel_count;     /**< Channel count.        */

    pj_thread_t         *thread;            /**< Thread handle.        */
    HANDLE               thread_quit_event; /**< Quit signal to thread */
};


/* Prototypes */
static pj_status_t factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t factory_destroy(pjmedia_aud_dev_factory *f);
static unsigned    factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t factory_get_dev_info(pjmedia_aud_dev_factory *f, 
					unsigned index,
					pjmedia_aud_dev_info *info);
static pj_status_t factory_default_param(pjmedia_aud_dev_factory *f,
					 unsigned index,
					 pjmedia_aud_param *param);
static pj_status_t factory_create_stream(pjmedia_aud_dev_factory *f,
					 const pjmedia_aud_param *param,
					 pjmedia_aud_rec_cb rec_cb,
					 pjmedia_aud_play_cb play_cb,
					 void *user_data,
					 pjmedia_aud_stream **p_aud_strm);

static pj_status_t stream_get_param(pjmedia_aud_stream *strm,
				    pjmedia_aud_param *param);
static pj_status_t stream_get_cap(pjmedia_aud_stream *strm,
				  pjmedia_aud_dev_cap cap,
				  void *value);
static pj_status_t stream_set_cap(pjmedia_aud_stream *strm,
				  pjmedia_aud_dev_cap cap,
				  const void *value);
static pj_status_t stream_start(pjmedia_aud_stream *strm);
static pj_status_t stream_stop(pjmedia_aud_stream *strm);
static pj_status_t stream_destroy(pjmedia_aud_stream *strm);


/* Operations */
static pjmedia_aud_dev_factory_op factory_op =
{
    &factory_init,
    &factory_destroy,
    &factory_get_dev_count,
    &factory_get_dev_info,
    &factory_default_param,
    &factory_create_stream
};

static pjmedia_aud_stream_op stream_op = 
{
    &stream_get_param,
    &stream_get_cap,
    &stream_set_cap,
    &stream_start,
    &stream_stop,
    &stream_destroy
};

/* Utility: convert MMERROR to pj_status_t */
PJ_INLINE(pj_status_t) CONVERT_MM_ERROR(MMRESULT mr)
{
    return PJ_RETURN_OS_ERROR(mr);
}


/****************************************************************************
 * Factory operations
 */
/*
 * Init WMME audio driver.
 */
pjmedia_aud_dev_factory* pjmedia_wmme_factory(pj_pool_factory *pf)
{
    struct wmme_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "WMME", 1000, 1000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct wmme_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;

    return &f->base;
}


/* Internal: build device info from WAVEINCAPS/WAVEOUTCAPS */
static void build_dev_info(UINT deviceId, struct wmme_dev_info *wdi, 
			   WAVEINCAPS *wic, WAVEOUTCAPS *woc)
{
#define WIC_WOC(wic,woc,field)	(wic? wic->field : woc->field)

    pj_bzero(wdi, sizeof(*wdi));
    wdi->deviceId = deviceId;

    /* Device Name */
    if (deviceId==WAVE_MAPPER) {
	strncpy(wdi->info.name, "Wave mapper", sizeof(wdi->info.name));
	wdi->info.name[sizeof(wdi->info.name)-1] = '\0';
    } else {
	pj_char_t *szPname = WIC_WOC(wic, woc, szPname);
	PJ_DECL_ANSI_TEMP_BUF(wTmp, sizeof(wdi->info.name));
	
	strncpy(wdi->info.name, 
		PJ_NATIVE_TO_STRING(szPname, wTmp, PJ_ARRAY_SIZE(wTmp)),
		sizeof(wdi->info.name));
	wdi->info.name[sizeof(wdi->info.name)-1] = '\0';
    }

    wdi->info.default_samples_per_sec = 16000;
    strcpy(wdi->info.driver, "WMME");

    if (wic) {
	wdi->info.input_count = wic->wChannels;
	wdi->info.caps |= PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY;

	/* Sometimes a device can return a rediculously large number of 
	 * channels. This happened with an SBLive card on a Windows ME box.
	 * It also happens on Win XP!
	 */
	if (wdi->info.input_count<1 || wdi->info.input_count>256) {
	    wdi->info.input_count = 2;
	}
    }

    if (woc) {
	wdi->info.output_count = woc->wChannels;
	wdi->info.caps |= PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
	
	if (woc->dwSupport & WAVECAPS_VOLUME) {
	    wdi->info.caps |= PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
	}

	/* Sometimes a device can return a rediculously large number of 
	 * channels. This happened with an SBLive card on a Windows ME box.
	 * It also happens on Win XP!
	 */
	if (wdi->info.output_count<1 || wdi->info.output_count>256) {
	    wdi->info.output_count = 2;
	}
    }
}

/* API: init factory */
static pj_status_t factory_init(pjmedia_aud_dev_factory *f)
{
    struct wmme_factory *wf = (struct wmme_factory*)f;
    unsigned c;
    int i;
    int inputDeviceCount, outputDeviceCount, devCount=0;
    pj_bool_t waveMapperAdded = PJ_FALSE;

    /* Enumerate sound devices */
    wf->dev_count = 0;

    inputDeviceCount = waveInGetNumDevs();
    devCount += inputDeviceCount;

    outputDeviceCount = waveOutGetNumDevs();
    devCount += outputDeviceCount;

    if (devCount) {
	/* Assume there is WAVE_MAPPER */
	devCount += 2;
    }

    if (devCount==0) {
	PJ_LOG(4,(THIS_FILE, "WMME found no sound devices"));
	return PJMEDIA_EAUD_NODEV;
    }

    wf->dev_info = (struct wmme_dev_info*)
		   pj_pool_calloc(wf->pool, devCount, 
				  sizeof(struct wmme_dev_info));

    if (devCount) {
	/* Attempt to add WAVE_MAPPER as input and output device */
	WAVEINCAPS wic;
	MMRESULT mr;

	pj_bzero(&wic, sizeof(WAVEINCAPS));
	mr = waveInGetDevCaps(WAVE_MAPPER, &wic, sizeof(WAVEINCAPS));

	if (mr == MMSYSERR_NOERROR) {
	    WAVEOUTCAPS woc;

	    pj_bzero(&woc, sizeof(WAVEOUTCAPS));
	    mr = waveOutGetDevCaps(WAVE_MAPPER, &woc, sizeof(WAVEOUTCAPS));
	    if (mr == MMSYSERR_NOERROR) {
		build_dev_info(WAVE_MAPPER, &wf->dev_info[wf->dev_count], 
			       &wic, &woc);
		++wf->dev_count;
		waveMapperAdded = PJ_TRUE;
	    }
	}

    }

    if (inputDeviceCount > 0) {
	/* -1 is the WAVE_MAPPER */
	for (i = (waveMapperAdded? 0 : -1); i < inputDeviceCount; ++i) {
	    UINT uDeviceID = (UINT)((i==-1) ? WAVE_MAPPER : i);
	    WAVEINCAPS wic;
	    MMRESULT mr;

	    pj_bzero(&wic, sizeof(WAVEINCAPS));

	    mr = waveInGetDevCaps(uDeviceID, &wic, sizeof(WAVEINCAPS));

	    if (mr == MMSYSERR_NOMEM)
		return PJ_ENOMEM;

	    if (mr != MMSYSERR_NOERROR)
		continue;

	    build_dev_info(uDeviceID, &wf->dev_info[wf->dev_count], 
			   &wic, NULL);
	    ++wf->dev_count;
	}
    }

    if( outputDeviceCount > 0 )
    {
	/* -1 is the WAVE_MAPPER */
	for (i = (waveMapperAdded? 0 : -1); i < outputDeviceCount; ++i) {
	    UINT uDeviceID = (UINT)((i==-1) ? WAVE_MAPPER : i);
	    WAVEOUTCAPS woc;
	    MMRESULT mr;

	    pj_bzero(&woc, sizeof(WAVEOUTCAPS));

	    mr = waveOutGetDevCaps(uDeviceID, &woc, sizeof(WAVEOUTCAPS));

	    if (mr == MMSYSERR_NOMEM)
		return PJ_ENOMEM;

	    if (mr != MMSYSERR_NOERROR)
		continue;

	    build_dev_info(uDeviceID, &wf->dev_info[wf->dev_count], 
			   NULL, &woc);
	    ++wf->dev_count;
	}
    }

    PJ_LOG(4, (THIS_FILE, "WMME initialized, found %d devices:", 
	       wf->dev_count));
    for (c = 0; c < wf->dev_count; ++c) {
	PJ_LOG(4, (THIS_FILE, " dev_id %d: %s  (in=%d, out=%d)", 
	    c,
	    wf->dev_info[c].info.name,
	    wf->dev_info[c].info.input_count,
	    wf->dev_info[c].info.output_count));
    }

    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct wmme_factory *wf = (struct wmme_factory*)f;
    pj_pool_t *pool = wf->pool;

    wf->pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    struct wmme_factory *wf = (struct wmme_factory*)f;
    return wf->dev_count;
}

/* API: get device info */
static pj_status_t factory_get_dev_info(pjmedia_aud_dev_factory *f, 
					unsigned index,
					pjmedia_aud_dev_info *info)
{
    struct wmme_factory *wf = (struct wmme_factory*)f;

    PJ_ASSERT_RETURN(index < wf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_memcpy(info, &wf->dev_info[index].info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t factory_default_param(pjmedia_aud_dev_factory *f,
					 unsigned index,
					 pjmedia_aud_param *param)
{
    struct wmme_factory *wf = (struct wmme_factory*)f;
    struct wmme_dev_info *di = &wf->dev_info[index];

    PJ_ASSERT_RETURN(index < wf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_bzero(param, sizeof(*param));
    if (di->info.input_count && di->info.output_count) {
	param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
	param->rec_id = index;
	param->play_id = index;
    } else if (di->info.input_count) {
	param->dir = PJMEDIA_DIR_CAPTURE;
	param->rec_id = index;
	param->play_id = PJMEDIA_AUD_DEV_DEFAULT;
    } else if (di->info.output_count) {
	param->dir = PJMEDIA_DIR_PLAYBACK;
	param->play_id = index;
	param->rec_id = PJMEDIA_AUD_DEV_DEFAULT;
    } else {
	return PJMEDIA_EAUD_INVDEV;
    }

    param->clock_rate = di->info.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = di->info.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 16;
    param->flags = di->info.caps;
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    return PJ_SUCCESS;
}

/* Internal: init WAVEFORMATEX */
static void init_waveformatex (LPWAVEFORMATEX pcmwf, 
			       unsigned clock_rate,
			       unsigned channel_count)
{
    pj_bzero(pcmwf, sizeof(PCMWAVEFORMAT)); 
    pcmwf->wFormatTag = WAVE_FORMAT_PCM; 
    pcmwf->nChannels = (pj_uint16_t)channel_count;
    pcmwf->nSamplesPerSec = clock_rate;
    pcmwf->nBlockAlign = (pj_uint16_t)(channel_count * BYTES_PER_SAMPLE);
    pcmwf->nAvgBytesPerSec = clock_rate * channel_count * BYTES_PER_SAMPLE;
    pcmwf->wBitsPerSample = BITS_PER_SAMPLE;
}


/* Internal: create WMME player device. */
static pj_status_t init_player_stream(  struct wmme_factory *wf,
				        pj_pool_t *pool,
				        struct wmme_channel *wmme_strm,
					unsigned dev_id,
					unsigned clock_rate,
					unsigned channel_count,
					unsigned samples_per_frame,
					unsigned buffer_count)
{
    MMRESULT mr;
    WAVEFORMATEX pcmwf; 
    unsigned bytes_per_frame;
    unsigned i;

    PJ_ASSERT_RETURN(dev_id < wf->dev_count, PJ_EINVAL);

    /*
     * Create a wait event.
     */
    wmme_strm->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == wmme_strm->hEvent)
	return pj_get_os_error();

    /*
     * Set up wave format structure for opening the device.
     */
    init_waveformatex(&pcmwf, clock_rate, channel_count);
    bytes_per_frame = samples_per_frame * BYTES_PER_SAMPLE;

    /*
     * Open wave device.
     */
    mr = waveOutOpen(&wmme_strm->hWave.Out, wf->dev_info[dev_id].deviceId, 
		     &pcmwf, (DWORD)wmme_strm->hEvent, 0, CALLBACK_EVENT);
    if (mr != MMSYSERR_NOERROR) {
	return CONVERT_MM_ERROR(mr);
    }

    /* Pause the wave out device */
    mr = waveOutPause(wmme_strm->hWave.Out);
    if (mr != MMSYSERR_NOERROR) {
	return CONVERT_MM_ERROR(mr);
    }

    /*
     * Create the buffers. 
     */
    wmme_strm->WaveHdr = (WAVEHDR*)
			 pj_pool_zalloc(pool, sizeof(WAVEHDR) * buffer_count);
    for (i = 0; i < buffer_count; ++i) {
	wmme_strm->WaveHdr[i].lpData = pj_pool_zalloc(pool, bytes_per_frame);
	wmme_strm->WaveHdr[i].dwBufferLength = bytes_per_frame;
	mr = waveOutPrepareHeader(wmme_strm->hWave.Out, 
				  &(wmme_strm->WaveHdr[i]),
				  sizeof(WAVEHDR));
	if (mr != MMSYSERR_NOERROR) {
	    return CONVERT_MM_ERROR(mr); 
	}
	mr = waveOutWrite(wmme_strm->hWave.Out, &(wmme_strm->WaveHdr[i]), 
			  sizeof(WAVEHDR));
	if (mr != MMSYSERR_NOERROR) {
	    return CONVERT_MM_ERROR(mr);
	}
    }

    wmme_strm->dwBufIdx = 0;
    wmme_strm->dwMaxBufIdx = buffer_count;
    wmme_strm->timestamp.u64 = 0;

    /* Done setting up play device. */
    PJ_LOG(5, (THIS_FILE, 
	       " WaveAPI Sound player \"%s\" initialized (clock_rate=%d, "
	       "channel_count=%d, samples_per_frame=%d (%dms))",
	       wf->dev_info[dev_id].info.name,
	       clock_rate, channel_count, samples_per_frame,
	       samples_per_frame * 1000 / clock_rate));

    return PJ_SUCCESS;
}


/* Internal: create Windows Multimedia recorder device */
static pj_status_t init_capture_stream( struct wmme_factory *wf,
				        pj_pool_t *pool,
					struct wmme_channel *wmme_strm,
					unsigned dev_id,
					unsigned clock_rate,
					unsigned channel_count,
					unsigned samples_per_frame,
					unsigned buffer_count)
{
    MMRESULT mr;
    WAVEFORMATEX pcmwf; 
    unsigned bytes_per_frame;
    unsigned i;

    PJ_ASSERT_RETURN(dev_id < wf->dev_count, PJ_EINVAL);

    /*
    * Create a wait event.
    */
    wmme_strm->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == wmme_strm->hEvent) {
	return pj_get_os_error();
    }

    /*
     * Set up wave format structure for opening the device.
     */
    init_waveformatex(&pcmwf, clock_rate, channel_count);
    bytes_per_frame = samples_per_frame * BYTES_PER_SAMPLE;

    /*
     * Open wave device.
     */
    mr = waveInOpen(&wmme_strm->hWave.In, wf->dev_info[dev_id].deviceId, 
		    &pcmwf, (DWORD)wmme_strm->hEvent, 0, CALLBACK_EVENT);
    if (mr != MMSYSERR_NOERROR) {
	return CONVERT_MM_ERROR(mr);
    }

    /*
     * Create the buffers. 
     */
    wmme_strm->WaveHdr = (WAVEHDR*)
			 pj_pool_zalloc(pool, sizeof(WAVEHDR) * buffer_count);
    for (i = 0; i < buffer_count; ++i) {
	wmme_strm->WaveHdr[i].lpData = pj_pool_zalloc(pool, bytes_per_frame);
	wmme_strm->WaveHdr[i].dwBufferLength = bytes_per_frame;
	mr = waveInPrepareHeader(wmme_strm->hWave.In, 
				 &(wmme_strm->WaveHdr[i]),
				 sizeof(WAVEHDR));
	if (mr != MMSYSERR_NOERROR) {
	    return CONVERT_MM_ERROR(mr);
	}
	mr = waveInAddBuffer(wmme_strm->hWave.In, &(wmme_strm->WaveHdr[i]), 
			     sizeof(WAVEHDR));
	if (mr != MMSYSERR_NOERROR) {
	    return CONVERT_MM_ERROR(mr);
	}
    }

    wmme_strm->dwBufIdx = 0;
    wmme_strm->dwMaxBufIdx = buffer_count;
    wmme_strm->timestamp.u64 = 0;

    /* Done setting up play device. */
    PJ_LOG(5,(THIS_FILE, 
	" WaveAPI Sound recorder \"%s\" initialized (clock_rate=%d, "
	"channel_count=%d, samples_per_frame=%d (%dms))",
	wf->dev_info[dev_id].info.name,
	clock_rate, channel_count, samples_per_frame,
	samples_per_frame * 1000 / clock_rate));

    return PJ_SUCCESS;
}


/* WMME capture and playback thread. */
static int PJ_THREAD_FUNC wmme_dev_thread(void *arg)
{
    struct wmme_stream *strm = (struct wmme_stream*)arg;
    HANDLE events[3];
    unsigned eventCount;
    unsigned bytes_per_frame;
    pj_status_t status = PJ_SUCCESS;


    eventCount = 0;
    events[eventCount++] = strm->thread_quit_event;
    if (strm->dir & PJMEDIA_DIR_PLAYBACK)
	events[eventCount++] = strm->play_strm.hEvent;
    if (strm->dir & PJMEDIA_DIR_CAPTURE)
	events[eventCount++] = strm->rec_strm.hEvent;


    /* Raise self priority. We don't want the audio to be distorted by
     * system activity.
     */
#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE != 0
    if (strm->dir & PJMEDIA_DIR_PLAYBACK)
	CeSetThreadPriority(GetCurrentThread(), 153);
    else
	CeSetThreadPriority(GetCurrentThread(), 247);
#else
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    /* Calculate bytes per frame */
    bytes_per_frame = strm->samples_per_frame * BYTES_PER_SAMPLE;

    /*
     * Loop while not signalled to quit, wait for event objects to be 
     * signalled by WMME capture and play buffer.
     */
    while (status == PJ_SUCCESS)
    {

	DWORD rc;
	pjmedia_dir signalled_dir;

	rc = WaitForMultipleObjects(eventCount, events, FALSE, INFINITE);
	if (rc < WAIT_OBJECT_0 || rc >= WAIT_OBJECT_0 + eventCount)
	    continue;

	if (rc == WAIT_OBJECT_0)
	    break;

	if (rc == (WAIT_OBJECT_0 + 1))
	{
	    if (events[1] == strm->play_strm.hEvent)
		signalled_dir = PJMEDIA_DIR_PLAYBACK;
	    else
		signalled_dir = PJMEDIA_DIR_CAPTURE;
	}
	else
	{
	    if (events[2] == strm->play_strm.hEvent)
		signalled_dir = PJMEDIA_DIR_PLAYBACK;
	    else
		signalled_dir = PJMEDIA_DIR_CAPTURE;
	}


	if (signalled_dir == PJMEDIA_DIR_PLAYBACK)
	{
	    struct wmme_channel *wmme_strm = &strm->play_strm;
	    MMRESULT mr = MMSYSERR_NOERROR;
	    status = PJ_SUCCESS;

	    /*
	     * Windows Multimedia has requested us to feed some frames to
	     * playback buffer.
	     */

	    while (wmme_strm->WaveHdr[wmme_strm->dwBufIdx].dwFlags & WHDR_DONE)
	    {
		void *buffer = wmme_strm->WaveHdr[wmme_strm->dwBufIdx].lpData;
		pjmedia_frame frame;

		//PJ_LOG(5,(THIS_FILE, "Finished writing buffer %d", 
		//	  wmme_strm->dwBufIdx));

		frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
		frame.size = bytes_per_frame;
		frame.buf = buffer;
		frame.timestamp.u64 = wmme_strm->timestamp.u64;
		frame.bit_info = 0;

		/* Get frame from application. */
		status = (*strm->play_cb)(strm->user_data, &frame);

		if (status != PJ_SUCCESS)
		    break;

		if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO) {
		    pj_bzero(buffer, bytes_per_frame);
		}

		/* Write to the device. */
		mr = waveOutWrite(wmme_strm->hWave.Out, 
				  &(wmme_strm->WaveHdr[wmme_strm->dwBufIdx]), 
				  sizeof(WAVEHDR));
		if (mr != MMSYSERR_NOERROR)
		{
		    status = CONVERT_MM_ERROR(mr);
		    break;
		}

		/* Increment position. */
		if (++wmme_strm->dwBufIdx >= wmme_strm->dwMaxBufIdx)
		    wmme_strm->dwBufIdx = 0;
		wmme_strm->timestamp.u64 += strm->samples_per_frame / 
					    strm->channel_count;
	    }
	}
	else
	{
	    struct wmme_channel *wmme_strm = &strm->rec_strm;
	    MMRESULT mr = MMSYSERR_NOERROR;
	    status = PJ_SUCCESS;

	    /*
	    * Windows Multimedia has indicated that it has some frames ready
	    * in the capture buffer. Get as much frames as possible to
	    * prevent overflows.
	    */
#if 0
	    {
		static DWORD tc = 0;
		DWORD now = GetTickCount();
		DWORD i = 0;
		DWORD bits = 0;

		if (tc == 0) tc = now;

		for (i = 0; i < wmme_strm->dwMaxBufIdx; ++i)
		{
		    bits = bits << 4;
		    bits |= wmme_strm->WaveHdr[i].dwFlags & WHDR_DONE;
		}
		PJ_LOG(5,(THIS_FILE, "Record Signal> Index: %d, Delta: %4.4d, "
			  "Flags: %6.6x\n",
			  wmme_strm->dwBufIdx,
			  now - tc,
			  bits));
		tc = now;
	    }
#endif

	    while (wmme_strm->WaveHdr[wmme_strm->dwBufIdx].dwFlags & WHDR_DONE)
	    {
		char* buffer = (char*)
			       wmme_strm->WaveHdr[wmme_strm->dwBufIdx].lpData;
		unsigned cap_len = 
			wmme_strm->WaveHdr[wmme_strm->dwBufIdx].dwBytesRecorded;
		pjmedia_frame frame;

		/*
		PJ_LOG(5,(THIS_FILE, "Read %d bytes from buffer %d", cap_len, 
			  wmme_strm->dwBufIdx));
		*/

		if (cap_len < bytes_per_frame)
		    pj_bzero(buffer + cap_len, bytes_per_frame - cap_len);

		/* Copy the audio data out of the wave buffer. */
		pj_memcpy(strm->buffer, buffer, bytes_per_frame);

		/* Re-add the buffer to the device. */
		mr = waveInAddBuffer(wmme_strm->hWave.In, 
				     &(wmme_strm->WaveHdr[wmme_strm->dwBufIdx]), 
				     sizeof(WAVEHDR));
		if (mr != MMSYSERR_NOERROR) {
		    status = CONVERT_MM_ERROR(mr);
		    break;
		}

		/* Prepare frame */
		frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
		frame.buf = strm->buffer;
		frame.size = bytes_per_frame;
		frame.timestamp.u64 = wmme_strm->timestamp.u64;
		frame.bit_info = 0;

		/* Call callback */
		status = (*strm->rec_cb)(strm->user_data, &frame);
		if (status != PJ_SUCCESS)
		    break;

		/* Increment position. */
		if (++wmme_strm->dwBufIdx >= wmme_strm->dwMaxBufIdx)
		    wmme_strm->dwBufIdx = 0;
		wmme_strm->timestamp.u64 += strm->samples_per_frame / 
					    strm->channel_count;
	    }
	}
    }

    PJ_LOG(5,(THIS_FILE, "WMME: thread stopping.."));
    return 0;
}


/* API: create stream */
static pj_status_t factory_create_stream(pjmedia_aud_dev_factory *f,
					 const pjmedia_aud_param *param,
					 pjmedia_aud_rec_cb rec_cb,
					 pjmedia_aud_play_cb play_cb,
					 void *user_data,
					 pjmedia_aud_stream **p_aud_strm)
{
    struct wmme_factory *wf = (struct wmme_factory*)f;
    pj_pool_t *pool;
    struct wmme_stream *strm;
    pj_status_t status;

    /* Can only support 16bits per sample */
    PJ_ASSERT_RETURN(param->bits_per_sample == BITS_PER_SAMPLE, PJ_EINVAL);

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(wf->pf, "wmme-dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct wmme_stream);
    strm->dir = param->dir;
    strm->play_id = param->play_id;
    strm->rec_id = param->rec_id;
    strm->pool = pool;
    strm->rec_cb = rec_cb;
    strm->play_cb = play_cb;
    strm->user_data = user_data;
    strm->clock_rate = param->clock_rate;
    strm->samples_per_frame = param->samples_per_frame;
    strm->bits_per_sample = param->bits_per_sample;
    strm->channel_count = param->channel_count;
    strm->buffer = pj_pool_alloc(pool, 
				 param->samples_per_frame * BYTES_PER_SAMPLE);
    if (!strm->buffer) {
	pj_pool_release(pool);
	return PJ_ENOMEM;
    }

    /* Create player stream */
    if (param->dir & PJMEDIA_DIR_PLAYBACK) {
	unsigned buf_count;

	if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY)
	    strm->play_strm.latency_ms = param->output_latency_ms;
	else
	    strm->play_strm.latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

	buf_count = strm->play_strm.latency_ms * param->clock_rate * 
		    param->channel_count / param->samples_per_frame / 1000;

	status = init_player_stream(wf, strm->pool,
				    &strm->play_strm,
				    param->play_id,
				    param->clock_rate,
				    param->channel_count,
				    param->samples_per_frame,
				    buf_count);

	if (status != PJ_SUCCESS) {
	    stream_destroy(&strm->base);
	    return status;
	}
    }

    /* Create capture stream */
    if (param->dir & PJMEDIA_DIR_CAPTURE) {
	unsigned buf_count;

	if (param->flags & PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY)
	    strm->rec_strm.latency_ms = param->input_latency_ms;
	else
	    strm->rec_strm.latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;

	buf_count = strm->rec_strm.latency_ms * param->clock_rate * 
		    param->channel_count / param->samples_per_frame / 1000;

	status = init_capture_stream(wf, strm->pool,
				     &strm->rec_strm,
				     param->rec_id,
				     param->clock_rate,
				     param->channel_count,
				     param->samples_per_frame,
				     buf_count);

	if (status != PJ_SUCCESS) {
	    stream_destroy(&strm->base);
	    return status;
	}
    }

    /* Create the stop event */
    strm->thread_quit_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (strm->thread_quit_event == NULL) {
	status = pj_get_os_error();
	stream_destroy(&strm->base);
	return status;
    }

    /* Create and start the thread */
    status = pj_thread_create(pool, "wmme", &wmme_dev_thread, strm, 0, 0, 
			      &strm->thread);
    if (status != PJ_SUCCESS) {
	stream_destroy(&strm->base);
	return status;
    }

    /* Done */
    strm->base.op = &stream_op;
    *p_aud_strm = &strm->base;

    return PJ_SUCCESS;
}

/* API: Get stream info. */
static pj_status_t stream_get_param(pjmedia_aud_stream *s,
				    pjmedia_aud_param *pi)
{
    struct wmme_stream *strm = (struct wmme_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_bzero(pi, sizeof(*pi));
    pi->dir = strm->dir;
    pi->play_id = strm->play_id;
    pi->rec_id = strm->rec_id;
    pi->clock_rate = strm->clock_rate;
    pi->channel_count = strm->channel_count;
    pi->samples_per_frame = strm->samples_per_frame;
    pi->bits_per_sample = strm->bits_per_sample;
    
    if (pi->dir & PJMEDIA_DIR_CAPTURE) {
	pi->flags |= PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY;
	pi->input_latency_ms = strm->rec_strm.latency_ms;
    }

    if (pi->dir & PJMEDIA_DIR_PLAYBACK) {
	/* TODO: report the actual latency? */
	pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
	pi->output_latency_ms = strm->play_strm.latency_ms;
    }

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t stream_get_cap(pjmedia_aud_stream *s,
				  pjmedia_aud_dev_cap cap,
				  void *pval)
{
    struct wmme_stream *strm = (struct wmme_stream*)s;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap==PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY && 
	(strm->dir & PJMEDIA_DIR_CAPTURE)) 
    {
	/* Recording latency */
	*(unsigned*)pval = strm->rec_strm.latency_ms;
	return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY  && 
	       (strm->dir & PJMEDIA_DIR_PLAYBACK))
    {
	/* Playback latency */
	*(unsigned*)pval = strm->play_strm.latency_ms;
	return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING &&
	       strm->play_strm.hWave.Out)
    {
	/* Output volume setting */
	DWORD dwVol;
	MMRESULT mr;

	mr = waveOutGetVolume(strm->play_strm.hWave.Out, &dwVol);
	if (mr != MMSYSERR_NOERROR) {
	    return CONVERT_MM_ERROR(mr);
	}

	dwVol &= 0xFFFF;
	*(unsigned*)pval = (dwVol * 100) / 0xFFFF;
	return PJ_SUCCESS;
    } else {
	return PJ_ENOTSUP;
    }
}

/* API: set capability */
static pj_status_t stream_set_cap(pjmedia_aud_stream *s,
				  pjmedia_aud_dev_cap cap,
				  const void *pval)
{
    struct wmme_stream *strm = (struct wmme_stream*)s;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING &&
	strm->play_strm.hWave.Out)
    {
	/* Output volume setting */
	DWORD dwVol;
	MMRESULT mr;

	dwVol = ((*(unsigned*)pval) * 0xFFFF) / 100;
	dwVol |= (dwVol << 16);

	mr = waveOutSetVolume(strm->play_strm.hWave.Out, dwVol);
	return (mr==MMSYSERR_NOERROR)? PJ_SUCCESS : CONVERT_MM_ERROR(mr);
    } else {
	return PJ_ENOTSUP;
    }

    return PJ_ENOTSUP;
}

/* API: Start stream. */
static pj_status_t stream_start(pjmedia_aud_stream *strm)
{
    struct wmme_stream *stream = (struct wmme_stream*)strm;
    MMRESULT mr;

    if (stream->play_strm.hWave.Out != NULL)
    {
	mr = waveOutRestart(stream->play_strm.hWave.Out);
	if (mr != MMSYSERR_NOERROR) {
	    return CONVERT_MM_ERROR(mr);
	}
	PJ_LOG(5,(THIS_FILE, "WMME playback stream started"));
    }

    if (stream->rec_strm.hWave.In != NULL)
    {
	mr = waveInStart(stream->rec_strm.hWave.In);
	if (mr != MMSYSERR_NOERROR) {
	    return CONVERT_MM_ERROR(mr);
	}
	PJ_LOG(5,(THIS_FILE, "WMME capture stream started"));
    }

    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t stream_stop(pjmedia_aud_stream *strm)
{
    struct wmme_stream *stream = (struct wmme_stream*)strm;
    MMRESULT mr;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    if (stream->play_strm.hWave.Out != NULL)
    {
	mr = waveOutPause(stream->play_strm.hWave.Out);
	if (mr != MMSYSERR_NOERROR) {
	    return CONVERT_MM_ERROR(mr);
	}
	PJ_LOG(5,(THIS_FILE, "Stopped WMME playback stream"));
    }

    if (stream->rec_strm.hWave.In != NULL)
    {
	mr = waveInStop(stream->rec_strm.hWave.In);
	if (mr != MMSYSERR_NOERROR) {
	    return CONVERT_MM_ERROR(mr);
	}
	PJ_LOG(5,(THIS_FILE, "Stopped WMME capture stream"));
    }

    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t stream_destroy(pjmedia_aud_stream *strm)
{
    struct wmme_stream *stream = (struct wmme_stream*)strm;
    unsigned i;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    stream_stop(strm);

    if (stream->thread)
    {
	SetEvent(stream->thread_quit_event);
	pj_thread_join(stream->thread);
	pj_thread_destroy(stream->thread);
	stream->thread = NULL;
    }

    /* Unprepare the headers and close the play device */
    if (stream->play_strm.hWave.Out)
    {
	waveOutReset(stream->play_strm.hWave.Out);
	for (i = 0; i < stream->play_strm.dwMaxBufIdx; ++i)
	    waveOutUnprepareHeader(stream->play_strm.hWave.Out, 
				   &(stream->play_strm.WaveHdr[i]),
				   sizeof(WAVEHDR));
	waveOutClose(stream->play_strm.hWave.Out);
	stream->play_strm.hWave.Out = NULL;
    }

    /* Close the play event */
    if (stream->play_strm.hEvent)
    {
	CloseHandle(stream->play_strm.hEvent);
	stream->play_strm.hEvent = NULL;
    }

    /* Unprepare the headers and close the record device */
    if (stream->rec_strm.hWave.In)
    {
	waveInReset(stream->rec_strm.hWave.In);
	for (i = 0; i < stream->play_strm.dwMaxBufIdx; ++i)
	    waveInUnprepareHeader(stream->rec_strm.hWave.In, 
				  &(stream->rec_strm.WaveHdr[i]),
				  sizeof(WAVEHDR));
	waveInClose(stream->rec_strm.hWave.In);
	stream->rec_strm.hWave.In = NULL;
    }

    /* Close the record event */
    if (stream->rec_strm.hEvent)
    {
	CloseHandle(stream->rec_strm.hEvent);
	stream->rec_strm.hEvent = NULL;
    }

    pj_pool_release(stream->pool);

    return PJ_SUCCESS;
}

