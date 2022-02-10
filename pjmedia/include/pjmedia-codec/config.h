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
#ifndef __PJMEDIA_CODEC_CONFIG_H__
#define __PJMEDIA_CODEC_CONFIG_H__

/**
 * @file config.h
 * @brief PJMEDIA-CODEC compile time settings
 */

/**
 * @defgroup pjmedia_codec_config PJMEDIA-CODEC Compile Time Settings
 * @ingroup PJMEDIA_CODEC
 * @brief Various compile time settings such as to enable/disable codecs
 * @{
 */

#include <pjmedia/types.h>

/*
 * Include config_auto.h if autoconf is used (PJ_AUTOCONF is set)
 */
#if defined(PJ_AUTOCONF)
#   include <pjmedia-codec/config_auto.h>
#endif


/**
 * Unless specified otherwise, L16 codec is included by default.
 */
#ifndef PJMEDIA_HAS_L16_CODEC
#   define PJMEDIA_HAS_L16_CODEC    1
#endif


/**
 * Settings to enable L16 codec 8KHz, mono. By default it is disabled.
 */
#ifndef PJMEDIA_CODEC_L16_HAS_8KHZ_MONO
#   define PJMEDIA_CODEC_L16_HAS_8KHZ_MONO	0
#endif


/**
 * Settings to enable L16 codec 8KHz, stereo. By default it is disabled.
 */
#ifndef PJMEDIA_CODEC_L16_HAS_8KHZ_STEREO
#   define PJMEDIA_CODEC_L16_HAS_8KHZ_STEREO	0
#endif


/**
 * Settings to enable L16 codec 16KHz, mono. By default it is disabled.
 */
#ifndef PJMEDIA_CODEC_L16_HAS_16KHZ_MONO
#   define PJMEDIA_CODEC_L16_HAS_16KHZ_MONO	0
#endif


/**
 * Settings to enable L16 codec 16KHz, stereo. By default it is disabled.
 */
#ifndef PJMEDIA_CODEC_L16_HAS_16KHZ_STEREO
#   define PJMEDIA_CODEC_L16_HAS_16KHZ_STEREO	0
#endif


/**
 * Settings to enable L16 codec 48KHz, mono. By default it is disabled.
 */
#ifndef PJMEDIA_CODEC_L16_HAS_48KHZ_MONO
#   define PJMEDIA_CODEC_L16_HAS_48KHZ_MONO	0
#endif


/**
 * Settings to enable L16 codec 48KHz, stereo. By default it is disabled.
 */
#ifndef PJMEDIA_CODEC_L16_HAS_48KHZ_STEREO
#   define PJMEDIA_CODEC_L16_HAS_48KHZ_STEREO	0
#endif


/**
 * Unless specified otherwise, GSM codec is included by default.
 */
#ifndef PJMEDIA_HAS_GSM_CODEC
#   define PJMEDIA_HAS_GSM_CODEC    1
#endif


/**
 * Unless specified otherwise, Speex codec is included by default.
 */
#ifndef PJMEDIA_HAS_SPEEX_CODEC
#   define PJMEDIA_HAS_SPEEX_CODEC    1
#endif

/**
 * Speex codec default complexity setting.
 */
#ifndef PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY
#   define PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY   2
#endif

/**
 * Speex codec default quality setting. Please note that pjsua-lib may override
 * this setting via its codec quality setting (i.e PJSUA_DEFAULT_CODEC_QUALITY).
 */
#ifndef PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY
#   define PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY	    8
#endif


/**
 * Unless specified otherwise, iLBC codec is included by default.
 */
#ifndef PJMEDIA_HAS_ILBC_CODEC
#   define PJMEDIA_HAS_ILBC_CODEC    1
#endif


/**
 * Unless specified otherwise, G.722 codec is included by default.
 */
#ifndef PJMEDIA_HAS_G722_CODEC
#   define PJMEDIA_HAS_G722_CODEC    1
#endif

/**
 * Initial memory block for G.722 codec implementation.
 */
#ifndef PJMEDIA_POOL_LEN_G722_CODEC
#   define PJMEDIA_POOL_LEN_G722_CODEC  1000
#endif

/**
 * Memory increment for G.722 codec implementation.
 */
#ifndef PJMEDIA_POOL_INC_G722_CODEC
#   define PJMEDIA_POOL_INC_G722_CODEC  1000
#endif

/**
 * Default G.722 codec encoder and decoder level adjustment. The G.722
 * specifies that it uses 14 bit PCM for input and output, while PJMEDIA
 * normally uses 16 bit PCM, so the conversion is done by applying
 * level adjustment. If the value is non-zero, then PCM input samples to
 * the encoder will be shifted right by this value, and similarly PCM
 * output samples from the decoder will be shifted left by this value.
 *
 * This can be changed at run-time after initialization by calling
 * #pjmedia_codec_g722_set_pcm_shift().
 *
 * Default: 2.
 */
#ifndef PJMEDIA_G722_DEFAULT_PCM_SHIFT
#   define PJMEDIA_G722_DEFAULT_PCM_SHIFT	    2
#endif


/**
 * Specifies whether G.722 PCM shifting should be stopped when clipping
 * detected in the decoder. Enabling this feature can be useful when
 * talking to G.722 implementation that uses 16 bit PCM for G.722 input/
 * output (for any reason it seems to work) and the PCM shifting causes
 * audio clipping.
 *
 * See also #PJMEDIA_G722_DEFAULT_PCM_SHIFT.
 *
 * Default: enabled.
 */
#ifndef PJMEDIA_G722_STOP_PCM_SHIFT_ON_CLIPPING
#   define PJMEDIA_G722_STOP_PCM_SHIFT_ON_CLIPPING  1
#endif


/**
 * Enable the features provided by Intel IPP libraries, for example
 * codecs such as G.729, G.723.1, G.726, G.728, G.722.1, and AMR.
 *
 * By default this is disabled. Please follow the instructions in
 * http://trac.pjsip.org/repos/wiki/Intel_IPP_Codecs on how to setup
 * Intel IPP with PJMEDIA.
 */
#ifndef PJMEDIA_HAS_INTEL_IPP
#   define PJMEDIA_HAS_INTEL_IPP		0
#endif


/**
 * Visual Studio only: when this option is set, the Intel IPP libraries
 * will be automatically linked to application using pragma(comment)
 * constructs. This is convenient, however it will only link with
 * the stub libraries and the Intel IPP DLL's will be required when
 * distributing the application.
 *
 * If application wants to link with the different types of the Intel IPP
 * libraries (for example, the static libraries), it must set this option
 * to zero and specify the Intel IPP libraries in the application's input
 * library specification manually.
 *
 * Default 1.
 */
#ifndef PJMEDIA_AUTO_LINK_IPP_LIBS
#   define PJMEDIA_AUTO_LINK_IPP_LIBS		1
#endif


/**
 * Enable Intel IPP AMR codec. This also needs to be enabled when AMR WB
 * codec is enabled. This option is only used when PJMEDIA_HAS_INTEL_IPP 
 * is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_AMR
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_AMR	1
#endif


/**
 * Enable Intel IPP AMR wideband codec. The PJMEDIA_HAS_INTEL_IPP_CODEC_AMR
 * option must also be enabled to use this codec. This option is only used 
 * when PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_AMRWB
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_AMRWB	1
#endif


/**
 * Enable Intel IPP G.729 codec. This option is only used when
 * PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G729
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G729	1
#endif


/**
 * Enable Intel IPP G.723.1 codec. This option is only used when
 * PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G723_1
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G723_1	1
#endif


/**
 * Enable Intel IPP G.726 codec. This option is only used when
 * PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G726
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G726	1
#endif


/**
 * Enable Intel IPP G.728 codec. This option is only used when
 * PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G728
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G728	1
#endif


/**
 * Enable Intel IPP G.722.1 codec. This option is only used when
 * PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1	1
#endif

/**
 * Enable Passthrough codecs.
 *
 * Default: 0
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODECS
#   define PJMEDIA_HAS_PASSTHROUGH_CODECS	0
#endif

/**
 * Enable AMR passthrough codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR
#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR	1
#endif

/**
 * Enable G.729 passthrough codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODEC_G729
#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_G729	1
#endif

/**
 * Enable iLBC passthrough codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC
#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC	1
#endif

/**
 * Enable PCMU passthrough codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU
#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU	1
#endif

/**
 * Enable PCMA passthrough codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA
#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA	1
#endif

/* If passthrough and PCMU/PCMA are enabled, disable the software
 * G.711 codec
 */
#if PJMEDIA_HAS_PASSTHROUGH_CODECS && \
    (PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU || PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA)
#   undef PJMEDIA_HAS_G711_CODEC
#   define PJMEDIA_HAS_G711_CODEC		0
#endif


/**
 * G.722.1 codec is disabled by default.
 */
#ifndef PJMEDIA_HAS_G7221_CODEC
#   define PJMEDIA_HAS_G7221_CODEC		0
#endif

/**
 * Enable OpenCORE AMR-NB codec.
 * See https://trac.pjsip.org/repos/ticket/1388 for some info.
 *
 * Default: 0
 */
#ifndef PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
#   define PJMEDIA_HAS_OPENCORE_AMRNB_CODEC	0
#endif

/**
 * Enable OpenCORE AMR-WB codec.
 * See https://trac.pjsip.org/repos/ticket/1608 for some info.
 *
 * Default: 0
 */
#ifndef PJMEDIA_HAS_OPENCORE_AMRWB_CODEC
#   define PJMEDIA_HAS_OPENCORE_AMRWB_CODEC	0
#endif

/**
 * Link with libopencore-amrXX via pragma comment on Visual Studio.
 * This option only makes sense if PJMEDIA_HAS_OPENCORE_AMRNB/WB_CODEC
 * is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_AUTO_LINK_OPENCORE_AMR_LIBS
#  define PJMEDIA_AUTO_LINK_OPENCORE_AMR_LIBS	1
#endif

/**
 * Link with libopencore-amrXX.a that has been produced with gcc.
 * This option only makes sense if PJMEDIA_HAS_OPENCORE_AMRNB/WB_CODEC
 * and PJMEDIA_AUTO_LINK_OPENCORE_AMR_LIBS are enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_OPENCORE_AMR_BUILT_WITH_GCC
#   define PJMEDIA_OPENCORE_AMR_BUILT_WITH_GCC	1
#endif


/**
 * Default G.722.1 codec encoder and decoder level adjustment. 
 * If the value is non-zero, then PCM input samples to the encoder will 
 * be shifted right by this value, and similarly PCM output samples from
 * the decoder will be shifted left by this value.
 *
 * This can be changed at run-time after initialization by calling
 * #pjmedia_codec_g7221_set_pcm_shift().
 */
#ifndef PJMEDIA_G7221_DEFAULT_PCM_SHIFT
#   define PJMEDIA_G7221_DEFAULT_PCM_SHIFT	1
#endif


/**
 * Enabling both G.722.1 codec implementations, internal PJMEDIA and IPP,
 * may cause problem in SDP, i.e: payload types duplications. So, let's 
 * just trap such case here at compile time.
 *
 * Application can control which implementation to be used by manipulating
 * PJMEDIA_HAS_G7221_CODEC and PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1 in
 * config_site.h.
 */
#if (PJMEDIA_HAS_G7221_CODEC != 0) && (PJMEDIA_HAS_INTEL_IPP != 0) && \
    (PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1 != 0)
#   error Only one G.722.1 implementation can be enabled at the same time. \
	  Please use PJMEDIA_HAS_G7221_CODEC and \
	  PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1 in your config_site.h \
	  to control which implementation to be used.
#endif


/**
 * Enable SILK codec.
 *
 * Default: 0
 */
#ifndef PJMEDIA_HAS_SILK_CODEC
#   define PJMEDIA_HAS_SILK_CODEC		0
#endif


/**
 * SILK codec default complexity setting, valid values are 0 (lowest), 1,
 * and 2.
 *
 * Default: 2
 */
#ifndef PJMEDIA_CODEC_SILK_DEFAULT_COMPLEXITY
#   define PJMEDIA_CODEC_SILK_DEFAULT_COMPLEXITY   2
#endif

/**
 * SILK codec default quality setting, valid values are ranging from
 * 0 (lowest) to 10. Please note that pjsua-lib may override this setting
 * via its codec quality setting (i.e PJSUA_DEFAULT_CODEC_QUALITY).
 *
 * Default: 10
 */
#ifndef PJMEDIA_CODEC_SILK_DEFAULT_QUALITY
#   define PJMEDIA_CODEC_SILK_DEFAULT_QUALITY	    10
#endif


/**
 * Enable OPUS codec.
 *
 * Default: 0
 */
#ifndef PJMEDIA_HAS_OPUS_CODEC
#   define PJMEDIA_HAS_OPUS_CODEC			0
#endif

/**
 * OPUS codec sample rate.
 *
 * Default: 48000
 */
#ifndef PJMEDIA_CODEC_OPUS_DEFAULT_SAMPLE_RATE
#   define PJMEDIA_CODEC_OPUS_DEFAULT_SAMPLE_RATE  	48000
#endif

/**
 * OPUS codec default maximum average bit rate.
 *
 * Default: 0 (leave it to default value specified by Opus, which will
 * take into account factors such as media content (speech/music), sample
 * rate, channel count, etc).
 */
#ifndef PJMEDIA_CODEC_OPUS_DEFAULT_BIT_RATE
#   define PJMEDIA_CODEC_OPUS_DEFAULT_BIT_RATE  	0
#endif


/**
 * OPUS default encoding complexity, which is an integer from
 * 0 to 10, where 0 is the lowest complexity and 10 is the highest.
 *
 * Default: 5
 */
#ifndef PJMEDIA_CODEC_OPUS_DEFAULT_COMPLEXITY
#   define PJMEDIA_CODEC_OPUS_DEFAULT_COMPLEXITY 	5
#endif


/**
 * OPUS default CBR (constant bit rate) setting
 *
 * Default: PJ_FALSE (which means Opus will use VBR (variable bit rate))
 */
#ifndef PJMEDIA_CODEC_OPUS_DEFAULT_CBR
#   define PJMEDIA_CODEC_OPUS_DEFAULT_CBR 		PJ_FALSE
#endif


/**
 * Enable G.729 codec using BCG729 backend.
 *
 * Default: 0 
 */
#ifndef PJMEDIA_HAS_BCG729
#   define PJMEDIA_HAS_BCG729				0
#endif


/**
 * Specify if FFMPEG codecs are available.
 *
 * Default: PJMEDIA_HAS_LIBAVFORMAT
 */
#ifndef PJMEDIA_HAS_FFMPEG_CODEC
#   define PJMEDIA_HAS_FFMPEG_CODEC		PJMEDIA_HAS_LIBAVFORMAT
#endif


/**
 * Specify if FFMPEG video codecs are available.
 *
 * Default: PJMEDIA_HAS_FFMPEG_CODEC
 */
#ifndef PJMEDIA_HAS_FFMPEG_VID_CODEC
#   define PJMEDIA_HAS_FFMPEG_VID_CODEC		PJMEDIA_HAS_FFMPEG_CODEC
#endif

/**
 * Enable FFMPEG H263+/H263-1998 codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_FFMPEG_CODEC_H263P
#   define PJMEDIA_HAS_FFMPEG_CODEC_H263P	PJMEDIA_HAS_FFMPEG_VID_CODEC
#endif

/**
 * Enable FFMPEG H264 codec (requires libx264).
 *
 * Default: disabled when OpenH264 is used, otherwise it is set to
 * PJMEDIA_HAS_FFMPEG_VID_CODEC
 */
#ifndef PJMEDIA_HAS_FFMPEG_CODEC_H264
#   if defined(PJMEDIA_HAS_OPENH264_CODEC) && PJMEDIA_HAS_OPENH264_CODEC != 0
#	define PJMEDIA_HAS_FFMPEG_CODEC_H264	0
#   else
#	define PJMEDIA_HAS_FFMPEG_CODEC_H264	PJMEDIA_HAS_FFMPEG_VID_CODEC
#   endif
#endif

/**
 * Enable FFMPEG VPX codec (requires libvpx)
 */
#ifndef PJMEDIA_HAS_FFMPEG_CODEC_VP8
#   if defined(PJMEDIA_HAS_VPX_CODEC) && PJMEDIA_HAS_VPX_CODEC != 0
#	define PJMEDIA_HAS_FFMPEG_CODEC_VP8		0
#   else
#	define PJMEDIA_HAS_FFMPEG_CODEC_VP8		1
#   endif
#endif

#ifndef PJMEDIA_HAS_FFMPEG_CODEC_VP9
#   if defined(PJMEDIA_HAS_VPX_CODEC) && PJMEDIA_HAS_VPX_CODEC != 0
#	define PJMEDIA_HAS_FFMPEG_CODEC_VP9		0
#   else
#	define PJMEDIA_HAS_FFMPEG_CODEC_VP9		1
#   endif
#endif


/**
 * Determine the log level of the native openH264 log which will be forwarded
 * to the library's log.
 * Set to WELS_LOG_QUIET to disable logging, or WELS_LOG_DETAIL for debugging.
 *
 * Default: WELS_LOG_ERROR.
 */
#ifndef PJMEDIA_CODEC_OPENH264_LOG_LEVEL
#   define PJMEDIA_CODEC_OPENH264_LOG_LEVEL  WELS_LOG_ERROR
#endif

/**
 * Enable VPX VP8 codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_VPX_CODEC_VP8
#   define PJMEDIA_HAS_VPX_CODEC_VP8		1
#endif

/**
 * Enable VPX VP9 codec.
 *
 * Default: 0 (disabled)
 */
#ifndef PJMEDIA_HAS_VPX_CODEC_VP9
#   define PJMEDIA_HAS_VPX_CODEC_VP9		0
#endif

/**
 * Enable Android MediaCodec AMRNB codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_AND_MEDIA_AMRNB
#   define PJMEDIA_HAS_AND_MEDIA_AMRNB		1
#endif

/**
 * Enable Android MediaCodec AMRWB codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_AND_MEDIA_AMRWB
#   define PJMEDIA_HAS_AND_MEDIA_AMRWB		1
#endif

/**
 * Enable Android MediaCodec AVC/H264 codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_AND_MEDIA_H264
#   define PJMEDIA_HAS_AND_MEDIA_H264		1
#endif

/**
 * Enable Android MediaCodec VP8 codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_AND_MEDIA_VP8
#   define PJMEDIA_HAS_AND_MEDIA_VP8		1
#endif

/**
 * Enable Android MediaCodec VP9 codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_AND_MEDIA_VP9
#   define PJMEDIA_HAS_AND_MEDIA_VP9		1
#endif

/**
 * Prioritize to use software video encoder on Android MediaCodec.
 * Set to 0 to prioritize Hardware encoder.
 * Note: based on test, software encoder configuration provided the most stable
 * configuration.
 *
 * Default: 1
 */
#ifndef PJMEDIA_AND_MEDIA_PRIO_SW_VID_ENC
#    define PJMEDIA_AND_MEDIA_PRIO_SW_VID_ENC 	1
#endif

/**
 * Prioritize to use software video encoder on Android MediaCodec.
 * Set to 0 to prioritize Hardware encoder.
 * Note: based on test, software decoder configuration provided the most stable
 * configuration.
 *
 * Default: 1
 */
#ifndef PJMEDIA_AND_MEDIA_PRIO_SW_VID_DEC
#    define PJMEDIA_AND_MEDIA_PRIO_SW_VID_DEC 	1
#endif


/**
 * Maximum interval between keyframes for Apple VideoToolbox codecs,
 * in second.
 *
 * Default: 5 (seconds)
 */
#ifndef PJMEDIA_CODEC_VID_TOOLBOX_MAX_KEYFRAME_INTERVAL
#   define PJMEDIA_CODEC_VID_TOOLBOX_MAX_KEYFRAME_INTERVAL	5
#endif

/**
 * @}
 */



#endif	/* __PJMEDIA_CODEC_CONFIG_H__ */
