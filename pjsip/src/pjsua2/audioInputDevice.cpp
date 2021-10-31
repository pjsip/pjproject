#include <atlstr.h> 
#include <pjsua-lib/pjsua.h>
#include <pjsua-lib/pjsua_internal.h>
#include <pjsua2/audioInputDevice.hpp>
#include <pjsua2/types.hpp>

using namespace pj;

#define THIS_FILE		"audioInputDevice.cpp"

///////////////////////////////////////////////////////////////////////////////

AudioInputDevice::AudioInputDevice(ConferenceBridge* conferenceBridge) :
	conferenceBridge(conferenceBridge)
{
	if (conferenceBridge == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioInputDevice..ctor", "Invalid parameter: conferenceBridge");
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

AudioInputDevice::AudioInputDevice(ConferenceBridge* conferenceBridge, unsigned int samplingRate, int bufferSize) :
	conferenceBridge(conferenceBridge)
{
	if (conferenceBridge == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioInputDevice..ctor", "Invalid parameter: conferenceBridge");
		return;
	}

	samplingRate = samplingRate;
	channelCount = pjsua_var.mconf_cfg.channel_count;
	samplesPerFrame = pjsua_var.mconf_cfg.samples_per_frame;
	bitsPerSample = pjsua_var.mconf_cfg.bits_per_sample;
	this->bufferSize = bufferSize;

	CreateConferenceBridgePort(conferenceBridge);
	RegisterCallback();
}

AudioInputDevice::~AudioInputDevice()
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
	
	free(onBufferReceivedParam->frameBuffer);
	delete onBufferReceivedParam;
	onBufferReceivedParam = NULL;

	pj_status_t status = pjmedia_mem_capture_set_eof_cb(port, this, NULL);
	if (status != PJ_SUCCESS)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "~AudioInputDevice", "Error when cleaning up callback");
	}
}

void pj::AudioInputDevice::CreateConferenceBridgePort(ConferenceBridge* conferenceBridge)
{
	if (conferenceBridge == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioInputDevice::CreateConferenceBridgePort", "Invalid parameter: conferenceBridge");
		return;
	}

	onBufferReceivedParam = new OnBufferReceivedParam();
	onBufferReceivedParam->frameBuffer = (unsigned char*)calloc(bufferSize, sizeof(unsigned char));

	pool = pjsua_pool_create("memCapture", 1000, 1000);
	pjmedia_mem_capture_create(pool,
		onBufferReceivedParam->frameBuffer,
		bufferSize,
		samplingRate,
		channelCount,
		samplesPerFrame,
		bitsPerSample,
		0,
		&port);
	slot = conferenceBridge->Add_port(port);

	pjmedia_mem_capture_set_eof_cb(port, this, OnBufferReceivedCallback);
	//pjmedia_mem_capture_set_eof_cb2(port, this, OnBufferReceivedCallback2);
}

void AudioInputDevice::RegisterCallback()
{
	if (port == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioInputDevice::RegisterCallback", "Invalid parameter: port");
		return;
	}

	pjmedia_mem_capture_set_eof_cb(port, this, OnBufferReceivedCallback);
	//pjmedia_mem_capture_set_eof_cb2(port, this, OnBufferReceivedCallback2);
}

// Register a callback to be called when no space left in the buffer. Note that when a callback is registered, 
// this callback will also be called when application destroys the port and the callback has not been called before.
pj_status_t AudioInputDevice::OnBufferReceivedCallback(pjmedia_port* port, void* usr_data)
{
	if (port == NULL || usr_data == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioInputDevice::OnBufferReceivedCallback", "Invalid parameters");
		return PJ_FALSE;
	}

	AudioInputDevice* self = static_cast<AudioInputDevice*>(usr_data);

	self->onBufferReceivedParam->bytesToRead = (int)pjmedia_mem_capture_get_size(self->port);
	self->onBufferReceived(*self->onBufferReceivedParam);

	return PJ_SUCCESS;
}

void AudioInputDevice::OnBufferReceivedCallback2(pjmedia_port* port, void* usr_data)
{
	if (port == NULL || usr_data == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "AudioInputDevice::OnBufferReceivedCallback2", "Invalid parameters");
		return;
	}

	AudioInputDevice* self = static_cast<AudioInputDevice*>(usr_data);

	self->onBufferReceivedParam->bytesToRead = (int)pjmedia_mem_capture_get_size(self->port);
	self->onBufferReceived(*self->onBufferReceivedParam);
}

unsigned int AudioInputDevice::GetSlot()
{
	return slot;
}

int AudioInputDevice::GetBufferSize()
{
	return bufferSize;
}