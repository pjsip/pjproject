/* $Id$ */
/*
 * Copyright (C) 2008-2012 Teluu Inc. (http://www.teluu.com)
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

/*
 * This is the implementation of BlackBerry 10 (BB10) audio device.
 * Original code was kindly donated by Truphone Ltd.
 * The key methods here are bb10_capture_open, bb10_play_open together
 * with the capture and play threads ca_thread_func and pb_thread_func
 */

#include <pjmedia_audiodev.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pjmedia/errno.h>

#if defined(PJMEDIA_AUDIO_DEV_HAS_BB10) && PJMEDIA_AUDIO_DEV_HAS_BB10 != 0

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>
#include <sys/asoundlib.h>


#define THIS_FILE 			"bb10_dev.c"
#define BB10_DEVICE_NAME 		"plughw:%d,%d"
/* Double these for 16khz sampling */
#define PREFERRED_FRAME_SIZE 320
#define VOIP_SAMPLE_RATE 8000

/* Set to 1 to enable tracing */
#if 1
#    define TRACE_(expr)		PJ_LOG(4,expr)
#else
#    define TRACE_(expr)
#endif

/*
 * Factory prototypes
 */
static pj_status_t bb10_factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t bb10_factory_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t bb10_factory_refresh(pjmedia_aud_dev_factory *f);
static unsigned    bb10_factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t bb10_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                             unsigned index,
                                             pjmedia_aud_dev_info *info);
static pj_status_t bb10_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param);
static pj_status_t bb10_factory_create_stream(pjmedia_aud_dev_factory *f,
                                              const pjmedia_aud_param *param,
                                              pjmedia_aud_rec_cb rec_cb,
                                              pjmedia_aud_play_cb play_cb,
                                              void *user_data,
                                              pjmedia_aud_stream **p_strm);

/*
 * Stream prototypes
 */
static pj_status_t bb10_stream_get_param(pjmedia_aud_stream *strm,
                                         pjmedia_aud_param *param);
static pj_status_t bb10_stream_get_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       void *value);
static pj_status_t bb10_stream_set_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       const void *value);
static pj_status_t bb10_stream_start(pjmedia_aud_stream *strm);
static pj_status_t bb10_stream_stop(pjmedia_aud_stream *strm);
static pj_status_t bb10_stream_destroy(pjmedia_aud_stream *strm);


struct bb10_factory
{
    pjmedia_aud_dev_factory	 base;
    pj_pool_factory		*pf;
    pj_pool_t			*pool;
    pj_pool_t			*base_pool;
    unsigned			 dev_cnt;
    pjmedia_aud_dev_info	 devs[1];
};

struct bb10_stream
{
    pjmedia_aud_stream	 base;

    /* Common */
    pj_pool_t		*pool;
    struct bb10_factory *af;
    void		*user_data;
    pjmedia_aud_param	 param;		/* Running parameter 		*/
    int                  rec_id;      	/* Capture device id		*/
    int                  quit;

    /* Playback */
    snd_pcm_t		*pb_pcm;
    snd_mixer_t         *pb_mixer;
    unsigned long        pb_frames; 	/* samples_per_frame		*/
    pjmedia_aud_play_cb  pb_cb;
    unsigned             pb_buf_size;
    char		*pb_buf;
    pj_thread_t		*pb_thread;

    /* Capture */
    snd_pcm_t		*ca_pcm;
    snd_mixer_t         *ca_mixer;
    unsigned long        ca_frames; 	/* samples_per_frame		*/
    pjmedia_aud_rec_cb   ca_cb;
    unsigned             ca_buf_size;
    char		*ca_buf;
    pj_thread_t		*ca_thread;
};

static pjmedia_aud_dev_factory_op bb10_factory_op =
{
    &bb10_factory_init,
    &bb10_factory_destroy,
    &bb10_factory_get_dev_count,
    &bb10_factory_get_dev_info,
    &bb10_factory_default_param,
    &bb10_factory_create_stream,
    &bb10_factory_refresh
};

static pjmedia_aud_stream_op bb10_stream_op =
{
    &bb10_stream_get_param,
    &bb10_stream_get_cap,
    &bb10_stream_set_cap,
    &bb10_stream_start,
    &bb10_stream_stop,
    &bb10_stream_destroy
};

/*
 * BB10 - tests loads the audio units and sets up the driver structure
 */
static pj_status_t bb10_add_dev (struct bb10_factory *af)
{
    pjmedia_aud_dev_info *adi;
    int pb_result, ca_result;
    int card = -1;
    int dev = 0;
    snd_pcm_t *pcm_handle;

    if (af->dev_cnt >= PJ_ARRAY_SIZE(af->devs))
        return PJ_ETOOMANY;

    adi = &af->devs[af->dev_cnt];

    TRACE_((THIS_FILE, "bb10_add_dev Enter"));

    if ((pb_result = snd_pcm_open_preferred (&pcm_handle, &card, &dev,
                                             SND_PCM_OPEN_PLAYBACK)) >= 0)
    {
        TRACE_((THIS_FILE, "Try to open the device for playback - success"));
	snd_pcm_close (pcm_handle);
    } else {
        TRACE_((THIS_FILE, "Try to open the device for playback - failure"));
    }

    if ((ca_result = snd_pcm_open_preferred (&pcm_handle, &card, &dev,
                                             SND_PCM_OPEN_CAPTURE)) >=0)
    {
        TRACE_((THIS_FILE, "Try to open the device for capture - success"));
        snd_pcm_close (pcm_handle);
    } else {
        TRACE_((THIS_FILE, "Try to open the device for capture - failure"));
    }

    if (pb_result < 0 && ca_result < 0) {
        TRACE_((THIS_FILE, "Unable to open sound device", "preferred"));
        return PJMEDIA_EAUD_NODEV;
    }

    /* Reset device info */
    pj_bzero(adi, sizeof(*adi));

    /* Set device name */
    strcpy(adi->name, "preferred");

    /* Check the number of playback channels */
    adi->output_count = (pb_result >= 0) ? 1 : 0;

    /* Check the number of capture channels */
    adi->input_count = (ca_result >= 0) ? 1 : 0;

    /* Set the default sample rate */
    adi->default_samples_per_sec = 8000;

    /* Driver name */
    strcpy(adi->driver, "BB10");

    ++af->dev_cnt;

    PJ_LOG (4,(THIS_FILE, "Added sound device %s", adi->name));

    return PJ_SUCCESS;
}

/* Create BB10 audio driver. */
pjmedia_aud_dev_factory* pjmedia_bb10_factory(pj_pool_factory *pf)
{
    struct bb10_factory *af;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "bb10_aud_base", 256, 256, NULL);
    af = PJ_POOL_ZALLOC_T(pool, struct bb10_factory);
    af->pf = pf;
    af->base_pool = pool;
    af->base.op = &bb10_factory_op;

    return &af->base;
}


/* API: init factory */
static pj_status_t bb10_factory_init(pjmedia_aud_dev_factory *f)
{
    pj_status_t status;
    
    status = bb10_factory_refresh(f);
    if (status != PJ_SUCCESS)
        return status;

    PJ_LOG(4,(THIS_FILE, "BB10 initialized"));
    return PJ_SUCCESS;
}


/* API: destroy factory */
static pj_status_t bb10_factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct bb10_factory *af = (struct bb10_factory*)f;

    if (af->pool) {
        TRACE_((THIS_FILE, "bb10_factory_destroy() - 1"));
        pj_pool_release(af->pool);
    }

    if (af->base_pool) {
        pj_pool_t *pool = af->base_pool;
        af->base_pool = NULL;
        TRACE_((THIS_FILE, "bb10_factory_destroy() - 2"));
        pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}


/* API: refresh the device list */
static pj_status_t bb10_factory_refresh(pjmedia_aud_dev_factory *f)
{
    struct bb10_factory *af = (struct bb10_factory*)f;
    int err;

    TRACE_((THIS_FILE, "bb10_factory_refresh()"));

    if (af->pool != NULL) {
        pj_pool_release(af->pool);
        af->pool = NULL;
    }

    af->pool = pj_pool_create(af->pf, "bb10_aud", 256, 256, NULL);
    af->dev_cnt = 0;

    err = bb10_add_dev(af);

    PJ_LOG(4,(THIS_FILE, "BB10 driver found %d devices", af->dev_cnt));

    return err;
}


/* API: get device count */
static unsigned  bb10_factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    struct bb10_factory *af = (struct bb10_factory*)f;
    return af->dev_cnt;
}


/* API: get device info */
static pj_status_t bb10_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                             unsigned index,
                                             pjmedia_aud_dev_info *info)
{
    struct bb10_factory *af = (struct bb10_factory*)f;

    PJ_ASSERT_RETURN(index>=0 && index<af->dev_cnt, PJ_EINVAL);

    pj_memcpy(info, &af->devs[index], sizeof(*info));
    info->caps = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
                 PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
    
    return PJ_SUCCESS;
}

/* API: create default parameter */
static pj_status_t bb10_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param)
{
    struct bb10_factory *af = (struct bb10_factory*)f;
    pjmedia_aud_dev_info *adi;

    PJ_ASSERT_RETURN(index>=0 && index<af->dev_cnt, PJ_EINVAL);

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

    TRACE_((THIS_FILE, "bb10_factory_default_param clock = %d flags = %d"
                       " spf = %d", param->clock_rate, param->flags,
                       param->samples_per_frame));
    
    return PJ_SUCCESS;
}


static void close_play_pcm(struct bb10_stream *stream)
{
    if (stream != NULL && stream->pb_pcm != NULL) {
        snd_pcm_close(stream->pb_pcm);
        stream->pb_pcm = NULL;
    }
}

static void close_play_mixer(struct bb10_stream *stream)
{
    if (stream != NULL && stream->pb_mixer != NULL) {
        snd_mixer_close(stream->pb_mixer);
        stream->pb_mixer = NULL;
    }
}

static void flush_play(struct bb10_stream *stream)
{
    if (stream != NULL && stream->pb_pcm != NULL) {
        snd_pcm_plugin_flush (stream->pb_pcm, SND_PCM_CHANNEL_PLAYBACK);
    }
}

static void close_capture_pcm(struct bb10_stream *stream)
{
    if (stream != NULL && stream->ca_pcm != NULL) {
        snd_pcm_close(stream->ca_pcm);
        stream->ca_pcm = NULL;
    }
}

static void close_capture_mixer(struct bb10_stream *stream)
{
    if (stream != NULL && stream->ca_mixer != NULL) {
        snd_mixer_close(stream->ca_mixer);
        stream->ca_mixer = NULL;
    }
}

static void flush_capture(struct bb10_stream *stream)
{
    if (stream != NULL && stream->ca_pcm != NULL) {
        snd_pcm_plugin_flush (stream->ca_pcm, SND_PCM_CHANNEL_CAPTURE);
    }
}


/**
 * Play audio received from PJMEDIA
 */
static int pb_thread_func (void *arg)
{
    struct bb10_stream* stream = (struct bb10_stream *) arg;
    /* Handle from bb10_open_playback */
    /* Will be 640 */
    int size                   	= stream->pb_buf_size;
    /* 160 frames for 20ms */
    unsigned long nframes	= stream->pb_frames;
    void *user_data            	= stream->user_data;
    char *buf 		       	= stream->pb_buf;
    pj_timestamp tstamp;
    int result = 0;

    pj_bzero (buf, size);
    tstamp.u64 = 0;

    TRACE_((THIS_FILE, "pb_thread_func: size = %d ", size));

    /* Do the final initialization now the thread has started. */
    if ((result = snd_pcm_plugin_prepare(stream->pb_pcm,
                                         SND_PCM_CHANNEL_PLAYBACK)) < 0)
    {
        close_play_mixer(stream);
        close_play_pcm(stream);
        TRACE_((THIS_FILE, "pb_thread_func failed prepare = %d", result));
	return PJ_SUCCESS;
    }

    while (!stream->quit) {
        pjmedia_frame frame;

        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        /* pointer to buffer filled by PJMEDIA */
        frame.buf = buf;
        frame.size = size;
        frame.timestamp.u64 = tstamp.u64;
        frame.bit_info = 0;

        result = stream->pb_cb (user_data, &frame);
        if (result != PJ_SUCCESS || stream->quit)
            break;

        if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
            pj_bzero (buf, size);

        /* Write 640 to play unit */
        result = snd_pcm_plugin_write(stream->pb_pcm,buf,size);
        if (result != size) {
            TRACE_((THIS_FILE, "pb_thread_func failed write = %d", result));
        }

	tstamp.u64 += nframes;
    }

    flush_play(stream);
    close_play_mixer(stream);
    close_play_pcm(stream);
    TRACE_((THIS_FILE, "pb_thread_func: Stopped"));
    
    return PJ_SUCCESS;
}



static int ca_thread_func (void *arg)
{
    struct bb10_stream* stream = (struct bb10_stream *) arg;
    int size                   = stream->ca_buf_size;
    unsigned long nframes      = stream->ca_frames;
    void *user_data            = stream->user_data;
    /* Buffer to fill for PJMEDIA */
    char *buf 		       = stream->ca_buf;
    pj_timestamp tstamp;
    int result;
    struct sched_param param;
    pthread_t *thid;

    TRACE_((THIS_FILE, "ca_thread_func: size = %d ", size));

    thid = (pthread_t*) pj_thread_get_os_handle (pj_thread_this());
    param.sched_priority = sched_get_priority_max (SCHED_RR);

    result = pthread_setschedparam (*thid, SCHED_RR, &param);
    if (result) {
        if (result == EPERM) {
            PJ_LOG (4,(THIS_FILE, "Unable to increase thread priority, "
                                  "root access needed."));
        } else {
            PJ_LOG (4,(THIS_FILE, "Unable to increase thread priority, "
                                  "error: %d", result));
        }
    }

    pj_bzero (buf, size);
    tstamp.u64 = 0;

    /* Final init now the thread has started */
    if ((result = snd_pcm_plugin_prepare (stream->ca_pcm,
                                          SND_PCM_CHANNEL_CAPTURE)) < 0)
    {
        close_capture_mixer(stream);
        close_capture_pcm(stream);
        TRACE_((THIS_FILE, "ca_thread_func failed prepare = %d", result));
	return PJ_SUCCESS;
    }

    while (!stream->quit) {
        pjmedia_frame frame;

        pj_bzero (buf, size);
        
        result = snd_pcm_plugin_read(stream->ca_pcm, buf,size);
        if (result == -EPIPE) {
            PJ_LOG (4,(THIS_FILE, "ca_thread_func: overrun!"));
            snd_pcm_plugin_prepare (stream->ca_pcm, SND_PCM_CHANNEL_CAPTURE);
            continue;
        } else if (result < 0) {
            PJ_LOG (4,(THIS_FILE, "ca_thread_func: error reading data!"));
        }

        if (stream->quit)
            break;

        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = (void *) buf;
        frame.size = size;
        frame.timestamp.u64 = tstamp.u64;
        frame.bit_info = 0;

        result = stream->ca_cb (user_data, &frame);
        if (result != PJ_SUCCESS || stream->quit)
            break;

        tstamp.u64 += nframes;
    }

    flush_capture(stream);
    close_capture_mixer(stream);
    close_capture_pcm(stream);
    TRACE_((THIS_FILE, "ca_thread_func: Stopped"));

    return PJ_SUCCESS;
}


static pj_status_t bb10_open_playback (struct bb10_stream *stream,
                                       const pjmedia_aud_param *param)
{
    int card = -1;
    int dev = 0;
    int ret = 0;
    snd_pcm_channel_info_t pi;
    snd_pcm_channel_setup_t setup;
    snd_mixer_group_t group;
    unsigned int rate;
    unsigned long tmp_buf_size;

    if (param->play_id < 0 || param->play_id >= stream->af->dev_cnt) {
        return PJMEDIA_EAUD_INVDEV;
    }

    if ((ret = snd_pcm_open_preferred (&stream->pb_pcm, &card, &dev,
                                       SND_PCM_OPEN_PLAYBACK)) < 0)
    {
        TRACE_((THIS_FILE, "snd_pcm_open_preferred ret = %d", ret));
        return PJMEDIA_EAUD_SYSERR;
    }

    /* TODO PJ_ZERO */
    memset (&pi, 0, sizeof (pi));
    pi.channel = SND_PCM_CHANNEL_PLAYBACK;
    if ((ret = snd_pcm_plugin_info (stream->pb_pcm, &pi)) < 0) {
        TRACE_((THIS_FILE, "snd_pcm_plugin_info ret = %d", ret));
        return PJMEDIA_EAUD_SYSERR;
    }

    snd_pcm_channel_params_t pp;
    memset (&pp, 0, sizeof (pp));

    /* Request VoIP compatible capabilities
     * On simulator frag_size is always negotiated to 170
     */
    pp.mode = SND_PCM_MODE_BLOCK;
    pp.channel = SND_PCM_CHANNEL_PLAYBACK;
    pp.start_mode = SND_PCM_START_DATA;
    pp.stop_mode = SND_PCM_STOP_ROLLOVER;
    /* HARD CODE for the time being PJMEDIA expects 640 for 16khz */
    pp.buf.block.frag_size = PREFERRED_FRAME_SIZE*2;
    /* Increasing this internal buffer count delays write failure in the loop */
    pp.buf.block.frags_max = 4;
    pp.buf.block.frags_min = 1;
    pp.format.interleave = 1;
    /* HARD CODE for the time being PJMEDIA expects 16khz */
    PJ_TODO(REMOVE_SAMPLE_RATE_HARD_CODE);    
    pp.format.rate = VOIP_SAMPLE_RATE*2;
    pp.format.voices = 1;
    pp.format.format = SND_PCM_SFMT_S16_LE;

    /* Make the calls as per the wave sample */
    if ((ret = snd_pcm_plugin_params (stream->pb_pcm, &pp)) < 0) {
        TRACE_((THIS_FILE, "snd_pcm_plugin_params ret = %d", ret));
        return PJMEDIA_EAUD_SYSERR;
    }

    memset (&setup, 0, sizeof (setup));
    memset (&group, 0, sizeof (group));
    setup.channel = SND_PCM_CHANNEL_PLAYBACK;
    setup.mixer_gid = &group.gid;
    
    if ((ret = snd_pcm_plugin_setup (stream->pb_pcm, &setup)) < 0) {
        TRACE_((THIS_FILE, "snd_pcm_plugin_setup ret = %d", ret));
        return PJMEDIA_EAUD_SYSERR;
    }

    if (group.gid.name[0] == 0) {
        return PJMEDIA_EAUD_SYSERR;
    }
    
    if ((ret = snd_mixer_open (&stream->pb_mixer, card,
                               setup.mixer_device)) < 0)
    {
        TRACE_((THIS_FILE, "snd_mixer_open ret = %d", ret));
        return PJMEDIA_EAUD_SYSERR;
    }


    rate = param->clock_rate;
    /* Set the sound device buffer size and latency */
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY) {
        tmp_buf_size = (rate / 1000) * param->output_latency_ms;
    } else {
	tmp_buf_size = (rate / 1000) * PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
    }
    /* Set period size to samples_per_frame frames. */
    stream->pb_frames = param->samples_per_frame;
    stream->param.output_latency_ms = tmp_buf_size / (rate / 1000);

    /* Set our buffer */
    stream->pb_buf_size = stream->pb_frames * param->channel_count *
                          (param->bits_per_sample/8);
    stream->pb_buf = (char *) pj_pool_alloc(stream->pool, stream->pb_buf_size);

    TRACE_((THIS_FILE, "bb10_open_playback: pb_frames = %d clock = %d",
                       stream->pb_frames, param->clock_rate));

    return PJ_SUCCESS;
}

static pj_status_t bb10_open_capture (struct bb10_stream *stream,
                                      const pjmedia_aud_param *param)
{
    int ret = 0;
    unsigned int rate;
    unsigned long tmp_buf_size;
    int card = -1;
    int dev = 0;
    int frame_size;
    snd_pcm_channel_info_t pi;
    snd_mixer_group_t group;
    snd_pcm_channel_params_t pp;
    snd_pcm_channel_setup_t setup;

    if (param->rec_id < 0 || param->rec_id >= stream->af->dev_cnt)
        return PJMEDIA_EAUD_INVDEV;

    /* BB10 Audio init here (not prepare) */
    if ((ret = snd_pcm_open_preferred (&stream->ca_pcm, &card, &dev,
                                       SND_PCM_OPEN_CAPTURE)) < 0)
    {
        TRACE_((THIS_FILE, "snd_pcm_open_preferred ret = %d", ret));
        return PJMEDIA_EAUD_SYSERR;
    }

    /* sample reads the capabilities of the capture */
    memset (&pi, 0, sizeof (pi));
    pi.channel = SND_PCM_CHANNEL_CAPTURE;
    if ((ret = snd_pcm_plugin_info (stream->ca_pcm, &pi)) < 0) {
        TRACE_((THIS_FILE, "snd_pcm_plugin_info ret = %d", ret));
        return PJMEDIA_EAUD_SYSERR;
    }

    /* Request the VoIP parameters
     * These parameters are different to waverec sample
     */
    memset (&pp, 0, sizeof (pp));
    /* Blocking read */
    pp.mode = SND_PCM_MODE_BLOCK;
    pp.channel = SND_PCM_CHANNEL_CAPTURE;
    pp.start_mode = SND_PCM_START_DATA;
    /* Auto-recover from errors */
    pp.stop_mode = SND_PCM_STOP_ROLLOVER;
    /* HARD CODE for the time being PJMEDIA expects 640 for 16khz */
    pp.buf.block.frag_size = PREFERRED_FRAME_SIZE*2;
    /* Not applicable for capture hence -1 */
    pp.buf.block.frags_max = -1;
    pp.buf.block.frags_min = 1;
    pp.format.interleave = 1;
    /* HARD CODE for the time being PJMEDIA expects 16khz */
    PJ_TODO(REMOVE_SAMPLE_RATE_HARD_CODE);
    pp.format.rate = VOIP_SAMPLE_RATE*2;
    pp.format.voices = 1;
    pp.format.format = SND_PCM_SFMT_S16_LE;

    /* make the request */
    if ((ret = snd_pcm_plugin_params (stream->ca_pcm, &pp)) < 0) {
        TRACE_((THIS_FILE, "snd_pcm_plugin_params ret = %d", ret));
        return PJMEDIA_EAUD_SYSERR;
    }

    /* Again based on the sample */
    memset (&setup, 0, sizeof (setup));
    memset (&group, 0, sizeof (group));
    setup.channel = SND_PCM_CHANNEL_CAPTURE;
    setup.mixer_gid = &group.gid;
    if ((ret = snd_pcm_plugin_setup (stream->ca_pcm, &setup)) < 0) {
        TRACE_((THIS_FILE, "snd_pcm_plugin_setup ret = %d", ret));
        return PJMEDIA_EAUD_SYSERR;
    }

    frame_size = setup.buf.block.frag_size;

    if (group.gid.name[0] == 0) {
    } else {
    }

    if ((ret = snd_mixer_open (&stream->ca_mixer, card,
                               setup.mixer_device)) < 0)
    {
        TRACE_((THIS_FILE,"snd_mixer_open ret = %d",ret));
        return PJMEDIA_EAUD_SYSERR;
    }

    /* frag_size should be 160 */
    frame_size = setup.buf.block.frag_size;

    /* END BB10 init */

    /* Set clock rate */
    rate = param->clock_rate;
    stream->ca_frames = (unsigned long) param->samples_per_frame /
			param->channel_count;

    /* Set the sound device buffer size and latency */
    if (param->flags & PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY) {
        tmp_buf_size = (rate / 1000) * param->input_latency_ms;
    } else {
        tmp_buf_size = (rate / 1000) * PJMEDIA_SND_DEFAULT_REC_LATENCY;
    }

    stream->param.input_latency_ms = tmp_buf_size / (rate / 1000);

    /* Set our buffer */
    stream->ca_buf_size = stream->ca_frames * param->channel_count *
			  (param->bits_per_sample/8);
    stream->ca_buf = (char *)pj_pool_alloc (stream->pool, stream->ca_buf_size);

    TRACE_((THIS_FILE, "bb10_open_capture: ca_frames = %d clock = %d",
                       stream->ca_frames, param->clock_rate));

    return PJ_SUCCESS;
}


/* API: create stream */
static pj_status_t bb10_factory_create_stream(pjmedia_aud_dev_factory *f,
                                              const pjmedia_aud_param *param,
                                              pjmedia_aud_rec_cb rec_cb,
                                              pjmedia_aud_play_cb play_cb,
                                              void *user_data,
                                              pjmedia_aud_stream **p_strm)
{
    struct bb10_factory *af = (struct bb10_factory*)f;
    pj_status_t status;
    pj_pool_t* pool;
    struct bb10_stream* stream;

    pool = pj_pool_create (af->pf, "bb10%p", 1024, 1024, NULL);
    if (!pool)
        return PJ_ENOMEM;

    /* Allocate and initialize comon stream data */
    stream = PJ_POOL_ZALLOC_T (pool, struct bb10_stream);
    stream->base.op   = &bb10_stream_op;
    stream->pool      = pool;
    stream->af 	      = af;
    stream->user_data = user_data;
    stream->pb_cb     = play_cb;
    stream->ca_cb     = rec_cb;
    stream->quit      = 0;
    pj_memcpy(&stream->param, param, sizeof(*param));

    /* Init playback */
    if (param->dir & PJMEDIA_DIR_PLAYBACK) {
        status = bb10_open_playback (stream, param);
        if (status != PJ_SUCCESS) {
            pj_pool_release (pool);
            return status;
        }
    }

    /* Init capture */
    if (param->dir & PJMEDIA_DIR_CAPTURE) {
        status = bb10_open_capture (stream, param);
        if (status != PJ_SUCCESS) {
            if (param->dir & PJMEDIA_DIR_PLAYBACK) {
                close_play_mixer(stream);
                close_play_pcm(stream);
            }
            pj_pool_release (pool);
            return status;
        }
    }

    *p_strm = &stream->base;
    return PJ_SUCCESS;
}


/* API: get running parameter */
static pj_status_t bb10_stream_get_param(pjmedia_aud_stream *s,
                                         pjmedia_aud_param *pi)
{
    struct bb10_stream *stream = (struct bb10_stream*)s;

    PJ_ASSERT_RETURN(s && pi, PJ_EINVAL);

    pj_memcpy(pi, &stream->param, sizeof(*pi));

    return PJ_SUCCESS;
}


/* API: get capability */
static pj_status_t bb10_stream_get_cap(pjmedia_aud_stream *s,
                                       pjmedia_aud_dev_cap cap,
                                       void *pval)
{
    struct bb10_stream *stream = (struct bb10_stream*)s;

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
static pj_status_t bb10_stream_set_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       const void *value)
{
    PJ_UNUSED_ARG(strm);
    PJ_UNUSED_ARG(cap);
    PJ_UNUSED_ARG(value);

    return PJMEDIA_EAUD_INVCAP;
}


/* API: start stream */
static pj_status_t bb10_stream_start (pjmedia_aud_stream *s)
{
    struct bb10_stream *stream = (struct bb10_stream*)s;
    pj_status_t status = PJ_SUCCESS;

    stream->quit = 0;
    if (stream->param.dir & PJMEDIA_DIR_PLAYBACK) {
        status = pj_thread_create (stream->pool,
				   "bb10sound_playback",
				   pb_thread_func,
				   stream,
				   0,
				   0,
				   &stream->pb_thread);
        if (status != PJ_SUCCESS)
            return status;
    }

    if (stream->param.dir & PJMEDIA_DIR_CAPTURE) {
        status = pj_thread_create (stream->pool,
				   "bb10sound_playback",
				   ca_thread_func,
				   stream,
				   0,
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
static pj_status_t bb10_stream_stop (pjmedia_aud_stream *s)
{
    struct bb10_stream *stream = (struct bb10_stream*)s;

    stream->quit = 1;
    TRACE_((THIS_FILE,"bb10_stream_stop()"));

    if (stream->pb_thread) {
        pj_thread_join (stream->pb_thread);
        pj_thread_destroy(stream->pb_thread);
        stream->pb_thread = NULL;
    }

    if (stream->ca_thread) {
        pj_thread_join (stream->ca_thread);
        pj_thread_destroy(stream->ca_thread);
        stream->ca_thread = NULL;
    }

    return PJ_SUCCESS;
}

static pj_status_t bb10_stream_destroy (pjmedia_aud_stream *s)
{
    struct bb10_stream *stream = (struct bb10_stream*)s;
    
    TRACE_((THIS_FILE,"bb10_stream_destroy()"));

    bb10_stream_stop (s);

    if (stream->param.dir & PJMEDIA_DIR_PLAYBACK) {
        /*
        snd_mixer_close (stream->pb_mixer);
	snd_pcm_close (stream->pb_pcm);
        stream->pb_mixer = NULL;
        stream->pb_pcm = NULL;
        */
    }
    if (stream->param.dir & PJMEDIA_DIR_CAPTURE) {
        /*
        snd_mixer_close (stream->ca_mixer);
        snd_pcm_close (stream->ca_pcm);
        stream->ca_mixer = NULL;
        stream->ca_pcm = NULL;
        */
    }

    pj_pool_release (stream->pool);

    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_AUDIO_DEV_HAS_BB10 */
