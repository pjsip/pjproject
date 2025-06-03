# ##############################################################################
# Target
# ##############################################################################

add_library(webrtc
  src/webrtc/modules/audio_processing/aec/aec_core.c
  src/webrtc/modules/audio_processing/aec/aec_rdft.c
  src/webrtc/modules/audio_processing/aec/aec_resampler.c
  src/webrtc/modules/audio_processing/aec/echo_cancellation.c
  src/webrtc/modules/audio_processing/aecm/aecm_core.c
  src/webrtc/modules/audio_processing/aecm/echo_control_mobile.c
  src/webrtc/modules/audio_processing/ns/noise_suppression.c
  src/webrtc/modules/audio_processing/ns/noise_suppression_x.c
  src/webrtc/modules/audio_processing/ns/ns_core.c
  src/webrtc/modules/audio_processing/ns/nsx_core.c
  src/webrtc/modules/audio_processing/utility/delay_estimator_wrapper.c
  src/webrtc/modules/audio_processing/utility/delay_estimator.c
  src/webrtc/common_audio/fft4g.c
  src/webrtc/common_audio/ring_buffer.c
  src/webrtc/common_audio/signal_processing/complex_bit_reverse.c
  src/webrtc/common_audio/signal_processing/complex_fft.c
  src/webrtc/common_audio/signal_processing/copy_set_operations.c
  src/webrtc/common_audio/signal_processing/cross_correlation.c
  src/webrtc/common_audio/signal_processing/division_operations.c
  src/webrtc/common_audio/signal_processing/downsample_fast.c
  src/webrtc/common_audio/signal_processing/energy.c
  src/webrtc/common_audio/signal_processing/get_scaling_square.c
  src/webrtc/common_audio/signal_processing/min_max_operations.c
  src/webrtc/common_audio/signal_processing/randomization_functions.c
  src/webrtc/common_audio/signal_processing/real_fft.c
  src/webrtc/common_audio/signal_processing/spl_init.c
  src/webrtc/common_audio/signal_processing/spl_sqrt.c
  src/webrtc/common_audio/signal_processing/spl_sqrt_floor.c
  src/webrtc/common_audio/signal_processing/vector_scaling_operations.c
)

target_include_directories(webrtc
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:src>
)

include(DetectWebRTCParameters)
target_link_libraries(webrtc PUBLIC WebRTC::Options)

# Include SIMD source files
get_target_property(_arch_simd WebRTC::Options WEBRTC_ARCH_SIMD)
if(_arch_simd STREQUAL "neon")
  target_sources(webrtc
    PRIVATE
      src/webrtc/modules/audio_processing/aec/aec_core_neon.c
      src/webrtc/modules/audio_processing/aec/aec_rdft_neon.c
      src/webrtc/modules/audio_processing/aecm/aecm_core_c.c
      src/webrtc/modules/audio_processing/aecm/aecm_core_neon.c
      src/webrtc/modules/audio_processing/ns/nsx_core_c.c
      src/webrtc/modules/audio_processing/ns/nsx_core_neon.c
      src/webrtc/common_audio/signal_processing/cross_correlation_neon.c
      src/webrtc/common_audio/signal_processing/downsample_fast_neon.c
      src/webrtc/common_audio/signal_processing/min_max_operations_neon.c
  )
  target_compile_definitions(webrtc PRIVATE WEBRTC_HAS_NEON)
elseif(_arch_simd STREQUAL "sse2")
  target_sources(webrtc
    PRIVATE
      src/webrtc/modules/audio_processing/aec/aec_core_sse2.c
      src/webrtc/modules/audio_processing/aec/aec_rdft_sse2.c
      src/webrtc/modules/audio_processing/aecm/aecm_core_c.c
      src/webrtc/modules/audio_processing/ns/nsx_core_c.c
      src/webrtc/system_wrappers/source/cpu_features.cc
  )
elseif(_arch_simd STREQUAL "mips")
  target_sources(webrtc
    PRIVATE
      src/webrtc/modules/audio_processing/aec/aec_core_mips.c
      src/webrtc/modules/audio_processing/aec/aec_rdft_mips.c
      src/webrtc/modules/audio_processing/aecm/aecm_core_mips.c
      src/webrtc/modules/audio_processing/ns/nsx_core_mips.c
      src/webrtc/common_audio/signal_processing/cross_correlation_mips.c
      src/webrtc/common_audio/signal_processing/downsample_fast_mips.c
      src/webrtc/common_audio/signal_processing/min_max_operations_mips.c
  )
  target_compile_definitions(webrtc PRIVATE MIPS_FPU_LE)
else()
  target_sources(webrtc
    PRIVATE
      src/webrtc/modules/audio_processing/aecm/aecm_core_c.c
      src/webrtc/modules/audio_processing/ns/nsx_core_c.c
      src/webrtc/common_audio/signal_processing/complex_fft.c
    )
endif()

