#include <memory>
#include <pjsua2/types.hpp>
#include "conferenceBridge.hpp"

#pragma once
class AudioPlayer
{

public:
	AudioPlayer(const std::string& fileName, ConferenceBridge* conferenceBridge, bool loops = false, bool useAsyncCallback = false);
	
	// ToDo: AudioPlayer can play a list of music BI #5088
	//AudioPlayer(const std::vector<std::string> fileNames, ConferenceBridge* conferenceBridge, bool loops = false);
	virtual ~AudioPlayer();

	unsigned int GetSlot();

	pj_status_t SetPosition(int seconds);
	pj_status_t Jump(int seconds);
	double GetTotalLength();
	virtual pj_status_t OnFileEnd();
	virtual void OnFileEndAsync() { };

private:
	pj_pool_t* pool;
	pjmedia_port* port;
	ConferenceBridge* conferenceBridge;
	typedef void* MediaPort;
	pj_caching_pool mediaCachingPool;
	pj_pool_t* mediaPool;
	pjsua_player_id playerID;

	void PlayFile(const std::string& fileName, bool loops, bool useAsyncCallback);
	pj_status_t CreatePlayer(const pj_str_t* filename, unsigned options);	

	// ToDo: AudioPlayer can play a list of music BI #5088
	//void PlayList(const std::vector<std::string> file_names, bool loops);
	//pj_status_t CreateList(const std::vector<std::string> file_names, const string& label, unsigned options);

	static pj_status_t onFileEndCallback(pjmedia_port* port, void* usr_data);
	static void onFileEndCallbackAsync(pjmedia_port* port, void* usr_data);

	struct file_reader_port
	{
		pjmedia_port     base;
		unsigned	     options;
		pjmedia_wave_fmt_tag fmt_tag;
		pj_uint16_t	     bytes_per_sample;
		pj_bool_t	     eof;
		pj_uint32_t	     bufsize;
		char* buf;
		char* readpos;
		char* eofpos;

		pj_off_t	     fsize;
		unsigned	     start_data;
		unsigned         data_len;
		unsigned         data_left;
		pj_off_t	     fpos;
		pj_oshandle_t    fd;

		pj_status_t(*cb)(pjmedia_port*, void*);
		pj_bool_t	     subscribed;
		void	   (*cb2)(pjmedia_port*, void*);
	};

protected:
	void RegisterMediaPort2(MediaPort mediaPort, pj_pool_t* pool) PJSUA2_THROW(Error);
	void RegisterMediaPort(MediaPort port) PJSUA2_THROW(Error);
	void UnregisterMediaPort();
	int portId;
};