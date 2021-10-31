#include <pjsua2/audioPlayer.hpp>
#include <pjsua2/types.hpp>
#include "util.hpp"

using namespace pj;

#include <pjsua-lib/pjsua_internal.h>

#define THIS_FILE		"audioPlayer.cpp"
#define MAX_FILE_NAMES 		64

AudioPlayer::AudioPlayer(const std::string& fileName, ConferenceBridge* conferenceBridge, bool loops, bool useAsyncCallback) : conferenceBridge(conferenceBridge)
{
	if (conferenceBridge == NULL || fileName.empty())
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer..ctor", "Invalid parameters");
		return;
	}

	this->conferenceBridge = conferenceBridge;
	this->playerID = PJSUA_INVALID_ID;
	PlayFile(fileName, loops, useAsyncCallback);
}

// ToDo: AudioPlayer can play a list of music BI #5088
//AudioPlayer::AudioPlayer(const std::vector<std::string> fileNames, ConferenceBridge* conferenceBridge, bool loops) : conferenceBridge(conferenceBridge)
//{
// 	if (conferenceBridge == NULL || fileNames.empty())
//{
//	PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer..ctor", "Invalid parameters");
//	return;
//}

//	this->conferenceBridge = conferenceBridge;
//	this->playerID = PJSUA_INVALID_ID;
//	PlayList(fileNames, loops);
//}

AudioPlayer::~AudioPlayer()
{
	if (playerID != PJSUA_INVALID_ID)
	{
		pj_status_t status = pjsua_player_get_port(playerID, &port);
		if (status == PJ_SUCCESS)
		{
			pj_status_t status = pjmedia_wav_player_set_eof_cb2(port, this, NULL);
			if (status != PJ_SUCCESS)
			{
				PJSUA2_RAISE_ERROR3(PJ_EINVAL, "~AudioPlayer", "Error when cleaning up callback");
			}
		}
		UnregisterMediaPort();
		pjsua_player_destroy(playerID);
	}
}

void AudioPlayer::PlayFile(const std::string& fileName, bool loops, bool useAsyncCallback)
{
	if (fileName.empty())
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer::PlayFile", "Invalid parameter: fileName");
		return;
	}
	if (playerID != PJSUA_INVALID_ID)
	{
		PJSUA2_RAISE_ERROR(PJ_EEXISTS);
	}

	unsigned int frameReadTime = 0; //zero triggers usage of default value
	unsigned int options = loops ? 0 : PJMEDIA_FILE_NO_LOOP;
	long bufferSize = 0; //zero triggers usage of default value
	pj_str_t pj_name = str2Pj(fileName);

	PJSUA2_CHECK_EXPR(CreatePlayer(&pj_name, options));

	pj_status_t status = pjsua_player_get_port(playerID, &port);
	if (status != PJ_SUCCESS)
	{
		pjsua_player_destroy(playerID);
		PJSUA2_RAISE_ERROR2(status, "AudioPlayer::createPlayer()");
	}

	if (useAsyncCallback)
	{
		status = pjmedia_wav_player_set_eof_cb2(port, this, &onFileEndCallbackAsync);
	}
	else
	{
		status = pjmedia_wav_player_set_eof_cb(port, this, onFileEndCallback);
	}

	if (status != PJ_SUCCESS)
	{
		pjsua_player_destroy(playerID);
		PJSUA2_RAISE_ERROR2(status, "Audiolayer::createPlayer()");
	}

	portId = pjsua_player_get_conf_port(playerID);
	RegisterMediaPort(NULL);
}

PJ_DEF(pj_status_t) AudioPlayer::CreatePlayer(const pj_str_t* filename, unsigned options)
{
	if (filename == NULL || conferenceBridge == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer::CreatePlayer", "Invalid parameters");
		return PJ_FALSE;
	}

	unsigned slot, file_id;
	char path[PJ_MAXPATH];
	pj_pool_t* pool = NULL;
	pjmedia_port* port;
	pj_status_t status = PJ_SUCCESS;

	if (pjsua_var.player_cnt >= PJ_ARRAY_SIZE(pjsua_var.player))
		return PJ_ETOOMANY;

	PJ_LOG(4, (THIS_FILE, "Creating file player: %.*s..",
		(int)filename->slen, filename->ptr));
	pj_log_push_indent();

	PJSUA_LOCK();

	for (file_id = 0; file_id < PJ_ARRAY_SIZE(pjsua_var.player); ++file_id) {
		if (pjsua_var.player[file_id].port == NULL)
			break;
	}

	if (file_id == PJ_ARRAY_SIZE(pjsua_var.player)) {
		/* This is unexpected */
		pj_assert(0);
		status = PJ_EBUG;
		goto on_error;
	}

	pj_memcpy(path, filename->ptr, filename->slen);
	path[filename->slen] = '\0';

	pool = pjsua_pool_create("myPlayer", 1000, 1000);
	if (!pool)
	{
		status = PJ_ENOMEM;
		goto on_error;
	}

	status = pjmedia_wav_player_port_create(
		pool, path,
		pjsua_var.mconf_cfg.samples_per_frame *
		1000 / pjsua_var.media_cfg.channel_count /
		pjsua_var.media_cfg.clock_rate,
		options, 0, &port);
	if (status != PJ_SUCCESS)
	{
		pjsua_perror(THIS_FILE, "Unable to open file for playback", status);
		goto on_error;
	}

	slot = conferenceBridge->Add_port(port);

	pjsua_var.player[file_id].type = 0;
	pjsua_var.player[file_id].pool = pool;
	pjsua_var.player[file_id].port = port;
	pjsua_var.player[file_id].slot = slot;

	if (playerID < 0) playerID = file_id;

	++pjsua_var.player_cnt;

	PJSUA_UNLOCK();

	PJ_LOG(4, (THIS_FILE, "Player created, id=%d, slot=%d", file_id, slot));

	pj_log_pop_indent();
	return PJ_SUCCESS;

on_error:
	PJSUA_UNLOCK();
	if (pool) pj_pool_release(pool);
	pj_log_pop_indent();
	return status;
}

// ToDo: AudioPlayer can play a list of music BI #5088
//void AudioPlayer::PlayList(const std::vector<std::string> fileNames, bool loops)
//{
//	if (playerID != PJSUA_INVALID_ID)
//	{
//		PJSUA2_RAISE_ERROR(PJ_EEXISTS);
//	}
// 
// 	if (fileNames == NULL)
//{
//	PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer::PlayList", "Invalid parameter: fileNames");
//	return;
//}
//
//	unsigned int options = loops ? 0 : PJMEDIA_FILE_NO_LOOP;
//	unsigned int frameReadTime = 0; //zero triggers usage of default value
//	long bufferSize = 0; //zero triggers usage of default value
//
//	pj_status_t status = pjsua_player_get_port(playerID, &port);
//
//	if (status != PJ_SUCCESS)
//	{
//		pjsua_player_destroy(playerID);
//		PJSUA2_RAISE_ERROR2(status, "AudioPlayer::createPlaylist()");
//	}
//	status = pjmedia_wav_player_set_eof_cb2(port, this, &onFileEndCallback);
//	if (status != PJ_SUCCESS)
//	{
//		pjsua_player_destroy(playerID);
//		PJSUA2_RAISE_ERROR2(status, "AudioPlayer::createPlayer()");
//	}
//	conferencebridgePortId = pjsua_player_get_conf_port(playerID);
//
//	RegisterMediaPort2(NULL, pool);
//}
//
//PJ_DEF(pj_status_t) AudioPlayer::CreateList(const std::vector<std::string> file_names, const string& label, unsigned options)
//{
//	if (playerID != PJSUA_INVALID_ID) {
//		PJSUA2_RAISE_ERROR(PJ_EEXISTS);
//	}
//
// 	if (file_names == NULL ||label.empty())
//  {
//   PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer::CreateList", "Invalid parameter");
//   return PJ_FALSE;
//   }
//	pj_str_t pj_files[MAX_FILE_NAMES];
//	unsigned i, count = 0;
//	pj_str_t pj_lbl = str2Pj(label);
//	pj_status_t status;
//
//	count = PJ_ARRAY_SIZE(pj_files);
//
//	for (i = 0; i < file_names.size() && i < count; ++i)
//	{
//		const string& file_name = file_names[i];
//
//		pj_files[i] = str2Pj(file_name);
//	}
//
//	PJSUA2_CHECK_EXPR(pjsua_playlist_create(pj_files,
//		i,
//		&pj_lbl,
//		options,
//		&playerID));
//
//	/* Register EOF callback */
//	pjmedia_port* port;
//	status = pjsua_player_get_port(playerID, &port);
//	if (status != PJ_SUCCESS) 
//	{
//		pjsua_player_destroy(playerID);
//		PJSUA2_RAISE_ERROR2(status, "AudioPlayer::createPlaylist()");
//	}
//	status = pjmedia_wav_playlist_set_eof_cb2(port, this, &onFileEndCallback);
//	if (status != PJ_SUCCESS) 
//	{
//		pjsua_player_destroy(playerID);
//		PJSUA2_RAISE_ERROR2(status, "AudioPlayer::createPlaylist()");
//	}
//
//	/* Get media port id. */
//	conferencebridgePortId = pjsua_player_get_conf_port(playerID);
//
//	RegisterMediaPort2(NULL, pool);
//}


void AudioPlayer::RegisterMediaPort(MediaPort mediaPort) PJSUA2_THROW(Error)
{
	/* Check if media already added to Conf bridge. */
	if (!Endpoint::instance().AudioPlayerExists(portId) && mediaPort != NULL)
	{
		pj_assert(portId == PJSUA_INVALID_ID);

		pj_caching_pool_init(&mediaCachingPool, NULL, 0);

		mediaPool = pj_pool_create(&mediaCachingPool.factory, "media", 512, 512, NULL);

		if (!mediaPool)
		{
			pj_caching_pool_destroy(&mediaCachingPool);
			PJSUA2_RAISE_ERROR(PJ_ENOMEM);
		}

		PJSUA2_CHECK_EXPR(pjsua_conf_add_port(mediaPool, (pjmedia_port*)mediaPort, &portId));
	}
}

void AudioPlayer::RegisterMediaPort2(MediaPort mediaPort, pj_pool_t* pool)
PJSUA2_THROW(Error)
{
	/* Check if media already added to Conf bridge. */
	pj_assert(!Endpoint::instance().AudioPlayerExists(portId));

	pj_assert(portId == PJSUA_INVALID_ID);
	pj_assert(pool);

	PJSUA2_CHECK_EXPR(pjsua_conf_add_port(pool, (pjmedia_port*)mediaPort, &portId));
}

void AudioPlayer::UnregisterMediaPort()
{
	if (conferenceBridge != NULL)
	{
		if (portId != PJSUA_INVALID_ID)
		{
			conferenceBridge->Remove_port(portId);
			portId = PJSUA_INVALID_ID;
		}
	}
	else
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer::UnregisterMediaPort", "Invalid parameter: conferenceBridge");
	}

	if (mediaPool != NULL)
	{
		pj_pool_release(mediaPool);
		mediaPool = NULL;
		pj_caching_pool_destroy(&mediaCachingPool);
	}
}

pj_status_t AudioPlayer::OnFileEnd()
{
	return PJ_SUCCESS;
}

pj_status_t AudioPlayer::onFileEndCallback(pjmedia_port* port, void* usr_data)
{
	if (port == NULL || usr_data == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer::onFileEndCallback", "Invalid parameters");
		return PJ_FALSE;
	}

	AudioPlayer* self = static_cast<AudioPlayer*>(usr_data);
	return self->OnFileEnd();
}

void AudioPlayer::onFileEndCallbackAsync(pjmedia_port* port, void* usr_data)
{
	if (port == NULL || usr_data == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer::onFileEndCallbackAsync", "Invalid parameters");
		return;
	}

	PJ_UNUSED_ARG(port);
	AudioPlayer* audioPlayer = (AudioPlayer*)usr_data;
	audioPlayer->OnFileEndAsync();
}

unsigned int AudioPlayer::GetSlot()
{
	return portId;
}

pj_status_t AudioPlayer::SetPosition(int seconds)
{
	if (port == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer::SetPosition", "Invalid parameter: port");
		return PJ_FALSE;
	}

	double totalLengthInSeconds = GetTotalLength();
	double relativePosition = seconds / totalLengthInSeconds;
	if (relativePosition < 0) relativePosition = 0;
	if (relativePosition >= 1)
	{
		struct file_reader_port* filePort = (struct file_reader_port*)port;
		return pjmedia_wav_player_port_set_pos(port, filePort->data_len - 1);
	}

	pj_ssize_t fileSizeInBytes = pjmedia_wav_player_get_len(port);
	pj_uint32_t position = (pj_uint32_t)(fileSizeInBytes * relativePosition);

	pjmedia_wav_player_info info;
	pjmedia_wav_player_get_info(port, &info);

	position = position - (position % 8);

	return pjmedia_wav_player_port_set_pos(port, position);
}

pj_status_t AudioPlayer::Jump(int seconds)
{
	if (port == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer::Jump", "Invalid parameter: port");
		return PJ_FALSE;
	}

	double totalLengthInSeconds = GetTotalLength();
	pj_uint32_t position = pjmedia_wav_player_port_get_pos(port);
	pj_ssize_t fileSizeInBytes = pjmedia_wav_player_get_len(port);
	pj_uint32_t bytesToMove = (pj_uint32_t)((abs(seconds) / totalLengthInSeconds) * fileSizeInBytes);

	if (seconds < 0)
	{
		if (position < bytesToMove)
		{
			position = 0;
		}
		else
		{
			position = position - bytesToMove;
		}
	}
	else
	{
		position = position + bytesToMove;
		struct file_reader_port* filePort = (struct file_reader_port*)port;

		if (position >= filePort->data_len)
		{
			position = filePort->data_len - 1;
		}
	}

	return pjmedia_wav_player_port_set_pos(port, position);
}

// Unfortunately there is a small difference of about 0.5 seconds. Tested and compared with NAudio and Windows-Explorer
double AudioPlayer::GetTotalLength()
{
	if (port == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioPlayer::GetTotalLength", "Invalid parameter: port");
		return 0;
	}

	pjmedia_wav_player_info info;
	pjmedia_wav_player_get_info(port, &info);

	struct file_reader_port* filePort = (struct file_reader_port*)port;
	pjmedia_audio_format_detail* audioFormatDetail = pjmedia_format_get_audio_format_detail(&filePort->base.info.fmt, 1);

	return info.size_samples / audioFormatDetail->clock_rate;
}