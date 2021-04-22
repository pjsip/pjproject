/* 
 * Copyright (C) 2011-2021 Teluu Inc. (http://www.teluu.com)
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

#include <pjmedia/echo.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#if defined(PJMEDIA_HAS_WEBRTC_AEC) && PJMEDIA_HAS_WEBRTC_AEC != 0

#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/audio_buffer.h"

using namespace webrtc;

#include "echo_internal.h"

#define THIS_FILE		"echo_webrtc.cpp"

typedef struct webrtc_ec
{
    EchoCanceller3  *aec;
    AudioBuffer     *cap_buf;
    AudioBuffer     *rend_buf;

    unsigned    options;
    unsigned	samples_per_frame;
    unsigned    clock_rate;
    unsigned	channel_count;
    unsigned	frame_length;
    unsigned	num_bands;
} webrtc_ec;


/*
 * Create the AEC.
 */
PJ_DEF(pj_status_t) webrtc_aec_create(pj_pool_t *pool,
                                      unsigned clock_rate,
                                      unsigned channel_count,
                                      unsigned samples_per_frame,
                                      unsigned tail_ms,
                                      unsigned options,
                                      void **p_echo )
{
    webrtc_ec *echo;
   
    *p_echo = NULL;
    
    echo = PJ_POOL_ZALLOC_T(pool, webrtc_ec);
    PJ_ASSERT_RETURN(echo != NULL, PJ_ENOMEM);
    
    if (clock_rate != 16000 && clock_rate != 32000 && clock_rate != 48000) {
    	PJ_LOG(3, (THIS_FILE, "Unsupported clock rate for WebRTC AEC3"));
    	return PJ_ENOTSUP;
    }
    
    echo->options = options;    
    echo->channel_count = channel_count;
    echo->samples_per_frame = samples_per_frame;
    echo->clock_rate = clock_rate;
    echo->frame_length = clock_rate/100;
    echo->num_bands = clock_rate/16000;
    
    echo->aec = new EchoCanceller3(EchoCanceller3Config(), clock_rate,
    				   channel_count, channel_count);
    
    echo->cap_buf = new AudioBuffer(clock_rate, channel_count, clock_rate,
                        	    channel_count, clock_rate, channel_count);
    echo->rend_buf = new AudioBuffer(clock_rate, channel_count, clock_rate,
                       		     channel_count, clock_rate, channel_count);

/*
    if (options & PJMEDIA_ECHO_USE_NOISE_SUPPRESSOR) {
        echo->NS_inst = WebRtcNs_Create();
        if (echo->NS_inst) {
            status = WebRtcNs_Init(echo->NS_inst, clock_rate);
            if (status != 0) {
                WebRtcNs_Free(echo->NS_inst);
                echo->NS_inst = NULL;
            }
        }
        if (!echo->NS_inst) {
            PJ_LOG(3, (THIS_FILE, "Unable to create WebRTC noise suppressor"));
        }
    }

#if PJMEDIA_WEBRTC_AEC_USE_MOBILE
    PJ_LOG(3, (THIS_FILE, "WebRTC AEC mobile successfully created with "
			  "options %d", options));
#else
    PJ_LOG(3, (THIS_FILE, "WebRTC AEC successfully created with "
			  "options %d", options));
#endif
*/

    /* Done */
    *p_echo = echo;
    return PJ_SUCCESS;
}


/*
 * Destroy AEC
 */
PJ_DEF(pj_status_t) webrtc_aec_destroy(void *state )
{
    webrtc_ec *echo = (webrtc_ec*) state;
    PJ_ASSERT_RETURN(echo, PJ_EINVAL);
    
    if (echo->aec) {
    	delete echo->aec;
    	echo->aec = NULL;
    }
    
    if (echo->cap_buf) {
    	delete echo->cap_buf;
    	echo->cap_buf = NULL;
    }    
    if (echo->rend_buf) {
    	delete echo->rend_buf;
    	echo->rend_buf = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Reset AEC
 */
PJ_DEF(void) webrtc_aec_reset(void *state )
{
    webrtc_ec *echo = (webrtc_ec*) state;
    
    pj_assert(echo != NULL);
    
    PJ_LOG(4, (THIS_FILE, "WebRTC AEC reset succeeded"));
}


/*
 * Perform echo cancellation.
 */
PJ_DEF(pj_status_t) webrtc_aec_cancel_echo( void *state,
					    pj_int16_t *rec_frm,
					    const pj_int16_t *play_frm,
					    unsigned options,
					    void *reserved )
{
    webrtc_ec *echo = (webrtc_ec*) state;
    unsigned i;

    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(reserved);

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && rec_frm && play_frm, PJ_EINVAL);
    

    for (i = 0; i < echo->samples_per_frame;
    	 i += echo->frame_length)
    {
	StreamConfig scfg(echo->clock_rate, echo->channel_count);

    	echo->cap_buf->CopyFrom(rec_frm + i, scfg);
    	echo->rend_buf->CopyFrom(play_frm + i, scfg);

    	if (echo->clock_rate > 16000) {
      	    echo->cap_buf->SplitIntoFrequencyBands();
      	    echo->rend_buf->SplitIntoFrequencyBands();
    	}

    	echo->aec->AnalyzeCapture(echo->cap_buf);
      	echo->aec->AnalyzeRender(echo->rend_buf);
      	
      	echo->aec->ProcessCapture(echo->cap_buf, false);

    	if (echo->clock_rate > 16000) {
      	    echo->cap_buf->MergeFrequencyBands();
	}
      
     	echo->cap_buf->CopyTo(scfg, rec_frm + i);
    }
    
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) webrtc_aec_get_stat(void *state,
					pjmedia_echo_stat *p_stat)
{
    webrtc_ec *echo = (webrtc_ec*) state;
/*
    if (WebRtcAec_GetDelayMetrics(echo->AEC_inst, &p_stat->median,
    				  &p_stat->std, &p_stat->frac_delay) != 0)
    {
        return PJ_EUNKNOWN;
    }
*/
    p_stat->name = "WebRTC AEC";
    p_stat->stat_info.ptr = p_stat->buf_;
    p_stat->stat_info.slen =
        pj_ansi_snprintf(p_stat->buf_, sizeof(p_stat->buf_),
		     	 "WebRTC delay metric: median=%d, std=%d, "
            	     	 "frac of poor delay=%.02f",
            	     	 p_stat->median, p_stat->std, p_stat->frac_delay);

    return PJ_SUCCESS;
}


#endif
