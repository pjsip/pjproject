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

#if PJMEDIA_AUDIO_DEV_HAS_WASAPI

#include <Avrt.h>
#include <windows.h>
#include <audioclient.h>
#include <Processthreadsapi.h>

#if defined(PJ_WIN32_UWP) && PJ_WIN32_UWP
#define USE_ASYNC_ACTIVATE 1;
#endif

#if defined(USE_ASYNC_ACTIVATE)
    #include <Mmdeviceapi.h>    
    #include <wrl\implements.h>
    #include <ppltasks.h>
    using namespace Windows::Media::Devices;  
    using namespace Microsoft::WRL;
    using namespace concurrency;   
#else
    #include <phoneaudioclient.h>
    #using <Windows.winmd>
    using namespace Windows::Phone::Media::Devices;
#endif

#define THIS_FILE               "wasapi_dev.cpp"

/* Set to 1 to enable tracing */
#if 1
#    define TRACE_(expr)                PJ_LOG(4,expr)
#else
#    define TRACE_(expr)
#endif

#define EXIT_ON_ERROR(hr) \
                    do { \
                        if(FAILED(hr)) { \
                            status = PJMEDIA_EAUD_WASAPI_ERROR; \
                            goto on_exit; }\
                    } while (0)

struct wasapi_stream;

class AudioActivator;

#if defined(USE_ASYNC_ACTIVATE)

class AudioActivator : public RuntimeClass< RuntimeClassFlags< ClassicCom >,
    FtmBase,
    IActivateAudioInterfaceCompletionHandler >
{
    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation
                                   *pAsyncOp);
public:
    AudioActivator() {};
    task_completion_event<ComPtr<IAudioClient2>> task_completed;
};

#endif

/* WASAPI factory */
struct wasapi_factory
{
    pjmedia_aud_dev_factory      base;
    pj_pool_t                   *base_pool;
    pj_pool_t                   *pool;
    pj_pool_factory             *pf;

    unsigned                     dev_count;
    pjmedia_aud_dev_info         devs[1];
};

/* Sound stream. */
struct wasapi_stream
{
    pjmedia_aud_stream      base;               /* Base stream              */

    /* Common */
    pjmedia_aud_param       param;              /* Settings                 */
    pj_pool_t              *pool;               /* Memory pool              */
    void                   *user_data;          /* Application data         */
    struct wasapi_factory  *wf;
    pj_thread_t            *thread;             /* Thread handle            */
    HANDLE                  quit_event;
    pjmedia_format_id       fmt_id;             /* Frame format             */
    unsigned                bytes_per_sample;

    /* Playback */    
    pjmedia_aud_play_cb     pb_cb;              /* Playback callback        */
    IAudioClient2          *default_pb_dev;     /* Default playback dev     */
    IAudioRenderClient     *pb_client;          /* Playback client          */
    ISimpleAudioVolume     *pb_volume;          /* Playback volume          */    
    unsigned                pb_max_frame_count;     
    HANDLE                  pb_event;           /* Playback event           */
    pj_timestamp            pb_timestamp;
#if defined(USE_ASYNC_ACTIVATE)
    ComPtr<AudioActivator>  pb_aud_act;
    Platform::String^       pb_id;
#else
    LPCWSTR                 pb_id;              
#endif

    /* Capture */    
    pjmedia_aud_rec_cb      cap_cb;             /* Capture callback         */
    IAudioClient2          *default_cap_dev;    /* Default capture dev      */    
    IAudioCaptureClient    *cap_client;         /* Capture client           */
    unsigned                bytes_per_frame;    /* Bytes per frame          */
    pj_int16_t             *cap_buf;            /* Capture audio buffer     */
    unsigned                cap_max_frame_count;
    HANDLE                  cap_event;          /* Capture event            */    
    unsigned                cap_buf_count;
    pj_timestamp            cap_timestamp;
#if defined(USE_ASYNC_ACTIVATE)
    ComPtr<AudioActivator>  cap_aud_act;
    Platform::String^       cap_id;
#else
    LPCWSTR                 cap_id;             
#endif
};

/* Prototypes */
static pj_status_t wasapi_factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t wasapi_factory_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t wasapi_factory_refresh(pjmedia_aud_dev_factory *f);
static unsigned    wasapi_factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t wasapi_factory_get_dev_info(pjmedia_aud_dev_factory *f, 
                                               unsigned index,
                                               pjmedia_aud_dev_info *info);
static pj_status_t wasapi_factory_default_param(pjmedia_aud_dev_factory *f,
                                                unsigned index,
                                                pjmedia_aud_param *param);
static pj_status_t wasapi_factory_create_stream(pjmedia_aud_dev_factory *f,
                                                const pjmedia_aud_param *param,
                                                pjmedia_aud_rec_cb rec_cb,
                                                pjmedia_aud_play_cb play_cb,
                                                void *user_data,
                                                pjmedia_aud_stream **p_strm);

/*
 * Stream prototypes
 */
static pj_status_t wasapi_stream_get_param(pjmedia_aud_stream *strm,
                                           pjmedia_aud_param *param);
static pj_status_t wasapi_stream_get_cap(pjmedia_aud_stream *strm,
                                         pjmedia_aud_dev_cap cap,
                                         void *value);
static pj_status_t wasapi_stream_set_cap(pjmedia_aud_stream *strm,
                                         pjmedia_aud_dev_cap cap,
                                         const void *value);
static pj_status_t wasapi_stream_start(pjmedia_aud_stream *strm);
static pj_status_t wasapi_stream_stop(pjmedia_aud_stream *strm);
static pj_status_t wasapi_stream_destroy(pjmedia_aud_stream *strm);

/* Operations */
static pjmedia_aud_dev_factory_op factory_op =
{
    &wasapi_factory_init,
    &wasapi_factory_destroy,
    &wasapi_factory_get_dev_count,
    &wasapi_factory_get_dev_info,
    &wasapi_factory_default_param,
    &wasapi_factory_create_stream,
    &wasapi_factory_refresh
};

static pjmedia_aud_stream_op stream_op = 
{
    &wasapi_stream_get_param,
    &wasapi_stream_get_cap,
    &wasapi_stream_set_cap,
    &wasapi_stream_start,
    &wasapi_stream_stop,
    &wasapi_stream_destroy
};

static pj_status_t init_waveformatex(WAVEFORMATEX *wfx,
                                     const pjmedia_aud_param *prm,
                                     struct wasapi_stream *strm)
{
    if (prm->ext_fmt.id == PJMEDIA_FORMAT_L16) {
        wfx->wFormatTag = WAVE_FORMAT_PCM;
        wfx->nChannels = (pj_uint16_t)prm->channel_count;
        wfx->nSamplesPerSec = prm->clock_rate;
        wfx->nBlockAlign = (pj_uint16_t)(prm->channel_count *
                                         strm->bytes_per_sample);
        wfx->nAvgBytesPerSec = prm->clock_rate * prm->channel_count *
                               strm->bytes_per_sample;
        wfx->wBitsPerSample = (WORD)prm->bits_per_sample;
        wfx->cbSize = 0;

        return PJ_SUCCESS;
    }
    else if ((prm->flags & PJMEDIA_AUD_DEV_CAP_EXT_FORMAT) &&
             (prm->ext_fmt.id == PJMEDIA_FORMAT_PCMA ||
             prm->ext_fmt.id == PJMEDIA_FORMAT_PCMU))
    {
        unsigned ptime;
        ptime = prm->samples_per_frame * 1000 /
            (prm->clock_rate * prm->channel_count);
        wfx->wFormatTag = (pj_uint16_t)
                          ((prm->ext_fmt.id == PJMEDIA_FORMAT_PCMA) ?
                           WAVE_FORMAT_ALAW : WAVE_FORMAT_MULAW);
        wfx->nChannels = (pj_uint16_t)prm->channel_count;
        wfx->nSamplesPerSec = prm->clock_rate;
        wfx->nAvgBytesPerSec = prm->clock_rate * prm->channel_count;
        wfx->nBlockAlign = (pj_uint16_t)(wfx->nAvgBytesPerSec * ptime /
                                         1000);
        wfx->wBitsPerSample = 8;
        wfx->cbSize = 0;

        return PJ_SUCCESS;
    } else {
        return PJMEDIA_EAUD_BADFORMAT;
    }
}

/* WMME capture and playback thread. */
static int PJ_THREAD_FUNC wasapi_dev_thread(void *arg)
{
    struct wasapi_stream *strm = (struct wasapi_stream*)arg;
    HANDLE events[3];
    unsigned eventCount;
    pj_status_t status = PJ_SUCCESS;
    static unsigned rec_cnt, play_cnt;
    enum { MAX_BURST = 1000 };

    rec_cnt = play_cnt = 0;
    strm->cap_buf_count = 0;
    eventCount = 0;
    events[eventCount++] = strm->quit_event;
    if (strm->param.dir & PJMEDIA_DIR_PLAYBACK)
        events[eventCount++] = strm->pb_event;
    if (strm->param.dir & PJMEDIA_DIR_CAPTURE)
        events[eventCount++] = strm->cap_event;

    /*
     * Loop while not signalled to quit, wait for event objects to be 
     * signalled by Wasapi capture and play buffer.
     */
    while (status == PJ_SUCCESS)
    {
        DWORD rc;
        pjmedia_dir signalled_dir;      
        HRESULT hr;
 
        /* Swap hWaveIn and hWaveOut to get equal opportunity for both */
        if (eventCount==3) {
            HANDLE hTemp = events[2];
            events[2] = events[1];
            events[1] = hTemp;
        }

        rc = WaitForMultipleObjectsEx(eventCount, events, FALSE, INFINITE, 
                                      FALSE);

        if (rc < WAIT_OBJECT_0 || rc >= WAIT_OBJECT_0 + eventCount)
            continue;

        if (rc == WAIT_OBJECT_0)
            break;

        if (rc == (WAIT_OBJECT_0 + 1)) {
            if (events[1] == strm->pb_event)
                signalled_dir = PJMEDIA_DIR_PLAYBACK;
            else
                signalled_dir = PJMEDIA_DIR_CAPTURE;
        } else {
            if (events[2] == strm->pb_event)
                signalled_dir = PJMEDIA_DIR_PLAYBACK;
            else
                signalled_dir = PJMEDIA_DIR_CAPTURE;
        }

        if (signalled_dir == PJMEDIA_DIR_PLAYBACK) {
            unsigned incoming_frame = 0;
            unsigned frame_to_render = 0;           
            unsigned padding = 0;
            BYTE *cur_pb_buf = NULL;
            pjmedia_frame frame;

            status = PJ_SUCCESS;    
            
            /* Get available space on buffer to render */
            hr = strm->default_pb_dev->GetCurrentPadding(&padding);
            if (FAILED(hr)) {
                continue;
            }

            incoming_frame = strm->bytes_per_frame /
                             (strm->bytes_per_sample *
                              strm->param.channel_count);
            frame_to_render = strm->pb_max_frame_count - padding;
            if (frame_to_render >= incoming_frame) {
                frame_to_render = incoming_frame;
            } else {
                /* Don't get new frame because there's no space */
                continue;
            }

            hr = strm->pb_client->GetBuffer(frame_to_render, &cur_pb_buf);
            if (FAILED(hr)) {
                PJ_LOG(4, (THIS_FILE, "Error getting wasapi buffer"));
                continue;
            }

            if (strm->fmt_id == PJMEDIA_FORMAT_L16) {
                /* PCM mode */
                frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
                frame.size = strm->bytes_per_frame;
                frame.timestamp.u64 = strm->pb_timestamp.u64;
                frame.bit_info = 0;
                frame.buf = cur_pb_buf;
            }

            status = (*strm->pb_cb)(strm->user_data, &frame);

            /* Write to the device. */
            hr = strm->pb_client->ReleaseBuffer(frame_to_render, 0);
            if (FAILED(hr)) {
                PJ_LOG(4, (THIS_FILE, "Error releasing wasapi buffer"));
                continue;
            }

            strm->pb_timestamp.u64 += strm->param.samples_per_frame /
                                      strm->param.channel_count;
        } else {
            /* Capture */           
            pj_uint32_t packet_size = 0;

            hr = strm->cap_client->GetNextPacketSize(&packet_size);
            if (FAILED(hr)) {
                PJ_LOG(4, (THIS_FILE, "Error getting next packet size"));
                continue;
            }
            
            while (packet_size) {

                unsigned next_frame_size = 0;
                DWORD cap_flag;
                pj_int16_t *cap_buf = NULL;
                unsigned nsamples = 0;

                hr = strm->cap_client->GetBuffer((BYTE**)&cap_buf,
                                                 &next_frame_size,
                                                 &cap_flag,
                                                 NULL,
                                                 NULL);

                if (FAILED(hr) || (next_frame_size == 0)) {
                    PJ_LOG(4, (THIS_FILE, "Error getting next buffer, \
                               next frame size : %d", next_frame_size));
                    packet_size = 0;
                    continue;
                }

                nsamples = (next_frame_size * strm->param.channel_count)
                           + strm->cap_buf_count;

                if (nsamples >= strm->param.samples_per_frame) {
                    /* If buffer is not empty, combine the buffer with the just
                     * incoming samples, then call put_frame.
                    */
                    if (strm->cap_buf_count) {
                        unsigned chunk_count = 0;
                        pjmedia_frame frame;

                        chunk_count = strm->param.samples_per_frame -
                                      strm->cap_buf_count;

                        pjmedia_copy_samples(strm->cap_buf + 
                                               strm->cap_buf_count,
                                             (pj_int16_t*)cap_buf, 
                                             chunk_count);

                        if (strm->fmt_id == PJMEDIA_FORMAT_L16) {
                            /* PCM mode */
                            /* Prepare frame */
                            frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
                            frame.buf = (void *)strm->cap_buf;
                            frame.size = strm->bytes_per_frame;
                            frame.timestamp.u64 = strm->cap_timestamp.u64;
                            frame.bit_info = 0;
                        }

                        status = (*strm->cap_cb)(strm->user_data, &frame);
                        if (status == PJ_SUCCESS) {
                            /* Update position */
                            cap_buf = (pj_int16_t *)cap_buf + chunk_count;
                            nsamples -= strm->param.samples_per_frame;
                            strm->cap_timestamp.u64 +=
                                                strm->param.samples_per_frame /
                                                strm->param.channel_count;
                        }
                    }

                    /* Give all frame we have */
                    while (nsamples >= strm->param.samples_per_frame &&
                           status == PJ_SUCCESS)
                    {
                        pjmedia_frame frame;

                        pjmedia_copy_samples(strm->cap_buf,
                                             (pj_int16_t*)cap_buf,
                                             strm->param.samples_per_frame);

                        if (strm->fmt_id == PJMEDIA_FORMAT_L16) {
                            /* PCM mode */
                            /* Prepare frame */
                            frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
                            frame.buf = (void *)strm->cap_buf;
                            frame.size = strm->bytes_per_frame;
                            frame.timestamp.u64 = strm->cap_timestamp.u64;
                            frame.bit_info = 0;
                        }
                        status = (*strm->cap_cb)(strm->user_data, &frame);

                        if (status == PJ_SUCCESS) {
                            /* Update position */
                            cap_buf = (pj_int16_t *)cap_buf +
                                      strm->param.samples_per_frame;

                            nsamples -= strm->param.samples_per_frame;
                            strm->cap_timestamp.u64 +=
                                                strm->param.samples_per_frame /
                                                strm->param.channel_count;
                        } else {
                            nsamples = 0;
                        }
                    }
                    strm->cap_buf_count = nsamples;

                    /* Store the remaining samples into the buffer */
                    if (nsamples && (status == PJ_SUCCESS)) {
                        pjmedia_copy_samples(strm->cap_buf, 
                                             (pj_int16_t*)cap_buf,
                                             nsamples);
                    }
                } else {
                    /* Not enough samples, let's just store them in the 
                     * buffer.
                     */
                    pjmedia_copy_samples(strm->cap_buf + strm->cap_buf_count,
                                         (pj_int16_t*)cap_buf,
                                         next_frame_size *
                                         strm->param.channel_count);

                    strm->cap_buf_count = nsamples;
                }
                hr = strm->cap_client->ReleaseBuffer(next_frame_size);

                hr = strm->cap_client->GetNextPacketSize(&packet_size);
                if (FAILED(hr)) {
                    PJ_LOG(4, (THIS_FILE, "Error getting next packet size"));
                    packet_size = 0;
                }
            }
        }
    }

    PJ_LOG(5,(THIS_FILE, "WASAPI: thread stopping.."));
    return 0;
}

static pj_status_t activate_capture_dev(struct wasapi_stream *ws)
{
    HRESULT hr = E_FAIL;

#if defined(USE_ASYNC_ACTIVATE)
    ComPtr<IActivateAudioInterfaceAsyncOperation> async_op;
    ws->cap_id = MediaDevice::GetDefaultAudioCaptureId(
                                              AudioDeviceRole::Communications);
#else
    ws->cap_id = GetDefaultAudioCaptureId(AudioDeviceRole::Communications);
#endif

    if (!ws->cap_id) {
        PJ_LOG(4, (THIS_FILE, "Error getting default capture device id"));
        return PJMEDIA_EAUD_SYSERR;
    }

#if defined(USE_ASYNC_ACTIVATE)

    ws->cap_aud_act = Make<AudioActivator>();
    if (ws->cap_aud_act == NULL) {
        PJ_LOG(4, (THIS_FILE, "Error activating capture device"));
        return PJMEDIA_EAUD_SYSERR;
    }
    hr = ActivateAudioInterfaceAsync(ws->cap_id->Data(), 
                                     __uuidof(IAudioClient2), 
                                     NULL, ws->cap_aud_act.Get(), 
                                     &async_op);

    //pj_thread_sleep(100);
    auto task_completed = create_task(ws->cap_aud_act->task_completed);
    task_completed.wait();    
    ws->default_cap_dev = task_completed.get().Get();
#else
    hr = ActivateAudioInterface(ws->cap_id, __uuidof(IAudioClient2),
                                (void**)&ws->default_cap_dev);
#endif
    AudioClientProperties properties = {};
    if (SUCCEEDED(hr))
    {
        properties.cbSize = sizeof AudioClientProperties;
        properties.eCategory = AudioCategory_Communications;
        hr = ws->default_cap_dev->SetClientProperties(&properties);
    }

    return FAILED(hr) ? PJMEDIA_EAUD_SYSERR : PJ_SUCCESS;
}

/* 
 * Init capture device
 */
static pj_status_t init_capture_dev(struct wasapi_stream *ws, 
                                    const pjmedia_aud_param *prm)
{
    pj_status_t status;
    HRESULT hr = E_FAIL;
    REFERENCE_TIME request_duration = 0;
    unsigned ptime;
    unsigned int stream_flags;
    WAVEFORMATEX wfx;
    int test;

    status = activate_capture_dev(ws);
    if (status != PJ_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Failed activating capture device"));
        goto on_exit;
    }

    stream_flags = 0x88140000;

    pj_bzero(&wfx, sizeof(WAVEFORMATEX));
    status = init_waveformatex(&wfx, prm, ws);

    if (status != PJ_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Error initiating wave format"));
        goto on_exit;
    }
    status = PJMEDIA_EAUD_SYSERR;

    ptime = prm->samples_per_frame * 1000 / 
            (prm->clock_rate * prm->channel_count);

    ws->bytes_per_frame = prm->samples_per_frame * ws->bytes_per_sample;

    /* Initialize the stream to play at the minimum latency. */
    hr = ws->default_cap_dev->GetDevicePeriod(NULL, &request_duration);

    EXIT_ON_ERROR(hr);

    test = ws->param.input_latency_ms * 10000;

    hr = ws->default_cap_dev->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         stream_flags,
                                         //2000 * 10000,
                                         ptime * ws->param.input_latency_ms * 
                                         10000,                                  
                                         0,
                                         &wfx,
                                         NULL);

    EXIT_ON_ERROR(hr);

    ws->cap_buf = (pj_int16_t *)pj_pool_zalloc(ws->pool, ws->bytes_per_frame);

    if (!ws->cap_buf) { 
        PJ_LOG(4, (THIS_FILE, "Error creating capture buffer"));
        status = PJ_ENOMEM;

        goto on_exit;
    }

    ws->cap_event = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
    if (!ws->cap_event) {
        hr = HRESULT_FROM_WIN32(GetLastError());        

        goto on_exit;
    }

    EXIT_ON_ERROR(hr);

    hr = ws->default_cap_dev->SetEventHandle(ws->cap_event);

    EXIT_ON_ERROR(hr);

    hr = ws->default_cap_dev->GetService(__uuidof(IAudioCaptureClient), 
                                         (void**)&ws->cap_client);

    EXIT_ON_ERROR(hr);

    PJ_LOG(4, (THIS_FILE, 
               "Wasapi Sound recorder initialized ("
               "clock_rate=%d, latency=%d, "
               "channel_count=%d, samples_per_frame=%d (%dms))",                       
               prm->clock_rate, ws->param.input_latency_ms, 
               prm->channel_count, prm->samples_per_frame,
               ptime));

    status = PJ_SUCCESS;
on_exit:
    if (status != PJ_SUCCESS)
        wasapi_stream_destroy(&ws->base);

    return status;
}

static pj_status_t activate_playback_dev(struct wasapi_stream *ws)
{
    HRESULT hr = E_FAIL;

#if defined(USE_ASYNC_ACTIVATE)
    ComPtr<IActivateAudioInterfaceAsyncOperation> async_op;

    ws->pb_id = MediaDevice::GetDefaultAudioRenderId(
                                              AudioDeviceRole::Communications);
#else
    ws->pb_id = GetDefaultAudioRenderId(AudioDeviceRole::Communications);
#endif

    if (!ws->pb_id) {
        PJ_LOG(4, (THIS_FILE, "Error getting default playback device id"));
        return PJMEDIA_EAUD_SYSERR;
    }

#if defined(USE_ASYNC_ACTIVATE)
    ws->pb_aud_act = Make<AudioActivator>();
    if (ws->pb_aud_act == NULL) {
        PJ_LOG(4, (THIS_FILE, "Error activating playback device"));
        return PJMEDIA_EAUD_SYSERR;
    }
    hr = ActivateAudioInterfaceAsync(ws->pb_id->Data(), 
                                     __uuidof(IAudioClient2),
                                     NULL, ws->pb_aud_act.Get(),
                                     &async_op);

    //pj_thread_sleep(100);
    auto task_completed = create_task(ws->pb_aud_act->task_completed);
    task_completed.wait();
    ws->default_pb_dev = task_completed.get().Get();
#else
    hr = ActivateAudioInterface(ws->pb_id, __uuidof(IAudioClient2),
                                (void**)&ws->default_pb_dev);
#endif

    AudioClientProperties properties = {};
    if (SUCCEEDED(hr))
    {
        properties.cbSize = sizeof AudioClientProperties;
        properties.eCategory = AudioCategory_Communications;
        hr = ws->default_pb_dev->SetClientProperties(&properties);
    }
    
    return FAILED(hr) ? PJMEDIA_EAUD_SYSERR : PJ_SUCCESS;
}

/* 
 * Init playback device
 */
static pj_status_t init_playback_dev(struct wasapi_stream *ws, 
                                     const pjmedia_aud_param *prm)
{
    HRESULT hr = E_FAIL;
    pj_status_t status;
    unsigned ptime;
    unsigned int stream_flags;
    WAVEFORMATEX wfx;

    status = activate_playback_dev(ws);
    if (status != PJ_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Failed activating playback device"));
        goto on_exit;
    }

    stream_flags = 0x88140000;
    pj_bzero(&wfx, sizeof(WAVEFORMATEX));

    status = init_waveformatex(&wfx, prm, ws);
    if (status != PJ_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Error initiating wave format"));

        goto on_exit;
    }
    status = PJMEDIA_EAUD_SYSERR;

    ptime = prm->samples_per_frame * 1000 / 
            (prm->clock_rate * prm->channel_count);

    ws->bytes_per_frame = prm->samples_per_frame * ws->bytes_per_sample;

    hr = ws->default_pb_dev->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                        stream_flags,                                   
                                        ws->param.output_latency_ms * 10000,
                                        //0,
                                        0,                                      
                                        &wfx,
                                        NULL);    

    EXIT_ON_ERROR(hr);

    hr = ws->default_pb_dev->GetBufferSize(&ws->pb_max_frame_count);    

    /* Create buffer */
    EXIT_ON_ERROR(hr);

    ws->pb_event = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
    if (!ws->pb_event) {
        hr = HRESULT_FROM_WIN32(GetLastError());        

        goto on_exit;
    }

    EXIT_ON_ERROR(hr);

    hr = ws->default_pb_dev->SetEventHandle(ws->pb_event);

    EXIT_ON_ERROR(hr);

    hr = ws->default_pb_dev->GetService(__uuidof(IAudioRenderClient), 
                                        (void**)&ws->pb_client);

    EXIT_ON_ERROR(hr);

    /* Other/optional supported interfaces */
    hr = ws->default_pb_dev->GetService(__uuidof(ISimpleAudioVolume), 
                                        (void**)&ws->pb_volume);

    if (FAILED(hr)) {
        PJ_LOG(4, (THIS_FILE, "Error getting vol service playback:0x%x", hr));
    }

    PJ_LOG(4, (THIS_FILE, 
               " Wasapi Sound player initialized ("
               "clock_rate=%d, latency=%d, "
               "channel_count=%d, samples_per_frame=%d (%dms))",                       
               prm->clock_rate, ws->param.output_latency_ms, 
               prm->channel_count, prm->samples_per_frame,
               ptime));  

    status = PJ_SUCCESS;
on_exit:
    if (status != PJ_SUCCESS)
        wasapi_stream_destroy(&ws->base);

    return status;
}

/*
 * wasapi - tests loads the audio units and sets up the driver structure
 */
static pj_status_t wasapi_add_dev(struct wasapi_factory *wf)
{
    pjmedia_aud_dev_info *adi;
    LPCWSTR capture_id = NULL;
    LPCWSTR render_id = NULL;

    if (wf->dev_count >= PJ_ARRAY_SIZE(wf->devs))
        return PJ_ETOOMANY;

    adi = &wf->devs[wf->dev_count];

    /* Reset device info */
    pj_bzero(adi, sizeof(*adi));

    /* Set device name */
    pj_ansi_strxcpy(adi->name, "default", sizeof(adi->name));
    pj_ansi_strxcpy(adi->driver, "wasapi", sizeof(adi->driver));

    /* Get default capture device */
#if defined(USE_ASYNC_ACTIVATE)
    capture_id = MediaDevice::GetDefaultAudioCaptureId(
                                      AudioDeviceRole::Communications)->Data();
#else
    capture_id = GetDefaultAudioCaptureId(AudioDeviceRole::Communications);
#endif

    if (!capture_id) {
        PJ_LOG(4, (THIS_FILE, "Failed to get default audio capture"));
    }

    /* Get default render device */
#if defined(USE_ASYNC_ACTIVATE)
    render_id = MediaDevice::GetDefaultAudioRenderId(
                                      AudioDeviceRole::Communications)->Data();
#else
    render_id = GetDefaultAudioRenderId(AudioDeviceRole::Communications);
#endif

    if (!render_id) {
        PJ_LOG(4, (THIS_FILE, "Failed to get default audio render"));
    }

    if (!capture_id && !render_id) {
        PJ_LOG(4, (THIS_FILE, "Unable to open default sound device"));
        return PJMEDIA_EAUD_NODEV;
    }

    /* Check the number of capture channels */
    adi->input_count = (capture_id) ? 1 : 0;

    /* Check the number of playback channels */
    adi->output_count = (render_id) ? 1 : 0;

    /* Set the default sample rate */
    adi->default_samples_per_sec = 8000;

    adi->caps = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
                PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY |
                PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE;

    adi->routes = PJMEDIA_AUD_DEV_ROUTE_DEFAULT |
                  PJMEDIA_AUD_DEV_ROUTE_EARPIECE |
                  PJMEDIA_AUD_DEV_ROUTE_LOUDSPEAKER |
                  PJMEDIA_AUD_DEV_ROUTE_BLUETOOTH;

    ++wf->dev_count;

    PJ_LOG(4, (THIS_FILE, "Added sound device %s", adi->name));

#if !defined(USE_ASYNC_ACTIVATE)
    if (capture_id)
        CoTaskMemFree((LPVOID*)capture_id);

    if (render_id)
        CoTaskMemFree((LPVOID*)render_id);
#endif

    return PJ_SUCCESS;
}

/****************************************************************************
 * Factory operations
 */
/*
 * Init wasapi audio driver.
 */

#ifdef __cplusplus
extern "C"{
#endif
pjmedia_aud_dev_factory* pjmedia_wasapi_factory(pj_pool_factory *pf)
{
    struct wasapi_factory *wf;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "WASAPI base", 256, 256, NULL);
    wf = PJ_POOL_ZALLOC_T(pool, struct wasapi_factory);
    wf->pf = pf;
    wf->base_pool = pool;
    wf->base.op = &factory_op;

    return &wf->base;
}
#ifdef __cplusplus
}
#endif

/* API: init factory */
static pj_status_t wasapi_factory_init(pjmedia_aud_dev_factory *f)
{
    pj_status_t status;
    status = wasapi_factory_refresh(f);
    if (status != PJ_SUCCESS)
        return status;

    PJ_LOG(4,(THIS_FILE, "wasapi initialized"));
    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t wasapi_factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;

    if (wf->pool) {
        TRACE_((THIS_FILE, "wasapi_factory_destroy()"));
        pj_pool_release(wf->pool);      
    }

    return PJ_SUCCESS;
}

/* API: refresh the list of devices */
static pj_status_t wasapi_factory_refresh(pjmedia_aud_dev_factory *f)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;
    pj_status_t status;
    
    TRACE_((THIS_FILE, "wasapi_factory_refresh()"));

    if (wf->pool != NULL) {
        pj_pool_release(wf->pool);
        wf->pool = NULL;
    }

    wf->pool = pj_pool_create(wf->pf, "wasapi_aud", 256, 256, NULL);
    wf->dev_count = 0;

    status = wasapi_add_dev(wf);

    PJ_LOG(4,(THIS_FILE, "wasapi driver found %d devices", wf->dev_count));

    return status;
}

/* API: get number of devices */
static unsigned wasapi_factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;
    return wf->dev_count;
}

/* API: get device info */
static pj_status_t wasapi_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                               unsigned index,
                                               pjmedia_aud_dev_info *info)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;

    PJ_ASSERT_RETURN(index>=0 && index<wf->dev_count, PJ_EINVAL);

    pj_memcpy(info, &wf->devs[index], sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t wasapi_factory_default_param(pjmedia_aud_dev_factory *f,
                                                unsigned index,
                                                pjmedia_aud_param *param)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;
    struct pjmedia_aud_dev_info *di;

    PJ_ASSERT_RETURN(index>=0 && index<wf->dev_count, PJ_EINVAL);

    di = &wf->devs[index];

    pj_bzero(param, sizeof(*param));
    if (di->input_count && di->output_count) {
        param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
        param->rec_id = index;
        param->play_id = index;
    } else if (di->input_count) {
        param->dir = PJMEDIA_DIR_CAPTURE;
        param->rec_id = index;
        param->play_id = PJMEDIA_AUD_INVALID_DEV;
    } else if (di->output_count) {
        param->dir = PJMEDIA_DIR_PLAYBACK;
        param->play_id = index;
        param->rec_id = PJMEDIA_AUD_INVALID_DEV;
    } else {
        return PJMEDIA_EAUD_INVDEV;
    }

    /* Set the mandatory settings here */
    /* The values here are just some examples */
    param->clock_rate = di->default_samples_per_sec;
    param->channel_count = 2;
    param->samples_per_frame = di->default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 32;
    param->flags = di->caps;
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    TRACE_((THIS_FILE, "wasapi_factory_default_param clock = %d flags = %d"
                       " spf = %d", param->clock_rate, param->flags,
                       param->samples_per_frame));

    return PJ_SUCCESS;
}

/* API: create stream */
static pj_status_t wasapi_factory_create_stream(pjmedia_aud_dev_factory *f,
                                                const pjmedia_aud_param *param,
                                                pjmedia_aud_rec_cb rec_cb,
                                                pjmedia_aud_play_cb play_cb,
                                                void *user_data,
                                                pjmedia_aud_stream **p_strm)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;
    pj_pool_t *pool;
    struct wasapi_stream *strm;
    pj_status_t status;

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(wf->pf, "wasapi-dev", 1024, 1024, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct wasapi_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->base.op   = &stream_op;
    strm->pool = pool;
    strm->cap_cb = rec_cb;
    strm->pb_cb = play_cb;
    strm->fmt_id = (pjmedia_format_id)param->ext_fmt.id;
    strm->user_data = user_data;
    strm->bytes_per_sample = param->bits_per_sample / 8;

    /* Init capture */
    if (param->dir & PJMEDIA_DIR_CAPTURE) {
        status = init_capture_dev(strm, param);
        if (status != PJ_SUCCESS) {
            wasapi_stream_destroy(&strm->base);
            return status;
        }
    }    
    /* Init playback */
    if (param->dir & PJMEDIA_DIR_PLAYBACK) {
        status = init_playback_dev(strm, param);        
        if (status != PJ_SUCCESS) {
            wasapi_stream_destroy(&strm->base);
            return status;
        }       
    }

    strm->quit_event = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, 
                                     EVENT_ALL_ACCESS);
    if (!strm->quit_event)
    {
        PJ_LOG(4, (THIS_FILE, "Error creating quit event:0x%x", 
                   HRESULT_FROM_WIN32(GetLastError())));

        return PJMEDIA_EAUD_SYSERR;                                        
    }
    
    /* Done */
    strm->base.op = &stream_op;
    *p_strm = &strm->base;

    return PJ_SUCCESS;
}

/* API: Get stream info. */
static pj_status_t wasapi_stream_get_param(pjmedia_aud_stream *s,
                                           pjmedia_aud_param *pi)
{
    struct wasapi_stream *strm = (struct wasapi_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t wasapi_stream_get_cap(pjmedia_aud_stream *s,
                                         pjmedia_aud_dev_cap cap,
                                         void *pval)
{
    struct wasapi_stream *strm = (struct wasapi_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap == PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY &&
        (strm->param.dir & PJMEDIA_DIR_CAPTURE))
    {
        /* Recording latency */
        *(unsigned*)pval = strm->param.input_latency_ms;
        return PJ_SUCCESS;

    } else if (cap == PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY &&
               (strm->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
        /* Playback latency */
        *(unsigned*)pval = strm->param.output_latency_ms;
        return PJ_SUCCESS;

    } else if (cap == PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) 
    {
        /* Output volume setting */
        *(unsigned*)pval = 0; // retrieve output device's volume here
        return PJ_SUCCESS;

    } else {
        return PJMEDIA_EAUD_INVCAP;
    }
}

/* API: set capability */
static pj_status_t wasapi_stream_set_cap(pjmedia_aud_stream *s,
                                         pjmedia_aud_dev_cap cap,
                                         const void *pval)
{
    struct wasapi_stream *strm = (struct wasapi_stream*)s;

    PJ_UNUSED_ARG(strm);
    PJ_UNUSED_ARG(cap);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    return PJMEDIA_EAUD_INVCAP;
}

/* API: Start stream. */
static pj_status_t wasapi_stream_start(pjmedia_aud_stream *strm)
{    
    struct wasapi_stream *ws = (struct wasapi_stream*)strm;
    HRESULT hr = E_FAIL;
    pj_status_t status = PJ_SUCCESS;

    PJ_ASSERT_RETURN(ws != NULL, PJ_EINVAL);

    TRACE_((THIS_FILE, "Starting wasapi audio stream"));

    if (ws->default_pb_dev) {
        hr = ws->default_pb_dev->Start();
        EXIT_ON_ERROR(hr);
    }

    if (ws->default_cap_dev) {
        hr = ws->default_cap_dev->Start();
        EXIT_ON_ERROR(hr);
    }

    /* Create and start the thread */
    if (!ws->thread) {
        status = pj_thread_create(ws->pool, "wasapi", &wasapi_dev_thread, strm, 
                                  0, 0, &ws->thread);

        if (status != PJ_SUCCESS) {
            PJ_LOG(4, (THIS_FILE, "Error creating wasapi thread:%d", status));
        }
    }

on_exit:
    if (status != PJ_SUCCESS) {
        PJ_LOG(4, (THIS_FILE, "Error starting wasapi:0x%x", hr));
        wasapi_stream_stop(&ws->base);  
    }

    return status;
}

/* API: Stop stream. */
static pj_status_t wasapi_stream_stop(pjmedia_aud_stream *strm)
{
    struct wasapi_stream *ws = (struct wasapi_stream*)strm;    
    HRESULT hr, hr_tmp;
    hr = hr_tmp = S_OK;    

    PJ_ASSERT_RETURN(ws != NULL, PJ_EINVAL);

    if (ws->default_pb_dev) {
        hr = ws->default_pb_dev->Stop();
        if (FAILED(hr)) {
            PJ_LOG(4, (THIS_FILE, "Error stopping wasapi playback stream:0x%x", 
                       hr));
        } else {
            PJ_LOG(4,(THIS_FILE, "Stopped wasapi playback stream"));
        }
    }
    
    if (ws->default_cap_dev) {
        hr_tmp = ws->default_cap_dev->Stop();
        if (FAILED(hr_tmp)) {
            hr = hr_tmp;
            PJ_LOG(4, (THIS_FILE, "Error stopping wasapi capture stream:0x%x", 
                       hr));
        } else {
            PJ_LOG(4,(THIS_FILE, "Stopped wasapi capture stream"));     
        }
    }

    if (FAILED(hr)) {
        return PJMEDIA_EAUD_WASAPI_ERROR;
    }

    return PJ_SUCCESS;
}

/* API: Destroy stream. */
static pj_status_t wasapi_stream_destroy(pjmedia_aud_stream *strm)
{
    struct wasapi_stream *ws = (struct wasapi_stream*)strm;

    PJ_ASSERT_RETURN(ws != NULL, PJ_EINVAL);

    wasapi_stream_stop(strm);

    /* Stop stream thread */
    if (ws->thread) {
        SetEvent(ws->quit_event);
        pj_thread_join(ws->thread);
        pj_thread_destroy(ws->thread);
        ws->thread = NULL;
    }

    /* Close thread quit event */
    if (ws->quit_event) {
        CloseHandle(ws->quit_event);
        ws->quit_event = NULL;
    }

    /* Close playback event */
    if (ws->pb_event) {
        CloseHandle(ws->pb_event);
        ws->pb_event = NULL;
    }

    /* Close capture event */
    if (ws->cap_event) {
        CloseHandle(ws->cap_event);
        ws->cap_event = NULL;
    }

    /* Release playback device */
    if (ws->default_pb_dev) {
        ws->default_pb_dev->Release();
        ws->default_pb_dev = NULL;
    }

    /* Release capture device */
    if (ws->default_cap_dev) {
        ws->default_cap_dev->Release();
        ws->default_cap_dev = NULL;
    }

    /* Release playback volume interface */
    if (ws->pb_volume) {
        ws->pb_volume->Release();
        ws->pb_volume = NULL;
    }


#if defined(USE_ASYNC_ACTIVATE)
    /* Release audio activator */
    if (ws->cap_aud_act) {
        ws->cap_aud_act = nullptr;
    }

    if (ws->pb_aud_act) {
        ws->pb_aud_act = nullptr;
    }

    if (ws->cap_id) {
        ws->cap_id = nullptr;
    }

    if (ws->pb_id) {
        ws->pb_id = nullptr;
    }
#else
    if (ws->cap_id) {
        CoTaskMemFree((LPVOID)ws->cap_id);
    }

    if (ws->pb_id) {
        CoTaskMemFree((LPVOID)ws->pb_id);
    }
#endif

    pj_pool_release(ws->pool);

    return PJ_SUCCESS;
}

#if defined(USE_ASYNC_ACTIVATE)
HRESULT AudioActivator::ActivateCompleted(
                               IActivateAudioInterfaceAsyncOperation *pAsyncOp)
{
    HRESULT hr = S_OK, hr2 = S_OK;
    IUnknown *aud_interface = NULL;
    IAudioClient2 *aud_client = NULL;

    hr = pAsyncOp->GetActivateResult(&hr2, &aud_interface);
    if (SUCCEEDED(hr) && SUCCEEDED(hr2)) {
        aud_interface->QueryInterface(IID_PPV_ARGS(&aud_client));
        if (aud_client)
        {
            pj_thread_desc thread_desc;
            pj_thread_t *act_thread;
            pj_bzero(thread_desc, sizeof(pj_thread_desc));

            if (!pj_thread_is_registered()) {
                pj_thread_register("activator", thread_desc, &act_thread);
            }       
        } 
    }     
    
    task_completed.set(aud_client);
    return hr;
}

#endif

#endif  /* PJMEDIA_AUDIO_DEV_HAS_WASAPI */
