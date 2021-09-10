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

#if defined(PJMEDIA_HAS_WEBRTC_AEC3) && PJMEDIA_HAS_WEBRTC_AEC3 != 0

#ifdef _MSC_VER
#    pragma warning(disable: 4100)    // Unreferenced parameter
#    pragma warning(disable: 4244)    // Possible loss of data
#endif

#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/ns/noise_suppressor.h"
#include "modules/audio_processing/gain_controller2.h"
#include "modules/audio_processing/audio_buffer.h"

using namespace webrtc;

#include "echo_internal.h"

#define THIS_FILE		"echo_webrtc_aec3.cpp"

typedef struct webrtc_ec
{
    unsigned    options;
    unsigned	samples_per_frame;
    unsigned    clock_rate;
    unsigned	channel_count;
    unsigned	frame_length;
    unsigned	num_bands;

    pj_bool_t 	get_metrics;
    EchoControl::Metrics metrics;

    EchoControl     *aec;
    NoiseSuppressor *ns;
    GainController2 *agc;
    AudioBuffer     *cap_buf;
    AudioBuffer     *rend_buf;
} webrtc_ec;


/*
 * Create the AEC.
 */
PJ_DEF(pj_status_t) webrtc_aec3_create(pj_pool_t *pool,
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

    if (options & PJMEDIA_ECHO_USE_NOISE_SUPPRESSOR) {
	NsConfig cfg;
	/* Valid values are 6, 12, 18, 21 dB */
	cfg.target_level = NsConfig::SuppressionLevel::k12dB;
	echo->ns = new NoiseSuppressor(cfg, clock_rate, channel_count);
    }

    if (options & PJMEDIA_ECHO_USE_GAIN_CONTROLLER) {
	echo->agc = new GainController2();
	echo->agc->Initialize(clock_rate);
	
	AudioProcessing::Config::GainController2 cfg;
	cfg.adaptive_digital.enabled = true;
	if (GainController2::Validate(cfg))
	    echo->agc->ApplyConfig(cfg);
    }

    /* Done */
    *p_echo = echo;
    return PJ_SUCCESS;
}


/*
 * Destroy AEC
 */
PJ_DEF(pj_status_t) webrtc_aec3_destroy(void *state )
{
    webrtc_ec *echo = (webrtc_ec*) state;
    PJ_ASSERT_RETURN(echo, PJ_EINVAL);
    
    if (echo->aec) {
    	delete echo->aec;
    	echo->aec = NULL;
    }
    if (echo->ns) {
    	delete echo->ns;
    	echo->ns = NULL;
    }
    if (echo->agc) {
    	delete echo->agc;
    	echo->agc = NULL;
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
PJ_DEF(void) webrtc_aec3_reset(void *state )
{
    webrtc_ec *echo = (webrtc_ec*) state;
    
    pj_assert(echo != NULL);
    
    PJ_LOG(4, (THIS_FILE, "WebRTC AEC3 reset no-op"));
}


/*
 * Perform echo cancellation.
 */
PJ_DEF(pj_status_t) webrtc_aec3_cancel_echo(void *state,
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
      	
      	if (echo->ns) {
      	    echo->ns->Analyze(*echo->cap_buf);
      	    echo->ns->Process(echo->cap_buf);
      	}
      	
      	echo->aec->ProcessCapture(echo->cap_buf, false);

      	if (echo->agc) {
      	    echo->agc->Process(echo->cap_buf);
      	}

    	if (echo->clock_rate > 16000) {
      	    echo->cap_buf->MergeFrequencyBands();
	}

     	echo->cap_buf->CopyTo(scfg, rec_frm + i);
    }

    if (echo->get_metrics) {
    	echo->metrics = echo->aec->GetMetrics();
    	echo->get_metrics = PJ_FALSE;
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) webrtc_aec3_get_stat(void *state,
					 pjmedia_echo_stat *p_stat)
{
    webrtc_ec *echo = (webrtc_ec*) state;
    unsigned i = 0;

    if (!echo || !echo->aec)
    	return PJ_EINVAL;    

    /* We cannot perform get metrics here since it may cause a race
     * condition with echo cancellation process and crash with:
     * "Check failed: !race_checker.RaceDetected()".
     * (The doc of EchoCanceller3 specifies that "The class is supposed
     * to be used in a non-concurrent manner").
     *
     * So we just do a simple dispatch. Using mutex seems like
     * an overkill here.
     */
    // echo->metrics = echo->aec->GetMetrics();
    echo->get_metrics = PJ_TRUE;
    while (echo->get_metrics && i < 100000) i++;

    p_stat->delay = echo->metrics.delay_ms;
    p_stat->return_loss = echo->metrics.echo_return_loss;
    p_stat->return_loss_enh = echo->metrics.echo_return_loss_enhancement;

    p_stat->name = "WebRTC AEC3";
    p_stat->stat_info.ptr = p_stat->buf_;
    p_stat->stat_info.slen =
        pj_ansi_snprintf(p_stat->buf_, sizeof(p_stat->buf_),
		     	 "WebRTC AEC3 metrics: delay=%d ms, "
            	     	 "return loss=%.02f, return loss enh=%.02f",
            	     	 p_stat->delay, p_stat->return_loss,
            	     	 p_stat->return_loss_enh);

    return PJ_SUCCESS;
}


#endif
