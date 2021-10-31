#ifndef __PJSUA2_AUDIOOUTPUTDEVICE_HPP__
#define __PJSUA2_AUDIOOUTPUTDEVICE_HPP__

#include "conferenceBridge.hpp"

namespace pj
{

#pragma once
	class AudioOutputDevice
	{
	public:
		AudioOutputDevice(ConferenceBridge* conferenceBridge);
		AudioOutputDevice(ConferenceBridge * conferenceBridge, unsigned int samplingRate, int framesPerBuffer);	
		virtual ~AudioOutputDevice();
		
		void CreateConferenceBridgePort(ConferenceBridge* conferenceBridge);
		
		unsigned int GetSlot();
		int GetBufferSize();


		unsigned int GetSamplingRate() const { return samplingRate; }
		unsigned int GetChannelCount() const { return channelCount; }
		unsigned int GetSamplesPerFrame() const { return samplesPerFrame; }
		unsigned int GetBitsPerSample() const { return bitsPerSample; }
		void SetBuffer(unsigned char * frameBuffer, int bytesToRead);

		struct BufferParam
		{
			unsigned char *frameBuffer;
			int bytesToRead;
		};

		virtual void OnReachedEndOfBuffer()
		{
			PJ_UNUSED_ARG(PJ_SUCCESS);
		}
		
		BufferParam *outputBufferParam;
		
	private:				
		pj_pool_t *pool;
		pjmedia_port *port;
		unsigned int slot;
		ConferenceBridge *conferenceBridge;
		unsigned int samplingRate;
		unsigned int channelCount;
		unsigned int samplesPerFrame;
		unsigned int bitsPerSample;
		pj_size_t bufferSize;

		void RegisterCallback();
		static pj_status_t OnReachedEndOfBufferCallback(pjmedia_port* port, void* usr_data);
		static void OnReachedEndOfBufferCallback2(pjmedia_port * port, void * usr_data);
	};
}
#endif