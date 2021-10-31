#ifndef __PJSUA2_AUDIOINPUTDEVICE_HPP__
#define __PJSUA2_AUDIOINPUTDEVICE_HPP__

#include "conferenceBridge.hpp"

namespace pj
{

#pragma once
	class AudioInputDevice
	{
	public:
		AudioInputDevice(ConferenceBridge* conferenceBridge);
		AudioInputDevice(ConferenceBridge * conferenceBridge, unsigned int samplingRate, int bufferSize);		
//		~AudioInputDevice();
		virtual ~AudioInputDevice();
		
		void CreateConferenceBridgePort(ConferenceBridge* conferenceBridge);
		
		unsigned int GetSlot();
		int GetBufferSize();

		unsigned int GetSamplingRate() const { return samplingRate; }
		unsigned int GetChannelCount() const { return channelCount; }
		unsigned int GetSamplesPerFrame() const { return samplesPerFrame; }
		unsigned int GetBitsPerSample() const { return bitsPerSample; }		

		struct OnBufferReceivedParam
		{
			unsigned char *frameBuffer;
			int bytesToRead;
		};		
		
		virtual void onBufferReceived(OnBufferReceivedParam &prm)
		{
			PJ_UNUSED_ARG(prm);
		}

		OnBufferReceivedParam *onBufferReceivedParam;
		
	private:
		pj_pool_t *pool;
		pjmedia_port *port;
		unsigned int slot;
		ConferenceBridge* conferenceBridge;
		unsigned int samplingRate;
		unsigned int channelCount;
		unsigned int samplesPerFrame;
		unsigned int bitsPerSample;
		pj_size_t bufferSize;

		void RegisterCallback();
		static pj_status_t OnBufferReceivedCallback(pjmedia_port* port, void* usr_data);
		static void OnBufferReceivedCallback2(pjmedia_port * port, void * usr_data);
	};
}
#endif