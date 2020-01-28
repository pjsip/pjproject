/* $Id$ */
/* 
 * Copyright (C) 2011-2015 Teluu Inc. (http://www.teluu.com)
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

#include <webrtc/modules/audio_processing/aec/include/echo_cancellation.h>
#include <webrtc/modules/audio_processing/aecm/include/echo_control_mobile.h>
#include <webrtc/modules/audio_processing/aec/aec_core.h>

#include "echo_internal.h"

#define THIS_FILE		"echo_webrtc.c"

#if PJMEDIA_WEBRTC_AEC_USE_MOBILE == 1
    #include <webrtc/modules/audio_processing/ns/include/noise_suppression_x.h>

    #define NsHandle NsxHandle
    #define WebRtcNs_Create WebRtcNsx_Create
    #define WebRtcNs_Init WebRtcNsx_Init
    #define WebRtcNs_Free WebRtcNsx_Free

    #define WebRtcAec_Create WebRtcAecm_Create
    #define WebRtcAec_Init(handle, clock, sclock) WebRtcAecm_Init(handle, clock)
    #define WebRtcAec_Free WebRtcAecm_Free
    #define WebRtcAec_get_error_code WebRtcAecm_get_error_code
    #define WebRtcAec_enable_delay_agnostic(core, enable)
    #define WebRtcAec_set_config WebRtcAecm_set_config
    #define WebRtcAec_BufferFarend WebRtcAecm_BufferFarend
    #define AecConfig AecmConfig
    typedef short sample;
#else
    #include <webrtc/modules/audio_processing/ns/include/noise_suppression.h>

    typedef float sample;

#endif

#define BUF_LEN			160

/* Set this to 0 to disable metrics calculation. */
#define SHOW_DELAY_METRICS	1

typedef struct webrtc_ec
{
    void       *AEC_inst;
    NsHandle   *NS_inst;
    unsigned    options;
    unsigned	samples_per_frame;
    unsigned	tail;
    unsigned    clock_rate;
    unsigned	channel_count;
    unsigned    subframe_len;
    sample      tmp_buf[BUF_LEN];
    sample      tmp_buf2[BUF_LEN];
} webrtc_ec;


static void print_webrtc_aec_error(const char *tag, void *AEC_inst)
{
    unsigned status = WebRtcAec_get_error_code(AEC_inst);
    PJ_LOG(3, (THIS_FILE, "WebRTC AEC error (%s) %d ", tag, status));
}

static void set_config(void *AEC_inst, unsigned options)
{
    unsigned aggr_opt = options & PJMEDIA_ECHO_AGGRESSIVENESS_MASK;
    int status;
    AecConfig aec_config;

#if PJMEDIA_WEBRTC_AEC_USE_MOBILE
    aec_config.echoMode = 3;
    if (aggr_opt == PJMEDIA_ECHO_AGGRESSIVENESS_CONSERVATIVE)
        aec_config.echoMode = 0;
    else if (aggr_opt == PJMEDIA_ECHO_AGGRESSIVENESS_AGGRESSIVE)
   	aec_config.echoMode = 4;
    aec_config.cngMode = AecmTrue;
#else
    
    aec_config.nlpMode = kAecNlpModerate;
    if (aggr_opt == PJMEDIA_ECHO_AGGRESSIVENESS_CONSERVATIVE)
	aec_config.nlpMode = kAecNlpConservative;
    else if (aggr_opt == PJMEDIA_ECHO_AGGRESSIVENESS_AGGRESSIVE)
        aec_config.nlpMode = kAecNlpAggressive;
    else
        aec_config.nlpMode = kAecNlpModerate;
    
    aec_config.skewMode = kAecFalse;
#if SHOW_DELAY_METRICS
    aec_config.metricsMode = kAecTrue;
    aec_config.delay_logging = kAecTrue;
#else
    aec_config.metricsMode = kAecFalse;
    aec_config.delay_logging = kAecFalse;
#endif

#endif

    status = WebRtcAec_set_config(AEC_inst, aec_config);
    if (status != 0) {
        print_webrtc_aec_error("Init config", AEC_inst);
    }
}

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
    int status;
    
    *p_echo = NULL;
    
    echo = PJ_POOL_ZALLOC_T(pool, webrtc_ec);
    PJ_ASSERT_RETURN(echo != NULL, PJ_ENOMEM);
    
    /* Currently we only support mono. */
    if (channel_count != 1) {
    	PJ_LOG(3, (THIS_FILE, "WebRTC AEC doesn't support stereo"));
    	return PJ_ENOTSUP;
    }

    echo->channel_count = channel_count;
    echo->samples_per_frame = samples_per_frame;
    echo->tail = tail_ms;
    echo->clock_rate = clock_rate;
    /* SWB is processed as 160 frame size */
    if (clock_rate > 8000)
        echo->subframe_len = 160;
    else
    	echo->subframe_len = 80;
    echo->options = options;
    
    /* Create WebRTC AEC */
    echo->AEC_inst = WebRtcAec_Create();
    if (!echo->AEC_inst) {
    	return PJ_ENOMEM;
    }
    
    /* Init WebRTC AEC */
    status = WebRtcAec_Init(echo->AEC_inst, clock_rate, clock_rate);
    if (status != 0) {
        print_webrtc_aec_error("Init", echo->AEC_inst);
        WebRtcAec_Free(echo->AEC_inst);
    	return PJ_ENOTSUP;
    }

    /* WebRtc is very dependent on delay calculation, which will be passed
     * to WebRtcAec_Process() below. A poor estimate, even by as little as
     * 40ms, may affect the echo cancellation results greatly.
     * Hence, we need to enable delay-agnostic echo cancellation. This
     * low-level feature relies on internally estimated delays between
     * the process and reverse streams, thus not relying on reported
     * system delays.
     * Still, with the delay agnostic feature, it may take some time (5-10s
     * or more) for the Aec module to learn the optimal delay, thus
     * a good initial estimate is necessary for good EC quality in
     * the beginning of a call.
     */
    WebRtcAec_enable_delay_agnostic(WebRtcAec_aec_core(echo->AEC_inst), 1);
    
    set_config(echo->AEC_inst, options);

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
    
    if (echo->AEC_inst) {
    	WebRtcAec_Free(echo->AEC_inst);
    	echo->AEC_inst = NULL;
    }
    if (echo->NS_inst) {
        WebRtcNs_Free(echo->NS_inst);
        echo->NS_inst = NULL;
    }
    
    return PJ_SUCCESS;
}


/*
 * Reset AEC
 */
PJ_DEF(void) webrtc_aec_reset(void *state )
{
    webrtc_ec *echo = (webrtc_ec*) state;
    int status;
    
    pj_assert(echo != NULL);
    
    /* Re-initialize the EC */
    status = WebRtcAec_Init(echo->AEC_inst, echo->clock_rate, echo->clock_rate);
    if (status != 0) {
        print_webrtc_aec_error("reset", echo->AEC_inst);
        return;
    }
    
    set_config(echo->AEC_inst, echo->options);
    
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
    int status;
    unsigned i, j, frm_idx = 0;
    const sample * buf_ptr;
    sample * out_buf_ptr;

    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(reserved);

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && rec_frm && play_frm, PJ_EINVAL);
    
    for(i = echo->samples_per_frame / echo->subframe_len; i > 0; i--) {
#if PJMEDIA_WEBRTC_AEC_USE_MOBILE
	buf_ptr = &play_frm[frm_idx];
#else
        for (j = 0; j < echo->subframe_len; j++) {
            echo->tmp_buf[j] = rec_frm[frm_idx+j];
            echo->tmp_buf2[j] = play_frm[frm_idx+j];
        }
        buf_ptr = echo->tmp_buf2;
#endif
        
        /* Feed farend buffer */
        status = WebRtcAec_BufferFarend(echo->AEC_inst, buf_ptr,
                                        echo->subframe_len);
        if (status != 0) {
            print_webrtc_aec_error("Buffer farend", echo->AEC_inst);
            return PJ_EUNKNOWN;
        }        
        
	buf_ptr = echo->tmp_buf;
	out_buf_ptr = echo->tmp_buf2;
        if (echo->NS_inst) {
#if PJMEDIA_WEBRTC_AEC_USE_MOBILE
	    buf_ptr = &rec_frm[frm_idx];
            WebRtcNsx_Process(echo->NS_inst, &buf_ptr, echo->channel_count,
            		      &out_buf_ptr);
            buf_ptr = out_buf_ptr;
            out_buf_ptr = echo->tmp_buf;
#else
            WebRtcNs_Analyze(echo->NS_inst, buf_ptr);
#endif
        }
        
        /* Process echo cancellation */
#if PJMEDIA_WEBRTC_AEC_USE_MOBILE
        status = WebRtcAecm_Process(echo->AEC_inst, &rec_frm[frm_idx],
        			    (echo->NS_inst? buf_ptr: NULL),
        			    out_buf_ptr, echo->subframe_len,
        			    echo->tail);
#else
        status = WebRtcAec_Process(echo->AEC_inst, &buf_ptr,
                                   echo->channel_count, &out_buf_ptr,
                                   echo->subframe_len, (int16_t)echo->tail, 0);
#endif
        if (status != 0) {
            print_webrtc_aec_error("Process echo", echo->AEC_inst);
            return PJ_EUNKNOWN;
        }

#if !PJMEDIA_WEBRTC_AEC_USE_MOBILE
    	if (echo->NS_inst) {
            /* Noise suppression */
	    buf_ptr = echo->tmp_buf2;
	    out_buf_ptr = echo->tmp_buf;
            WebRtcNs_Process(echo->NS_inst, &buf_ptr,
                             echo->channel_count, &out_buf_ptr);
    	}
#endif
    
       	for (j = 0; j < echo->subframe_len; j++) {
 	    rec_frm[frm_idx++] = (pj_int16_t)out_buf_ptr[j];
    	}
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) webrtc_aec_get_stat(void *state,
					pjmedia_echo_stat *p_stat)
{
    webrtc_ec *echo = (webrtc_ec*) state;

    if (WebRtcAec_GetDelayMetrics(echo->AEC_inst, &p_stat->median,
    				  &p_stat->std, &p_stat->frac_delay) != 0)
    {
        return PJ_EUNKNOWN;
    }

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
