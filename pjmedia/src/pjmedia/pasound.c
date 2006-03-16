/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/sound.h>
#include <pjmedia/errno.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/string.h>
#include <portaudio.h>

#if defined(PJMEDIA_HAS_PORTAUDIO_SOUND) && PJMEDIA_HAS_PORTAUDIO_SOUND!=0

#define THIS_FILE	"pasound.c"

static struct snd_mgr
{
    pj_pool_factory *factory;
} snd_mgr;

struct pjmedia_snd_stream
{
    pj_pool_t	    *pool;
    pj_str_t	     name;
    PaStream	    *stream;
    int		     dev_index;
    int		     bytes_per_sample;
    pj_uint32_t	     samples_per_sec;
    int		     channel_count;
    pj_uint32_t	     timestamp;
    pj_uint32_t	     underflow;
    pj_uint32_t	     overflow;
    void	    *user_data;
    pjmedia_snd_rec_cb    rec_cb;
    pjmedia_snd_play_cb   play_cb;
    pj_bool_t	     quit_flag;
    pj_bool_t	     thread_has_exited;
    pj_bool_t	     thread_initialized;
    pj_thread_desc   thread_desc;
    pj_thread_t	    *thread;
};


static int PaRecorderCallback(const void *input, 
			      void *output,
			      unsigned long frameCount,
			      const PaStreamCallbackTimeInfo* timeInfo,
			      PaStreamCallbackFlags statusFlags,
			      void *userData )
{
    pjmedia_snd_stream *stream = userData;
    pj_status_t status;

    PJ_UNUSED_ARG(output);
    PJ_UNUSED_ARG(timeInfo);

    if (stream->quit_flag)
	goto on_break;

    if (stream->thread_initialized == 0) {
	status = pj_thread_register("pa_rec", stream->thread_desc, &stream->thread);
	stream->thread_initialized = 1;
	PJ_LOG(5,(THIS_FILE, "Recorder thread started"));
    }

    if (statusFlags & paInputUnderflow)
	++stream->underflow;
    if (statusFlags & paInputOverflow)
	++stream->overflow;

    stream->timestamp += frameCount;

    status = (*stream->rec_cb)(stream->user_data, stream->timestamp, 
			       input, frameCount * stream->bytes_per_sample);
    
    if (status==0) 
	return paContinue;

on_break:
    stream->thread_has_exited = 1;
    return paAbort;
}

static int PaPlayerCallback( const void *input, 
			     void *output,
			     unsigned long frameCount,
			     const PaStreamCallbackTimeInfo* timeInfo,
			     PaStreamCallbackFlags statusFlags,
			     void *userData )
{
    pjmedia_snd_stream *stream = userData;
    pj_status_t status;
    unsigned size = frameCount * stream->bytes_per_sample;

    PJ_UNUSED_ARG(input);
    PJ_UNUSED_ARG(timeInfo);

    if (stream->quit_flag)
	goto on_break;

    if (stream->thread_initialized == 0) {
	status = pj_thread_register("pa_rec", stream->thread_desc, &stream->thread);
	stream->thread_initialized = 1;
	PJ_LOG(5,(THIS_FILE, "Player thread started"));
    }

    if (statusFlags & paInputUnderflow)
	++stream->underflow;
    if (statusFlags & paInputOverflow)
	++stream->overflow;

    stream->timestamp += frameCount;

    status = (*stream->play_cb)(stream->user_data, stream->timestamp, 
			        output, size);
    
    if (status==0) 
	return paContinue;

on_break:
    stream->thread_has_exited = 1;
    return paAbort;
}


/*
 * Init sound library.
 */
PJ_DEF(pj_status_t) pjmedia_snd_init(pj_pool_factory *factory)
{
    int err;

    snd_mgr.factory = factory;
    err = Pa_Initialize();

    PJ_LOG(4,(THIS_FILE, "PortAudio sound library initialized, status=%d", err));
    PJ_LOG(4,(THIS_FILE, "PortAudio host api count=%d",
			 Pa_GetHostApiCount()));
    PJ_LOG(4,(THIS_FILE, "Sound device count=%d",
			 pjmedia_snd_get_dev_count()));

    return err ? PJMEDIA_ERRNO_FROM_PORTAUDIO(err) : PJ_SUCCESS;
}


/*
 * Get device count.
 */
PJ_DEF(int) pjmedia_snd_get_dev_count(void)
{
    return Pa_GetDeviceCount();
}


/*
 * Get device info.
 */
PJ_DEF(const pjmedia_snd_dev_info*) pjmedia_snd_get_dev_info(unsigned index)
{
    static pjmedia_snd_dev_info info;
    const PaDeviceInfo *pa_info;

    pa_info = Pa_GetDeviceInfo(index);
    if (!pa_info)
	return NULL;

    pj_memset(&info, 0, sizeof(info));
    strncpy(info.name, pa_info->name, sizeof(info.name));
    info.name[sizeof(info.name)-1] = '\0';
    info.input_count = pa_info->maxInputChannels;
    info.output_count = pa_info->maxOutputChannels;
    info.default_samples_per_sec = (unsigned)pa_info->defaultSampleRate;

    return &info;
}


/*
 * Open stream.
 */
PJ_DEF(pj_status_t) pjmedia_snd_open_recorder( int index,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned bits_per_sample,
					  pjmedia_snd_rec_cb rec_cb,
					  void *user_data,
					  pjmedia_snd_stream **p_snd_strm)
{
    pj_pool_t *pool;
    pjmedia_snd_stream *stream;
    PaStreamParameters inputParam;
    int sampleFormat;
    const PaDeviceInfo *paDevInfo = NULL;
    PaError err;

    if (index == -1) {
	int count = Pa_GetDeviceCount();
	for (index=0; index<count; ++index) {
	    paDevInfo = Pa_GetDeviceInfo(index);
	    if (paDevInfo->maxInputChannels > 0)
		break;
	}
	if (index == count) {
	    /* No such device. */
	    return PJ_ENOTFOUND;
	}
    } else {
	paDevInfo = Pa_GetDeviceInfo(index);
	if (!paDevInfo) {
	    /* Assumed it is "No such device" error. */
	    return PJ_ENOTFOUND;
	}
    }

    if (bits_per_sample == 8)
	sampleFormat = paUInt8;
    else if (bits_per_sample == 16)
	sampleFormat = paInt16;
    else if (bits_per_sample == 32)
	sampleFormat = paInt32;
    else
	return PJ_ENOTSUP;
    
    pool = pj_pool_create( snd_mgr.factory, "sndstream", 1024, 1024, NULL);
    if (!pool)
	return PJ_ENOMEM;

    stream = pj_pool_zalloc(pool, sizeof(*stream));
    stream->pool = pool;
    stream->name = pj_str("snd-rec");
    stream->user_data = user_data;
    stream->dev_index = index;
    stream->samples_per_sec = samples_per_frame;
    stream->bytes_per_sample = bits_per_sample / 8;
    stream->channel_count = channel_count;
    stream->rec_cb = rec_cb;

    pj_memset(&inputParam, 0, sizeof(inputParam));
    inputParam.device = index;
    inputParam.channelCount = channel_count;
    inputParam.hostApiSpecificStreamInfo = NULL;
    inputParam.sampleFormat = sampleFormat;
    inputParam.suggestedLatency = paDevInfo->defaultLowInputLatency;

    err = Pa_OpenStream( &stream->stream, &inputParam, NULL,
			 clock_rate, samples_per_frame, 
			 0, &PaRecorderCallback, stream );
    if (err != paNoError) {
	pj_pool_release(pool);
	return PJMEDIA_ERRNO_FROM_PORTAUDIO(err);
    }

    PJ_LOG(5,(THIS_FILE, "%s opening device %s for recording, sample rate=%d, "
			 "channel count=%d, "
			 "%d bits per sample, %d samples per buffer",
			 (err==0 ? "Success" : "Error"),
			 paDevInfo->name, clock_rate, channel_count,
			 bits_per_sample, samples_per_frame));

    *p_snd_strm = stream;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_snd_open_player( int index,
					unsigned clock_rate,
					unsigned channel_count,
					unsigned samples_per_frame,
					unsigned bits_per_sample,
					pjmedia_snd_play_cb play_cb,
					void *user_data,
					pjmedia_snd_stream **p_snd_strm)
{
    pj_pool_t *pool;
    pjmedia_snd_stream *stream;
    PaStreamParameters outputParam;
    int sampleFormat;
    const PaDeviceInfo *paDevInfo = NULL;
    PaError err;

    if (index == -1) {
	int count = Pa_GetDeviceCount();
	for (index=0; index<count; ++index) {
	    paDevInfo = Pa_GetDeviceInfo(index);
	    if (paDevInfo->maxOutputChannels > 0)
		break;
	}
	if (index == count) {
	    /* No such device. */
	    return PJ_ENOTFOUND;
	}
    } else {
	paDevInfo = Pa_GetDeviceInfo(index);
	if (!paDevInfo) {
	    /* Assumed it is "No such device" error. */
	    return PJ_ENOTFOUND;
	}
    }

    if (bits_per_sample == 8)
	sampleFormat = paUInt8;
    else if (bits_per_sample == 16)
	sampleFormat = paInt16;
    else if (bits_per_sample == 32)
	sampleFormat = paInt32;
    else
	return PJ_ENOTSUP;
    
    pool = pj_pool_create( snd_mgr.factory, "sndstream", 1024, 1024, NULL);
    if (!pool)
	return PJ_ENOMEM;

    stream = pj_pool_calloc(pool, 1, sizeof(*stream));
    stream->pool = pool;
    stream->name = pj_str("player");
    stream->user_data = user_data;
    stream->samples_per_sec = samples_per_frame;
    stream->bytes_per_sample = bits_per_sample / 8;
    stream->channel_count = channel_count;
    stream->dev_index = index;
    stream->play_cb = play_cb;

    pj_memset(&outputParam, 0, sizeof(outputParam));
    outputParam.device = index;
    outputParam.channelCount = channel_count;
    outputParam.hostApiSpecificStreamInfo = NULL;
    outputParam.sampleFormat = sampleFormat;
    outputParam.suggestedLatency = paDevInfo->defaultLowInputLatency;

    err = Pa_OpenStream( &stream->stream, NULL, &outputParam,
			 clock_rate,  samples_per_frame, 
			 0, &PaPlayerCallback, stream );
    if (err != paNoError) {
	pj_pool_release(pool);
	return PJMEDIA_ERRNO_FROM_PORTAUDIO(err);
    }

    PJ_LOG(5,(THIS_FILE, "%s opening device %s for playing, sample rate=%d, "
			 "channel count=%d, "
			 "%d bits per sample, %d samples per buffer",
			 (err==0 ? "Success" : "Error"),
			 paDevInfo->name, clock_rate, channel_count,
		 	 bits_per_sample, samples_per_frame));

    *p_snd_strm = stream;

    return PJ_SUCCESS;
}


/*
 * Start stream.
 */
PJ_DEF(pj_status_t) pjmedia_snd_stream_start(pjmedia_snd_stream *stream)
{
    pj_status_t err;

    PJ_LOG(5,(THIS_FILE, "Starting %s stream..", stream->name.ptr));

    err = Pa_StartStream(stream->stream);

    PJ_LOG(5,(THIS_FILE, "Done, status=%d", err));

    return err ? PJMEDIA_ERRNO_FROM_PORTAUDIO(err) : PJ_SUCCESS;
}

/*
 * Stop stream.
 */
PJ_DEF(pj_status_t) pjmedia_snd_stream_stop(pjmedia_snd_stream *stream)
{
    int i, err;

    stream->quit_flag = 1;
    for (i=0; !stream->thread_has_exited && i<100; ++i)
	pj_thread_sleep(10);

    pj_thread_sleep(1);

    PJ_LOG(5,(THIS_FILE, "Stopping stream.."));

    err = Pa_StopStream(stream->stream);

    PJ_LOG(5,(THIS_FILE, "Done, status=%d", err));

    return err ? PJMEDIA_ERRNO_FROM_PORTAUDIO(err) : PJ_SUCCESS;
}

/*
 * Destroy stream.
 */
PJ_DEF(pj_status_t) pjmedia_snd_stream_close(pjmedia_snd_stream *stream)
{
    int i, err;
    const PaDeviceInfo *paDevInfo;

    stream->quit_flag = 1;
    for (i=0; !stream->thread_has_exited && i<100; ++i)
	pj_thread_sleep(10);

    pj_thread_sleep(1);

    paDevInfo = Pa_GetDeviceInfo(stream->dev_index);

    PJ_LOG(5,(THIS_FILE, "Closing %s: %lu underflow, %lu overflow",
			 paDevInfo->name,
			 stream->underflow, stream->overflow));

    err = Pa_CloseStream(stream->stream);
    pj_pool_release(stream->pool);

    return err ? PJMEDIA_ERRNO_FROM_PORTAUDIO(err) : PJ_SUCCESS;
}

/*
 * Deinitialize sound library.
 */
PJ_DEF(pj_status_t) pjmedia_snd_deinit(void)
{
    int err;

    PJ_LOG(4,(THIS_FILE, "PortAudio sound library shutting down.."));

    err = Pa_Terminate();

    return err ? PJMEDIA_ERRNO_FROM_PORTAUDIO(err) : PJ_SUCCESS;
}


#endif	/* PJMEDIA_HAS_PORTAUDIO_SOUND */
