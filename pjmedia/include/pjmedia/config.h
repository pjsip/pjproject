/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJMEDIA_CONFIG_H__
#define __PJMEDIA_CONFIG_H__

/**
 * @file pjmedia/config.h Compile time config
 * @brief Contains some compile time constants.
 */
#include <pj/config.h>

/**
 * @defgroup PJMEDIA_BASE Base Types and Configurations
 */

/**
 * @defgroup PJMEDIA_CONFIG Compile time configuration
 * @ingroup PJMEDIA_BASE
 * @brief Some compile time configuration settings.
 * @{
 */

/*
 * Include config_auto.h if autoconf is used (PJ_AUTOCONF is set)
 */
#if defined(PJ_AUTOCONF)
#   include <pjmedia/config_auto.h>
#endif

/**
 * Initial memory block for media endpoint.
 */
#ifndef PJMEDIA_POOL_LEN_ENDPT
#   define PJMEDIA_POOL_LEN_ENDPT		512
#endif

/**
 * Memory increment for media endpoint.
 */
#ifndef PJMEDIA_POOL_INC_ENDPT
#   define PJMEDIA_POOL_INC_ENDPT		512
#endif

/**
 * Initial memory block for event manager.
 */
#ifndef PJMEDIA_POOL_LEN_EVTMGR
#   define PJMEDIA_POOL_LEN_EVTMGR		500
#endif

/**
 * Memory increment for evnt manager.
 */
#ifndef PJMEDIA_POOL_INC_EVTMGR
#   define PJMEDIA_POOL_INC_EVTMGR		500
#endif

/**
 * Specify whether we prefer to use audio switch board rather than 
 * conference bridge.
 *
 * Audio switch board is a kind of simplified version of conference 
 * bridge, but not really the subset of conference bridge. It has 
 * stricter rules on audio routing among the pjmedia ports and has
 * no audio mixing capability. The power of it is it could work with
 * encoded audio frames where conference brigde couldn't.
 *
 * Default: 0
 */
#ifndef PJMEDIA_CONF_USE_SWITCH_BOARD
#   define PJMEDIA_CONF_USE_SWITCH_BOARD    0
#endif

/**
 * Specify buffer size for audio switch board, in bytes. This buffer will
 * be used for transmitting/receiving audio frame data (and some overheads,
 * i.e: pjmedia_frame structure) among conference ports in the audio
 * switch board. For example, if a port uses PCM format @44100Hz mono
 * and frame time 20ms, the PCM audio data will require 1764 bytes,
 * so with overhead, a safe buffer size will be ~1900 bytes.
 *
 * Default: PJMEDIA_MAX_MTU
 */
#ifndef PJMEDIA_CONF_SWITCH_BOARD_BUF_SIZE
#   define PJMEDIA_CONF_SWITCH_BOARD_BUF_SIZE    PJMEDIA_MAX_MTU
#endif

/**
 * Specify whether the conference bridge uses AGC, an automatic adjustment to
 * avoid dramatic change in the signal level which can cause noise.
 *
 * Default: 1 (enabled)
 */
#ifndef PJMEDIA_CONF_USE_AGC
#   define PJMEDIA_CONF_USE_AGC    	    1
#endif


/*
 * Types of sound stream backends.
 */

/**
 * This macro has been deprecated in releasee 1.1. Please see
 * http://trac.pjsip.org/repos/wiki/Audio_Dev_API for more information.
 */
#if defined(PJMEDIA_SOUND_IMPLEMENTATION)
#   error PJMEDIA_SOUND_IMPLEMENTATION has been deprecated
#endif

/**
 * This macro has been deprecated in releasee 1.1. Please see
 * http://trac.pjsip.org/repos/wiki/Audio_Dev_API for more information.
 */
#if defined(PJMEDIA_PREFER_DIRECT_SOUND)
#   error PJMEDIA_PREFER_DIRECT_SOUND has been deprecated
#endif

/**
 * This macro controls whether the legacy sound device API is to be
 * implemented, for applications that still use the old sound device
 * API (sound.h). If this macro is set to non-zero, the sound_legacy.c
 * will be included in the compilation. The sound_legacy.c is an
 * implementation of old sound device (sound.h) using the new Audio
 * Device API.
 *
 * Please see http://trac.pjsip.org/repos/wiki/Audio_Dev_API for more
 * info.
 */
#ifndef PJMEDIA_HAS_LEGACY_SOUND_API
#   define PJMEDIA_HAS_LEGACY_SOUND_API	    1
#endif

/**
 * Specify default sound device latency, in milisecond.
 */
#ifndef PJMEDIA_SND_DEFAULT_REC_LATENCY
#   define PJMEDIA_SND_DEFAULT_REC_LATENCY  100
#endif

/**
 * Specify default sound device latency, in milisecond. 
 *
 * Default is 160ms for Windows Mobile and 140ms for other platforms.
 */
#ifndef PJMEDIA_SND_DEFAULT_PLAY_LATENCY
#   if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
#	define PJMEDIA_SND_DEFAULT_PLAY_LATENCY	    160
#   else
#	define PJMEDIA_SND_DEFAULT_PLAY_LATENCY	    140
#   endif
#endif


/*
 * Types of WSOLA backend algorithm.
 */

/**
 * This denotes implementation of WSOLA using null algorithm. Expansion
 * will generate zero frames, and compression will just discard some
 * samples from the input.
 *
 * This type of implementation may be used as it requires the least
 * processing power.
 */
#define PJMEDIA_WSOLA_IMP_NULL		    0

/**
 * This denotes implementation of WSOLA using fixed or floating point WSOLA
 * algorithm. This implementation provides the best quality of the result,
 * at the expense of one frame delay and intensive processing power 
 * requirement.
 */
#define PJMEDIA_WSOLA_IMP_WSOLA		    1

/**
 * This denotes implementation of WSOLA algorithm with faster waveform 
 * similarity calculation. This implementation provides fair quality of 
 * the result with the main advantage of low processing power requirement.
 */
#define PJMEDIA_WSOLA_IMP_WSOLA_LITE	    2

/**
 * Specify type of Waveform based Similarity Overlap and Add (WSOLA) backend
 * implementation to be used. WSOLA is an algorithm to expand and/or compress 
 * audio frames without changing the pitch, and used by the delaybuf and as PLC
 * backend algorithm.
 *
 * Default is PJMEDIA_WSOLA_IMP_WSOLA
 */
#ifndef PJMEDIA_WSOLA_IMP
#   define PJMEDIA_WSOLA_IMP		    PJMEDIA_WSOLA_IMP_WSOLA
#endif


/**
 * Specify the default maximum duration of synthetic audio that is generated
 * by WSOLA. This value should be long enough to cover burst of packet losses. 
 * but not too long, because as the duration increases the quality would 
 * degrade considerably.
 *
 * Note that this limit is only applied when fading is enabled in the WSOLA
 * session.
 *
 * Default: 80
 */
#ifndef PJMEDIA_WSOLA_MAX_EXPAND_MSEC
#   define PJMEDIA_WSOLA_MAX_EXPAND_MSEC    80
#endif


/**
 * Specify WSOLA template length, in milliseconds. The longer the template,
 * the smoother signal to be generated at the expense of more computation
 * needed, since the algorithm will have to compare more samples to find
 * the most similar pitch.
 *
 * Default: 5
 */
#ifndef PJMEDIA_WSOLA_TEMPLATE_LENGTH_MSEC
#   define PJMEDIA_WSOLA_TEMPLATE_LENGTH_MSEC	5
#endif


/**
 * Specify WSOLA algorithm delay, in milliseconds. The algorithm delay is
 * used to merge synthetic samples with real samples in the transition
 * between real to synthetic and vice versa. The longer the delay, the 
 * smoother signal to be generated, at the expense of longer latency and
 * a slighty more computation.
 *
 * Default: 5
 */
#ifndef PJMEDIA_WSOLA_DELAY_MSEC
#   define PJMEDIA_WSOLA_DELAY_MSEC	    5
#endif


/**
 * Set this to non-zero to disable fade-out/in effect in the PLC when it
 * instructs WSOLA to generate synthetic frames. The use of fading may
 * or may not improve the quality of audio, depending on the nature of
 * packet loss and the type of audio input (e.g. speech vs music).
 * Disabling fading also implicitly remove the maximum limit of synthetic
 * audio samples generated by WSOLA (see PJMEDIA_WSOLA_MAX_EXPAND_MSEC).
 *
 * Default: 0
 */
#ifndef PJMEDIA_WSOLA_PLC_NO_FADING
#   define PJMEDIA_WSOLA_PLC_NO_FADING	    0
#endif


/**
 * Limit the number of calls by stream to the PLC to generate synthetic
 * frames to this duration. If packets are still lost after this maximum
 * duration, silence will be generated by the stream instead. Since the
 * PLC normally should have its own limit on the maximum duration of
 * synthetic frames to be generated (for PJMEDIA's PLC, the limit is
 * PJMEDIA_WSOLA_MAX_EXPAND_MSEC), we can set this value to a large number
 * to give additional flexibility should the PLC wants to do something
 * clever with the lost frames.
 *
 * Default: 240 ms
 */
#ifndef PJMEDIA_MAX_PLC_DURATION_MSEC
#   define PJMEDIA_MAX_PLC_DURATION_MSEC    240
#endif


/**
 * Specify number of sound buffers. Larger number is better for sound
 * stability and to accommodate sound devices that are unable to send frames
 * in timely manner, however it would probably cause more audio delay (and 
 * definitely will take more memory). One individual buffer is normally 10ms
 * or 20 ms long, depending on ptime settings (samples_per_frame value).
 *
 * The setting here currently is used by the conference bridge, the splitter
 * combiner port, and dsound.c.
 *
 * Default: (PJMEDIA_SND_DEFAULT_PLAY_LATENCY+20)/20
 */
#ifndef PJMEDIA_SOUND_BUFFER_COUNT
#   define PJMEDIA_SOUND_BUFFER_COUNT	    ((PJMEDIA_SND_DEFAULT_PLAY_LATENCY+20)/20)
#endif


/**
 * Specify which A-law/U-law conversion algorithm to use.
 * By default the conversion algorithm uses A-law/U-law table which gives
 * the best performance, at the expense of 33 KBytes of static data.
 * If this option is disabled, a smaller but slower algorithm will be used.
 */
#ifndef PJMEDIA_HAS_ALAW_ULAW_TABLE
#   define PJMEDIA_HAS_ALAW_ULAW_TABLE	    1
#endif


/**
 * Unless specified otherwise, G711 codec is included by default.
 */
#ifndef PJMEDIA_HAS_G711_CODEC
#   define PJMEDIA_HAS_G711_CODEC	    1
#endif


/*
 * Warn about obsolete macros.
 *
 * PJMEDIA_HAS_SMALL_FILTER has been deprecated in 0.7.
 */
#if defined(PJMEDIA_HAS_SMALL_FILTER)
#   ifdef _MSC_VER
#	pragma message("Warning: PJMEDIA_HAS_SMALL_FILTER macro is deprecated"\
		       " and has no effect")
#   else
#	warning "PJMEDIA_HAS_SMALL_FILTER macro is deprecated and has no effect"
#   endif
#endif


/*
 * Warn about obsolete macros.
 *
 * PJMEDIA_HAS_LARGE_FILTER has been deprecated in 0.7.
 */
#if defined(PJMEDIA_HAS_LARGE_FILTER)
#   ifdef _MSC_VER
#	pragma message("Warning: PJMEDIA_HAS_LARGE_FILTER macro is deprecated"\
		       " and has no effect")
#   else
#	warning "PJMEDIA_HAS_LARGE_FILTER macro is deprecated"
#   endif
#endif


/*
 * These macros are obsolete in 0.7.1 so it will trigger compilation error.
 * Please use PJMEDIA_RESAMPLE_IMP to select the resample implementation
 * to use.
 */
#ifdef PJMEDIA_HAS_LIBRESAMPLE
#   error "PJMEDIA_HAS_LIBRESAMPLE macro is deprecated. Use '#define PJMEDIA_RESAMPLE_IMP PJMEDIA_RESAMPLE_LIBRESAMPLE'"
#endif

#ifdef PJMEDIA_HAS_SPEEX_RESAMPLE
#   error "PJMEDIA_HAS_SPEEX_RESAMPLE macro is deprecated. Use '#define PJMEDIA_RESAMPLE_IMP PJMEDIA_RESAMPLE_SPEEX'"
#endif


/*
 * Sample rate conversion backends.
 * Select one of these backends in PJMEDIA_RESAMPLE_IMP.
 */
#define PJMEDIA_RESAMPLE_NONE		    1	/**< No resampling.	    */
#define PJMEDIA_RESAMPLE_LIBRESAMPLE	    2	/**< Sample rate conversion 
						     using libresample.  */
#define PJMEDIA_RESAMPLE_SPEEX		    3	/**< Sample rate conversion 
						     using Speex. */
#define PJMEDIA_RESAMPLE_LIBSAMPLERATE	    4	/**< Sample rate conversion 
						     using libsamplerate 
						     (a.k.a Secret Rabbit Code)
						 */

/**
 * Select which resample implementation to use. Currently pjmedia supports:
 *  - #PJMEDIA_RESAMPLE_LIBRESAMPLE, to use libresample-1.7, this is the default
 *    implementation to be used.
 *  - #PJMEDIA_RESAMPLE_LIBSAMPLERATE, to use libsamplerate implementation
 *    (a.k.a. Secret Rabbit Code).
 *  - #PJMEDIA_RESAMPLE_SPEEX, to use sample rate conversion in Speex library.
 *  - #PJMEDIA_RESAMPLE_NONE, to disable sample rate conversion. Any calls to
 *    resample function will return error.
 *
 * Default is PJMEDIA_RESAMPLE_LIBRESAMPLE
 */
#ifndef PJMEDIA_RESAMPLE_IMP
#   define PJMEDIA_RESAMPLE_IMP		    PJMEDIA_RESAMPLE_LIBRESAMPLE
#endif


/**
 * Specify whether libsamplerate, when used, should be linked statically
 * into the application. This option is only useful for Visual Studio
 * projects, and when this static linking is enabled
 */


/**
 * Default file player/writer buffer size.
 */
#ifndef PJMEDIA_FILE_PORT_BUFSIZE
#   define PJMEDIA_FILE_PORT_BUFSIZE		4000
#endif


/**
 * Maximum frame duration (in msec) to be supported.
 * This (among other thing) will affect the size of buffers to be allocated
 * for outgoing packets.
 */
#ifndef PJMEDIA_MAX_FRAME_DURATION_MS   
#   define PJMEDIA_MAX_FRAME_DURATION_MS   	200
#endif


/**
 * Max packet size for transmitting direction.
 */
#ifndef PJMEDIA_MAX_MTU			
#  define PJMEDIA_MAX_MTU			1500
#endif


/**
 * Max packet size for receiving direction.
 */
#ifndef PJMEDIA_MAX_MRU			
#  define PJMEDIA_MAX_MRU			2000
#endif


/**
 * DTMF/telephone-event duration, in timestamp. To specify the duration in
 * milliseconds, use the setting PJMEDIA_DTMF_DURATION_MSEC instead.
 */
#ifndef PJMEDIA_DTMF_DURATION		
#  define PJMEDIA_DTMF_DURATION			1600	/* in timestamp */
#endif


/**
 * DTMF/telephone-event duration, in milliseconds. If the value is greater
 * than zero, than this setting will be used instead of PJMEDIA_DTMF_DURATION.
 *
 * Note that for a clockrate of 8 KHz, a dtmf duration of 1600 timestamp
 * units (the default value of PJMEDIA_DTMF_DURATION) is equivalent to 200 ms. 
 */
#ifndef PJMEDIA_DTMF_DURATION_MSEC		
#  define PJMEDIA_DTMF_DURATION_MSEC		0
#endif


/**
 * Number of RTP packets received from different source IP address from the
 * remote address required to make the stream switch transmission
 * to the source address.
 */
#ifndef PJMEDIA_RTP_NAT_PROBATION_CNT	
#  define PJMEDIA_RTP_NAT_PROBATION_CNT		10
#endif


/**
 * Number of RTCP packets received from different source IP address from the
 * remote address required to make the stream switch RTCP transmission
 * to the source address.
 */
#ifndef PJMEDIA_RTCP_NAT_PROBATION_CNT
#  define PJMEDIA_RTCP_NAT_PROBATION_CNT	3
#endif


/**
 * Specify whether RTCP should be advertised in SDP. This setting would
 * affect whether RTCP candidate will be added in SDP when ICE is used.
 * Application might want to disable RTCP advertisement in SDP to
 * reduce the message size.
 *
 * Default: 1 (yes)
 */
#ifndef PJMEDIA_ADVERTISE_RTCP
#   define PJMEDIA_ADVERTISE_RTCP		1
#endif


/**
 * Interval to send regular RTCP packets, in msec.
 */
#ifndef PJMEDIA_RTCP_INTERVAL
#   define PJMEDIA_RTCP_INTERVAL		5000	/* msec*/
#endif


/**
 * Minimum interval between two consecutive outgoing RTCP-FB packets,
 * such as Picture Loss Indication, in msec.
 */
#ifndef PJMEDIA_RTCP_FB_INTERVAL
#   define PJMEDIA_RTCP_FB_INTERVAL		50	/* msec*/
#endif


/**
 * Tell RTCP to ignore the first N packets when calculating the
 * jitter statistics. From experimentation, the first few packets
 * (25 or so) have relatively big jitter, possibly because during
 * this time, the program is also busy setting up the signaling,
 * so they make the average jitter big.
 *
 * Default: 25.
 */
#ifndef PJMEDIA_RTCP_IGNORE_FIRST_PACKETS
#   define  PJMEDIA_RTCP_IGNORE_FIRST_PACKETS	25
#endif


/**
 * Specify whether RTCP statistics includes raw jitter statistics.
 * Raw jitter is defined as absolute value of network transit time
 * difference of two consecutive packets; refering to "difference D"
 * term in interarrival jitter calculation in RFC 3550 section 6.4.1.
 *
 * Default: 0 (no).
 */
#ifndef PJMEDIA_RTCP_STAT_HAS_RAW_JITTER
#   define PJMEDIA_RTCP_STAT_HAS_RAW_JITTER	0
#endif

/**
 * Specify the factor with wich RTCP RTT statistics should be normalized 
 * if exceptionally high. For e.g. mobile networks with potentially large
 * fluctuations, this might be unwanted.
 *
 * Use (0) to disable this feature.
 *
 * Default: 3.
 */
#ifndef PJMEDIA_RTCP_NORMALIZE_FACTOR
#   define PJMEDIA_RTCP_NORMALIZE_FACTOR	3
#endif


/**
 * Specify whether RTCP statistics includes IP Delay Variation statistics.
 * IPDV is defined as network transit time difference of two consecutive
 * packets. The IPDV statistic can be useful to inspect clock skew existance
 * and level, e.g: when the IPDV mean values were stable in positive numbers,
 * then the remote clock (used in sending RTP packets) is faster than local
 * system clock. Ideally, the IPDV mean values are always equal to 0.
 *
 * Default: 0 (no).
 */
#ifndef PJMEDIA_RTCP_STAT_HAS_IPDV
#   define PJMEDIA_RTCP_STAT_HAS_IPDV		0
#endif


/**
 * Specify whether RTCP XR support should be built into PJMEDIA. Disabling
 * this feature will reduce footprint slightly. Note that even when this 
 * setting is enabled, RTCP XR processing will only be performed in stream 
 * if it is enabled on run-time on per stream basis. See  
 * PJMEDIA_STREAM_ENABLE_XR setting for more info.
 *
 * Default: 0 (no).
 */
#ifndef PJMEDIA_HAS_RTCP_XR
#   define PJMEDIA_HAS_RTCP_XR			0
#endif


/**
 * The RTCP XR feature is activated and used by stream if \a enable_rtcp_xr
 * field of \a pjmedia_stream_info structure is non-zero. This setting 
 * controls the default value of this field.
 *
 * Default: 0 (disabled)
 */
#ifndef PJMEDIA_STREAM_ENABLE_XR
#   define PJMEDIA_STREAM_ENABLE_XR		0
#endif


/**
 * Specify the buffer length for storing any received RTCP SDES text
 * in a stream session. Usually RTCP contains only the mandatory SDES
 * field, i.e: CNAME.
 * 
 * Default: 64 bytes.
 */
#ifndef PJMEDIA_RTCP_RX_SDES_BUF_LEN
#   define PJMEDIA_RTCP_RX_SDES_BUF_LEN		64
#endif


/**
 * Specify the maximum number of RTCP Feedback capability definition.
 * 
 * Default: 16
 */
#ifndef PJMEDIA_RTCP_FB_MAX_CAP
#   define PJMEDIA_RTCP_FB_MAX_CAP		16
#endif


/**
 * Specify how long (in miliseconds) the stream should suspend the
 * silence detector/voice activity detector (VAD) during the initial
 * period of the session. This feature is useful to open bindings in
 * all NAT routers between local and remote endpoint since most NATs
 * do not allow incoming packet to get in before local endpoint sends
 * outgoing packets.
 *
 * Specify zero to disable this feature.
 *
 * Default: 600 msec (which gives good probability that some RTP 
 *                    packets will reach the destination, but without
 *                    filling up the jitter buffer on the remote end).
 */
#ifndef PJMEDIA_STREAM_VAD_SUSPEND_MSEC
#   define PJMEDIA_STREAM_VAD_SUSPEND_MSEC	600
#endif

/**
 * Perform RTP payload type checking in the audio stream. Normally the peer
 * MUST send RTP with payload type as we specified in our SDP. Certain
 * agents may not be able to follow this hence the only way to have
 * communication is to disable this check.
 *
 * Default: 1
 */
#ifndef PJMEDIA_STREAM_CHECK_RTP_PT
#   define PJMEDIA_STREAM_CHECK_RTP_PT		1
#endif

/**
 * Reserve some space for application extra data, e.g: SRTP auth tag,
 * in RTP payload, so the total payload length will not exceed the MTU.
 */
#ifndef PJMEDIA_STREAM_RESV_PAYLOAD_LEN
#   define PJMEDIA_STREAM_RESV_PAYLOAD_LEN	20
#endif


/**
 * Specify the maximum duration of silence period in the codec, in msec. 
 * This is useful for example to keep NAT binding open in the firewall
 * and to prevent server from disconnecting the call because no 
 * RTP packet is received.
 *
 * This only applies to codecs that use PJMEDIA's VAD (pretty much
 * everything including iLBC, except Speex, which has its own DTX 
 * mechanism).
 *
 * Use (-1) to disable this feature.
 *
 * Default: 5000 ms
 *
 */
#ifndef PJMEDIA_CODEC_MAX_SILENCE_PERIOD
#   define PJMEDIA_CODEC_MAX_SILENCE_PERIOD	5000
#endif


/**
 * Suggested or default threshold to be set for fixed silence detection
 * or as starting threshold for adaptive silence detection. The threshold
 * has the range from zero to 0xFFFF.
 */
#ifndef PJMEDIA_SILENCE_DET_THRESHOLD
#   define PJMEDIA_SILENCE_DET_THRESHOLD	4
#endif


/**
 * Maximum silence threshold in the silence detector. The silence detector
 * will not cut the audio transmission if the audio level is above this
 * level.
 *
 * Use 0x10000 (or greater) to disable this feature.
 *
 * Default: 0x10000 (disabled)
 */
#ifndef PJMEDIA_SILENCE_DET_MAX_THRESHOLD
#   define PJMEDIA_SILENCE_DET_MAX_THRESHOLD	0x10000
#endif


/**
 * Speex Accoustic Echo Cancellation (AEC).
 * By default is enabled.
 */
#ifndef PJMEDIA_HAS_SPEEX_AEC
#   define PJMEDIA_HAS_SPEEX_AEC		1
#endif


/**
 * Specify whether Automatic Gain Control (AGC) should also be enabled in
 * Speex AEC.
 *
 * Default: 1 (yes)
 */
#ifndef PJMEDIA_SPEEX_AEC_USE_AGC
#   define PJMEDIA_SPEEX_AEC_USE_AGC		1
#endif


/**
 * Specify whether denoise should also be enabled in Speex AEC.
 *
 * Default: 1 (yes)
 */
#ifndef PJMEDIA_SPEEX_AEC_USE_DENOISE
#   define PJMEDIA_SPEEX_AEC_USE_DENOISE	1
#endif


/**
 * WebRtc Accoustic Echo Cancellation (AEC).
 * By default is disabled.
 */
#ifndef PJMEDIA_HAS_WEBRTC_AEC
#   define PJMEDIA_HAS_WEBRTC_AEC		0
#endif

/**
 * Specify whether WebRtc EC should use its mobile version AEC.
 *
 * Default: 0 (no)
 */
#ifndef PJMEDIA_WEBRTC_AEC_USE_MOBILE
#   define PJMEDIA_WEBRTC_AEC_USE_MOBILE 	0
#endif


/**
 * Maximum number of parameters in SDP fmtp attribute.
 *
 * Default: 16
 */
#ifndef PJMEDIA_CODEC_MAX_FMTP_CNT
#   define PJMEDIA_CODEC_MAX_FMTP_CNT		16
#endif


/**
 * This specifies the behavior of the SDP negotiator when responding to an
 * offer, whether it should rather use the codec preference as set by
 * remote, or should it rather use the codec preference as specified by
 * local endpoint.
 *
 * For example, suppose incoming call has codec order "8 0 3", while 
 * local codec order is "3 0 8". If remote codec order is preferable,
 * the selected codec will be 8, while if local codec order is preferable,
 * the selected codec will be 3.
 *
 * If set to non-zero, the negotiator will use the codec order as specified
 * by remote in the offer.
 *
 * Note that this behavior can be changed during run-time by calling
 * pjmedia_sdp_neg_set_prefer_remote_codec_order().
 *
 * Default is 1 (to maintain backward compatibility)
 */
#ifndef PJMEDIA_SDP_NEG_PREFER_REMOTE_CODEC_ORDER
#   define PJMEDIA_SDP_NEG_PREFER_REMOTE_CODEC_ORDER	1
#endif

/**
 * This specifies the behavior of the SDP negotiator when responding to an
 * offer, whether it should answer with multiple formats or not.
 *
 * Note that this behavior can be changed during run-time by calling
 * pjmedia_sdp_neg_set_allow_multiple_codecs().
 *
 * Default is 0 (to maintain backward compatibility)
 */
#ifndef PJMEDIA_SDP_NEG_ANSWER_MULTIPLE_CODECS
#   define PJMEDIA_SDP_NEG_ANSWER_MULTIPLE_CODECS	0
#endif


/**
 * This specifies the maximum number of the customized SDP format
 * negotiation callbacks.
 */
#ifndef PJMEDIA_SDP_NEG_MAX_CUSTOM_FMT_NEG_CB
#   define PJMEDIA_SDP_NEG_MAX_CUSTOM_FMT_NEG_CB	8
#endif


/**
 * This specifies if the SDP negotiator should rewrite answer payload
 * type numbers to use the same payload type numbers as the remote offer
 * for all matched codecs.
 *
 * Default is 1 (yes)
 */
#ifndef PJMEDIA_SDP_NEG_ANSWER_SYMMETRIC_PT
#   define PJMEDIA_SDP_NEG_ANSWER_SYMMETRIC_PT		1
#endif


/**
 * This specifies if the SDP negotiator should compare its content before 
 * incrementing the origin version on the subsequent offer/answer. 
 * If this is set to 1, origin version will only by incremented if the 
 * new offer/answer is different than the previous one. For backward 
 * compatibility and performance this is set to 0.
 *
 * Default is 0 (No)
 */
#ifndef PJMEDIA_SDP_NEG_COMPARE_BEFORE_INC_VERSION
#   define PJMEDIA_SDP_NEG_COMPARE_BEFORE_INC_VERSION	0
#endif


/**
 * Support for sending and decoding RTCP port in SDP (RFC 3605).
 * Default is equal to PJMEDIA_ADVERTISE_RTCP setting.
 */
#ifndef PJMEDIA_HAS_RTCP_IN_SDP
#   define PJMEDIA_HAS_RTCP_IN_SDP		(PJMEDIA_ADVERTISE_RTCP)
#endif


/**
 * This macro controls whether pjmedia should include SDP
 * bandwidth modifier "TIAS" (RFC3890).
 *
 * Note that there is also a run-time variable to turn this setting
 * on or off, defined in endpoint.c. To access this variable, use
 * the following construct
 *
 \verbatim
    extern pj_bool_t pjmedia_add_bandwidth_tias_in_sdp;

    // Do not enable bandwidth information inclusion in sdp
    pjmedia_add_bandwidth_tias_in_sdp = PJ_FALSE;
 \endverbatim
 *
 * Default: 1 (yes)
 */
#ifndef PJMEDIA_ADD_BANDWIDTH_TIAS_IN_SDP
#   define PJMEDIA_ADD_BANDWIDTH_TIAS_IN_SDP	1
#endif


/**
 * This macro controls whether pjmedia should include SDP rtpmap 
 * attribute for static payload types. SDP rtpmap for static
 * payload types are optional, although they are normally included
 * for interoperability reason.
 *
 * Note that there is also a run-time variable to turn this setting
 * on or off, defined in endpoint.c. To access this variable, use
 * the following construct
 *
 \verbatim
    extern pj_bool_t pjmedia_add_rtpmap_for_static_pt;

    // Do not include rtpmap for static payload types (<96)
    pjmedia_add_rtpmap_for_static_pt = PJ_FALSE;
 \endverbatim
 *
 * Default: 1 (yes)
 */
#ifndef PJMEDIA_ADD_RTPMAP_FOR_STATIC_PT
#   define PJMEDIA_ADD_RTPMAP_FOR_STATIC_PT	1
#endif


/**
 * This macro declares the start payload type for telephone-event
 * that is advertised by PJMEDIA for outgoing SDP. If this macro
 * is set to zero, telephone events would not be advertised nor
 * supported.
 */
#ifndef PJMEDIA_RTP_PT_TELEPHONE_EVENTS
#   define PJMEDIA_RTP_PT_TELEPHONE_EVENTS	    120
#endif


/**
 * This macro declares whether PJMEDIA should generate multiple
 * telephone-event formats in SDP offer, i.e: one for each audio codec
 * clock rate (see also ticket #2088). If this macro is set to zero, only
 * one telephone event format will be generated and it uses clock rate 8kHz
 * (old behavior before ticket #2088).
 *
 * Default: 1 (yes)
 */
#ifndef PJMEDIA_TELEPHONE_EVENT_ALL_CLOCKRATES
#   define PJMEDIA_TELEPHONE_EVENT_ALL_CLOCKRATES   1
#endif


/**
 * Maximum tones/digits that can be enqueued in the tone generator.
 */
#ifndef PJMEDIA_TONEGEN_MAX_DIGITS
#   define PJMEDIA_TONEGEN_MAX_DIGITS		    32
#endif


/* 
 * Below specifies the various tone generator backend algorithm.
 */

/** 
 * The math's sine(), floating point. This has very good precision 
 * but it's the slowest and requires floating point support and
 * linking with the math library.
 */
#define PJMEDIA_TONEGEN_SINE			    1

/**
 * Floating point approximation of sine(). This has relatively good
 * precision and much faster than plain sine(), but it requires floating-
 * point support and linking with the math library.
 */
#define PJMEDIA_TONEGEN_FLOATING_POINT		    2

/**
 * Fixed point using sine signal generated by Cordic algorithm. This
 * algorithm can be tuned to provide balance between precision and
 * performance by tuning the PJMEDIA_TONEGEN_FIXED_POINT_CORDIC_LOOP 
 * setting, and may be suitable for platforms that lack floating-point
 * support.
 */
#define PJMEDIA_TONEGEN_FIXED_POINT_CORDIC	    3

/**
 * Fast fixed point using some approximation to generate sine waves.
 * The tone generated by this algorithm is not very precise, however
 * the algorithm is very fast.
 */
#define PJMEDIA_TONEGEN_FAST_FIXED_POINT	    4


/**
 * Specify the tone generator algorithm to be used. Please see 
 * http://trac.pjsip.org/repos/wiki/Tone_Generator for the performance
 * analysis results of the various tone generator algorithms.
 *
 * Default value:
 *  - PJMEDIA_TONEGEN_FLOATING_POINT when PJ_HAS_FLOATING_POINT is set
 *  - PJMEDIA_TONEGEN_FIXED_POINT_CORDIC when PJ_HAS_FLOATING_POINT is not set
 */
#ifndef PJMEDIA_TONEGEN_ALG
#   if defined(PJ_HAS_FLOATING_POINT) && PJ_HAS_FLOATING_POINT
#	define PJMEDIA_TONEGEN_ALG	PJMEDIA_TONEGEN_FLOATING_POINT
#   else
#	define PJMEDIA_TONEGEN_ALG	PJMEDIA_TONEGEN_FIXED_POINT_CORDIC
#   endif
#endif


/**
 * Specify the number of calculation loops to generate the tone, when
 * PJMEDIA_TONEGEN_FIXED_POINT_CORDIC algorithm is used. With more calculation
 * loops, the tone signal gets more precise, but this will add more 
 * processing.
 *
 * Valid values are 1 to 28.
 *
 * Default value: 10
 */
#ifndef PJMEDIA_TONEGEN_FIXED_POINT_CORDIC_LOOP
#   define PJMEDIA_TONEGEN_FIXED_POINT_CORDIC_LOOP  10
#endif


/**
 * Enable high quality of tone generation, the better quality will cost
 * more CPU load. This is only applied to floating point enabled machines.
 *
 * By default it is enabled when PJ_HAS_FLOATING_POINT is set.
 *
 * This macro has been deprecated in version 1.0-rc3.
 */
#ifdef PJMEDIA_USE_HIGH_QUALITY_TONEGEN
#   error   "The PJMEDIA_USE_HIGH_QUALITY_TONEGEN macro is obsolete"
#endif


/**
 * Fade-in duration for the tone, in milliseconds. Set to zero to disable
 * this feature.
 *
 * Default: 1 (msec)
 */
#ifndef PJMEDIA_TONEGEN_FADE_IN_TIME
#   define PJMEDIA_TONEGEN_FADE_IN_TIME		    1
#endif


/**
 * Fade-out duration for the tone, in milliseconds. Set to zero to disable
 * this feature.
 *
 * Default: 2 (msec)
 */
#ifndef PJMEDIA_TONEGEN_FADE_OUT_TIME
#   define PJMEDIA_TONEGEN_FADE_OUT_TIME	    2
#endif


/**
 * The default tone generator amplitude (1-32767).
 *
 * Default value: 12288
 */
#ifndef PJMEDIA_TONEGEN_VOLUME
#   define PJMEDIA_TONEGEN_VOLUME		    12288
#endif


/**
 * Enable support for SRTP media transport. This will require linking
 * with libsrtp from the third_party directory.
 *
 * By default it is enabled.
 */
#ifndef PJMEDIA_HAS_SRTP
#   define PJMEDIA_HAS_SRTP			    1
#endif


/**
 * Enable session description for SRTP keying.
 *
 * By default it is enabled.
 */
#ifndef PJMEDIA_SRTP_HAS_SDES
#   define PJMEDIA_SRTP_HAS_SDES		    1
#endif


/**
 * Enable DTLS for SRTP keying.
 *
 * Default value: 0 (disabled)
 */
#ifndef PJMEDIA_SRTP_HAS_DTLS
#   define PJMEDIA_SRTP_HAS_DTLS		    0
#endif


/**
 * Set OpenSSL ciphers for DTLS-SRTP.
 *
 * Default value: "DEFAULT"
 */
#ifndef PJMEDIA_SRTP_DTLS_OSSL_CIPHERS
#   define PJMEDIA_SRTP_DTLS_OSSL_CIPHERS	    "DEFAULT"
#endif


/**
 * Maximum number of SRTP cryptos.
 *
 * Default: 16
 */
#ifndef PJMEDIA_SRTP_MAX_CRYPTOS
#   define PJMEDIA_SRTP_MAX_CRYPTOS		    16
#endif


/**
 * Enable AES_CM_256 cryptos in SRTP.
 * Default: enabled.
 */
#ifndef PJMEDIA_SRTP_HAS_AES_CM_256
#   define PJMEDIA_SRTP_HAS_AES_CM_256	    	    1
#endif


/**
 * Enable AES_CM_192 cryptos in SRTP.
 * It was reported that this crypto only works among libsrtp backends,
 * so we recommend to disable this.
 *
 * To enable this, you would require OpenSSL which supports it.
 * See https://trac.pjsip.org/repos/ticket/1943 for more info.
 *
 * Default: disabled.
 */
#ifndef PJMEDIA_SRTP_HAS_AES_CM_192
#   define PJMEDIA_SRTP_HAS_AES_CM_192	    	    0
#endif


/**
 * Enable AES_CM_128 cryptos in SRTP.
 * Default: enabled.
 */
#ifndef PJMEDIA_SRTP_HAS_AES_CM_128
#   define PJMEDIA_SRTP_HAS_AES_CM_128    	    1
#endif


/**
 * Enable AES_GCM_256 cryptos in SRTP.
 *
 * To enable this, you would require OpenSSL which supports it.
 * See https://trac.pjsip.org/repos/ticket/1943 for more info. 
 *
 * Default: disabled.
 */
#ifndef PJMEDIA_SRTP_HAS_AES_GCM_256
#   define PJMEDIA_SRTP_HAS_AES_GCM_256	    	    0
#endif


/**
 * Enable AES_GCM_128 cryptos in SRTP.
 *
 * To enable this, you would require OpenSSL which supports it.
 * See https://trac.pjsip.org/repos/ticket/1943 for more info.
 *
 * Default: disabled.
 */
#ifndef PJMEDIA_SRTP_HAS_AES_GCM_128
#   define PJMEDIA_SRTP_HAS_AES_GCM_128    	    0
#endif


/**
 * Specify whether SRTP needs to handle condition that old packets with
 * incorect RTP seq are still coming when SRTP is restarted.
 *
 * Default: enabled.
 */
#ifndef PJMEDIA_SRTP_CHECK_RTP_SEQ_ON_RESTART
#   define PJMEDIA_SRTP_CHECK_RTP_SEQ_ON_RESTART    1
#endif


/**
 * Specify whether SRTP needs to handle condition that remote may reset
 * or maintain ROC when SRTP is restarted.
 *
 * Default: enabled.
 */
#ifndef PJMEDIA_SRTP_CHECK_ROC_ON_RESTART
#   define PJMEDIA_SRTP_CHECK_ROC_ON_RESTART        1
#endif


/**
 * Let the library handle libsrtp initialization and deinitialization.
 * Application may want to disable this and manually perform libsrtp
 * initialization and deinitialization when it needs to use libsrtp
 * before the library is initialized or after the library is shutdown.
 *
 * By default it is enabled.
 */
#ifndef PJMEDIA_LIBSRTP_AUTO_INIT_DEINIT
#   define PJMEDIA_LIBSRTP_AUTO_INIT_DEINIT	    1
#endif


/**
 * Enable support to handle codecs with inconsistent clock rate
 * between clock rate in SDP/RTP & the clock rate that is actually used.
 * This happens for example with G.722 and MPEG audio codecs.
 * See:
 *  - G.722      : RFC 3551 4.5.2
 *  - MPEG audio : RFC 3551 4.5.13 & RFC 3119
 *  - OPUS	 : RFC 7587
 *
 * Also when this feature is enabled, some handling will be performed
 * to deal with clock rate incompatibilities of some phones.
 *
 * By default it is enabled.
 */
#ifndef PJMEDIA_HANDLE_G722_MPEG_BUG
#   define PJMEDIA_HANDLE_G722_MPEG_BUG		    1
#endif


/* Setting to determine if media transport should switch RTP and RTCP
 * remote address to the source address of the packets it receives.
 *
 * By default it is enabled.
 */
#ifndef PJMEDIA_TRANSPORT_SWITCH_REMOTE_ADDR
#   define PJMEDIA_TRANSPORT_SWITCH_REMOTE_ADDR	    1
#endif


/**
 * Transport info (pjmedia_transport_info) contains a socket info and list
 * of transport specific info, since transports can be chained together 
 * (for example, SRTP transport uses UDP transport as the underlying 
 * transport). This constant specifies maximum number of transport specific
 * infos that can be held in a transport info.
 */
#ifndef PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXCNT
#   define PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXCNT   4
#endif


/**
 * Maximum size in bytes of storage buffer of a transport specific info.
 */
#ifndef PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXSIZE
#   define PJMEDIA_TRANSPORT_SPECIFIC_INFO_MAXSIZE  (50*sizeof(long))
#endif


/**
 * Value to be specified in PJMEDIA_STREAM_ENABLE_KA setting.
 * This indicates that an empty RTP packet should be used as
 * the keep-alive packet.
 */
#define PJMEDIA_STREAM_KA_EMPTY_RTP		    1

/**
 * Value to be specified in PJMEDIA_STREAM_ENABLE_KA setting.
 * This indicates that a user defined packet should be used
 * as the keep-alive packet. The content of the user-defined
 * packet is specified by PJMEDIA_STREAM_KA_USER_PKT. Default
 * content is a CR-LF packet.
 */
#define PJMEDIA_STREAM_KA_USER			    2

/**
 * The content of the user defined keep-alive packet. The format
 * of the packet is initializer to pj_str_t structure. Note that
 * the content may contain NULL character.
 */
#ifndef PJMEDIA_STREAM_KA_USER_PKT
#   define PJMEDIA_STREAM_KA_USER_PKT	{ "\r\n", 2 }
#endif

/**
 * Specify another type of keep-alive and NAT hole punching 
 * mechanism (the other type is PJMEDIA_STREAM_VAD_SUSPEND_MSEC
 * and PJMEDIA_CODEC_MAX_SILENCE_PERIOD) to be used by stream. 
 * When this feature is enabled, the stream will initially 
 * transmit one packet to punch a hole in NAT, and periodically
 * transmit keep-alive packets.
 *
 * When this alternative keep-alive mechanism is used, application
 * may disable the other keep-alive mechanisms, i.e: by setting 
 * PJMEDIA_STREAM_VAD_SUSPEND_MSEC to zero and 
 * PJMEDIA_CODEC_MAX_SILENCE_PERIOD to -1.
 *
 * The value of this macro specifies the type of packet used
 * for the keep-alive mechanism. Valid values are
 * PJMEDIA_STREAM_KA_EMPTY_RTP and PJMEDIA_STREAM_KA_USER.
 * 
 * The duration of the keep-alive interval further can be set
 * with PJMEDIA_STREAM_KA_INTERVAL setting.
 *
 * Default: 0 (disabled)
 */
#ifndef PJMEDIA_STREAM_ENABLE_KA
#   define PJMEDIA_STREAM_ENABLE_KA		    0
#endif


/**
 * Specify the keep-alive interval of PJMEDIA_STREAM_ENABLE_KA
 * mechanism, in seconds.
 *
 * Default: 5 seconds
 */
#ifndef PJMEDIA_STREAM_KA_INTERVAL
#   define PJMEDIA_STREAM_KA_INTERVAL		    5
#endif


/**
 * Specify the number of keep-alive needed to be sent after the stream is
 * created.
 *
 * Setting this to 0 will disable it.
 *
 * Default : 2
 */
#ifndef PJMEDIA_STREAM_START_KA_CNT
#   define PJMEDIA_STREAM_START_KA_CNT	2
#endif


/**
 * Specify the interval to send keep-alive after the stream is created,
 * in msec.
 *
 * Default : 1000
 */
#ifndef PJMEDIA_STREAM_START_KA_INTERVAL_MSEC
#   define PJMEDIA_STREAM_START_KA_INTERVAL_MSEC  1000
#endif


/**
 * Specify the number of identical consecutive error that will be ignored when 
 * receiving RTP/RTCP data before the library tries to restart the transport.
 *
 * When receiving RTP/RTCP data, the library will ignore error besides 
 * PJ_EPENDING or PJ_ECANCELLED and continue the loop to receive the data. 
 * If the OS always return error, then the loop will continue non stop.
 * This setting will limit the number of the identical consecutive error, 
 * before the library start to restart the transport. If error still happens
 * after transport restart, then PJMEDIA_EVENT_MEDIA_TP_ERR event will be 
 * publish as a notification.
 *
 * If PJ_ESOCKETSTOP is raised, then transport will be restarted regardless
 * of this setting.
 * 
 * To always ignore the error when receving RTP/RTCP, set this to 0.
 *
 * Default : 20
 */
#ifndef PJMEDIA_IGNORE_RECV_ERR_CNT
#   define PJMEDIA_IGNORE_RECV_ERR_CNT		20
#endif


/*
 * .... new stuffs ...
 */

/*
 * Video
 */

/**
 * Top level option to enable/disable video features.
 *
 * Default: 0 (disabled)
 */
#ifndef PJMEDIA_HAS_VIDEO
#   define PJMEDIA_HAS_VIDEO				0
#endif


/**
 * Specify if FFMPEG is available. The value here will be used as the default
 * value for other FFMPEG settings below.
 *
 * Default: 0
 */
#ifndef PJMEDIA_HAS_FFMPEG
#   define PJMEDIA_HAS_FFMPEG				0
#endif

/**
 * Specify if FFMPEG libavformat is available.
 *
 * Default: PJMEDIA_HAS_FFMPEG (or detected by configure)
 */
#ifndef PJMEDIA_HAS_LIBAVFORMAT
#   define PJMEDIA_HAS_LIBAVFORMAT			PJMEDIA_HAS_FFMPEG
#endif

/**
 * Specify if FFMPEG libavformat is available.
 *
 * Default: PJMEDIA_HAS_FFMPEG (or detected by configure)
 */
#ifndef PJMEDIA_HAS_LIBAVCODEC
#   define PJMEDIA_HAS_LIBAVCODEC			PJMEDIA_HAS_FFMPEG
#endif

/**
 * Specify if FFMPEG libavutil is available.
 *
 * Default: PJMEDIA_HAS_FFMPEG (or detected by configure)
 */
#ifndef PJMEDIA_HAS_LIBAVUTIL
#   define PJMEDIA_HAS_LIBAVUTIL			PJMEDIA_HAS_FFMPEG
#endif

/**
 * Specify if FFMPEG libswscale is available.
 *
 * Default: PJMEDIA_HAS_FFMPEG (or detected by configure)
 */
#ifndef PJMEDIA_HAS_LIBSWSCALE
#   define PJMEDIA_HAS_LIBSWSCALE			PJMEDIA_HAS_FFMPEG
#endif

/**
 * Specify if FFMPEG libavdevice is available.
 *
 * Default: PJMEDIA_HAS_FFMPEG (or detected by configure)
 */
#ifndef PJMEDIA_HAS_LIBAVDEVICE
#   define PJMEDIA_HAS_LIBAVDEVICE			PJMEDIA_HAS_FFMPEG
#endif

/**
 * Maximum video planes.
 *
 * Default: 4
 */
#ifndef PJMEDIA_MAX_VIDEO_PLANES
#   define PJMEDIA_MAX_VIDEO_PLANES			4
#endif

/**
 * Maximum number of video formats.
 *
 * Default: 32
 */
#ifndef PJMEDIA_MAX_VIDEO_FORMATS
#   define PJMEDIA_MAX_VIDEO_FORMATS			32
#endif

/**
 * Specify the maximum time difference (in ms) for synchronization between
 * two medias. If the synchronization media source is ahead of time
 * greater than this duration, it is considered to make a very large jump
 * and the synchronization will be reset.
 *
 * Default: 20000
 */
#ifndef PJMEDIA_CLOCK_SYNC_MAX_SYNC_MSEC
#   define PJMEDIA_CLOCK_SYNC_MAX_SYNC_MSEC         20000
#endif

/**
 * Maximum video frame size.
 * Default: 128kB
 */
#ifndef PJMEDIA_MAX_VIDEO_ENC_FRAME_SIZE
#  define PJMEDIA_MAX_VIDEO_ENC_FRAME_SIZE	    (1<<17)
#endif


/**
 * Specify the maximum duration (in ms) for resynchronization. When a media
 * is late to another media it is supposed to be synchronized to, it is
 * guaranteed to be synchronized again after this duration. While if the
 * media is ahead/early by t ms, it is guaranteed to be synchronized after
 * t + this duration. This timing only applies if there is no additional
 * resynchronization required during the specified duration.
 *
 * Default: 2000
 */
#ifndef PJMEDIA_CLOCK_SYNC_MAX_RESYNC_DURATION
#   define PJMEDIA_CLOCK_SYNC_MAX_RESYNC_DURATION 2000
#endif


/**
 * Minimum gap between two consecutive discards in jitter buffer,
 * in milliseconds.
 *
 * Default: 200 ms
 */
#ifndef PJMEDIA_JBUF_DISC_MIN_GAP
#   define PJMEDIA_JBUF_DISC_MIN_GAP		    200
#endif


/**
 * Minimum burst level reference used for calculating discard duration
 * in jitter buffer progressive discard algorithm, in frames.
 * 
 * Default: 1 frame
 */
#ifndef PJMEDIA_JBUF_PRO_DISC_MIN_BURST
#   define PJMEDIA_JBUF_PRO_DISC_MIN_BURST	    1
#endif


/**
 * Maximum burst level reference used for calculating discard duration
 * in jitter buffer progressive discard algorithm, in frames.
 * 
 * Default: 200 frames
 */
#ifndef PJMEDIA_JBUF_PRO_DISC_MAX_BURST
#   define PJMEDIA_JBUF_PRO_DISC_MAX_BURST	    100
#endif


/**
 * Duration for progressive discard algotithm in jitter buffer to discard
 * an excessive frame when burst is equal to or lower than
 * PJMEDIA_JBUF_PRO_DISC_MIN_BURST, in milliseconds.
 *
 * Default: 2000 ms
 */
#ifndef PJMEDIA_JBUF_PRO_DISC_T1
#   define PJMEDIA_JBUF_PRO_DISC_T1		    2000
#endif


/**
 * Duration for progressive discard algotithm in jitter buffer to discard
 * an excessive frame when burst is equal to or greater than
 * PJMEDIA_JBUF_PRO_DISC_MAX_BURST, in milliseconds.
 *
 * Default: 10000 ms
 */
#ifndef PJMEDIA_JBUF_PRO_DISC_T2
#   define PJMEDIA_JBUF_PRO_DISC_T2		    10000
#endif


/**
 * Reset jitter buffer and return silent audio on stream playback start
 * (first get_frame()). This is useful to avoid possible noise that may be
 * introduced by discard algorithm and neutralize latency when audio device
 * is started later than the stream.
 *
 * Set this to N>0 to allow N silent audio frames returned on stream playback
 * start, this will allow about N frames to be buffered in the jitter buffer
 * before the playback is started (prefetching effect).
 * Set this to zero to disable this feature.
 *
 * Default: 1
 */
#ifndef PJMEDIA_STREAM_SOFT_START
#   define PJMEDIA_STREAM_SOFT_START		    1
#endif


/**
 * Video stream will discard old picture from the jitter buffer as soon as
 * new picture is received, to reduce latency.
 *
 * Default: 0
 */
#ifndef PJMEDIA_VID_STREAM_SKIP_PACKETS_TO_REDUCE_LATENCY
#   define PJMEDIA_VID_STREAM_SKIP_PACKETS_TO_REDUCE_LATENCY	0
#endif


/**
 * Maximum video payload size. Note that this must not be greater than
 * PJMEDIA_MAX_MTU.
 *
 * Default: (PJMEDIA_MAX_MTU - 20 - (128+16)) if SRTP is enabled, 
 *	    otherwise (PJMEDIA_MAX_MTU - 20). 
 *          Note that (128+16) constant value is taken from libSRTP macro 
 *          SRTP_MAX_TRAILER_LEN.
 */
#ifndef PJMEDIA_MAX_VID_PAYLOAD_SIZE
#  if PJMEDIA_HAS_SRTP
#     define PJMEDIA_MAX_VID_PAYLOAD_SIZE     (PJMEDIA_MAX_MTU - 20 - (128+16))
#  else
#     define PJMEDIA_MAX_VID_PAYLOAD_SIZE     (PJMEDIA_MAX_MTU - 20)
#  endif
#endif


/**
 * Specify target value for socket receive buffer size. It will be
 * applied to RTP socket of media transport using setsockopt(). When
 * transport failed to set the specified size, it will try with lower
 * value until the highest possible is successfully set.
 *
 * Setting this to zero will leave the socket receive buffer size to
 * OS default (e.g: usually 8 KB on desktop platforms).
 *
 * Default: 64 KB when video is enabled, otherwise zero (OS default)
 */
#ifndef PJMEDIA_TRANSPORT_SO_RCVBUF_SIZE
#   if PJMEDIA_HAS_VIDEO
#	define PJMEDIA_TRANSPORT_SO_RCVBUF_SIZE	(64*1024)
#   else
#	define PJMEDIA_TRANSPORT_SO_RCVBUF_SIZE	0
#   endif
#endif


/**
 * Specify target value for socket send buffer size. It will be
 * applied to RTP socket of media transport using setsockopt(). When
 * transport failed to set the specified size, it will try with lower
 * value until the highest possible is successfully set.
 *
 * Setting this to zero will leave the socket send buffer size to
 * OS default (e.g: usually 8 KB on desktop platforms).
 *
 * Default: 64 KB when video is enabled, otherwise zero (OS default)
 */
#ifndef PJMEDIA_TRANSPORT_SO_SNDBUF_SIZE
#   if PJMEDIA_HAS_VIDEO
#	define PJMEDIA_TRANSPORT_SO_SNDBUF_SIZE	(64*1024)
#   else
#	define PJMEDIA_TRANSPORT_SO_SNDBUF_SIZE	0
#   endif
#endif


/**
 * Specify if libyuv is available.
 *
 * Default: 0 (disable)
 */
#ifndef PJMEDIA_HAS_LIBYUV
#   define PJMEDIA_HAS_LIBYUV				0
#endif


/**
 * Specify if dtmf flash in RFC 2833 is available.
 */
#ifndef PJMEDIA_HAS_DTMF_FLASH
#   define PJMEDIA_HAS_DTMF_FLASH			1
#endif


/**
 * Specify the number of keyframe needed to be sent after the stream is 
 * created. Setting this to 0 will disable it.
 *
 * Default : 5
 */
#ifndef PJMEDIA_VID_STREAM_START_KEYFRAME_CNT
#   define PJMEDIA_VID_STREAM_START_KEYFRAME_CNT	5
#endif


/**
 * Specify the interval to send keyframe after the stream is created, in msec.
 *
 * Default : 1000
 */
#ifndef PJMEDIA_VID_STREAM_START_KEYFRAME_INTERVAL_MSEC
#   define PJMEDIA_VID_STREAM_START_KEYFRAME_INTERVAL_MSEC  1000
#endif


/**
 * Specify the minimum interval to send video keyframe, in msec.
 *
 * Default : 1000
 */
#ifndef PJMEDIA_VID_STREAM_MIN_KEYFRAME_INTERVAL_MSEC
#   define PJMEDIA_VID_STREAM_MIN_KEYFRAME_INTERVAL_MSEC    1000
#endif


/**
 * Specify minimum delay of video decoding, in milliseconds. Lower value may
 * degrade video quality significantly in a bad network environment (e.g:
 * with persistent late and out-of-order RTP packets). Note that the value
 * must be lower than jitter buffer maximum delay (configurable via
 * pjmedia_stream_info.jb_max or pjsua_media_config.jb_max).
 *
 * Default : 100
 */
#ifndef PJMEDIA_VID_STREAM_DECODE_MIN_DELAY_MSEC
#   define PJMEDIA_VID_STREAM_DECODE_MIN_DELAY_MSEC	    100
#endif


/**
 * Perform RTP payload type checking in the video stream. Normally the peer
 * MUST send RTP with payload type as we specified in our SDP. Certain
 * agents may not be able to follow this hence the only way to have
 * communication is to disable this check.
 *
 * Default: PJMEDIA_STREAM_CHECK_RTP_PT (follow audio stream's setting)
 */
#ifndef PJMEDIA_VID_STREAM_CHECK_RTP_PT
#   define PJMEDIA_VID_STREAM_CHECK_RTP_PT	PJMEDIA_STREAM_CHECK_RTP_PT
#endif

/**
 * @}
 */


#endif	/* __PJMEDIA_CONFIG_H__ */


