/* $Id$ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pjmedia/sound.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/os.h>

#include <e32msgqueue.h>
#include <sounddevice.h>
#include <APSClientSession.h>

//////////////////////////////////////////////////////////////////////////////
//

#define THIS_FILE	    "symbian_sound_aps.cpp"

#define BYTES_PER_SAMPLE    2
#define POOL_NAME	    "SymbianSoundAps"
#define POOL_SIZE	    512
#define POOL_INC	    512

#if 1
#   define TRACE_(st) PJ_LOG(3, st)
#else
#   define TRACE_(st)
#endif

static pjmedia_snd_dev_info symbian_snd_dev_info = 
{
    "Symbian Sound Device (APS)",
    1,
    1,
    8000
};

/* App UID to open global APS queues to communicate with the APS server. */
extern TPtrC		    APP_UID;

/* Default setting for loudspeaker */
static pj_bool_t act_loudspeaker = PJ_FALSE;

/* Forward declaration of CPjAudioEngine */
class CPjAudioEngine;

/* 
 * PJMEDIA Sound Stream instance 
 */
struct pjmedia_snd_stream
{
    // Pool
    pj_pool_t			*pool;

    // Common settings.
    pjmedia_dir		 	 dir;
    unsigned			 clock_rate;
    unsigned			 channel_count;
    unsigned			 samples_per_frame;

    // Audio engine
    CPjAudioEngine		*engine;
};

static pj_pool_factory *snd_pool_factory;


/*
 * Utility: print sound device error
 */
static void snd_perror(const char *title, TInt rc) 
{
    PJ_LOG(1,(THIS_FILE, "%s (error code=%d)", title, rc));
}
 
//////////////////////////////////////////////////////////////////////////////
//

/**
 * Abstract class for handler of callbacks from APS client.
 */
class MQueueHandlerObserver
{
public:
    virtual void InputStreamInitialized(const TInt aStatus) = 0;
    virtual void OutputStreamInitialized(const TInt aStatus) = 0;
    virtual void NotifyError(const TInt aError) = 0;

    virtual void RecCb(TAPSCommBuffer &buffer) = 0;
    virtual void PlayCb(TAPSCommBuffer &buffer) = 0;
};

/**
 * Handler for communication and data queue.
 */
class CQueueHandler : public CActive
{
public:
    // Types of queue handler
    enum TQueueHandlerType {
        ERecordCommQueue,
        EPlayCommQueue,
        ERecordQueue,
        EPlayQueue
    };

    // The order corresponds to the APS Server state, do not change!
    enum TState {
    	EAPSPlayerInitialize        = 1,
    	EAPSRecorderInitialize      = 2,
    	EAPSPlayData                = 3,
    	EAPSRecordData              = 4,
    	EAPSPlayerInitComplete      = 5,
    	EAPSRecorderInitComplete    = 6
    };

    static CQueueHandler* NewL(MQueueHandlerObserver* aObserver, 
			       RMsgQueue<TAPSCommBuffer>* aQ, 
			       TQueueHandlerType aType)
    {
	CQueueHandler* self = new (ELeave) CQueueHandler(aObserver, aQ, aType);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
    }

    // Destructor
    ~CQueueHandler() { Cancel(); }

    // Start listening queue event
    void Start() {
	iQ->NotifyDataAvailable(iStatus);
	SetActive();
    }

private:
    // Constructor
    CQueueHandler(MQueueHandlerObserver* aObserver, 
		  RMsgQueue<TAPSCommBuffer>* aQ, 
		  TQueueHandlerType aType) 
	: CActive(CActive::EPriorityHigh),
	  iQ(aQ), iObserver(aObserver), iType(aType)
    {
	CActiveScheduler::Add(this);

	// use lower priority for comm queues
	if ((iType == ERecordCommQueue) || (iType == EPlayCommQueue))
	    SetPriority(CActive::EPriorityStandard);
    }

    // Second phase constructor
    void ConstructL() {}

    // Inherited from CActive
    void DoCancel() { iQ->CancelDataAvailable(); }

    void RunL() {
	if (iStatus != KErrNone) {
	    iObserver->NotifyError(iStatus.Int());
	    return;
        }    

	TAPSCommBuffer buffer;
	TInt ret = iQ->Receive(buffer);

	if (ret != KErrNone) {
	    iObserver->NotifyError(ret);
	    return;
	}

	switch (iType) {
	case ERecordQueue:
	    if (buffer.iCommand == EAPSRecordData) {
		iObserver->RecCb(buffer);
	    }
	    break;

	// Callbacks from the APS main thread
	case EPlayCommQueue:
	    switch (buffer.iCommand) {
		case EAPSPlayData:
		    if (buffer.iStatus == KErrUnderflow) {
			iObserver->PlayCb(buffer);
		    }
		    break;
		case EAPSPlayerInitialize:
		    iObserver->NotifyError(buffer.iStatus);
		    break;
		case EAPSPlayerInitComplete:
		    iObserver->OutputStreamInitialized(buffer.iStatus);
		    break;
		case EAPSRecorderInitComplete:
		    iObserver->InputStreamInitialized(buffer.iStatus);
		    break;
		default:
		    iObserver->NotifyError(buffer.iStatus);
		    break;
	    }
	    break;

	// Callbacks from the APS recorder thread
	case ERecordCommQueue:
	    switch (buffer.iCommand) {
		// The APS recorder thread will only report errors
		// through this handler. All other callbacks will be
		// sent from the APS main thread through EPlayCommQueue
		case EAPSRecorderInitialize:
		    if (buffer.iStatus == KErrNone) {
			iObserver->InputStreamInitialized(buffer.iStatus);
			break;
		    }
		case EAPSRecordData:
		    iObserver->NotifyError(buffer.iStatus);
		    break;
		default:
		    break;
	    }
	    break;

	default:
	    break;
        }

        // issue next request
        iQ->NotifyDataAvailable(iStatus);
        SetActive();
    }

    // Data
    RMsgQueue<TAPSCommBuffer>	*iQ;   // (not owned)
    MQueueHandlerObserver	*iObserver; // (not owned)
    TQueueHandlerType            iType;
};


/*
 * Implementation: Symbian Input & Output Stream.
 */
class CPjAudioEngine : public CBase, MQueueHandlerObserver
{
public:
    enum State
    {
	STATE_NULL,
	STATE_READY,
	STATE_STREAMING
    };

    ~CPjAudioEngine();

    static CPjAudioEngine *NewL(pjmedia_snd_stream *parent_strm,
				pjmedia_snd_rec_cb rec_cb,
				pjmedia_snd_play_cb play_cb,
				void *user_data);

    TInt StartL();
    void Stop();

    TInt ActivateSpeaker(TBool active);

private:
    CPjAudioEngine(pjmedia_snd_stream *parent_strm,
		   pjmedia_snd_rec_cb rec_cb,
		   pjmedia_snd_play_cb play_cb,
		   void *user_data);
    void ConstructL();
    
    TInt InitPlayL();
    TInt InitRecL();
    TInt StartStreamL();
    
    // Inherited from MQueueHandlerObserver
    virtual void InputStreamInitialized(const TInt aStatus);
    virtual void OutputStreamInitialized(const TInt aStatus);
    virtual void NotifyError(const TInt aError);

    virtual void RecCb(TAPSCommBuffer &buffer);
    virtual void PlayCb(TAPSCommBuffer &buffer);

    State			 state_;
    pjmedia_snd_stream		*parentStrm_;
    pjmedia_snd_rec_cb		 recCb_;
    pjmedia_snd_play_cb		 playCb_;
    void			*userData_;
    pj_uint32_t			 TsPlay_;
    pj_uint32_t			 TsRec_;

    RAPSSession                  iSession;
    TAPSInitSettings             iSettings;
    RMsgQueue<TAPSCommBuffer>    iReadQ;
    RMsgQueue<TAPSCommBuffer>    iReadCommQ;
    RMsgQueue<TAPSCommBuffer>    iWriteQ;
    RMsgQueue<TAPSCommBuffer>    iWriteCommQ;

    CQueueHandler		*iPlayCommHandler;
    CQueueHandler		*iRecCommHandler;
    CQueueHandler		*iRecHandler;
    
    static pj_uint8_t		 aps_samples_per_frame;
    
    pj_int16_t			*play_buf;
    pj_uint16_t			 play_buf_len;
    pj_uint16_t			 play_buf_start;
    pj_int16_t			*rec_buf;
    pj_uint16_t			 rec_buf_len;
};


pj_uint8_t CPjAudioEngine::aps_samples_per_frame = 0;


CPjAudioEngine* CPjAudioEngine::NewL(pjmedia_snd_stream *parent_strm,
				     pjmedia_snd_rec_cb rec_cb,
				     pjmedia_snd_play_cb play_cb,
				     void *user_data)
{
    CPjAudioEngine* self = new (ELeave) CPjAudioEngine(parent_strm,
						       rec_cb, play_cb,
						       user_data);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
}

CPjAudioEngine::CPjAudioEngine(pjmedia_snd_stream *parent_strm,
			       pjmedia_snd_rec_cb rec_cb,
			       pjmedia_snd_play_cb play_cb,
			       void *user_data) 
      : state_(STATE_NULL),
	parentStrm_(parent_strm),
	recCb_(rec_cb),
	playCb_(play_cb),
	userData_(user_data),
	iPlayCommHandler(0),
	iRecCommHandler(0),
	iRecHandler(0)
{
}

CPjAudioEngine::~CPjAudioEngine()
{
    Stop();

    delete iPlayCommHandler;
    iPlayCommHandler = NULL;
    delete iRecCommHandler;
    iRecCommHandler = NULL;

    iSession.Close();

    if (state_ == STATE_READY) {
	if (parentStrm_->dir != PJMEDIA_DIR_PLAYBACK) {
	    iReadQ.Close();
	    iReadCommQ.Close();
	}
	iWriteQ.Close();
	iWriteCommQ.Close();
    }
}

TInt CPjAudioEngine::InitPlayL()
{
    if (state_ == STATE_STREAMING || state_ == STATE_READY)
	return 0;

    TInt err = iSession.InitializePlayer(iSettings);
    if (err != KErrNone) {
	snd_perror("Failed to initialize player", err);
	return err;
    }

    // Open message queues for the output stream
    TBuf<128> buf2 = iSettings.iGlobal;
    buf2.Append(_L("PlayQueue"));
    TBuf<128> buf3 = iSettings.iGlobal;
    buf3.Append(_L("PlayCommQueue"));

    while (iWriteQ.OpenGlobal(buf2))
	User::After(10);
    while (iWriteCommQ.OpenGlobal(buf3))
	User::After(10);

    // Construct message queue handler
    iPlayCommHandler = CQueueHandler::NewL(this,
					   &iWriteCommQ,
					   CQueueHandler::EPlayCommQueue);

    // Start observing APS callbacks on output stream message queue
    iPlayCommHandler->Start();

    return 0;
}

TInt CPjAudioEngine::InitRecL()
{
    if (state_ == STATE_STREAMING || state_ == STATE_READY)
	return 0;

    // Initialize input stream device
    TInt err = iSession.InitializeRecorder(iSettings);
    if (err != KErrNone) {
	snd_perror("Failed to initialize recorder", err);
	return err;
    }

    TBuf<128> buf1 = iSettings.iGlobal;
    buf1.Append(_L("RecordQueue"));
    TBuf<128> buf4 = iSettings.iGlobal;
    buf4.Append(_L("RecordCommQueue"));

    // Must wait for APS thread to finish creating message queues
    // before we can open and use them.
    while (iReadQ.OpenGlobal(buf1))
	User::After(10);
    while (iReadCommQ.OpenGlobal(buf4))
	User::After(10);

    // Construct message queue handlers
    iRecCommHandler = CQueueHandler::NewL(this,
					  &iReadCommQ,
					  CQueueHandler::ERecordCommQueue);

    // Start observing APS callbacks from on input stream message queue
    iRecCommHandler->Start();
    
    return 0;
}

TInt CPjAudioEngine::StartL()
{
    TInt err = iSession.Connect();
    if (err != KErrNone && err != KErrAlreadyExists)
	return err;

    if (state_ == STATE_READY)
	return StartStreamL();

    // Even if only capturer are opened, playback thread of APS Server need 
    // to be run(?). Since some messages will be delivered via play comm queue.
    return InitPlayL();
}

void CPjAudioEngine::Stop()
{
    iSession.Stop();

    delete iRecHandler;
    iRecHandler = NULL;

    state_ = STATE_READY;
}

void CPjAudioEngine::ConstructL()
{
    iSettings.iFourCC		    = TFourCC(KMCPFourCCIdG711);
    iSettings.iGlobal		    = APP_UID;
    iSettings.iPriority		    = TMdaPriority(100);
    iSettings.iPreference	    = TMdaPriorityPreference(0x05210001);
    iSettings.iSettings.iChannels   = EMMFMono;
    iSettings.iSettings.iSampleRate = EMMFSampleRate8000Hz;
    iSettings.iSettings.iVolume	    = 0;
    
    /* play_buf size is samples per frame of parent stream. */
    play_buf = (pj_int16_t*)pj_pool_alloc(parentStrm_->pool, 
				          parentStrm_->samples_per_frame << 1);
    play_buf_len = 0;
    play_buf_start = 0;
    
    /* rec_buf size is samples per frame of parent stream. */
    rec_buf  = (pj_int16_t*)pj_pool_alloc(parentStrm_->pool, 
					  parentStrm_->samples_per_frame << 1);
    rec_buf_len = 0;
}

TInt CPjAudioEngine::StartStreamL()
{
    if (state_ == STATE_STREAMING)
	return 0;

    iSession.SetCng(EFalse);
    iSession.SetVadMode(EFalse);
    iSession.SetPlc(EFalse);
    iSession.SetEncoderMode(EULawOr30ms);
    iSession.SetDecoderMode(EULawOr30ms);
    iSession.ActivateLoudspeaker(act_loudspeaker);

    // Not only playback
    if (parentStrm_->dir != PJMEDIA_DIR_PLAYBACK) {
	iRecHandler = CQueueHandler::NewL(this, &iReadQ, 
					  CQueueHandler::ERecordQueue);
	iRecHandler->Start();
	iSession.Read();
	TRACE_((THIS_FILE, "APS recorder started"));
    }

    // Not only capture
    if (parentStrm_->dir != PJMEDIA_DIR_CAPTURE) {
	iSession.Write();
	TRACE_((THIS_FILE, "APS player started"));
    }

    state_ = STATE_STREAMING;
    return 0;
}

///////////////////////////////////////////////////////////
// Inherited from MQueueHandlerObserver
//

void CPjAudioEngine::InputStreamInitialized(const TInt aStatus)
{
    TRACE_((THIS_FILE, "InputStreamInitialized %d", aStatus));

    state_ = STATE_READY;
    if (aStatus == KErrNone) {
	StartStreamL();
    }
}

void CPjAudioEngine::OutputStreamInitialized(const TInt aStatus)
{
    TRACE_((THIS_FILE, "OutputStreamInitialized %d", aStatus));

    if (aStatus == KErrNone) {
	if (parentStrm_->dir == PJMEDIA_DIR_PLAYBACK) {
	    state_ = STATE_READY;
	    // Only playback, start directly
	    StartStreamL();
	} else
	    InitRecL();
    }
}

void CPjAudioEngine::NotifyError(const TInt aError)
{
    snd_perror("Error from CQueueHandler", aError);
}

void CPjAudioEngine::RecCb(TAPSCommBuffer &buffer)
{
    pj_assert(buffer.iBuffer[0] == 1 && buffer.iBuffer[1] == 0);

    /* Detect the recorder G.711 frame size, player frame size will follow 
     * this recorder frame size. 
     */
    if (CPjAudioEngine::aps_samples_per_frame == 0) {
	CPjAudioEngine::aps_samples_per_frame = buffer.iBuffer.Length() < 160?
						 80 : 160;
	TRACE_((THIS_FILE, "Detected APS G.711 frame size = %u samples", 
		CPjAudioEngine::aps_samples_per_frame));
    }

    /* Decode APS buffer (coded in G.711) and put the PCM result into rec_buf.
     * Whenever rec_buf is full, call parent stream callback.  
     */ 
    unsigned dec_len = 0;

    while (dec_len < CPjAudioEngine::aps_samples_per_frame) {
	unsigned tmp;

	tmp = PJ_MIN(parentStrm_->samples_per_frame - rec_buf_len, 
		     CPjAudioEngine::aps_samples_per_frame - dec_len);
	pjmedia_ulaw_decode(&rec_buf[rec_buf_len], 
			    buffer.iBuffer.Ptr() + 2 + dec_len, 
			    tmp);
	rec_buf_len += tmp;
	dec_len += tmp;
	
	pj_assert(rec_buf_len <= parentStrm_->samples_per_frame);
	
	if (rec_buf_len == parentStrm_->samples_per_frame) {
	    recCb_(userData_, 0, rec_buf, rec_buf_len << 1);
	    rec_buf_len = 0;
	}
    }
}

void CPjAudioEngine::PlayCb(TAPSCommBuffer &buffer)
{
    buffer.iCommand = CQueueHandler::EAPSPlayData;
    buffer.iStatus = 0;
    buffer.iBuffer.Zero();
    buffer.iBuffer.Append(1);
    buffer.iBuffer.Append(0);

    /* Send 10ms silence frame if frame size hasn't been known. */
    if (CPjAudioEngine::aps_samples_per_frame == 0) {
	pjmedia_zero_samples(play_buf, 80);
	pjmedia_ulaw_encode((pj_uint8_t*)play_buf, play_buf, 80);
	buffer.iBuffer.Append((TUint8*)play_buf, 80);
	iWriteQ.Send(buffer);
	return;
    }
    
    unsigned enc_len = 0;
    
    /* Call parent stream callback to get PCM samples to play,
     * encode the PCM samples into G.711 and put it into APS buffer. 
     */
    while (enc_len < CPjAudioEngine::aps_samples_per_frame) {
	if (play_buf_len == 0) {
	    playCb_(userData_, 0, play_buf, 
		    sizeof(parentStrm_->samples_per_frame<<1));
	    play_buf_len = parentStrm_->samples_per_frame;
	    play_buf_start = 0;
	}
	
	unsigned tmp;
	
	tmp = PJ_MIN(play_buf_len, 
		     CPjAudioEngine::aps_samples_per_frame - enc_len);
	pjmedia_ulaw_encode((pj_uint8_t*)&play_buf[play_buf_start], 
			    &play_buf[play_buf_start], 
			    tmp);
	buffer.iBuffer.Append((TUint8*)&play_buf[play_buf_start], tmp);
	enc_len += tmp;
	play_buf_len -= tmp;
	play_buf_start += tmp;
    }

    iWriteQ.Send(buffer);
}

//
// End of inherited from MQueueHandlerObserver
/////////////////////////////////////////////////////////////


TInt CPjAudioEngine::ActivateSpeaker(TBool active)
{
    if (state_ == STATE_READY || state_ == STATE_STREAMING) {
        iSession.ActivateLoudspeaker(active);
	return KErrNone;
    }
    return KErrNotReady;
}
//////////////////////////////////////////////////////////////////////////////
//


/*
 * Initialize sound subsystem.
 */
PJ_DEF(pj_status_t) pjmedia_snd_init(pj_pool_factory *factory)
{
    snd_pool_factory = factory;
    return PJ_SUCCESS;
}

/*
 * Get device count.
 */
PJ_DEF(int) pjmedia_snd_get_dev_count(void)
{
    /* Always return 1 */
    return 1;
}

/*
 * Get device info.
 */
PJ_DEF(const pjmedia_snd_dev_info*) pjmedia_snd_get_dev_info(unsigned index)
{
    /* Always return the default sound device */
    if (index == (unsigned)-1)
	index = 0;

    PJ_ASSERT_RETURN(index==0, NULL);
    return &symbian_snd_dev_info;
}

static pj_status_t sound_open(pjmedia_dir dir,
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

    PJ_ASSERT_RETURN(p_snd_strm, PJ_EINVAL);
    PJ_ASSERT_RETURN(clock_rate == 8000 && channel_count == 1 && 
		     bits_per_sample == 16, PJ_ENOTSUP);
    PJ_ASSERT_RETURN((dir == PJMEDIA_DIR_CAPTURE_PLAYBACK && rec_cb && play_cb)
		     || (dir == PJMEDIA_DIR_CAPTURE && rec_cb && !play_cb)
		     || (dir == PJMEDIA_DIR_PLAYBACK && !rec_cb && play_cb),
		     PJ_EINVAL);

    pool = pj_pool_create(snd_pool_factory, POOL_NAME, POOL_SIZE, POOL_INC, 
    			  NULL);
    if (!pool)
	return PJ_ENOMEM;

    strm = (pjmedia_snd_stream*) pj_pool_zalloc(pool, 
    						sizeof(pjmedia_snd_stream));
    strm->dir = dir;
    strm->pool = pool;
    strm->clock_rate = clock_rate;
    strm->channel_count = channel_count;
    strm->samples_per_frame = samples_per_frame;

    // Create the audio engine.
    TRAPD(err, strm->engine = CPjAudioEngine::NewL(strm, rec_cb, play_cb, 
						   user_data));
    if (err != KErrNone) {
    	pj_pool_release(pool);	
	return PJ_RETURN_OS_ERROR(err);
    }

    // Done.
    *p_snd_strm = strm;
    return PJ_SUCCESS;
}



/*
 * Open sound recorder stream.
 */
PJ_DEF(pj_status_t) pjmedia_snd_open_rec( int index,
					  unsigned clock_rate,
					  unsigned channel_count,
					  unsigned samples_per_frame,
					  unsigned bits_per_sample,
					  pjmedia_snd_rec_cb rec_cb,
					  void *user_data,
					  pjmedia_snd_stream **p_snd_strm)
{
    if (index < 0) index = 0;
    PJ_ASSERT_RETURN(index == 0, PJ_EINVAL);

    return sound_open(PJMEDIA_DIR_CAPTURE, clock_rate, channel_count, 
		      samples_per_frame, bits_per_sample, rec_cb, NULL,
		      user_data, p_snd_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_open_player( int index,
					unsigned clock_rate,
					unsigned channel_count,
					unsigned samples_per_frame,
					unsigned bits_per_sample,
					pjmedia_snd_play_cb play_cb,
					void *user_data,
					pjmedia_snd_stream **p_snd_strm )
{
    if (index < 0) index = 0;
    PJ_ASSERT_RETURN(index == 0, PJ_EINVAL);

    return sound_open(PJMEDIA_DIR_PLAYBACK, clock_rate, channel_count, 
		      samples_per_frame, bits_per_sample, NULL, play_cb,
		      user_data, p_snd_strm);
}

PJ_DEF(pj_status_t) pjmedia_snd_open( int rec_id,
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
    if (rec_id < 0) rec_id = 0;
    if (play_id < 0) play_id = 0;
    PJ_ASSERT_RETURN(play_id == 0 && rec_id == 0, PJ_EINVAL);

    return sound_open(PJMEDIA_DIR_CAPTURE_PLAYBACK, clock_rate, channel_count, 
		      samples_per_frame, bits_per_sample, rec_cb, play_cb,
		      user_data, p_snd_strm);
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
    pi->play_id = 0;
    pi->rec_id = 0;
    pi->clock_rate = strm->clock_rate;
    pi->channel_count = strm->channel_count;
    pi->samples_per_frame = strm->samples_per_frame;
    pi->bits_per_sample = BYTES_PER_SAMPLE * 8;
    // latencies approximation (in samples)
    pi->rec_latency  = strm->samples_per_frame * 2;
    pi->play_latency = strm->samples_per_frame * 2;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_snd_stream_start(pjmedia_snd_stream *stream)
{
    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);
    
    if (stream->engine) {
	TInt err = stream->engine->StartL();
    	if (err != KErrNone)
    	    return PJ_RETURN_OS_ERROR(err);
    }
    	
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_snd_stream_stop(pjmedia_snd_stream *stream)
{
    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);
    
    if (stream->engine) {
    	stream->engine->Stop();
    }
    
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_snd_stream_close(pjmedia_snd_stream *stream)
{
    pj_pool_t *pool;
    
    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);
    
    if (stream->engine) {
    	delete stream->engine;
    	stream->engine = NULL;
    }

    pool = stream->pool;
    if (pool) {	
    	stream->pool = NULL;
    	pj_pool_release(pool);
    }
    
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_snd_deinit(void)
{
    /* Nothing to do */
    return PJ_SUCCESS;
}


/*
 * Set sound latency.
 */
PJ_DEF(pj_status_t) pjmedia_snd_set_latency(unsigned input_latency, 
					    unsigned output_latency)
{
    /* Nothing to do */
    PJ_UNUSED_ARG(input_latency);
    PJ_UNUSED_ARG(output_latency);
    return PJ_SUCCESS;
}


/*
 * Activate/deactivate loudspeaker.
 */
PJ_DEF(pj_status_t) pjmedia_snd_aps_activate_loudspeaker(
					pjmedia_snd_stream *stream, 
					pj_bool_t active)
{
    if (stream == NULL) {
	act_loudspeaker = active;
    } else {
	if (stream->engine == NULL)
	    return PJ_EINVAL;

	TInt err = stream->engine->ActivateSpeaker(active);
	if (err != KErrNone)
	    return PJ_RETURN_OS_ERROR(err);
    }

    return PJ_SUCCESS;
}
