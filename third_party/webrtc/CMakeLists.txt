# ##############################################################################
# Target

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

include(DetectWebRTCParameters)
target_compile_options(webrtc PRIVATE ${WEBRTC_COMPILE_OPTIONS})
target_compile_definitions(webrtc PRIVATE ${WEBRTC_COMPILE_DEFINITIONS})

# include SIMD source files
if(WEBRTC_ARCH_SIMD STREQUAL "neon")
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
elseif(WEBRTC_ARCH_SIMD STREQUAL "sse2")
  target_sources(webrtc
    PRIVATE
      src/webrtc/modules/audio_processing/aec/aec_core_sse2.c
      src/webrtc/modules/audio_processing/aec/aec_rdft_sse2.c
      src/webrtc/modules/audio_processing/aecm/aecm_core_c.c
      src/webrtc/modules/audio_processing/ns/nsx_core_c.c
      src/webrtc/system_wrappers/source/cpu_features.cc
  )
elseif(WEBRTC_ARCH_SIMD STREQUAL "mips")
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
else()
  target_sources(webrtc
    PRIVATE
      src/webrtc/modules/audio_processing/aecm/aecm_core_c.c
      src/webrtc/modules/audio_processing/ns/nsx_core_c.c
      src/webrtc/common_audio/signal_processing/complex_fft.c
    )
endif()

# headers
target_sources(webrtc
  PUBLIC
    FILE_SET HEADERS
      BASE_DIRS
        src
      FILES
        src/pj_config.h
        src/webrtc/common_audio/fft4g.h
        src/webrtc/common_audio/ring_buffer.h
        src/webrtc/common_audio/signal_processing/complex_fft_tables.h
        src/webrtc/common_audio/signal_processing/include/real_fft.h
        src/webrtc/common_audio/signal_processing/include/signal_processing_library.h
        src/webrtc/common_audio/signal_processing/include/spl_inl.h
        src/webrtc/common_audio/signal_processing/include/spl_inl_armv7.h
        src/webrtc/common_audio/signal_processing/include/spl_inl_mips.h
        src/webrtc/common_audio/signal_processing/resample_by_2_internal.h
        src/webrtc/common_audio/wav_file.h
        src/webrtc/modules/audio_processing/aec/aec_common.h
        src/webrtc/modules/audio_processing/aec/aec_core.h
        src/webrtc/modules/audio_processing/aec/aec_core_internal.h
        src/webrtc/modules/audio_processing/aec/aec_rdft.h
        src/webrtc/modules/audio_processing/aec/aec_resampler.h
        src/webrtc/modules/audio_processing/aec/echo_cancellation_internal.h
        src/webrtc/modules/audio_processing/aec/include/echo_cancellation.h
        src/webrtc/modules/audio_processing/aecm/aecm_core.h
        src/webrtc/modules/audio_processing/aecm/aecm_defines.h
        src/webrtc/modules/audio_processing/aecm/include/echo_control_mobile.h
        src/webrtc/modules/audio_processing/logging/aec_logging.h
        src/webrtc/modules/audio_processing/logging/aec_logging_file_handling.h
        src/webrtc/modules/audio_processing/ns/defines.h
        src/webrtc/modules/audio_processing/ns/include/noise_suppression.h
        src/webrtc/modules/audio_processing/ns/include/noise_suppression_x.h
        src/webrtc/modules/audio_processing/ns/ns_core.h
        src/webrtc/modules/audio_processing/ns/nsx_core.h
        src/webrtc/modules/audio_processing/ns/nsx_defines.h
        src/webrtc/modules/audio_processing/ns/windows_private.h
        src/webrtc/modules/audio_processing/utility/delay_estimator.h
        src/webrtc/modules/audio_processing/utility/delay_estimator_internal.h
        src/webrtc/modules/audio_processing/utility/delay_estimator_wrapper.h
        src/webrtc/system_wrappers/interface/compile_assert_c.h
        src/webrtc/system_wrappers/interface/cpu_features_wrapper.h
        src/webrtc/typedefs.h
)

# ##############################################################################
# Dependencies

if (NOT (WIN32 OR CYGWIN OR MIGW))
  # math library
  find_library(MATH_LIBRARY m REQUIRED)
  target_link_libraries(webrtc PRIVATE "${MATH_LIBRARY}")
endif()
