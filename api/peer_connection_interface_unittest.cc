/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/peer_connection_interface.h"

#include <utility>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_media_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;

TEST(CreateModularPeerConnectionFactory, CreateWithMinimumDependencies) {
  PeerConnectionFactoryDependencies deps;
  deps.task_queue_factory = CreateDefaultTaskQueueFactory();
  EXPECT_THAT(CreateModularPeerConnectionFactory(std::move(deps)), NotNull());
}

TEST(CreateModularPeerConnectionFactory, CreateWithMedia) {
  PeerConnectionFactoryDependencies deps;
  deps.task_queue_factory = CreateDefaultTaskQueueFactory();
  deps.media_factory = CreateMediaFactory();
  deps.audio_encoder_factory = CreateBuiltinAudioEncoderFactory();
  deps.audio_decoder_factory = CreateBuiltinAudioDecoderFactory();
  deps.video_encoder_factory = CreateBuiltinVideoEncoderFactory();
  deps.video_decoder_factory = CreateBuiltinVideoDecoderFactory();

  rtc::scoped_refptr<PeerConnectionFactoryInterface> pcf =
      CreateModularPeerConnectionFactory(std::move(deps));
  EXPECT_THAT(pcf, NotNull());
  EXPECT_THAT(pcf->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_AUDIO).codecs,
              Not(IsEmpty()));
  EXPECT_THAT(pcf->GetRtpSenderCapabilities(cricket::MEDIA_TYPE_VIDEO).codecs,
              Not(IsEmpty()));
}

}  // namespace
}  // namespace webrtc
