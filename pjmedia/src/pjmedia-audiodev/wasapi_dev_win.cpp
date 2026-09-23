/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
 * Contributed by Andreas Ahland
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
#include <pjmedia/event.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>

/* WASAPI for Windows Desktop. The existing driver (wasapi_dev.cpp) only
 * supports Windows Phone 8 / UWP and is left unchanged; exactly one of the
 * two files is built, depending on the platform. */
#if defined(PJMEDIA_AUDIO_DEV_HAS_WASAPI) && \
    PJMEDIA_AUDIO_DEV_HAS_WASAPI != 0 && \
    !(defined(PJ_WIN32_UWP) && PJ_WIN32_UWP != 0) && \
    !(defined(PJ_WIN32_WINPHONE8) && PJ_WIN32_WINPHONE8 != 0)

/* ===========================================================================
 * Windows Desktop (Win32/Win64)
 *
 * WASAPI in shared mode, event driven. It is an alternative to WMME with a
 * lower latency: since Vista, WMME itself is only a layer on top of WASAPI
 * and it adds buffering of its own.
 *
 * The device list is modelled on WMME, so that applications can keep finding
 * their sound cards by matching parts of the device name:
 *   index 0     "Wave mapper" = default capture + default playback
 *                               (eConsole, like the WMME WAVE_MAPPER)
 *   then        all active capture endpoints  (input_count only)
 *   then        all active render endpoints   (output_count only)
 * The name is the friendly name of the endpoint, in ANSI as WMME does with
 * PJMEDIA_WMME_DEV_USE_MMDEVICE_API. The channel count comes from the mix
 * format.
 *
 * Index 0 resolves the default endpoints when the stream is created and then
 * stays on them: unlike the WMME WAVE_MAPPER, an IAudioClient is bound to a
 * concrete endpoint by Initialize(). Losing that endpoint therefore stops the
 * stream with PJMEDIA_EVENT_AUD_DEV_ERROR rather than following the new
 * default, leaving the choice of the replacement device to the application.
 *
 * The stream asks for 16 bit PCM at the requested rate and channel count;
 * AUTOCONVERTPCM lets the audio engine convert to its mix format (both rate
 * and channels). That way 8/16/48 kHz and mono/multichannel work without a
 * resampler here.
 *
 * Latency:
 *   capture     Data is passed on as soon as one pjmedia frame is full (the
 *               engine period, usually 10 ms, plus the time to collect up to
 *               the frame length). input_latency_ms only sizes the buffer
 *               against overflow.
 *   playback    The buffer is refilled up to output_latency_ms, but at least
 *               up to frame length + period. Less is not possible, because
 *               pjmedia delivers whole frames only.
 *
 * All COM calls for a stream run in its own audio thread (MTA). The library
 * may be initialized from an application thread that is STA, and audio
 * objects created there would be bound to it, so start/stop/volume are sent
 * to the audio thread as a command.
 * ======================================================================== */

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <pj/string.h>
#include <pj/unicode.h>

#define THIS_FILE               "wasapi_dev_win.cpp"

/* From audiosessiontypes.h, missing in older SDKs */
#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#   define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM       0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#   define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY  0x08000000
#endif

#define WASAPI_STREAM_FLAGS     (AUDCLNT_STREAMFLAGS_EVENTCALLBACK | \
                                 AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | \
                                 AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY)

/* Name of the default device, the same name WMME gives to WAVE_MAPPER, so
 * that an application moving over only has to change the driver name in
 * pjmedia_aud_dev_lookup(). */
#define WASAPI_DEFAULT_NAME     "Wave mapper"

/* How long start/stop/destroy wait for the audio thread */
#define WASAPI_CMD_TIMEOUT_MS   5000

/* Minimum capture buffer size in frames (overflow protection only) */
#define WASAPI_CAP_MIN_FRAMES   4

#define WASAPI_MAX_DEVS         PJMEDIA_AUD_DEV_MAX_DEVS

#define SAFE_RELEASE(p)         do { if (p) { (p)->Release(); (p) = NULL; } \
                                } while (0)

/* AudioClientProperties including Options (Windows 8.1). Redefined locally
 * because pjlib sets _WIN32_WINNT to XP, which makes the SDK hide those
 * fields; for Windows, cbSize = 16 identifies the 8.1 version. */
#if PJMEDIA_WASAPI_DEV_USE_RAW_MODE
typedef struct wasapi_client_props
{
    UINT32  cbSize;
    BOOL    bIsOffload;
    int     eCategory;          /* AUDIO_STREAM_CATEGORY, 0 = Other     */
    int     Options;            /* AUDCLNT_STREAMOPTIONS                */
} wasapi_client_props;

#define WASAPI_STREAMOPTIONS_RAW    0x1
#endif

/* PKEY_Device_FriendlyName from functiondiscoverykeys_devpkey.h, local here
 * so that no GUID instantiation (INITGUID) is needed. */
static const PROPERTYKEY wasapi_pkey_friendly_name =
{
    { 0xa45c254e, 0xdf1c, 0x4efd,
      { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } },
    14
};

enum wasapi_cmd
{
    WASAPI_CMD_NONE,
    WASAPI_CMD_START,
    WASAPI_CMD_STOP,
    WASAPI_CMD_QUIT,
    WASAPI_CMD_GET_VOLUME,
    WASAPI_CMD_SET_VOLUME
};

struct wasapi_dev
{
    pjmedia_aud_dev_info info;
    WCHAR               *cap_id;        /* Capture endpoint, NULL = none      */
    WCHAR               *pb_id;         /* Render endpoint                    */
    pj_bool_t            is_default;    /* Default devices, not fixed IDs     */
};

/* WASAPI factory */
struct wasapi_factory
{
    pjmedia_aud_dev_factory base;
    pj_pool_t              *base_pool;
    pj_pool_t              *pool;
    pj_pool_factory        *pf;

    unsigned                dev_count;
    struct wasapi_dev      *devs;
};

/* Sound stream. */
struct wasapi_stream
{
    pjmedia_aud_stream      base;
    pjmedia_aud_param       param;          /* with effective latencies    */
    pj_pool_t              *pool;
    void                   *user_data;
    pjmedia_aud_rec_cb      rec_cb;
    pjmedia_aud_play_cb     play_cb;

    unsigned                frame_samples;  /* samples_per_frame, all chans */
    unsigned                frame_frames;   /* WASAPI frames per pj frame   */
    unsigned                frame_bytes;
    unsigned                ptime_ms;

    /* Endpoints (copies, opened by the audio thread) */
    WCHAR                  *cap_id;
    pj_bool_t               cap_default;
    WCHAR                  *pb_id;
    pj_bool_t               pb_default;

    /* Audio thread and commands */
    pj_thread_t            *thread;
    DWORD                   thread_id;
    CRITICAL_SECTION        cmd_lock;
    pj_bool_t               cmd_lock_init;
    HANDLE                  cmd_event;      /* A command is pending         */
    HANDLE                  done_event;     /* Command is done              */
    enum wasapi_cmd         cmd;
    float                   cmd_volume;
    pj_status_t             cmd_status;
    pj_bool_t               cmd_dead;       /* Thread stopped answering     */

    pj_bool_t               running;
    pj_bool_t               halted;         /* Device gone/callback error   */
    REFERENCE_TIME          period;         /* Engine period (100 ns)       */

    /* Playback */
    IAudioClient           *pb_client;
    IAudioRenderClient     *pb_render;
    ISimpleAudioVolume     *pb_volume;
    HANDLE                  pb_event;
    UINT32                  pb_buf_frames;
    UINT32                  pb_target_frames;
    pj_timestamp            pb_ts;
    unsigned                pb_underrun;

    /* Capture */
    IAudioClient           *cap_client;
    IAudioCaptureClient    *cap_capture;
    HANDLE                  cap_event;
    pj_int16_t             *cap_buf;        /* collects one pjmedia frame   */
    unsigned                cap_len;        /* Samples in cap_buf           */
    pj_timestamp            cap_ts;
    unsigned                cap_glitch;
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


/* ---------------------------------------------------------------------------
 * Helper functions
 * ------------------------------------------------------------------------- */

static void log_hr(const char *what, HRESULT hr)
{
    PJ_LOG(2, (THIS_FILE, "WASAPI: %s failed (hr=0x%08lx)", what,
               (unsigned long)hr));
}

static WCHAR *wstr_dup(pj_pool_t *pool, const WCHAR *s)
{
    pj_size_t len = wcslen(s) + 1;
    WCHAR *d = (WCHAR*)pj_pool_alloc(pool, len * sizeof(WCHAR));
    pj_memcpy(d, s, len * sizeof(WCHAR));
    return d;
}

static unsigned hns_to_ms(REFERENCE_TIME t)
{
    return (unsigned)((t + 5000) / 10000);
}

/* Friendly name of an endpoint, in ANSI as WMME does */
static void get_endpoint_name(IMMDevice *dev, char *buf, int size)
{
    IPropertyStore *props = NULL;
    PROPVARIANT v;

    buf[0] = '\0';
    PropVariantInit(&v);
    if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) &&
        SUCCEEDED(props->GetValue(wasapi_pkey_friendly_name, &v)) &&
        v.vt == VT_LPWSTR && v.pwszVal)
    {
        pj_unicode_to_ansi(v.pwszVal, wcslen(v.pwszVal), buf, size);
    }
    PropVariantClear(&v);
    SAFE_RELEASE(props);
}

/* Channel count and rate of the mix format */
static void get_endpoint_format(IMMDevice *dev, unsigned *channels,
                                unsigned *rate)
{
    IAudioClient *client = NULL;
    WAVEFORMATEX *wfx = NULL;

    *channels = 2;
    *rate = 48000;

    if (SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
                                (void**)&client)) &&
        SUCCEEDED(client->GetMixFormat(&wfx)) && wfx)
    {
        /* Report at least 2 channels, as WMME did even for mono
          * microphones, so that applications auto-detecting the channel
          * count keep seeing the same value. The engine converts the
          * channels anyway (AUTOCONVERTPCM). */
        if (wfx->nChannels > 2 && wfx->nChannels <= 256)
            *channels = wfx->nChannels;
        if (wfx->nSamplesPerSec)
            *rate = wfx->nSamplesPerSec;
    }
    if (wfx)
        CoTaskMemFree(wfx);
    SAFE_RELEASE(client);
}

static void fill_dev_info(struct wasapi_dev *d, unsigned in, unsigned out,
                          unsigned rate)
{
    pj_ansi_strxcpy(d->info.driver, "WASAPI", sizeof(d->info.driver));
    d->info.input_count = in;
    d->info.output_count = out;
    d->info.default_samples_per_sec = rate;
    d->info.caps = 0;
    if (in)
        d->info.caps |= PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY;
    if (out)
        d->info.caps |= PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY |
                        PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
}

/* MMCSS "Pro Audio" for the audio thread. avrt.dll is loaded dynamically so
 * that applications do not have to link against it. */
typedef HANDLE (WINAPI *av_set_mm_thread_char_t)(LPCWSTR, LPDWORD);
typedef BOOL   (WINAPI *av_revert_mm_thread_char_t)(HANDLE);

static HANDLE mmcss_enter(HMODULE *p_avrt)
{
    av_set_mm_thread_char_t fn;
    HANDLE h = NULL;
    DWORD task_idx = 0;

    *p_avrt = LoadLibraryW(L"avrt.dll");
    if (*p_avrt) {
        fn = (av_set_mm_thread_char_t)
             GetProcAddress(*p_avrt, "AvSetMmThreadCharacteristicsW");
        if (fn)
            h = fn(L"Pro Audio", &task_idx);
    }
    if (!h) {
        PJ_LOG(4, (THIS_FILE, "WASAPI: MMCSS not available, using thread "
                   "priority"));
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    }
    return h;
}

static void mmcss_leave(HMODULE avrt, HANDLE h)
{
    if (avrt) {
        if (h) {
            av_revert_mm_thread_char_t fn = (av_revert_mm_thread_char_t)
                GetProcAddress(avrt, "AvRevertMmThreadCharacteristics");
            if (fn)
                fn(h);
        }
        FreeLibrary(avrt);
    }
}


/* ---------------------------------------------------------------------------
 * Factory
 * ------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
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

static pj_status_t wasapi_factory_init(pjmedia_aud_dev_factory *f)
{
    pj_status_t status = wasapi_factory_refresh(f);
    if (status != PJ_SUCCESS)
        return status;

    PJ_LOG(4, (THIS_FILE, "WASAPI initialized"));
    return PJ_SUCCESS;
}

static pj_status_t wasapi_factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;
    pj_pool_t *pool = wf->base_pool;

    if (wf->pool)
        pj_pool_release(wf->pool);
    wf->pool = NULL;
    wf->base_pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* Append the endpoints of one direction */
static void add_endpoints(struct wasapi_factory *wf, IMMDeviceCollection *col,
                          UINT count, pj_bool_t capture, unsigned max)
{
    UINT i;

    for (i = 0; i < count && wf->dev_count < max; ++i) {
        IMMDevice *dev = NULL;
        LPWSTR id = NULL;
        struct wasapi_dev *d;
        unsigned ch, rate;

        if (FAILED(col->Item(i, &dev)))
            continue;
        if (FAILED(dev->GetId(&id)) || !id) {
            SAFE_RELEASE(dev);
            continue;
        }

        d = &wf->devs[wf->dev_count];
        pj_bzero(d, sizeof(*d));
        get_endpoint_name(dev, d->info.name, sizeof(d->info.name));
        if (d->info.name[0] == '\0') {
            pj_ansi_snprintf(d->info.name, sizeof(d->info.name),
                             "WASAPI %s %u", capture ? "in" : "out", i);
        }
        get_endpoint_format(dev, &ch, &rate);
        if (capture) {
            d->cap_id = wstr_dup(wf->pool, id);
            fill_dev_info(d, ch, 0, rate);
        } else {
            d->pb_id = wstr_dup(wf->pool, id);
            fill_dev_info(d, 0, ch, rate);
        }
        ++wf->dev_count;

        CoTaskMemFree(id);
        SAFE_RELEASE(dev);
    }
}

static pj_status_t wasapi_factory_refresh(pjmedia_aud_dev_factory *f)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;
    HRESULT co, hr;
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDeviceCollection *caps = NULL, *pbs = NULL;
    IMMDevice *def_cap = NULL, *def_pb = NULL;
    UINT ncap = 0, npb = 0;
    unsigned max, i;

    if (wf->pool != NULL) {
        pj_pool_release(wf->pool);
        wf->pool = NULL;
    }
    wf->pool = pj_pool_create(wf->pf, "wasapi_aud", 1000, 1000, NULL);
    wf->dev_count = 0;
    wf->devs = NULL;

    /* Enumeration only, everything is released again here, so an STA
     * thread (RPC_E_CHANGED_MODE) is fine for this. */
    co = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) {
        log_hr("CoCreateInstance(MMDeviceEnumerator)", hr);
        goto on_return;
    }

    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE,
                                                 &caps)))
        caps->GetCount(&ncap);
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                                 &pbs)))
        pbs->GetCount(&npb);

    max = 1 + ncap + npb;
    if (max > WASAPI_MAX_DEVS)
        max = WASAPI_MAX_DEVS;
    wf->devs = (struct wasapi_dev*)
               pj_pool_calloc(wf->pool, max, sizeof(struct wasapi_dev));

    /* Index 0: the default devices, only when both directions exist (WMME
     * adds WAVE_MAPPER under the same condition). */
    if (ncap && npb &&
        SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eCapture, eConsole,
                                                      &def_cap)) &&
        SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                      &def_pb)))
    {
        struct wasapi_dev *d = &wf->devs[wf->dev_count++];
        unsigned in, out, rate_in, rate_out;

        get_endpoint_format(def_cap, &in, &rate_in);
        get_endpoint_format(def_pb, &out, &rate_out);
        pj_ansi_strxcpy(d->info.name, WASAPI_DEFAULT_NAME,
                        sizeof(d->info.name));
        d->is_default = PJ_TRUE;
        fill_dev_info(d, in, out, rate_out);
    }

    if (caps)
        add_endpoints(wf, caps, ncap, PJ_TRUE, max);
    if (pbs)
        add_endpoints(wf, pbs, npb, PJ_FALSE, max);

on_return:
    SAFE_RELEASE(def_cap);
    SAFE_RELEASE(def_pb);
    SAFE_RELEASE(caps);
    SAFE_RELEASE(pbs);
    SAFE_RELEASE(enumerator);
    if (SUCCEEDED(co))
        CoUninitialize();

    PJ_LOG(4, (THIS_FILE, "WASAPI found %d devices:", wf->dev_count));
    for (i = 0; i < wf->dev_count; ++i) {
        PJ_LOG(4, (THIS_FILE, " dev_id %d: %s  (in=%d, out=%d, %d Hz)", i,
                   wf->devs[i].info.name, wf->devs[i].info.input_count,
                   wf->devs[i].info.output_count,
                   wf->devs[i].info.default_samples_per_sec));
    }

    /* As WMME: succeed even with no device, otherwise pjsua fails with
     * --null-audio on machines without a sound card. */
    return PJ_SUCCESS;
}

static unsigned wasapi_factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;
    return wf->dev_count;
}

static pj_status_t wasapi_factory_get_dev_info(pjmedia_aud_dev_factory *f,
                                               unsigned index,
                                               pjmedia_aud_dev_info *info)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;

    PJ_ASSERT_RETURN(index < wf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_memcpy(info, &wf->devs[index].info, sizeof(*info));
    return PJ_SUCCESS;
}

static pj_status_t wasapi_factory_default_param(pjmedia_aud_dev_factory *f,
                                                unsigned index,
                                                pjmedia_aud_param *param)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;
    const pjmedia_aud_dev_info *di;

    PJ_ASSERT_RETURN(index < wf->dev_count, PJMEDIA_EAUD_INVDEV);
    di = &wf->devs[index].info;

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

    param->clock_rate = 16000;
    param->channel_count = 1;
    param->samples_per_frame = param->clock_rate * 20 / 1000;
    param->bits_per_sample = 16;
    param->flags = di->caps & (PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
                               PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY);
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    return PJ_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * Audio thread
 * ------------------------------------------------------------------------- */

/* Get the endpoint for the stream (in the audio thread) */
static HRESULT open_endpoint(IMMDeviceEnumerator *enumerator, EDataFlow flow,
                             pj_bool_t use_default, const WCHAR *id,
                             IMMDevice **p_dev)
{
    if (use_default)
        return enumerator->GetDefaultAudioEndpoint(flow, eConsole, p_dev);
    return enumerator->GetDevice(id, p_dev);
}

static pj_status_t init_client(struct wasapi_stream *s, IMMDevice *dev,
                               pj_bool_t playback)
{
    const pjmedia_aud_param *prm = &s->param;
    IAudioClient *client = NULL;
    WAVEFORMATEX wfx;
    REFERENCE_TIME def_period = 0, min_period = 0, latency = 0;
    unsigned period_ms, buffer_ms, target_ms = 0;
    UINT32 buf_frames = 0;
    pj_bool_t raw = PJ_FALSE;
    HRESULT hr;

    hr = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
                       (void**)&client);
    if (FAILED(hr)) {
        log_hr("IMMDevice::Activate", hr);
        return PJMEDIA_EAUD_SYSERR;
    }

#if PJMEDIA_WASAPI_DEV_USE_RAW_MODE
    /* Raw mode has to be set before Initialize() */
    {
        IAudioClient2 *client2 = NULL;

        hr = client->QueryInterface(__uuidof(IAudioClient2),
                                    (void**)&client2);
        if (SUCCEEDED(hr)) {
            wasapi_client_props props;

            pj_bzero(&props, sizeof(props));
            props.cbSize = sizeof(props);
            props.Options = WASAPI_STREAMOPTIONS_RAW;
            hr = client2->SetClientProperties(
                                (const AudioClientProperties*)&props);
            raw = SUCCEEDED(hr);
            SAFE_RELEASE(client2);
        }
        if (!raw) {
            PJ_LOG(4, (THIS_FILE, "WASAPI %s: raw mode not available "
                       "(hr=0x%08lx)", playback ? "playback" : "capture",
                       (unsigned long)hr));
        }
    }
#endif

    pj_bzero(&wfx, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)prm->channel_count;
    wfx.nSamplesPerSec = prm->clock_rate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (WORD)(prm->channel_count * 2);
    wfx.nAvgBytesPerSec = prm->clock_rate * wfx.nBlockAlign;

    client->GetDevicePeriod(&def_period, &min_period);
    if (def_period <= 0)
        def_period = 100000;                    /* 10 ms */
    s->period = def_period;
    period_ms = hns_to_ms(def_period);

    /* The latency members are optional, they are only meaningful when the
     * matching capability flag is set, so fall back to the defaults as WMME
     * does.
     */
    if (playback) {
        target_ms = (prm->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY)?
                    prm->output_latency_ms : PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
        if (target_ms < s->ptime_ms + period_ms)
            target_ms = s->ptime_ms + period_ms;
        buffer_ms = target_ms + s->ptime_ms;
    } else {
        buffer_ms = (prm->flags & PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY)?
                    prm->input_latency_ms : PJMEDIA_SND_DEFAULT_REC_LATENCY;
        if (buffer_ms < WASAPI_CAP_MIN_FRAMES * s->ptime_ms)
            buffer_ms = WASAPI_CAP_MIN_FRAMES * s->ptime_ms;
    }

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, WASAPI_STREAM_FLAGS,
                            (REFERENCE_TIME)buffer_ms * 10000, 0, &wfx, NULL);
    if (FAILED(hr)) {
        PJ_LOG(2, (THIS_FILE, "WASAPI: %s Initialize(%u Hz, %u ch, %u ms) "
                   "failed (hr=0x%08lx)", playback ? "playback" : "capture",
                   prm->clock_rate, prm->channel_count, buffer_ms,
                   (unsigned long)hr));
        SAFE_RELEASE(client);
        return (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) ? PJMEDIA_EAUD_BADFORMAT :
                                                      PJMEDIA_EAUD_SYSERR;
    }

    client->GetBufferSize(&buf_frames);
    client->GetStreamLatency(&latency);

    if (playback) {
        hr = client->SetEventHandle(s->pb_event);
        if (SUCCEEDED(hr))
            hr = client->GetService(__uuidof(IAudioRenderClient),
                                    (void**)&s->pb_render);
        if (FAILED(hr)) {
            log_hr("render client", hr);
            SAFE_RELEASE(client);
            return PJMEDIA_EAUD_SYSERR;
        }
        /* Volume is optional */
        client->GetService(__uuidof(ISimpleAudioVolume),
                           (void**)&s->pb_volume);

        s->pb_client = client;
        s->pb_buf_frames = buf_frames;
        s->pb_target_frames = target_ms * prm->clock_rate / 1000;
        if (s->pb_target_frames > buf_frames)
            s->pb_target_frames = buf_frames;
        if (s->pb_target_frames < s->frame_frames)
            s->pb_target_frames = s->frame_frames;
        s->param.output_latency_ms = s->pb_target_frames * 1000 /
                                     prm->clock_rate;
        s->param.flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
    } else {
        hr = client->SetEventHandle(s->cap_event);
        if (SUCCEEDED(hr))
            hr = client->GetService(__uuidof(IAudioCaptureClient),
                                    (void**)&s->cap_capture);
        if (FAILED(hr)) {
            log_hr("capture client", hr);
            SAFE_RELEASE(client);
            return PJMEDIA_EAUD_SYSERR;
        }
        s->cap_client = client;
        /* Effective: until one frame is full, plus the period */
        s->param.input_latency_ms = s->ptime_ms + period_ms;
        s->param.flags |= PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY;
    }

    PJ_LOG(4, (THIS_FILE, "WASAPI %s: %u Hz, %u ch, %u ms frames, "
               "engine period %u ms (min %u.%u ms), stream latency %u ms, "
               "buffer %u frames (%u ms)%s",
               playback ? "playback" : "capture",
               prm->clock_rate, prm->channel_count, s->ptime_ms, period_ms,
               (unsigned)(min_period / 10000),
               (unsigned)((min_period / 1000) % 10),
               hns_to_ms(latency), buf_frames,
               buf_frames * 1000 / prm->clock_rate,
               raw ? ", raw mode" : ""));
    if (playback) {
        PJ_LOG(4, (THIS_FILE, "WASAPI playback: fill level %u ms",
                   s->param.output_latency_ms));
    }

    return PJ_SUCCESS;
}

static pj_status_t thread_init(struct wasapi_stream *s)
{
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDevice *dev = NULL;
    pj_status_t status = PJ_SUCCESS;
    HRESULT hr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) {
        log_hr("CoCreateInstance(MMDeviceEnumerator)", hr);
        return PJMEDIA_EAUD_SYSERR;
    }

    if (s->param.dir & PJMEDIA_DIR_CAPTURE) {
        hr = open_endpoint(enumerator, eCapture, s->cap_default, s->cap_id,
                           &dev);
        if (FAILED(hr)) {
            log_hr("open capture endpoint", hr);
            status = PJMEDIA_EAUD_INVDEV;
        } else {
            status = init_client(s, dev, PJ_FALSE);
        }
        SAFE_RELEASE(dev);
    }

    if (status == PJ_SUCCESS && (s->param.dir & PJMEDIA_DIR_PLAYBACK)) {
        hr = open_endpoint(enumerator, eRender, s->pb_default, s->pb_id,
                           &dev);
        if (FAILED(hr)) {
            log_hr("open playback endpoint", hr);
            status = PJMEDIA_EAUD_INVDEV;
        } else {
            status = init_client(s, dev, PJ_TRUE);
        }
        SAFE_RELEASE(dev);
    }

    SAFE_RELEASE(enumerator);
    return status;
}

static void thread_release(struct wasapi_stream *s)
{
    SAFE_RELEASE(s->pb_volume);
    SAFE_RELEASE(s->pb_render);
    SAFE_RELEASE(s->pb_client);
    SAFE_RELEASE(s->cap_capture);
    SAFE_RELEASE(s->cap_client);
}

static pj_status_t do_stop(struct wasapi_stream *s);

/* Stop serving the device until the application recreates the stream, and say
 * why: it cannot tell from the callbacks alone that they will never come
 * back. */
static void halt_stream(struct wasapi_stream *s, pjmedia_dir dir,
                        pj_status_t status)
{
    pjmedia_event e;
    pj_timestamp *ts;

    if (s->halted)
        return;

    s->halted = PJ_TRUE;

    /* The stream is documented to stop, so stop it for real: leaving it
     * running would keep the thread waking on its timeout and would make
     * do_start() report success without restarting anything.
     */
    do_stop(s);

    ts = (dir == PJMEDIA_DIR_PLAYBACK)? &s->pb_ts : &s->cap_ts;
    pjmedia_event_init(&e, PJMEDIA_EVENT_AUD_DEV_ERROR, ts, &s->base);
    e.data.aud_dev_err.dir = dir;
    e.data.aud_dev_err.status = status;
    e.data.aud_dev_err.id = (dir == PJMEDIA_DIR_PLAYBACK)? s->param.play_id :
                                                           s->param.rec_id;
    pjmedia_event_publish(NULL, &s->base, &e, PJMEDIA_EVENT_PUBLISH_DEFAULT);
}

/* The device failed, e.g: a USB headset was unplugged. */
static void halt_stream_hr(struct wasapi_stream *s, pjmedia_dir dir,
                           const char *what, HRESULT hr)
{
    if (s->halted)
        return;

    if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
        PJ_LOG(2, (THIS_FILE, "WASAPI: %s: device removed", what));
    } else {
        log_hr(what, hr);
    }
    halt_stream(s, dir, PJMEDIA_EAUD_SYSERR);
}

static void process_capture(struct wasapi_stream *s)
{
    for (;;) {
        UINT32 packet = 0, frames = 0;
        BYTE *data = NULL;
        DWORD flags = 0;
        const pj_int16_t *src;
        unsigned remaining;
        HRESULT hr;

        hr = s->cap_capture->GetNextPacketSize(&packet);
        if (FAILED(hr)) {
            halt_stream_hr(s, PJMEDIA_DIR_CAPTURE,
                           "capture GetNextPacketSize", hr);
            return;
        }
        if (packet == 0)
            return;

        hr = s->cap_capture->GetBuffer(&data, &frames, &flags, NULL, NULL);
        if (hr == AUDCLNT_S_BUFFER_EMPTY)
            return;
        if (FAILED(hr)) {
            halt_stream_hr(s, PJMEDIA_DIR_CAPTURE, "capture GetBuffer", hr);
            return;
        }

        if (flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)
            ++s->cap_glitch;

        src = (flags & AUDCLNT_BUFFERFLAGS_SILENT) ? NULL :
              (const pj_int16_t*)data;
        remaining = frames * s->param.channel_count;

        while (remaining) {
            unsigned n = s->frame_samples - s->cap_len;
            if (n > remaining)
                n = remaining;

            if (src) {
                pjmedia_copy_samples(s->cap_buf + s->cap_len, src, n);
                src += n;
            } else {
                pjmedia_zero_samples(s->cap_buf + s->cap_len, n);
            }
            s->cap_len += n;
            remaining -= n;

            if (s->cap_len == s->frame_samples) {
                pjmedia_frame frame;
                pj_status_t status;

                frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
                frame.buf = s->cap_buf;
                frame.size = s->frame_bytes;
                frame.timestamp.u64 = s->cap_ts.u64;
                frame.bit_info = 0;

                status = (*s->rec_cb)(s->user_data, &frame);
                s->cap_ts.u64 += s->frame_frames;
                s->cap_len = 0;

                if (status != PJ_SUCCESS) {
                    s->cap_capture->ReleaseBuffer(frames);
                    PJ_PERROR(4, (THIS_FILE, status, "WASAPI: capture "
                                  "callback failed, stopping"));
                    halt_stream(s, PJMEDIA_DIR_CAPTURE, status);
                    return;
                }

                /* The callback may have stopped the stream */
                if (!s->running || s->halted) {
                    s->cap_capture->ReleaseBuffer(frames);
                    return;
                }
            }
        }

        s->cap_capture->ReleaseBuffer(frames);
    }
}

static void process_playback(struct wasapi_stream *s)
{
    UINT32 padding = 0;
    HRESULT hr;

    hr = s->pb_client->GetCurrentPadding(&padding);
    if (FAILED(hr)) {
        halt_stream_hr(s, PJMEDIA_DIR_PLAYBACK, "playback GetCurrentPadding",
                       hr);
        return;
    }
    if (padding == 0 && s->pb_ts.u64 != 0)
        ++s->pb_underrun;

    while (padding + s->frame_frames <= s->pb_target_frames) {
        BYTE *buf = NULL;
        pjmedia_frame frame;
        pj_status_t status;

        hr = s->pb_render->GetBuffer(s->frame_frames, &buf);
        if (hr == AUDCLNT_E_BUFFER_TOO_LARGE)
            return;
        if (FAILED(hr)) {
            halt_stream_hr(s, PJMEDIA_DIR_PLAYBACK, "playback GetBuffer", hr);
            return;
        }

        frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
        frame.buf = buf;
        frame.size = s->frame_bytes;
        frame.timestamp.u64 = s->pb_ts.u64;
        frame.bit_info = 0;

        status = (*s->play_cb)(s->user_data, &frame);

        s->pb_render->ReleaseBuffer(s->frame_frames,
                                    (status != PJ_SUCCESS ||
                                     frame.type != PJMEDIA_FRAME_TYPE_AUDIO) ?
                                    AUDCLNT_BUFFERFLAGS_SILENT : 0);
        s->pb_ts.u64 += s->frame_frames;
        padding += s->frame_frames;

        if (status != PJ_SUCCESS) {
            PJ_PERROR(4, (THIS_FILE, status, "WASAPI: playback callback "
                          "failed, stopping"));
            halt_stream(s, PJMEDIA_DIR_PLAYBACK, status);
            return;
        }

        /* The callback may have stopped the stream */
        if (!s->running || s->halted)
            return;
    }
}

static pj_status_t do_start(struct wasapi_stream *s)
{
    HRESULT hr;

    if (s->running)
        return PJ_SUCCESS;

    s->halted = PJ_FALSE;
    s->cap_len = 0;

    if (s->pb_client) {
        /* Prefill with silence to just below the fill level, so the first
         * event already fetches the first real frame. */
        UINT32 padding = 0, prefill;
        BYTE *buf = NULL;

        s->pb_client->GetCurrentPadding(&padding);
        prefill = s->pb_target_frames > s->frame_frames + padding ?
                  s->pb_target_frames - s->frame_frames - padding : 0;
        if (prefill && SUCCEEDED(s->pb_render->GetBuffer(prefill, &buf)))
            s->pb_render->ReleaseBuffer(prefill, AUDCLNT_BUFFERFLAGS_SILENT);

        hr = s->pb_client->Start();
        if (FAILED(hr)) {
            log_hr("playback Start", hr);
            return PJMEDIA_EAUD_SYSERR;
        }
    }
    if (s->cap_client) {
        hr = s->cap_client->Start();
        if (FAILED(hr)) {
            log_hr("capture Start", hr);
            if (s->pb_client)
                s->pb_client->Stop();
            return PJMEDIA_EAUD_SYSERR;
        }
    }

    s->running = PJ_TRUE;
    PJ_LOG(4, (THIS_FILE, "WASAPI stream started"));
    return PJ_SUCCESS;
}

static pj_status_t do_stop(struct wasapi_stream *s)
{
    HRESULT hr;

    if (!s->running)
        return PJ_SUCCESS;

    /* Reset() returns AUDCLNT_E_BUFFER_OPERATION_PENDING when a buffer from
     * GetBuffer() is still outstanding, which is the case when the stream is
     * stopped from inside a callback. That is expected, the buffer is left
     * with whatever it still holds, so only report the other failures.
     */
    if (s->pb_client) {
        s->pb_client->Stop();
        hr = s->pb_client->Reset();
        if (FAILED(hr) && hr != AUDCLNT_E_BUFFER_OPERATION_PENDING &&
            hr != AUDCLNT_E_DEVICE_INVALIDATED)
        {
            log_hr("playback Reset", hr);
        }
    }
    if (s->cap_client) {
        s->cap_client->Stop();
        hr = s->cap_client->Reset();
        if (FAILED(hr) && hr != AUDCLNT_E_BUFFER_OPERATION_PENDING &&
            hr != AUDCLNT_E_DEVICE_INVALIDATED)
        {
            log_hr("capture Reset", hr);
        }
    }
    s->running = PJ_FALSE;

    PJ_LOG(4, (THIS_FILE, "WASAPI stream stopped (underruns %u, capture "
               "glitches %u)", s->pb_underrun, s->cap_glitch));
    return PJ_SUCCESS;
}

/* Execute a command in the audio thread. The command and its volume
 * argument/result are passed explicitly rather than read from the stream, so
 * that a command issued from a callback, i.e: already running in the audio
 * thread, does not overwrite one that an application thread has queued and is
 * still waiting for. PJ_TRUE = quit the thread.
 */
static pj_bool_t exec_cmd(struct wasapi_stream *s, enum wasapi_cmd cmd,
                          float *volume, pj_status_t *p_status)
{
    pj_bool_t quit = PJ_FALSE;

    switch (cmd) {
    case WASAPI_CMD_START:
        *p_status = do_start(s);
        break;
    case WASAPI_CMD_STOP:
        *p_status = do_stop(s);
        break;
    case WASAPI_CMD_QUIT:
        *p_status = do_stop(s);
        quit = PJ_TRUE;
        break;
    case WASAPI_CMD_GET_VOLUME:
        *p_status = PJMEDIA_EAUD_INVCAP;
        if (s->pb_volume && volume &&
            SUCCEEDED(s->pb_volume->GetMasterVolume(volume)))
        {
            *p_status = PJ_SUCCESS;
        }
        break;
    case WASAPI_CMD_SET_VOLUME:
        *p_status = PJMEDIA_EAUD_INVCAP;
        if (s->pb_volume && volume &&
            SUCCEEDED(s->pb_volume->SetMasterVolume(*volume, NULL)))
        {
            *p_status = PJ_SUCCESS;
        }
        break;
    default:
        *p_status = PJ_EINVALIDOP;
        break;
    }
    return quit;
}

static int PJ_THREAD_FUNC wasapi_thread(void *arg)
{
    struct wasapi_stream *s = (struct wasapi_stream*)arg;
    HANDLE events[3];
    DWORD nevents = 0;
    HMODULE avrt = NULL;
    HANDLE mmcss;
    HRESULT co;
    pj_bool_t quit = PJ_FALSE;
    unsigned stalls = 0;

    s->thread_id = GetCurrentThreadId();
    co = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    mmcss = mmcss_enter(&avrt);

    s->cmd_status = FAILED(co) ? PJMEDIA_EAUD_SYSERR : thread_init(s);
    if (s->cmd_status != PJ_SUCCESS)
        quit = PJ_TRUE;
    SetEvent(s->done_event);

    events[nevents++] = s->cmd_event;
    if (s->cap_event)
        events[nevents++] = s->cap_event;
    if (s->pb_event)
        events[nevents++] = s->pb_event;

    while (!quit) {
        DWORD rc = WaitForMultipleObjects(nevents, events, FALSE,
                                          s->running ? 500 : INFINITE);

        if (rc == WAIT_OBJECT_0) {
            /* Nobody is waiting for a command once the channel is closed,
             * and running it now could undo what a callback did since.
             */
            if (!s->cmd_dead) {
                quit = exec_cmd(s, s->cmd, &s->cmd_volume, &s->cmd_status);
                if (!quit)
                    SetEvent(s->done_event);
            }
            s->cmd = WASAPI_CMD_NONE;
            continue;
        }

        if (!s->running || s->halted)
            continue;

        if (rc == WAIT_TIMEOUT) {
            /* The engine has stopped delivering events */
            if (++stalls == 1 || stalls % 100 == 0)
                PJ_LOG(3, (THIS_FILE, "WASAPI: no device events for 500 ms"));
            continue;
        }

        /* Whichever event it was, serve both directions */
        if (s->cap_capture)
            process_capture(s);
        if (s->pb_render && !s->halted)
            process_playback(s);
    }

    thread_release(s);
    mmcss_leave(avrt, mmcss);
    if (SUCCEEDED(co))
        CoUninitialize();

    /* QUIT waits for this, so signal only after releasing. After a failed
     * init this is a second signal that nobody collects any more
     * (create_stream then only waits for the thread to end). */
    SetEvent(s->done_event);

    return 0;
}

/* Send a command to the audio thread and wait for it. volume is the argument
 * of SET_VOLUME and the result of GET_VOLUME, and NULL for the others.
 */
static pj_status_t send_cmd(struct wasapi_stream *s, enum wasapi_cmd cmd,
                            float *volume)
{
    pj_status_t status = PJ_SUCCESS;

    if (!s->thread)
        return PJ_EINVALIDOP;

    /* Once the audio thread has missed a command it may still be about to
     * pick it up, so anything issued afterwards would race with it. Refuse
     * everything from here on, including from a callback: an inline command
     * would otherwise still run and then be undone by the command the
     * timeout left behind.
     */
    if (s->cmd_dead)
        return PJ_ETIMEDOUT;

    /* From the audio thread itself (a callback), execute it directly. It must
     * not touch the shared command slots here, an application thread may have
     * queued a command and still be waiting for it.
     */
    if (GetCurrentThreadId() == s->thread_id) {
        exec_cmd(s, cmd, volume, &status);
        return status;
    }

    EnterCriticalSection(&s->cmd_lock);

    if (s->cmd_dead) {
        LeaveCriticalSection(&s->cmd_lock);
        return PJ_ETIMEDOUT;
    }

    if (volume)
        s->cmd_volume = *volume;
    s->cmd = cmd;
    ResetEvent(s->done_event);
    SetEvent(s->cmd_event);
    if (WaitForSingleObject(s->done_event, WASAPI_CMD_TIMEOUT_MS) ==
        WAIT_OBJECT_0)
    {
        status = s->cmd_status;
        if (volume)
            *volume = s->cmd_volume;
    } else {
        PJ_LOG(2, (THIS_FILE, "WASAPI: audio thread did not answer command "
                   "%d", cmd));
        s->cmd_dead = PJ_TRUE;
        status = PJ_ETIMEDOUT;
    }
    LeaveCriticalSection(&s->cmd_lock);

    return status;
}


/* ---------------------------------------------------------------------------
 * Stream
 * ------------------------------------------------------------------------- */

static void stream_free(struct wasapi_stream *s)
{
    if (s->cmd_event)
        CloseHandle(s->cmd_event);
    if (s->done_event)
        CloseHandle(s->done_event);
    if (s->pb_event)
        CloseHandle(s->pb_event);
    if (s->cap_event)
        CloseHandle(s->cap_event);
    if (s->cmd_lock_init)
        DeleteCriticalSection(&s->cmd_lock);
    pj_pool_release(s->pool);
}

static pj_status_t wasapi_factory_create_stream(pjmedia_aud_dev_factory *f,
                                                const pjmedia_aud_param *param,
                                                pjmedia_aud_rec_cb rec_cb,
                                                pjmedia_aud_play_cb play_cb,
                                                void *user_data,
                                                pjmedia_aud_stream **p_strm)
{
    struct wasapi_factory *wf = (struct wasapi_factory*)f;
    struct wasapi_stream *s;
    pj_pool_t *pool;
    pj_status_t status;

    PJ_ASSERT_RETURN(param && p_strm, PJ_EINVAL);

    /* 16 bit PCM only */
    if (param->bits_per_sample != 16 ||
        (param->ext_fmt.id != 0 && param->ext_fmt.id != PJMEDIA_FORMAT_L16))
    {
        return PJMEDIA_EAUD_BADFORMAT;
    }
    if (param->channel_count < 1 || param->clock_rate == 0 ||
        param->samples_per_frame == 0 ||
        param->samples_per_frame % param->channel_count != 0)
    {
        return PJ_EINVAL;
    }
    /* Each active direction is served from the audio thread, so it needs its
     * callback.
     */
    if (((param->dir & PJMEDIA_DIR_CAPTURE) && !rec_cb) ||
        ((param->dir & PJMEDIA_DIR_PLAYBACK) && !play_cb))
    {
        return PJ_EINVAL;
    }
    if ((param->dir & PJMEDIA_DIR_CAPTURE) &&
        ((unsigned)param->rec_id >= wf->dev_count ||
         (!wf->devs[param->rec_id].cap_id &&
          !wf->devs[param->rec_id].is_default)))
    {
        return PJMEDIA_EAUD_INVDEV;
    }
    if ((param->dir & PJMEDIA_DIR_PLAYBACK) &&
        ((unsigned)param->play_id >= wf->dev_count ||
         (!wf->devs[param->play_id].pb_id &&
          !wf->devs[param->play_id].is_default)))
    {
        return PJMEDIA_EAUD_INVDEV;
    }

    pool = pj_pool_create(wf->pf, "wasapi%p", 1024, 1024, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    s = PJ_POOL_ZALLOC_T(pool, struct wasapi_stream);
    pj_memcpy(&s->param, param, sizeof(*param));
    s->base.op = &stream_op;
    s->pool = pool;
    s->user_data = user_data;
    s->rec_cb = rec_cb;
    s->play_cb = play_cb;

    s->frame_samples = param->samples_per_frame;
    s->frame_frames = param->samples_per_frame / param->channel_count;
    s->frame_bytes = param->samples_per_frame * 2;
    s->ptime_ms = s->frame_frames * 1000 / param->clock_rate;
    if (s->ptime_ms == 0)
        s->ptime_ms = 1;

    if (param->dir & PJMEDIA_DIR_CAPTURE) {
        const struct wasapi_dev *d = &wf->devs[param->rec_id];
        s->cap_default = d->is_default;
        if (!d->is_default)
            s->cap_id = wstr_dup(pool, d->cap_id);
        s->cap_buf = (pj_int16_t*)pj_pool_zalloc(pool, s->frame_bytes);
        s->cap_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
    if (param->dir & PJMEDIA_DIR_PLAYBACK) {
        const struct wasapi_dev *d = &wf->devs[param->play_id];
        s->pb_default = d->is_default;
        if (!d->is_default)
            s->pb_id = wstr_dup(pool, d->pb_id);
        s->pb_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    InitializeCriticalSection(&s->cmd_lock);
    s->cmd_lock_init = PJ_TRUE;
    s->cmd_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    s->done_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (!s->cmd_event || !s->done_event ||
        ((param->dir & PJMEDIA_DIR_CAPTURE) && !s->cap_event) ||
        ((param->dir & PJMEDIA_DIR_PLAYBACK) && !s->pb_event))
    {
        stream_free(s);
        return PJMEDIA_EAUD_SYSERR;
    }

    /* The thread opens the devices and reports the result */
    status = pj_thread_create(pool, "wasapi", &wasapi_thread, s, 0, 0,
                              &s->thread);
    if (status != PJ_SUCCESS) {
        stream_free(s);
        return status;
    }
    WaitForSingleObject(s->done_event, INFINITE);

    if (s->cmd_status != PJ_SUCCESS) {
        status = s->cmd_status;
        pj_thread_join(s->thread);
        pj_thread_destroy(s->thread);
        s->thread = NULL;
        stream_free(s);
        return status;
    }

    /* Apply the remaining settings */
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) {
        wasapi_stream_set_cap(&s->base,
                              PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
                              &param->output_vol);
    }

    *p_strm = &s->base;
    return PJ_SUCCESS;
}

static pj_status_t wasapi_stream_get_param(pjmedia_aud_stream *strm,
                                           pjmedia_aud_param *pi)
{
    struct wasapi_stream *s = (struct wasapi_stream*)strm;

    PJ_ASSERT_RETURN(s && pi, PJ_EINVAL);
    pj_memcpy(pi, &s->param, sizeof(*pi));

    /* Update the volume setting */
    if (wasapi_stream_get_cap(strm, PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
                              &pi->output_vol) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
    }

    return PJ_SUCCESS;
}

static pj_status_t wasapi_stream_get_cap(pjmedia_aud_stream *strm,
                                         pjmedia_aud_dev_cap cap,
                                         void *pval)
{
    struct wasapi_stream *s = (struct wasapi_stream*)strm;
    pj_status_t status;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap == PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY &&
        (s->param.dir & PJMEDIA_DIR_CAPTURE))
    {
        *(unsigned*)pval = s->param.input_latency_ms;
        return PJ_SUCCESS;
    }
    if (cap == PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY &&
        (s->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
        *(unsigned*)pval = s->param.output_latency_ms;
        return PJ_SUCCESS;
    }
    if (cap == PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING &&
        (s->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
        float vol = 0;

        status = send_cmd(s, WASAPI_CMD_GET_VOLUME, &vol);
        if (status == PJ_SUCCESS)
            *(unsigned*)pval = (unsigned)(vol * 100.0f + 0.5f);
        return status;
    }
    return PJMEDIA_EAUD_INVCAP;
}

static pj_status_t wasapi_stream_set_cap(pjmedia_aud_stream *strm,
                                         pjmedia_aud_dev_cap cap,
                                         const void *pval)
{
    struct wasapi_stream *s = (struct wasapi_stream*)strm;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap == PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING &&
        (s->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
        unsigned vol = *(const unsigned*)pval;
        float fvol;

        if (vol > 100)
            vol = 100;
        fvol = vol / 100.0f;
        return send_cmd(s, WASAPI_CMD_SET_VOLUME, &fvol);
    }
    return PJMEDIA_EAUD_INVCAP;
}

static pj_status_t wasapi_stream_start(pjmedia_aud_stream *strm)
{
    struct wasapi_stream *s = (struct wasapi_stream*)strm;
    PJ_ASSERT_RETURN(s, PJ_EINVAL);
    return send_cmd(s, WASAPI_CMD_START, NULL);
}

static pj_status_t wasapi_stream_stop(pjmedia_aud_stream *strm)
{
    struct wasapi_stream *s = (struct wasapi_stream*)strm;
    PJ_ASSERT_RETURN(s, PJ_EINVAL);
    return send_cmd(s, WASAPI_CMD_STOP, NULL);
}

static pj_status_t wasapi_stream_destroy(pjmedia_aud_stream *strm)
{
    struct wasapi_stream *s = (struct wasapi_stream*)strm;

    PJ_ASSERT_RETURN(s, PJ_EINVAL);

    /* From the audio thread itself, i.e: from a callback, send_cmd() would
     * run the QUIT inline and pj_thread_join() would return immediately, so
     * the stream would be freed while this thread is still running in it.
     * The application has to destroy the stream from another thread.
     */
    if (s->thread && GetCurrentThreadId() == s->thread_id) {
        PJ_LOG(2, (THIS_FILE, "WASAPI: the stream cannot be destroyed from "
                   "its own audio thread"));
        return PJ_EINVALIDOP;
    }

    if (s->thread) {
        if (send_cmd(s, WASAPI_CMD_QUIT, NULL) != PJ_SUCCESS) {
            /* The audio thread is not answering. Joining it would block for
             * good and freeing the stream would pull the pool from under it,
             * so leave both alone.
             */
            PJ_LOG(1, (THIS_FILE, "WASAPI: audio thread is not responding, "
                       "the stream is left allocated"));
            return PJ_ETIMEDOUT;
        }
        pj_thread_join(s->thread);
        pj_thread_destroy(s->thread);
        s->thread = NULL;
    }
    stream_free(s);

    return PJ_SUCCESS;
}

#endif  /* PJMEDIA_AUDIO_DEV_HAS_WASAPI, Windows Desktop */
