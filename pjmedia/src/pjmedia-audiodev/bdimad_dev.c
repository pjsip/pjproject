/*******************************************************
 
 bdimad_dev.c

 Author: bdSound Development Team (techsupport@bdsound.com)
 
 Date: 30/10/2012
 Version 1.0.206

 Copyright (c) 2012 bdSound s.r.l. (www.bdsound.com)
 All Rights Reserved.
 
 *******************************************************/

#include <pjmedia-audiodev/audiodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/string.h>
#include <pj/unicode.h>
#include <pjmedia/errno.h>

#if defined(PJMEDIA_AUDIO_DEV_HAS_BDIMAD) && PJMEDIA_AUDIO_DEV_HAS_BDIMAD != 0

#include <math.h>
#include <wchar.h>

/**************************
*           bdIMAD
***************************/
#include <bdimad.h>

/* Maximum supported audio devices */
#define BD_IMAD_MAX_DEV_COUNT               100                                
/* Maximum supported device name */
#define BD_IMAD_MAX_DEV_LENGTH_NAME         256                                
/* Only mono mode */
#define BD_IMAD_MAX_CHANNELS                1                                  
/* Frequency default value (admitted 8000 Hz, 16000 Hz, 32000 Hz and 48000Hz) */
#define BD_IMAD_DEFAULT_FREQ                48000                              
/* Default milliseconds per buffer */
#define BD_IMAD_MSECOND_PER_BUFFER          16                                 
/* Only for supported systems */
#define BD_IMAD_STARTING_INPUT_VOLUME       50                                 
/* Only for supported systems */
#define BD_IMAD_STARTING_OUTPUT_VOLUME      100                                
/* Diagnostic Enable/Disable */
#define BD_IMAD_DIAGNOSTIC                  BD_IMAD_DIAGNOSTIC_DISABLE         

/* Diagnostic folder path */ 
wchar_t * bdImadPjDiagnosticFolderPath   = L"";                                

#define THIS_FILE            "bdimad_dev.c"

/* BD device info */
struct bddev_info
{
    pjmedia_aud_dev_info    info;
    unsigned            deviceId;
};

/* BD factory */
struct bd_factory
{
    pjmedia_aud_dev_factory  base;
    pj_pool_t                *base_pool;
    pj_pool_t                *pool;
    pj_pool_factory          *pf;
    unsigned                 dev_count;
    struct bddev_info        *dev_info;
};

/* Sound stream. */
struct bd_stream
{
    /** Base stream.                    */
    pjmedia_aud_stream   base;                
    /** Settings.                       */
    pjmedia_aud_param    param;               
    /** Memory pool.                    */
    pj_pool_t           *pool;                
					
    /** Capture callback.               */
    pjmedia_aud_rec_cb   rec_cb;              
    /** Playback callback.              */
    pjmedia_aud_play_cb  play_cb;             
    /** Application data.               */
    void                *user_data;           
					
    /** Frame format                    */
    pjmedia_format_id    fmt_id;              
    /** Silence pattern                 */
    pj_uint8_t           silence_char;    
    /** Bytes per frame                 */
    unsigned             bytes_per_frame;
    /** Samples per frame               */
    unsigned		 samples_per_frame;
    /** Channel count	                 */
    int			 channel_count;
					
    /** Extended frame buffer           */
    pjmedia_frame_ext   *xfrm;                
    /** Total ext frm size              */
    unsigned             xfrm_size;           
       
    /** Check running variable          */
    int                  go;                

    /** Timestamp iterator for capture  */
    pj_timestamp         timestampCapture;    
    /** Timestamp iterator for playback */
    pj_timestamp         timestampPlayback;   

    /** bdIMAD current session instance */
    bdIMADpj             bdIMADpjInstance;    
    /** bdIMAD current session settings */
    bdIMADpj_Setting_t  *bdIMADpjSettingsPtr; 
    /** bdIMAD current session warnings */
    bdIMADpj_Warnings_t *bdIMADpjWarningPtr;

    pj_bool_t		 quit_flag;

    pj_bool_t		 rec_thread_exited;
    pj_bool_t		 rec_thread_initialized;
    pj_thread_desc	 rec_thread_desc;
    pj_thread_t		*rec_thread;

    pj_bool_t		 play_thread_exited;
    pj_bool_t		 play_thread_initialized;
    pj_thread_desc	 play_thread_desc;
    pj_thread_t		*play_thread;

    /* Sometime the record callback does not return framesize as configured
     * (e.g: in OSS), while this module must guarantee returning framesize
     * as configured in the creation settings. In this case, we need a buffer 
     * for the recorded samples.
     */
    pj_int16_t		*rec_buf;
    unsigned		 rec_buf_count;

    /* Sometime the player callback does not request framesize as configured
     * (e.g: in Linux OSS) while sound device will always get samples from 
     * the other component as many as configured samples_per_frame. 
     */
    pj_int16_t		*play_buf;
    unsigned		 play_buf_count;
};

/* Prototypes */

// pjmedia_aud_dev_factory_op
static pj_status_t factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t factory_destroy(pjmedia_aud_dev_factory *f);
static unsigned    factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t factory_get_dev_info(pjmedia_aud_dev_factory *f, 
					unsigned index,    
					pjmedia_aud_dev_info *info);
static pj_status_t factory_default_param(pjmedia_aud_dev_factory *f, 
					 unsigned index, 
					 pjmedia_aud_param *param);
static pj_status_t factory_create_stream(pjmedia_aud_dev_factory *f, 
					 const pjmedia_aud_param *param, 
					 pjmedia_aud_rec_cb rec_cb, 
					 pjmedia_aud_play_cb play_cb, 
					 void *user_data, 
					 pjmedia_aud_stream **p_aud_strm);
static pj_status_t factory_refresh(pjmedia_aud_dev_factory *f);

// pjmedia_aud_stream_op
static pj_status_t stream_get_param(pjmedia_aud_stream *strm, 
				    pjmedia_aud_param *param);
static pj_status_t stream_get_cap(pjmedia_aud_stream *strm, 
				  pjmedia_aud_dev_cap cap, 
				  void *value);
static pj_status_t stream_set_cap(pjmedia_aud_stream *strm, 
				  pjmedia_aud_dev_cap cap, 
				  const void *value);
static pj_status_t stream_start(pjmedia_aud_stream *strm);
static pj_status_t stream_stop(pjmedia_aud_stream *strm);
static pj_status_t stream_destroy(pjmedia_aud_stream *strm);

/* End Prototypes */

/* Operations */
static pjmedia_aud_dev_factory_op factory_op =
{
    &factory_init,
    &factory_destroy,
    &factory_get_dev_count,
    &factory_get_dev_info,
    &factory_default_param,
    &factory_create_stream,
    &factory_refresh
};

static pjmedia_aud_stream_op stream_op = 
{
    &stream_get_param,
    &stream_get_cap,
    &stream_set_cap,
    &stream_start,
    &stream_stop,
    &stream_destroy
};

/* End Operations */

/* Utility functions */

char* BD_IMAD_PJ_WCHARtoCHAR(wchar_t *orig)
{
    size_t origsize = wcslen(orig)+1;
    const size_t newsize = origsize*sizeof(wchar_t);
    char *nstring = (char*)calloc(newsize, sizeof(char));
    wcstombs(nstring, orig, newsize);
    return nstring;
}

#ifdef __cplusplus
extern "C" {
#endif
void manage_code(const unsigned char * pCode, unsigned char *pMsg1, 
		 unsigned char * pMsg2);
#ifdef __cplusplus
}
#endif

/* End Utility functions */

/****************************************************************************
* Factory operations
*/

/* Init BDIMAD audio driver */
pjmedia_aud_dev_factory* pjmedia_bdimad_factory(pj_pool_factory *pf)
{
    struct bd_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "BDIMAD_DRIVER", 1000, 1000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct bd_factory);
    f->pf = pf;
    f->base_pool = pool;
    f->base.op = &factory_op;
    f->pool = NULL;
    f->dev_count = 0;
    f->dev_info = NULL;

    return &f->base;
}

/* API: init factory */
static pj_status_t factory_init(pjmedia_aud_dev_factory *f)
{
    pj_status_t ret = factory_refresh(f);
    if (ret != PJ_SUCCESS) return ret;

    PJ_LOG(4, (THIS_FILE, "BDIMAD initialized"));

    return PJ_SUCCESS;
}

/* API: refresh the device list */
static pj_status_t factory_refresh(pjmedia_aud_dev_factory *f)
{    
    struct bd_factory *wf = (struct bd_factory*)f;
    unsigned int i = 0;
    wchar_t *deviceNamep=NULL;
    wchar_t captureDevName[BD_IMAD_MAX_DEV_COUNT][BD_IMAD_MAX_DEV_LENGTH_NAME];
    unsigned int captureDeviceCount = 0;
    wchar_t playbackDevName[BD_IMAD_MAX_DEV_COUNT][BD_IMAD_MAX_DEV_LENGTH_NAME];
    unsigned int playbackDeviceCount = 0;


    if(wf->pool != NULL) {
        pj_pool_release(wf->pool);
        wf->pool = NULL;
    }

    // Enumerate capture sound devices
    while(bdIMADpj_getDeviceName(BD_IMAD_CAPTURE_DEVICES, &deviceNamep) != 
	  BD_PJ_ERROR_IMAD_DEVICE_LIST_EMPTY)
    {
        wcscpy(captureDevName[captureDeviceCount], deviceNamep);
        captureDeviceCount++;
    }

    // Enumerate playback sound devices
    while(bdIMADpj_getDeviceName(BD_IMAD_PLAYBACK_DEVICES, &deviceNamep) != 
	  BD_PJ_ERROR_IMAD_DEVICE_LIST_EMPTY)
    {
        wcscpy(playbackDevName[playbackDeviceCount], deviceNamep);
        playbackDeviceCount++;
    }
    
    // Set devices info
    wf->dev_count = captureDeviceCount + playbackDeviceCount;
    wf->pool = pj_pool_create(wf->pf, "BD_IMAD_DEVICES", 1000, 1000, NULL);
    wf->dev_info = (struct bddev_info*)pj_pool_calloc(wf->pool, wf->dev_count, 
						     sizeof(struct bddev_info));
    
    // Capture device properties
    for(i=0;i<captureDeviceCount;i++) {
        wf->dev_info[i].deviceId = i;
        wf->dev_info[i].info.caps = PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING | 
				    PJMEDIA_AUD_DEV_CAP_EC;
        wf->dev_info[i].info.default_samples_per_sec = BD_IMAD_DEFAULT_FREQ;
        strcpy(wf->dev_info[i].info.driver, "BD_IMAD");
        wf->dev_info[i].info.ext_fmt_cnt = 0;
        wf->dev_info[i].info.input_count = BD_IMAD_MAX_CHANNELS;
        wf->dev_info[i].info.output_count = 0;
        strcpy(wf->dev_info[i].info.name, 
	       BD_IMAD_PJ_WCHARtoCHAR(captureDevName[i]));
        wf->dev_info[i].info.routes = 0;
    }

    // Playback device properties
    for(i=0;i<playbackDeviceCount;i++) {
        wf->dev_info[captureDeviceCount+i].deviceId = captureDeviceCount+i;
        wf->dev_info[captureDeviceCount+i].info.caps = 
				PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
        wf->dev_info[captureDeviceCount+i].info.default_samples_per_sec = 
				BD_IMAD_DEFAULT_FREQ;
        strcpy(wf->dev_info[captureDeviceCount+i].info.driver, "BD_IMAD");
        wf->dev_info[captureDeviceCount+i].info.ext_fmt_cnt = 0;
        wf->dev_info[captureDeviceCount+i].info.input_count = 0;
        wf->dev_info[captureDeviceCount+i].info.output_count = 
				BD_IMAD_MAX_CHANNELS;
        strcpy(wf->dev_info[captureDeviceCount+i].info.name, 
	       BD_IMAD_PJ_WCHARtoCHAR(playbackDevName[i]));
        wf->dev_info[captureDeviceCount+i].info.routes = 0;
    }

    PJ_LOG(4, (THIS_FILE, "BDIMAD found %d devices:", wf->dev_count));
    for(i=0; i<wf->dev_count; i++) {
        PJ_LOG(4,   
	       (THIS_FILE, " dev_id %d: %s  (in=%d, out=%d)", 
               i,
               wf->dev_info[i].info.name,
               wf->dev_info[i].info.input_count,
               wf->dev_info[i].info.output_count));
    }    
    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct bd_factory *wf = (struct bd_factory*)f;
    pj_pool_t *pool = wf->base_pool;

    pj_pool_release(wf->pool);
    wf->base_pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* API: get number of devices */
static unsigned factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    struct bd_factory *wf = (struct bd_factory*)f;
    return wf->dev_count;
}

/* API: get device info */
static pj_status_t factory_get_dev_info(pjmedia_aud_dev_factory *f, 
                                        unsigned index,
                                        pjmedia_aud_dev_info *info)
{
    struct bd_factory *wf = (struct bd_factory*)f;

    PJ_ASSERT_RETURN(index < wf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_memcpy(info, &wf->dev_info[index].info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t factory_default_param(pjmedia_aud_dev_factory *f,
                                         unsigned index,
                                         pjmedia_aud_param *param)
{
    struct bd_factory *wf = (struct bd_factory*)f;
    struct bddev_info *di = &wf->dev_info[index];

    pj_bzero(param, sizeof(*param));
    if (di->info.input_count && di->info.output_count) {
        param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
        param->rec_id = index;
        param->play_id = index;
        param->channel_count = di->info.output_count;
    } else if(di->info.input_count) {
        param->dir = PJMEDIA_DIR_CAPTURE;
        param->rec_id = index;
        param->play_id = PJMEDIA_AUD_INVALID_DEV;
        param->channel_count = di->info.input_count;
    } else if(di->info.output_count) {
        param->dir = PJMEDIA_DIR_PLAYBACK;
        param->play_id = index;
        param->rec_id = PJMEDIA_AUD_INVALID_DEV;
        param->channel_count = di->info.output_count;
    } else {
        return PJMEDIA_EAUD_INVDEV;
    }

    param->bits_per_sample = BD_IMAD_BITS_X_SAMPLE;
    param->clock_rate = di->info.default_samples_per_sec;
    param->flags = di->info.caps;
    param->samples_per_frame = di->info.default_samples_per_sec * 
			       param->channel_count * 
			       BD_IMAD_MSECOND_PER_BUFFER / 
			       1000;    

    if(di->info.caps & PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING) {
        param->input_vol = BD_IMAD_STARTING_INPUT_VOLUME;
    }

    if(di->info.caps & PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) {
        param->output_vol = BD_IMAD_STARTING_OUTPUT_VOLUME;
    }

    if(di->info.caps & PJMEDIA_AUD_DEV_CAP_EC) {
        param->ec_enabled = PJ_TRUE;
    }

    return PJ_SUCCESS;
}

/* callbacks to set data */
void bdimad_CaptureCallback(void *buffer, int samples, void *user_data)
{
    pj_status_t status = PJ_SUCCESS;
    pjmedia_frame frame;
    unsigned nsamples;    

    struct bd_stream *strm = (struct bd_stream*)user_data;    

    if(!strm->go) 
	goto on_break;

    /* Known cases of callback's thread:
     * - The thread may be changed in the middle of a session, e.g: in MacOS 
     *   it happens when plugging/unplugging headphone.
     * - The same thread may be reused in consecutive sessions. The first
     *   session will leave TLS set, but release the TLS data address,
     *   so the second session must re-register the callback's thread.
     */
    if (strm->rec_thread_initialized == 0 || !pj_thread_is_registered()) 
    {
	pj_bzero(strm->rec_thread_desc, sizeof(pj_thread_desc));
	status = pj_thread_register("bd_CaptureCallback", 
				    strm->rec_thread_desc, 
				    &strm->rec_thread);
	if (status != PJ_SUCCESS)
	    goto on_break;

	strm->rec_thread_initialized = 1;
	PJ_LOG(5,(THIS_FILE, "Recorder thread started"));
    }
    
    /* Calculate number of samples we've got */
    nsamples = samples * strm->channel_count + strm->rec_buf_count;

    /*
    RECORD
    */
    if (strm->fmt_id == PJMEDIA_FORMAT_L16) {
	if (nsamples >= strm->samples_per_frame) {
	    /* If buffer is not empty, combine the buffer with the just incoming
	     * samples, then call put_frame.
	     */
	    if (strm->rec_buf_count) {
		unsigned chunk_count = 0;		

		chunk_count = strm->samples_per_frame - strm->rec_buf_count;
		pjmedia_copy_samples(strm->rec_buf + strm->rec_buf_count,
				     (pj_int16_t*)buffer, chunk_count);

		frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
		frame.buf = (void*) strm->rec_buf;
		frame.size = strm->bytes_per_frame;
		frame.timestamp.u64 = strm->timestampCapture.u64;
		frame.bit_info = 0;

		status = (*strm->rec_cb)(strm->user_data, &frame);

		buffer = (pj_int16_t*) buffer + chunk_count;
		nsamples -= strm->samples_per_frame;
		strm->rec_buf_count = 0;
		strm->timestampCapture.u64 += strm->samples_per_frame /
					      strm->channel_count;
	    }

	    /* Give all frames we have */
	    while (nsamples >= strm->samples_per_frame && status == 0) {		
		frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
		frame.buf = (void*) buffer;
		frame.size = strm->bytes_per_frame;
		frame.timestamp.u64 = strm->timestampCapture.u64;
		frame.bit_info = 0;

		status = (*strm->rec_cb)(strm->user_data, &frame);

		buffer = (pj_int16_t*) buffer + strm->samples_per_frame;
		nsamples -= strm->samples_per_frame;
		strm->timestampCapture.u64 += strm->samples_per_frame /
					      strm->channel_count;
	    }

	    /* Store the remaining samples into the buffer */
	    if (nsamples && status == 0) {
		strm->rec_buf_count = nsamples;
		pjmedia_copy_samples(strm->rec_buf, (pj_int16_t*)buffer, 
				     nsamples);
	    }

	} else {
	    /* Not enough samples, let's just store them in the buffer */
	    pjmedia_copy_samples(strm->rec_buf + strm->rec_buf_count,
				 (pj_int16_t*)buffer, 
				 samples * strm->channel_count);
	    strm->rec_buf_count += samples * strm->channel_count;	
	}
    }  else {
        pj_assert(!"Frame type not supported");
    }
    
    strm->timestampCapture.u64 += strm->param.samples_per_frame / 
				  strm->param.channel_count;

    if (status==0)
	return;

on_break:
    strm->rec_thread_exited = 1;    
}

/* callbacks to get data */
int bdimad_PlaybackCallback(void *buffer, int samples, void *user_data)
{
    pj_status_t status = PJ_SUCCESS;
    pjmedia_frame frame;
    struct bd_stream *strm = (struct bd_stream*)user_data;
    unsigned nsamples_req = samples * strm->channel_count;

    if(!strm->go) 
	goto on_break;

    /* Known cases of callback's thread:
     * - The thread may be changed in the middle of a session, e.g: in MacOS 
     *   it happens when plugging/unplugging headphone.
     * - The same thread may be reused in consecutive sessions. The first
     *   session will leave TLS set, but release the TLS data address,
     *   so the second session must re-register the callback's thread.
     */
    if (strm->play_thread_initialized == 0 || !pj_thread_is_registered()) 
    {
	pj_bzero(strm->play_thread_desc, sizeof(pj_thread_desc));
	status = pj_thread_register("bd_PlaybackCallback", 
				    strm->play_thread_desc,
				    &strm->play_thread);
	if (status != PJ_SUCCESS)
	    goto on_break;

	strm->play_thread_initialized = 1;
	PJ_LOG(5,(THIS_FILE, "Player thread started"));
    }

    /*
    PLAY
    */
    if(strm->fmt_id == PJMEDIA_FORMAT_L16) {
	/* Check if any buffered samples */
	if (strm->play_buf_count) {
	    /* samples buffered >= requested by sound device */
	    if (strm->play_buf_count >= nsamples_req) {
		pjmedia_copy_samples((pj_int16_t*)buffer, strm->play_buf, 
				     nsamples_req);
		strm->play_buf_count -= nsamples_req;
		pjmedia_move_samples(strm->play_buf, 
				     strm->play_buf + nsamples_req,
				     strm->play_buf_count);		

		return nsamples_req;
	    }

	    /* samples buffered < requested by sound device */
	    pjmedia_copy_samples((pj_int16_t*)buffer, strm->play_buf, 
				 strm->play_buf_count);
	    nsamples_req -= strm->play_buf_count;
	    buffer = (pj_int16_t*)buffer + strm->play_buf_count;
	    strm->play_buf_count = 0;
	}

	/* Fill output buffer as requested */
	while (nsamples_req && status == 0) {
	    if (nsamples_req >= strm->samples_per_frame) {		
		frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
		frame.buf = buffer;
		frame.size = strm->bytes_per_frame;
		frame.timestamp.u64 = strm->timestampPlayback.u64;
		frame.bit_info = 0;

		status = (*strm->play_cb)(strm->user_data, &frame);
		if (status != PJ_SUCCESS)
		    return 0;

		if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
		    pj_bzero(frame.buf, frame.size);

		nsamples_req -= strm->samples_per_frame;
		buffer = (pj_int16_t*)buffer + strm->samples_per_frame;
	    } else {		
		frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
		frame.buf = strm->play_buf;
		frame.size = strm->bytes_per_frame;
		frame.timestamp.u64 = strm->timestampPlayback.u64;
		frame.bit_info = 0;

		status = (*strm->play_cb)(strm->user_data, &frame);
		if (status != PJ_SUCCESS)
		    return 0;

		if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
		    pj_bzero(frame.buf, frame.size);

		pjmedia_copy_samples((pj_int16_t*)buffer, strm->play_buf, 
				     nsamples_req);
		strm->play_buf_count = strm->samples_per_frame - 
				       nsamples_req;
		pjmedia_move_samples(strm->play_buf, 
				     strm->play_buf+nsamples_req,
				     strm->play_buf_count);
		nsamples_req = 0;
	    }

	    strm->timestampPlayback.u64 += strm->samples_per_frame /
					   strm->channel_count;
	}
    } else {
        pj_assert(!"Frame type not supported");
    }

    if(status != PJ_SUCCESS) {
        return 0;
    }

    strm->timestampPlayback.u64 += strm->param.samples_per_frame / 
				   strm->param.channel_count;

    if (status == 0)
	return samples;

on_break:
    strm->play_thread_exited = 1;
    return 0;
}

/* Internal: Get format name */
static const char *get_fmt_name(pj_uint32_t id)
{
    static char name[8];

    if (id == PJMEDIA_FORMAT_L16)
        return "PCM";
    pj_memcpy(name, &id, 4);
    name[4] = '\0';
    return name;
}

/* Internal: create BD device. */
static pj_status_t init_streams(struct bd_factory *wf,
                                struct bd_stream *strm,
                                const pjmedia_aud_param *prm)
{
    unsigned ptime;
    enum bdIMADpj_Status errorInitAEC;
    wchar_t *deviceNamep=NULL;
    wchar_t captureDevName[BD_IMAD_MAX_DEV_COUNT][BD_IMAD_MAX_DEV_LENGTH_NAME];
    int captureDeviceCount = 0;
    wchar_t playbackDevName[BD_IMAD_MAX_DEV_COUNT][BD_IMAD_MAX_DEV_LENGTH_NAME];
    int playbackDeviceCount = 0;

    PJ_ASSERT_RETURN(prm->play_id < (int)wf->dev_count, PJ_EINVAL);
    PJ_ASSERT_RETURN(prm->rec_id < (int)wf->dev_count, PJ_EINVAL);

    ptime = prm->samples_per_frame * 
	    1000 / 
	    (prm->clock_rate * prm->channel_count);
    strm->bytes_per_frame = (prm->clock_rate * 
			    ((prm->channel_count * prm->bits_per_sample) / 8)) * 
			     ptime / 
			     1000;
    strm->timestampCapture.u64 = 0;
    strm->timestampPlayback.u64 = 0;

    //BD_IMAD_PJ
    bdIMADpj_CreateStructures(&strm->bdIMADpjSettingsPtr, 
			      &strm->bdIMADpjWarningPtr);
    
    strm->bdIMADpjSettingsPtr->FrameSize_ms = ptime;
    strm->bdIMADpjSettingsPtr->DiagnosticEnable = BD_IMAD_DIAGNOSTIC;
    strm->bdIMADpjSettingsPtr->DiagnosticFolderPath = 
					    bdImadPjDiagnosticFolderPath;
    strm->bdIMADpjSettingsPtr->validate = (void *)manage_code;

    if(prm->clock_rate != 8000 && prm->clock_rate != 16000 
	   && prm->clock_rate != 32000 && prm->clock_rate != 48000) {
        PJ_LOG(4, (THIS_FILE, 
		   "BDIMAD support 8000 Hz, 16000 Hz, 32000 Hz and 48000 Hz "
		   "frequency."));
    }
    strm->bdIMADpjSettingsPtr->SamplingFrequency = prm->clock_rate;

    if(prm->channel_count > BD_IMAD_MAX_CHANNELS) {
        PJ_LOG(4, (THIS_FILE, 
		   "BDIMAD doesn't support a number of channels upper than %d.", 
		   BD_IMAD_MAX_CHANNELS));
    }
    
    // Enumerate capture sound devices
    while(bdIMADpj_getDeviceName(BD_IMAD_CAPTURE_DEVICES, &deviceNamep) != 
	  BD_PJ_ERROR_IMAD_DEVICE_LIST_EMPTY)
    {
        wcscpy(captureDevName[captureDeviceCount], deviceNamep);
        captureDeviceCount++;
    }
    strm->bdIMADpjSettingsPtr->CaptureDevice = captureDevName[(int)prm->rec_id];

    // Enumerate playback sound devices
    while(bdIMADpj_getDeviceName(BD_IMAD_PLAYBACK_DEVICES, &deviceNamep) != 
	  BD_PJ_ERROR_IMAD_DEVICE_LIST_EMPTY)
    {
        wcscpy(playbackDevName[playbackDeviceCount], deviceNamep);
        playbackDeviceCount++;
    }
    strm->bdIMADpjSettingsPtr->PlayDevice = 
		    playbackDevName[(int)(prm->play_id-captureDeviceCount)];

    strm->bdIMADpjSettingsPtr->cb_emptyCaptureBuffer = &bdimad_CaptureCallback;
    strm->bdIMADpjSettingsPtr->cb_emptyCaptureBuffer_user_data = (void*)strm;
    strm->bdIMADpjSettingsPtr->cb_fillPlayBackBuffer = &bdimad_PlaybackCallback;
    strm->bdIMADpjSettingsPtr->cb_fillPlayBackBuffer_user_data = (void*)strm;

    if(strm->bdIMADpjInstance != NULL) 
        bdIMADpj_FreeAEC(&strm->bdIMADpjInstance);
    strm->bdIMADpjInstance = NULL;

    errorInitAEC = bdIMADpj_InitAEC(&strm->bdIMADpjInstance, 
				    &strm->bdIMADpjSettingsPtr, 
				    &strm->bdIMADpjWarningPtr);

    {
	int auxInt = (prm->ec_enabled == PJ_TRUE ? 1 : 0);
        bdIMADpj_setParameter(strm->bdIMADpjInstance, 
			      BD_PARAM_IMAD_PJ_AEC_ENABLE, 
			      &auxInt);
        auxInt = 1;
        //Mic control On by default
        bdIMADpj_setParameter(strm->bdIMADpjInstance, 
			      BD_PARAM_IMAD_PJ_MIC_CONTROL_ENABLE, 
			      &auxInt);
    }

    if(errorInitAEC != BD_PJ_OK && 
       errorInitAEC != BD_PJ_WARN_BDIMAD_WARNING_ASSERTED) 
    {
        return PJMEDIA_AUDIODEV_ERRNO_FROM_BDIMAD(errorInitAEC);
    }
    
    return PJ_SUCCESS;
}

/**************************************** 
            API: create stream 
*****************************************/
static pj_status_t stream_stopBDIMAD(pjmedia_aud_stream *s)
{
    struct bd_stream *strm = (struct bd_stream*)s;
    pj_status_t status = PJ_SUCCESS;
    int i, err = 0;

    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);

    strm->go = 0;    
    
    for (i=0; !strm->rec_thread_exited && i<100; ++i)
	pj_thread_sleep(10);
    for (i=0; !strm->play_thread_exited && i<100; ++i)
	pj_thread_sleep(10);

    pj_thread_sleep(1);

    PJ_LOG(5,(THIS_FILE, "Stopping stream.."));

    strm->play_thread_initialized = 0;
    strm->rec_thread_initialized = 0;

    PJ_LOG(5,(THIS_FILE, "Done, status=%d", err));

    return status;
}

static pj_status_t stream_stop(pjmedia_aud_stream *s)
{
    struct bd_stream *strm = (struct bd_stream*)s;

    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);

    if(strm->bdIMADpjInstance != NULL) {
        return stream_stopBDIMAD(s);
    } else {
        return PJMEDIA_EAUD_ERR;
    }
}

static pj_status_t stream_destroyBDIMAD(pjmedia_aud_stream *s)
{
    struct bd_stream *strm = (struct bd_stream*)s;
    int i = 0;

    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);

    stream_stopBDIMAD(s);

    // DeInit BDIMAD
    bdIMADpj_FreeAEC(&strm->bdIMADpjInstance); 
	PJ_LOG(4, (THIS_FILE, "Free AEC"));

    bdIMADpj_FreeStructures(&strm->bdIMADpjSettingsPtr, 
			    &strm->bdIMADpjWarningPtr);
    PJ_LOG(4, (THIS_FILE, "Free AEC Structure"));

    strm->bdIMADpjInstance = NULL;
    strm->bdIMADpjSettingsPtr = NULL;
    strm->bdIMADpjWarningPtr = NULL;    

    strm->quit_flag = 1;
    for (i=0; !strm->rec_thread_exited && i<100; ++i) {
	pj_thread_sleep(1);
    }
    for (i=0; !strm->play_thread_exited && i<100; ++i) {
	pj_thread_sleep(1);
    }

    PJ_LOG(5,(THIS_FILE, "Destroying stream.."));

    pj_pool_release(strm->pool);
    return PJ_SUCCESS;
}

/* API: Destroy stream. */
static pj_status_t stream_destroy(pjmedia_aud_stream *s)
{
    struct bd_stream *strm = (struct bd_stream*)s;

    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);

    if(strm->bdIMADpjInstance != NULL) {
        return stream_destroyBDIMAD(s);
    } else {
        return PJMEDIA_EAUD_ERR;
    }
}

/* API: set capability */
static pj_status_t stream_set_capBDIMAD(pjmedia_aud_stream *s,
					pjmedia_aud_dev_cap cap,
					const void *pval)
{
    struct bd_stream *strm = (struct bd_stream*)s;
    bdIMADpj_Status res = BD_PJ_OK;
    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if(cap == PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) {
        /* Output volume setting */
        float vol = (float)*(unsigned*)pval;

        if(vol > 100.0f) vol = 100.0f;
        if(vol < 0.0f) vol = 0.0f;

        vol = vol / 100.0f;
        res = bdIMADpj_setParameter(strm->bdIMADpjInstance, 
				    BD_PARAM_IMAD_PJ_SPK_VOLUME, &vol);


        if(res == BD_PJ_OK) {
            strm->param.output_vol = *(unsigned*)pval;
            return PJ_SUCCESS;
        } else {
            return PJMEDIA_AUDIODEV_ERRNO_FROM_BDIMAD(res);
        }
    }

    if(cap == PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING) {
        /* Input volume setting */
        float vol = (float)*(unsigned*)pval;

        if(vol > 100.0f) vol = 100.0f;
        if(vol < 0.0f) vol = 0.0f;

        vol = vol / 100.0f;
        res = bdIMADpj_setParameter(strm->bdIMADpjInstance, 
				    BD_PARAM_IMAD_PJ_MIC_VOLUME, &vol);
        if(res == BD_PJ_OK) {
            strm->param.input_vol = *(unsigned*)pval;
            return PJ_SUCCESS;
        } else {
            return PJMEDIA_AUDIODEV_ERRNO_FROM_BDIMAD(res);
        }
    }

    if(cap == PJMEDIA_AUD_DEV_CAP_EC) {
        int aecOnOff = (*(pj_bool_t*)pval == PJ_TRUE ? 1 : 0);

        /* AEC setting */
        res = bdIMADpj_setParameter(strm->bdIMADpjInstance, 
				    BD_PARAM_IMAD_PJ_AEC_ENABLE, 
				    &aecOnOff);
        if(res == BD_PJ_OK) {
            strm->param.ec_enabled = (aecOnOff == 1 ? PJ_TRUE : PJ_FALSE);
            return PJ_SUCCESS;
        } else {
            return PJMEDIA_AUDIODEV_ERRNO_FROM_BDIMAD(res);
        }
    }

    return PJMEDIA_EAUD_INVCAP;
}

static pj_status_t factory_create_streamBDIMAD(pjmedia_aud_dev_factory *f,
                                    const pjmedia_aud_param *param,
                                    pjmedia_aud_rec_cb rec_cb,
                                    pjmedia_aud_play_cb play_cb,
                                    void *user_data,
                                    pjmedia_aud_stream **p_aud_strm)
{
    struct bd_factory *wf = (struct bd_factory*)f;
    pj_pool_t *pool;
    struct bd_stream *strm;
    pj_uint8_t silence_char;
    pj_status_t status;

    switch (param->ext_fmt.id) {
    case PJMEDIA_FORMAT_L16:
	silence_char = '\0';
	break;
    default:
	return PJMEDIA_EAUD_BADFORMAT;
    }

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(wf->pf, "BDIMAD_STREAM", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct bd_stream);
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    strm->rec_cb = rec_cb;
    strm->play_cb = play_cb;
    strm->user_data = user_data;
    strm->fmt_id = (pjmedia_format_id)param->ext_fmt.id;
    strm->silence_char = silence_char;
    strm->channel_count = param->channel_count;
    strm->samples_per_frame = param->samples_per_frame;

    if (param->dir & PJMEDIA_DIR_CAPTURE_PLAYBACK) {
        status = init_streams(wf, strm, param);

        if (status != PJ_SUCCESS) {
            stream_destroyBDIMAD(&strm->base);
            return status;
        }
    } else {
        stream_destroyBDIMAD(&strm->base);
        return PJMEDIA_EAUD_ERR;
    }

    strm->rec_buf = (pj_int16_t*)pj_pool_alloc(pool, 
		    strm->bytes_per_frame);
    if (!strm->rec_buf) {
        pj_pool_release(pool);
        return PJ_ENOMEM;
    }
    strm->rec_buf_count = 0;

    strm->play_buf = (pj_int16_t*)pj_pool_alloc(pool, 
		     strm->bytes_per_frame);
    if (!strm->play_buf) {
        pj_pool_release(pool);
        return PJ_ENOMEM;
    }
    strm->play_buf_count = 0;

    /* Apply the remaining settings */
    if(param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) {
        stream_set_capBDIMAD(&strm->base, 
			     PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, 
			     &param->output_vol);
    }
    if(param->flags & PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING) {
        stream_set_capBDIMAD(&strm->base, 
			     PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING, 
			     &param->input_vol);
    }
    if(param->flags & PJMEDIA_AUD_DEV_CAP_EC) {
        stream_set_capBDIMAD(&strm->base, 
			     PJMEDIA_AUD_DEV_CAP_EC, 
			     &param->ec_enabled);
    }

    strm->base.op = &stream_op;
    *p_aud_strm = &strm->base;

    return PJ_SUCCESS;
}

static pj_status_t factory_create_stream(pjmedia_aud_dev_factory *f,
					 const pjmedia_aud_param *param,
					 pjmedia_aud_rec_cb rec_cb,
					 pjmedia_aud_play_cb play_cb,
					 void *user_data,
					 pjmedia_aud_stream **p_aud_strm)
{
    return factory_create_streamBDIMAD(f, param, rec_cb, 
				       play_cb, user_data, p_aud_strm);
}
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

/* API: Get stream info. */
static pj_status_t stream_get_param(pjmedia_aud_stream *s,
                                    pjmedia_aud_param *pi)
{
    struct bd_stream *strm = (struct bd_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    // Get the output volume setting
    if(stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, 
		      &pi->output_vol) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
    }

    // Get the input volume setting
    if(stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING, 
		      &pi->input_vol) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING;
    }

    // Get the AEC setting
    if(stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_EC, &pi->ec_enabled) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_EC;
    }

    return PJ_SUCCESS;
}

static pj_status_t stream_get_capBDIMAD(pjmedia_aud_stream *s,
                                        pjmedia_aud_dev_cap cap,
                                        void *pval)
{
    struct bd_stream *strm = (struct bd_stream*)s;
    bdIMADpj_Status res = BD_PJ_OK;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if(cap == PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING)
    {
        /* Input volume setting */
        float vol;
        res = bdIMADpj_getParameter(strm->bdIMADpjInstance, 
				    BD_PARAM_IMAD_PJ_MIC_VOLUME, &vol);
        if(res == BD_PJ_OK) {
	    vol = vol * 100;
	    if(vol > 100.0f) vol = 100.0f;
	    if(vol < 0.0f) vol = 0.0f;
	    *(unsigned int *)pval = (unsigned int)vol;
	    return PJ_SUCCESS;
        } else{
            return PJMEDIA_AUDIODEV_ERRNO_FROM_BDIMAD(res);
        }
    } else if(cap == PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) {
        /* Output volume setting */
        float vol;
        res = bdIMADpj_getParameter(strm->bdIMADpjInstance, 
				    BD_PARAM_IMAD_PJ_SPK_VOLUME, &vol);
        if(res == BD_PJ_OK) {
			vol = vol * 100;
            if(vol > 100.0f) vol = 100.0f;
            if(vol < 0.0f) vol = 0.0f;
            *(unsigned int *)pval = (unsigned int)vol;
            return PJ_SUCCESS;
        } else {
            return PJMEDIA_AUDIODEV_ERRNO_FROM_BDIMAD(res);
        }
    }
    else if(cap == PJMEDIA_AUD_DEV_CAP_EC) {
        int aecIsOn;
        res = bdIMADpj_getParameter(strm->bdIMADpjInstance, 
				    BD_PARAM_IMAD_PJ_AEC_ENABLE, &aecIsOn);
        if(res == BD_PJ_OK) {
            *(pj_bool_t*)pval = (aecIsOn == 1 ? PJ_TRUE : PJ_FALSE);
            return PJ_SUCCESS;
        } else {
            return PJMEDIA_AUDIODEV_ERRNO_FROM_BDIMAD(res);
        }
    } else {
        return PJMEDIA_EAUD_INVCAP;
    }
}

static pj_status_t stream_startBDIMAD(pjmedia_aud_stream *s)
{
    struct bd_stream *strm = (struct bd_stream*)s;

    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);

    strm->go = 1;

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t stream_get_cap(pjmedia_aud_stream *s,
                                  pjmedia_aud_dev_cap cap,
                                  void *pval)
{
    struct bd_stream *strm = (struct bd_stream*)s;

    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);

    if(strm->bdIMADpjInstance != NULL) {
        return stream_get_capBDIMAD(s, cap, pval);
    } else {
        return PJMEDIA_EAUD_ERR;
    }
}

/* API: set capability */
static pj_status_t stream_set_cap(pjmedia_aud_stream *s,
                                  pjmedia_aud_dev_cap cap,
                                  const void *pval)
{
    struct bd_stream *strm = (struct bd_stream*)s;

    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);

    if(strm->bdIMADpjInstance != NULL) {
        return stream_set_capBDIMAD(s, cap, pval);
    } else {
        return PJMEDIA_EAUD_ERR;
    }
}

/* API: Start stream. */
static pj_status_t stream_start(pjmedia_aud_stream *s)
{
    struct bd_stream *strm = (struct bd_stream*)s;

    PJ_ASSERT_RETURN(strm != NULL, PJ_EINVAL);

    if(strm->bdIMADpjInstance != NULL) {
        return stream_startBDIMAD(s);
    } else {
        return PJMEDIA_EAUD_ERR;
    }
}

#if defined (_MSC_VER)
#pragma comment ( lib, "bdClientValidation.lib" )
#pragma comment ( lib, "bdIMADpj.lib" )
#endif


#endif    /* PJMEDIA_AUDIO_DEV_HAS_BDIMAD */

