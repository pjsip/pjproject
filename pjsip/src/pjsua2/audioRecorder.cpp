#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include <pjsua2/audioRecorder.hpp>
#include <pjsua2/types.hpp>
#include "util.hpp"

using namespace pj;

#define THIS_FILE		"audioRecorder.cpp"

AudioRecorder::AudioRecorder(const std::string& file, ConferenceBridge* conferenceBridge) : 
	conferenceBridge(conferenceBridge)
{
	if (conferenceBridge == NULL || file.empty())
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioRecorder..ctor", "Invalid parameters");
		return;
	}

	pool = pjsua_pool_create("myRecorder", 1000, 1000);
	pjmedia_wav_writer_port_create(
		pool, 
		file.c_str(),
		pjsua_var.media_cfg.clock_rate,
		pjsua_var.mconf_cfg.channel_count,
		pjsua_var.mconf_cfg.samples_per_frame,
		pjsua_var.mconf_cfg.bits_per_sample,
		0,
		0,
		&port);
	slot = conferenceBridge->Add_port(port);
}

AudioRecorder::~AudioRecorder()
{
	if (conferenceBridge != NULL)
	{
		conferenceBridge->Remove_port(slot);
	}
	else
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "~AudioRecorder", "Invalid parameter: conferenceBridge");
	}

	if (port != NULL)
	{
		pjmedia_port_destroy(port);
	}
	else
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "~AudioRecorder", "Invalid parameter: port");
	}

	if (pool != NULL)
	{
		pj_pool_release(pool);
	}
	else
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "~AudioRecorder", "Invalid parameter: pool");
	}
}

unsigned int AudioRecorder::GetSlot()
{
	return slot;
}