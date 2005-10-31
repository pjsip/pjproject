/* $Header: /pjproject/pjmedia/src/pjmedia/dsound.c 6     6/14/05 12:54a Bennylp $ */

#ifdef _MSC_VER
//# pragma warning(disable: 4201)   // non-standard extension: nameless struct/union
# pragma warning(push, 3)
#endif
#include <pj/config.h>
#include <pj/os.h>
#include <pj/log.h>

#include <dsound.h>
#include <stdio.h>
#include <assert.h>
#include <pjmedia/sound.h>

#define THIS_FILE   "dsound.c"

/*
 * Constants
 */
#define PACKET_BUFFER_COUNT 4

typedef struct PJ_Direct_Sound_Device PJ_Direct_Sound_Device;


/*
 * DirectSound Factory Operations
 */
static pj_status_t dsound_init(void);
static const char *dsound_get_name(void);
static pj_status_t dsound_destroy(void);
static pj_status_t dsound_enum_devices(int *count, char *dev_names[]);
static pj_status_t dsound_create_dev(const char *dev_name, pj_snd_dev *dev);
static pj_status_t dsound_destroy_dev(pj_snd_dev *dev);


/*
 * DirectSound Device Operations
 */
static pj_status_t dsound_dev_open( pj_snd_dev *dev, pj_snd_role_t role );
static pj_status_t dsound_dev_close( pj_snd_dev *dev );
static pj_status_t dsound_dev_play( pj_snd_dev *dev );
static pj_status_t dsound_dev_record( pj_snd_dev *dev );

/*
 * Utils.
 */
static pj_status_t dsound_destroy_dsound_dev( PJ_Direct_Sound_Device *dsDev );


static pj_snd_dev_factory dsound_factory = 
{
    &dsound_init,
    &dsound_get_name,
    &dsound_destroy,
    &dsound_enum_devices,
    &dsound_create_dev,
    &dsound_destroy_dev
};

static struct pj_snd_dev_op dsound_dev_op = 
{
    &dsound_dev_open,
    &dsound_dev_close,
    &dsound_dev_play,
    &dsound_dev_record
};

#define DSOUND_TYPE_PLAYER	1
#define DSOUND_TYPE_RECORDER	2

typedef struct Direct_Sound_Descriptor
{
    int			type;

    LPDIRECTSOUND	lpDsPlay;
    LPDIRECTSOUNDBUFFER	lpDsPlayBuffer;

    LPDIRECTSOUNDCAPTURE	lpDsCapture;
    LPDIRECTSOUNDCAPTUREBUFFER	lpDsCaptureBuffer;

    LPDIRECTSOUNDNOTIFY	lpDsNotify;
    HANDLE		hEvent;
    HANDLE		hThread;
    HANDLE		hStartEvent;
    DWORD		dwThreadQuitFlag;
    pj_thread_desc	thread_desc;
    pj_thread_t	       *thread;
} Direct_Sound_Descriptor;

struct PJ_Direct_Sound_Device
{
    Direct_Sound_Descriptor playDesc;
    Direct_Sound_Descriptor recDesc;
};

struct Thread_Param
{
    pj_snd_dev	    *dev;
    Direct_Sound_Descriptor *desc;
};

PJ_DEF(pj_snd_dev_factory*) pj_dsound_get_factory()
{
    return &dsound_factory;
}

/*
 * Init DirectSound.
 */
static pj_status_t dsound_init(void)
{
    /* Nothing to do. */
    return 0;
}

/*
 * Get the name of the factory.
 */
static const char *dsound_get_name(void)
{
    return "DirectSound";
}

/*
 * Destroy DirectSound.
 */
static pj_status_t dsound_destroy(void)
{
    /* TODO: clean up devices in case application haven't done it. */
    return 0;
}

/*
 * Enum devices in the system.
 */
static pj_status_t dsound_enum_devices(int *count, char *dev_names[])
{
    dev_names[0] = "DirectSound Default Device";
    *count = 1;
    return 0;
}

/*
 * Create DirectSound device.
 */
static pj_status_t dsound_create_dev(const char *dev_name, pj_snd_dev *dev)
{
    PJ_Direct_Sound_Device *dsDev;

    /* TODO: create based on the name. */
    PJ_TODO(DSOUND_CREATE_DEVICE_BY_NAME);
    
    /* Create DirectSound structure. */
    dsDev = malloc(sizeof(*dsDev));
    if (!dsDev) {
	PJ_LOG(1,(THIS_FILE, "No memory to allocate device!"));
	return -1;
    }
    memset(dsDev, 0, sizeof(*dsDev));

    /* Associate DirectSound device with device. */
    dev->device = dsDev;
    dev->op = &dsound_dev_op;

    return 0;
}

/*
 * Destroy DirectSound device.
 */
static pj_status_t dsound_destroy_dev( pj_snd_dev *dev )
{
    if (dev->device) {
	free(dev->device);
	dev->device = NULL;
    }
    return 0;
}

static void dsound_release_descriptor(Direct_Sound_Descriptor *desc)
{
    if (desc->lpDsNotify)
	IDirectSoundNotify_Release( desc->lpDsNotify );
    
    if (desc->hEvent)
	CloseHandle(desc->hEvent);

    if (desc->lpDsPlayBuffer)
	IDirectSoundBuffer_Release( desc->lpDsPlayBuffer );

    if (desc->lpDsPlay)
	IDirectSound_Release( desc->lpDsPlay );

    if (desc->lpDsCaptureBuffer)
	IDirectSoundCaptureBuffer_Release(desc->lpDsCaptureBuffer);

    if (desc->lpDsCapture)
	IDirectSoundCapture_Release(desc->lpDsCapture);
}

/*
 * Destroy DirectSound resources.
 */
static pj_status_t dsound_destroy_dsound_dev( PJ_Direct_Sound_Device *dsDev )
{
    dsound_release_descriptor( &dsDev->playDesc );
    dsound_release_descriptor( &dsDev->recDesc );
    memset(dsDev, 0, sizeof(*dsDev));
    return 0;
}

static void init_waveformatex (PCMWAVEFORMAT *pcmwf, pj_snd_dev *dev)
{
    memset(pcmwf, 0, sizeof(PCMWAVEFORMAT)); 
    pcmwf->wf.wFormatTag = WAVE_FORMAT_PCM; 
    pcmwf->wf.nChannels = 1;
    pcmwf->wf.nSamplesPerSec = dev->param.samples_per_sec;
    pcmwf->wf.nBlockAlign = dev->param.bytes_per_frame;
    pcmwf->wf.nAvgBytesPerSec = 
	dev->param.samples_per_sec * dev->param.bytes_per_frame;
    pcmwf->wBitsPerSample = dev->param.bits_per_sample;
}

/*
 * Initialize DirectSound player device.
 */
static pj_status_t dsound_init_player (pj_snd_dev *dev)
{
    HRESULT hr;
    HWND hwnd;
    PCMWAVEFORMAT pcmwf; 
    DSBUFFERDESC dsbdesc;
    DSBPOSITIONNOTIFY dsPosNotify[PACKET_BUFFER_COUNT];
    unsigned i;
    PJ_Direct_Sound_Device *dsDev = dev->device;

    /*
     * Check parameters.
     */
    if (dev->play_cb == NULL) {
	assert(0);
	return -1;
    }
    if (dev->device == NULL) {
	assert(0);
	return -1;
    }

    PJ_LOG(4,(THIS_FILE, "Creating DirectSound player device"));

    /*
     * Create DirectSound device.
     */
    hr = DirectSoundCreate(NULL, &dsDev->playDesc.lpDsPlay, NULL);
    if (FAILED(hr))
	goto on_error;

    hwnd = GetForegroundWindow();
    if (hwnd == NULL) {
	hwnd = GetDesktopWindow();
    }    
    hr = IDirectSound_SetCooperativeLevel( dsDev->playDesc.lpDsPlay, hwnd, 
					   DSSCL_PRIORITY);
    if FAILED(hr)
	goto on_error;
    
    /*
     * Create DirectSound play buffer.
     */    
    // Set up wave format structure. 
    init_waveformatex (&pcmwf, dev);

    // Set up DSBUFFERDESC structure. 
    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC)); 
    dsbdesc.dwSize = sizeof(DSBUFFERDESC); 
    dsbdesc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPOSITIONNOTIFY |
		      DSBCAPS_GETCURRENTPOSITION2;

    dsbdesc.dwBufferBytes = 
	(PACKET_BUFFER_COUNT * dev->param.bytes_per_frame * 
	 dev->param.frames_per_packet); 
    dsbdesc.lpwfxFormat = (LPWAVEFORMATEX)&pcmwf; 

    // Create buffer. 
    hr = IDirectSound_CreateSoundBuffer(dsDev->playDesc.lpDsPlay, &dsbdesc, 
					&dsDev->playDesc.lpDsPlayBuffer, NULL); 
    if (FAILED(hr) )
	goto on_error;

    /*
     * Create event for play notification.
     */
    dsDev->playDesc.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL);
    if (dsDev->playDesc.hEvent == NULL)
	goto on_error;

    /*
     * Setup notification for play.
     */
    hr = IDirectSoundBuffer_QueryInterface( dsDev->playDesc.lpDsPlayBuffer, 
					    &IID_IDirectSoundNotify, 
					    (LPVOID *)&dsDev->playDesc.lpDsNotify); 
    if (FAILED(hr))
	goto on_error;

    
    for (i=0; i<PACKET_BUFFER_COUNT; ++i) {
	dsPosNotify[i].dwOffset = i * dev->param.bytes_per_frame * 
				  dev->param.frames_per_packet;
	dsPosNotify[i].hEventNotify = dsDev->playDesc.hEvent;
    }
    
    hr = IDirectSoundNotify_SetNotificationPositions( dsDev->playDesc.lpDsNotify, 
						      PACKET_BUFFER_COUNT, 
						      dsPosNotify);
    if (FAILED(hr))
	goto on_error;

    /* Done setting up play device. */
    PJ_LOG(4,(THIS_FILE, "DirectSound player device created"));

    return 0;

on_error:
    PJ_LOG(2,(THIS_FILE, "Error creating player device, hresult=0x%x", hr));
    dsound_destroy_dsound_dev(dsDev);
    return -1;
}

/*
 * Initialize DirectSound recorder device
 */
static pj_status_t dsound_init_recorder (pj_snd_dev *dev)
{
    HRESULT hr;
    PCMWAVEFORMAT pcmwf; 
    DSCBUFFERDESC dscbdesc;
    DSBPOSITIONNOTIFY dsPosNotify[PACKET_BUFFER_COUNT];
    unsigned i;
    PJ_Direct_Sound_Device *dsDev = dev->device;

    /*
     * Check parameters.
     */
    if (dev->rec_cb == NULL) {
	assert(0);
	return -1;
    }
    if (dev->device == NULL) {
	assert(0);
	return -1;
    }

    PJ_LOG(4,(THIS_FILE, "Creating DirectSound recorder device"));

    /*
     * Creating recorder device.
     */
    hr = DirectSoundCaptureCreate(NULL, &dsDev->recDesc.lpDsCapture, NULL);
    if (FAILED(hr))
	goto on_error;

    /* Init wave format */
    init_waveformatex (&pcmwf, dev);

    /* 
     * Setup capture buffer using sound buffer structure that was passed
     * to play buffer creation earlier.
     */
    memset(&dscbdesc, 0, sizeof(DSCBUFFERDESC));
    dscbdesc.dwSize = sizeof(DSCBUFFERDESC); 
    dscbdesc.dwFlags = DSCBCAPS_WAVEMAPPED ;
    dscbdesc.dwBufferBytes = 
	(PACKET_BUFFER_COUNT * dev->param.bytes_per_frame * 
	 dev->param.frames_per_packet); 
    dscbdesc.lpwfxFormat = (LPWAVEFORMATEX)&pcmwf; 

    hr = IDirectSoundCapture_CreateCaptureBuffer( dsDev->recDesc.lpDsCapture,
						  &dscbdesc, 
						  &dsDev->recDesc.lpDsCaptureBuffer, 
						  NULL);
    if (FAILED(hr))
	goto on_error;

    /*
     * Create event for play notification.
     */
    dsDev->recDesc.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL);
    if (dsDev->recDesc.hEvent == NULL)
	goto on_error;

    /*
     * Setup notifications for recording.
     */
    hr = IDirectSoundCaptureBuffer_QueryInterface( dsDev->recDesc.lpDsCaptureBuffer, 
						   &IID_IDirectSoundNotify, 
						   (LPVOID *)&dsDev->recDesc.lpDsNotify); 
    if (FAILED(hr))
	goto on_error;

    
    for (i=0; i<PACKET_BUFFER_COUNT; ++i) {
	dsPosNotify[i].dwOffset = i * dev->param.bytes_per_frame * 
				  dev->param.frames_per_packet;
	dsPosNotify[i].hEventNotify = dsDev->recDesc.hEvent;
    }
    
    hr = IDirectSoundNotify_SetNotificationPositions( dsDev->recDesc.lpDsNotify, 
						      PACKET_BUFFER_COUNT, 
						      dsPosNotify);
    if (FAILED(hr))
	goto on_error;

    /* Done setting up recorder device. */
    PJ_LOG(4,(THIS_FILE, "DirectSound recorder device created"));

    return 0;

on_error:
    PJ_LOG(4,(THIS_FILE, "Error creating device, hresult=%d", hr));
    dsound_destroy_dsound_dev(dsDev);
    return -1;
}

/*
 * Initialize DirectSound device.
 */
static pj_status_t dsound_dev_open( pj_snd_dev *dev, pj_snd_role_t role )
{
    PJ_Direct_Sound_Device *dsDev = dev->device;
    pj_status_t status;

    dsDev->playDesc.type = DSOUND_TYPE_PLAYER;
    dsDev->recDesc.type = DSOUND_TYPE_RECORDER;

    if (role & PJ_SOUND_PLAYER) {
	status = dsound_init_player (dev);
	if (status != 0)
	    return status;
    }

    if (role & PJ_SOUND_RECORDER) {
	status = dsound_init_recorder (dev);
	if (status != 0)
	    return status;
    }

    return 0;
}

/*
 * Close DirectSound device.
 */
static pj_status_t dsound_dev_close( pj_snd_dev *dev )
{
    PJ_LOG(4,(THIS_FILE, "Closing DirectSound device"));

    if (dev->device) {
	PJ_Direct_Sound_Device *dsDev = dev->device;

	if (dsDev->playDesc.hThread) {
	    PJ_LOG(4,(THIS_FILE, "Stopping DirectSound player"));
	    dsDev->playDesc.dwThreadQuitFlag = 1;
	    SetEvent(dsDev->playDesc.hEvent);
	    if (WaitForSingleObject(dsDev->playDesc.hThread, 1000) != WAIT_OBJECT_0) {
		PJ_LOG(4,(THIS_FILE, "Timed out waiting player thread to quit"));
		TerminateThread(dsDev->playDesc.hThread, -1);
		IDirectSoundBuffer_Stop( dsDev->playDesc.lpDsPlayBuffer );
	    }
	    
	    pj_thread_destroy (dsDev->playDesc.thread);
	}

	if (dsDev->recDesc.hThread) {
	    PJ_LOG(4,(THIS_FILE, "Stopping DirectSound recorder"));
	    dsDev->recDesc.dwThreadQuitFlag = 1;
	    SetEvent(dsDev->recDesc.hEvent);
	    if (WaitForSingleObject(dsDev->recDesc.hThread, 1000) != WAIT_OBJECT_0) {
		PJ_LOG(4,(THIS_FILE, "Timed out waiting recorder thread to quit"));
		TerminateThread(dsDev->recDesc.hThread, -1);
		IDirectSoundCaptureBuffer_Stop( dsDev->recDesc.lpDsCaptureBuffer );
	    }
	    
	    pj_thread_destroy (dsDev->recDesc.thread);
	}

	dsound_destroy_dsound_dev( dev->device );
	dev->op = NULL;
    }

    PJ_LOG(4,(THIS_FILE, "DirectSound device closed"));
    return 0;
}

static BOOL AppReadDataFromBuffer(LPDIRECTSOUNDCAPTUREBUFFER lpDsb, // The buffer.
				  DWORD dwOffset,		    // Our own write cursor.
				  LPBYTE lpbSoundData,		    // Start of our data.
				  DWORD dwSoundBytes)		    // Size of block to copy.
{ 
    LPVOID  lpvPtr1; 
    DWORD dwBytes1; 
    LPVOID  lpvPtr2; 
    DWORD dwBytes2; 
    HRESULT hr; 
    
    // Obtain memory address of write block. This will be in two parts
    // if the block wraps around.
    
    hr = IDirectSoundCaptureBuffer_Lock( lpDsb, dwOffset, dwSoundBytes, &lpvPtr1, 
					 &dwBytes1, &lpvPtr2, &dwBytes2, 0); 
    
    if SUCCEEDED(hr) { 
	// Read from pointers. 
	CopyMemory(lpbSoundData, lpvPtr1, dwBytes1); 
	if (lpvPtr2 != NULL)
	    CopyMemory(lpbSoundData+dwBytes1, lpvPtr2, dwBytes2); 
	
	// Release the data back to DirectSound. 
	hr = IDirectSoundCaptureBuffer_Unlock(lpDsb, lpvPtr1, dwBytes1, lpvPtr2, dwBytes2); 
	if SUCCEEDED(hr)
	    return TRUE; 
    } 
    
    // Lock, Unlock, or Restore failed. 
    return FALSE; 
}


static BOOL AppWriteDataToBuffer(LPDIRECTSOUNDBUFFER lpDsb,  // The buffer.
				 DWORD dwOffset,	      // Our own write cursor.
				 LPBYTE lpbSoundData,	      // Start of our data.
				 DWORD dwSoundBytes)	      // Size of block to copy.
{ 
    LPVOID  lpvPtr1; 
    DWORD dwBytes1; 
    LPVOID  lpvPtr2; 
    DWORD dwBytes2; 
    HRESULT hr; 
    
    // Obtain memory address of write block. This will be in two parts
    // if the block wraps around.
    
    hr = IDirectSoundBuffer_Lock( lpDsb, dwOffset, dwSoundBytes, &lpvPtr1, 
				  &dwBytes1, &lpvPtr2, &dwBytes2, 0); 
    
    // If the buffer was lost, restore and retry lock. 
    if (DSERR_BUFFERLOST == hr) { 
	IDirectSoundBuffer_Restore(lpDsb); 
	hr = IDirectSoundBuffer_Lock( lpDsb, dwOffset, dwSoundBytes, 
				      &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0); 
    } 
    if SUCCEEDED(hr) { 
	CopyMemory(lpvPtr1, lpbSoundData, dwBytes1); 
	if (NULL != lpvPtr2) 
	    CopyMemory(lpvPtr2, lpbSoundData+dwBytes1, dwBytes2); 
	
	hr = IDirectSoundBuffer_Unlock(lpDsb, lpvPtr1, dwBytes1, lpvPtr2, dwBytes2); 
	if SUCCEEDED(hr)
	    return TRUE; 
    } 
    
    return FALSE; 
}


/*
 * Player thread.
 */
static DWORD WINAPI dsound_dev_thread(void *arg)
{
    struct Thread_Param *param = arg;
    pj_snd_dev *dev = param->dev;
    Direct_Sound_Descriptor *desc = param->desc;
    unsigned bytes_per_pkt = dev->param.bytes_per_frame *
			     dev->param.frames_per_packet;
    unsigned samples_per_pkt = dev->param.samples_per_frame *
			       dev->param.frames_per_packet;
    void *buffer = NULL;
    DWORD size;
    pj_status_t status;
    DWORD sample_pos;
    DWORD byte_pos;
    DWORD buffer_size = 
	(PACKET_BUFFER_COUNT * dev->param.bytes_per_frame * 
	 dev->param.frames_per_packet);
    HRESULT hr;

    PJ_LOG(4,(THIS_FILE, "DirectSound thread starting"));

    /* Allocate buffer for sound data */
    buffer = malloc(bytes_per_pkt);
    if (!buffer) {
	PJ_LOG(1,(THIS_FILE, "Unable to allocate packet buffer!"));
	return (DWORD)-1;
    }

    desc->thread = pj_thread_register ("dsound", desc->thread_desc);
    if (desc->thread == NULL)
	return (DWORD)-1;

    /*
     * Start playing or recording!
     */
    if (desc->type == DSOUND_TYPE_PLAYER) {
	hr = IDirectSoundBuffer_SetCurrentPosition( desc->lpDsPlayBuffer, 0);
	if (FAILED(hr)) {
	    PJ_LOG(1,(THIS_FILE, "DirectSound play: SetCurrentPosition() error %d", hr));
	    goto on_error;
	}

	hr = IDirectSoundBuffer_Play(desc->lpDsPlayBuffer, 0, 0, DSBPLAY_LOOPING);
	if (FAILED(hr)) {
	    PJ_LOG(1,(THIS_FILE, "DirectSound: Play() error %d", hr));
	    goto on_error;
	}
    } else {
	hr = IDirectSoundCaptureBuffer_Start(desc->lpDsCaptureBuffer, DSCBSTART_LOOPING );
	if (FAILED(hr)) {
	    PJ_LOG(1,(THIS_FILE, "DirectSound: Record() error %d", hr));
	    goto on_error;
	}
    }

    /*
     * Reset initial positions.
     */
    byte_pos = 0xFFFFFFFF;
    sample_pos = 0;

    /*
     * Wait to get the first notification.
     */
    if (WaitForSingleObject(desc->hEvent, 100) != WAIT_OBJECT_0) {
	PJ_LOG(1,(THIS_FILE, "DirectSound: error getting notification"));
	goto on_error;
    }

	
    /* Get initial byte position. */
    if (desc->type == DSOUND_TYPE_PLAYER) {
	hr = IDirectSoundBuffer_GetCurrentPosition( desc->lpDsPlayBuffer, 
						    NULL, &byte_pos );
	if (FAILED(hr)) {
	    PJ_LOG(1,(THIS_FILE, "DirectSound: unable to get "
				 "position, err %d", hr));
	    goto on_error;
	}
    } else {
	hr = IDirectSoundCaptureBuffer_GetCurrentPosition( desc->lpDsCaptureBuffer, 
							   NULL, &byte_pos );
	if (FAILED(hr)) {
	    PJ_LOG(1,(THIS_FILE, "DirectSound: unable to get "
				 "position, err %d", hr));
	    goto on_error;
	}
    }

    /* Signal main thread that we're running. */
    assert( desc->hStartEvent );
    SetEvent( desc->hStartEvent );

    /*
     * Loop while not signalled to quit, wait for event object to be signalled
     * by DirectSound play buffer, then request for sound data from the 
     * application and write it to DirectSound buffer.
     */
    do {
	
	/* Call callback to get sound data */
	if (desc->type == DSOUND_TYPE_PLAYER) {
	    size = bytes_per_pkt;
	    status = (*dev->play_cb)(dev, sample_pos, buffer, &size);

	    /* Quit thread on error. */
	    if (status != 0)
		break;

	    /* Write zeroes when we've got nothing from application. */
	    if (size == 0) {
		memset(buffer, 0, bytes_per_pkt);
		size = bytes_per_pkt;
	    }

	    /* Write to DirectSound buffer. */
	    AppWriteDataToBuffer( desc->lpDsPlayBuffer, byte_pos,
				  (LPBYTE)buffer, size);

	} else {
	    /* Capture from DirectSound buffer. */
	    size = bytes_per_pkt;
	    if (AppReadDataFromBuffer( desc->lpDsCaptureBuffer, byte_pos,
				       (LPBYTE)buffer, size)) {

	    } else {
		memset(buffer, 0, size);
	    }

	    /* Call callback */
	    status = (*dev->rec_cb)(dev, sample_pos, buffer, size);

	    /* Quit thread on error. */
	    if (status != 0)
		break;
	}

	/* Increment position. */
	byte_pos += size;
	if (byte_pos >= buffer_size)
	    byte_pos -= buffer_size;
	sample_pos += samples_per_pkt;

	while (WaitForSingleObject(desc->hEvent, 500) != WAIT_OBJECT_0 &&
	       (!desc->dwThreadQuitFlag)) 
	{
	    Sleep(1);
	}
    } while (!desc->dwThreadQuitFlag);


    PJ_LOG(4,(THIS_FILE, "DirectSound: stopping.."));

    free(buffer);
    if (desc->type == DSOUND_TYPE_PLAYER) {
	IDirectSoundBuffer_Stop( desc->lpDsPlayBuffer );
    } else {
	IDirectSoundCaptureBuffer_Stop( desc->lpDsCaptureBuffer );
    }
    return 0;

on_error:
    PJ_LOG(4,(THIS_FILE, "DirectSound play stopping"));

    if (buffer) 
	free(buffer);
    if (desc->type == DSOUND_TYPE_PLAYER) {
	IDirectSoundBuffer_Stop( desc->lpDsPlayBuffer );
    } else {
	IDirectSoundCaptureBuffer_Stop( desc->lpDsCaptureBuffer );
    }
    desc->dwThreadQuitFlag = 1;

    /* Signal main thread that we failed to initialize */
    assert( desc->hStartEvent );
    SetEvent( desc->hStartEvent );
    return -1;
}


/*
 * Generic starter for play/record.
 */
static pj_status_t dsound_dev_play_record( pj_snd_dev *dev,
					   Direct_Sound_Descriptor *desc )
{
    DWORD threadId;
    int op_type = desc->type;
    const char *op_name = (op_type == DSOUND_TYPE_PLAYER) ? "play" : "record";
    struct Thread_Param param;

    PJ_LOG(4,(THIS_FILE, "DirectSound %s()", op_name));

    /*
     * Create event for the thread to signal us that it is starting or
     * quitting during startup.
     */
    desc->hStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (desc->hStartEvent == NULL) {
	PJ_LOG(1,(THIS_FILE, "DirectSound %s: unable to create event", op_name));
	return -1;
    }

    param.dev = dev;
    param.desc = desc;

    /*
     * Create thread to handle feeding up data to player/recorder.
     */
    desc->hThread = NULL;
    desc->dwThreadQuitFlag = 0;
    desc->hThread = CreateThread( NULL, 0, &dsound_dev_thread, &param, 
					    CREATE_SUSPENDED, &threadId);
    if (!desc->hThread) {
	PJ_LOG(1,(THIS_FILE, "DirectSound %s(): unable to create thread", op_name));
	return -1;
    }

    SetThreadPriority( desc->hThread, THREAD_PRIORITY_HIGHEST);

    /*
     * Resume thread.
     */
    if (ResumeThread(desc->hThread) == (DWORD)-1) {
	PJ_LOG(1,(THIS_FILE, "DirectSound %s(): unable to resume thread", op_name));
	goto on_error;
    }

    /*
     * Wait until we've got signal from the thread that it has successfully
     * started, or when it is quitting.
     */
    WaitForSingleObject( desc->hStartEvent, INFINITE);

    /* We can destroy the event now. */
    CloseHandle( desc->hStartEvent );
    desc->hStartEvent = NULL;

    /* Examine thread status. */
    if (desc->dwThreadQuitFlag != 0) {
	/* Thread failed to initialize */
	WaitForSingleObject(desc->hThread, INFINITE);
	CloseHandle(desc->hThread);
	desc->hThread = NULL;
	return -1;
    }

    return 0;

on_error:
    TerminateThread(desc->hThread, -1);
    CloseHandle(desc->hThread);
    desc->hThread = NULL;
    return -1;
}

/*
 * Start playing.
 */
static pj_status_t dsound_dev_play( pj_snd_dev *dev )
{
    PJ_Direct_Sound_Device *dsDev = dev->device;

    assert(dsDev);
    if (!dsDev) {
	assert(0);
	return -1;
    }

    return dsound_dev_play_record( dev, &dsDev->playDesc );
}

/*
 * Start recording.
 */
static pj_status_t dsound_dev_record( pj_snd_dev *dev )
{
    PJ_Direct_Sound_Device *dsDev = dev->device;

    assert(dsDev);
    if (!dsDev) {
	assert(0);
	return -1;
    }

    return dsound_dev_play_record( dev, &dsDev->recDesc );
}

#ifdef _MSC_VER
# pragma warning(pop)
# pragma warning(disable: 4514)	// unreferenced inline function has been removed
#endif
