#include "conferenceBridge.hpp"

#pragma once
class AudioRecorder
{
public:
	AudioRecorder(const std::string& file, ConferenceBridge* conferenceBridge);
	~AudioRecorder();

	unsigned int GetSlot();

private:
	pj_pool_t* pool;
	pjmedia_port* port;
	unsigned int slot;
	ConferenceBridge* conferenceBridge;
};
