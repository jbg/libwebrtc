/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "media/engine/webrtc_media_engine_defaults.h"

#include <memory>
#include <string>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "media/engine/internal_decoder_factory.h"
#include "media/engine/internal_encoder_factory.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// This class wraps the internal factory and adds simulcast.
class InternalEncoderFactoryWithSimulcast : public VideoEncoderFactory {
 public:
  InternalEncoderFactoryWithSimulcast() = default;
  ~InternalEncoderFactoryWithSimulcast() override = default;

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override {
    // Try creating an InternalEncoderFactory-backed SimulcastEncoderAdapter.
    // The adapter has a passthrough mode for the case that simulcast is not
    // used, so all responsibility can be delegated to it.
    if (format.IsCodecInList(factory_.GetSupportedFormats())) {
      return std::make_unique<SimulcastEncoderAdapter>(&factory_, format);
    }

    return nullptr;
  }

  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    return factory_.GetSupportedFormats();
  }

  CodecSupport QueryCodecSupport(
      const SdpVideoFormat& format,
      absl::optional<std::string> scalability_mode) const override {
    return factory_.QueryCodecSupport(format, scalability_mode);
  }

 private:
  InternalEncoderFactory factory_;
};

}  // namespace

void SetMediaEngineDefaults(cricket::MediaEngineDependencies* deps) {
  RTC_DCHECK(deps);
  if (deps->task_queue_factory == nullptr) {
    static TaskQueueFactory* const task_queue_factory =
        CreateDefaultTaskQueueFactory().release();
    deps->task_queue_factory = task_queue_factory;
  }
  if (deps->audio_encoder_factory == nullptr)
    deps->audio_encoder_factory = CreateBuiltinAudioEncoderFactory();
  if (deps->audio_decoder_factory == nullptr)
    deps->audio_decoder_factory = CreateBuiltinAudioDecoderFactory();
  if (deps->audio_processing == nullptr)
    deps->audio_processing = AudioProcessingBuilder().Create();

  if (deps->video_encoder_factory == nullptr)
    deps->video_encoder_factory =
        std::make_unique<InternalEncoderFactoryWithSimulcast>();
  if (deps->video_decoder_factory == nullptr)
    deps->video_decoder_factory = std::make_unique<InternalDecoderFactory>();
}

}  // namespace webrtc
