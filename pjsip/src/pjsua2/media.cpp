/* $Id$ */
/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
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
#include <pj/ctype.h>
#include <pjsua2/media.hpp>
#include <pjsua2/types.hpp>
#include <pjsua2/endpoint.hpp>
#include "util.hpp"

using namespace pj;
using namespace std;

#define THIS_FILE		"media.cpp"
#define MAX_FILE_NAMES 		64
#define MAX_DEV_COUNT		64

///////////////////////////////////////////////////////////////////////////////
void MediaFormatAudio::fromPj(const pjmedia_format &format)
{
    if ((format.type != PJMEDIA_TYPE_AUDIO) &&
	(format.detail_type != PJMEDIA_FORMAT_DETAIL_AUDIO))
    {
	type = PJMEDIA_TYPE_UNKNOWN;
	return;
    }

    id = format.id;
    type = format.type;

    /* Detail. */
    clockRate = format.det.aud.clock_rate;
    channelCount = format.det.aud.channel_count;
    frameTimeUsec = format.det.aud.frame_time_usec;
    bitsPerSample = format.det.aud.bits_per_sample;
    avgBps = format.det.aud.avg_bps;
    maxBps = format.det.aud.max_bps;
}

pjmedia_format MediaFormatAudio::toPj() const
{
    pjmedia_format pj_format;

    pj_format.id = id;
    pj_format.type = type;

    pj_format.detail_type = PJMEDIA_FORMAT_DETAIL_AUDIO;
    pj_format.det.aud.clock_rate = clockRate;
    pj_format.det.aud.channel_count = channelCount;
    pj_format.det.aud.frame_time_usec = frameTimeUsec;
    pj_format.det.aud.bits_per_sample = bitsPerSample;
    pj_format.det.aud.avg_bps = avgBps;
    pj_format.det.aud.max_bps = maxBps;

    return pj_format;
}

///////////////////////////////////////////////////////////////////////////////
/* Audio Media operations. */
void ConfPortInfo::fromPj(const pjsua_conf_port_info &port_info)
{
    portId = port_info.slot_id;
    name = pj2Str(port_info.name);
    format.fromPj(port_info.format);
    txLevelAdj = port_info.tx_level_adj;
    rxLevelAdj = port_info.rx_level_adj;

    /*
    format.id = PJMEDIA_FORMAT_PCM;
    format.type = PJMEDIA_TYPE_AUDIO;
    format.clockRate = port_info.clock_rate;
    format.channelCount = port_info.channel_count;
    format.bitsPerSample = port_info.bits_per_sample;
    format.frameTimeUsec = (port_info.samples_per_frame *
			   1000000) /
			   (port_info.clock_rate *
			   port_info.channel_count);

    format.avgBps = format.maxBps = port_info.clock_rate *
				    port_info.channel_count *
				    port_info.bits_per_sample;
    */
    listeners.clear();
    for (unsigned i=0; i<port_info.listener_cnt; ++i) {
	listeners.push_back(port_info.listeners[i]);
    }
}
///////////////////////////////////////////////////////////////////////////////
Media::Media(pjmedia_type med_type)
: type(med_type)
{

}

Media::~Media()
{

}

pjmedia_type Media::getType() const
{
    return type;
}

///////////////////////////////////////////////////////////////////////////////
AudioMedia::AudioMedia() 
: Media(PJMEDIA_TYPE_AUDIO), id(PJSUA_INVALID_ID), mediaPool(NULL)
{

}

void AudioMedia::registerMediaPort(MediaPort port) throw(Error)
{
    /* Check if media already added to Conf bridge. */
    pj_assert(!Endpoint::instance().mediaExists(*this));

    if (port != NULL) {
	pj_assert(id == PJSUA_INVALID_ID);

	pj_caching_pool_init(&mediaCachingPool, NULL, 0);

	mediaPool = pj_pool_create(&mediaCachingPool.factory,
				   "media",
				   512,
				   512,
				   NULL);

	if (!mediaPool) {
	    pj_caching_pool_destroy(&mediaCachingPool);
	    PJSUA2_RAISE_ERROR(PJ_ENOMEM);
	}

	PJSUA2_CHECK_EXPR( pjsua_conf_add_port(mediaPool,
					       (pjmedia_port *)port,
					       &id) );
    }

    Endpoint::instance().mediaAdd(*this);
}

void AudioMedia::unregisterMediaPort()
{
    if (id != PJSUA_INVALID_ID)
	pjsua_conf_remove_port(id);

    if (mediaPool) {
	pj_pool_release(mediaPool);
	mediaPool = NULL;
	pj_caching_pool_destroy(&mediaCachingPool);
    }

    id = PJSUA_INVALID_ID;

    Endpoint::instance().mediaRemove(*this);
}

AudioMedia::~AudioMedia() 
{
    unregisterMediaPort();
}

ConfPortInfo AudioMedia::getPortInfo() const throw(Error)
{
    return AudioMedia::getPortInfoFromId(id);
}

int AudioMedia::getPortId() const
{
    return id;
}

ConfPortInfo AudioMedia::getPortInfoFromId(int port_id) throw(Error)
{
    pjsua_conf_port_info pj_info;
    ConfPortInfo pi;

    PJSUA2_CHECK_EXPR( pjsua_conf_get_port_info(port_id, &pj_info) );
    pi.fromPj(pj_info);
    return pi;
}

void AudioMedia::startTransmit(const AudioMedia &sink) const throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_conf_connect(id, sink.id) );
}

void AudioMedia::stopTransmit(const AudioMedia &sink) const throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_conf_disconnect(id, sink.id) );
}

void AudioMedia::adjustRxLevel(float level) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_conf_adjust_rx_level(id, level) );
}

void AudioMedia::adjustTxLevel(float level) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_conf_adjust_tx_level(id, level) );
}

unsigned AudioMedia::getRxLevel() const throw(Error)
{
    return getSignalLevel(true);
}

unsigned AudioMedia::getTxLevel() const throw(Error)
{
    return getSignalLevel(false);
}

unsigned AudioMedia::getSignalLevel(bool is_rx) const throw(Error)
{    
    unsigned rx_level;
    unsigned tx_level;
    
    PJSUA2_CHECK_EXPR( pjsua_conf_get_signal_level(id, &tx_level, &rx_level) );
    return is_rx?rx_level:tx_level;
}

///////////////////////////////////////////////////////////////////////////////

AudioMediaPlayer::AudioMediaPlayer()
: playerId(PJSUA_INVALID_ID)
{

}

AudioMediaPlayer::~AudioMediaPlayer()
{
    if (playerId != PJSUA_INVALID_ID)
	pjsua_player_destroy(playerId);
}

void AudioMediaPlayer::createPlayer(const string &file_name,
				    unsigned options)
				    throw(Error)
{
    pj_str_t pj_name = str2Pj(file_name);

    PJSUA2_CHECK_EXPR( pjsua_player_create(&pj_name,
					   options, 
					   &playerId) );

    /* Get media port id. */
    id = pjsua_player_get_conf_port(playerId);

    registerMediaPort(NULL);
}

void AudioMediaPlayer::createPlaylist(const StringVector &file_names,
				      const string &label,
				      unsigned options)
				      throw(Error)
{
    pj_str_t pj_files[MAX_FILE_NAMES];
    unsigned i, count = 0;
    pj_str_t pj_lbl = str2Pj(label);

    count = PJ_ARRAY_SIZE(pj_files);

    for(i=0; i<file_names.size() && i<count;++i)
    {
	const string &file_name = file_names[i];
	
	pj_files[i] = str2Pj(file_name);
    }

    PJSUA2_CHECK_EXPR( pjsua_playlist_create(pj_files,
					     i,
					     &pj_lbl,
					     options, 
					     &playerId) );

    /* Get media port id. */
    id = pjsua_player_get_conf_port(playerId);

    registerMediaPort(NULL);
}

void AudioMediaPlayer::setPos(pj_uint32_t samples) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_player_set_pos(playerId, samples) );
}

///////////////////////////////////////////////////////////////////////////////
AudioMediaRecorder::AudioMediaRecorder()
: recorderId(PJSUA_INVALID_ID)
{

}

AudioMediaRecorder::~AudioMediaRecorder()
{
    if (recorderId != PJSUA_INVALID_ID)
	pjsua_recorder_destroy(recorderId);
}

void AudioMediaRecorder::createRecorder(const string &file_name,
				        unsigned enc_type,
				        pj_ssize_t max_size,
				        unsigned options)
				        throw(Error)
{
    PJ_UNUSED_ARG(max_size);

    pj_str_t pj_name = str2Pj(file_name);

    PJSUA2_CHECK_EXPR( pjsua_recorder_create(&pj_name,
					     enc_type,
					     NULL,
					     -1,
					     options,
					     &recorderId) );

    /* Get media port id. */
    id = pjsua_recorder_get_conf_port(recorderId);

    registerMediaPort(NULL);
}
///////////////////////////////////////////////////////////////////////////////
void AudioDevInfo::fromPj(const pjmedia_aud_dev_info &dev_info)
{
    name = dev_info.name;
    inputCount = dev_info.input_count;
    outputCount = dev_info.output_count;
    defaultSamplesPerSec = dev_info.default_samples_per_sec;
    driver = dev_info.driver;
    caps = dev_info.caps;
    routes = dev_info.routes;

    for (unsigned i=0; i<dev_info.ext_fmt_cnt;++i) {
	MediaFormatAudio *format = new MediaFormatAudio;

	format->fromPj(dev_info.ext_fmt[i]);
	if (format->type == PJMEDIA_TYPE_AUDIO)
	    extFmt.push_back(format);
    }
}

AudioDevInfo::~AudioDevInfo()
{
    for(unsigned i=0;i<extFmt.size();++i) {
	delete extFmt[i];
    }
    extFmt.clear();
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Simple AudioMedia class for sound device.
 */
class DevAudioMedia : public AudioMedia
{
public:
    DevAudioMedia();
};

DevAudioMedia::DevAudioMedia()
{
    this->id = 0;
    registerMediaPort(NULL);
}

///////////////////////////////////////////////////////////////////////////////
/* Audio device operations. */

AudDevManager::AudDevManager()
: devMedia(NULL)
{
}

AudDevManager::~AudDevManager()
{
    delete devMedia;
    clearAudioDevList();
}

int AudDevManager::getCaptureDev() const throw(Error)
{
    return getActiveDev(true);
}

AudioMedia &AudDevManager::getCaptureDevMedia() throw(Error)
{
    if (!devMedia)
	devMedia = new DevAudioMedia;
    return *devMedia;
}

int AudDevManager::getPlaybackDev() const throw(Error)
{
    return getActiveDev(false);
}

AudioMedia &AudDevManager::getPlaybackDevMedia() throw(Error)
{
    if (!devMedia)
    	devMedia = new DevAudioMedia;
    return *devMedia;
}

void AudDevManager::setCaptureDev(int capture_dev) const throw(Error)
{
    int playback_dev = getPlaybackDev();

    PJSUA2_CHECK_EXPR( pjsua_set_snd_dev(capture_dev, playback_dev) );
}

void AudDevManager::setPlaybackDev(int playback_dev) const throw(Error)
{
    int capture_dev = getCaptureDev();

    PJSUA2_CHECK_EXPR( pjsua_set_snd_dev(capture_dev, playback_dev) );
}

const AudioDevInfoVector &AudDevManager::enumDev() throw(Error)
{
    pjmedia_aud_dev_info pj_info[MAX_DEV_COUNT];
    unsigned count;

    PJSUA2_CHECK_EXPR( pjsua_enum_aud_devs(pj_info, &count) );

    clearAudioDevList();

    pj_enter_critical_section();
    for (unsigned i = 0; (i<count && i<MAX_DEV_COUNT) ;++i) {
	AudioDevInfo *dev_info = new AudioDevInfo;
	dev_info->fromPj(pj_info[i]);
	audioDevList.push_back(dev_info);
    }
    pj_leave_critical_section();
    return audioDevList;
}

void AudDevManager::setNullDev() throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_set_null_snd_dev() );
}

MediaPort *AudDevManager::setNoDev()
{
    return (MediaPort*)pjsua_set_no_snd_dev();
}

void AudDevManager::setEcOptions(unsigned tail_msec,
				 unsigned options) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_set_ec(tail_msec, options) );
}

unsigned AudDevManager::getEcTail() const throw(Error)
{
    unsigned tail_msec = 0;

    PJSUA2_CHECK_EXPR( pjsua_get_ec_tail(&tail_msec) );

    return tail_msec;
}

bool AudDevManager::sndIsActive() const
{
    return PJ2BOOL(pjsua_snd_is_active());
}

void AudDevManager::refreshDevs() throw(Error)
{
    PJSUA2_CHECK_EXPR( pjmedia_aud_dev_refresh() );
}

unsigned AudDevManager::getDevCount() const
{
    return pjmedia_aud_dev_count();
}

AudioDevInfo
AudDevManager::getDevInfo(int id) const throw(Error)
{
    AudioDevInfo dev_info;
    pjmedia_aud_dev_info pj_info;

    PJSUA2_CHECK_EXPR( pjmedia_aud_dev_get_info(id, &pj_info) );

    dev_info.fromPj(pj_info);
    return dev_info;
}

int AudDevManager::lookupDev(const string &drv_name,
			     const string &dev_name) const throw(Error)
{
    pjmedia_aud_dev_index pj_idx = 0;

    PJSUA2_CHECK_EXPR( pjmedia_aud_dev_lookup(drv_name.c_str(),
					      dev_name.c_str(),
					      &pj_idx) );

    return pj_idx;
}


string AudDevManager::capName(pjmedia_aud_dev_cap cap) const
{
    return pjmedia_aud_dev_cap_name(cap, NULL);
}

void
AudDevManager::setExtFormat(const MediaFormatAudio &format,
			    bool keep) throw(Error)
{
    pjmedia_format pj_format = format.toPj();

    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_EXT_FORMAT,
					     &pj_format,
					     keep) );
}

MediaFormatAudio AudDevManager::getExtFormat() const throw(Error)
{
    pjmedia_format pj_format;
    MediaFormatAudio format;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_EXT_FORMAT,
					     &pj_format) );

    format.fromPj(pj_format);

    return format;
}

void AudDevManager::setInputLatency(unsigned latency_msec,
				    bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY,
					     &latency_msec,
					     keep) );
}

unsigned AudDevManager::getInputLatency() const throw(Error)
{
    unsigned latency_msec = 0;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY,
					     &latency_msec) );

    return latency_msec;
}

void
AudDevManager::setOutputLatency(unsigned latency_msec,
				bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY,
					     &latency_msec,
					     keep) );
}

unsigned AudDevManager::getOutputLatency() const throw(Error)
{
    unsigned latency_msec = 0;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY,
					     &latency_msec) );

    return latency_msec;
}

void AudDevManager::setInputVolume(unsigned volume, bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR(
	    pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING,
				  &volume,
				  keep) );
}

unsigned AudDevManager::getInputVolume() const throw(Error)
{
    unsigned volume = 0;

    PJSUA2_CHECK_EXPR(
	    pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING,
				  &volume) );

    return volume;
}

void AudDevManager::setOutputVolume(unsigned volume, bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR(
	    pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
				  &volume,
				  keep) );
}

unsigned AudDevManager::getOutputVolume() const throw(Error)
{
    unsigned volume = 0;

    PJSUA2_CHECK_EXPR(
	    pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
				  &volume) );

    return volume;
}

unsigned AudDevManager::getInputSignal() const throw(Error)
{
    unsigned signal = 0;

    PJSUA2_CHECK_EXPR(
	    pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_INPUT_SIGNAL_METER,
				  &signal) );

    return signal;
}

unsigned AudDevManager::getOutputSignal() const throw(Error)
{
    unsigned signal = 0;

    PJSUA2_CHECK_EXPR(
	    pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_SIGNAL_METER,
				  &signal) );

    return signal;
}

void
AudDevManager::setInputRoute(pjmedia_aud_dev_route route,
			     bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE,
					     &route,
					     keep) );
}

pjmedia_aud_dev_route AudDevManager::getInputRoute() const throw(Error)
{
    pjmedia_aud_dev_route route = PJMEDIA_AUD_DEV_ROUTE_DEFAULT;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE,
					     &route) );

    return route;
}

void
AudDevManager::setOutputRoute(pjmedia_aud_dev_route route,
			      bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE,
					     &route,
					     keep) );
}

pjmedia_aud_dev_route AudDevManager::getOutputRoute() const throw(Error)
{
    pjmedia_aud_dev_route route = PJMEDIA_AUD_DEV_ROUTE_DEFAULT;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE,
					     &route) );

    return route;
}

void AudDevManager::setVad(bool enable, bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_VAD,
					     &enable,
					     keep) );
}

bool AudDevManager::getVad() const throw(Error)
{
    bool enable = false;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_VAD,
					     &enable) );

    return enable;
}

void AudDevManager::setCng(bool enable, bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_CNG,
					     &enable,
					     keep) );
}

bool AudDevManager::getCng() const throw(Error)
{
    bool enable = false;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_CNG,
					     &enable) );

    return enable;
}

void AudDevManager::setPlc(bool enable, bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_PLC,
					     &enable,
					     keep) );
}

bool AudDevManager::getPlc() const throw(Error)
{
    bool enable = false;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_PLC,
					     &enable) );

    return enable;
}

void AudDevManager::clearAudioDevList()
{
    pj_enter_critical_section();
    for(unsigned i=0;i<audioDevList.size();++i) {
	delete audioDevList[i];
    }
    audioDevList.clear();
    pj_leave_critical_section();
}

int AudDevManager::getActiveDev(bool is_capture) const throw(Error)
{
    int capture_dev = 0, playback_dev = 0;
    PJSUA2_CHECK_EXPR( pjsua_get_snd_dev(&capture_dev, &playback_dev) );

    return is_capture?capture_dev:playback_dev;
}

///////////////////////////////////////////////////////////////////////////////
void CodecInfo::fromPj(const pjsua_codec_info &codec_info)
{
    codecId = pj2Str(codec_info.codec_id);
    priority = codec_info.priority;
    desc = pj2Str(codec_info.desc);
}
