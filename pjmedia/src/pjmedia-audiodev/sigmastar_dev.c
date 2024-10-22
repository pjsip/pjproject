/*
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#include <pj/log.h>
#include <pj/os.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>

#if PJMEDIA_AUDIO_DEV_HAS_SIGMASTAR_AUDIO

#define THIS_FILE               "sigmastar_dev.c"

/* sigmastar_audio device info */
struct sigmastar_audio_dev_info
{
    pjmedia_aud_dev_info         info;
    unsigned                     dev_id;
};

/* sigmastar_audio factory */
struct sigmastar_audio_factory
{
    pjmedia_aud_dev_factory      base;
    pj_pool_t                   *pool;
    pj_pool_factory             *pf;

    unsigned                     dev_count;
    struct sigmastar_audio_dev_info  *dev_info;
};

/* Sound stream. */
struct sigmastar_audio_stream
{
    pjmedia_aud_stream   base;              /**< Base stream           */
    pjmedia_aud_param    param;             /**< Settings              */
    pj_pool_t           *pool;              /**< Memory pool.          */

    pjmedia_aud_rec_cb   rec_cb;            /**< Capture callback.     */
    pjmedia_aud_play_cb  play_cb;           /**< Playback callback.    */
    void                *user_data;         /**< Application data.     */

    /* Playback */
    //snd_pcm_t           *pb_pcm;
    uint32_t    pb_frames;     /* samples_per_frame            */
    //pjmedia_aud_play_cb  pb_cb;
    unsigned             pb_buf_size;
    char                *pb_buf;
    pj_thread_t         *pb_thread;

    /* Capture */
    //snd_pcm_t           *ca_pcm;
    uint32_t    ca_frames;     /* samples_per_frame            */
    //pjmedia_aud_rec_cb   ca_cb;
    unsigned             ca_buf_size;
    char                *ca_buf;
    pj_thread_t         *ca_thread;

    int                  quit;
};


/* Prototypes */
static pj_status_t sigmastar_factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t sigmastar_factory_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t sigmastar_factory_refresh(pjmedia_aud_dev_factory *f);
static unsigned    sigmastar_factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t sigmastar_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                             unsigned index,
                                             pjmedia_aud_dev_info *info);
static pj_status_t sigmastar_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param);
static pj_status_t sigmastar_factory_create_stream(pjmedia_aud_dev_factory *f,
                                              const pjmedia_aud_param *param,
                                              pjmedia_aud_rec_cb rec_cb,
                                              pjmedia_aud_play_cb play_cb,
                                              void *user_data,
                                              pjmedia_aud_stream **p_aud_strm);

static pj_status_t sigmastar_stream_get_param(pjmedia_aud_stream *strm,
                                         pjmedia_aud_param *param);
static pj_status_t sigmastar_stream_get_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       void *value);
static pj_status_t sigmastar_stream_set_cap(pjmedia_aud_stream *strm,
                                       pjmedia_aud_dev_cap cap,
                                       const void *value);
static pj_status_t sigmastar_stream_start(pjmedia_aud_stream *strm);
static pj_status_t sigmastar_stream_stop(pjmedia_aud_stream *strm);
static pj_status_t sigmastar_stream_destroy(pjmedia_aud_stream *strm);

/* Operations */
static pjmedia_aud_dev_factory_op factory_op =
{
    &sigmastar_factory_init,
    &sigmastar_factory_destroy,
    &sigmastar_factory_get_dev_count,
    &sigmastar_factory_get_dev_info,
    &sigmastar_factory_default_param,
    &sigmastar_factory_create_stream,
    &sigmastar_factory_refresh
};

static pjmedia_aud_stream_op stream_op =
{
    &sigmastar_stream_get_param,
    &sigmastar_stream_get_cap,
    &sigmastar_stream_set_cap,
    &sigmastar_stream_start,
    &sigmastar_stream_stop,
    &sigmastar_stream_destroy
};


/****************************************************************************
 * Factory operations
 */
/*
 * Init sigmastar_audio audio driver.
 */
pjmedia_aud_dev_factory* pjmedia_sigmastar_audio_factory(pj_pool_factory *pf)
{
    struct sigmastar_audio_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "sigmastar audio", 1000, 1000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct sigmastar_audio_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &factory_op;

    return &f->base;
}


/* API: init factory */
static pj_status_t sigmastar_factory_init(pjmedia_aud_dev_factory *f)
{
    struct sigmastar_audio_factory *nf = (struct sigmastar_audio_factory*)f;
    struct sigmastar_audio_dev_info *ndi;

    /* Initialize input and output devices here */
    nf->dev_count = 1;
    nf->dev_info = (struct sigmastar_audio_dev_info*)
                   pj_pool_calloc(nf->pool, nf->dev_count,
                                  sizeof(struct sigmastar_audio_dev_info));
    ndi = &nf->dev_info[0];
    pj_bzero(ndi, sizeof(*ndi));
    pj_ansi_strxcpy(ndi->info.name, "sigmastar device", sizeof(ndi->info.name));
    pj_ansi_strxcpy(ndi->info.driver, "sigmastar", sizeof(ndi->info.driver));
    ndi->info.input_count = 1;
    ndi->info.output_count = 1;
    ndi->info.default_samples_per_sec = 16000;
    /* Set the device capabilities here */
    ndi->info.caps = 0;

    PJ_LOG(4, (THIS_FILE, "sigmastar audio initialized"));

    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t sigmastar_factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct sigmastar_audio_factory *nf = (struct sigmastar_audio_factory*)f;

    pj_pool_safe_release(&nf->pool);

    return PJ_SUCCESS;
}

/* API: refresh the list of devices */
static pj_status_t sigmastar_factory_refresh(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned sigmastar_factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    struct sigmastar_audio_factory *nf = (struct sigmastar_audio_factory*)f;
    return nf->dev_count;
}

/* API: get device info */
static pj_status_t sigmastar_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                             unsigned index,
                                             pjmedia_aud_dev_info *info)
{
    struct sigmastar_audio_factory *nf = (struct sigmastar_audio_factory*)f;

    PJ_ASSERT_RETURN(index < nf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_memcpy(info, &nf->dev_info[index].info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t sigmastar_factory_default_param(pjmedia_aud_dev_factory *f,
                                              unsigned index,
                                              pjmedia_aud_param *param)
{
    struct sigmastar_audio_factory *nf = (struct sigmastar_audio_factory*)f;
    struct sigmastar_audio_dev_info *di = &nf->dev_info[index];

    PJ_ASSERT_RETURN(index < nf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_bzero(param, sizeof(*param));
    if (di->info.input_count && di->info.output_count) {
        param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
        param->rec_id = index;
        param->play_id = index;
    } else if (di->info.input_count) {
        param->dir = PJMEDIA_DIR_CAPTURE;
        param->rec_id = index;
        param->play_id = PJMEDIA_AUD_INVALID_DEV;
    } else if (di->info.output_count) {
        param->dir = PJMEDIA_DIR_PLAYBACK;
        param->play_id = index;
        param->rec_id = PJMEDIA_AUD_INVALID_DEV;
    } else {
        return PJMEDIA_EAUD_INVDEV;
    }

    /* Set the mandatory settings here */
    /* The values here are just some examples */
    param->clock_rate = di->info.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = di->info.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 16;

    /* Set the device capabilities here */
    param->flags = 0;

    return PJ_SUCCESS;
}

#include "mi_ao.h"
#include "mi_ai.h"
#include "mi_sys.h"
#include "mi_common_datatype.h"

static pj_pool_t* pool;
// Audio Input
MI_AUDIO_DEV AiDevId = 0;
MI_AI_CHN AiChn = 0;
MI_AUDIO_Frame_t stAiChFrame;
MI_AUDIO_AecFrame_t stAecFrame;
MI_SYS_ChnPort_t stAiChnOutputPort;
// Audio Output
MI_AUDIO_DEV AoDevId = 0;
MI_AO_CHN AoChn = 0;
MI_AUDIO_Frame_t stAoSendFrame;
#define SAMPLE_RATE E_MI_AUDIO_SAMPLE_RATE_16000
#define SAMPLE_PER_FRAME (SAMPLE_RATE * 20 / 1000)
MI_U8 u8Buf[SAMPLE_PER_FRAME * 2];
MI_SYS_ChnPort_t stChnPort;
MI_S32 s32Fd;
fd_set readFdSet;
struct timeval stTimeOut;

void init_vqe(void) {
    MI_AI_VqeConfig_t stAiSetVqeConfig, stAiGetVqeConfig;
    memset(&stAiSetVqeConfig, 0x0, sizeof(MI_AI_VqeConfig_t));
    stAiSetVqeConfig.u32ChnNum = 1;
    stAiSetVqeConfig.bAecOpen = FALSE;
    stAiSetVqeConfig.bAgcOpen = FALSE;
    stAiSetVqeConfig.bAnrOpen = FALSE;
    stAiSetVqeConfig.bEqOpen = FALSE;
    stAiSetVqeConfig.bHpfOpen = FALSE;
    stAiSetVqeConfig.s32FrameSample = 128;
    stAiSetVqeConfig.s32WorkSampleRate = SAMPLE_RATE;
    MI_AI_SetVqeAttr(AiDevId, AiChn, AiDevId, AiChn, &stAiSetVqeConfig);
    MI_AI_GetVqeAttr(AiDevId, AiChn, &stAiGetVqeConfig);
    MI_AI_EnableVqe(AiDevId, AiChn);
}

static pj_status_t initialize_audio_capture(struct sigmastar_audio_stream* stream, const pjmedia_aud_param *param) {
    
    MI_AUDIO_Attr_t stAttr;
    stAttr.eBitwidth = E_MI_AUDIO_BIT_WIDTH_16;
    stAttr.eSamplerate = param->clock_rate;
    PJ_LOG (4,(THIS_FILE, "capture sr = %d", param->clock_rate));
    stAttr.eSoundmode = E_MI_AUDIO_SOUND_MODE_MONO;
    stAttr.eWorkmode = E_MI_AUDIO_MODE_I2S_SLAVE;
    stAttr.u32PtNumPerFrm = param->samples_per_frame;
    stAttr.u32ChnCnt = 1;

    if (MI_AI_SetPubAttr(AiDevId, &stAttr) != MI_SUCCESS) return -1;
    if (MI_AI_Enable(AiDevId) != MI_SUCCESS) return -1;
    if (MI_AI_EnableChn(AiDevId, AiChn) != MI_SUCCESS) return -1;

    stAiChnOutputPort.eModId = E_MI_MODULE_ID_AI;
    stAiChnOutputPort.u32DevId = AiDevId;
    stAiChnOutputPort.u32ChnId = AiChn;
    stAiChnOutputPort.u32PortId = 0;

    MI_SYS_SetChnOutputPortDepth(&stAiChnOutputPort, 1, 8);

    stChnPort.eModId = E_MI_MODULE_ID_AI;
    stChnPort.u32DevId = AiDevId;
    stChnPort.u32ChnId = AiChn;
    stChnPort.u32PortId = 0;
    if (MI_SYS_GetFd(&stChnPort, &s32Fd) != MI_SUCCESS) return -1;

    //init_vqe();

    MI_AI_SetVqeVolume(AiDevId, AiChn, 9);
    
    stream->ca_frames = (uint32_t) param->samples_per_frame /
                                            param->channel_count;
    stream->ca_buf_size = stream->ca_frames * param->channel_count *
                          (param->bits_per_sample/8);
    stream->ca_buf = (char*) pj_pool_alloc (stream->pool, stream->ca_buf_size);
    if(stream->ca_buf == NULL) PJ_LOG (4,(THIS_FILE, "NULL 2"));

    return 0;
}

static pj_status_t initialize_audio_output(struct sigmastar_audio_stream* stream, const pjmedia_aud_param *param) {
    
    MI_AUDIO_Attr_t stAttr;
    stAttr.eBitwidth = E_MI_AUDIO_BIT_WIDTH_16;
    stAttr.eSamplerate = param->clock_rate;
    PJ_LOG (4,(THIS_FILE, "playback sr = %d", param->clock_rate));
    stAttr.eSoundmode = E_MI_AUDIO_SOUND_MODE_MONO;
    stAttr.eWorkmode = E_MI_AUDIO_MODE_I2S_MASTER;
    stAttr.u32PtNumPerFrm = param->samples_per_frame;
    stAttr.u32ChnCnt = 1;

    if (MI_AO_SetPubAttr(AoDevId, &stAttr) != MI_SUCCESS) return -1;
    if (MI_AO_Enable(AoDevId) != MI_SUCCESS) return -1;
    if (MI_AO_EnableChn(AoDevId, AoChn) != MI_SUCCESS) return -1;

    MI_AO_ChnState_t stStatus;
    MI_AO_QueryChnStat(AoDevId, AoChn, &stStatus);
    PJ_LOG (4,(THIS_FILE, "total=%d, free=%d, busy=%d", stStatus.u32ChnTotalNum, stStatus.u32ChnFreeNum, stStatus.u32ChnBusyNum));

    memset(&stAoSendFrame, 0x0, sizeof(MI_AUDIO_Frame_t));  
    stAoSendFrame.u32Len = SAMPLE_PER_FRAME * 2;
    stAoSendFrame.apVirAddr[0] = u8Buf;
    stAoSendFrame.apVirAddr[1] = NULL;

    MI_AO_SetVolume(AoDevId, 0);
    
    stream->pb_frames = (uint32_t) param->samples_per_frame /
                                            param->channel_count;
    stream->pb_buf_size = stream->pb_frames * param->channel_count *
                          (param->bits_per_sample/8);
    stream->pb_buf = (char*) pj_pool_alloc(stream->pool, stream->pb_buf_size);
    if(stream->pb_buf == NULL) PJ_LOG (4,(THIS_FILE, "NULL 1"));

    return 0;
}

/* API: create stream */
static pj_status_t sigmastar_factory_create_stream(pjmedia_aud_dev_factory *f,
                                              const pjmedia_aud_param *param,
                                              pjmedia_aud_rec_cb rec_cb,
                                              pjmedia_aud_play_cb play_cb,
                                              void *user_data,
                                              pjmedia_aud_stream **p_aud_strm)
{
    struct sigmastar_audio_factory *nf = (struct sigmastar_audio_factory*)f;
    pj_pool_t *pool;
    struct sigmastar_audio_stream *strm;

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(nf->pf, "sigmastar_audio-dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct sigmastar_audio_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    strm->rec_cb = rec_cb;
    strm->play_cb = play_cb;
    strm->user_data = user_data;

    MI_SYS_Init();

    /* Create player stream here */
    if (param->dir & PJMEDIA_DIR_PLAYBACK) {
        initialize_audio_output(strm, param);
    }

    /* Create capture stream here */
    if (param->dir & PJMEDIA_DIR_CAPTURE) {
        initialize_audio_capture(strm, param);
    }

    /* Apply the remaining settings */
    /* Below is an example if you want to set the output volume */
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) {
        sigmastar_stream_set_cap(&strm->base,
                            PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
                            &param->output_vol);
    }

    /* Done */
    strm->base.op = &stream_op;
    *p_aud_strm = &strm->base;

    return PJ_SUCCESS;
}

/* API: Get stream info. */
static pj_status_t sigmastar_stream_get_param(pjmedia_aud_stream *s,
                                         pjmedia_aud_param *pi)
{
    struct sigmastar_audio_stream *strm = (struct sigmastar_audio_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    /* Example: Update the volume setting */
    if (sigmastar_stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
                            &pi->output_vol) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
    }

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t sigmastar_stream_get_cap(pjmedia_aud_stream *s,
                                       pjmedia_aud_dev_cap cap,
                                       void *pval)
{
    struct sigmastar_audio_stream *strm = (struct sigmastar_audio_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    /* Example: Get the output's volume setting */
    if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING)
    {
        /* Output volume setting */
        *(unsigned*)pval = 0; // retrieve output device's volume here
        return PJ_SUCCESS;
    } else {
        return PJMEDIA_EAUD_INVCAP;
    }
}

/* API: set capability */
static pj_status_t sigmastar_stream_set_cap(pjmedia_aud_stream *s,
                                       pjmedia_aud_dev_cap cap,
                                       const void *pval)
{
    struct sigmastar_audio_stream *strm = (struct sigmastar_audio_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    /* Example */
    if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING)
    {
        /* Output volume setting */
        // set output's volume level here
        return PJ_SUCCESS;
    }

    return PJMEDIA_EAUD_INVCAP;
}

static int my_get_frame(void *buffer, uint32_t nsample) {
    FD_ZERO(&readFdSet);
    FD_SET(s32Fd, &readFdSet);
    stTimeOut.tv_sec = 0;
    stTimeOut.tv_usec = 100 * 1000;

    int res;
    
    if ((res = select(s32Fd + 1, &readFdSet, NULL, NULL, &stTimeOut)) > 0) {
        if (FD_ISSET(s32Fd, &readFdSet)) {
            if ((res = MI_AI_GetFrame(AiDevId, AiChn, &stAiChFrame, &stAecFrame, 0)) == MI_SUCCESS) {
                pj_memcpy(buffer, stAiChFrame.apVirAddr[0], nsample * 2);
                MI_AI_ReleaseFrame(AiDevId, AiChn, &stAiChFrame, &stAecFrame);
                //PJ_LOG(3, (THIS_FILE, "ts=%u, seq=%u, len=%u", stAiChFrame.u64TimeStamp, stAiChFrame.u32Seq, stAiChFrame.u32Len));
                return 0;
            }
        }
    }
    PJ_LOG(3, (THIS_FILE, "Error receiving frame from audio input %x", res));
    return -1;
}

static int my_put_frame(void *buffer, uint32_t nsamples) {
    int ret;

    pj_memcpy(stAoSendFrame.apVirAddr[0], buffer, nsamples * 2);
    
    do{  
        ret = MI_AO_SendFrame(AoDevId, AoChn, &stAoSendFrame, 0);  
    }while(ret == MI_AO_ERR_NOBUF); 
    
    //PJ_LOG(3, (THIS_FILE, "Hello"));

    return 0;
}

static int ca_thread_func (void *arg)
{
    struct sigmastar_audio_stream* stream = (struct sigmastar_audio_stream*) arg;
    //snd_pcm_t* pcm             = stream->ca_pcm;
    int ca_size                   = stream->ca_buf_size;
    int pb_size                   = stream->pb_buf_size;
    uint32_t ca_nframes           = stream->ca_frames;
    uint32_t pb_nframes           = stream->pb_frames;
    void* user_data               = stream->user_data;
    char* ca_buf                  = stream->ca_buf;
    char* pb_buf                  = stream->pb_buf;
    pj_timestamp ca_tstamp;
    pj_timestamp pb_tstamp;
    int result = 0;
    struct sched_param param;
    pthread_t* thid;
    uint8_t cnt = 0;

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
    
    pj_bzero (ca_buf, ca_size);
    pj_bzero (pb_buf, pb_size);
    ca_tstamp.u64 = 0;
    pb_tstamp.u64 = 0;

    //snd_pcm_prepare (pcm);

    //for (uint8_t i = 0; i < 10; i++) my_put_frame (pb_buf, pb_nframes);

    while (!stream->quit) {
        pjmedia_frame frame;


        // record
        pj_bzero (ca_buf, ca_size);
        result = my_get_frame (ca_buf, ca_nframes);
        /*
        if (result == 0) {
            PJ_LOG (4,(THIS_FILE, "ca_thread_func: overrun!"));
            //snd_pcm_prepare (pcm);
            continue;
        }
        else
        */
        {
            if (result < 0) {
                PJ_LOG (4,(THIS_FILE, "ca_thread_func: error reading data!"));
            }
        }
        if (stream->quit)
            break;

        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = (void*) ca_buf;
        frame.size = ca_size;
        frame.timestamp.u64 = ca_tstamp.u64;
        frame.bit_info = 0;
        
        result = stream->rec_cb (user_data, &frame);
        if (result != PJ_SUCCESS || stream->quit)
            break;
        

        // play
        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = (void*) pb_buf;
        frame.size = pb_size;
        frame.timestamp.u64 = pb_tstamp.u64;
        frame.bit_info = 0;

        result = stream->play_cb (user_data, &frame);
        if (result != PJ_SUCCESS || stream->quit)
            break;
        
        if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
            pj_bzero (pb_buf, pb_size);
        result = my_put_frame (pb_buf, pb_nframes);
        
        //if (result == -EPIPE) {
            //PJ_LOG (4,(THIS_FILE, "pb_thread_func: underrun!"));
            //snd_pcm_prepare (pcm);
        //}
        //else
        {
            if (result < 0) {
                PJ_LOG (4,(THIS_FILE, "pb_thread_func: error writing data!"));
            }
        }
        
        if(cnt++ == 100) {
            cnt = 0;
            MI_AO_ChnState_t stStatus;
            MI_AO_QueryChnStat(AoDevId, AoChn, &stStatus);
            PJ_LOG (4,(THIS_FILE, "total=%d, free=%d, busy=%d", stStatus.u32ChnTotalNum, stStatus.u32ChnFreeNum, stStatus.u32ChnBusyNum));
        }

        ca_tstamp.u64 += ca_nframes;
        pb_tstamp.u64 += pb_nframes;
    }
    //snd_pcm_drop(pcm);

    return PJ_SUCCESS;
}

static int pb_thread_func (void *arg)
{
    struct sigmastar_audio_stream* stream = (struct sigmastar_audio_stream*) arg;
    //snd_pcm_t* pcm             = stream->pb_pcm;
    int size                   = stream->pb_buf_size;
    uint32_t nframes = stream->pb_frames;
    void* user_data            = stream->user_data;
    char* buf                  = stream->pb_buf;
    pj_timestamp tstamp;
    int result = 0;
    struct sched_param param;
    pthread_t* thid;
    /*
    thid = (pthread_t*) pj_thread_get_os_handle (pj_thread_this());
    param.sched_priority = sched_get_priority_max (SCHED_RR);
    PJ_LOG (5,(THIS_FILE, "pb_thread_func(%u): Set thread priority "
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
    */
    pj_bzero (buf, size);
    tstamp.u64 = 0;

    //snd_pcm_prepare (pcm);

    while (!stream->quit) {
        pjmedia_frame frame;

        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = buf;
        frame.size = size;
        frame.timestamp.u64 = tstamp.u64;
        frame.bit_info = 0;
        
        result = stream->play_cb (user_data, &frame);
        if (result != PJ_SUCCESS || stream->quit)
            break;
        
        if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
            pj_bzero (buf, size);
        result = my_put_frame (buf, nframes);
        /*
        if (result == -EPIPE) {
            PJ_LOG (4,(THIS_FILE, "pb_thread_func: underrun!"));
            snd_pcm_prepare (pcm);
        }
        else
        */
        {
            if (result < 0) {
                PJ_LOG (4,(THIS_FILE, "pb_thread_func: error writing data!"));
            }
        }

        tstamp.u64 += nframes;
    }

    //snd_pcm_drop(pcm);
    return PJ_SUCCESS;
}

/* API: Start stream. */
static pj_status_t sigmastar_stream_start(pjmedia_aud_stream *strm)
{
    struct sigmastar_audio_stream *stream = (struct sigmastar_audio_stream*)strm;


    PJ_LOG(4, (THIS_FILE, "Starting sigmastar audio stream"));


    pj_status_t status = PJ_SUCCESS;

    stream->quit = 0;
    /*
    if (stream->param.dir & PJMEDIA_DIR_PLAYBACK) {
        status = pj_thread_create (stream->pool,
                                   "custom_playback",
                                   pb_thread_func,
                                   stream,
                                   0, //ZERO,
                                   0,
                                   &stream->pb_thread);
        if (status != PJ_SUCCESS)
            return status;
    }
    */
    if (stream->param.dir & PJMEDIA_DIR_CAPTURE) {
        status = pj_thread_create (stream->pool,
                                   "custom_capture",
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

/* API: Stop stream. */
static pj_status_t sigmastar_stream_stop(pjmedia_aud_stream *strm)
{
    struct sigmastar_audio_stream *stream = (struct sigmastar_audio_stream*)strm;

    stream->quit = 1;

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

static pj_status_t deinitialize_audio_capture() {
    if (MI_AI_DisableChn(AiDevId, AiChn) != MI_SUCCESS) return -1;
    if (MI_AI_Disable(AiDevId) != MI_SUCCESS) return -1;
    return 0;
}

static pj_status_t deinitialize_audio_output() {
    if (MI_AO_DisableChn(AoDevId, AoChn) != MI_SUCCESS) return -1;
    if (MI_AO_Disable(AoDevId) != MI_SUCCESS) return -1;
    return 0;
}

/* API: Destroy stream. */
static pj_status_t sigmastar_stream_destroy(pjmedia_aud_stream *strm)
{
    struct sigmastar_audio_stream *stream = (struct sigmastar_audio_stream*)strm;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    sigmastar_stream_stop(strm);
    
    deinitialize_audio_capture();
    deinitialize_audio_output();

    pj_pool_release(stream->pool);

    return PJ_SUCCESS;
}

#endif  /* PJMEDIA_AUDIO_DEV_HAS_NULL_AUDIO */
