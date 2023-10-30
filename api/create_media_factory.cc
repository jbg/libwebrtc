/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/create_media_factory.h"

#include <utility>

#include "api/media_factory.h"
#include "api/peer_connection_interface.h"
#include "call/call_factory.h"
#include "media/engine/webrtc_media_engine.h"
#include "media/engine/webrtc_voice_engine.h"

#ifdef HAVE_WEBRTC_VIDEO
#include "media/engine/webrtc_video_engine.h"
#else
#include "media/engine/null_webrtc_video_engine.h"
#endif

namespace webrtc {
namespace {

using ::cricket::CompositeMediaEngine;
using ::cricket::MediaEngineInterface;
using ::cricket::WebRtcVoiceEngine;

class MediaFactoryImpl : public MediaFactory {
 public:
  MediaFactoryImpl() = default;
  MediaFactoryImpl(const MediaFactoryImpl&) = delete;
  MediaFactoryImpl& operator=(const MediaFactoryImpl&) = delete;
  ~MediaFactoryImpl() override = default;

 private:
  std::unique_ptr<Call> CreateCall(const CallConfig& config) override {
    CallFactory call_factory;
    return static_cast<CallFactoryInterface&>(call_factory).CreateCall(config);
  }

  std::unique_ptr<MediaEngineInterface> CreateMediaEngine(
      PeerConnectionFactoryDependencies& deps) override {
    std::unique_ptr<FieldTrialsView> fallback_trials;
    const webrtc::FieldTrialsView* trials;
    if (deps.trials) {
      trials = deps.trials.get();
    } else {
      fallback_trials = std::make_unique<FieldTrialBasedConfig>();
      trials = fallback_trials.get();
    }
    auto audio_engine = std::make_unique<WebRtcVoiceEngine>(
        deps.task_queue_factory.get(), deps.adm.get(),
        std::move(deps.audio_encoder_factory),
        std::move(deps.audio_decoder_factory), std::move(deps.audio_mixer),
        std::move(deps.audio_processing), /*audio_frame_processor=*/nullptr,
        /*owned_audio_frame_processor=*/std::move(deps.audio_frame_processor),
        *trials);
#ifdef HAVE_WEBRTC_VIDEO
    auto video_engine = std::make_unique<cricket::WebRtcVideoEngine>(
        std::move(deps.video_encoder_factory),
        std::move(deps.video_decoder_factory), *trials);
#else
    auto video_engine = std::make_unique<cricket::NullWebRtcVideoEngine>();
#endif
    return std::make_unique<CompositeMediaEngine>(std::move(fallback_trials),
                                                  std::move(audio_engine),
                                                  std::move(video_engine));
  }
};

}  // namespace

std::unique_ptr<MediaFactory> CreateMediaFactory() {
  return std::make_unique<MediaFactoryImpl>();
}

}  // namespace webrtc
