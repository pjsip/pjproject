# ##############################################################################
# Target
# ##############################################################################

add_library(webrtc_aec3
  src/absl/types/bad_optional_access.cc
  src/common_audio/audio_util.cc
  src/common_audio/third_party/ooura/fft_size_128/ooura_fft.cc
  src/common_audio/third_party/ooura/fft_size_256/fft4g.cc
  src/common_audio/resampler/push_resampler.cc
  src/common_audio/resampler/push_sinc_resampler.cc
  src/common_audio/resampler/sinc_resampler.cc
  src/common_audio/signal_processing/splitting_filter2.c
  src/api/audio/channel_layout.cc
  src/api/audio/echo_canceller3_config.cc
  src/api/audio/echo_canceller3_factory.cc
  src/modules/audio_processing/audio_buffer.cc
  src/modules/audio_processing/gain_controller2.cc
  src/modules/audio_processing/high_pass_filter.cc
  src/modules/audio_processing/splitting_filter.cc
  src/modules/audio_processing/three_band_filter_bank.cc
  src/modules/audio_processing/aec3/adaptive_fir_filter_erl.cc
  src/modules/audio_processing/aec3/adaptive_fir_filter.cc
  src/modules/audio_processing/aec3/aec_state.cc
  src/modules/audio_processing/aec3/aec3_common.cc
  src/modules/audio_processing/aec3/aec3_fft.cc
  src/modules/audio_processing/aec3/alignment_mixer.cc
  src/modules/audio_processing/aec3/api_call_jitter_metrics.cc
  src/modules/audio_processing/aec3/block_framer.cc
  src/modules/audio_processing/aec3/block_delay_buffer.cc
  src/modules/audio_processing/aec3/block_buffer.cc
  src/modules/audio_processing/aec3/block_processor_metrics.cc
  src/modules/audio_processing/aec3/block_processor.cc
  src/modules/audio_processing/aec3/clockdrift_detector.cc
  src/modules/audio_processing/aec3/coarse_filter_update_gain.cc
  src/modules/audio_processing/aec3/comfort_noise_generator.cc
  src/modules/audio_processing/aec3/decimator.cc
  src/modules/audio_processing/aec3/dominant_nearend_detector.cc
  src/modules/audio_processing/aec3/downsampled_render_buffer.cc
  src/modules/audio_processing/aec3/echo_audibility.cc
  src/modules/audio_processing/aec3/echo_canceller3.cc
  src/modules/audio_processing/aec3/echo_path_delay_estimator.cc
  src/modules/audio_processing/aec3/echo_path_variability.cc
  src/modules/audio_processing/aec3/echo_remover_metrics.cc
  src/modules/audio_processing/aec3/echo_remover.cc
  src/modules/audio_processing/aec3/erl_estimator.cc
  src/modules/audio_processing/aec3/erle_estimator.cc
  src/modules/audio_processing/aec3/fft_buffer.cc
  src/modules/audio_processing/aec3/filter_analyzer.cc
  src/modules/audio_processing/aec3/frame_blocker.cc
  src/modules/audio_processing/aec3/fullband_erle_estimator.cc
  src/modules/audio_processing/aec3/matched_filter_lag_aggregator.cc
  src/modules/audio_processing/aec3/matched_filter.cc
  src/modules/audio_processing/aec3/moving_average.cc
  src/modules/audio_processing/aec3/refined_filter_update_gain.cc
  src/modules/audio_processing/aec3/render_buffer.cc
  src/modules/audio_processing/aec3/render_delay_buffer.cc
  src/modules/audio_processing/aec3/render_delay_controller_metrics.cc
  src/modules/audio_processing/aec3/render_delay_controller.cc
  src/modules/audio_processing/aec3/render_signal_analyzer.cc
  src/modules/audio_processing/aec3/residual_echo_estimator.cc
  src/modules/audio_processing/aec3/reverb_decay_estimator.cc
  src/modules/audio_processing/aec3/reverb_frequency_response.cc
  src/modules/audio_processing/aec3/reverb_model_estimator.cc
  src/modules/audio_processing/aec3/reverb_model.cc
  src/modules/audio_processing/aec3/signal_dependent_erle_estimator.cc
  src/modules/audio_processing/aec3/spectrum_buffer.cc
  src/modules/audio_processing/aec3/stationarity_estimator.cc
  src/modules/audio_processing/aec3/subband_erle_estimator.cc
  src/modules/audio_processing/aec3/subband_nearend_detector.cc
  src/modules/audio_processing/aec3/subtractor_output_analyzer.cc
  src/modules/audio_processing/aec3/subtractor_output.cc
  src/modules/audio_processing/aec3/subtractor.cc
  src/modules/audio_processing/aec3/suppression_filter.cc
  src/modules/audio_processing/aec3/suppression_gain.cc
  src/modules/audio_processing/aec3/transparent_mode.cc
  src/modules/audio_processing/agc2/adaptive_agc.cc
  src/modules/audio_processing/agc2/adaptive_digital_gain_applier.cc
  src/modules/audio_processing/agc2/adaptive_mode_level_estimator.cc
  src/modules/audio_processing/agc2/biquad_filter.cc
  src/modules/audio_processing/agc2/cpu_features.cc
  src/modules/audio_processing/agc2/down_sampler.cc
  src/modules/audio_processing/agc2/fixed_digital_level_estimator.cc
  src/modules/audio_processing/agc2/gain_applier.cc
  src/modules/audio_processing/agc2/interpolated_gain_curve.cc
  src/modules/audio_processing/agc2/limiter_db_gain_curve.cc
  src/modules/audio_processing/agc2/limiter.cc
  src/modules/audio_processing/agc2/noise_level_estimator.cc
  src/modules/audio_processing/agc2/noise_spectrum_estimator.cc
  src/modules/audio_processing/agc2/saturation_protector_buffer.cc
  src/modules/audio_processing/agc2/saturation_protector.cc
  src/modules/audio_processing/agc2/signal_classifier.cc
  src/modules/audio_processing/agc2/vad_with_level.cc
  src/modules/audio_processing/agc2/vector_float_frame.cc
  src/modules/audio_processing/agc2/rnn_vad/auto_correlation.cc
  src/modules/audio_processing/agc2/rnn_vad/features_extraction.cc
  src/modules/audio_processing/agc2/rnn_vad/lp_residual.cc
  src/modules/audio_processing/agc2/rnn_vad/rnn.cc
  src/modules/audio_processing/agc2/rnn_vad/rnn_fc.cc
  src/modules/audio_processing/agc2/rnn_vad/rnn_gru.cc
  src/modules/audio_processing/agc2/rnn_vad/pitch_search_internal.cc
  src/modules/audio_processing/agc2/rnn_vad/pitch_search.cc
  src/modules/audio_processing/agc2/rnn_vad/spectral_features_internal.cc
  src/modules/audio_processing/agc2/rnn_vad/spectral_features.cc
  src/modules/audio_processing/ns/fast_math.cc
  src/modules/audio_processing/ns/histograms.cc
  src/modules/audio_processing/ns/noise_estimator.cc
  src/modules/audio_processing/ns/noise_suppressor.cc
  src/modules/audio_processing/ns/ns_fft.cc
  src/modules/audio_processing/ns/prior_signal_model_estimator.cc
  src/modules/audio_processing/ns/prior_signal_model.cc
  src/modules/audio_processing/ns/quantile_noise_estimator.cc
  src/modules/audio_processing/ns/signal_model_estimator.cc
  src/modules/audio_processing/ns/signal_model.cc
  src/modules/audio_processing/ns/speech_probability_estimator.cc
  src/modules/audio_processing/ns/suppression_params.cc
  src/modules/audio_processing/ns/wiener_filter.cc
  src/modules/audio_processing/logging/apm_data_dumper.cc
  src/modules/audio_processing/utility/cascaded_biquad_filter.cc
  src/modules/audio_processing/utility/delay_estimator_wrapper.cc
  src/modules/audio_processing/utility/delay_estimator.cc
  src/modules/audio_processing/utility/pffft_wrapper.cc
  src/rtc_base/checks.cc
  src/rtc_base/logging.cc
  src/rtc_base/platform_thread_types.cc
  src/rtc_base/race_checker.cc
  src/rtc_base/string_encode.cc
  src/rtc_base/string_to_number.cc
  src/rtc_base/string_utils.cc
  src/rtc_base/system_time.cc
  src/rtc_base/time_utils.cc
  src/rtc_base/experiments/field_trial_parser.cc
  src/rtc_base/memory/aligned_malloc.cc
  src/rtc_base/strings/string_builder.cc
  src/rtc_base/synchronization/mutex.cc
  src/rtc_base/synchronization/yield.cc
  src/rtc_base/system/file_wrapper.cc
  src/system_wrappers/source/cpu_features2.cc
  src/system_wrappers/source/field_trial.cc
  src/system_wrappers/source/metrics.cc
  src/third_party/rnnoise/src/rnn_vad_weights.cc
  src/third_party/pffft/src/pffft.c
)

target_include_directories(webrtc_aec3
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:src>
)

set_target_properties(webrtc_aec3 PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

include(DetectWebRTCParameters)
target_link_libraries(webrtc_aec3 PUBLIC WebRTC::Options)

# Include SIMD source files
get_target_property(_arch_simd WebRTC::Options WEBRTC_ARCH_SIMD)
if(_arch_simd STREQUAL "neon")
  target_sources(webrtc_aec3
    PRIVATE
      src/common_audio/resampler/sinc_resampler_neon.cc
      src/common_audio/third_party/ooura/fft_size_128/ooura_fft_neon.cc
  )
elseif(_arch_simd STREQUAL "sse2")
  target_sources(webrtc_aec3
    PRIVATE
      src/common_audio/resampler/sinc_resampler_sse.cc
      src/common_audio/third_party/ooura/fft_size_128/ooura_fft_sse2.cc
      src/common_audio/resampler/sinc_resampler_avx2.cc
      src/modules/audio_processing/aec3/adaptive_fir_filter_erl_avx2.cc
      src/modules/audio_processing/aec3/adaptive_fir_filter_avx2.cc
      src/modules/audio_processing/aec3/fft_data_avx2.cc
      src/modules/audio_processing/aec3/matched_filter_avx2.cc
      src/modules/audio_processing/aec3/vector_math_avx2.cc
      src/modules/audio_processing/agc2/rnn_vad/rnn_vector_math_avx2.cc
  )
endif()

# Suppress `-Wreturn-type`
target_compile_options(webrtc_aec3 PRIVATE -Wno-return-type)
