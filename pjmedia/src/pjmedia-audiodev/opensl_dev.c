/* $Id$ */
/* 
 * Copyright (C) 2012-2012 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2010-2012 Regis Montoya (aka r3gis - www.r3gis.fr)
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
/* This file is the implementation of Android OpenSL ES audio device.
 * The original code was originally part of CSipSimple
 * (http://code.google.com/p/csipsimple/) and was kindly donated
 * by Regis Montoya.
 */

#include <pjmedia-audiodev/audiodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/string.h>
#include <pjmedia/errno.h>

#if defined(PJMEDIA_AUDIO_DEV_HAS_OPENSL) && PJMEDIA_AUDIO_DEV_HAS_OPENSL != 0

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#define THIS_FILE	"opensl_dev.c"
#define DRIVER_NAME	"OpenSL"

#define NUM_BUFFERS 2

struct opensl_aud_factory
{
    pjmedia_aud_dev_factory  base;
    pj_pool_factory         *pf;
    pj_pool_t               *pool;
    
    SLObjectItf              engineObject;
    SLEngineItf              engineEngine;
    SLObjectItf              outputMixObject;
};

/*
 * Sound stream descriptor.
 * This struct may be used for both unidirectional or bidirectional sound
 * streams.
 */
struct opensl_aud_stream
{
    pjmedia_aud_stream  base;
    pj_pool_t          *pool;
    pj_str_t            name;
    pjmedia_dir         dir;
    pjmedia_aud_param   param;
    
    void               *user_data;
    pj_bool_t           quit_flag;
    pjmedia_aud_rec_cb  rec_cb;
    pjmedia_aud_play_cb play_cb;

    pj_bool_t		rec_thread_initialized;
    pj_thread_desc	rec_thread_desc;
    pj_thread_t        *rec_thread;
    
    pj_bool_t		play_thread_initialized;
    pj_thread_desc	play_thread_desc;
    pj_thread_t        *play_thread;
    
    /* Player */
    SLObjectItf         playerObj;
    SLPlayItf           playerPlay;
    unsigned            playerBufferSize;
    char               *playerBuffer[NUM_BUFFERS];
    int                 playerBufIdx;
    
    /* Recorder */
    SLObjectItf         recordObj;
    SLRecordItf         recordRecord;
    unsigned            recordBufferSize;
    char               *recordBuffer[NUM_BUFFERS];
    int                 recordBufIdx;

    SLAndroidSimpleBufferQueueItf playerBufQ;
    SLAndroidSimpleBufferQueueItf recordBufQ;
};

/* Factory prototypes */
static pj_status_t opensl_init(pjmedia_aud_dev_factory *f);
static pj_status_t opensl_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t opensl_refresh(pjmedia_aud_dev_factory *f);
static unsigned opensl_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t opensl_get_dev_info(pjmedia_aud_dev_factory *f,
                                       unsigned index,
                                       pjmedia_aud_dev_info *info);
static pj_status_t opensl_default_param(pjmedia_aud_dev_factory *f,
                                        unsigned index,
                                        pjmedia_aud_param *param);
static pj_status_t opensl_create_stream(pjmedia_aud_dev_factory *f,
                                        const pjmedia_aud_param *param,
                                        pjmedia_aud_rec_cb rec_cb,
                                        pjmedia_aud_play_cb play_cb,
                                        void *user_data,
                                        pjmedia_aud_stream **p_aud_strm);

/* Stream prototypes */
static pj_status_t strm_get_param(pjmedia_aud_stream *strm,
                                  pjmedia_aud_param *param);
static pj_status_t strm_get_cap(pjmedia_aud_stream *strm,
                                pjmedia_aud_dev_cap cap,
                                void *value);
static pj_status_t strm_set_cap(pjmedia_aud_stream *strm,
                                pjmedia_aud_dev_cap cap,
                                const void *value);
static pj_status_t strm_start(pjmedia_aud_stream *strm);
static pj_status_t strm_stop(pjmedia_aud_stream *strm);
static pj_status_t strm_destroy(pjmedia_aud_stream *strm);

static pjmedia_aud_dev_factory_op opensl_op =
{
    &opensl_init,
    &opensl_destroy,
    &opensl_get_dev_count,
    &opensl_get_dev_info,
    &opensl_default_param,
    &opensl_create_stream,
    &opensl_refresh
};

static pjmedia_aud_stream_op opensl_strm_op =
{
    &strm_get_param,
    &strm_get_cap,
    &strm_set_cap,
    &strm_start,
    &strm_stop,
    &strm_destroy
};

/* This callback is called every time a buffer finishes playing. */
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    struct opensl_aud_stream *stream = (struct opensl_aud_stream*) context;
    SLresult result;
    int status;

    pj_assert(context != NULL);
    pj_assert(bq == stream->playerBufQ);

    if (stream->play_thread_initialized == 0 || !pj_thread_is_registered())
    {
	pj_bzero(stream->play_thread_desc, sizeof(pj_thread_desc));
	status = pj_thread_register("opensl_play", stream->play_thread_desc,
				    &stream->play_thread);
	stream->play_thread_initialized = 1;
	PJ_LOG(5, (THIS_FILE, "Player thread started"));
    }
    
    if (!stream->quit_flag) {
        pjmedia_frame frame;
        char * buf;
        
        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = buf = stream->playerBuffer[stream->playerBufIdx++];
        frame.size = stream->playerBufferSize;
        frame.bit_info = 0;
        
        status = (*stream->play_cb)(stream->user_data, &frame);
        if (status != PJ_SUCCESS || frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
            pj_bzero(buf, stream->playerBufferSize);
        
        result = (*bq)->Enqueue(bq, buf, stream->playerBufferSize);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Unable to enqueue next player buffer !!! %d",
                                  result));
        }
        
        stream->playerBufIdx %= NUM_BUFFERS;
    }
}

/* This callback handler is called every time a buffer finishes recording */
void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    struct opensl_aud_stream *stream = (struct opensl_aud_stream*) context;
    SLresult result;
    int status;

    pj_assert(context != NULL);
    pj_assert(bq == stream->recordBufQ);

    if (stream->rec_thread_initialized == 0 || !pj_thread_is_registered())
    {
	pj_bzero(stream->rec_thread_desc, sizeof(pj_thread_desc));
	status = pj_thread_register("opensl_rec", stream->rec_thread_desc,
				    &stream->rec_thread);
	stream->rec_thread_initialized = 1;
	PJ_LOG(5, (THIS_FILE, "Recorder thread started")); 
    }
    
    if (!stream->quit_flag) {
        pjmedia_frame frame;
        char *buf;
        
        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = buf = stream->recordBuffer[stream->recordBufIdx++];
        frame.size = stream->recordBufferSize;
        frame.timestamp.u64 = 0;
        frame.bit_info = 0;
        
        status = (*stream->rec_cb)(stream->user_data, &frame);
        
        /* And now enqueue next buffer */
        result = (*bq)->Enqueue(bq, buf, stream->recordBufferSize);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Unable to enqueue next record buffer !!! %d",
                                  result));
        }
        
        stream->recordBufIdx %= NUM_BUFFERS;
    }
}

pj_status_t opensl_to_pj_error(SLresult code)
{
    switch(code) {
	case SL_RESULT_SUCCESS:
            return PJ_SUCCESS;
	case SL_RESULT_PRECONDITIONS_VIOLATED:
	case SL_RESULT_PARAMETER_INVALID:
	case SL_RESULT_CONTENT_CORRUPTED:
	case SL_RESULT_FEATURE_UNSUPPORTED:
            return PJMEDIA_EAUD_INVOP;
	case SL_RESULT_MEMORY_FAILURE:
	case SL_RESULT_BUFFER_INSUFFICIENT:
            return PJ_ENOMEM;
	case SL_RESULT_RESOURCE_ERROR:
	case SL_RESULT_RESOURCE_LOST:
	case SL_RESULT_CONTROL_LOST:
            return PJMEDIA_EAUD_NOTREADY;
	case SL_RESULT_CONTENT_UNSUPPORTED:
            return PJ_ENOTSUP;
	default:
            return PJMEDIA_EAUD_ERR;
    }
}

/* Init Android audio driver. */
pjmedia_aud_dev_factory* pjmedia_opensl_factory(pj_pool_factory *pf)
{
    struct opensl_aud_factory *f;
    pj_pool_t *pool;
    
    pool = pj_pool_create(pf, "opensles", 256, 256, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct opensl_aud_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &opensl_op;
    
    return &f->base;
}

/* API: Init factory */
static pj_status_t opensl_init(pjmedia_aud_dev_factory *f)
{
    struct opensl_aud_factory *pa = (struct opensl_aud_factory*)f;
    SLresult result;    
    
    /* Create engine */
    result = slCreateEngine(&pa->engineObject, 0, NULL, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "Cannot create engine %d ", result));
        return opensl_to_pj_error(result);
    }
    
    /* Realize the engine */
    result = (*pa->engineObject)->Realize(pa->engineObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "Cannot realize engine"));
        opensl_destroy(f);
        return opensl_to_pj_error(result);
    }
    
    /* Get the engine interface, which is needed in order to create
     * other objects.
     */
    result = (*pa->engineObject)->GetInterface(pa->engineObject,
                                               SL_IID_ENGINE,
                                               &pa->engineEngine);
    if (result != SL_RESULT_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "Cannot get engine interface"));
        opensl_destroy(f);
        return opensl_to_pj_error(result);
    }
    
    /* Create output mix */
    result = (*pa->engineEngine)->CreateOutputMix(pa->engineEngine,
                                                  &pa->outputMixObject,
                                                  0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "Cannot create output mix"));
        opensl_destroy(f);
        return opensl_to_pj_error(result);
    }
    
    /* Realize the output mix */
    result = (*pa->outputMixObject)->Realize(pa->outputMixObject,
                                             SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        PJ_LOG(3, (THIS_FILE, "Cannot realize output mix"));
        opensl_destroy(f);
        return opensl_to_pj_error(result);
    }
    
    PJ_LOG(4,(THIS_FILE, "OpenSL sound library initialized"));
    return PJ_SUCCESS;
}

/* API: Destroy factory */
static pj_status_t opensl_destroy(pjmedia_aud_dev_factory *f)
{
    struct opensl_aud_factory *pa = (struct opensl_aud_factory*)f;
    pj_pool_t *pool;
    
    PJ_LOG(4,(THIS_FILE, "OpenSL sound library shutting down.."));
    
    /* Destroy Output Mix object */
    if (pa->outputMixObject) {
        (*pa->outputMixObject)->Destroy(pa->outputMixObject);
        pa->outputMixObject = NULL;
    }
    
    /* Destroy engine object, and invalidate all associated interfaces */
    if (pa->engineObject) {
        (*pa->engineObject)->Destroy(pa->engineObject);
        pa->engineObject = NULL;
        pa->engineEngine = NULL;
    }
    
    pool = pa->pool;
    pa->pool = NULL;
    pj_pool_release(pool);
    
    return PJ_SUCCESS;
}

/* API: refresh the list of devices */
static pj_status_t opensl_refresh(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
}

/* API: Get device count. */
static unsigned opensl_get_dev_count(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return 1;
}

/* API: Get device info. */
static pj_status_t opensl_get_dev_info(pjmedia_aud_dev_factory *f,
                                       unsigned index,
                                       pjmedia_aud_dev_info *info)
{
    PJ_UNUSED_ARG(f);

    pj_bzero(info, sizeof(*info));
    
    pj_ansi_strcpy(info->name, "OpenSL ES Audio");
    info->default_samples_per_sec = 16000;
    info->caps = PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING |
                 PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
    info->input_count = 1;
    info->output_count = 1;
    
    return PJ_SUCCESS;
}

/* API: fill in with default parameter. */
static pj_status_t opensl_default_param(pjmedia_aud_dev_factory *f,
                                        unsigned index,
                                        pjmedia_aud_param *param)
{
    
    pjmedia_aud_dev_info adi;
    pj_status_t status;
    
    status = opensl_get_dev_info(f, index, &adi);
    if (status != PJ_SUCCESS)
	return status;
    
    pj_bzero(param, sizeof(*param));
    if (adi.input_count && adi.output_count) {
        param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
        param->rec_id = index;
        param->play_id = index;
    } else if (adi.input_count) {
        param->dir = PJMEDIA_DIR_CAPTURE;
        param->rec_id = index;
        param->play_id = PJMEDIA_AUD_INVALID_DEV;
    } else if (adi.output_count) {
        param->dir = PJMEDIA_DIR_PLAYBACK;
        param->play_id = index;
        param->rec_id = PJMEDIA_AUD_INVALID_DEV;
    } else {
        return PJMEDIA_EAUD_INVDEV;
    }
    
    param->clock_rate = adi.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = adi.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 16;
    param->flags = adi.caps;
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
    
    return PJ_SUCCESS;
}

/* API: create stream */
static pj_status_t opensl_create_stream(pjmedia_aud_dev_factory *f,
                                        const pjmedia_aud_param *param,
                                        pjmedia_aud_rec_cb rec_cb,
                                        pjmedia_aud_play_cb play_cb,
                                        void *user_data,
                                        pjmedia_aud_stream **p_aud_strm)
{
    /* Audio sink for recorder and audio source for player */
    SLDataLocator_AndroidSimpleBufferQueue loc_bq =
        { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, NUM_BUFFERS };
    struct opensl_aud_factory *pa = (struct opensl_aud_factory*)f;
    pj_pool_t *pool;
    struct opensl_aud_stream *stream;
    pj_status_t status = PJ_SUCCESS;
    int i, bufferSize;
    SLresult result;
    SLDataFormat_PCM format_pcm;
    
    /* Only supports for mono channel for now */
    PJ_ASSERT_RETURN(param->channel_count == 1, PJ_EINVAL);
    PJ_ASSERT_RETURN(play_cb && rec_cb && p_aud_strm, PJ_EINVAL);

    PJ_LOG(4,(THIS_FILE, "Creating OpenSL stream"));
    
    pool = pj_pool_create(pa->pf, "openslstrm", 1024, 1024, NULL);
    if (!pool)
        return PJ_ENOMEM;
    
    stream = PJ_POOL_ZALLOC_T(pool, struct opensl_aud_stream);
    stream->pool = pool;
    pj_strdup2_with_null(pool, &stream->name, "OpenSL");
    stream->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    pj_memcpy(&stream->param, param, sizeof(*param));
    stream->user_data = user_data;
    stream->rec_cb = rec_cb;
    stream->play_cb = play_cb;
    bufferSize = param->samples_per_frame * param->bits_per_sample / 8;

    /* Configure audio PCM format */
    format_pcm.formatType = SL_DATAFORMAT_PCM;
    format_pcm.numChannels = param->channel_count;
    /* Here samples per sec should be supported else we will get an error */
    format_pcm.samplesPerSec  = (SLuint32) param->clock_rate * 1000;
    format_pcm.bitsPerSample = (SLuint16) param->bits_per_sample;
    format_pcm.containerSize = (SLuint16) param->bits_per_sample;
    format_pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
    format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

    if (stream->dir & PJMEDIA_DIR_PLAYBACK) {
        /* Audio source */
        SLDataSource audioSrc = {&loc_bq, &format_pcm};
        /* Audio sink */
        SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX,
                                              pa->outputMixObject};
        SLDataSink audioSnk = {&loc_outmix, NULL};
        /* Audio interface */
        const SLInterfaceID ids[2] = {SL_IID_BUFFERQUEUE,
                                      SL_IID_VOLUME/*,
                                      SL_IID_ANDROIDCONFIGURATION*/};
        const SLboolean req[2] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
        
        /* Create audio player */
        result = (*pa->engineEngine)->CreateAudioPlayer(pa->engineEngine,
                                                        &stream->playerObj,
                                                        &audioSrc, &audioSnk,
                                                        2, ids, req);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot create audio player: %d", result));
            goto on_error;
        }

        /*
        SLAndroidConfigurationItf playerConfig;
        SLint32 streamType = SL_ANDROID_STREAM_VOICE;
        result = (*stream->playerObj)->GetInterface(stream->playerObj,
                                                    SL_IID_ANDROIDCONFIGURATION,
                                                    &playerConfig);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(4, (THIS_FILE, "Cannot get android configuration iface"));
        }
        result = (*playerConfig)->SetConfiguration(playerConfig,
                                                   SL_ANDROID_KEY_STREAM_TYPE,
                                                   &streamType,
                                                   sizeof(SLint32));
        */
        
        /* Realize the player */
        result = (*stream->playerObj)->Realize(stream->playerObj,
                                               SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot realize player : %d", result));
            goto on_error;
        }
        
        /* Get the play interface */
        result = (*stream->playerObj)->GetInterface(stream->playerObj,
                                                    SL_IID_PLAY,
                                                    &stream->playerPlay);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot get play interface"));
            goto on_error;
        }
        
        /* Get the buffer queue interface */
        result = (*stream->playerObj)->GetInterface(stream->playerObj,
                                                    SL_IID_BUFFERQUEUE,
                                                    &stream->playerBufQ);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot get buffer queue interface"));
            goto on_error;
        }
        
        /* Register callback on the buffer queue */
        result = (*stream->playerBufQ)->RegisterCallback(stream->playerBufQ,
                                                         bqPlayerCallback,
                                                         (void *)stream);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot register player callback"));
            goto on_error;
        }
        
        stream->playerBufferSize = bufferSize;
        for (i = 0; i < NUM_BUFFERS; i++) {
            stream->playerBuffer[i] = (char *)
                                      pj_pool_alloc(stream->pool,
                                                    stream->playerBufferSize);
        }
    }

    if (stream->dir & PJMEDIA_DIR_CAPTURE) {
        /* Audio source */
        SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE,
                                          SL_IODEVICE_AUDIOINPUT,
                                          SL_DEFAULTDEVICEID_AUDIOINPUT,
                                          NULL};
        SLDataSource audioSrc = {&loc_dev, NULL};
        /* Audio sink */
        SLDataSink audioSnk = {&loc_bq, &format_pcm};
        /* Audio interface */
        const SLInterfaceID ids[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[1] = {SL_BOOLEAN_TRUE};
        
        /* Create audio recorder
         * (requires the RECORD_AUDIO permission)
         */
        result = (*pa->engineEngine)->CreateAudioRecorder(pa->engineEngine,
                                                          &stream->recordObj,
                                                          &audioSrc, &audioSnk,
                                                          1, ids, req);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot create recorder: %d", result));
            goto on_error;
        }

        /*
        SLAndroidConfigurationItf recorderConfig;
        result = (*stream->recordObj)->GetInterface(stream->recordObj,
                                                    SL_IID_ANDROIDCONFIGURATION,
                                                    &recorderConfig);
        if (result == SL_RESULT_SUCCESS) {
            SLint32 streamType = SL_ANDROID_RECORDING_PRESET_GENERIC;
            char sdk_version[PROP_VALUE_MAX];
            pj_str_t pj_sdk_version;
            int sdk_v;

            __system_property_get("ro.build.version.sdk", sdk_version);
            pj_sdk_version = pj_str(sdk_version);
            sdk_v = pj_strtoul(&pj_sdk_version);
            if (sdk_v >= 10)
                streamType = 0x7;
        
            PJ_LOG(4, (THIS_FILE, "We have a stream type %d SDK : %d",
                                  streamType, sdk_v));
            result = (*recorderConfig)->SetConfiguration(
                         recorderConfig, SL_ANDROID_KEY_RECORDING_PRESET,
                         &streamType, sizeof(SLint32));
        } else {
            PJ_LOG(4, (THIS_FILE, "Cannot get recorder config interface"));
        }
         */
        
        /* Realize the recorder */
        result = (*stream->recordObj)->Realize(stream->recordObj,
                                               SL_BOOLEAN_FALSE);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot realize recorder : %d", result));
            goto on_error;
        }
        
        /* Get the record interface */
        result = (*stream->recordObj)->GetInterface(stream->recordObj,
                                                    SL_IID_RECORD,
                                                    &stream->recordRecord);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot get record interface"));
            goto on_error;
        }
        
        /* Get the buffer queue interface */
        result = (*stream->recordObj)->GetInterface(
                     stream->recordObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                     &stream->recordBufQ);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot get recorder buffer queue iface"));
            goto on_error;
        }
        
        /* Register callback on the buffer queue */
        result = (*stream->recordBufQ)->RegisterCallback(stream->recordBufQ,
                                                         bqRecorderCallback, 
                                                         (void *) stream);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot register recorder callback"));
            goto on_error;
        }
        
        stream->recordBufferSize = bufferSize;
        for (i = 0; i < NUM_BUFFERS; i++) {
            stream->recordBuffer[i] = (char *)
                                      pj_pool_alloc(stream->pool,
                                                    stream->recordBufferSize);
        }

    }
    
    /* Done */
    stream->base.op = &opensl_strm_op;
    *p_aud_strm = &stream->base;
    return PJ_SUCCESS;
    
on_error:
    strm_destroy(&stream->base);
    return status;
}

/* API: Get stream parameters */
static pj_status_t strm_get_param(pjmedia_aud_stream *s,
                                  pjmedia_aud_param *pi)
{
    struct opensl_aud_stream *strm = (struct opensl_aud_stream*)s;
    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);
    pj_memcpy(pi, &strm->param, sizeof(*pi));
    
    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t strm_get_cap(pjmedia_aud_stream *s,
                                pjmedia_aud_dev_cap cap,
                                void *pval)
{
    struct opensl_aud_stream *strm = (struct opensl_aud_stream*)s;    
    pj_status_t status = PJMEDIA_EAUD_INVCAP;
    
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);
    
    PJ_UNUSED_ARG(strm);
    
    switch (cap) {
        case PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING:
            status = PJ_SUCCESS;
            break;
        case PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING:
            status = PJ_SUCCESS;
            break;
        default:
            break;
    }
    
    return status;
}

/* API: set capability */
static pj_status_t strm_set_cap(pjmedia_aud_stream *strm,
                                pjmedia_aud_dev_cap cap,
                                const void *value)
{
    PJ_UNUSED_ARG(strm);
    PJ_UNUSED_ARG(cap);
    PJ_UNUSED_ARG(value);

    return PJMEDIA_EAUD_INVCAP;
}

/* API: start stream. */
static pj_status_t strm_start(pjmedia_aud_stream *s)
{
    struct opensl_aud_stream *stream = (struct opensl_aud_stream*)s;
    int i;
    SLresult result = SL_RESULT_SUCCESS;
    
    PJ_LOG(4, (THIS_FILE, "Starting %s stream..", stream->name.ptr));
    stream->quit_flag = 0;

    if (stream->recordBufQ && stream->recordRecord) {
        /* Enqueue an empty buffer to be filled by the recorder
         * (for streaming recording, we need to enqueue at least 2 empty
         * buffers to start things off)
         */
        for (i = 0; i < NUM_BUFFERS; i++) {
            result = (*stream->recordBufQ)->Enqueue(stream->recordBufQ,
                                                stream->recordBuffer[i],
                                                stream->recordBufferSize);
            /* The most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
             * which for this code would indicate a programming error
             */
            pj_assert(result == SL_RESULT_SUCCESS);
        }
        
        result = (*stream->recordRecord)->SetRecordState(
                     stream->recordRecord, SL_RECORDSTATE_RECORDING);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot start recorder"));
            goto on_error;
        }
    }
    
    if (stream->playerPlay && stream->playerBufQ) {
        /* Set the player's state to playing */
        result = (*stream->playerPlay)->SetPlayState(stream->playerPlay,
                                                     SL_PLAYSTATE_PLAYING);
        if (result != SL_RESULT_SUCCESS) {
            PJ_LOG(3, (THIS_FILE, "Cannot start player"));
            goto on_error;
        }
        
        for (i = 0; i < NUM_BUFFERS; i++) {
            pj_bzero(stream->playerBuffer[i], stream->playerBufferSize/100);
            result = (*stream->playerBufQ)->Enqueue(stream->playerBufQ,
                                                stream->playerBuffer[i],
                                                stream->playerBufferSize/100);
            pj_assert(result == SL_RESULT_SUCCESS);
        }
    }
    
    PJ_LOG(4, (THIS_FILE, "%s stream started", stream->name.ptr));
    return PJ_SUCCESS;
    
on_error:
    if (result != SL_RESULT_SUCCESS)
        strm_stop(&stream->base);
    return opensl_to_pj_error(result);
}

/* API: stop stream. */
static pj_status_t strm_stop(pjmedia_aud_stream *s)
{
    struct opensl_aud_stream *stream = (struct opensl_aud_stream*)s;
    
    if (stream->quit_flag)
        return PJ_SUCCESS;
    
    PJ_LOG(4, (THIS_FILE, "Stopping stream"));
    
    stream->quit_flag = 1;    
    
    if (stream->recordBufQ && stream->recordRecord) {
        /* Stop recording and clear buffer queue */
        (*stream->recordRecord)->SetRecordState(stream->recordRecord,
                                                  SL_RECORDSTATE_STOPPED);
        (*stream->recordBufQ)->Clear(stream->recordBufQ);
    }

    if (stream->playerBufQ && stream->playerPlay) {
        /* Wait until the PCM data is done playing, the buffer queue callback
         will continue to queue buffers until the entire PCM data has been
         played. This is indicated by waiting for the count member of the
         SLBufferQueueState to go to zero.
         */
/*      
        SLresult result;
        SLAndroidSimpleBufferQueueState state;

        result = (*stream->playerBufQ)->GetState(stream->playerBufQ, &state);
        while (state.count) {
            (*stream->playerBufQ)->GetState(stream->playerBufQ, &state);
        } */
        /* Stop player */
        (*stream->playerPlay)->SetPlayState(stream->playerPlay,
                                            SL_PLAYSTATE_STOPPED);
    }

    PJ_LOG(4,(THIS_FILE, "OpenSL stream stopped"));
    
    return PJ_SUCCESS;
    
}

/* API: destroy stream. */
static pj_status_t strm_destroy(pjmedia_aud_stream *s)
{    
    struct opensl_aud_stream *stream = (struct opensl_aud_stream*)s;
    
    /* Stop the stream */
    strm_stop(s);
    
    if (stream->playerObj) {
        /* Destroy the player */
        (*stream->playerObj)->Destroy(stream->playerObj);
        stream->playerObj = NULL;
        stream->playerPlay = NULL;
        stream->playerBufQ = NULL;
    }
    
    if (stream->recordObj) {
        /* Destroy the recorder */
        (*stream->recordObj)->Destroy(stream->recordObj);
        stream->recordObj = NULL;
        stream->recordRecord = NULL;
        stream->recordBufQ = NULL;
    }
    
    pj_pool_release(stream->pool);
    PJ_LOG(4, (THIS_FILE, "OpenSL stream destroyed"));
    
    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_AUDIO_DEV_HAS_OPENSL */
