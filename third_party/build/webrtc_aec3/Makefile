include ../../../build.mak
include ../../../build/common.mak
include ../os-$(OS_NAME).mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export WEBRTC_AEC3_LIB := libwebrtc-aec3-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export WEBRTC_AEC3_SONAME := libwebrtc-aec3.$(SHLIB_SUFFIX)
export WEBRTC_AEC3_SHLIB := $(WEBRTC_AEC3_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../webrtc_aec3/src  \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS) $(_CFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export WEBRTC_AEC3_SRCDIR = ../../webrtc_aec3/src/
export WEBRTC_AEC3_OBJS = \
	absl/types/bad_optional_access.o \
	common_audio/audio_util.o \
	common_audio/third_party/ooura/fft_size_128/ooura_fft.o \
	common_audio/third_party/ooura/fft_size_256/fft4g.o \
	common_audio/resampler/push_resampler.o \
	common_audio/resampler/push_sinc_resampler.o \
	common_audio/resampler/sinc_resampler.o \
	common_audio/signal_processing/splitting_filter2.o \
	api/audio/channel_layout.o \
	api/audio/echo_canceller3_config.o \
	api/audio/echo_canceller3_factory.o \
	api/audio/echo_detector_creator.o \
	modules/audio_processing/audio_buffer.o \
	modules/audio_processing/gain_controller2.o \
	modules/audio_processing/high_pass_filter.o \
	modules/audio_processing/splitting_filter.o \
	modules/audio_processing/three_band_filter_bank.o \
	modules/audio_processing/aec3/adaptive_fir_filter_erl.o \
	modules/audio_processing/aec3/adaptive_fir_filter.o \
	modules/audio_processing/aec3/aec_state.o \
	modules/audio_processing/aec3/aec3_common.o \
	modules/audio_processing/aec3/aec3_fft.o \
	modules/audio_processing/aec3/alignment_mixer.o \
	modules/audio_processing/aec3/api_call_jitter_metrics.o \
	modules/audio_processing/aec3/block_framer.o \
	modules/audio_processing/aec3/block_delay_buffer.o \
	modules/audio_processing/aec3/block_buffer.o \
	modules/audio_processing/aec3/block_processor_metrics.o \
	modules/audio_processing/aec3/block_processor.o \
	modules/audio_processing/aec3/clockdrift_detector.o \
	modules/audio_processing/aec3/coarse_filter_update_gain.o \
	modules/audio_processing/aec3/comfort_noise_generator.o \
	modules/audio_processing/aec3/decimator.o \
	modules/audio_processing/aec3/dominant_nearend_detector.o \
	modules/audio_processing/aec3/downsampled_render_buffer.o \
	modules/audio_processing/aec3/echo_audibility.o \
	modules/audio_processing/aec3/echo_canceller3.o \
	modules/audio_processing/aec3/echo_path_delay_estimator.o \
	modules/audio_processing/aec3/echo_path_variability.o \
	modules/audio_processing/aec3/echo_remover_metrics.o \
	modules/audio_processing/aec3/echo_remover.o \
	modules/audio_processing/aec3/erl_estimator.o \
	modules/audio_processing/aec3/erle_estimator.o \
	modules/audio_processing/aec3/fft_buffer.o \
	modules/audio_processing/aec3/filter_analyzer.o \
	modules/audio_processing/aec3/frame_blocker.o \
	modules/audio_processing/aec3/fullband_erle_estimator.o \
	modules/audio_processing/aec3/matched_filter_lag_aggregator.o \
	modules/audio_processing/aec3/matched_filter.o \
	modules/audio_processing/aec3/moving_average.o \
	modules/audio_processing/aec3/refined_filter_update_gain.o \
	modules/audio_processing/aec3/render_buffer.o \
	modules/audio_processing/aec3/render_delay_buffer.o \
	modules/audio_processing/aec3/render_delay_controller_metrics.o \
	modules/audio_processing/aec3/render_delay_controller.o \
	modules/audio_processing/aec3/render_signal_analyzer.o \
	modules/audio_processing/aec3/residual_echo_estimator.o \
	modules/audio_processing/aec3/reverb_decay_estimator.o \
	modules/audio_processing/aec3/reverb_frequency_response.o \
	modules/audio_processing/aec3/reverb_model_estimator.o \
	modules/audio_processing/aec3/reverb_model.o \
	modules/audio_processing/aec3/signal_dependent_erle_estimator.o \
	modules/audio_processing/aec3/spectrum_buffer.o \
	modules/audio_processing/aec3/stationarity_estimator.o \
	modules/audio_processing/aec3/subband_erle_estimator.o \
	modules/audio_processing/aec3/subband_nearend_detector.o \
	modules/audio_processing/aec3/subtractor_output_analyzer.o \
	modules/audio_processing/aec3/subtractor_output.o \
	modules/audio_processing/aec3/subtractor.o \
	modules/audio_processing/aec3/suppression_filter.o \
	modules/audio_processing/aec3/suppression_gain.o \
	modules/audio_processing/aec3/transparent_mode.o \
	modules/audio_processing/agc2/adaptive_agc.o \
	modules/audio_processing/agc2/adaptive_digital_gain_applier.o \
	modules/audio_processing/agc2/adaptive_mode_level_estimator.o \
	modules/audio_processing/agc2/biquad_filter.o \
	modules/audio_processing/agc2/compute_interpolated_gain_curve.o \
	modules/audio_processing/agc2/cpu_features.o \
	modules/audio_processing/agc2/down_sampler.o \
	modules/audio_processing/agc2/fixed_digital_level_estimator.o \
	modules/audio_processing/agc2/gain_applier.o \
	modules/audio_processing/agc2/interpolated_gain_curve.o \
	modules/audio_processing/agc2/limiter_db_gain_curve.o \
	modules/audio_processing/agc2/limiter.o \
	modules/audio_processing/agc2/noise_level_estimator.o \
	modules/audio_processing/agc2/noise_spectrum_estimator.o \
	modules/audio_processing/agc2/saturation_protector_buffer.o \
	modules/audio_processing/agc2/saturation_protector.o \
	modules/audio_processing/agc2/signal_classifier.o \
	modules/audio_processing/agc2/vad_with_level.o \
	modules/audio_processing/agc2/vector_float_frame.o \
	modules/audio_processing/agc2/rnn_vad/auto_correlation.o \
	modules/audio_processing/agc2/rnn_vad/features_extraction.o \
	modules/audio_processing/agc2/rnn_vad/lp_residual.o \
	modules/audio_processing/agc2/rnn_vad/rnn.o \
	modules/audio_processing/agc2/rnn_vad/rnn_fc.o \
	modules/audio_processing/agc2/rnn_vad/rnn_gru.o \
	modules/audio_processing/agc2/rnn_vad/pitch_search_internal.o \
	modules/audio_processing/agc2/rnn_vad/pitch_search.o \
	modules/audio_processing/agc2/rnn_vad/spectral_features_internal.o \
	modules/audio_processing/agc2/rnn_vad/spectral_features.o \
	modules/audio_processing/ns/fast_math.o \
	modules/audio_processing/ns/histograms.o \
	modules/audio_processing/ns/noise_estimator.o \
	modules/audio_processing/ns/noise_suppressor.o \
	modules/audio_processing/ns/ns_fft.o \
	modules/audio_processing/ns/prior_signal_model_estimator.o \
	modules/audio_processing/ns/prior_signal_model.o \
	modules/audio_processing/ns/quantile_noise_estimator.o \
	modules/audio_processing/ns/signal_model_estimator.o \
	modules/audio_processing/ns/signal_model.o \
	modules/audio_processing/ns/speech_probability_estimator.o \
	modules/audio_processing/ns/suppression_params.o \
	modules/audio_processing/ns/wiener_filter.o \
	modules/audio_processing/logging/apm_data_dumper.o \
	modules/audio_processing/utility/cascaded_biquad_filter.o \
	modules/audio_processing/utility/delay_estimator_wrapper.o \
	modules/audio_processing/utility/delay_estimator.o \
	modules/audio_processing/utility/pffft_wrapper.o \
	rtc_base/checks.o \
	rtc_base/logging.o \
	rtc_base/platform_thread_types.o \
	rtc_base/race_checker.o \
	rtc_base/string_encode.o \
	rtc_base/string_to_number.o \
	rtc_base/string_utils.o \
	rtc_base/system_time.o \
	rtc_base/time_utils.o \
	rtc_base/experiments/field_trial_parser.o \
	rtc_base/memory/aligned_malloc.o \
	rtc_base/strings/string_builder.o \
	rtc_base/synchronization/mutex.o \
	rtc_base/synchronization/yield.o \
	rtc_base/system/file_wrapper.o \
	system_wrappers/source/cpu_features2.o \
	system_wrappers/source/field_trial.o \
	system_wrappers/source/metrics.o \
	third_party/rnnoise/src/rnn_vad_weights.o \
	third_party/pffft/src/pffft.o \
	$(WEBRTC_AEC3_SRC)

export WEBRTC_AEC3_CFLAGS = $(_CFLAGS) $(WEBRTC_AEC3_OTHER_CFLAGS)
export WEBRTC_AEC3_CXXFLAGS = $(WEBRTC_AEC3_CFLAGS) $(_CXXFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := $(WEBRTC_AEC3_LIB) $(WEBRTC_AEC3_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(WEBRTC_AEC3_LIB) $(WEBRTC_AEC3_SONAME)

libwebrtc-aec3: $(WEBRTC_AEC3_LIB)
$(WEBRTC_AEC3_SONAME): $(WEBRTC_AEC3_LIB)
$(WEBRTC_AEC3_LIB) $(WEBRTC_AEC3_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=WEBRTC_AEC3 app=libwebrtc-aec3 $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=WEBRTC_AEC3 app=libwebrtc-aec3 $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.webrtc-aec3-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=WEBRTC_AEC3 app=libwebrtc-aec3 $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=WEBRTC_AEC3 app=libwebrtc-aec3 $@
