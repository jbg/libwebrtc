/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/include/audio_device_data_observer.h"

#include "api/make_ref_counted.h"
#include "modules/audio_device/include/audio_device_defines.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

// A wrapper over AudioDeviceModule that registers itself as AudioTransport
// callback and redirects the PCM data to AudioDeviceDataObserver callback.
class ADMWrapper : public AudioDeviceModule, public AudioTransport {
 public:
  ADMWrapper(rtc::scoped_refptr<AudioDeviceModule> impl,
             AudioDeviceDataObserver* legacy_observer,
             std::unique_ptr<AudioDeviceDataObserver> observer)
      : impl_(impl),
        legacy_observer_(legacy_observer),
        observer_(std::move(observer)) {
    is_valid_ = impl_.get() != nullptr;
  }
  ADMWrapper(AudioLayer audio_layer,
             TaskQueueFactory* task_queue_factory,
             AudioDeviceDataObserver* legacy_observer,
             std::unique_ptr<AudioDeviceDataObserver> observer)
      : ADMWrapper(AudioDeviceModule::Create(audio_layer, task_queue_factory),
                   legacy_observer,
                   std::move(observer)) {}
  ~ADMWrapper() override {
    audio_transport_ = nullptr;
    observer_ = nullptr;
  }

  // Make sure we have a valid ADM before returning it to user.
  bool IsValid() { return is_valid_; }

  int32_t RecordedDataIsAvailable(const void* audioSamples,
                                  size_t nSamples,
                                  size_t nBytesPerSample,
                                  size_t nChannels,
                                  uint32_t samples_per_sec,
                                  uint32_t total_delay_ms,
                                  int32_t clockDrift,
                                  uint32_t currentMicLevel,
                                  bool keyPressed,
                                  uint32_t& newMicLevel) override {
    return RecordedDataIsAvailable(
        audioSamples, nSamples, nBytesPerSample, nChannels, samples_per_sec,
        total_delay_ms, clockDrift, currentMicLevel, keyPressed, newMicLevel,
        /*capture_timestamp_ns=*/absl::nullopt);
  }

  // AudioTransport methods overrides.
  int32_t RecordedDataIsAvailable(
      const void* audioSamples,
      size_t nSamples,
      size_t nBytesPerSample,
      size_t nChannels,
      uint32_t samples_per_sec,
      uint32_t total_delay_ms,
      int32_t clockDrift,
      uint32_t currentMicLevel,
      bool keyPressed,
      uint32_t& newMicLevel,
      absl::optional<int64_t> capture_timestamp_ns) override {
    int32_t res = 0;
    // Capture PCM data of locally captured audio.
    if (observer_) {
      observer_->OnCaptureData(audioSamples, nSamples, nBytesPerSample,
                               nChannels, samples_per_sec);
    }

    // Send to the actual audio transport.
    if (audio_transport_) {
      res = audio_transport_->RecordedDataIsAvailable(
          audioSamples, nSamples, nBytesPerSample, nChannels, samples_per_sec,
          total_delay_ms, clockDrift, currentMicLevel, keyPressed, newMicLevel,
          capture_timestamp_ns);
    }

    return res;
  }

  int32_t NeedMorePlayData(const size_t nSamples,
                           const size_t nBytesPerSample,
                           const size_t nChannels,
                           const uint32_t samples_per_sec,
                           void* audioSamples,
                           size_t& nSamplesOut,
                           int64_t* elapsed_time_ms,
                           int64_t* ntp_time_ms) override {
    int32_t res = 0;
    // Set out parameters to safe values to be sure not to return corrupted
    // data.
    nSamplesOut = 0;
    *elapsed_time_ms = -1;
    *ntp_time_ms = -1;
    // Request data from audio transport.
    if (audio_transport_) {
      res = audio_transport_->NeedMorePlayData(
          nSamples, nBytesPerSample, nChannels, samples_per_sec, audioSamples,
          nSamplesOut, elapsed_time_ms, ntp_time_ms);
    }

    // Capture rendered data.
    if (observer_) {
      observer_->OnRenderData(audioSamples, nSamples, nBytesPerSample,
                              nChannels, samples_per_sec);
    }

    return res;
  }

  void PullRenderData(int bits_per_sample,
                      int sample_rate,
                      size_t number_of_channels,
                      size_t number_of_frames,
                      void* audio_data,
                      int64_t* elapsed_time_ms,
                      int64_t* ntp_time_ms) override {
    RTC_DCHECK_NOTREACHED();
  }

  // Override AudioDeviceModule's RegisterAudioCallback method to remember the
  // actual audio transport (e.g.: voice engine).
  int32_t RegisterAudioCallback(AudioTransport* audio_callback) override {
    // Remember the audio callback to forward PCM data
    audio_transport_ = audio_callback;
    return 0;
  }

  int32_t Init() override {
    int res = impl_->Init();
    if (res != 0) {
      return res;
    }
    // Register self as the audio transport callback for underlying ADM impl.
    impl_->RegisterAudioCallback(this);
    return res;
  }
  int32_t Terminate() override { return impl_->Terminate(); }
  bool Initialized() const override { return impl_->Initialized(); }
  int32_t InitPlayout() override { return impl_->InitPlayout(); }
  bool PlayoutIsInitialized() const override {
    return impl_->PlayoutIsInitialized();
  }
  int32_t InitRecording() override { return impl_->InitRecording(); }
  bool RecordingIsInitialized() const override {
    return impl_->RecordingIsInitialized();
  }
  int32_t StartPlayout() override { return impl_->StartPlayout(); }
  int32_t StopPlayout() override { return impl_->StopPlayout(); }
  bool Playing() const override { return impl_->Playing(); }
  int32_t StartRecording() override { return impl_->StartRecording(); }
  int32_t StopRecording() override { return impl_->StopRecording(); }
  bool Recording() const override { return impl_->Recording(); }
  int32_t InitSpeaker() override { return impl_->InitSpeaker(); }
  bool SpeakerIsInitialized() const override {
    return impl_->SpeakerIsInitialized();
  }
  int32_t InitMicrophone() override { return impl_->InitMicrophone(); }
  bool MicrophoneIsInitialized() const override {
    return impl_->MicrophoneIsInitialized();
  }
  int32_t StereoPlayoutIsAvailable(bool* available) const override {
    return impl_->StereoPlayoutIsAvailable(available);
  }
  int32_t SetStereoPlayout(bool enable) override {
    return impl_->SetStereoPlayout(enable);
  }
  int32_t StereoPlayout(bool* enabled) const override {
    return impl_->StereoPlayout(enabled);
  }
  int32_t StereoRecordingIsAvailable(bool* available) const override {
    return impl_->StereoRecordingIsAvailable(available);
  }
  int32_t SetStereoRecording(bool enable) override {
    return impl_->SetStereoRecording(enable);
  }
  int32_t StereoRecording(bool* enabled) const override {
    return impl_->StereoRecording(enabled);
  }
  int32_t PlayoutDelay(uint16_t* delay_ms) const override {
    return impl_->PlayoutDelay(delay_ms);
  }
  bool BuiltInAECIsAvailable() const override {
    return impl_->BuiltInAECIsAvailable();
  }
  bool BuiltInAGCIsAvailable() const override {
    return impl_->BuiltInAGCIsAvailable();
  }
  bool BuiltInNSIsAvailable() const override {
    return impl_->BuiltInNSIsAvailable();
  }
  int32_t EnableBuiltInAEC(bool enable) override {
    return impl_->EnableBuiltInAEC(enable);
  }
  int32_t EnableBuiltInAGC(bool enable) override {
    return impl_->EnableBuiltInAGC(enable);
  }
  int32_t EnableBuiltInNS(bool enable) override {
    return impl_->EnableBuiltInNS(enable);
  }
  int32_t GetPlayoutUnderrunCount() const override {
    return impl_->GetPlayoutUnderrunCount();
  }

 protected:
  rtc::scoped_refptr<AudioDeviceModule> impl_;
  AudioDeviceDataObserver* legacy_observer_ = nullptr;
  std::unique_ptr<AudioDeviceDataObserver> observer_;
  AudioTransport* audio_transport_ = nullptr;
  bool is_valid_ = false;
};

}  // namespace

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceWithDataObserver(
    rtc::scoped_refptr<AudioDeviceModule> impl,
    std::unique_ptr<AudioDeviceDataObserver> observer) {
  auto audio_device = rtc::make_ref_counted<ADMWrapper>(impl, observer.get(),
                                                        std::move(observer));

  if (!audio_device->IsValid()) {
    return nullptr;
  }

  return audio_device;
}

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceWithDataObserver(
    rtc::scoped_refptr<AudioDeviceModule> impl,
    AudioDeviceDataObserver* legacy_observer) {
  auto audio_device =
      rtc::make_ref_counted<ADMWrapper>(impl, legacy_observer, nullptr);

  if (!audio_device->IsValid()) {
    return nullptr;
  }

  return audio_device;
}

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceWithDataObserver(
    AudioDeviceModule::AudioLayer audio_layer,
    TaskQueueFactory* task_queue_factory,
    std::unique_ptr<AudioDeviceDataObserver> observer) {
  auto audio_device = rtc::make_ref_counted<ADMWrapper>(
      audio_layer, task_queue_factory, observer.get(), std::move(observer));

  if (!audio_device->IsValid()) {
    return nullptr;
  }

  return audio_device;
}

rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceWithDataObserver(
    AudioDeviceModule::AudioLayer audio_layer,
    TaskQueueFactory* task_queue_factory,
    AudioDeviceDataObserver* legacy_observer) {
  auto audio_device = rtc::make_ref_counted<ADMWrapper>(
      audio_layer, task_queue_factory, legacy_observer, nullptr);

  if (!audio_device->IsValid()) {
    return nullptr;
  }

  return audio_device;
}
}  // namespace webrtc
