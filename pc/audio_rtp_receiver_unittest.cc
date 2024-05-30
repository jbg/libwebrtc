/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/audio_rtp_receiver.h"

#include <atomic>
#include <limits>
#include <utility>

#include "pc/test/mock_voice_media_receive_channel_interface.h"
#include "rtc_base/gunit.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"
#include "test/time_controller/simulated_time_controller.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NotNull;

static const int kTimeOut = 100;
static const double kDefaultVolume = 1;
static const double kVolume = 3.7;
static const double kVolumeMuted = 0.0;
static const uint32_t kSsrc = 3;

namespace webrtc {
class AudioRtpReceiverTest : public ::testing::Test {
 protected:
  AudioRtpReceiverTest()
      : worker_(rtc::Thread::Current()),
        receiver_(
            rtc::make_ref_counted<AudioRtpReceiver>(worker_,
                                                    std::string(),
                                                    std::vector<std::string>(),
                                                    false)) {
    EXPECT_CALL(receive_channel_, SetRawAudioSink(kSsrc, NotNull()));
    EXPECT_CALL(receive_channel_, SetRawAudioSink(kSsrc, IsNull()));
    EXPECT_CALL(receive_channel_, SetBaseMinimumPlayoutDelayMs(kSsrc, _));
  }

  ~AudioRtpReceiverTest() {
    EXPECT_CALL(receive_channel_, SetOutputVolume(kSsrc, kVolumeMuted));
    receiver_->SetMediaChannel(nullptr);
  }

  rtc::AutoThread main_thread_;
  rtc::Thread* worker_;
  rtc::scoped_refptr<AudioRtpReceiver> receiver_;
  cricket::MockVoiceMediaReceiveChannelInterface receive_channel_;
};

TEST_F(AudioRtpReceiverTest, SetOutputVolumeIsCalled) {
  std::atomic_int set_volume_calls(0);

  EXPECT_CALL(receive_channel_, SetOutputVolume(kSsrc, kDefaultVolume))
      .WillOnce(InvokeWithoutArgs([&] {
        set_volume_calls++;
        return true;
      }));

  receiver_->track();
  receiver_->track()->set_enabled(true);
  receiver_->SetMediaChannel(&receive_channel_);
  EXPECT_CALL(receive_channel_, SetDefaultRawAudioSink(_)).Times(0);
  receiver_->SetupMediaChannel(kSsrc);

  EXPECT_CALL(receive_channel_, SetOutputVolume(kSsrc, kVolume))
      .WillOnce(InvokeWithoutArgs([&] {
        set_volume_calls++;
        return true;
      }));

  receiver_->OnSetVolume(kVolume);
  EXPECT_TRUE_WAIT(set_volume_calls == 2, kTimeOut);
}

TEST_F(AudioRtpReceiverTest, VolumesSetBeforeStartingAreRespected) {
  // Set the volume before setting the media channel. It should still be used
  // as the initial volume.
  receiver_->OnSetVolume(kVolume);

  receiver_->track()->set_enabled(true);
  receiver_->SetMediaChannel(&receive_channel_);

  // The previosly set initial volume should be propagated to the provided
  // media_channel_ as soon as SetupMediaChannel is called.
  EXPECT_CALL(receive_channel_, SetOutputVolume(kSsrc, kVolume));

  receiver_->SetupMediaChannel(kSsrc);
}

// Tests that OnChanged notifications are processed correctly on the worker
// thread when a media channel pointer is passed to the receiver via the
// constructor.
TEST(AudioRtpReceiver, OnChangedNotificationsAfterConstruction) {
  test::RunLoop loop;
  auto* thread = rtc::Thread::Current();  // Points to loop's thread.
  cricket::MockVoiceMediaReceiveChannelInterface receive_channel;
  auto receiver = rtc::make_ref_counted<AudioRtpReceiver>(
      thread, std::string(), std::vector<std::string>(), true,
      &receive_channel);

  EXPECT_CALL(receive_channel, SetDefaultRawAudioSink(IsNull())).Times(1);
  EXPECT_CALL(receive_channel, SetDefaultRawAudioSink(NotNull())).Times(1);
  EXPECT_CALL(receive_channel, SetDefaultOutputVolume(kDefaultVolume)).Times(1);
  receiver->SetupUnsignaledMediaChannel();
  loop.Flush();

  // Mark the track as disabled.
  receiver->track()->set_enabled(false);

  // When the track was marked as disabled, an async notification was queued
  // for the worker thread. This notification should trigger the volume
  // of the media channel to be set to kVolumeMuted.
  // Flush the worker thread, but set the expectation first for the call.
  EXPECT_CALL(receive_channel, SetDefaultOutputVolume(kVolumeMuted)).Times(1);
  loop.Flush();

  EXPECT_CALL(receive_channel, SetDefaultOutputVolume(kVolumeMuted)).Times(1);
  receiver->SetMediaChannel(nullptr);
}

TEST(AudioRtpReceiver, SourceStateMutedWhenNoPacketsArrive) {
  // Start the clock close to max uint32_t and run the tests twice, once before
  // wrap-around and once with wrap-around considered.
  webrtc::GlobalSimulatedTimeController time_controller(
      Timestamp::Millis(std::numeric_limits<uint32_t>::max() - 2000));
  cricket::MockVoiceMediaReceiveChannelInterface media_channel;
  auto receiver = rtc::make_ref_counted<AudioRtpReceiver>(
      time_controller.GetMainThread(), std::string(),
      std::vector<std::string>(), true, &media_channel);

  constexpr uint32_t kSsrc = 123;

  EXPECT_CALL(media_channel, SetBaseMinimumPlayoutDelayMs(kSsrc, _));
  EXPECT_CALL(media_channel, SetRawAudioSink(kSsrc, IsNull())).Times(1);
  EXPECT_CALL(media_channel, SetRawAudioSink(kSsrc, NotNull())).Times(1);
  EXPECT_CALL(media_channel, SetOutputVolume(kSsrc, _)).Times(1);

  // Grab the callback object.
  absl::AnyInvocable<void(uint32_t, absl::optional<uint8_t>)> level_callback;
  EXPECT_CALL(media_channel,
              SetAudioLevelCallback(absl::optional<uint32_t>(kSsrc), _))
      .WillRepeatedly(testing::Invoke(
          [&](absl::optional<uint32_t> ssrc,
              absl::AnyInvocable<void(uint32_t, absl::optional<uint8_t>)> cb) {
            level_callback = std::move(cb);
          }));
  receiver->SetupMediaChannel(kSsrc);
  ASSERT_TRUE(level_callback);

  auto track = receiver->audio_track();
  EXPECT_TRUE(track->enabled());
  EXPECT_EQ(track->state(), AudioTrackInterface::kLive);
  auto* source = track->GetSource();

  // Mock class to verify that we get state change notifications from the track
  // whenever the source state changes.
  class MockObserver : public ObserverInterface {
   public:
    MOCK_METHOD(void, OnChanged, (), (override));
  } observer;

  track->RegisterObserver(&observer);

  for (int i = 0; i < 2; ++i) {
    // Simulate the first audio packet arriving.
    level_callback(time_controller.GetClock()->CurrentTime().ms(), 30);
    EXPECT_EQ(source->state(), AudioSourceInterface::kLive);
    EXPECT_CALL(observer, OnChanged()).Times(0);
    time_controller.AdvanceTime(TimeDelta::Millis(20));

    // Now simulate no packets arriving. Passing `absl::nullopt` is what
    // SourceTracker will do when no packets have arrived from a remote source
    // based on a timeout interval. See `SourceTracker::kTimeout`.
    // The state of the source should transition to `kMuted`.
    const auto muted_timestamp = time_controller.GetClock()->CurrentTime();
    level_callback(muted_timestamp.ms(), absl::nullopt);
    EXPECT_CALL(observer, OnChanged()).Times(1);
    time_controller.AdvanceTime(TimeDelta::Millis(20));
    EXPECT_EQ(source->state(), AudioSourceInterface::kMuted);
    testing::Mock::VerifyAndClearExpectations(&observer);

    // Wake up the track again with a valid audio packet again.
    const auto wakeup_timestamp = time_controller.GetClock()->CurrentTime();
    level_callback(wakeup_timestamp.ms(), 50);
    EXPECT_CALL(observer, OnChanged()).Times(1);
    time_controller.AdvanceTime(TimeDelta::Millis(20));
    EXPECT_EQ(source->state(), AudioSourceInterface::kLive);
    testing::Mock::VerifyAndClearExpectations(&observer);

    // Deliver an out-of-order 'muted' packet, pretend that we're back at the
    // `muted_timestamp` point in time. This callback should now be ignored and
    // the state should remain `kLive`.
    level_callback(muted_timestamp.ms(), absl::nullopt);
    EXPECT_CALL(observer, OnChanged()).Times(0);
    time_controller.AdvanceTime(TimeDelta::Millis(20));
    EXPECT_EQ(source->state(), AudioSourceInterface::kLive);
    testing::Mock::VerifyAndClearExpectations(&observer);

    if (i == 0) {
      // Skip a bit into the future, close to the point of max uint32_t
      // (max-25ms), and do the same thing again. This time, the timestamp
      // passed to the callback function, will wrap-around.
      auto now = time_controller.GetClock()->CurrentTime();
      time_controller.AdvanceTime(TimeDelta::Millis(
          std::numeric_limits<uint32_t>::max() - now.ms() - 25));
    }
  }

  track->UnregisterObserver(&observer);

  EXPECT_CALL(media_channel, SetOutputVolume(kSsrc, kVolumeMuted));
  receiver->SetMediaChannel(nullptr);
  ASSERT_FALSE(level_callback);
}

}  // namespace webrtc
