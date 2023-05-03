/*
 * Copyright (C) 2009-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2007-2009 Keystream AB and Konftel AB, All rights reserved.
 *                         Author: <dan.aberg@keystream.se>
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
#include <pjmedia_audiodev.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pjmedia/errno.h>

#if defined(PJMEDIA_AUDIO_DEV_HAS_ALSA) && PJMEDIA_AUDIO_DEV_HAS_ALSA

#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>
#include <alsa/asoundlib.h>


#define THIS_FILE                       "alsa_dev.c"
#define ALSA_DEVICE_NAME                "plughw:%d,%d"
#define ALSASOUND_PLAYBACK              1
#define ALSASOUND_CAPTURE               2
#define MAX_SOUND_CARDS                 5
#define MAX_SOUND_DEVICES_PER_CARD      5
#define MAX_DEVICES                     32
#define MAX_MIX_NAME_LEN                64 

/* Set to 1 to enable tracing */
#define ENABLE_TRACING                  0

#if ENABLE_TRACING
#       define TRACE_(expr)             PJ_LOG(5,expr)
#else
#       define TRACE_(expr)
#endif

/*
 * Factory prototypes
 */
static pj_status_t alsa_factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t alsa_factory_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t alsa_factory_refresh(pjmedia_aud_dev_factory *f);
static unsigned    alsa_factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t alsa_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                             unsigned index,
                                             pjmedia_aud_dev_info *info);
static pj_status_t alsa_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param);
static pj_status_t alsa_factory_create_stream(pjmedia_aud_dev_factory *f,
                                              const pjmedia_aud_param *param,
                                              pjmedia_aud_rec_cb rec_cb,
                                              pjmedia_aud_play_cb play_cb,
                                              void *user_data,
                                              pjmedia_aud_stream **p_strm);

/*
 * Stream prototypes
 */
static pj_status_t alsa_stream_get_param(pjmedia_aud_stream *strm,
                                         pjmedia_aud_param *param);
static pj_status_t alsa_stream_get_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       void *value);
static pj_status_t alsa_stream_set_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       const void *value);
static pj_status_t alsa_stream_start(pjmedia_aud_stream *strm);
static pj_status_t alsa_stream_stop(pjmedia_aud_stream *strm);
static pj_status_t alsa_stream_destroy(pjmedia_aud_stream *strm);


struct alsa_factory
{
    pjmedia_aud_dev_factory      base;
    pj_pool_factory             *pf;
    pj_pool_t                   *pool;
    pj_pool_t                   *base_pool;

    unsigned                     dev_cnt;
    pjmedia_aud_dev_info         devs[MAX_DEVICES];
    char                         pb_mixer_name[MAX_MIX_NAME_LEN];
    char                         cap_mixer_name[MAX_MIX_NAME_LEN];
};

struct alsa_stream
{
    pjmedia_aud_stream   base;

    /* Common */
    pj_pool_t           *pool;
    struct alsa_factory *af;
    void                *user_data;
    pjmedia_aud_param    param;         /* Running parameter            */
    int                  rec_id;        /* Capture device id            */
    int                  quit;

    /* Playback */
    snd_pcm_t           *pb_pcm;
    snd_pcm_uframes_t    pb_frames;     /* samples_per_frame            */
    pjmedia_aud_play_cb  pb_cb;
    unsigned             pb_buf_size;
    char                *pb_buf;
    pj_thread_t         *pb_thread;

    /* Capture */
    snd_pcm_t           *ca_pcm;
    snd_pcm_uframes_t    ca_frames;     /* samples_per_frame            */
    pjmedia_aud_rec_cb   ca_cb;
    unsigned             ca_buf_size;
    char                *ca_buf;
    pj_thread_t         *ca_thread;
};

static pjmedia_aud_dev_factory_op alsa_factory_op =
{
    &alsa_factory_init,
    &alsa_factory_destroy,
    &alsa_factory_get_dev_count,
    &alsa_factory_get_dev_info,
    &alsa_factory_default_param,
    &alsa_factory_create_stream,
    &alsa_factory_refresh
};

static pjmedia_aud_stream_op alsa_stream_op =
{
    &alsa_stream_get_param,
    &alsa_stream_get_cap,
    &alsa_stream_set_cap,
    &alsa_stream_start,
    &alsa_stream_stop,
    &alsa_stream_destroy
};

#if ENABLE_TRACING==0
static void null_alsa_error_handler (const char *file,
                                int line,
                                const char *function,
                                int err,
                                const char *fmt,
                                ...)
{
    PJ_UNUSED_ARG(file);
    PJ_UNUSED_ARG(line);
    PJ_UNUSED_ARG(function);
    PJ_UNUSED_ARG(err);
    PJ_UNUSED_ARG(fmt);
}
#endif

static void alsa_error_handler (const char *file,
                                int line,
                                const char *function,
                                int err,
                                const char *fmt,
                                ...)
{
    char err_msg[128];
    int index, len;
    va_list arg;

#ifndef NDEBUG
    index = snprintf (err_msg, sizeof(err_msg), "ALSA lib %s:%i:(%s) ",
                      file, line, function);
#else
    index = snprintf (err_msg, sizeof(err_msg), "ALSA lib: ");
#endif
    if (index < 1 || index >= (int)sizeof(err_msg)) {
        index = sizeof(err_msg)-1;
        err_msg[index] = '\0';
        goto print_msg;
    }

    va_start (arg, fmt);
    if (index < (int)sizeof(err_msg)-1) {
        len = vsnprintf( err_msg+index, sizeof(err_msg)-index, fmt, arg);
        if (len < 1 || len >= (int)sizeof(err_msg)-index)
            len = sizeof(err_msg)-index-1;
        index += len;
        err_msg[index] = '\0';
    }
    va_end(arg);
    if (err && index < (int)sizeof(err_msg)-1) {
        len = snprintf( err_msg+index, sizeof(err_msg)-index, ": %s",
                        snd_strerror(err));
        if (len < 1 || len >= (int)sizeof(err_msg)-index)
            len = sizeof(err_msg)-index-1;
        index += len;
        err_msg[index] = '\0';
    }
print_msg:
    PJ_LOG (4,(THIS_FILE, "%s", err_msg));
}


static pj_status_t add_dev (struct alsa_factory *af, const char *dev_name)
{
    pjmedia_aud_dev_info *adi;
    snd_pcm_t* pcm;
    int pb_result, ca_result;

    if (af->dev_cnt >= PJ_ARRAY_SIZE(af->devs))
        return PJ_ETOOMANY;

    adi = &af->devs[af->dev_cnt];

    TRACE_((THIS_FILE, "add_dev (%s): Enter", dev_name));

    /* Try to open the device in playback mode */
    pb_result = snd_pcm_open (&pcm, dev_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (pb_result >= 0) {
        TRACE_((THIS_FILE, "Try to open the device for playback - success"));
        snd_pcm_close (pcm);
    } else {
        TRACE_((THIS_FILE, "Try to open the device for playback - failure"));
    }

    /* Try to open the device in capture mode */
    ca_result = snd_pcm_open (&pcm, dev_name, SND_PCM_STREAM_CAPTURE, 0);
    if (ca_result >= 0) {
        TRACE_((THIS_FILE, "Try to open the device for capture - success"));
        snd_pcm_close (pcm);
    } else {
        TRACE_((THIS_FILE, "Try to open the device for capture - failure"));
    }

    /* Check if the device could be opened in playback or capture mode */
    if (pb_result<0 && ca_result<0) {
        TRACE_((THIS_FILE, "Unable to open sound device %s, setting "
                           "in/out channel count to 0", dev_name));
        /* Set I/O channel counts to 0 to indicate unavailable device */
        adi->output_count = 0;
        adi->input_count =  0;
    }

    /* Reset device info */
    pj_bzero(adi, sizeof(*adi));

    /* Set device name */
    pj_ansi_strxcpy(adi->name, dev_name, sizeof(adi->name));

    /* Check the number of playback channels */
    adi->output_count = (pb_result>=0) ? 1 : 0;

    /* Check the number of capture channels */
    adi->input_count = (ca_result>=0) ? 1 : 0;

    /* Set the default sample rate */
    adi->default_samples_per_sec = 8000;

    /* Driver name */
    pj_ansi_strxcpy(adi->driver, "ALSA", sizeof(adi->driver));

    ++af->dev_cnt;

    PJ_LOG (5,(THIS_FILE, "Added sound device %s", adi->name));

    return PJ_SUCCESS;
}

static void get_mixer_name(struct alsa_factory *af)
{
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;

    if (snd_mixer_open(&handle, 0) < 0)
        return;

    if (snd_mixer_attach(handle, "default") < 0) {
        snd_mixer_close(handle);
        return;
    }

    if (snd_mixer_selem_register(handle, NULL, NULL) < 0) {
        snd_mixer_close(handle);
        return;
    }

    if (snd_mixer_load(handle) < 0) {
        snd_mixer_close(handle);
        return;
    }

    for (elem = snd_mixer_first_elem(handle); elem;
         elem = snd_mixer_elem_next(elem))
    {
        const char* elemname;
        elemname = snd_mixer_selem_get_name(elem);
        if (snd_mixer_selem_is_active(elem))
        {
            if (snd_mixer_selem_has_playback_volume(elem))
            {
                pj_ansi_strxcpy(af->pb_mixer_name, elemname,
                                     sizeof(af->pb_mixer_name));
                TRACE_((THIS_FILE, "Playback mixer name: %s",
                        af->pb_mixer_name));
            }
            if (snd_mixer_selem_has_capture_volume(elem))
            {
                pj_ansi_strxcpy(af->cap_mixer_name, elemname,
                                     sizeof(af->cap_mixer_name));
                TRACE_((THIS_FILE, "Capture mixer name: %s",
                        af->cap_mixer_name));
            }
         }
    }
    snd_mixer_close(handle);
}


/* Create ALSA audio driver. */
pjmedia_aud_dev_factory* pjmedia_alsa_factory(pj_pool_factory *pf)
{
    struct alsa_factory *af;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "alsa_aud_base", 256, 256, NULL);
    af = PJ_POOL_ZALLOC_T(pool, struct alsa_factory);
    af->pf = pf;
    af->base_pool = pool;
    af->base.op = &alsa_factory_op;

    return &af->base;
}


/* API: init factory */
static pj_status_t alsa_factory_init(pjmedia_aud_dev_factory *f)
{
    pj_status_t status = alsa_factory_refresh(f);
    if (PJ_SUCCESS != status)
        return status;

    PJ_LOG(4,(THIS_FILE, "ALSA initialized"));
    return PJ_SUCCESS;
}


/* API: destroy factory */
static pj_status_t alsa_factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct alsa_factory *af = (struct alsa_factory*)f;

    if (af->pool)
        pj_pool_release(af->pool);

    if (af->base_pool) {
        pj_pool_t *pool = af->base_pool;
        af->base_pool = NULL;
        pj_pool_release(pool);
    }

    /* Restore handler */
    snd_lib_error_set_handler(NULL);

    return PJ_SUCCESS;
}


/* API: refresh the device list */
static pj_status_t alsa_factory_refresh(pjmedia_aud_dev_factory *f)
{
    struct alsa_factory *af = (struct alsa_factory*)f;
    char **hints, **n;
    int err;

    TRACE_((THIS_FILE, "pjmedia_snd_init: Enumerate sound devices"));

    if (af->pool != NULL) {
        pj_pool_release(af->pool);
        af->pool = NULL;
    }

    af->pool = pj_pool_create(af->pf, "alsa_aud", 256, 256, NULL);
    af->dev_cnt = 0;

    /* Enumerate sound devices */
    err = snd_device_name_hint(-1, "pcm", (void***)&hints);
    if (err != 0)
        return PJMEDIA_EAUD_SYSERR;

#if ENABLE_TRACING
    snd_lib_error_set_handler(alsa_error_handler);
#else
    /* Set a null error handler prior to enumeration to suppress errors */
    snd_lib_error_set_handler(null_alsa_error_handler);
#endif

    n = hints;
    while (*n != NULL) {
        char *name = snd_device_name_get_hint(*n, "NAME");
        if (name != NULL) {
            if (0 != strcmp("null", name))
                add_dev(af, name);
            free(name);
        }
        n++;
    }

    /* Get the mixer name */
    get_mixer_name(af);

    /* Install error handler after enumeration, otherwise we'll get many
     * error messages about invalid card/device ID.
     */
    snd_lib_error_set_handler(alsa_error_handler);

    err = snd_device_name_free_hint((void**)hints);

    PJ_LOG(4,(THIS_FILE, "ALSA driver found %d devices", af->dev_cnt));

    return PJ_SUCCESS;
}


/* API: get device count */
static unsigned  alsa_factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    struct alsa_factory *af = (struct alsa_factory*)f;
    return af->dev_cnt;
}


/* API: get device info */
static pj_status_t alsa_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                             unsigned index,
                                             pjmedia_aud_dev_info *info)
{
    struct alsa_factory *af = (struct alsa_factory*)f;

    PJ_ASSERT_RETURN(index<af->dev_cnt, PJ_EINVAL);

    pj_memcpy(info, &af->devs[index], sizeof(*info));
    info->caps = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
                 PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY |
                 PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING |
                 PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;

    return PJ_SUCCESS;
}

/* API: create default parameter */
static pj_status_t alsa_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param)
{
    struct alsa_factory *af = (struct alsa_factory*)f;
    pjmedia_aud_dev_info *adi;

    PJ_ASSERT_RETURN(index<af->dev_cnt, PJ_EINVAL);

    adi = &af->devs[index];

    pj_bzero(param, sizeof(*param));
    if (adi->input_count && adi->output_count) {
        param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
        param->rec_id = index;
        param->play_id = index;
    } else if (adi->input_count) {
        param->dir = PJMEDIA_DIR_CAPTURE;
        param->rec_id = index;
        param->play_id = PJMEDIA_AUD_INVALID_DEV;
    } else if (adi->output_count) {
        param->dir = PJMEDIA_DIR_PLAYBACK;
        param->play_id = index;
        param->rec_id = PJMEDIA_AUD_INVALID_DEV;
    } else {
        return PJMEDIA_EAUD_INVDEV;
    }

    param->clock_rate = adi->default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = adi->default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 16;
    param->flags = adi->caps;
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    return PJ_SUCCESS;
}


static int pb_thread_func (void *arg)
{
    struct alsa_stream* stream = (struct alsa_stream*) arg;
    snd_pcm_t* pcm             = stream->pb_pcm;
    int size                   = stream->pb_buf_size;
    snd_pcm_uframes_t nframes  = stream->pb_frames;
    void* user_data            = stream->user_data;
    char* buf                  = stream->pb_buf;
    pj_timestamp tstamp;
    int result;

    pj_bzero (buf, size);
    tstamp.u64 = 0;

    TRACE_((THIS_FILE, "pb_thread_func(%u): Started",
            (unsigned)syscall(SYS_gettid)));

    snd_pcm_prepare (pcm);

    while (!stream->quit) {
        pjmedia_frame frame;

        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = buf;
        frame.size = size;
        frame.timestamp.u64 = tstamp.u64;
        frame.bit_info = 0;

        result = stream->pb_cb (user_data, &frame);
        if (result != PJ_SUCCESS || stream->quit)
            break;

        if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
            pj_bzero (buf, size);

        result = snd_pcm_writei (pcm, buf, nframes);
        if (result == -EPIPE) {
            PJ_LOG (4,(THIS_FILE, "pb_thread_func: underrun!"));
            snd_pcm_prepare (pcm);
        } else if (result < 0) {
            PJ_LOG (4,(THIS_FILE, "pb_thread_func: error writing data!"));
        }

        tstamp.u64 += nframes;
    }

    snd_pcm_drop(pcm);
    TRACE_((THIS_FILE, "pb_thread_func: Stopped"));
    return PJ_SUCCESS;
}



static int ca_thread_func (void *arg)
{
    struct alsa_stream* stream = (struct alsa_stream*) arg;
    snd_pcm_t* pcm             = stream->ca_pcm;
    int size                   = stream->ca_buf_size;
    snd_pcm_uframes_t nframes  = stream->ca_frames;
    void* user_data            = stream->user_data;
    char* buf                  = stream->ca_buf;
    pj_timestamp tstamp;
    int result;
    struct sched_param param;
    pthread_t* thid;

    thid = (pthread_t*) pj_thread_get_os_handle (pj_thread_this());
    param.sched_priority = sched_get_priority_max (SCHED_RR);
    PJ_LOG (5,(THIS_FILE, "ca_thread_func(%u): Set thread priority "
                          "for audio capture thread.",
                          (unsigned)syscall(SYS_gettid)));
    result = pthread_setschedparam (*thid, SCHED_RR, &param);
    if (result) {
        if (result == EPERM)
            PJ_LOG (5,(THIS_FILE, "Unable to increase thread priority, "
                                  "root access needed."));
        else
            PJ_LOG (5,(THIS_FILE, "Unable to increase thread priority, "
                                  "error: %d",
                                  result));
    }

    pj_bzero (buf, size);
    tstamp.u64 = 0;

    TRACE_((THIS_FILE, "ca_thread_func(%u): Started",
            (unsigned)syscall(SYS_gettid)));

    snd_pcm_prepare (pcm);

    while (!stream->quit) {
        pjmedia_frame frame;

        pj_bzero (buf, size);
        result = snd_pcm_readi (pcm, buf, nframes);
        if (result == -EPIPE) {
            PJ_LOG (4,(THIS_FILE, "ca_thread_func: overrun!"));
            snd_pcm_prepare (pcm);
            continue;
        } else if (result < 0) {
            PJ_LOG (4,(THIS_FILE, "ca_thread_func: error reading data!"));
        }
        if (stream->quit)
            break;

        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = (void*) buf;
        frame.size = size;
        frame.timestamp.u64 = tstamp.u64;
        frame.bit_info = 0;

        result = stream->ca_cb (user_data, &frame);
        if (result != PJ_SUCCESS || stream->quit)
            break;

        tstamp.u64 += nframes;
    }
    snd_pcm_drop(pcm);
    TRACE_((THIS_FILE, "ca_thread_func: Stopped"));

    return PJ_SUCCESS;
}


static pj_status_t open_playback (struct alsa_stream* stream,
                                  const pjmedia_aud_param *param)
{
    snd_pcm_hw_params_t* params;
    snd_pcm_format_t format;
    int result;
    unsigned int rate;
    snd_pcm_uframes_t tmp_buf_size;
    snd_pcm_uframes_t tmp_period_size;

    if (param->play_id < 0 || param->play_id >= (int)stream->af->dev_cnt)
        return PJMEDIA_EAUD_INVDEV;

    /* Open PCM for playback */
    PJ_LOG (5,(THIS_FILE, "open_playback: Open playback device '%s'",
               stream->af->devs[param->play_id].name));
    result = snd_pcm_open (&stream->pb_pcm,
                           stream->af->devs[param->play_id].name,
                           SND_PCM_STREAM_PLAYBACK,
                           0);
    if (result < 0)
        return PJMEDIA_EAUD_SYSERR;

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca (&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any (stream->pb_pcm, params);

    /* Set interleaved mode */
    snd_pcm_hw_params_set_access (stream->pb_pcm, params,
                                  SND_PCM_ACCESS_RW_INTERLEAVED);

    /* Set format */
    switch (param->bits_per_sample) {
    case 8:
        TRACE_((THIS_FILE, "open_playback: set format SND_PCM_FORMAT_S8"));
        format = SND_PCM_FORMAT_S8;
        break;
    case 16:
        TRACE_((THIS_FILE, "open_playback: set format SND_PCM_FORMAT_S16_LE"));
        format = SND_PCM_FORMAT_S16_LE;
        break;
    case 24:
        TRACE_((THIS_FILE, "open_playback: set format SND_PCM_FORMAT_S24_LE"));
        format = SND_PCM_FORMAT_S24_LE;
        break;
    case 32:
        TRACE_((THIS_FILE, "open_playback: set format SND_PCM_FORMAT_S32_LE"));
        format = SND_PCM_FORMAT_S32_LE;
        break;
    default:
        TRACE_((THIS_FILE, "open_playback: set format SND_PCM_FORMAT_S16_LE"));
        format = SND_PCM_FORMAT_S16_LE;
        break;
    }
    snd_pcm_hw_params_set_format (stream->pb_pcm, params, format);

    /* Set number of channels */
    TRACE_((THIS_FILE, "open_playback: set channels: %d",
                       param->channel_count));
    result = snd_pcm_hw_params_set_channels (stream->pb_pcm, params,
                                             param->channel_count);
    if (result < 0) {
        PJ_LOG (3,(THIS_FILE, "Unable to set a channel count of %d for "
                   "playback device '%s'", param->channel_count,
                   stream->af->devs[param->play_id].name));
        snd_pcm_close (stream->pb_pcm);
        return PJMEDIA_EAUD_SYSERR;
    }

    /* Set clock rate */
    rate = param->clock_rate;
    TRACE_((THIS_FILE, "open_playback: set clock rate: %d", rate));
    snd_pcm_hw_params_set_rate_near (stream->pb_pcm, params, &rate, NULL);
    TRACE_((THIS_FILE, "open_playback: clock rate set to: %d", rate));

    /* Set period size to samples_per_frame frames. */
    stream->pb_frames = (snd_pcm_uframes_t) param->samples_per_frame /
                                            param->channel_count;
    TRACE_((THIS_FILE, "open_playback: set period size: %d",
            stream->pb_frames));
    tmp_period_size = stream->pb_frames;
    snd_pcm_hw_params_set_period_size_near (stream->pb_pcm, params,
                                            &tmp_period_size, NULL);
    /* Commenting this as it may cause the number of samples per frame
     * to be incorrest.
     */  
    // stream->pb_frames = tmp_period_size > stream->pb_frames ?
    //                  tmp_period_size : stream->pb_frames;                                                                                
    TRACE_((THIS_FILE, "open_playback: period size set to: %d",
            tmp_period_size));

    /* Set the sound device buffer size and latency */
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY)
        tmp_buf_size = (rate / 1000) * param->output_latency_ms;
    else
        tmp_buf_size = (rate / 1000) * PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
    if (tmp_buf_size < tmp_period_size * 2)
        tmp_buf_size = tmp_period_size * 2;
    snd_pcm_hw_params_set_buffer_size_near (stream->pb_pcm, params,
                                            &tmp_buf_size);
    stream->param.output_latency_ms = tmp_buf_size / (rate / 1000);

    /* Set our buffer */
    stream->pb_buf_size = stream->pb_frames * param->channel_count *
                          (param->bits_per_sample/8);
    stream->pb_buf = (char*) pj_pool_alloc(stream->pool, stream->pb_buf_size);

    TRACE_((THIS_FILE, "open_playback: buffer size set to: %d",
            (int)tmp_buf_size));
    TRACE_((THIS_FILE, "open_playback: playback_latency set to: %d ms",
            (int)stream->param.output_latency_ms));

    /* Activate the parameters */
    result = snd_pcm_hw_params (stream->pb_pcm, params);
    if (result < 0) {
        snd_pcm_close (stream->pb_pcm);
        return PJMEDIA_EAUD_SYSERR;
    }

    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) {
        alsa_stream_set_cap(&stream->base,
                            PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
                            &param->output_vol);
    }

    PJ_LOG (5,(THIS_FILE, "Opened device alsa(%s) for playing, sample rate=%d"
               ", ch=%d, bits=%d, period size=%ld frames, latency=%d ms",
               stream->af->devs[param->play_id].name,
               rate, param->channel_count,
               param->bits_per_sample, stream->pb_frames,
               (int)stream->param.output_latency_ms));

    return PJ_SUCCESS;
}


static pj_status_t open_capture (struct alsa_stream* stream,
                                 const pjmedia_aud_param *param)
{
    snd_pcm_hw_params_t* params;
    snd_pcm_format_t format;
    int result;
    unsigned int rate;
    snd_pcm_uframes_t tmp_buf_size;
    snd_pcm_uframes_t tmp_period_size;

    if (param->rec_id < 0 || param->rec_id >= (int)stream->af->dev_cnt)
        return PJMEDIA_EAUD_INVDEV;

    /* Open PCM for capture */
    PJ_LOG (5,(THIS_FILE, "open_capture: Open capture device '%s'",
               stream->af->devs[param->rec_id].name));
    result = snd_pcm_open (&stream->ca_pcm,
                            stream->af->devs[param->rec_id].name,
                           SND_PCM_STREAM_CAPTURE,
                           0);
    if (result < 0)
        return PJMEDIA_EAUD_SYSERR;

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca (&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any (stream->ca_pcm, params);

    /* Set interleaved mode */
    snd_pcm_hw_params_set_access (stream->ca_pcm, params,
                                  SND_PCM_ACCESS_RW_INTERLEAVED);

    /* Set format */
    switch (param->bits_per_sample) {
    case 8:
        TRACE_((THIS_FILE, "open_capture: set format SND_PCM_FORMAT_S8"));
        format = SND_PCM_FORMAT_S8;
        break;
    case 16:
        TRACE_((THIS_FILE, "open_capture: set format SND_PCM_FORMAT_S16_LE"));
        format = SND_PCM_FORMAT_S16_LE;
        break;
    case 24:
        TRACE_((THIS_FILE, "open_capture: set format SND_PCM_FORMAT_S24_LE"));
        format = SND_PCM_FORMAT_S24_LE;
        break;
    case 32:
        TRACE_((THIS_FILE, "open_capture: set format SND_PCM_FORMAT_S32_LE"));
        format = SND_PCM_FORMAT_S32_LE;
        break;
    default:
        TRACE_((THIS_FILE, "open_capture: set format SND_PCM_FORMAT_S16_LE"));
        format = SND_PCM_FORMAT_S16_LE;
        break;
    }
    snd_pcm_hw_params_set_format (stream->ca_pcm, params, format);

    /* Set number of channels */
    TRACE_((THIS_FILE, "open_capture: set channels: %d",
            param->channel_count));
    result = snd_pcm_hw_params_set_channels (stream->ca_pcm, params,
                                             param->channel_count);
    if (result < 0) {
        PJ_LOG (3,(THIS_FILE, "Unable to set a channel count of %d for "
                   "capture device '%s'", param->channel_count,
                   stream->af->devs[param->rec_id].name));
        snd_pcm_close (stream->ca_pcm);
        return PJMEDIA_EAUD_SYSERR;
    }

    /* Set clock rate */
    rate = param->clock_rate;
    TRACE_((THIS_FILE, "open_capture: set clock rate: %d", rate));
    snd_pcm_hw_params_set_rate_near (stream->ca_pcm, params, &rate, NULL);
    TRACE_((THIS_FILE, "open_capture: clock rate set to: %d", rate));

    /* Set period size to samples_per_frame frames. */
    stream->ca_frames = (snd_pcm_uframes_t) param->samples_per_frame /
                                            param->channel_count;
    TRACE_((THIS_FILE, "open_capture: set period size: %d",
            stream->ca_frames));
    tmp_period_size = stream->ca_frames;
    snd_pcm_hw_params_set_period_size_near (stream->ca_pcm, params,
                                            &tmp_period_size, NULL);
    /* Commenting this as it may cause the number of samples per frame
     * to be incorrest.
     */
    // stream->ca_frames = tmp_period_size > stream->ca_frames ?
    //                  tmp_period_size : stream->ca_frames;
    TRACE_((THIS_FILE, "open_capture: period size set to: %d",
            tmp_period_size));

    /* Set the sound device buffer size and latency */
    if (param->flags & PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY)
        tmp_buf_size = (rate / 1000) * param->input_latency_ms;
    else
        tmp_buf_size = (rate / 1000) * PJMEDIA_SND_DEFAULT_REC_LATENCY;
    if (tmp_buf_size < tmp_period_size * 2)
        tmp_buf_size = tmp_period_size * 2;
    snd_pcm_hw_params_set_buffer_size_near (stream->ca_pcm, params,
                                            &tmp_buf_size);
    stream->param.input_latency_ms = tmp_buf_size / (rate / 1000);

    /* Set our buffer */
    stream->ca_buf_size = stream->ca_frames * param->channel_count *
                          (param->bits_per_sample/8);
    stream->ca_buf = (char*) pj_pool_alloc (stream->pool, stream->ca_buf_size);

    TRACE_((THIS_FILE, "open_capture: buffer size set to: %d",
            (int)tmp_buf_size));
    TRACE_((THIS_FILE, "open_capture: capture_latency set to: %d ms",
            (int)stream->param.input_latency_ms));

    /* Activate the parameters */
    result = snd_pcm_hw_params (stream->ca_pcm, params);
    if (result < 0) {
        snd_pcm_close (stream->ca_pcm);
        return PJMEDIA_EAUD_SYSERR;
    }

    if (param->flags & PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING) {
        alsa_stream_set_cap(&stream->base,
                            PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING,
                            &param->input_vol);
    }

    PJ_LOG (5,(THIS_FILE, "Opened device alsa(%s) for capture, sample rate=%d"
               ", ch=%d, bits=%d, period size=%ld frames, latency=%d ms",
               stream->af->devs[param->rec_id].name,
               rate, param->channel_count,
               param->bits_per_sample, stream->ca_frames,
               (int)stream->param.input_latency_ms));

    return PJ_SUCCESS;
}


/* API: create stream */
static pj_status_t alsa_factory_create_stream(pjmedia_aud_dev_factory *f,
                                              const pjmedia_aud_param *param,
                                              pjmedia_aud_rec_cb rec_cb,
                                              pjmedia_aud_play_cb play_cb,
                                              void *user_data,
                                              pjmedia_aud_stream **p_strm)
{
    struct alsa_factory *af = (struct alsa_factory*)f;
    pj_status_t status;
    pj_pool_t* pool;
    struct alsa_stream* stream;

    pool = pj_pool_create (af->pf, "alsa%p", 1024, 1024, NULL);
    if (!pool)
        return PJ_ENOMEM;

    /* Allocate and initialize comon stream data */
    stream = PJ_POOL_ZALLOC_T (pool, struct alsa_stream);
    stream->base.op = &alsa_stream_op;
    stream->pool      = pool;
    stream->af        = af;
    stream->user_data = user_data;
    stream->pb_cb     = play_cb;
    stream->ca_cb     = rec_cb;
    stream->quit      = 0;
    pj_memcpy(&stream->param, param, sizeof(*param));

    /* Init playback */
    if (param->dir & PJMEDIA_DIR_PLAYBACK) {
        status = open_playback (stream, param);
        if (status != PJ_SUCCESS) {
            pj_pool_release (pool);
            return status;
        }
    }

    /* Init capture */
    if (param->dir & PJMEDIA_DIR_CAPTURE) {
        status = open_capture (stream, param);
        if (status != PJ_SUCCESS) {
            if (param->dir & PJMEDIA_DIR_PLAYBACK)
                snd_pcm_close (stream->pb_pcm);
            pj_pool_release (pool);
            return status;
        }
    }

    *p_strm = &stream->base;
    return PJ_SUCCESS;
}


/* API: get running parameter */
static pj_status_t alsa_stream_get_param(pjmedia_aud_stream *s,
                                         pjmedia_aud_param *pi)
{
    struct alsa_stream *stream = (struct alsa_stream*)s;

    PJ_ASSERT_RETURN(s && pi, PJ_EINVAL);

    pj_memcpy(pi, &stream->param, sizeof(*pi));

    return PJ_SUCCESS;
}


/* API: get capability */
static pj_status_t alsa_stream_get_cap(pjmedia_aud_stream *s,
                                       pjmedia_aud_dev_cap cap,
                                       void *pval)
{
    struct alsa_stream *stream = (struct alsa_stream*)s;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap==PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY &&
        (stream->param.dir & PJMEDIA_DIR_CAPTURE))
    {
        /* Recording latency */
        *(unsigned*)pval = stream->param.input_latency_ms;
        return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY &&
               (stream->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
        /* Playback latency */
        *(unsigned*)pval = stream->param.output_latency_ms;
        return PJ_SUCCESS;
    } else {
        return PJMEDIA_EAUD_INVCAP;
    }
}


/* API: set capability */
static pj_status_t alsa_stream_set_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       const void *value)
{
    struct alsa_factory *af = ((struct alsa_stream*)strm)->af;

    if ((cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING &&
        pj_ansi_strlen(af->pb_mixer_name)) ||
        (cap==PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING &&
        pj_ansi_strlen(af->cap_mixer_name)))
    {
        pj_ssize_t min, max;
        snd_mixer_t *handle;
        snd_mixer_selem_id_t *sid;
        snd_mixer_elem_t* elem;
        unsigned vol = *(unsigned*)value;

        if (snd_mixer_open(&handle, 0) < 0)
            return PJMEDIA_EAUD_SYSERR;

        if (snd_mixer_attach(handle, "default") < 0)
            return PJMEDIA_EAUD_SYSERR;

        if (snd_mixer_selem_register(handle, NULL, NULL) < 0)
            return PJMEDIA_EAUD_SYSERR;

        if (snd_mixer_load(handle) < 0)
            return PJMEDIA_EAUD_SYSERR;

        snd_mixer_selem_id_alloca(&sid);
        snd_mixer_selem_id_set_index(sid, 0);
        if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING)
            snd_mixer_selem_id_set_name(sid, af->pb_mixer_name);
        else if (cap==PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING)
            snd_mixer_selem_id_set_name(sid, af->cap_mixer_name);

        elem = snd_mixer_find_selem(handle, sid);
        if (!elem)
            return PJMEDIA_EAUD_SYSERR;

        if (cap == PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) {
            snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
            if (snd_mixer_selem_set_playback_volume_all(elem,
                                                        vol * max / 100) < 0)
            {
                return PJMEDIA_EAUD_SYSERR;
            }
        } else if (cap == PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING) {
             snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
             if (snd_mixer_selem_set_capture_volume_all(elem,
                                                        vol * max / 100) < 0)
             {
                 return PJMEDIA_EAUD_SYSERR;
             }
        }

        snd_mixer_close(handle);
        return PJ_SUCCESS;
    }

    return PJMEDIA_EAUD_INVCAP;
}


/* API: start stream */
static pj_status_t alsa_stream_start (pjmedia_aud_stream *s)
{
    struct alsa_stream *stream = (struct alsa_stream*)s;
    pj_status_t status = PJ_SUCCESS;

    stream->quit = 0;
    if (stream->param.dir & PJMEDIA_DIR_PLAYBACK) {
        status = pj_thread_create (stream->pool,
                                   "alsasound_playback",
                                   pb_thread_func,
                                   stream,
                                   0, //ZERO,
                                   0,
                                   &stream->pb_thread);
        if (status != PJ_SUCCESS)
            return status;
    }

    if (stream->param.dir & PJMEDIA_DIR_CAPTURE) {
        status = pj_thread_create (stream->pool,
                                   "alsasound_playback",
                                   ca_thread_func,
                                   stream,
                                   0, //ZERO,
                                   0,
                                   &stream->ca_thread);
        if (status != PJ_SUCCESS) {
            stream->quit = PJ_TRUE;
            pj_thread_join(stream->pb_thread);
            pj_thread_destroy(stream->pb_thread);
            stream->pb_thread = NULL;
        }
    }

    return status;
}


/* API: stop stream */
static pj_status_t alsa_stream_stop (pjmedia_aud_stream *s)
{
    struct alsa_stream *stream = (struct alsa_stream*)s;

    stream->quit = 1;

    if (stream->pb_thread) {
        TRACE_((THIS_FILE,
                   "alsa_stream_stop(%u): Waiting for playback to stop.",
                   (unsigned)syscall(SYS_gettid)));
        pj_thread_join (stream->pb_thread);
        TRACE_((THIS_FILE,
                   "alsa_stream_stop(%u): playback stopped.",
                   (unsigned)syscall(SYS_gettid)));
        pj_thread_destroy(stream->pb_thread);
        stream->pb_thread = NULL;
    }

    if (stream->ca_thread) {
        TRACE_((THIS_FILE,
                   "alsa_stream_stop(%u): Waiting for capture to stop.",
                   (unsigned)syscall(SYS_gettid)));
        pj_thread_join (stream->ca_thread);
        TRACE_((THIS_FILE,
                   "alsa_stream_stop(%u): capture stopped.",
                   (unsigned)syscall(SYS_gettid)));
        pj_thread_destroy(stream->ca_thread);
        stream->ca_thread = NULL;
    }

    return PJ_SUCCESS;
}



static pj_status_t alsa_stream_destroy (pjmedia_aud_stream *s)
{
    struct alsa_stream *stream = (struct alsa_stream*)s;

    alsa_stream_stop (s);

    if (stream->param.dir & PJMEDIA_DIR_PLAYBACK) {
        snd_pcm_close (stream->pb_pcm);
        stream->pb_pcm = NULL;
    }
    if (stream->param.dir & PJMEDIA_DIR_CAPTURE) {
        snd_pcm_close (stream->ca_pcm);
        stream->ca_pcm = NULL;
    }

    pj_pool_release (stream->pool);

    return PJ_SUCCESS;
}

#endif  /* PJMEDIA_AUDIO_DEV_HAS_ALSA */
