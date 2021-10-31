#include "pjsua.h"
#include "endpoint.hpp"

#pragma once
class ConferenceBridge
{
	friend class pj::Endpoint;
	friend class AudioPlayer;

public:
	ConferenceBridge(const pj::MediaConfig& config);
	virtual ~ConferenceBridge();

	virtual void LinkMediaStreams(unsigned int slotA, unsigned int slotB);
	virtual void DisconnectMediaStreams(unsigned int sourceSlot, unsigned int sinkSlot);

	virtual unsigned int Add_port(void* stream_port);
	virtual void Remove_port(unsigned int callSlot);
	virtual void Adjust_receiving_signal_level_from_slot(unsigned int callSlot, int level);
	unsigned int Get_bytes_per_frame();
	virtual pj_status_t StopClock();

private:
	pj_pool_t				*pool;
	pjmedia_conf			*conference;
	pjmedia_port			*conferencePort;
	pjmedia_port			*nullPort;
	pjmedia_master_port		*masterPort;	
	unsigned int			bytes_per_frame;
};