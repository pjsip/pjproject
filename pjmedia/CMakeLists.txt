# ##############################################################################
# Module options

include(Pj/Option)
include(CMakeDependentOption)

macro(import_if_or_set opt failure_value package)
  if(${opt})
    find_package(${package} ${ARGN})
    if(NOT ${package}_FOUND)
      pj_force_set(${opt} ${failure_value} "${package} was not found")
    endif()
  endif()
endmacro()

#
# General features
#

# Include sRTP (Secure Real-time Transport Protocol)
cmake_dependent_option(PJMEDIA_WITH_SRTP
  "Enable SRTP support" ON
  "TARGET Pj::Dep::SRTP" OFF
)

# Include resampling implementation
pj_option(PJMEDIA_WITH_RESAMPLE "Enable resampling implementation"
  REQUIRED
  DEFAULT libresample
  ALLOWED_VALUES none libresample libsamplerate speex
)

if(PJMEDIA_WITH_RESAMPLE STREQUAL "libresample")
  if(NOT TARGET Pj::Dep::Resample)
    pj_force_set(PJMEDIA_WITH_RESAMPLE "none" "libresample not found")
  endif()
elseif(PJMEDIA_WITH_RESAMPLE STREQUAL "libsamplerate")
  find_package(SampleRate REQUIRED)
elseif(PJMEDIA_WITH_RESAMPLE STREQUAL "speex")
  if(NOT TARGET Pj::Dep::Speex)
    message(FATAL_ERROR "Resampling backend cannot be set to `speex`, \
                         because Speex is not available")
  endif()
endif()

#
# Acoustic Echo Cancellation (AEC)
#

# Speex Acoustic Echo Cancellation (AEC)
cmake_dependent_option(PJMEDIA_WITH_SPEEX_AEC
  "Enable Speex Acoustic Echo Cancellation (AEC) support" ON
  "TARGET Pj::Dep::Speex" OFF
)
import_if_or_set(PJMEDIA_WITH_SPEEX_AEC OFF SpeexDSP)

# WebRTC Acoustic Echo Cancellation (AEC)
cmake_dependent_option(PJMEDIA_WITH_WEBRTC_AEC
  "Enable WebRTC Acoustic Echo Cancellation (AEC) support" ON
  "TARGET Pj::Dep::WebRTC" OFF
)

cmake_dependent_option(PJMEDIA_WITH_WEBRTC_AEC3
  "Enable WebRTC Acoustic Echo Cancellation 3 (WebRTC AEC3) support" ON
  "TARGET Pj::Dep::WebRTC_AEC3" OFF
)

#
# Video support
#

option(PJMEDIA_WITH_VIDEO "Enable video support" ON)

# libyuv
cmake_dependent_option(PJMEDIA_WITH_LIBYUV
  "Enable libyuv support" ON
  "PJMEDIA_WITH_VIDEO;TARGET Pj::Dep::YUV" OFF
)

# ffmpeg
cmake_dependent_option(PJMEDIA_WITH_FFMPEG
  "Enable FFMPEG support" ON
  "PJMEDIA_WITH_VIDEO" OFF
)

set(ffmpeg_libs_required avutil)
set(ffmpeg_libs_optional swscale avcodec avformat avdevice)

import_if_or_set(PJMEDIA_WITH_FFMPEG OFF
  FFMPEG
    COMPONENTS ${ffmpeg_libs_required}
    OPTIONAL_COMPONENTS ${ffmpeg_libs_optional}
)

foreach(lib IN LISTS ffmpeg_libs_required ffmpeg_libs_optional)
  string(TOUPPER "${lib}" lib_upper)
  set(lib_option PJMEDIA_WITH_FFMPEG_${lib_upper})

  cmake_dependent_option(${lib_option}
    "Enable FFMPEG `${lib}` component" ON
    "PJMEDIA_WITH_FFMPEG;TARGET FFMPEG::${lib}" OFF
  )
endforeach()

#
# Codecs
#

# G.711 PCMA/PCMU codecs
option(PJMEDIA_WITH_G711_CODEC "Enable G.711 codec" ON)

# Linear/L16 codec family
option(PJMEDIA_WITH_L16_CODEC "Enable L16 codec family" ON)

# GSM codec
cmake_dependent_option(PJMEDIA_WITH_GSM_CODEC
  "Enable GSM codec" ON
  "TARGET Pj::Dep::GSM" OFF
)

# Speex codec
cmake_dependent_option(PJMEDIA_WITH_SPEEX_CODEC
  "Enable Speex codecs" ON
  "TARGET Pj::Dep::Speex" OFF
)

# iLBC codec
cmake_dependent_option(PJMEDIA_WITH_ILBC_CODEC
  "Enable iLBC codec" ON
  "TARGET Pj::Dep::iLBC" OFF
)

# G.722 codec
option(PJMEDIA_WITH_G722_CODEC "Enable G.722 codec" ON)

# G.7221 codec
cmake_dependent_option(PJMEDIA_WITH_G7221_CODEC
  "Enable G.7221 codec" ON
  "TARGET Pj::Dep::G7221" OFF
)

# OpenCore AMR-NB Codecs
option(PJMEDIA_WITH_OPENCORE_AMRNB_CODEC "Enable OpenCore AMR-NB codec" ON)
import_if_or_set(PJMEDIA_WITH_OPENCORE_AMRNB_CODEC OFF OpenCoreAMRNB)

# OpenCore AMR-WB Codecs
option(PJMEDIA_WITH_OPENCORE_AMRWB_CODEC "Enable OpenCore AMR-WB codec" ON)
import_if_or_set(PJMEDIA_WITH_OPENCORE_AMRWB_CODEC OFF OpenCoreAMRWB)
import_if_or_set(PJMEDIA_WITH_OPENCORE_AMRWB_CODEC OFF VisualOnAMRWBEnc)

# Silk codec
option(PJMEDIA_WITH_SILK_CODEC "Enable Skype Silk" ON)
import_if_or_set(PJMEDIA_WITH_SILK_CODEC OFF Silk)

# OPUS codec
option(PJMEDIA_WITH_OPUS_CODEC "Enable OPUS codec" ON)
import_if_or_set(PJMEDIA_WITH_OPUS_CODEC OFF OPUS)

# BCG729 codec
option(PJMEDIA_WITH_BCG729_CODEC "Enable BCG729 codec" ON)
import_if_or_set(PJMEDIA_WITH_BCG729_CODEC OFF BCG729)

# Lyra codec
option(PJMEDIA_WITH_LYRA_CODEC "Enable Lyra codec" ON)
import_if_or_set(PJMEDIA_WITH_LYRA_CODEC OFF Lyra)

# Android MediaCodec codecs - Not supported yet
option(PJMEDIA_WITH_ANDROID_MEDIACODEC_CODEC "Enable Android MediaCodec codec")
mark_as_advanced(PJMEDIA_WITH_ANDROID_MEDIACODEC_CODEC)

# Google VP8/VP9 codecs
cmake_dependent_option(PJMEDIA_WITH_VPX_CODEC
  "Enable VPX codecs support" ON
  "PJMEDIA_WITH_VIDEO" OFF
)
import_if_or_set(PJMEDIA_WITH_VPX_CODEC OFF VPX)

# OpenH264 codecs
cmake_dependent_option(PJMEDIA_WITH_OPEN_H264_CODEC
  "Enable OpenH264 codecs support" ON
  "PJMEDIA_WITH_VIDEO" OFF
)
import_if_or_set(PJMEDIA_WITH_OPEN_H264_CODEC OFF OpenH264)

#
# Audio device
#

option(PJMEDIA_WITH_AUDIODEV "Enable audio device support" ON)

# NULL audio backend
cmake_dependent_option(PJMEDIA_WITH_AUDIODEV_NULL
  "Enable NULL audio device backend" ON
  "PJMEDIA_WITH_AUDIO_DEVICE" OFF
)

# Android Java Native Interface backend
cmake_dependent_option(PJMEDIA_WITH_AUDIODEV_JNI
  "Enable JNI audio device backend" ON
  "PJMEDIA_WITH_AUDIODEV;ANDROID" OFF
)

# Android OBOE backend
cmake_dependent_option(PJMEDIA_WITH_AUDIODEV_OBOE
  "Enable OBOE audio device backend" ON
  "PJMEDIA_WITH_AUDIODEV;ANDROID" OFF
)

# macOS/iOS CoreAudio backend
cmake_dependent_option(PJMEDIA_WITH_AUDIODEV_COREAUDIO
  "Enable CoreAudio audio device backend" ON
  "PJMEDIA_WITH_AUDIODEV;APPLE" OFF
)

# Advanced Linux Sound Architecture (ALSA) backend
cmake_dependent_option(PJMEDIA_WITH_AUDIODEV_ALSA
  "Enable ALSA audio device backend" ON
  "PJMEDIA_WITH_AUDIODEV;LINUX" OFF
)
import_if_or_set(PJMEDIA_WITH_AUDIODEV_ALSA OFF ALSA)

# Windows MultiMedia Extensions (WMME) backend
cmake_dependent_option(PJMEDIA_WITH_AUDIODEV_WMME
  "Enable WMME audio device backend" ON
  "PJMEDIA_WITH_AUDIODEV;WIN32 OR CYGWIN OR MINGW" OFF
)

# Windows Audio Session API (WASAPI) backend
cmake_dependent_option(PJMEDIA_WITH_AUDIODEV_WASAPI
  "Enable WASAPI audio device backend" ON
  "PJMEDIA_WITH_AUDIODEV;WIN32 OR CYGWIN OR MINGW" OFF
)
import_if_or_set(PJMEDIA_WITH_AUDIODEV_WASAPI OFF WASAPI)

#
# Video device
#

cmake_dependent_option(PJMEDIA_WITH_VIDEODEV
  "Enable video device support" ON
  "PJMEDIA_WITH_VIDEO" OFF
)

# AVI backend
cmake_dependent_option(PJMEDIA_WITH_VIDEODEV_AVI
  "Enable AVI video device backend" ON
  "PJMEDIA_WITH_VIDEODEV" OFF
)

# OpenGL backend
cmake_dependent_option(PJMEDIA_WITH_VIDEODEV_OPENGL
  "Enable OpenGL video device backend" ON
  "PJMEDIA_WITH_VIDEODEV;ANDROID OR IOS" OFF
)
import_if_or_set(PJMEDIA_WITH_VIDEODEV_OPENGL OFF OpenGL COMPONENTS GLES2)

# FFMPEG backend
cmake_dependent_option(PJMEDIA_WITH_VIDEODEV_FFMPEG
  "Enable FFMPEG video device backend" ON
  "PJMEDIA_WITH_VIDEODEV;PJMEDIA_WITH_FFMPEG_AVDEVICE" OFF
)

# SDL backend
cmake_dependent_option(PJMEDIA_WITH_VIDEODEV_SDL
  "Enable FFMPEG video device backend" ON
  "PJMEDIA_WITH_VIDEODEV" OFF
)
import_if_or_set(PJMEDIA_WITH_VIDEODEV_SDL OFF SDL2)

# Metal backend
cmake_dependent_option(PJMEDIA_WITH_VIDEODEV_METAL
  "Enable Metal video device backend" OFF
  "PJMEDIA_WITH_VIDEODEV;APPLE" OFF
)

# QT backend
cmake_dependent_option(PJMEDIA_WITH_VIDEODEV_QT
  "Enable QT video device backend" OFF
  "PJMEDIA_WITH_VIDEODEV;APPLE" OFF
)

# V4L2 backend
cmake_dependent_option(PJMEDIA_WITH_VIDEODEV_V4L2
  "Enable V4L2 video device backend" ON
  "PJMEDIA_WITH_VIDEODEV;LINUX" OFF
)
import_if_or_set(PJMEDIA_WITH_VIDEODEV_V4L2 OFF V4L2)

# DirectShow backend
cmake_dependent_option(PJMEDIA_WITH_VIDEODEV_DSHOW
  "Enable DirectShow video device backend" OFF
  "PJMEDIA_WITH_VIDEODEV;WIN32 OR CYGWIN OR MINGW" OFF
)

# ##############################################################################
# pjmedia

add_library(pjmedia
  src/pjmedia/alaw_ulaw.c
  src/pjmedia/alaw_ulaw_table.c
  src/pjmedia/avi_player.c
  src/pjmedia/avi_writer.c
  src/pjmedia/av_sync.c
  src/pjmedia/bidirectional.c
  src/pjmedia/clock_thread.c
  src/pjmedia/codec.c
  src/pjmedia/conference.c
  src/pjmedia/conf_switch.c
  src/pjmedia/converter.c
  src/pjmedia/converter_libswscale.c
  src/pjmedia/converter_libyuv.c
  src/pjmedia/delaybuf.c
  src/pjmedia/echo_common.c
  src/pjmedia/echo_port.c
  src/pjmedia/echo_speex.c
  src/pjmedia/echo_suppress.c
  src/pjmedia/echo_webrtc.c
  src/pjmedia/echo_webrtc_aec3.cpp
  src/pjmedia/endpoint.c
  src/pjmedia/errno.c
  src/pjmedia/event.c
  src/pjmedia/format.c
  src/pjmedia/ffmpeg_util.c
  src/pjmedia/g711.c
  src/pjmedia/jbuf.c
  src/pjmedia/master_port.c
  src/pjmedia/mem_capture.c
  src/pjmedia/mem_player.c
  src/pjmedia/null_port.c
  src/pjmedia/plc_common.c
  src/pjmedia/port.c
  src/pjmedia/splitcomb.c
  src/pjmedia/resample_resample.c
  src/pjmedia/resample_libsamplerate.c
  src/pjmedia/resample_speex.c
  src/pjmedia/resample_port.c
  src/pjmedia/rtcp.c
  src/pjmedia/rtcp_xr.c
  src/pjmedia/rtcp_fb.c
  src/pjmedia/rtp.c
  src/pjmedia/sdp.c
  src/pjmedia/sdp_cmp.c
  src/pjmedia/sdp_neg.c
  src/pjmedia/session.c
  src/pjmedia/silencedet.c
  src/pjmedia/sound_legacy.c
  src/pjmedia/sound_port.c
  src/pjmedia/stereo_port.c
  src/pjmedia/stream_common.c
  src/pjmedia/stream.c
  src/pjmedia/stream_info.c
  src/pjmedia/tonegen.c
  src/pjmedia/transport_adapter_sample.c
  src/pjmedia/transport_ice.c
  src/pjmedia/transport_loop.c
  src/pjmedia/transport_srtp.c
  src/pjmedia/transport_udp.c
  src/pjmedia/types.c
  src/pjmedia/txt_stream.c
  src/pjmedia/vid_codec.c
  src/pjmedia/vid_codec_util.c
  src/pjmedia/vid_port.c
  src/pjmedia/vid_stream.c
  src/pjmedia/vid_stream_info.c
  src/pjmedia/vid_conf.c
  src/pjmedia/wav_player.c
  src/pjmedia/wav_playlist.c
  src/pjmedia/wav_writer.c
  src/pjmedia/wave.c
  src/pjmedia/wsola.c
  src/pjmedia/audiodev.c
  src/pjmedia/videodev.c
)

# headers
target_sources(pjmedia
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      include
    FILES
      include/pjmedia.h
      include/pjmedia/alaw_ulaw.h
      include/pjmedia/audiodev.h
      include/pjmedia/av_sync.h
      include/pjmedia/avi.h
      include/pjmedia/avi_stream.h
      include/pjmedia/bidirectional.h
      include/pjmedia/circbuf.h
      include/pjmedia/clock.h
      include/pjmedia/codec.h
      include/pjmedia/conference.h
      include/pjmedia/config.h
      include/pjmedia/config_auto.h.cm
      include/pjmedia/config_auto.h.in
      include/pjmedia/converter.h
      include/pjmedia/delaybuf.h
      include/pjmedia/doxygen.h
      include/pjmedia/echo.h
      include/pjmedia/echo_port.h
      include/pjmedia/endpoint.h
      include/pjmedia/errno.h
      include/pjmedia/event.h
      include/pjmedia/format.h
      include/pjmedia/frame.h
      include/pjmedia/g711.h
      include/pjmedia/jbuf.h
      include/pjmedia/master_port.h
      include/pjmedia/mem_port.h
      include/pjmedia/null_port.h
      include/pjmedia/plc.h
      include/pjmedia/port.h
      include/pjmedia/resample.h
      include/pjmedia/rtcp.h
      include/pjmedia/rtcp_fb.h
      include/pjmedia/rtcp_xr.h
      include/pjmedia/rtp.h
      include/pjmedia/sdp.h
      include/pjmedia/sdp_neg.h
      include/pjmedia/session.h
      include/pjmedia/signatures.h
      include/pjmedia/silencedet.h
      include/pjmedia/sound.h
      include/pjmedia/sound_port.h
      include/pjmedia/splitcomb.h
      include/pjmedia/stereo.h
      include/pjmedia/stream.h
      include/pjmedia/stream_common.h
      include/pjmedia/symbian_sound_aps.h
      include/pjmedia/tonegen.h
      include/pjmedia/transport.h
      include/pjmedia/transport_adapter_sample.h
      include/pjmedia/transport_ice.h
      include/pjmedia/transport_loop.h
      include/pjmedia/transport_srtp.h
      include/pjmedia/transport_udp.h
      include/pjmedia/txt_stream.h
      include/pjmedia/types.h
      include/pjmedia/vid_codec.h
      include/pjmedia/vid_codec_util.h
      include/pjmedia/vid_conf.h
      include/pjmedia/vid_port.h
      include/pjmedia/vid_stream.h
      include/pjmedia/vid_tee.h
      include/pjmedia/videodev.h
      include/pjmedia/wav_playlist.h
      include/pjmedia/wav_port.h
      include/pjmedia/wave.h
      include/pjmedia/wsola.h
)

#
# pjmedia - configuration
#

# compile definitions
target_compile_definitions(pjmedia
  PUBLIC
    # srtp
    PJMEDIA_HAS_SRTP=$<BOOL:${PJMEDIA_WITH_SRTP}>

    # resampling backend
    PJMEDIA_RESAMPLE_IMP=PJMEDIA_RESAMPLE_$<UPPER_CASE:${PJMEDIA_WITH_RESAMPLE}>

    # aec
    PJMEDIA_HAS_SPEEX_AEC=$<BOOL:${PJMEDIA_WITH_SPEEX_AEC}>
    PJMEDIA_HAS_LIBWEBRTC=$<BOOL:${PJMEDIA_WITH_WEBRTC_AEC}>
    PJMEDIA_WEBRTC_AEC_USE_MOBILE=$<OR:$<BOOL:${ANDROID}>,$<BOOL:${WINDOWS_PHONE}>>
    PJMEDIA_HAS_LIBWEBRTC_AEC3=$<BOOL:${PJMEDIA_WITH_WEBRTC_AEC3}>

    # video
    PJMEDIA_HAS_VIDEO=$<BOOL:${PJMEDIA_WITH_VIDEO}>
    PJMEDIA_HAS_LIBYUV=$<BOOL:${PJMEDIA_WITH_LIBYUV}>
    PJMEDIA_HAS_FFMPEG=$<BOOL:${PJMEDIA_WITH_FFMPEG}>
    PJMEDIA_HAS_LIBAVUTIL=$<BOOL:${PJMEDIA_WITH_FFMPEG_AVUTIL}>
    PJMEDIA_HAS_LIBSWSCALE=$<BOOL:${PJMEDIA_WITH_FFMPEG_SWSCALE}>
    PJMEDIA_HAS_LIBAVCODEC=$<BOOL:${PJMEDIA_WITH_FFMPEG_AVCODEC}>
    PJMEDIA_HAS_LIBAVFORMAT=$<BOOL:${PJMEDIA_WITH_FFMPEG_AVFORMAT}>
    PJMEDIA_HAS_LIBAVDEVICE=$<BOOL:${PJMEDIA_WITH_FFMPEG_AVDEVICE}>
)

# external srtp values
if(PJMEDIA_WITH_SRTP)
  get_target_property(srtp_provider Pj::Dep::SRTP PJ_DEP_PROVIDER)
  if(srtp_provider STREQUAL "system")
    get_target_property(srtp_version_major Pj::Dep::SRTP SRTP_VERSION_MAJOR)
    get_target_property(srtp_has_deinit Pj::Dep::SRTP SRTP_HAS_DEINIT)
    get_target_property(srtp_has_shutdown Pj::Dep::SRTP SRTP_HAS_SHUTDOWN)

    target_compile_definitions(pjmedia
      PRIVATE
        PJMEDIA_EXTERNAL_SRTP=${srtp_version_major}
        PJMEDIA_SRTP_HAS_DEINIT=${srtp_has_deinit}
        PJMEDIA_SRTP_HAS_SHUTDOWN=${srtp_has_shutdown}
    )
  endif()
endif()

# definitions header
block(SCOPE_FOR VARIABLES)
  # G711 codec
  set(PJMEDIA_HAS_G711_CODEC ${PJMEDIA_WITH_G711_CODEC})

  # write configured header
  configure_file(
    include/pjmedia/config_auto.h.cm
    include/pjmedia/config_auto.h
  )
endblock()

# add generated file to headers file set
target_sources(pjmedia
  PUBLIC
    FILE_SET config_headers TYPE HEADERS
    BASE_DIRS
      "${CMAKE_CURRENT_BINARY_DIR}/include"
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/include/pjmedia/config_auto.h"
)

#
# pjmedia - dependencies
#

target_link_libraries(pjmedia
  PRIVATE
    # project
    pjlib pjlib-ssl pjlib-util pjnath

    # srtp
    $<$<BOOL:${PJMEDIA_WITH_SRTP}>:Pj::Dep::SRTP>

    # resampling backend
    $<$<STREQUAL:${PJMEDIA_WITH_RESAMPLE},libresample>:Pj::Dep::Resample>
    $<$<STREQUAL:${PJMEDIA_WITH_RESAMPLE},libsamplerate>:SampleRate::SampleRate>
    $<$<STREQUAL:${PJMEDIA_WITH_RESAMPLE},speex>:Pj::Dep::Speex>

    # aec
    $<$<BOOL:${PJMEDIA_WITH_SPEEX_AEC}>:Pj::Dep::Speex;SpeexDSP::SpeexDSP>
    $<$<BOOL:${PJMEDIA_WITH_WEBRTC_AEC}>:Pj::Dep::WebRTC>
    $<$<BOOL:${PJMEDIA_WITH_WEBRTC_AEC3}>:Pj::Dep::WebRTC_AEC3>

    # video
    $<$<BOOL:${PJMEDIA_WITH_LIBYUV}>:Pj::Dep::YUV>
    $<$<BOOL:${PJMEDIA_WITH_FFMPEG_AVUTIL}>:FFMPEG::avutil>
    $<$<BOOL:${PJMEDIA_WITH_FFMPEG_SWSCALE}>:FFMPEG::swscale>
    $<$<BOOL:${PJMEDIA_WITH_FFMPEG_AVCODEC}>:FFMPEG::avcodec>
    $<$<BOOL:${PJMEDIA_WITH_FFMPEG_AVFORMAT}>:FFMPEG::avformat>
    $<$<BOOL:${PJMEDIA_WITH_FFMPEG_AVDEVICE}>:FFMPEG::avdevice>
)

if (NOT (WIN32 OR CYGWIN OR MINGW))
  # math library
  find_library(MATH_LIBRARY m)
  if(MATH_LIBRARY)
    target_link_libraries(pjmedia PRIVATE "${MATH_LIBRARY}")
  endif()
endif()

# ##############################################################################
# pjmedia-codec

add_library(pjmedia-codec
  src/pjmedia-codec/amr_sdp_match.c
  src/pjmedia-codec/audio_codecs.c
  src/pjmedia-codec/bcg729.c
  src/pjmedia-codec/ffmpeg_vid_codecs.c
  src/pjmedia-codec/g722.c
  src/pjmedia-codec/g722/g722_enc.c
  src/pjmedia-codec/g722/g722_dec.c
  src/pjmedia-codec/g7221.c
  src/pjmedia-codec/g7221_sdp_match.c
  src/pjmedia-codec/gsm.c
  src/pjmedia-codec/h263_packetizer.c
  src/pjmedia-codec/h264_packetizer.c
  src/pjmedia-codec/ilbc.c
  src/pjmedia-codec/l16.c
  src/pjmedia-codec/lyra.cpp
  src/pjmedia-codec/opencore_amr.c
  src/pjmedia-codec/speex_codec.c
  src/pjmedia-codec/openh264.cpp
  src/pjmedia-codec/opus.c
  src/pjmedia-codec/silk.c
  src/pjmedia-codec/vpx_packetizer.c
  src/pjmedia-codec/vpx.c
)

# headers
target_sources(pjmedia-codec
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS
        include
      FILES
        include/pjmedia-codec.h
        include/pjmedia-codec/amr_helper.h
        include/pjmedia-codec/amr_sdp_match.h
        include/pjmedia-codec/and_aud_mediacodec.h
        include/pjmedia-codec/and_vid_mediacodec.h
        include/pjmedia-codec/audio_codecs.h
        include/pjmedia-codec/bcg729.h
        include/pjmedia-codec/config.h
        include/pjmedia-codec/config_auto.h.cm
        include/pjmedia-codec/config_auto.h.in
        include/pjmedia-codec/ffmpeg_vid_codecs.h
        include/pjmedia-codec/g722.h
        include/pjmedia-codec/g7221.h
        include/pjmedia-codec/g7221_sdp_match.h
        include/pjmedia-codec/gsm.h
        include/pjmedia-codec/h263_packetizer.h
        include/pjmedia-codec/h264_packetizer.h
        include/pjmedia-codec/ilbc.h
        include/pjmedia-codec/ipp_codecs.h
        include/pjmedia-codec/l16.h
        include/pjmedia-codec/lyra.h
        include/pjmedia-codec/opencore_amr.h
        include/pjmedia-codec/openh264.h
        include/pjmedia-codec/opus.h
        include/pjmedia-codec/passthrough.h
        include/pjmedia-codec/silk.h
        include/pjmedia-codec/speex.h
        include/pjmedia-codec/types.h
        include/pjmedia-codec/vid_toolbox.h
        include/pjmedia-codec/vpx.h
        include/pjmedia-codec/vpx_packetizer.h
)

#
# pjmedia-codec - configuration
#

# lyra codec models path
if(PJMEDIA_WITH_LYRA)
  get_target_property(lyra_model_path Lyra::Lyra LYRA_PREFIX)
  if(lyra_model_path)
    cmake_path(APPEND lyra_model_path "model_coeffs")
    target_compile_definitions(pjmedia-codec
      PRIVATE
        "PJMEDIA_CODEC_LYRA_DEFAULT_MODEL_PATH=${lyra_model_path}"
    )
  endif()
endif()

# definitions header
block(SCOPE_FOR VARIABLES)
  # audio codecs
  set(PJMEDIA_HAS_L16_CODEC ${PJMEDIA_WITH_L16_CODEC})
  set(PJMEDIA_HAS_GSM_CODEC ${PJMEDIA_WITH_GSM_CODEC})
  set(PJMEDIA_HAS_SPEEX_CODEC ${PJMEDIA_WITH_SPEEX_CODEC})
  set(PJMEDIA_HAS_ILBC_CODEC ${PJMEDIA_WITH_ILBC_CODEC})
  set(PJMEDIA_HAS_G722_CODEC ${PJMEDIA_WITH_G722_CODEC})
  set(PJMEDIA_HAS_G7221_CODEC ${PJMEDIA_WITH_G7221_CODEC})
  set(PJMEDIA_HAS_OPENCORE_AMRNB_CODEC ${PJMEDIA_WITH_OPENCORE_AMRNB_CODEC})
  set(PJMEDIA_HAS_OPENCORE_AMRWB_CODEC ${PJMEDIA_WITH_OPENCORE_AMRWB_CODEC})
  set(PJMEDIA_HAS_SILK_CODEC ${PJMEDIA_WITH_SILK_CODEC})
  set(PJMEDIA_HAS_OPUS_CODEC ${PJMEDIA_WITH_OPUS_CODEC})
  set(PJMEDIA_HAS_BCG729 ${PJMEDIA_WITH_BCG729_CODEC})
  set(PJMEDIA_HAS_LYRA_CODEC ${PJMEDIA_WITH_LYRA_CODEC})

  # Always treat `GSM` as external (even if bundled is used), to
  # prevent using relative path include.
  set(PJMEDIA_EXTERNAL_GSM_GSM_H 0)
  set(PJMEDIA_EXTERNAL_GSM_H 1)

  # video codecs
  set(PJMEDIA_HAS_VPX_CODEC ${PJMEDIA_WITH_VPX_CODEC})
  set(PJMEDIA_HAS_OPENH264_CODEC ${PJMEDIA_WITH_OPEN_H264_CODEC})

  # write configured header
  configure_file(
    include/pjmedia-codec/config_auto.h.cm
    include/pjmedia-codec/config_auto.h
  )
endblock()

# add generated file to headers file set
target_sources(pjmedia-codec
  PUBLIC
    FILE_SET config_headers TYPE HEADERS
    BASE_DIRS
      "${CMAKE_CURRENT_BINARY_DIR}/include"
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/include/pjmedia-codec/config_auto.h"
)

#
# pjmedia-codec - dependencies
#

target_link_libraries(pjmedia-codec
  PRIVATE
    # project
    pjlib pjlib-util pjmedia

    # audio codecs
    $<$<BOOL:${PJMEDIA_WITH_GSM_CODEC}>:Pj::Dep::GSM>
    $<$<BOOL:${PJMEDIA_WITH_SPEEX_CODEC}>:Pj::Dep::Speex>
    $<$<BOOL:${PJMEDIA_WITH_ILBC_CODEC}>:Pj::Dep::iLBC>
    $<$<BOOL:${PJMEDIA_WITH_G7221_CODEC}>:Pj::Dep::G7221>
    $<$<BOOL:${PJMEDIA_WITH_OPENCORE_AMRNB_CODEC}>:OpenCoreAMRNB::OpenCoreAMRNB>
    $<$<BOOL:${PJMEDIA_WITH_OPENCORE_AMRWB_CODEC}>:OpenCoreAMRWB::OpenCoreAMRWB>
    $<$<BOOL:${PJMEDIA_WITH_OPENCORE_AMRWB_CODEC}>:VisualOnAMRWBEnc::VisualOnAMRWBEnc>
    $<$<BOOL:${PJMEDIA_WITH_SILK_CODEC}>:Silk::Silk>
    $<$<BOOL:${PJMEDIA_WITH_OPUS_CODEC}>:OPUS::OPUS>
    $<$<BOOL:${PJMEDIA_WITH_BCG729_CODEC}>:BCG729::BCG729>
    $<$<BOOL:${PJMEDIA_WITH_LYRA_CODEC}>:Lyra::Lyra>

    # video codecs
    $<$<BOOL:${PJMEDIA_WITH_VPX_CODEC}>:VPX::VPX>
    $<$<BOOL:${PJMEDIA_WITH_OPEN_H264_CODEC}>:OpenH264::OpenH264>
    $<$<BOOL:${PJMEDIA_HAS_LIBAVFORMAT}>:FFMPEG::avformat>
)

# ##############################################################################
# pjmedia-audiodev

add_library(pjmedia-audiodev
  src/pjmedia-audiodev/alsa_dev.c
  src/pjmedia-audiodev/audiodev.c
  src/pjmedia-audiodev/audiotest.c
  src/pjmedia-audiodev/android_jni_dev.c
  src/pjmedia-audiodev/errno.c
  src/pjmedia-audiodev/null_dev.c
  src/pjmedia-audiodev/oboe_dev.cpp
  src/pjmedia-audiodev/wasapi_dev.cpp
  src/pjmedia-audiodev/wmme_dev.c

  $<$<BOOL:${APPLE}>:src/pjmedia-audiodev/coreaudio_dev.m>
)

# headers
target_sources(pjmedia-audiodev
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      include
    FILES
      include/pjmedia_audiodev.h
      include/pjmedia-audiodev/alsa.h
      include/pjmedia-audiodev/audiodev.h
      include/pjmedia-audiodev/audiodev_imp.h
      include/pjmedia-audiodev/audiotest.h
      include/pjmedia-audiodev/config.h
      include/pjmedia-audiodev/errno.h
)

#
# pjmedia-audiodev - configuration
#

# compile definitions
target_compile_definitions(pjmedia-audiodev
  PUBLIC
    PJMEDIA_AUDIO_DEV_HAS_NULL_AUDIO=$<BOOL:${PJMEDIA_WITH_AUDIODEV_NULL}>
    PJMEDIA_AUDIO_DEV_HAS_ANDROID_JNI=$<BOOL:${PJMEDIA_WITH_AUDIODEV_JNI}>
    PJMEDIA_AUDIO_DEV_HAS_OBOE=$<BOOL:${PJMEDIA_AUDIO_DEV_HAS_OBOE}>
    PJMEDIA_AUDIO_DEV_HAS_COREAUDIO=$<BOOL:${PJMEDIA_WITH_AUDIODEV_COREAUDIO}>
    PJMEDIA_AUDIO_DEV_HAS_ALSA=$<BOOL:${PJMEDIA_WITH_AUDIODEV_ALSA}>
    PJMEDIA_AUDIO_DEV_HAS_WMME=$<BOOL:${PJMEDIA_WITH_AUDIODEV_WMME}>
    PJMEDIA_AUDIO_DEV_HAS_WASAPI=$<BOOL:${PJMEDIA_WITH_AUDIODEV_WASAPI}>

    # unsupported backends
    PJMEDIA_AUDIO_DEV_HAS_BDIMAD=0
    PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO=0
    PJMEDIA_AUDIO_DEV_HAS_OPENSL=0
    PJMEDIA_AUDIO_DEV_HAS_BB10=0
    PJMEDIA_AUDIO_DEV_HAS_SYMB_APS=0
    PJMEDIA_AUDIO_DEV_HAS_LEGACY_DEVICE=0
)

#
# pjmedia-audiodev - dependencies
#

target_link_libraries(pjmedia-audiodev
  PRIVATE
    # project
    pjlib pjlib-util pjmedia

    # audio backends
    $<$<BOOL:${PJMEDIA_WITH_AUDIODEV_ALSA}>:ALSA::ALSA>
    # TODO: add remaining APIs' dependencies
)

# ##############################################################################
#  pjmedia-videodev

add_library(pjmedia-videodev
  src/pjmedia-videodev/avi_dev.c
  src/pjmedia-videodev/android_opengl.c
  src/pjmedia-videodev/colorbar_dev.c
  src/pjmedia-videodev/dshow_dev.c
  src/pjmedia-videodev/dshowclasses.cpp
  src/pjmedia-videodev/errno.c
  src/pjmedia-videodev/ffmpeg_dev.c
  src/pjmedia-videodev/opengl_dev.c
  src/pjmedia-videodev/util.c
  src/pjmedia-videodev/v4l2_dev.c
  src/pjmedia-videodev/videodev.c

  $<$<BOOL:${IOS}>:src/pjmedia-videodev/ios_opengl_dev.m>
  $<$<BOOL:${APPLE}>:src/pjmedia-videodev/sdl_dev_m.m>
  $<$<BOOL:${APPLE}>:src/pjmedia-videodev/metal_dev.m>
  $<$<BOOL:${APPLE}>:src/pjmedia-videodev/qt_dev.m>

  $<$<NOT:$<BOOL:${APPLE}>>:src/pjmedia-videodev/sdl_dev.c>
)

# headers
target_sources(pjmedia-videodev
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS
        include
      FILES
        include/pjmedia_videodev.h
        include/pjmedia-videodev/avi_dev.h
        include/pjmedia-videodev/config.h
        include/pjmedia-videodev/errno.h
        include/pjmedia-videodev/opengl_dev.h
        include/pjmedia-videodev/videodev.h
        include/pjmedia-videodev/videodev_imp.h
)

#
# pjmedia-videodev - configuration
#

# compile definitions
target_compile_definitions(pjmedia-videodev
  PUBLIC
    PJMEDIA_VIDEO_DEV_HAS_AVI=$<BOOL:${PJMEDIA_WITH_VIDEODEV_AVI}>
    PJMEDIA_VIDEO_DEV_HAS_OPENGL=$<BOOL:${PJMEDIA_WITH_VIDEODEV_OPENGL}>
    PJMEDIA_VIDEO_DEV_HAS_OPENGL_ES=$<BOOL:${PJMEDIA_WITH_VIDEODEV_OPENGL}>
    PJMEDIA_VIDEO_DEV_HAS_FFMPEG=$<BOOL:${PJMEDIA_WITH_VIDEODEV_FFMPEG}>
    PJMEDIA_VIDEO_DEV_HAS_SDL=$<BOOL:${PJMEDIA_WITH_VIDEODEV_SDL}>
    PJMEDIA_VIDEO_DEV_HAS_METAL=$<BOOL:${PJMEDIA_WITH_VIDEODEV_METAL}>
    PJMEDIA_VIDEO_DEV_HAS_QT=$<BOOL:${PJMEDIA_WITH_VIDEODEV_QT}>
    PJMEDIA_VIDEO_DEV_HAS_V4L2=$<BOOL:${PJMEDIA_WITH_VIDEODEV_V4L2}>
    PJMEDIA_VIDEO_DEV_HAS_DSHOW=$<BOOL:${PJMEDIA_WITH_VIDEODEV_DSHOW}>
)

# os-specific opengl
if(PJMEDIA_WITH_VIDEODEV_OPENGL)
  target_compile_definitions(pjmedia-videodev
    PUBLIC
      PJMEDIA_VIDEO_DEV_HAS_ANDROID_OPENGL=$<BOOL:${ANDROID}>
      PJMEDIA_VIDEO_DEV_HAS_IOS_OPENGL=$<BOOL:${IOS}>
  )
endif()

#
# pjmedia-videodev - dependencies
#

target_link_libraries(pjmedia-videodev
  PRIVATE
    # project
    pjlib pjlib-util pjmedia

    # video backends
    $<$<BOOL:${PJMEDIA_WITH_VIDEODEV_OPENGL}>:OpenGL::GL;OpenGL::GLES2>
    $<$<BOOL:${PJMEDIA_WITH_VIDEODEV_FFMPEG}>:FFMPEG::avdevice>
    $<$<BOOL:${PJMEDIA_WITH_VIDEODEV_SDL}>:SDL2::SDL2>
    $<$<BOOL:${PJMEDIA_WITH_VIDEODEV_V4L2}>:V4L2::V4L2>
    # TODO: add remaining APIs' dependencies

    # transitive deps
    $<$<BOOL:${PJMEDIA_WITH_LIBYUV}>:Pj::Dep::YUV>
)

# ##############################################################################
#  Test application

if(BIULD_TESTING)
  add_executable(pjmedia-test
    src/test/codec_vectors.c
    src/test/jbuf_test.c
    src/test/main.c
    src/test/mips_test.c
    src/test/vid_codec_test.c
    src/test/vid_dev_test.c
    src/test/vid_port_test.c
    src/test/rtp_test.c
    src/test/test.c
    src/test/sdp_neg_test.c
  )

  target_link_libraries(pjmedia-test
    PRIVATE
      pjlib
      pjlib-util
      pjnath
      pjmedia
      pjmedia-codec
      pjmedia-audiodev
      pjmedia-videodev
  )

  add_test(NAME pjmedia-test COMMAND pjmedia-test)
endif()
