/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_send_frame_transformer_delegate.h"

#include <memory>
#include <utility>

#include "api/test/mock_frame_transformer.h"
#include "api/test/mock_transformable_frame.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/task_queue_for_test.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

void RunAllTasks(TaskQueueForTest* queue) {
  // Post an empty task on the queue and wait for it to finish, to ensure
  // that all existing tasks posted on the queue get executed.
  queue->SendTask([]() {}, RTC_FROM_HERE);
}

class MockChannelSend {
 public:
  MockChannelSend() = default;
  ~MockChannelSend() = default;

  MOCK_METHOD(int32_t,
              SendFrame,
              (AudioFrameType frameType,
               uint8_t payloadType,
               uint32_t rtp_timestamp,
               rtc::ArrayView<const uint8_t> payload,
               int64_t absolute_capture_timestamp_ms));

  ChannelSendFrameTransformerDelegate::SendFrameCallback callback() {
    return [this](AudioFrameType frameType, uint8_t payloadType,
                  uint32_t rtp_timestamp, rtc::ArrayView<const uint8_t> payload,
                  int64_t absolute_capture_timestamp_ms) {
      return SendFrame(frameType, payloadType, rtp_timestamp, payload,
                       absolute_capture_timestamp_ms);
    };
  }
};

TEST(ChannelSendFrameTransformerDelegateTest,
     RegisterTransformedFrameCallbackOnInit) {
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      new rtc::RefCountedObject<MockFrameTransformer>();
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      new rtc::RefCountedObject<ChannelSendFrameTransformerDelegate>(
          ChannelSendFrameTransformerDelegate::SendFrameCallback(),
          mock_frame_transformer, nullptr);
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback);
  delegate->Init();
}

TEST(ChannelSendFrameTransformerDelegateTest,
     UnregisterTransformedFrameCallbackOnReset) {
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      new rtc::RefCountedObject<MockFrameTransformer>();
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      new rtc::RefCountedObject<ChannelSendFrameTransformerDelegate>(
          ChannelSendFrameTransformerDelegate::SendFrameCallback(),
          mock_frame_transformer, nullptr);
  EXPECT_CALL(*mock_frame_transformer, UnregisterTransformedFrameCallback);
  delegate->Reset();
}

TEST(ChannelSendFrameTransformerDelegateTest,
     TransformRunsChannelSendCallback) {
  TaskQueueForTest channel_queue("channel_queue");
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      new rtc::RefCountedObject<MockFrameTransformer>();
  MockChannelSend mock_channel;
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      new rtc::RefCountedObject<ChannelSendFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, &channel_queue);
  rtc::scoped_refptr<TransformedFrameCallback> callback;
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback)
      .WillOnce(::testing::Invoke(
          [&callback](rtc::scoped_refptr<TransformedFrameCallback> cb) {
            callback = cb;
          }));
  delegate->Init();
  ASSERT_TRUE(callback);

  const uint8_t data[] = {1, 2, 3, 4};
  EXPECT_CALL(mock_channel, SendFrame);
  EXPECT_CALL(*mock_frame_transformer, Transform)
      .WillOnce(::testing::Invoke(
          [&callback](std::unique_ptr<TransformableFrameInterface> frame) {
            callback->OnTransformedFrame(std::move(frame));
          }));
  delegate->Transform(AudioFrameType::kEmptyFrame, 0, 0, 0, data, sizeof(data),
                      0, 0);
  RunAllTasks(&channel_queue);
}

TEST(ChannelSendFrameTransformerDelegateTest,
     OnTransformedDoesNotRunChannelSendCallbackAfterReset) {
  TaskQueueForTest channel_queue("channel_queue");
  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      new rtc::RefCountedObject<testing::NiceMock<MockFrameTransformer>>();
  MockChannelSend mock_channel;
  rtc::scoped_refptr<ChannelSendFrameTransformerDelegate> delegate =
      new rtc::RefCountedObject<ChannelSendFrameTransformerDelegate>(
          mock_channel.callback(), mock_frame_transformer, &channel_queue);

  delegate->Reset();
  EXPECT_CALL(mock_channel, SendFrame).Times(0);
  delegate->OnTransformedFrame(std::make_unique<MockTransformableFrame>());
  RunAllTasks(&channel_queue);
}

}  // namespace
}  // namespace webrtc
