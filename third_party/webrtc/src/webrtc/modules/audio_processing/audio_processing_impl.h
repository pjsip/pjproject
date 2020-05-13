/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_

#include <list>
#include <string>
#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class AgcManagerDirect;
class AudioBuffer;
class AudioConverter;

template<typename T>
class Beamformer;

class CriticalSectionWrapper;
class EchoCancellationImpl;
class EchoControlMobileImpl;
class FileWrapper;
class GainControlImpl;
class GainControlForNewAgc;
class HighPassFilterImpl;
class LevelEstimatorImpl;
class NoiseSuppressionImpl;
class ProcessingComponent;
class TransientSuppressor;
class VoiceDetectionImpl;
class IntelligibilityEnhancer;

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
namespace audioproc {

class Event;

}  // namespace audioproc
#endif

class AudioProcessingImpl : public AudioProcessing {
 public:
  explicit AudioProcessingImpl(const Config& config);

  // AudioProcessingImpl takes ownership of beamformer.
  AudioProcessingImpl(const Config& config, Beamformer<float>* beamformer);
  virtual ~AudioProcessingImpl();

  // AudioProcessing methods.
  int Initialize() override;
  int Initialize(int input_sample_rate_hz,
                 int output_sample_rate_hz,
                 int reverse_sample_rate_hz,
                 ChannelLayout input_layout,
                 ChannelLayout output_layout,
                 ChannelLayout reverse_layout) override;
  int Initialize(const ProcessingConfig& processing_config) override;
  void SetExtraOptions(const Config& config) override;
  int proc_sample_rate_hz() const override;
  int proc_split_sample_rate_hz() const override;
  int num_input_channels() const override;
  int num_output_channels() const override;
  int num_reverse_channels() const override;
  void set_output_will_be_muted(bool muted) override;
  int ProcessStream(AudioFrame* frame) override;
  int ProcessStream(const float* const* src,
                    size_t samples_per_channel,
                    int input_sample_rate_hz,
                    ChannelLayout input_layout,
                    int output_sample_rate_hz,
                    ChannelLayout output_layout,
                    float* const* dest) override;
  int ProcessStream(const float* const* src,
                    const StreamConfig& input_config,
                    const StreamConfig& output_config,
                    float* const* dest) override;
  int AnalyzeReverseStream(AudioFrame* frame) override;
  int ProcessReverseStream(AudioFrame* frame) override;
  int AnalyzeReverseStream(const float* const* data,
                           size_t samples_per_channel,
                           int sample_rate_hz,
                           ChannelLayout layout) override;
  int ProcessReverseStream(const float* const* src,
                           const StreamConfig& reverse_input_config,
                           const StreamConfig& reverse_output_config,
                           float* const* dest) override;
  int set_stream_delay_ms(int delay) override;
  int stream_delay_ms() const override;
  bool was_stream_delay_set() const override;
  void set_delay_offset_ms(int offset) override;
  int delay_offset_ms() const override;
  void set_stream_key_pressed(bool key_pressed) override;
  int StartDebugRecording(const char filename[kMaxFilenameSize]) override;
  int StartDebugRecording(FILE* handle) override;
  int StartDebugRecordingForPlatformFile(rtc::PlatformFile handle) override;
  int StopDebugRecording() override;
  void UpdateHistogramsOnCallEnd() override;
  EchoCancellation* echo_cancellation() const override;
  EchoControlMobile* echo_control_mobile() const override;
  GainControl* gain_control() const override;
  HighPassFilter* high_pass_filter() const override;
  LevelEstimator* level_estimator() const override;
  NoiseSuppression* noise_suppression() const override;
  VoiceDetection* voice_detection() const override;

 protected:
  // Overridden in a mock.
  virtual int InitializeLocked() EXCLUSIVE_LOCKS_REQUIRED(crit_);

 private:
  int InitializeLocked(const ProcessingConfig& config)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);
  int MaybeInitializeLocked(const ProcessingConfig& config)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);
  // TODO(ekm): Remove once all clients updated to new interface.
  int AnalyzeReverseStream(const float* const* src,
                           const StreamConfig& input_config,
                           const StreamConfig& output_config);
  int ProcessStreamLocked() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  int ProcessReverseStreamLocked() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  bool is_data_processed() const;
  bool output_copy_needed(bool is_data_processed) const;
  bool synthesis_needed(bool is_data_processed) const;
  bool analysis_needed(bool is_data_processed) const;
  bool is_rev_processed() const;
  bool rev_conversion_needed() const;
  void InitializeExperimentalAgc() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  void InitializeTransient() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  void InitializeBeamformer() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  void InitializeIntelligibility() EXCLUSIVE_LOCKS_REQUIRED(crit_);
  void MaybeUpdateHistograms() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  EchoCancellationImpl* echo_cancellation_;
  EchoControlMobileImpl* echo_control_mobile_;
  GainControlImpl* gain_control_;
  HighPassFilterImpl* high_pass_filter_;
  LevelEstimatorImpl* level_estimator_;
  NoiseSuppressionImpl* noise_suppression_;
  VoiceDetectionImpl* voice_detection_;
  rtc::scoped_ptr<GainControlForNewAgc> gain_control_for_new_agc_;

  std::list<ProcessingComponent*> component_list_;
  CriticalSectionWrapper* crit_;
  rtc::scoped_ptr<AudioBuffer> render_audio_;
  rtc::scoped_ptr<AudioBuffer> capture_audio_;
  rtc::scoped_ptr<AudioConverter> render_converter_;
#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // TODO(andrew): make this more graceful. Ideally we would split this stuff
  // out into a separate class with an "enabled" and "disabled" implementation.
  int WriteMessageToDebugFile();
  int WriteInitMessage();

  // Writes Config message. If not |forced|, only writes the current config if
  // it is different from the last saved one; if |forced|, writes the config
  // regardless of the last saved.
  int WriteConfigMessage(bool forced);

  rtc::scoped_ptr<FileWrapper> debug_file_;
  rtc::scoped_ptr<audioproc::Event> event_msg_;  // Protobuf message.
  std::string event_str_;  // Memory for protobuf serialization.

  // Serialized string of last saved APM configuration.
  std::string last_serialized_config_;
#endif

  // Format of processing streams at input/output call sites.
  ProcessingConfig api_format_;

  // Only the rate and samples fields of fwd_proc_format_ are used because the
  // forward processing number of channels is mutable and is tracked by the
  // capture_audio_.
  StreamConfig fwd_proc_format_;
  StreamConfig rev_proc_format_;
  int split_rate_;

  int stream_delay_ms_;
  int delay_offset_ms_;
  bool was_stream_delay_set_;
  int last_stream_delay_ms_;
  int last_aec_system_delay_ms_;
  int stream_delay_jumps_;
  int aec_system_delay_jumps_;

  bool output_will_be_muted_ GUARDED_BY(crit_);

  bool key_pressed_;

  // Only set through the constructor's Config parameter.
  const bool use_new_agc_;
  rtc::scoped_ptr<AgcManagerDirect> agc_manager_ GUARDED_BY(crit_);
  int agc_startup_min_volume_;

  bool transient_suppressor_enabled_;
  rtc::scoped_ptr<TransientSuppressor> transient_suppressor_;
  const bool beamformer_enabled_;
  rtc::scoped_ptr<Beamformer<float>> beamformer_;
  const std::vector<Point> array_geometry_;

  bool intelligibility_enabled_;
  rtc::scoped_ptr<IntelligibilityEnhancer> intelligibility_enhancer_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_
