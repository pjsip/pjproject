#include <pjmedia/sound.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/string.h>

#if PJMEDIA_SOUND_IMPLEMENTATION == PJMEDIA_SOUND_WIN32_MME_SOUND

#ifdef _MSC_VER
#   pragma warning(push, 3)
#endif

#include <windows.h>
#include <mmsystem.h>

#ifdef _MSC_VER
#   pragma warning(pop)
#endif

#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
#   pragma comment(lib, "Coredll.lib")
#elif defined(_MSC_VER)
#   pragma comment(lib, "winmm.lib")
#endif


#define THIS_FILE			"wmme_sound.c"
#define BITS_PER_SAMPLE			16
#define BYTES_PER_SAMPLE		(BITS_PER_SAMPLE/8)

#define MAX_PACKET_BUFFER_COUNT		32
#define MAX_HARDWARE			16

struct wmme_dev_info
{
    pjmedia_snd_dev_info info;
    unsigned             deviceId;
};

static unsigned dev_count;
static struct wmme_dev_info dev_info[MAX_HARDWARE];
static pj_bool_t snd_initialized = PJ_FALSE;

/* Latency settings */
static unsigned snd_input_latency  = PJMEDIA_SND_DEFAULT_REC_LATENCY;
static unsigned snd_output_latency = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;


/* Individual WMME capture/playback stream descriptor */
struct wmme_stream
{
    union
    {
	HWAVEIN   In;
	HWAVEOUT  Out;
    } hWave;

    WAVEHDR      *WaveHdr;
    HANDLE        hEvent;
    DWORD         dwBufIdx;
    DWORD         dwMaxBufIdx;
    pj_timestamp  timestamp;
};


/* Sound stream. */
struct pjmedia_snd_stream
{
    pjmedia_dir          dir;               /**< Sound direction.      */
    int                  play_id;           /**< Playback dev id.      */
    int                  rec_id;            /**< Recording dev id.     */
    pj_pool_t           *pool;              /**< Memory pool.          */

    pjmedia_snd_rec_cb   rec_cb;            /**< Capture callback.     */
    pjmedia_snd_play_cb  play_cb;           /**< Playback callback.    */
    void                *user_data;         /**< Application data.     */

    struct wmme_stream   play_strm;         /**< Playback stream.      */
    struct wmme_stream   rec_strm;          /**< Capture stream.       */

    void    		*buffer;	    /**< Temp. frame buffer.   */
    unsigned             clock_rate;        /**< Clock rate.           */
    unsigned             samples_per_frame; /**< Samples per frame.    */
    unsigned             bits_per_sample;   /**< Bits per sample.      */
    unsigned             channel_count;     /**< Channel count.        */

    pj_thread_t         *thread;            /**< Thread handle.        */
    HANDLE               thread_quit_event; /**< Quit signal to thread */
};


static pj_pool_factory *pool_factory;

static void init_waveformatex (LPWAVEFORMATEX pcmwf, 
			       unsigned clock_rate,
			       unsigned channel_count)
{
    pj_bzero(pcmwf, sizeof(PCMWAVEFORMAT)); 
    pcmwf->wFormatTag = WAVE_FORMAT_PCM; 
    pcmwf->nChannels = (pj_uint16_t)channel_count;
    pcmwf->nSamplesPerSec = clock_rate;
    pcmwf->nBlockAlign = (pj_uint16_t)(channel_count * BYTES_PER_SAMPLE);
    pcmwf->nAvgBytesPerSec = clock_rate * channel_count * BYTES_PER_SAMPLE;
    pcmwf->wBitsPerSample = BITS_PER_SAMPLE;
}


/*
 * Initialize WMME player device.
 */
static pj_status_t init_player_stream(  pj_pool_t *pool,
				        struct wmme_stream *wmme_strm,
					int dev_id,
					unsigned clock_rate,
					unsigned channel_count,
					unsigned samples_per_frame,
					unsigned buffer_count)
{
    MMRESULT mr;
    WAVEFORMATEX pcmwf; 
    unsigned bytes_per_frame;
    unsigned i;

    PJ_ASSERT_RETURN(buffer_count <= MAX_PACKET_BUFFER_COUNT, PJ_EINVAL);

    /* Check device ID */
    if (dev_id == -1)
	dev_id = 0;

    PJ_ASSERT_RETURN(dev_id >= 0 && dev_id < (int)dev_count, PJ_EINVAL);

    /*
     * Create a wait event.
     */
    wmme_strm->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == wmme_strm->hEvent)
	return pj_get_os_error();

    /*
     * Set up wave format structure for opening the device.
     */
    init_waveformatex(&pcmwf, clock_rate, channel_count);
    bytes_per_frame = samples_per_frame * BYTES_PER_SAMPLE;

    /*
     * Open wave device.
     */
    mr = waveOutOpen(&wmme_strm->hWave.Out, dev_info[dev_id].deviceId, &pcmwf, 
		     (DWORD)wmme_strm->hEvent, 0, CALLBACK_EVENT);
    if (mr != MMSYSERR_NOERROR)
	/* TODO: This is for HRESULT/GetLastError() */
	PJ_RETURN_OS_ERROR(mr);

    /* Pause the wave out device */
    mr = waveOutPause(wmme_strm->hWave.Out);
    if (mr != MMSYSERR_NOERROR)
	/* TODO: This is for HRESULT/GetLastError() */
	PJ_RETURN_OS_ERROR(mr);

    /*
     * Create the buffers. 
     */
    wmme_strm->WaveHdr = pj_pool_zalloc(pool, sizeof(WAVEHDR) * buffer_count);
    for (i = 0; i < buffer_count; ++i)
    {
	wmme_strm->WaveHdr[i].lpData = pj_pool_zalloc(pool, bytes_per_frame);
	wmme_strm->WaveHdr[i].dwBufferLength = bytes_per_frame;
	mr = waveOutPrepareHeader(wmme_strm->hWave.Out, 
				  &(wmme_strm->WaveHdr[i]),
				  sizeof(WAVEHDR));
	if (mr != MMSYSERR_NOERROR)
	    /* TODO: This is for HRESULT/GetLastError() */
	    PJ_RETURN_OS_ERROR(mr); 
	mr = waveOutWrite(wmme_strm->hWave.Out, &(wmme_strm->WaveHdr[i]), 
			  sizeof(WAVEHDR));
	if (mr != MMSYSERR_NOERROR)
	    /* TODO: This is for HRESULT/GetLastError() */
	    PJ_RETURN_OS_ERROR(mr);
    }

    wmme_strm->dwBufIdx = 0;
    wmme_strm->dwMaxBufIdx = buffer_count;
    wmme_strm->timestamp.u64 = 0;

    /* Done setting up play device. */
    PJ_LOG(5, (THIS_FILE, 
	       " WaveAPI Sound player \"%s\" initialized (clock_rate=%d, "
	       "channel_count=%d, samples_per_frame=%d (%dms))",
	       dev_info[dev_id].info.name,
	       clock_rate, channel_count, samples_per_frame,
	       samples_per_frame * 1000 / clock_rate));

    return PJ_SUCCESS;
}


/*
 * Initialize Windows Multimedia recorder device
 */
static pj_status_t init_capture_stream( pj_pool_t *pool,
					struct wmme_stream *wmme_strm,
					int dev_id,
					unsigned clock_rate,
					unsigned channel_count,
					unsigned samples_per_frame,
					unsigned buffer_count)
{
    MMRESULT mr;
    WAVEFORMATEX pcmwf; 
    unsigned bytes_per_frame;
    unsigned i;

    PJ_ASSERT_RETURN(buffer_count <= MAX_PACKET_BUFFER_COUNT, PJ_EINVAL);

    /* Check device ID */
    if (dev_id == -1)
	dev_id = 0;

    PJ_ASSERT_RETURN(dev_id >= 0 && dev_id < (int)dev_count, PJ_EINVAL);

    /*
    * Create a wait event.
    */
    wmme_strm->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == wmme_strm->hEvent)
	return pj_get_os_error();

    /*
     * Set up wave format structure for opening the device.
     */
    init_waveformatex(&pcmwf, clock_rate, channel_count);
    bytes_per_frame = samples_per_frame * BYTES_PER_SAMPLE;

    /*
     * Open wave device.
     */
    mr = waveInOpen(&wmme_strm->hWave.In, dev_info[dev_id].deviceId, &pcmwf, 
		    (DWORD)wmme_strm->hEvent, 0, CALLBACK_EVENT);
    if (mr != MMSYSERR_NOERROR)
	/* TODO: This is for HRESULT/GetLastError() */
	PJ_RETURN_OS_ERROR(mr);

    /*
     * Create the buffers. 
     */
    wmme_strm->WaveHdr = pj_pool_zalloc(pool, sizeof(WAVEHDR) * buffer_count);
    for (i = 0; i < buffer_count; ++i)
    {
	wmme_strm->WaveHdr[i].lpData = pj_pool_zalloc(pool, bytes_per_frame);
	wmme_strm->WaveHdr[i].dwBufferLength = bytes_per_frame;
	mr = waveInPrepareHeader(wmme_strm->hWave.In, &(wmme_strm->WaveHdr[i]),
							sizeof(WAVEHDR));
	if (mr != MMSYSERR_NOERROR)
	    /* TODO: This is for HRESULT/GetLastError() */
	    PJ_RETURN_OS_ERROR(mr);
	mr = waveInAddBuffer(wmme_strm->hWave.In, &(wmme_strm->WaveHdr[i]), 
			     sizeof(WAVEHDR));
	if (mr != MMSYSERR_NOERROR)
	    /* TODO: This is for HRESULT/GetLastError() */
	    PJ_RETURN_OS_ERROR(mr);
    }

    wmme_strm->dwBufIdx = 0;
    wmme_strm->dwMaxBufIdx = buffer_count;
    wmme_strm->timestamp.u64 = 0;

    /* Done setting up play device. */
    PJ_LOG(5,(THIS_FILE, 
	" WaveAPI Sound recorder \"%s\" initialized (clock_rate=%d, "
	"channel_count=%d, samples_per_frame=%d (%dms))",
	dev_info[dev_id].info.name,
	clock_rate, channel_count, samples_per_frame,
	samples_per_frame * 1000 / clock_rate));

    return PJ_SUCCESS;
}



/*
* WMME capture and playback thread.
*/
static int PJ_THREAD_FUNC wmme_dev_thread(void *arg)
{
    pjmedia_snd_stream *strm = arg;
    HANDLE events[3];
    unsigned eventCount;
    unsigned bytes_per_frame;
    pj_status_t status = PJ_SUCCESS;


    eventCount = 0;
    events[eventCount++] = strm->thread_quit_event;
    if (strm->dir & PJMEDIA_DIR_PLAYBACK)
	events[eventCount++] = strm->play_strm.hEvent;
    if (strm->dir & PJMEDIA_DIR_CAPTURE)
	events[eventCount++] = strm->rec_strm.hEvent;


    /* Raise self priority. We don't want the audio to be distorted by
     * system activity.
     */
#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE != 0
    if (strm->dir & PJMEDIA_DIR_PLAYBACK)
	CeSetThreadPriority(GetCurrentThread(), 153);
    else
	CeSetThreadPriority(GetCurrentThread(), 247);
#else
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    /* Calculate bytes per frame */
    bytes_per_frame = strm->samples_per_frame * BYTES_PER_SAMPLE;

    /*
     * Loop while not signalled to quit, wait for event objects to be 
     * signalled by WMME capture and play buffer.
     */
    while (status == PJ_SUCCESS)
    {

	DWORD rc;
	pjmedia_dir signalled_dir;

	rc = WaitForMultipleObjects(eventCount, events, FALSE, INFINITE);
	if (rc < WAIT_OBJECT_0 || rc >= WAIT_OBJECT_0 + eventCount)
	    continue;

	if (rc == WAIT_OBJECT_0)
	    break;

	if (rc == (WAIT_OBJECT_0 + 1))
	{
	    if (events[1] == strm->play_strm.hEvent)
		signalled_dir = PJMEDIA_DIR_PLAYBACK;
	    else
		signalled_dir = PJMEDIA_DIR_CAPTURE;
	}
	else
	{
	    if (events[2] == strm->play_strm.hEvent)
		signalled_dir = PJMEDIA_DIR_PLAYBACK;
	    else
		signalled_dir = PJMEDIA_DIR_CAPTURE;
	}


	if (signalled_dir == PJMEDIA_DIR_PLAYBACK)
	{
	    struct wmme_stream *wmme_strm = &strm->play_strm;
	    MMRESULT mr = MMSYSERR_NOERROR;
	    status = PJ_SUCCESS;

	    /*
	    * Windows Multimedia has requested us to feed some frames to
	    * playback buffer.
	    */

	    while (wmme_strm->WaveHdr[wmme_strm->dwBufIdx].dwFlags & WHDR_DONE)
	    {
		void* buffer = wmme_strm->WaveHdr[wmme_strm->dwBufIdx].lpData;

		PJ_LOG(5,(THIS_FILE, "Finished writing buffer %d", 
			  wmme_strm->dwBufIdx));

		/* Get frame from application. */
		status = (*strm->play_cb)(strm->user_data, 
					  wmme_strm->timestamp.u32.lo,
					  buffer,
					  bytes_per_frame);

		if (status != PJ_SUCCESS)
		    break;

		/* Write to the device. */
		mr = waveOutWrite(wmme_strm->hWave.Out, 
				  &(wmme_strm->WaveHdr[wmme_strm->dwBufIdx]), 
				  sizeof(WAVEHDR));
		if (mr != MMSYSERR_NOERROR)
		{
		    status = PJ_STATUS_FROM_OS(mr);
		    break;
		}

		/* Increment position. */
		if (++wmme_strm->dwBufIdx >= wmme_strm->dwMaxBufIdx)
		    wmme_strm->dwBufIdx = 0;
		wmme_strm->timestamp.u64 += strm->samples_per_frame / 
					    strm->channel_count;
	    }
	}
	else
	{
	    struct wmme_stream *wmme_strm = &strm->rec_strm;
	    MMRESULT mr = MMSYSERR_NOERROR;
	    status = PJ_SUCCESS;

	    /*
	    * Windows Multimedia has indicated that it has some frames ready
	    * in the capture buffer. Get as much frames as possible to
	    * prevent overflows.
	    */
#if 0
	    {
		static DWORD tc = 0;
		DWORD now = GetTickCount();
		DWORD i = 0;
		DWORD bits = 0;

		if (tc == 0) tc = now;

		for (i = 0; i < wmme_strm->dwMaxBufIdx; ++i)
		{
		    bits = bits << 4;
		    bits |= wmme_strm->WaveHdr[i].dwFlags & WHDR_DONE;
		}
		PJ_LOG(5,(THIS_FILE, "Record Signal> Index: %d, Delta: %4.4d, "
			  "Flags: %6.6x\n",
			  wmme_strm->dwBufIdx,
			  now - tc,
			  bits));
		tc = now;
	    }
#endif

	    while (wmme_strm->WaveHdr[wmme_strm->dwBufIdx].dwFlags & WHDR_DONE)
	    {
		char* buffer = (char*)
			       wmme_strm->WaveHdr[wmme_strm->dwBufIdx].lpData;
		unsigned cap_len = 
			wmme_strm->WaveHdr[wmme_strm->dwBufIdx].dwBytesRecorded;

		/*
		PJ_LOG(5,(THIS_FILE, "Read %d bytes from buffer %d", cap_len, 
			  wmme_strm->dwBufIdx));
		*/

		if (cap_len < bytes_per_frame)
		    pj_bzero(buffer + cap_len, bytes_per_frame - cap_len);

		/* Copy the audio data out of the wave buffer. */
		pj_memcpy(strm->buffer, buffer, bytes_per_frame);

		/* Re-add the buffer to the device. */
		mr = waveInAddBuffer(wmme_strm->hWave.In, 
				     &(wmme_strm->WaveHdr[wmme_strm->dwBufIdx]), 
				     sizeof(WAVEHDR));
		if (mr != MMSYSERR_NOERROR) {
		    status = PJ_STATUS_FROM_OS(mr);
		    break;
		}

		/* Call callback */
		status = (*strm->rec_cb)(strm->user_data, 
					 wmme_strm->timestamp.u32.lo, 
					 strm->buffer, 
					 bytes_per_frame);

		if (status != PJ_SUCCESS)
		    break;

		/* Increment position. */
		if (++wmme_strm->dwBufIdx >= wmme_strm->dwMaxBufIdx)
		    wmme_strm->dwBufIdx = 0;
		wmme_strm->timestamp.u64 += strm->samples_per_frame / 
					    strm->channel_count;
	    }
	}
    }

    PJ_LOG(5,(THIS_FILE, "WMME: thread stopping.."));
    return 0;
}


/*
* Init sound library.
*/
PJ_DEF(pj_status_t) pjmedia_snd_init(pj_pool_factory *factory)
{
    unsigned c;
    int i;
    int inputDeviceCount, outputDeviceCount, maximumPossibleDeviceCount;

    if (snd_initialized)
	return PJ_SUCCESS;

    pj_bzero(&dev_info, sizeof(dev_info));

    dev_count = 0;
    pool_factory = factory;

    /* Enumerate sound playback devices */
    maximumPossibleDeviceCount = 0;

    inputDeviceCount = waveInGetNumDevs();
    if (inputDeviceCount > 0)
	/* assume there is a WAVE_MAPPER */
	maximumPossibleDeviceCount += inputDeviceCount + 1;

    outputDeviceCount = waveOutGetNumDevs();
    if (outputDeviceCount > 0)
	/* assume there is a WAVE_MAPPER */
	maximumPossibleDeviceCount += outputDeviceCount + 1;

    if (maximumPossibleDeviceCount >= MAX_HARDWARE)
    {
	pj_assert(!"Too many hardware found");
	PJ_LOG(3,(THIS_FILE, "Too many hardware found, "
		  "some devices will not be listed"));
    }

    if (inputDeviceCount > 0)
    {
	/* -1 is the WAVE_MAPPER */
	for (i = -1; i < inputDeviceCount && dev_count < MAX_HARDWARE; ++i)
	{
	    UINT uDeviceID = (UINT)((i==-1) ? WAVE_MAPPER : i);
	    WAVEINCAPS wic;
	    MMRESULT mr;

	    pj_bzero(&wic, sizeof(WAVEINCAPS));

	    mr = waveInGetDevCaps(uDeviceID, &wic, sizeof(WAVEINCAPS));

	    if (mr == MMSYSERR_NOMEM)
		return PJ_ENOMEM;

	    if (mr != MMSYSERR_NOERROR)
		continue;

#ifdef UNICODE
	    WideCharToMultiByte(CP_ACP, 0, wic.szPname, wcslen(wic.szPname), 
				dev_info[dev_count].info.name, 64, NULL, NULL);
#else
	    strncpy(dev_info[dev_count].info.name, wic.szPname, MAXPNAMELEN);
#endif
	    if (uDeviceID == WAVE_MAPPER)
		strcat(dev_info[dev_count].info.name, " - Input");

	    dev_info[dev_count].info.input_count = wic.wChannels;
	    dev_info[dev_count].info.output_count = 0;
	    dev_info[dev_count].info.default_samples_per_sec = 44100;
	    dev_info[dev_count].deviceId = uDeviceID;

	    /* Sometimes a device can return a rediculously large number of 
	     * channels. This happened with an SBLive card on a Windows ME box.
	     * It also happens on Win XP!
	     */
	    if ((dev_info[dev_count].info.input_count < 1) || 
		(dev_info[dev_count].info.input_count > 256))
		dev_info[dev_count].info.input_count = 2;

	    ++dev_count;
	}
    }

    if( outputDeviceCount > 0 )
    {
	/* -1 is the WAVE_MAPPER */
	for (i = -1; i < outputDeviceCount && dev_count < MAX_HARDWARE; ++i)
	{
	    UINT uDeviceID = (UINT)((i==-1) ? WAVE_MAPPER : i);
	    WAVEOUTCAPS woc;
	    MMRESULT mr;

	    pj_bzero(&woc, sizeof(WAVEOUTCAPS));

	    mr = waveOutGetDevCaps(uDeviceID, &woc, sizeof(WAVEOUTCAPS));

	    if (mr == MMSYSERR_NOMEM)
		return PJ_ENOMEM;

	    if (mr != MMSYSERR_NOERROR)
		continue;

#ifdef UNICODE
	    WideCharToMultiByte(CP_ACP, 0, woc.szPname, wcslen(woc.szPname),
				dev_info[dev_count].info.name, 64, NULL, NULL);
#else
	    strncpy(dev_info[dev_count].info.name, woc.szPname, MAXPNAMELEN);
#endif
	    if (uDeviceID == WAVE_MAPPER)
		strcat(dev_info[dev_count].info.name, " - Output");

	    dev_info[dev_count].info.output_count = woc.wChannels;
	    dev_info[dev_count].info.input_count = 0;
	    dev_info[dev_count].deviceId = uDeviceID;
	    /* TODO: Perform a search! */
	    dev_info[dev_count].info.default_samples_per_sec = 44100;

	    /* Sometimes a device can return a rediculously large number of channels.
	     * This happened with an SBLive card on a Windows ME box.
	     * It also happens on Win XP!
	     */
	    if ((dev_info[dev_count].info.output_count < 1) || 
		(dev_info[dev_count].info.output_count > 256))
		dev_info[dev_count].info.output_count = 2;

	    ++dev_count;
	}
    }

    PJ_LOG(4, (THIS_FILE, "WMME initialized, found %d devices:", dev_count));
    for (c = 0; c < dev_count; ++c)
    {
	PJ_LOG(4, (THIS_FILE, " dev_id %d: %s  (in=%d, out=%d)", 
	    c,
	    dev_info[c].info.name,
	    dev_info[c].info.input_count,
	    dev_info[c].info.output_count));
    }
    return PJ_SUCCESS;
}

/*
 * Deinitialize sound library.
 */
PJ_DEF(pj_status_t) pjmedia_snd_deinit(void)
{
    snd_initialized = PJ_FALSE;
    return PJ_SUCCESS;
}

/*
 * Get device count.
 */
PJ_DEF(int) pjmedia_snd_get_dev_count(void)
{
    return dev_count;
}

/*
 * Get device info.
 */
PJ_DEF(const pjmedia_snd_dev_info*) pjmedia_snd_get_dev_info(unsigned index)
{
    if (index == (unsigned)-1) 
	index = 0;

    PJ_ASSERT_RETURN(index < dev_count, NULL);

    return &dev_info[index].info;
}


/*
 * Open stream.
 */
static pj_status_t open_stream(pjmedia_dir dir,
			       int rec_id,
			       int play_id,
			       unsigned clock_rate,
			       unsigned channel_count,
			       unsigned samples_per_frame,
			       unsigned bits_per_sample,
			       pjmedia_snd_rec_cb rec_cb,
			       pjmedia_snd_play_cb play_cb,
			       void *user_data,
			       pjmedia_snd_stream **p_snd_strm)
{
    pj_pool_t *pool;
    pjmedia_snd_stream *strm;
    pj_status_t status;


    /* Make sure sound subsystem has been initialized with
     * pjmedia_snd_init()
     */
    PJ_ASSERT_RETURN(pool_factory != NULL, PJ_EINVALIDOP);


    /* Can only support 16bits per sample */
    PJ_ASSERT_RETURN(bits_per_sample == BITS_PER_SAMPLE, PJ_EINVAL);

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(pool_factory, "wmme-dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = pj_pool_zalloc(pool, sizeof(pjmedia_snd_stream));
    strm->dir = dir;
    strm->play_id = play_id;
    strm->rec_id = rec_id;
    strm->pool = pool;
    strm->rec_cb = rec_cb;
    strm->play_cb = play_cb;
    strm->user_data = user_data;
    strm->clock_rate = clock_rate;
    strm->samples_per_frame = samples_per_frame;
    strm->bits_per_sample = bits_per_sample;
    strm->channel_count = channel_count;
    strm->buffer = pj_pool_alloc(pool, samples_per_frame * BYTES_PER_SAMPLE);
    if (!strm->buffer)
    {
	pj_pool_release(pool);
	return PJ_ENOMEM;
    }

    /* Create player stream */
    if (dir & PJMEDIA_DIR_PLAYBACK)
    {
	unsigned buf_count;

	buf_count = snd_output_latency * clock_rate * channel_count / 
		    samples_per_frame / 1000;

	status = init_player_stream(strm->pool,
				    &strm->play_strm,
				    play_id,
				    clock_rate,
				    channel_count,
				    samples_per_frame,
				    buf_count);

	if (status != PJ_SUCCESS)
	{
	    pjmedia_snd_stream_close(strm);
	    return status;
	}
    }

    /* Create capture stream */
    if (dir & PJMEDIA_DIR_CAPTURE)
    {
	unsigned buf_count;

	buf_count = snd_input_latency * clock_rate * channel_count / 
		    samples_per_frame / 1000;

	status = init_capture_stream(strm->pool,
				     &strm->rec_strm,
				     rec_id,
				     clock_rate,
				     channel_count,
				     samples_per_frame,
				     buf_count);

	if (status != PJ_SUCCESS)
	{
	    pjmedia_snd_stream_close(strm);
	    return status;
	}
    }

    /* Create the stop event */
    strm->thread_quit_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (strm->thread_quit_event == NULL)
	return pj_get_os_error();

    /* Create and start the thread */
    status = pj_thread_create(pool, "wmme", &wmme_dev_thread, strm, 0, 0, 
			      &strm->thread);
    if (status != PJ_SUCCESS)
    {
	pjmedia_snd_stream_close(strm);
	return status;
    }

    *p_snd_strm = strm;

    return PJ_SUCCESS;
}

/*
 * Open stream.
 */
PJ_DEF(pj_status_t) pjmedia_snd_open_rec(int index,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned bits_per_sample,
					 pjmedia_snd_rec_cb rec_cb,
					 void *user_data,
					 pjmedia_snd_stream **p_snd_strm)
{
    PJ_ASSERT_RETURN(rec_cb && p_snd_strm, PJ_EINVAL);

    return open_stream( PJMEDIA_DIR_CAPTURE,
			index,
			-1,
			clock_rate,
			channel_count,
			samples_per_frame,
			bits_per_sample,
			rec_cb,
			NULL,
			user_data,
			p_snd_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_open_player(int index,
					    unsigned clock_rate,
					    unsigned channel_count,
					    unsigned samples_per_frame,
					    unsigned bits_per_sample,
					    pjmedia_snd_play_cb play_cb,
					    void *user_data,
					    pjmedia_snd_stream **p_snd_strm)
{
    PJ_ASSERT_RETURN(play_cb && p_snd_strm, PJ_EINVAL);

    return open_stream( PJMEDIA_DIR_PLAYBACK,
			-1,
			index,
			clock_rate,
			channel_count,
			samples_per_frame,
			bits_per_sample,
			NULL,
			play_cb,
			user_data,
			p_snd_strm);
}

/*
 * Open both player and recorder.
 */
PJ_DEF(pj_status_t) pjmedia_snd_open(int rec_id,
				     int play_id,
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned bits_per_sample,
				     pjmedia_snd_rec_cb rec_cb,
				     pjmedia_snd_play_cb play_cb,
				     void *user_data,
				     pjmedia_snd_stream **p_snd_strm)
{
    PJ_ASSERT_RETURN(rec_cb && play_cb && p_snd_strm, PJ_EINVAL);

    return open_stream( PJMEDIA_DIR_CAPTURE_PLAYBACK,
			rec_id,
			play_id,
			clock_rate,
			channel_count,
			samples_per_frame,
			bits_per_sample,
			rec_cb,
			play_cb,
			user_data,
			p_snd_strm);
}

/*
 * Get stream info.
 */
PJ_DEF(pj_status_t) pjmedia_snd_stream_get_info(pjmedia_snd_stream *strm, 
						pjmedia_snd_stream_info *pi)
{
    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_bzero(pi, sizeof(*pi));
    pi->dir = strm->dir;
    pi->play_id = strm->play_id;
    pi->rec_id = strm->rec_id;
    pi->clock_rate = strm->clock_rate;
    pi->channel_count = strm->channel_count;
    pi->samples_per_frame = strm->samples_per_frame;
    pi->bits_per_sample = strm->bits_per_sample;
    pi->rec_latency = snd_input_latency * strm->clock_rate * 
		      strm->channel_count / 1000;
    pi->play_latency = snd_output_latency * strm->clock_rate * 
		       strm->channel_count / 1000;

    return PJ_SUCCESS;
}


/*
* Start stream.
*/
PJ_DEF(pj_status_t) pjmedia_snd_stream_start(pjmedia_snd_stream *stream)
{
    MMRESULT mr;

    PJ_UNUSED_ARG(stream);

    if (stream->play_strm.hWave.Out != NULL)
    {
	mr = waveOutRestart(stream->play_strm.hWave.Out);
	if (mr != MMSYSERR_NOERROR)
	    /* TODO: This macro is supposed to be used for HRESULT, fix. */
	    PJ_RETURN_OS_ERROR(mr);
	PJ_LOG(5,(THIS_FILE, "WMME playback stream started"));
    }

    if (stream->rec_strm.hWave.In != NULL)
    {
	mr = waveInStart(stream->rec_strm.hWave.In);
	if (mr != MMSYSERR_NOERROR)
	    /* TODO: This macro is supposed to be used for HRESULT, fix. */
	    PJ_RETURN_OS_ERROR(mr);
	PJ_LOG(5,(THIS_FILE, "WMME capture stream started"));
    }

    return PJ_SUCCESS;
}

/*
 * Stop stream.
 */
PJ_DEF(pj_status_t) pjmedia_snd_stream_stop(pjmedia_snd_stream *stream)
{
    MMRESULT mr;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    if (stream->play_strm.hWave.Out != NULL)
    {
	mr = waveOutPause(stream->play_strm.hWave.Out);
	if (mr != MMSYSERR_NOERROR)
	    /* TODO: This macro is supposed to be used for HRESULT, fix. */
	    PJ_RETURN_OS_ERROR(mr);
	PJ_LOG(5,(THIS_FILE, "Stopped WMME playback stream"));
    }

    if (stream->rec_strm.hWave.In != NULL)
    {
	mr = waveInStop(stream->rec_strm.hWave.In);
	if (mr != MMSYSERR_NOERROR)
	    /* TODO: This macro is supposed to be used for HRESULT, fix. */
	    PJ_RETURN_OS_ERROR(mr);
	PJ_LOG(5,(THIS_FILE, "Stopped WMME capture stream"));
    }

    return PJ_SUCCESS;
}


/*
 * Destroy stream.
 */
PJ_DEF(pj_status_t) pjmedia_snd_stream_close(pjmedia_snd_stream *stream)
{
    unsigned i;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    pjmedia_snd_stream_stop(stream);

    if (stream->thread)
    {
	SetEvent(stream->thread_quit_event);
	pj_thread_join(stream->thread);
	pj_thread_destroy(stream->thread);
	stream->thread = NULL;
    }

    /* Unprepare the headers and close the play device */
    if (stream->play_strm.hWave.Out)
    {
	waveOutReset(stream->play_strm.hWave.Out);
	for (i = 0; i < stream->play_strm.dwMaxBufIdx; ++i)
	    waveOutUnprepareHeader(stream->play_strm.hWave.Out, 
				   &(stream->play_strm.WaveHdr[i]),
				   sizeof(WAVEHDR));
	waveOutClose(stream->play_strm.hWave.Out);
	stream->play_strm.hWave.Out = NULL;
    }

    /* Close the play event */
    if (stream->play_strm.hEvent)
    {
	CloseHandle(stream->play_strm.hEvent);
	stream->play_strm.hEvent = NULL;
    }

    /* Unprepare the headers and close the record device */
    if (stream->rec_strm.hWave.In)
    {
	waveInReset(stream->rec_strm.hWave.In);
	for (i = 0; i < stream->play_strm.dwMaxBufIdx; ++i)
	    waveInUnprepareHeader(stream->rec_strm.hWave.In, 
				  &(stream->rec_strm.WaveHdr[i]),
				  sizeof(WAVEHDR));
	waveInClose(stream->rec_strm.hWave.In);
	stream->rec_strm.hWave.In = NULL;
    }

    /* Close the record event */
    if (stream->rec_strm.hEvent)
    {
	CloseHandle(stream->rec_strm.hEvent);
	stream->rec_strm.hEvent = NULL;
    }

    pj_pool_release(stream->pool);

    return PJ_SUCCESS;
}

/*
 * Set sound latency.
 */
PJ_DEF(pj_status_t) pjmedia_snd_set_latency(unsigned input_latency, 
					    unsigned output_latency)
{
    snd_input_latency  = (input_latency == 0)? 
			  PJMEDIA_SND_DEFAULT_REC_LATENCY : input_latency;
    snd_output_latency = (output_latency == 0)? 
			 PJMEDIA_SND_DEFAULT_PLAY_LATENCY : output_latency;

    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_SOUND_IMPLEMENTATION */

