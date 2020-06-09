/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "pc/test/fake_periodic_video_source.h"
#include "pc/test/peer_connection_test_wrapper.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

const int64_t kDefaultTimeoutMs = 5000;

class PeerConnectionAdaptationIntegrationTest : public ::testing::Test {
 public:
  // The PeerConnectionTestWrapper uses default fake source configs.
  // TODO(hbos): Update PeerConnectionTestWrapper to make this dependency
  // explicit.
  static constexpr int kMaxSenderWidth = FakePeriodicVideoSource::kDefaultWidth;

  PeerConnectionAdaptationIntegrationTest()
      : virtual_socket_server_(),
        network_thread_(new rtc::Thread(&virtual_socket_server_)),
        worker_thread_(rtc::Thread::Create()),
        caller_(nullptr),
        callee_(nullptr),
        caller_video_sender_(nullptr) {
    RTC_CHECK(network_thread_->Start());
    RTC_CHECK(worker_thread_->Start());

    caller_ = new rtc::RefCountedObject<PeerConnectionTestWrapper>(
        "caller", network_thread_.get(), worker_thread_.get());
    callee_ = new rtc::RefCountedObject<PeerConnectionTestWrapper>(
        "callee", network_thread_.get(), worker_thread_.get());

    PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    EXPECT_TRUE(caller_->CreatePc(config, CreateBuiltinAudioEncoderFactory(),
                                  CreateBuiltinAudioDecoderFactory()));
    EXPECT_TRUE(callee_->CreatePc(config, CreateBuiltinAudioEncoderFactory(),
                                  CreateBuiltinAudioDecoderFactory()));
    // Wires up ICE candidates and SDP to be exchanged in response to events and
    // callbacks.
    PeerConnectionTestWrapper::Connect(caller_.get(), callee_.get());
    // Add a video-only stream.
    caller_->GetAndAddUserMedia(false, cricket::AudioOptions(), true);
    auto senders = caller_->pc()->GetSenders();
    caller_video_sender_ = senders[0];
  }

  void PerformOfferAnswer() {
    // The test wrapper takes care of the entire offer/answer exchange in
    // response to CreateOffer().
    caller_->CreateOffer(PeerConnectionInterface::RTCOfferAnswerOptions());
  }

 protected:
  rtc::VirtualSocketServer virtual_socket_server_;
  std::unique_ptr<rtc::Thread> network_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  rtc::scoped_refptr<PeerConnectionTestWrapper> caller_;
  rtc::scoped_refptr<PeerConnectionTestWrapper> callee_;
  rtc::scoped_refptr<RtpSenderInterface> caller_video_sender_;
};

TEST_F(PeerConnectionAdaptationIntegrationTest, HelloWorld) {
  PerformOfferAnswer();
  // Wait until we are connected. This ensures O/A has completed.
  EXPECT_TRUE_WAIT(caller_->pc()->peer_connection_state() ==
                       PeerConnectionInterface::PeerConnectionState::kConnected,
                   kDefaultTimeoutMs);
  // After negotiation, the callee will have a renderer that its receiving track
  // is attached to.
  EXPECT_NE(nullptr, callee_->renderer());
  // Ensure resolution ramps up to the sender maximum.
  EXPECT_TRUE_WAIT(callee_->renderer()->width() == kMaxSenderWidth,
                   kDefaultTimeoutMs);

  // TODO(hbos): Now trigger adapting down.

  // Wait until adaptation is applied.
  EXPECT_TRUE_WAIT(callee_->renderer()->width() < kMaxSenderWidth,
                   kDefaultTimeoutMs);
}

}  // namespace webrtc
