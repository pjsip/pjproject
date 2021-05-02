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
/* This file is the implementation of Android JNI audio device.
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

#if defined(PJMEDIA_AUDIO_DEV_HAS_ANDROID_JNI) && \
    PJMEDIA_AUDIO_DEV_HAS_ANDROID_JNI != 0

#include <jni.h>
#include <sys/resource.h>
#include <sys/system_properties.h>

#define THIS_FILE	"android_jni_dev.c"
#define DRIVER_NAME	"Android JNI"

struct android_aud_factory
{
    pjmedia_aud_dev_factory base;
    pj_pool_factory        *pf;
    pj_pool_t              *pool;
};

/* 
 * Sound stream descriptor.
 * This struct may be used for both unidirectional or bidirectional sound
 * streams.
 */
struct android_aud_stream
{
    pjmedia_aud_stream  base;
    pj_pool_t          *pool;
    pj_str_t            name;
    pjmedia_dir         dir;
    pjmedia_aud_param   param;

    int                 bytes_per_sample;
    pj_uint32_t         samples_per_sec;
    unsigned            samples_per_frame;
    int                 channel_count;
    void               *user_data;
    pj_bool_t           quit_flag;
    pj_bool_t           running;

    /* Record */
    jobject             record;
    jclass              record_class;
    unsigned            rec_buf_size;
    pjmedia_aud_rec_cb  rec_cb;
    pj_bool_t           rec_thread_exited;
    pj_thread_t        *rec_thread;
    pj_sem_t           *rec_sem;
    pj_timestamp        rec_timestamp;    

    /* Track */
    jobject             track;
    jclass              track_class;
    unsigned            play_buf_size;
    pjmedia_aud_play_cb play_cb;
    pj_bool_t           play_thread_exited;
    pj_thread_t        *play_thread;
    pj_sem_t           *play_sem;
    pj_timestamp        play_timestamp;
};

/* Factory prototypes */
static pj_status_t android_init(pjmedia_aud_dev_factory *f);
static pj_status_t android_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t android_refresh(pjmedia_aud_dev_factory *f);
static unsigned android_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t android_get_dev_info(pjmedia_aud_dev_factory *f,
                                        unsigned index,
                                        pjmedia_aud_dev_info *info);
static pj_status_t android_default_param(pjmedia_aud_dev_factory *f,
                                         unsigned index,
                                         pjmedia_aud_param *param);
static pj_status_t android_create_stream(pjmedia_aud_dev_factory *f,
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

static pjmedia_aud_dev_factory_op android_op =
{
    &android_init,
    &android_destroy,
    &android_get_dev_count,
    &android_get_dev_info,
    &android_default_param,
    &android_create_stream,
    &android_refresh
};

static pjmedia_aud_stream_op android_strm_op =
{
    &strm_get_param,
    &strm_get_cap,
    &strm_set_cap,
    &strm_start,
    &strm_stop,
    &strm_destroy
};

PJ_DECL(pj_bool_t) pj_jni_attach_jvm(JNIEnv **jni_env);
PJ_DECL(void) pj_jni_dettach_jvm(pj_bool_t attached);
#define attach_jvm(jni_env)	pj_jni_attach_jvm(jni_env)
#define detach_jvm(attached)	pj_jni_dettach_jvm(attached)
#define THREAD_PRIORITY_AUDIO		-16
#define THREAD_PRIORITY_URGENT_AUDIO	-19


static int AndroidRecorderCallback(void *userData)
{
    struct android_aud_stream *stream = (struct android_aud_stream *)userData;
    jmethodID read_method=0, record_method=0, stop_method=0;
    int size = stream->rec_buf_size;
    jbyteArray inputBuffer;
    jbyte *buf;
    JNIEnv *jni_env = 0;
    pj_bool_t attached = attach_jvm(&jni_env);
    
    PJ_ASSERT_RETURN(jni_env, 0);
    
    if (!stream->record) {
        goto on_return;
    }

    PJ_LOG(5, (THIS_FILE, "Recorder thread started"));

    /* Get methods ids */
    read_method = (*jni_env)->GetMethodID(jni_env, stream->record_class, 
                                          "read", "([BII)I");
    record_method = (*jni_env)->GetMethodID(jni_env, stream->record_class,
                                            "startRecording", "()V");
    stop_method = (*jni_env)->GetMethodID(jni_env, stream->record_class,
                                          "stop", "()V");
    if (read_method==0 || record_method==0 || stop_method==0) {
        PJ_LOG(3, (THIS_FILE, "Unable to get recording methods"));
        goto on_return;
    }
    
    /* Create a buffer for frames read */
    inputBuffer = (*jni_env)->NewByteArray(jni_env, size);
    if (inputBuffer == 0) {
        PJ_LOG(3, (THIS_FILE, "Unable to allocate input buffer"));
        goto on_return;
    }
    
    /* Start recording */
    pj_thread_set_prio(NULL, THREAD_PRIORITY_URGENT_AUDIO);
    (*jni_env)->CallVoidMethod(jni_env, stream->record, record_method);
    
    while (!stream->quit_flag) {
        pjmedia_frame frame;
        pj_status_t status;
        int bytesRead;
        
        if (!stream->running) {
            (*jni_env)->CallVoidMethod(jni_env, stream->record, stop_method);
            pj_sem_wait(stream->rec_sem);
            if (stream->quit_flag)
                break;
            (*jni_env)->CallVoidMethod(jni_env, stream->record, record_method);
        }
        
        bytesRead = (*jni_env)->CallIntMethod(jni_env, stream->record,
                                              read_method, inputBuffer,
                                              0, size);
        if (bytesRead <= 0 || bytesRead != size) {
            PJ_LOG (4, (THIS_FILE, "Record thread : error %d reading data",
                                   bytesRead));
            continue;
        }

        buf = (*jni_env)->GetByteArrayElements(jni_env, inputBuffer, 0);
        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.size =  size;
        frame.bit_info = 0;
        frame.buf = (void *)buf;
        frame.timestamp.u64 = stream->rec_timestamp.u64;

        status = (*stream->rec_cb)(stream->user_data, &frame);
        (*jni_env)->ReleaseByteArrayElements(jni_env, inputBuffer, buf,
        				     JNI_ABORT);
	if (status != PJ_SUCCESS || stream->quit_flag)
	    break;

        stream->rec_timestamp.u64 += stream->param.samples_per_frame /
                                     stream->param.channel_count;
    }

    (*jni_env)->DeleteLocalRef(jni_env, inputBuffer);
    
on_return:
    detach_jvm(attached);
    PJ_LOG(5, (THIS_FILE, "Recorder thread stopped"));
    stream->rec_thread_exited = 1;

    return 0;
}


static int AndroidTrackCallback(void *userData)
{
    struct android_aud_stream *stream = (struct android_aud_stream*) userData;
    jmethodID write_method=0, play_method=0, stop_method=0, flush_method=0;
    int size = stream->play_buf_size;
    jbyteArray outputBuffer;
    jbyte *buf;
    JNIEnv *jni_env = 0;
    pj_bool_t attached = attach_jvm(&jni_env);
    
    if (!stream->track) {
        goto on_return;
    }

    PJ_LOG(5, (THIS_FILE, "Playback thread started"));

    /* Get methods ids */
    write_method = (*jni_env)->GetMethodID(jni_env, stream->track_class,
                                           "write", "([BII)I");
    play_method = (*jni_env)->GetMethodID(jni_env, stream->track_class,
                                          "play", "()V");
    stop_method = (*jni_env)->GetMethodID(jni_env, stream->track_class,
                                          "stop", "()V");
    flush_method = (*jni_env)->GetMethodID(jni_env, stream->track_class,
                                           "flush", "()V");
    if (write_method==0 || play_method==0 || stop_method==0 ||
        flush_method==0)
    {
        PJ_LOG(3, (THIS_FILE, "Unable to get audio track methods"));
        goto on_return;
    }

    outputBuffer = (*jni_env)->NewByteArray(jni_env, size);
    if (outputBuffer == 0) {
        PJ_LOG(3, (THIS_FILE, "Unable to allocate output buffer"));
        goto on_return;
    }
    buf = (*jni_env)->GetByteArrayElements(jni_env, outputBuffer, 0);

    /* Start playing */
    pj_thread_set_prio(NULL, THREAD_PRIORITY_URGENT_AUDIO);
    (*jni_env)->CallVoidMethod(jni_env, stream->track, play_method);

    while (!stream->quit_flag) {
        pjmedia_frame frame;
        pj_status_t status;
        int bytesWritten;

        if (!stream->running) {
            (*jni_env)->CallVoidMethod(jni_env, stream->track, stop_method);
            (*jni_env)->CallVoidMethod(jni_env, stream->track, flush_method);
            pj_sem_wait(stream->play_sem);
            if (stream->quit_flag)
                break;
            (*jni_env)->CallVoidMethod(jni_env, stream->track, play_method);
        }
        
        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.size = size;
        frame.buf = (void *)buf;
        frame.timestamp.u64 = stream->play_timestamp.u64;
        frame.bit_info = 0;
        
        status = (*stream->play_cb)(stream->user_data, &frame);
        if (status != PJ_SUCCESS)
            continue;
        
        if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
            pj_bzero(frame.buf, frame.size);
        
        (*jni_env)->ReleaseByteArrayElements(jni_env, outputBuffer, buf,
        				     JNI_COMMIT);

        /* Write to the device output. */
        bytesWritten = (*jni_env)->CallIntMethod(jni_env, stream->track,
                                                 write_method, outputBuffer,
                                                 0, size);
        if (bytesWritten <= 0 || bytesWritten != size) {
            PJ_LOG(4, (THIS_FILE, "Player thread: Error %d writing data",
                                  bytesWritten));
            continue;
        }

        stream->play_timestamp.u64 += stream->param.samples_per_frame /
                                      stream->param.channel_count;
    };
    
    (*jni_env)->ReleaseByteArrayElements(jni_env, outputBuffer, buf, 0);
    (*jni_env)->DeleteLocalRef(jni_env, outputBuffer);
    
on_return:
    detach_jvm(attached);
    PJ_LOG(5, (THIS_FILE, "Player thread stopped"));
    stream->play_thread_exited = 1;
    
    return 0;
}

/*
 * Init Android audio driver.
 */
pjmedia_aud_dev_factory* pjmedia_android_factory(pj_pool_factory *pf)
{
    struct android_aud_factory *f;
    pj_pool_t *pool;
    
    pool = pj_pool_create(pf, "androidjni", 256, 256, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct android_aud_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &android_op;
    
    return &f->base;
}

/* API: Init factory */
static pj_status_t android_init(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    
    PJ_LOG(4, (THIS_FILE, "Android JNI sound library initialized"));
    
    return PJ_SUCCESS;
}


/* API: refresh the list of devices */
static pj_status_t android_refresh(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
}


/* API: Destroy factory */
static pj_status_t android_destroy(pjmedia_aud_dev_factory *f)
{
    struct android_aud_factory *pa = (struct android_aud_factory*)f;
    pj_pool_t *pool;
    
    PJ_LOG(4, (THIS_FILE, "Android JNI sound library shutting down.."));
    
    pool = pa->pool;
    pa->pool = NULL;
    pj_pool_release(pool);
    
    return PJ_SUCCESS;
}

/* API: Get device count. */
static unsigned android_get_dev_count(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return 1;
}

/* API: Get device info. */
static pj_status_t android_get_dev_info(pjmedia_aud_dev_factory *f,
                                        unsigned index,
                                        pjmedia_aud_dev_info *info)
{
    PJ_UNUSED_ARG(f);
    
    pj_bzero(info, sizeof(*info));
    
    pj_ansi_strcpy(info->name, "Android JNI");
    info->default_samples_per_sec = 8000;
    info->caps = PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING |
    		 PJMEDIA_AUD_DEV_CAP_INPUT_SOURCE;
    info->input_count = 1;
    info->output_count = 1;
    info->routes = PJMEDIA_AUD_DEV_ROUTE_CUSTOM;
    
    return PJ_SUCCESS;
}

/* API: fill in with default parameter. */
static pj_status_t android_default_param(pjmedia_aud_dev_factory *f,
                                         unsigned index,
                                         pjmedia_aud_param *param)
{
    pjmedia_aud_dev_info adi;
    pj_status_t status;
    
    status = android_get_dev_info(f, index, &adi);
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
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
    
    return PJ_SUCCESS;
}

/* API: create stream */
static pj_status_t android_create_stream(pjmedia_aud_dev_factory *f,
                                         const pjmedia_aud_param *param,
                                         pjmedia_aud_rec_cb rec_cb,
                                         pjmedia_aud_play_cb play_cb,
                                         void *user_data,
                                         pjmedia_aud_stream **p_aud_strm)
{
    struct android_aud_factory *pa = (struct android_aud_factory*)f;
    pj_pool_t *pool;
    struct android_aud_stream *stream;
    pj_status_t status = PJ_SUCCESS;
    int state = 0;
    int buffSize, inputBuffSizePlay = 0, inputBuffSizeRec = 0;
    int channelInCfg, channelOutCfg, sampleFormat;
    jmethodID constructor_method=0, bufsize_method = 0;
    jmethodID method_id = 0;
    jclass jcl;
    JNIEnv *jni_env = 0;
    pj_bool_t attached;
    
    PJ_ASSERT_RETURN(param->channel_count >= 1 && param->channel_count <= 2,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(param->bits_per_sample==8 || param->bits_per_sample==16,
                     PJ_EINVAL);
    PJ_ASSERT_RETURN(play_cb && rec_cb && p_aud_strm, PJ_EINVAL);

    pool = pj_pool_create(pa->pf, "jnistrm", 1024, 1024, NULL);
    if (!pool)
        return PJ_ENOMEM;

    PJ_LOG(4, (THIS_FILE, "Creating Android JNI stream"));
    
    stream = PJ_POOL_ZALLOC_T(pool, struct android_aud_stream);
    stream->pool = pool;
    pj_strdup2_with_null(pool, &stream->name, "JNI stream");
    stream->dir = param->dir;
    pj_memcpy(&stream->param, param, sizeof(*param));
    stream->user_data = user_data;
    stream->rec_cb = rec_cb;
    stream->play_cb = play_cb;
    buffSize = stream->param.samples_per_frame*stream->param.bits_per_sample/8;
    stream->rec_buf_size = stream->play_buf_size = buffSize;
    channelInCfg = (param->channel_count == 1)? 16 /*CHANNEL_IN_MONO*/:
                   12 /*CHANNEL_IN_STEREO*/;
    channelOutCfg = (param->channel_count == 1)? 4 /*CHANNEL_OUT_MONO*/:
                    12 /*CHANNEL_OUT_STEREO*/;
    sampleFormat = (param->bits_per_sample == 8)? 3 /*ENCODING_PCM_8BIT*/:
                   2 /*ENCODING_PCM_16BIT*/;

    attached = attach_jvm(&jni_env);

    if (stream->dir & PJMEDIA_DIR_CAPTURE) {
        /* Find audio record class and create global ref */
        jcl = (*jni_env)->FindClass(jni_env, "android/media/AudioRecord");
        if (jcl == NULL) {
            PJ_LOG(3, (THIS_FILE, "Unable to find audio record class"));
            status = PJMEDIA_EAUD_SYSERR;
            goto on_error;
        }
        stream->record_class = (jclass)(*jni_env)->NewGlobalRef(jni_env, jcl);
        (*jni_env)->DeleteLocalRef(jni_env, jcl);
        if (stream->record_class == 0) {
            status = PJ_ENOMEM;
            goto on_error;
        }

        /* Get the min buffer size function */
        bufsize_method = (*jni_env)->GetStaticMethodID(jni_env,
                                                       stream->record_class,
                                                       "getMinBufferSize",
                                                       "(III)I");
        if (bufsize_method == 0) {
            PJ_LOG(3, (THIS_FILE, "Unable to find audio record "
                                  "getMinBufferSize() method"));
            status = PJMEDIA_EAUD_SYSERR;
            goto on_error;
        }

        inputBuffSizeRec = (*jni_env)->CallStaticIntMethod(jni_env,
                                                           stream->record_class,
                                                           bufsize_method,
                                                           param->clock_rate,
                                                           channelInCfg,
                                                           sampleFormat);
        if (inputBuffSizeRec <= 0) {
            PJ_LOG(3, (THIS_FILE, "Unsupported audio record params"));
            status = PJMEDIA_EAUD_INIT;
            goto on_error;
        }
    }
    
    if (stream->dir & PJMEDIA_DIR_PLAYBACK) {
        /* Find audio track class and create global ref */
        jcl = (*jni_env)->FindClass(jni_env, "android/media/AudioTrack");
        if (jcl == NULL) {
            PJ_LOG(3, (THIS_FILE, "Unable to find audio track class"));
            status = PJMEDIA_EAUD_SYSERR;
            goto on_error;
        }
        stream->track_class = (jclass)(*jni_env)->NewGlobalRef(jni_env, jcl);
        (*jni_env)->DeleteLocalRef(jni_env, jcl);
        if (stream->track_class == 0) {
            status = PJ_ENOMEM;
            goto on_error;
        }

        /* Get the min buffer size function */
        bufsize_method = (*jni_env)->GetStaticMethodID(jni_env,
                                                       stream->track_class,
                                                       "getMinBufferSize",
                                                       "(III)I");
        if (bufsize_method == 0) {
            PJ_LOG(3, (THIS_FILE, "Unable to find audio track "
                                  "getMinBufferSize() method"));
            status = PJMEDIA_EAUD_SYSERR;
            goto on_error;
        }
        
        inputBuffSizePlay = (*jni_env)->CallStaticIntMethod(jni_env,
                                                            stream->track_class,
                                                            bufsize_method,
                                                            param->clock_rate,
                                                            channelOutCfg,
                                                            sampleFormat);
        if (inputBuffSizePlay <= 0) {
            PJ_LOG(3, (THIS_FILE, "Unsupported audio track params"));
            status = PJMEDIA_EAUD_INIT;
            goto on_error;
        }
    }
    
    if (stream->dir & PJMEDIA_DIR_CAPTURE) {
        jthrowable exc;
        jobject record_obj;
        int mic_source = 0; /* DEFAULT: default audio source */

	if ((param->flags & PJMEDIA_AUD_DEV_CAP_INPUT_SOURCE) &&
	    (param->input_route & PJMEDIA_AUD_DEV_ROUTE_CUSTOM))
	{
    	    mic_source = param->input_route & ~PJMEDIA_AUD_DEV_ROUTE_CUSTOM;
    	}

        /* Get pointer to the constructor */
        constructor_method = (*jni_env)->GetMethodID(jni_env,
                                                     stream->record_class,
                                                     "<init>", "(IIIII)V");
        if (constructor_method == 0) {
            PJ_LOG(3, (THIS_FILE, "Unable to find audio record's constructor"));
            status = PJMEDIA_EAUD_SYSERR;
            goto on_error;
        }
        
        if (mic_source == 0) {
            /* Android-L (android-21) removes __system_property_get
             * from the NDK.
	     */
	    /*           
	    char sdk_version[PROP_VALUE_MAX];
            pj_str_t pj_sdk_version;
            int sdk_v;

            __system_property_get("ro.build.version.sdk", sdk_version);
            pj_sdk_version = pj_str(sdk_version);
            sdk_v = pj_strtoul(&pj_sdk_version);
            if (sdk_v > 10)
            */
            mic_source = 7; /* VOICE_COMMUNICATION */
        }
        PJ_LOG(4, (THIS_FILE, "Using audio input source : %d", mic_source));
        
        do {
            record_obj =  (*jni_env)->NewObject(jni_env,
                                                stream->record_class,
                                                constructor_method,
                                                mic_source, 
                                                param->clock_rate,
                                                channelInCfg,
                                                sampleFormat,
                                                inputBuffSizeRec);
            if (record_obj == 0) {
                PJ_LOG(3, (THIS_FILE, "Unable to create audio record object"));
                status = PJMEDIA_EAUD_INIT;
                goto on_error;
            }
        
            exc = (*jni_env)->ExceptionOccurred(jni_env);
            if (exc) {
                (*jni_env)->ExceptionDescribe(jni_env);
                (*jni_env)->ExceptionClear(jni_env);
                PJ_LOG(3, (THIS_FILE, "Failure in audio record's constructor"));
                if (mic_source == 0) {
                    status = PJMEDIA_EAUD_INIT;
                    goto on_error;
                }
                mic_source = 0;
                PJ_LOG(4, (THIS_FILE, "Trying the default audio source."));
                continue;
            }

            /* Check state */
            method_id = (*jni_env)->GetMethodID(jni_env, stream->record_class,
                                                "getState", "()I");
            if (method_id == 0) {
                PJ_LOG(3, (THIS_FILE, "Unable to find audio record getState() "
                                      "method"));
                status = PJMEDIA_EAUD_SYSERR;
                goto on_error;
            }
            state = (*jni_env)->CallIntMethod(jni_env, record_obj, method_id);
            if (state == 0) { /* STATE_UNINITIALIZED */
                PJ_LOG(3, (THIS_FILE, "Failure in initializing audio record."));
                if (mic_source == 0) {
                    status = PJMEDIA_EAUD_INIT;
                    goto on_error;
                }
                mic_source = 0;
                PJ_LOG(4, (THIS_FILE, "Trying the default audio source."));
            }
        } while (state == 0);
        
        stream->record = (*jni_env)->NewGlobalRef(jni_env, record_obj);
        if (stream->record == 0) {
            jmethodID release_method=0;
            
            PJ_LOG(3, (THIS_FILE, "Unable to create audio record global ref."));            
            release_method = (*jni_env)->GetMethodID(jni_env, 
                                                     stream->record_class,
                                                     "release", "()V");
            (*jni_env)->CallVoidMethod(jni_env, record_obj, release_method);
            
            status = PJMEDIA_EAUD_INIT;
            goto on_error;
        }

        status = pj_sem_create(stream->pool, NULL, 0, 1, &stream->rec_sem);
        if (status != PJ_SUCCESS)
            goto on_error;
        
        status = pj_thread_create(stream->pool, "android_recorder",
                                  AndroidRecorderCallback, stream, 0, 0,
                                  &stream->rec_thread);
        if (status != PJ_SUCCESS)
            goto on_error;

        PJ_LOG(4, (THIS_FILE, "Audio record initialized successfully."));
    }
    
    if (stream->dir & PJMEDIA_DIR_PLAYBACK) {
        jthrowable exc;
        jobject track_obj;
        
        /* Get pointer to the constructor */
        constructor_method = (*jni_env)->GetMethodID(jni_env,
                                                     stream->track_class,
                                                     "<init>", "(IIIIII)V");
        if (constructor_method == 0) {
            PJ_LOG(3, (THIS_FILE, "Unable to find audio track's constructor."));
            status = PJMEDIA_EAUD_SYSERR;
            goto on_error;
        }
        
        track_obj = (*jni_env)->NewObject(jni_env,
                                          stream->track_class,
                                          constructor_method,
                                          0, /* STREAM_VOICE_CALL */
                                          param->clock_rate,
                                          channelOutCfg,
                                          sampleFormat,
                                          inputBuffSizePlay,
                                          1 /* MODE_STREAM */);
        if (track_obj == 0) {
            PJ_LOG(3, (THIS_FILE, "Unable to create audio track object."));
            status = PJMEDIA_EAUD_INIT;
            goto on_error;
        }
        
        exc = (*jni_env)->ExceptionOccurred(jni_env);
        if (exc) {
            (*jni_env)->ExceptionDescribe(jni_env);
            (*jni_env)->ExceptionClear(jni_env);
            PJ_LOG(3, (THIS_FILE, "Failure in audio track's constructor"));
            status = PJMEDIA_EAUD_INIT;
            goto on_error;
        }
        
        stream->track = (*jni_env)->NewGlobalRef(jni_env, track_obj);
        if (stream->track == 0) {
            jmethodID release_method=0;
        	
            release_method = (*jni_env)->GetMethodID(jni_env, 
                                                     stream->track_class,
                                                     "release", "()V");
            (*jni_env)->CallVoidMethod(jni_env, track_obj, release_method);
            
            PJ_LOG(3, (THIS_FILE, "Unable to create audio track's global ref"));
            status = PJMEDIA_EAUD_INIT;
            goto on_error;
        }
        
        /* Check state */
        method_id = (*jni_env)->GetMethodID(jni_env, stream->track_class,
                                            "getState", "()I");
        if (method_id == 0) {
            PJ_LOG(3, (THIS_FILE, "Unable to find audio track getState() "
                                  "method"));
            status = PJMEDIA_EAUD_SYSERR;
            goto on_error;
        }
        state = (*jni_env)->CallIntMethod(jni_env, stream->track,
                                          method_id);
        if (state == 0) { /* STATE_UNINITIALIZED */
            PJ_LOG(3, (THIS_FILE, "Failure in initializing audio track."));
            status = PJMEDIA_EAUD_INIT;
            goto on_error;
        }

        status = pj_sem_create(stream->pool, NULL, 0, 1, &stream->play_sem);
        if (status != PJ_SUCCESS)
            goto on_error;
        
        status = pj_thread_create(stream->pool, "android_track",
                                  AndroidTrackCallback, stream, 0, 0,
                                  &stream->play_thread);
        if (status != PJ_SUCCESS)
            goto on_error;
        
        PJ_LOG(4, (THIS_FILE, "Audio track initialized successfully."));
    }

    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) {
	strm_set_cap(&stream->base, PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
                     &param->output_vol);
    }
    
    /* Done */
    stream->base.op = &android_strm_op;
    *p_aud_strm = &stream->base;
    
    detach_jvm(attached);
    
    return PJ_SUCCESS;
    
on_error:
    detach_jvm(attached);
    strm_destroy(&stream->base);
    return status;
}

/* API: Get stream parameters */
static pj_status_t strm_get_param(pjmedia_aud_stream *s,
                                  pjmedia_aud_param *pi)
{
    struct android_aud_stream *strm = (struct android_aud_stream*)s;
    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);
    pj_memcpy(pi, &strm->param, sizeof(*pi));

    if (strm_get_cap(s, PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
                     &pi->output_vol) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
    }    
    
    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t strm_get_cap(pjmedia_aud_stream *s,
                                pjmedia_aud_dev_cap cap,
                                void *pval)
{
    struct android_aud_stream *strm = (struct android_aud_stream*)s;
    pj_status_t status = PJMEDIA_EAUD_INVCAP;
    
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);
    
    if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING &&
	(strm->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
    }
    
    return status;
}

/* API: set capability */
static pj_status_t strm_set_cap(pjmedia_aud_stream *s,
                                pjmedia_aud_dev_cap cap,
                                const void *value)
{
    struct android_aud_stream *stream = (struct android_aud_stream*)s;
    JNIEnv *jni_env = 0;
    pj_bool_t attached;
    
    PJ_ASSERT_RETURN(s && value, PJ_EINVAL);
    
    if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING &&
	(stream->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
        if (stream->track) {
            jmethodID vol_method = 0;
            int retval = 0;
            float vol = *(int *)value;
            
            attached = attach_jvm(&jni_env);
            
            vol_method = (*jni_env)->GetMethodID(jni_env, stream->track_class,
                                                 "setStereoVolume", "(FF)I");
            if (vol_method) {
                retval = (*jni_env)->CallIntMethod(jni_env, stream->track,
                                                   vol_method,
                                                   vol/100, vol/100);
            }
            
            detach_jvm(attached);
            
            if (vol_method && retval == 0)
                return PJ_SUCCESS;
        }
    }
    
    return PJMEDIA_EAUD_INVCAP;
}

/* API: start stream. */
static pj_status_t strm_start(pjmedia_aud_stream *s)
{
    struct android_aud_stream *stream = (struct android_aud_stream*)s;    
    
    if (!stream->running) {
        stream->running = PJ_TRUE;
        if (stream->record)
            pj_sem_post(stream->rec_sem);
        if (stream->track)
            pj_sem_post(stream->play_sem);
    }
    
    PJ_LOG(4, (THIS_FILE, "Android JNI stream started"));
    
    return PJ_SUCCESS;
}

/* API: stop stream. */
static pj_status_t strm_stop(pjmedia_aud_stream *s)
{
    struct android_aud_stream *stream = (struct android_aud_stream*)s;

    if (!stream->running)
        return PJ_SUCCESS;
    
    stream->running = PJ_FALSE;
    PJ_LOG(4,(THIS_FILE, "Android JNI stream stopped"));
    
    return PJ_SUCCESS;
}

/* API: destroy stream. */
static pj_status_t strm_destroy(pjmedia_aud_stream *s)
{
    struct android_aud_stream *stream = (struct android_aud_stream*)s;
    JNIEnv *jni_env = 0;
    jmethodID release_method=0;
    pj_bool_t attached;
    
    PJ_LOG(4,(THIS_FILE, "Destroying Android JNI stream..."));

    stream->quit_flag = PJ_TRUE;
    
    /* Stop the stream */
    strm_stop(s);
    
    attached = attach_jvm(&jni_env);

    if (stream->record){
        if (stream->rec_thread) {
            pj_sem_post(stream->rec_sem);
            pj_thread_join(stream->rec_thread);
            pj_thread_destroy(stream->rec_thread);
            stream->rec_thread = NULL;
        }
        
        if (stream->rec_sem) {
            pj_sem_destroy(stream->rec_sem);
            stream->rec_sem = NULL;
        }
        if (stream->record_class) {
            release_method = (*jni_env)->GetMethodID(jni_env, 
                                                     stream->record_class,
                                                     "release", "()V");
            (*jni_env)->CallVoidMethod(jni_env, stream->record,
                                       release_method);
        }
        (*jni_env)->DeleteGlobalRef(jni_env, stream->record);
        stream->record = NULL;
        PJ_LOG(4, (THIS_FILE, "Audio record released"));
    }
    if (stream->record_class) {
    	(*jni_env)->DeleteGlobalRef(jni_env, stream->record_class);
    	stream->record_class = NULL;
    }
    
    if (stream->track) {
        if (stream->play_thread) {
            pj_sem_post(stream->play_sem);
            pj_thread_join(stream->play_thread);
            pj_thread_destroy(stream->play_thread);
            stream->play_thread = NULL;
        }
        
        if (stream->play_sem) {
            pj_sem_destroy(stream->play_sem);
            stream->play_sem = NULL;
        }
        if (stream->track_class) {
            release_method = (*jni_env)->GetMethodID(jni_env, 
                                                     stream->track_class,
                                                     "release", "()V");
            (*jni_env)->CallVoidMethod(jni_env, stream->track, 
                                       release_method);
        }
        (*jni_env)->DeleteGlobalRef(jni_env, stream->track);
        stream->track = NULL;        
        PJ_LOG(4, (THIS_FILE, "Audio track released"));
    }
    if (stream->track_class) {
    	(*jni_env)->DeleteGlobalRef(jni_env, stream->track_class);
    	stream->track_class = NULL;
    }

    pj_pool_release(stream->pool);
    PJ_LOG(4, (THIS_FILE, "Android JNI stream destroyed"));
    
    detach_jvm(attached);
    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_AUDIO_DEV_HAS_ANDROID_JNI */
