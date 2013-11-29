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

///////////////////////////////////////////////////////////////////////////////
/* Audio Media operations. */
void ConfPortInfo::fromPj(const pjsua_conf_port_info &port_info)
{
    portId = port_info.slot_id;
    name = pj2Str(port_info.name);

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

    listeners.clear();
    for (unsigned i=0; i<port_info.listener_cnt; ++i) {
	listeners.push_back(port_info.listeners[i]);
    }
}
///////////////////////////////////////////////////////////////////////////////
Media::Media()
{

}

Media::~Media()
{

}

///////////////////////////////////////////////////////////////////////////////
AudioMedia::AudioMedia() 
: id(PJSUA_INVALID_ID), mediaPool(NULL)
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

    Endpoint::instance().addMedia(*this);
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

    Endpoint::instance().removeMedia(*this);
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

