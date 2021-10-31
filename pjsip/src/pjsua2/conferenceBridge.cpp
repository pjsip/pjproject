#include <pjsua2/conferenceBridge.hpp>

using namespace pj;

#define THIS_FILE		"conferenceBridge.cpp"

ConferenceBridge::ConferenceBridge(const pj::MediaConfig& config)
{
	pool = pjsua_pool_create("mycall", 1000, 1000);

	unsigned int bits_per_sample = 16;
	unsigned int samples_per_frame = config.clockRate * config.channelCount * config.audioFramePtime / 1000;
	bytes_per_frame = (bits_per_sample / 8) * samples_per_frame;

	pj_status_t status = pjmedia_conf_create(pool, config.maxMediaPorts, config.clockRate, config.channelCount, samples_per_frame, bits_per_sample, PJMEDIA_CONF_NO_DEVICE, &conference);
	conferencePort = pjmedia_conf_get_master_port(conference);
	status = pjmedia_null_port_create(pool, config.clockRate, config.channelCount, samples_per_frame, bits_per_sample, &nullPort);
	status = pjmedia_master_port_create(pool, nullPort, conferencePort, 0, &masterPort);
	status = pjmedia_master_port_start(masterPort);
}

ConferenceBridge::~ConferenceBridge()
{
	if (masterPort != NULL)
	{
		pjmedia_master_port_stop(masterPort);
		pjmedia_master_port_destroy(masterPort, PJ_FALSE);
	}
	else
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "~ConferenceBridge", "Invalid parameter: masterPort");
	}

	if (conference != NULL)
	{
		pjmedia_conf_destroy(conference);
	}
	else
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "~ConferenceBridge", "Invalid parameter: conference");
	}

	if (nullPort != NULL)
	{
		pjmedia_port_destroy(nullPort);
	}
	else
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "~ConferenceBridge", "Invalid parameter: nullPort");
	}
	
	if (pool != NULL)
	{
		pj_pool_release(pool);
	}
	else
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "~ConferenceBridge", "Invalid parameter: pool");
	}	
}

void ConferenceBridge::LinkMediaStreams(unsigned int sourceSlot, unsigned int sinkSlot)
{
	if (conference == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "ConferenceBridge::LinkMediaStreams", "Invalid parameter: conference");
		return;
	}

	pjmedia_conf_connect_port(conference, sourceSlot, sinkSlot, 0);
}

void ConferenceBridge::DisconnectMediaStreams(unsigned int sourceSlot, unsigned int sinkSlot)
{
	if (conference == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "ConferenceBridge::DisconnectMediaStreams", "Invalid parameter: conference");
		return;
	}

	pjmedia_conf_disconnect_port(conference, sourceSlot, sinkSlot);
}

unsigned int ConferenceBridge::Add_port(void* stream_port)
{
	if (stream_port == NULL || conference == NULL || pool == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "ConferenceBridge::Add_port", "Invalid parameters");
		return 0;
	}

	unsigned int callSlot;
	pj_status_t status = pjmedia_conf_add_port(conference, pool, (pjmedia_port*)stream_port, NULL, &callSlot);
	return callSlot;
}

unsigned int ConferenceBridge::Get_bytes_per_frame()
{
	return bytes_per_frame;
}

void ConferenceBridge::Remove_port(unsigned int callSlot)
{
	if (conference == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "ConferenceBridge::Remove_port", "Invalid parameter: conference");
		return;
	}

	pjmedia_conf_remove_port(conference, callSlot);
}

void ConferenceBridge::Adjust_receiving_signal_level_from_slot(unsigned int callSlot, int level)
{
	if (conference == NULL)
	{
		PJSUA2_RAISE_ERROR3(PJ_EINVAL, "ConferenceBridge::Adjust_receiving_signal_level_from_slot", "Invalid parameter: conference");
		return;
	}

	pjmedia_conf_adjust_rx_level(conference, callSlot, level);
}

pj_status_t ConferenceBridge::StopClock()
{
	return pjmedia_master_port_stop(masterPort);
}