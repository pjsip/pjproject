#include <atlstr.h> 
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include <pjsua2/audioOutputDevice.hpp>

using namespace pj;

#define THIS_FILE		"audioOutputDevice.cpp"

///////////////////////////////////////////////////////////////////////////////

AudioOutputDevice::AudioOutputDevice(ConferenceBridge* conferenceBridge) :
	conferenceBridge(conferenceBridge)
{
	if (conferenceBridge == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioOutputDevice..ctor", "Invalid parameter: conferenceBridge");
		return;
	}

	samplingRate = pjsua_var.media_cfg.clock_rate;
	channelCount = pjsua_var.mconf_cfg.channel_count;
	samplesPerFrame = pjsua_var.mconf_cfg.samples_per_frame;
	bitsPerSample = pjsua_var.mconf_cfg.bits_per_sample;
	bufferSize = conferenceBridge->Get_bytes_per_frame() * 7; // 7 * 640 ->  bufferSize = 4480

	CreateConferenceBridgePort(conferenceBridge);
	RegisterCallback();
}

AudioOutputDevice::AudioOutputDevice(ConferenceBridge* conferenceBridge, unsigned int samplingRate, int framesPerBuffer) :
	conferenceBridge(conferenceBridge)
{
	if (conferenceBridge == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioOutputDevice..ctor", "Invalid parameter: conferenceBridge");
		return;
	}

	samplingRate = samplingRate;
	channelCount = pjsua_var.mconf_cfg.channel_count;
	samplesPerFrame = pjsua_var.mconf_cfg.samples_per_frame;
	bitsPerSample = pjsua_var.mconf_cfg.bits_per_sample;
	bufferSize = conferenceBridge->Get_bytes_per_frame() * framesPerBuffer;

	CreateConferenceBridgePort(conferenceBridge);
	RegisterCallback();
}

AudioOutputDevice::~AudioOutputDevice()
{
	if (conferenceBridge != NULL)
	{
		conferenceBridge->Remove_port(slot);
	}
	
	if (port != NULL)
	{
		pjmedia_port_destroy(port);
	}
	
	if (pool != NULL)
	{
		pj_pool_release(pool);
	}
	
	free(outputBufferParam->frameBuffer);
	delete outputBufferParam;
	outputBufferParam = NULL;

	pj_status_t status = pjmedia_mem_player_set_eof_cb(port, this, NULL);
	if (status != PJ_SUCCESS)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "~AudioOutputDevice", "Error when cleaning up callback");
	}
}

void pj::AudioOutputDevice::CreateConferenceBridgePort(ConferenceBridge* conferenceBridge)
{
	if (conferenceBridge == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioOutputDevice::CreateConferenceBridgePort", "Invalid parameter: conferenceBridge");
		return;
	}

	outputBufferParam = new BufferParam();
	outputBufferParam->frameBuffer = (unsigned char*)calloc(bufferSize, sizeof(unsigned char));

	pool = pjsua_pool_create("memPlayer", 1000, 1000);
	pjmedia_mem_player_create(pool,
		outputBufferParam->frameBuffer,
		bufferSize,
		samplingRate,
		channelCount,
		samplesPerFrame,
		bitsPerSample,
		0,
		&port);
	slot = conferenceBridge->Add_port(port);
}

void AudioOutputDevice::RegisterCallback()
{
	if (port == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioOutputDevice::RegisterCallback", "Invalid parameter: port");
		return;
	}

	pjmedia_mem_player_set_eof_cb(port, this, OnReachedEndOfBufferCallback);
	//pjmedia_mem_player_set_eof_cb2(port, this, OnReachedEndOfBufferCallback2);
}

// Register a callback to be called when the buffer reading has reached the end of buffer. 
// If the player is set to play repeatedly, then the callback will be called multiple times. 
// Note that only one callback can be registered for each player port.
pj_status_t AudioOutputDevice::OnReachedEndOfBufferCallback(pjmedia_port* port, void* usr_data)
{
	if (usr_data == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioOutputDevice::OnReachedEndOfBufferCallback", "Invalid parameter: usr_data");
		return PJ_FALSE;
	}

	AudioOutputDevice* self = static_cast<AudioOutputDevice*>(usr_data);
	self->OnReachedEndOfBuffer();

	return PJ_SUCCESS;
}

void AudioOutputDevice::OnReachedEndOfBufferCallback2(pjmedia_port* port, void* usr_data)
{
	if (usr_data == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioOutputDevice::OnReachedEndOfBufferCallback2", "Invalid parameter: usr_data");
		return;
	}

	AudioOutputDevice* self = static_cast<AudioOutputDevice*>(usr_data);
	self->OnReachedEndOfBuffer();
}

void AudioOutputDevice::SetBuffer(unsigned char* frameBuffer, int bytesToRead)
{
	if (outputBufferParam == NULL || frameBuffer == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioOutputDevice::SetBuffer", "Invalid parameters");
		return;
	}

	pj_memcpy(outputBufferParam->frameBuffer, frameBuffer, bytesToRead);
	outputBufferParam->bytesToRead = bytesToRead;
}

unsigned int AudioOutputDevice::GetSlot()
{
	return slot;
}

int AudioOutputDevice::GetBufferSize()
{
	return bufferSize;
}