/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/audio_processing_impl.h"

#include <assert.h>
#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/platform_file.h"
#include "webrtc/common_audio/audio_converter.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
extern "C" {
#include "webrtc/modules/audio_processing/aec/aec_core.h"
}
#include "webrtc/modules/audio_processing/agc/agc_manager_direct.h"
#include "webrtc/modules/audio_processing/audio_buffer.h"
#include "webrtc/modules/audio_processing/beamformer/nonlinear_beamformer.h"
#include "webrtc/modules/audio_processing/common.h"
#include "webrtc/modules/audio_processing/echo_cancellation_impl.h"
#include "webrtc/modules/audio_processing/echo_control_mobile_impl.h"
#include "webrtc/modules/audio_processing/gain_control_impl.h"
#include "webrtc/modules/audio_processing/high_pass_filter_impl.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_enhancer.h"
#include "webrtc/modules/audio_processing/level_estimator_impl.h"
#include "webrtc/modules/audio_processing/noise_suppression_impl.h"
#include "webrtc/modules/audio_processing/processing_component.h"
#include "webrtc/modules/audio_processing/transient/transient_suppressor.h"
#include "webrtc/modules/audio_processing/voice_detection_impl.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/metrics.h"

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
// Files generated at build-time by the protobuf compiler.
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/debug.pb.h"
#else
#include "webrtc/audio_processing/debug.pb.h"
#endif
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP

#define RETURN_ON_ERR(expr) \
  do {                      \
    int err = (expr);       \
    if (err != kNoError) {  \
      return err;           \
    }                       \
  } while (0)

namespace webrtc {
namespace {

static bool LayoutHasKeyboard(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kStereo:
      return false;
    case AudioProcessing::kMonoAndKeyboard:
    case AudioProcessing::kStereoAndKeyboard:
      return true;
  }

  assert(false);
  return false;
}

}  // namespace

// Throughout webrtc, it's assumed that success is represented by zero.
static_assert(AudioProcessing::kNoError == 0, "kNoError must be zero");

// This class has two main functionalities:
//
// 1) It is returned instead of the real GainControl after the new AGC has been
//    enabled in order to prevent an outside user from overriding compression
//    settings. It doesn't do anything in its implementation, except for
//    delegating the const methods and Enable calls to the real GainControl, so
//    AGC can still be disabled.
//
// 2) It is injected into AgcManagerDirect and implements volume callbacks for
//    getting and setting the volume level. It just caches this value to be used
//    in VoiceEngine later.
class GainControlForNewAgc : public GainControl, public VolumeCallbacks {
 public:
  explicit GainControlForNewAgc(GainControlImpl* gain_control)
      : real_gain_control_(gain_control), volume_(0) {}

  // GainControl implementation.
  int Enable(bool enable) override {
    return real_gain_control_->Enable(enable);
  }
  bool is_enabled() const override { return real_gain_control_->is_enabled(); }
  int set_stream_analog_level(int level) override {
    volume_ = level;
    return AudioProcessing::kNoError;
  }
  int stream_analog_level() override { return volume_; }
  int set_mode(Mode mode) override { return AudioProcessing::kNoError; }
  Mode mode() const override { return GainControl::kAdaptiveAnalog; }
  int set_target_level_dbfs(int level) override {
    return AudioProcessing::kNoError;
  }
  int target_level_dbfs() const override {
    return real_gain_control_->target_level_dbfs();
  }
  int set_compression_gain_db(int gain) override {
    return AudioProcessing::kNoError;
  }
  int compression_gain_db() const override {
    return real_gain_control_->compression_gain_db();
  }
  int enable_limiter(bool enable) override { return AudioProcessing::kNoError; }
  bool is_limiter_enabled() const override {
    return real_gain_control_->is_limiter_enabled();
  }
  int set_analog_level_limits(int minimum, int maximum) override {
    return AudioProcessing::kNoError;
  }
  int analog_level_minimum() const override {
    return real_gain_control_->analog_level_minimum();
  }
  int analog_level_maximum() const override {
    return real_gain_control_->analog_level_maximum();
  }
  bool stream_is_saturated() const override {
    return real_gain_control_->stream_is_saturated();
  }

  // VolumeCallbacks implementation.
  void SetMicVolume(int volume) override { volume_ = volume; }
  int GetMicVolume() override { return volume_; }

 private:
  GainControl* real_gain_control_;
  int volume_;
};

const int AudioProcessing::kNativeSampleRatesHz[] = {
    AudioProcessing::kSampleRate8kHz,
    AudioProcessing::kSampleRate16kHz,
    AudioProcessing::kSampleRate32kHz,
    AudioProcessing::kSampleRate48kHz};
const size_t AudioProcessing::kNumNativeSampleRates =
    arraysize(AudioProcessing::kNativeSampleRatesHz);
const int AudioProcessing::kMaxNativeSampleRateHz = AudioProcessing::
    kNativeSampleRatesHz[AudioProcessing::kNumNativeSampleRates - 1];
const int AudioProcessing::kMaxAECMSampleRateHz = kSampleRate16kHz;

AudioProcessing* AudioProcessing::Create() {
  Config config;
  return Create(config, nullptr);
}

AudioProcessing* AudioProcessing::Create(const Config& config) {
  return Create(config, nullptr);
}

AudioProcessing* AudioProcessing::Create(const Config& config,
                                         Beamformer<float>* beamformer) {
  AudioProcessingImpl* apm = new AudioProcessingImpl(config, beamformer);
  if (apm->Initialize() != kNoError) {
    delete apm;
    apm = NULL;
  }

  return apm;
}

AudioProcessingImpl::AudioProcessingImpl(const Config& config)
    : AudioProcessingImpl(config, nullptr) {}

AudioProcessingImpl::AudioProcessingImpl(const Config& config,
                                         Beamformer<float>* beamformer)
    : echo_cancellation_(NULL),
      echo_control_mobile_(NULL),
      gain_control_(NULL),
      high_pass_filter_(NULL),
      level_estimator_(NULL),
      noise_suppression_(NULL),
      voice_detection_(NULL),
      crit_(CriticalSectionWrapper::CreateCriticalSection()),
#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
      debug_file_(FileWrapper::Create()),
      event_msg_(new audioproc::Event()),
#endif
      api_format_({{{kSampleRate16kHz, 1, false},
                    {kSampleRate16kHz, 1, false},
                    {kSampleRate16kHz, 1, false},
                    {kSampleRate16kHz, 1, false}}}),
      fwd_proc_format_(kSampleRate16kHz),
      rev_proc_format_(kSampleRate16kHz, 1),
      split_rate_(kSampleRate16kHz),
      stream_delay_ms_(0),
      delay_offset_ms_(0),
      was_stream_delay_set_(false),
      last_stream_delay_ms_(0),
      last_aec_system_delay_ms_(0),
      stream_delay_jumps_(-1),
      aec_system_delay_jumps_(-1),
      output_will_be_muted_(false),
      key_pressed_(false),
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
      use_new_agc_(false),
#else
      use_new_agc_(config.Get<ExperimentalAgc>().enabled),
#endif
      agc_startup_min_volume_(config.Get<ExperimentalAgc>().startup_min_volume),
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
      transient_suppressor_enabled_(false),
#else
      transient_suppressor_enabled_(config.Get<ExperimentalNs>().enabled),
#endif
      beamformer_enabled_(config.Get<Beamforming>().enabled),
      beamformer_(beamformer),
      array_geometry_(config.Get<Beamforming>().array_geometry),
      intelligibility_enabled_(config.Get<Intelligibility>().enabled) {
  echo_cancellation_ = new EchoCancellationImpl(this, crit_);
  component_list_.push_back(echo_cancellation_);

  echo_control_mobile_ = new EchoControlMobileImpl(this, crit_);
  component_list_.push_back(echo_control_mobile_);

  gain_control_ = new GainControlImpl(this, crit_);
  component_list_.push_back(gain_control_);

  high_pass_filter_ = new HighPassFilterImpl(this, crit_);
  component_list_.push_back(high_pass_filter_);

  level_estimator_ = new LevelEstimatorImpl(this, crit_);
  component_list_.push_back(level_estimator_);

  noise_suppression_ = new NoiseSuppressionImpl(this, crit_);
  component_list_.push_back(noise_suppression_);

  voice_detection_ = new VoiceDetectionImpl(this, crit_);
  component_list_.push_back(voice_detection_);

  gain_control_for_new_agc_.reset(new GainControlForNewAgc(gain_control_));

  SetExtraOptions(config);
}

AudioProcessingImpl::~AudioProcessingImpl() {
  {
    CriticalSectionScoped crit_scoped(crit_);
    // Depends on gain_control_ and gain_control_for_new_agc_.
    agc_manager_.reset();
    // Depends on gain_control_.
    gain_control_for_new_agc_.reset();
    while (!component_list_.empty()) {
      ProcessingComponent* component = component_list_.front();
      component->Destroy();
      delete component;
      component_list_.pop_front();
    }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
    if (debug_file_->Open()) {
      debug_file_->CloseFile();
    }
#endif
  }
  delete crit_;
  crit_ = NULL;
}

int AudioProcessingImpl::Initialize() {
  CriticalSectionScoped crit_scoped(crit_);
  return InitializeLocked();
}

int AudioProcessingImpl::Initialize(int input_sample_rate_hz,
                                    int output_sample_rate_hz,
                                    int reverse_sample_rate_hz,
                                    ChannelLayout input_layout,
                                    ChannelLayout output_layout,
                                    ChannelLayout reverse_layout) {
  const ProcessingConfig processing_config = {
      {{input_sample_rate_hz,
        ChannelsFromLayout(input_layout),
        LayoutHasKeyboard(input_layout)},
       {output_sample_rate_hz,
        ChannelsFromLayout(output_layout),
        LayoutHasKeyboard(output_layout)},
       {reverse_sample_rate_hz,
        ChannelsFromLayout(reverse_layout),
        LayoutHasKeyboard(reverse_layout)},
       {reverse_sample_rate_hz,
        ChannelsFromLayout(reverse_layout),
        LayoutHasKeyboard(reverse_layout)}}};

  return Initialize(processing_config);
}

int AudioProcessingImpl::Initialize(const ProcessingConfig& processing_config) {
  CriticalSectionScoped crit_scoped(crit_);
  return InitializeLocked(processing_config);
}

int AudioProcessingImpl::InitializeLocked() {
  const int fwd_audio_buffer_channels =
      beamformer_enabled_ ? api_format_.input_stream().num_channels()
                          : api_format_.output_stream().num_channels();
  const int rev_audio_buffer_out_num_frames =
      api_format_.reverse_output_stream().num_frames() == 0
          ? rev_proc_format_.num_frames()
          : api_format_.reverse_output_stream().num_frames();
  if (api_format_.reverse_input_stream().num_channels() > 0) {
    render_audio_.reset(new AudioBuffer(
        api_format_.reverse_input_stream().num_frames(),
        api_format_.reverse_input_stream().num_channels(),
        rev_proc_format_.num_frames(), rev_proc_format_.num_channels(),
        rev_audio_buffer_out_num_frames));
    if (rev_conversion_needed()) {
      render_converter_ = AudioConverter::Create(
          api_format_.reverse_input_stream().num_channels(),
          api_format_.reverse_input_stream().num_frames(),
          api_format_.reverse_output_stream().num_channels(),
          api_format_.reverse_output_stream().num_frames());
    } else {
      render_converter_.reset(nullptr);
    }
  } else {
    render_audio_.reset(nullptr);
    render_converter_.reset(nullptr);
  }
  capture_audio_.reset(new AudioBuffer(
      api_format_.input_stream().num_frames(),
      api_format_.input_stream().num_channels(), fwd_proc_format_.num_frames(),
      fwd_audio_buffer_channels, api_format_.output_stream().num_frames()));

  // Initialize all components.
  for (auto item : component_list_) {
    int err = item->Initialize();
    if (err != kNoError) {
      return err;
    }
  }

  InitializeExperimentalAgc();

  InitializeTransient();

  InitializeBeamformer();

  InitializeIntelligibility();

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    int err = WriteInitMessage();
    if (err != kNoError) {
      return err;
    }
  }
#endif

  return kNoError;
}

int AudioProcessingImpl::InitializeLocked(const ProcessingConfig& config) {
  for (const auto& stream : config.streams) {
    if (stream.num_channels() < 0) {
      return kBadNumberChannelsError;
    }
    if (stream.num_channels() > 0 && stream.sample_rate_hz() <= 0) {
      return kBadSampleRateError;
    }
  }

  const int num_in_channels = config.input_stream().num_channels();
  const int num_out_channels = config.output_stream().num_channels();

  // Need at least one input channel.
  // Need either one output channel or as many outputs as there are inputs.
  if (num_in_channels == 0 ||
      !(num_out_channels == 1 || num_out_channels == num_in_channels)) {
    return kBadNumberChannelsError;
  }

  if (beamformer_enabled_ &&
      (static_cast<size_t>(num_in_channels) != array_geometry_.size() ||
       num_out_channels > 1)) {
    return kBadNumberChannelsError;
  }

  api_format_ = config;

  // We process at the closest native rate >= min(input rate, output rate)...
  const int min_proc_rate =
      std::min(api_format_.input_stream().sample_rate_hz(),
               api_format_.output_stream().sample_rate_hz());
  int fwd_proc_rate;
  for (size_t i = 0; i < kNumNativeSampleRates; ++i) {
    fwd_proc_rate = kNativeSampleRatesHz[i];
    if (fwd_proc_rate >= min_proc_rate) {
      break;
    }
  }
  // ...with one exception.
  if (echo_control_mobile_->is_enabled() &&
      min_proc_rate > kMaxAECMSampleRateHz) {
    fwd_proc_rate = kMaxAECMSampleRateHz;
  }

  fwd_proc_format_ = StreamConfig(fwd_proc_rate);

  // We normally process the reverse stream at 16 kHz. Unless...
  int rev_proc_rate = kSampleRate16kHz;
  if (fwd_proc_format_.sample_rate_hz() == kSampleRate8kHz) {
    // ...the forward stream is at 8 kHz.
    rev_proc_rate = kSampleRate8kHz;
  } else {
    if (api_format_.reverse_input_stream().sample_rate_hz() ==
        kSampleRate32kHz) {
      // ...or the input is at 32 kHz, in which case we use the splitting
      // filter rather than the resampler.
      rev_proc_rate = kSampleRate32kHz;
    }
  }

  // Always downmix the reverse stream to mono for analysis. This has been
  // demonstrated to work well for AEC in most practical scenarios.
  rev_proc_format_ = StreamConfig(rev_proc_rate, 1);

  if (fwd_proc_format_.sample_rate_hz() == kSampleRate32kHz ||
      fwd_proc_format_.sample_rate_hz() == kSampleRate48kHz) {
    split_rate_ = kSampleRate16kHz;
  } else {
    split_rate_ = fwd_proc_format_.sample_rate_hz();
  }

  return InitializeLocked();
}

// Calls InitializeLocked() if any of the audio parameters have changed from
// their current values.
int AudioProcessingImpl::MaybeInitializeLocked(
    const ProcessingConfig& processing_config) {
  if (processing_config == api_format_) {
    return kNoError;
  }
  return InitializeLocked(processing_config);
}

void AudioProcessingImpl::SetExtraOptions(const Config& config) {
  CriticalSectionScoped crit_scoped(crit_);
  for (auto item : component_list_) {
    item->SetExtraOptions(config);
  }

  if (transient_suppressor_enabled_ != config.Get<ExperimentalNs>().enabled) {
    transient_suppressor_enabled_ = config.Get<ExperimentalNs>().enabled;
    InitializeTransient();
  }
}


int AudioProcessingImpl::proc_sample_rate_hz() const {
  return fwd_proc_format_.sample_rate_hz();
}

int AudioProcessingImpl::proc_split_sample_rate_hz() const {
  return split_rate_;
}

int AudioProcessingImpl::num_reverse_channels() const {
  return rev_proc_format_.num_channels();
}

int AudioProcessingImpl::num_input_channels() const {
  return api_format_.input_stream().num_channels();
}

int AudioProcessingImpl::num_output_channels() const {
  return api_format_.output_stream().num_channels();
}

void AudioProcessingImpl::set_output_will_be_muted(bool muted) {
  CriticalSectionScoped lock(crit_);
  output_will_be_muted_ = muted;
  if (agc_manager_.get()) {
    agc_manager_->SetCaptureMuted(output_will_be_muted_);
  }
}


int AudioProcessingImpl::ProcessStream(const float* const* src,
                                       size_t samples_per_channel,
                                       int input_sample_rate_hz,
                                       ChannelLayout input_layout,
                                       int output_sample_rate_hz,
                                       ChannelLayout output_layout,
                                       float* const* dest) {
  CriticalSectionScoped crit_scoped(crit_);
  StreamConfig input_stream = api_format_.input_stream();
  input_stream.set_sample_rate_hz(input_sample_rate_hz);
  input_stream.set_num_channels(ChannelsFromLayout(input_layout));
  input_stream.set_has_keyboard(LayoutHasKeyboard(input_layout));

  StreamConfig output_stream = api_format_.output_stream();
  output_stream.set_sample_rate_hz(output_sample_rate_hz);
  output_stream.set_num_channels(ChannelsFromLayout(output_layout));
  output_stream.set_has_keyboard(LayoutHasKeyboard(output_layout));

  if (samples_per_channel != input_stream.num_frames()) {
    return kBadDataLengthError;
  }
  return ProcessStream(src, input_stream, output_stream, dest);
}

int AudioProcessingImpl::ProcessStream(const float* const* src,
                                       const StreamConfig& input_config,
                                       const StreamConfig& output_config,
                                       float* const* dest) {
  CriticalSectionScoped crit_scoped(crit_);
  if (!src || !dest) {
    return kNullPointerError;
  }

  ProcessingConfig processing_config = api_format_;
  processing_config.input_stream() = input_config;
  processing_config.output_stream() = output_config;

  RETURN_ON_ERR(MaybeInitializeLocked(processing_config));
  assert(processing_config.input_stream().num_frames() ==
         api_format_.input_stream().num_frames());

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    RETURN_ON_ERR(WriteConfigMessage(false));

    event_msg_->set_type(audioproc::Event::STREAM);
    audioproc::Stream* msg = event_msg_->mutable_stream();
    const size_t channel_size =
        sizeof(float) * api_format_.input_stream().num_frames();
    for (int i = 0; i < api_format_.input_stream().num_channels(); ++i)
      msg->add_input_channel(src[i], channel_size);
  }
#endif

  capture_audio_->CopyFrom(src, api_format_.input_stream());
  RETURN_ON_ERR(ProcessStreamLocked());
  capture_audio_->CopyTo(api_format_.output_stream(), dest);

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    audioproc::Stream* msg = event_msg_->mutable_stream();
    const size_t channel_size =
        sizeof(float) * api_format_.output_stream().num_frames();
    for (int i = 0; i < api_format_.output_stream().num_channels(); ++i)
      msg->add_output_channel(dest[i], channel_size);
    RETURN_ON_ERR(WriteMessageToDebugFile());
  }
#endif

  return kNoError;
}

int AudioProcessingImpl::ProcessStream(AudioFrame* frame) {
  CriticalSectionScoped crit_scoped(crit_);
  if (!frame) {
    return kNullPointerError;
  }
  // Must be a native rate.
  if (frame->sample_rate_hz_ != kSampleRate8kHz &&
      frame->sample_rate_hz_ != kSampleRate16kHz &&
      frame->sample_rate_hz_ != kSampleRate32kHz &&
      frame->sample_rate_hz_ != kSampleRate48kHz) {
    return kBadSampleRateError;
  }
  if (echo_control_mobile_->is_enabled() &&
      frame->sample_rate_hz_ > kMaxAECMSampleRateHz) {
    LOG(LS_ERROR) << "AECM only supports 16 or 8 kHz sample rates";
    return kUnsupportedComponentError;
  }

  // TODO(ajm): The input and output rates and channels are currently
  // constrained to be identical in the int16 interface.
  ProcessingConfig processing_config = api_format_;
  processing_config.input_stream().set_sample_rate_hz(frame->sample_rate_hz_);
  processing_config.input_stream().set_num_channels(frame->num_channels_);
  processing_config.output_stream().set_sample_rate_hz(frame->sample_rate_hz_);
  processing_config.output_stream().set_num_channels(frame->num_channels_);

  RETURN_ON_ERR(MaybeInitializeLocked(processing_config));
  if (frame->samples_per_channel_ != api_format_.input_stream().num_frames()) {
    return kBadDataLengthError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    event_msg_->set_type(audioproc::Event::STREAM);
    audioproc::Stream* msg = event_msg_->mutable_stream();
    const size_t data_size =
        sizeof(int16_t) * frame->samples_per_channel_ * frame->num_channels_;
    msg->set_input_data(frame->data_, data_size);
  }
#endif

  capture_audio_->DeinterleaveFrom(frame);
  RETURN_ON_ERR(ProcessStreamLocked());
  capture_audio_->InterleaveTo(frame, output_copy_needed(is_data_processed()));

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    audioproc::Stream* msg = event_msg_->mutable_stream();
    const size_t data_size =
        sizeof(int16_t) * frame->samples_per_channel_ * frame->num_channels_;
    msg->set_output_data(frame->data_, data_size);
    RETURN_ON_ERR(WriteMessageToDebugFile());
  }
#endif

  return kNoError;
}

int AudioProcessingImpl::ProcessStreamLocked() {
#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    audioproc::Stream* msg = event_msg_->mutable_stream();
    msg->set_delay(stream_delay_ms_);
    msg->set_drift(echo_cancellation_->stream_drift_samples());
    msg->set_level(gain_control()->stream_analog_level());
    msg->set_keypress(key_pressed_);
  }
#endif

  MaybeUpdateHistograms();

  AudioBuffer* ca = capture_audio_.get();  // For brevity.

  if (use_new_agc_ && gain_control_->is_enabled()) {
    agc_manager_->AnalyzePreProcess(ca->channels()[0], ca->num_channels(),
                                    fwd_proc_format_.num_frames());
  }

  bool data_processed = is_data_processed();
  if (analysis_needed(data_processed)) {
    ca->SplitIntoFrequencyBands();
  }

  if (intelligibility_enabled_) {
    intelligibility_enhancer_->AnalyzeCaptureAudio(
        ca->split_channels_f(kBand0To8kHz), split_rate_, ca->num_channels());
  }

  if (beamformer_enabled_) {
    beamformer_->ProcessChunk(*ca->split_data_f(), ca->split_data_f());
    ca->set_num_channels(1);
  }

  RETURN_ON_ERR(high_pass_filter_->ProcessCaptureAudio(ca));
  RETURN_ON_ERR(gain_control_->AnalyzeCaptureAudio(ca));
  RETURN_ON_ERR(noise_suppression_->AnalyzeCaptureAudio(ca));
  RETURN_ON_ERR(echo_cancellation_->ProcessCaptureAudio(ca));

  if (echo_control_mobile_->is_enabled() && noise_suppression_->is_enabled()) {
    ca->CopyLowPassToReference();
  }
  RETURN_ON_ERR(noise_suppression_->ProcessCaptureAudio(ca));
  RETURN_ON_ERR(echo_control_mobile_->ProcessCaptureAudio(ca));
  RETURN_ON_ERR(voice_detection_->ProcessCaptureAudio(ca));

  if (use_new_agc_ && gain_control_->is_enabled() &&
      (!beamformer_enabled_ || beamformer_->is_target_present())) {
    agc_manager_->Process(ca->split_bands_const(0)[kBand0To8kHz],
                          ca->num_frames_per_band(), split_rate_);
  }
  RETURN_ON_ERR(gain_control_->ProcessCaptureAudio(ca));

  if (synthesis_needed(data_processed)) {
    ca->MergeFrequencyBands();
  }

  // TODO(aluebs): Investigate if the transient suppression placement should be
  // before or after the AGC.
  if (transient_suppressor_enabled_) {
    float voice_probability =
        agc_manager_.get() ? agc_manager_->voice_probability() : 1.f;

    transient_suppressor_->Suppress(
        ca->channels_f()[0], ca->num_frames(), ca->num_channels(),
        ca->split_bands_const_f(0)[kBand0To8kHz], ca->num_frames_per_band(),
        ca->keyboard_data(), ca->num_keyboard_frames(), voice_probability,
        key_pressed_);
  }

  // The level estimator operates on the recombined data.
  RETURN_ON_ERR(level_estimator_->ProcessStream(ca));

  was_stream_delay_set_ = false;
  return kNoError;
}

int AudioProcessingImpl::AnalyzeReverseStream(const float* const* data,
                                              size_t samples_per_channel,
                                              int rev_sample_rate_hz,
                                              ChannelLayout layout) {
  const StreamConfig reverse_config = {
      rev_sample_rate_hz, ChannelsFromLayout(layout), LayoutHasKeyboard(layout),
  };
  if (samples_per_channel != reverse_config.num_frames()) {
    return kBadDataLengthError;
  }
  return AnalyzeReverseStream(data, reverse_config, reverse_config);
}

int AudioProcessingImpl::ProcessReverseStream(
    const float* const* src,
    const StreamConfig& reverse_input_config,
    const StreamConfig& reverse_output_config,
    float* const* dest) {
  RETURN_ON_ERR(
      AnalyzeReverseStream(src, reverse_input_config, reverse_output_config));
  if (is_rev_processed()) {
    render_audio_->CopyTo(api_format_.reverse_output_stream(), dest);
  } else if (rev_conversion_needed()) {
    render_converter_->Convert(src, reverse_input_config.num_samples(), dest,
                               reverse_output_config.num_samples());
  } else {
    CopyAudioIfNeeded(src, reverse_input_config.num_frames(),
                      reverse_input_config.num_channels(), dest);
  }

  return kNoError;
}

int AudioProcessingImpl::AnalyzeReverseStream(
    const float* const* src,
    const StreamConfig& reverse_input_config,
    const StreamConfig& reverse_output_config) {
  CriticalSectionScoped crit_scoped(crit_);
  if (src == NULL) {
    return kNullPointerError;
  }

  if (reverse_input_config.num_channels() <= 0) {
    return kBadNumberChannelsError;
  }

  ProcessingConfig processing_config = api_format_;
  processing_config.reverse_input_stream() = reverse_input_config;
  processing_config.reverse_output_stream() = reverse_output_config;

  RETURN_ON_ERR(MaybeInitializeLocked(processing_config));
  assert(reverse_input_config.num_frames() ==
         api_format_.reverse_input_stream().num_frames());

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    event_msg_->set_type(audioproc::Event::REVERSE_STREAM);
    audioproc::ReverseStream* msg = event_msg_->mutable_reverse_stream();
    const size_t channel_size =
        sizeof(float) * api_format_.reverse_input_stream().num_frames();
    for (int i = 0; i < api_format_.reverse_input_stream().num_channels(); ++i)
      msg->add_channel(src[i], channel_size);
    RETURN_ON_ERR(WriteMessageToDebugFile());
  }
#endif

  render_audio_->CopyFrom(src, api_format_.reverse_input_stream());
  return ProcessReverseStreamLocked();
}

int AudioProcessingImpl::ProcessReverseStream(AudioFrame* frame) {
  RETURN_ON_ERR(AnalyzeReverseStream(frame));
  if (is_rev_processed()) {
    render_audio_->InterleaveTo(frame, true);
  }

  return kNoError;
}

int AudioProcessingImpl::AnalyzeReverseStream(AudioFrame* frame) {
  CriticalSectionScoped crit_scoped(crit_);
  if (frame == NULL) {
    return kNullPointerError;
  }
  // Must be a native rate.
  if (frame->sample_rate_hz_ != kSampleRate8kHz &&
      frame->sample_rate_hz_ != kSampleRate16kHz &&
      frame->sample_rate_hz_ != kSampleRate32kHz &&
      frame->sample_rate_hz_ != kSampleRate48kHz) {
    return kBadSampleRateError;
  }
  // This interface does not tolerate different forward and reverse rates.
  if (frame->sample_rate_hz_ != api_format_.input_stream().sample_rate_hz()) {
    return kBadSampleRateError;
  }

  if (frame->num_channels_ <= 0) {
    return kBadNumberChannelsError;
  }

  ProcessingConfig processing_config = api_format_;
  processing_config.reverse_input_stream().set_sample_rate_hz(
      frame->sample_rate_hz_);
  processing_config.reverse_input_stream().set_num_channels(
      frame->num_channels_);
  processing_config.reverse_output_stream().set_sample_rate_hz(
      frame->sample_rate_hz_);
  processing_config.reverse_output_stream().set_num_channels(
      frame->num_channels_);

  RETURN_ON_ERR(MaybeInitializeLocked(processing_config));
  if (frame->samples_per_channel_ !=
      api_format_.reverse_input_stream().num_frames()) {
    return kBadDataLengthError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  if (debug_file_->Open()) {
    event_msg_->set_type(audioproc::Event::REVERSE_STREAM);
    audioproc::ReverseStream* msg = event_msg_->mutable_reverse_stream();
    const size_t data_size =
        sizeof(int16_t) * frame->samples_per_channel_ * frame->num_channels_;
    msg->set_data(frame->data_, data_size);
    RETURN_ON_ERR(WriteMessageToDebugFile());
  }
#endif
  render_audio_->DeinterleaveFrom(frame);
  return ProcessReverseStreamLocked();
}

int AudioProcessingImpl::ProcessReverseStreamLocked() {
  AudioBuffer* ra = render_audio_.get();  // For brevity.
  if (rev_proc_format_.sample_rate_hz() == kSampleRate32kHz) {
    ra->SplitIntoFrequencyBands();
  }

  if (intelligibility_enabled_) {
    intelligibility_enhancer_->ProcessRenderAudio(
        ra->split_channels_f(kBand0To8kHz), split_rate_, ra->num_channels());
  }

  RETURN_ON_ERR(echo_cancellation_->ProcessRenderAudio(ra));
  RETURN_ON_ERR(echo_control_mobile_->ProcessRenderAudio(ra));
  if (!use_new_agc_) {
    RETURN_ON_ERR(gain_control_->ProcessRenderAudio(ra));
  }

  if (rev_proc_format_.sample_rate_hz() == kSampleRate32kHz &&
      is_rev_processed()) {
    ra->MergeFrequencyBands();
  }

  return kNoError;
}

int AudioProcessingImpl::set_stream_delay_ms(int delay) {
  Error retval = kNoError;
  was_stream_delay_set_ = true;
  delay += delay_offset_ms_;

  if (delay < 0) {
    delay = 0;
    retval = kBadStreamParameterWarning;
  }

  // TODO(ajm): the max is rather arbitrarily chosen; investigate.
  if (delay > 500) {
    delay = 500;
    retval = kBadStreamParameterWarning;
  }

  stream_delay_ms_ = delay;
  return retval;
}

int AudioProcessingImpl::stream_delay_ms() const {
  return stream_delay_ms_;
}

bool AudioProcessingImpl::was_stream_delay_set() const {
  return was_stream_delay_set_;
}

void AudioProcessingImpl::set_stream_key_pressed(bool key_pressed) {
  key_pressed_ = key_pressed;
}

void AudioProcessingImpl::set_delay_offset_ms(int offset) {
  CriticalSectionScoped crit_scoped(crit_);
  delay_offset_ms_ = offset;
}

int AudioProcessingImpl::delay_offset_ms() const {
  return delay_offset_ms_;
}

int AudioProcessingImpl::StartDebugRecording(
    const char filename[AudioProcessing::kMaxFilenameSize]) {
  CriticalSectionScoped crit_scoped(crit_);
  static_assert(kMaxFilenameSize == FileWrapper::kMaxFileNameSize, "");

  if (filename == NULL) {
    return kNullPointerError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // Stop any ongoing recording.
  if (debug_file_->Open()) {
    if (debug_file_->CloseFile() == -1) {
      return kFileError;
    }
  }

  if (debug_file_->OpenFile(filename, false) == -1) {
    debug_file_->CloseFile();
    return kFileError;
  }

  RETURN_ON_ERR(WriteConfigMessage(true));
  RETURN_ON_ERR(WriteInitMessage());
  return kNoError;
#else
  return kUnsupportedFunctionError;
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

int AudioProcessingImpl::StartDebugRecording(FILE* handle) {
  CriticalSectionScoped crit_scoped(crit_);

  if (handle == NULL) {
    return kNullPointerError;
  }

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // Stop any ongoing recording.
  if (debug_file_->Open()) {
    if (debug_file_->CloseFile() == -1) {
      return kFileError;
    }
  }

  if (debug_file_->OpenFromFileHandle(handle, true, false) == -1) {
    return kFileError;
  }

  RETURN_ON_ERR(WriteConfigMessage(true));
  RETURN_ON_ERR(WriteInitMessage());
  return kNoError;
#else
  return kUnsupportedFunctionError;
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

int AudioProcessingImpl::StartDebugRecordingForPlatformFile(
    rtc::PlatformFile handle) {
  FILE* stream = rtc::FdopenPlatformFileForWriting(handle);
  return StartDebugRecording(stream);
}

int AudioProcessingImpl::StopDebugRecording() {
  CriticalSectionScoped crit_scoped(crit_);

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
  // We just return if recording hasn't started.
  if (debug_file_->Open()) {
    if (debug_file_->CloseFile() == -1) {
      return kFileError;
    }
  }
  return kNoError;
#else
  return kUnsupportedFunctionError;
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP
}

EchoCancellation* AudioProcessingImpl::echo_cancellation() const {
  return echo_cancellation_;
}

EchoControlMobile* AudioProcessingImpl::echo_control_mobile() const {
  return echo_control_mobile_;
}

GainControl* AudioProcessingImpl::gain_control() const {
  if (use_new_agc_) {
    return gain_control_for_new_agc_.get();
  }
  return gain_control_;
}

HighPassFilter* AudioProcessingImpl::high_pass_filter() const {
  return high_pass_filter_;
}

LevelEstimator* AudioProcessingImpl::level_estimator() const {
  return level_estimator_;
}

NoiseSuppression* AudioProcessingImpl::noise_suppression() const {
  return noise_suppression_;
}

VoiceDetection* AudioProcessingImpl::voice_detection() const {
  return voice_detection_;
}

bool AudioProcessingImpl::is_data_processed() const {
  if (beamformer_enabled_) {
    return true;
  }

  int enabled_count = 0;
  for (auto item : component_list_) {
    if (item->is_component_enabled()) {
      enabled_count++;
    }
  }

  // Data is unchanged if no components are enabled, or if only level_estimator_
  // or voice_detection_ is enabled.
  if (enabled_count == 0) {
    return false;
  } else if (enabled_count == 1) {
    if (level_estimator_->is_enabled() || voice_detection_->is_enabled()) {
      return false;
    }
  } else if (enabled_count == 2) {
    if (level_estimator_->is_enabled() && voice_detection_->is_enabled()) {
      return false;
    }
  }
  return true;
}

bool AudioProcessingImpl::output_copy_needed(bool is_data_processed) const {
  // Check if we've upmixed or downmixed the audio.
  return ((api_format_.output_stream().num_channels() !=
           api_format_.input_stream().num_channels()) ||
          is_data_processed || transient_suppressor_enabled_);
}

bool AudioProcessingImpl::synthesis_needed(bool is_data_processed) const {
  return (is_data_processed &&
          (fwd_proc_format_.sample_rate_hz() == kSampleRate32kHz ||
           fwd_proc_format_.sample_rate_hz() == kSampleRate48kHz));
}

bool AudioProcessingImpl::analysis_needed(bool is_data_processed) const {
  if (!is_data_processed && !voice_detection_->is_enabled() &&
      !transient_suppressor_enabled_) {
    // Only level_estimator_ is enabled.
    return false;
  } else if (fwd_proc_format_.sample_rate_hz() == kSampleRate32kHz ||
             fwd_proc_format_.sample_rate_hz() == kSampleRate48kHz) {
    // Something besides level_estimator_ is enabled, and we have super-wb.
    return true;
  }
  return false;
}

bool AudioProcessingImpl::is_rev_processed() const {
  return intelligibility_enabled_ && intelligibility_enhancer_->active();
}

bool AudioProcessingImpl::rev_conversion_needed() const {
  return (api_format_.reverse_input_stream() !=
          api_format_.reverse_output_stream());
}

void AudioProcessingImpl::InitializeExperimentalAgc() {
  if (use_new_agc_) {
    if (!agc_manager_.get()) {
      agc_manager_.reset(new AgcManagerDirect(gain_control_,
                                              gain_control_for_new_agc_.get(),
                                              agc_startup_min_volume_));
    }
    agc_manager_->Initialize();
    agc_manager_->SetCaptureMuted(output_will_be_muted_);
  }
}

void AudioProcessingImpl::InitializeTransient() {
  if (transient_suppressor_enabled_) {
    if (!transient_suppressor_.get()) {
      transient_suppressor_.reset(new TransientSuppressor());
    }
    transient_suppressor_->Initialize(
        fwd_proc_format_.sample_rate_hz(), split_rate_,
        api_format_.output_stream().num_channels());
  }
}

void AudioProcessingImpl::InitializeBeamformer() {
  if (beamformer_enabled_) {
    if (!beamformer_) {
      beamformer_.reset(new NonlinearBeamformer(array_geometry_));
    }
    beamformer_->Initialize(kChunkSizeMs, split_rate_);
  }
}

void AudioProcessingImpl::InitializeIntelligibility() {
  if (intelligibility_enabled_) {
    IntelligibilityEnhancer::Config config;
    config.sample_rate_hz = split_rate_;
    config.num_capture_channels = capture_audio_->num_channels();
    config.num_render_channels = render_audio_->num_channels();
    intelligibility_enhancer_.reset(new IntelligibilityEnhancer(config));
  }
}

void AudioProcessingImpl::MaybeUpdateHistograms() {
  static const int kMinDiffDelayMs = 60;

  if (echo_cancellation()->is_enabled()) {
    // Activate delay_jumps_ counters if we know echo_cancellation is runnning.
    // If a stream has echo we know that the echo_cancellation is in process.
    if (stream_delay_jumps_ == -1 && echo_cancellation()->stream_has_echo()) {
      stream_delay_jumps_ = 0;
    }
    if (aec_system_delay_jumps_ == -1 &&
        echo_cancellation()->stream_has_echo()) {
      aec_system_delay_jumps_ = 0;
    }

    // Detect a jump in platform reported system delay and log the difference.
    const int diff_stream_delay_ms = stream_delay_ms_ - last_stream_delay_ms_;
    if (diff_stream_delay_ms > kMinDiffDelayMs && last_stream_delay_ms_ != 0) {
      RTC_HISTOGRAM_COUNTS("WebRTC.Audio.PlatformReportedStreamDelayJump",
                           diff_stream_delay_ms, kMinDiffDelayMs, 1000, 100);
      if (stream_delay_jumps_ == -1) {
        stream_delay_jumps_ = 0;  // Activate counter if needed.
      }
      stream_delay_jumps_++;
    }
    last_stream_delay_ms_ = stream_delay_ms_;

    // Detect a jump in AEC system delay and log the difference.
    const int frames_per_ms = rtc::CheckedDivExact(split_rate_, 1000);
    const int aec_system_delay_ms =
        WebRtcAec_system_delay(echo_cancellation()->aec_core()) / frames_per_ms;
    const int diff_aec_system_delay_ms =
        aec_system_delay_ms - last_aec_system_delay_ms_;
    if (diff_aec_system_delay_ms > kMinDiffDelayMs &&
        last_aec_system_delay_ms_ != 0) {
      RTC_HISTOGRAM_COUNTS("WebRTC.Audio.AecSystemDelayJump",
                           diff_aec_system_delay_ms, kMinDiffDelayMs, 1000,
                           100);
      if (aec_system_delay_jumps_ == -1) {
        aec_system_delay_jumps_ = 0;  // Activate counter if needed.
      }
      aec_system_delay_jumps_++;
    }
    last_aec_system_delay_ms_ = aec_system_delay_ms;
  }
}

void AudioProcessingImpl::UpdateHistogramsOnCallEnd() {
  CriticalSectionScoped crit_scoped(crit_);
  if (stream_delay_jumps_ > -1) {
    RTC_HISTOGRAM_ENUMERATION(
        "WebRTC.Audio.NumOfPlatformReportedStreamDelayJumps",
        stream_delay_jumps_, 51);
  }
  stream_delay_jumps_ = -1;
  last_stream_delay_ms_ = 0;

  if (aec_system_delay_jumps_ > -1) {
    RTC_HISTOGRAM_ENUMERATION("WebRTC.Audio.NumOfAecSystemDelayJumps",
                              aec_system_delay_jumps_, 51);
  }
  aec_system_delay_jumps_ = -1;
  last_aec_system_delay_ms_ = 0;
}

#ifdef WEBRTC_AUDIOPROC_DEBUG_DUMP
int AudioProcessingImpl::WriteMessageToDebugFile() {
  int32_t size = event_msg_->ByteSize();
  if (size <= 0) {
    return kUnspecifiedError;
  }
#if defined(WEBRTC_ARCH_BIG_ENDIAN)
// TODO(ajm): Use little-endian "on the wire". For the moment, we can be
//            pretty safe in assuming little-endian.
#endif

  if (!event_msg_->SerializeToString(&event_str_)) {
    return kUnspecifiedError;
  }

  // Write message preceded by its size.
  if (!debug_file_->Write(&size, sizeof(int32_t))) {
    return kFileError;
  }
  if (!debug_file_->Write(event_str_.data(), event_str_.length())) {
    return kFileError;
  }

  event_msg_->Clear();

  return kNoError;
}

int AudioProcessingImpl::WriteInitMessage() {
  event_msg_->set_type(audioproc::Event::INIT);
  audioproc::Init* msg = event_msg_->mutable_init();
  msg->set_sample_rate(api_format_.input_stream().sample_rate_hz());
  msg->set_num_input_channels(api_format_.input_stream().num_channels());
  msg->set_num_output_channels(api_format_.output_stream().num_channels());
  msg->set_num_reverse_channels(
      api_format_.reverse_input_stream().num_channels());
  msg->set_reverse_sample_rate(
      api_format_.reverse_input_stream().sample_rate_hz());
  msg->set_output_sample_rate(api_format_.output_stream().sample_rate_hz());
  // TODO(ekmeyerson): Add reverse output fields to event_msg_.

  RETURN_ON_ERR(WriteMessageToDebugFile());
  return kNoError;
}

int AudioProcessingImpl::WriteConfigMessage(bool forced) {
  audioproc::Config config;

  config.set_aec_enabled(echo_cancellation_->is_enabled());
  config.set_aec_delay_agnostic_enabled(
      echo_cancellation_->is_delay_agnostic_enabled());
  config.set_aec_drift_compensation_enabled(
      echo_cancellation_->is_drift_compensation_enabled());
  config.set_aec_extended_filter_enabled(
      echo_cancellation_->is_extended_filter_enabled());
  config.set_aec_suppression_level(
      static_cast<int>(echo_cancellation_->suppression_level()));

  config.set_aecm_enabled(echo_control_mobile_->is_enabled());
  config.set_aecm_comfort_noise_enabled(
      echo_control_mobile_->is_comfort_noise_enabled());
  config.set_aecm_routing_mode(
      static_cast<int>(echo_control_mobile_->routing_mode()));

  config.set_agc_enabled(gain_control_->is_enabled());
  config.set_agc_mode(static_cast<int>(gain_control_->mode()));
  config.set_agc_limiter_enabled(gain_control_->is_limiter_enabled());
  config.set_noise_robust_agc_enabled(use_new_agc_);

  config.set_hpf_enabled(high_pass_filter_->is_enabled());

  config.set_ns_enabled(noise_suppression_->is_enabled());
  config.set_ns_level(static_cast<int>(noise_suppression_->level()));

  config.set_transient_suppression_enabled(transient_suppressor_enabled_);

  std::string serialized_config = config.SerializeAsString();
  if (!forced && last_serialized_config_ == serialized_config) {
    return kNoError;
  }

  last_serialized_config_ = serialized_config;

  event_msg_->set_type(audioproc::Event::CONFIG);
  event_msg_->mutable_config()->CopyFrom(config);

  RETURN_ON_ERR(WriteMessageToDebugFile());
  return kNoError;
}
#endif  // WEBRTC_AUDIOPROC_DEBUG_DUMP

}  // namespace webrtc
