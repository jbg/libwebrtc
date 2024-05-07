/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_factory.h"
#include "modules/rtp_rtcp/source/rtp_descriptor_authentication.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

// Using a reasonable default of 10ms for the retransmission delay for frames
// not coming from this sender's encoder. This is usually taken from an
// estimate of the RTT of the link,so 10ms should be a reasonable estimate for
// frames being re-transmitted to a peer, probably on the same network.
const TimeDelta kDefaultRetransmissionsTime = TimeDelta::Millis(10);

class TransformableVideoSenderFrame : public TransformableVideoFrameInterface {
 public:
  TransformableVideoSenderFrame(
      RTPVideoFrameSenderInterface::RtpVideoFrame video_frame,
      uint32_t ssrc)
      : video_frame_(std::move(video_frame)), ssrc_(ssrc) {
    video_frame_.encoded_output_size = video_frame_.payload->size();
  }

  ~TransformableVideoSenderFrame() override = default;

  // Implements TransformableVideoFrameInterface.
  rtc::ArrayView<const uint8_t> GetData() const override {
    return *video_frame_.payload;
  }

  void SetData(rtc::ArrayView<const uint8_t> data) override {
    video_frame_.payload = EncodedImageBuffer::Create(data.data(), data.size());
  }

  size_t GetPreTransformPayloadSize() const {
    return video_frame_.encoded_output_size;
  }

  uint32_t GetTimestamp() const override { return video_frame_.rtp_timestamp; }
  void SetRTPTimestamp(uint32_t timestamp) override {
    video_frame_.rtp_timestamp = timestamp;
  }

  uint32_t GetSsrc() const override { return ssrc_; }

  bool IsKeyFrame() const override {
    return video_frame_.video_header.frame_type ==
           VideoFrameType::kVideoFrameKey;
  }

  VideoFrameMetadata Metadata() const override {
    VideoFrameMetadata metadata = video_frame_.video_header.GetAsMetadata();
    metadata.SetSsrc(ssrc_);
    metadata.SetCsrcs(video_frame_.csrcs);
    return metadata;
  }

  void SetMetadata(const VideoFrameMetadata& metadata) override {
    video_frame_.video_header.SetFromMetadata(metadata);
    ssrc_ = metadata.GetSsrc();
    video_frame_.csrcs = metadata.GetCsrcs();
  }

  const RTPVideoHeader& GetHeader() const { return video_frame_.video_header; }
  uint8_t GetPayloadType() const override { return video_frame_.payload_type; }
  absl::optional<VideoCodecType> GetCodecType() const {
    return video_frame_.codec_type;
  }
  Timestamp GetCaptureTime() const { return video_frame_.capture_time; }
  absl::optional<Timestamp> GetCaptureTimeIdentifier() const override {
    return video_frame_.capture_time_identifier;
  }

  TimeDelta GetExpectedRetransmissionTime() const {
    return video_frame_.expected_retransmission_time;
  }

  Direction GetDirection() const override { return Direction::kSender; }
  std::string GetMimeType() const override {
    if (!video_frame_.codec_type.has_value()) {
      return "video/x-unknown";
    }
    std::string mime_type = "video/";
    return mime_type + CodecTypeToPayloadString(*video_frame_.codec_type);
  }

  RTPVideoFrameSenderInterface::RtpVideoFrame ExtractVideoFrame() {
    return std::move(video_frame_);
  }

 private:
  RTPVideoFrameSenderInterface::RtpVideoFrame video_frame_;
  uint32_t ssrc_;
};
}  // namespace

RTPSenderVideoFrameTransformerDelegate::RTPSenderVideoFrameTransformerDelegate(
    RTPVideoFrameSenderInterface* sender,
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
    uint32_t ssrc,
    TaskQueueFactory* task_queue_factory)
    : sender_(sender),
      frame_transformer_(std::move(frame_transformer)),
      ssrc_(ssrc),
      transformation_queue_(task_queue_factory->CreateTaskQueue(
          "video_frame_transformer",
          TaskQueueFactory::Priority::NORMAL)) {}

void RTPSenderVideoFrameTransformerDelegate::Init() {
  frame_transformer_->RegisterTransformedFrameSinkCallback(
      rtc::scoped_refptr<TransformedFrameCallback>(this), ssrc_);
}

bool RTPSenderVideoFrameTransformerDelegate::Send(
    RTPVideoFrameSenderInterface::RtpVideoFrame video_frame) {
  {
    MutexLock lock(&sender_lock_);
    if (short_circuit_) {
      sender_->Send(std::move(video_frame));
      return true;
    }
  }
  frame_transformer_->Transform(std::make_unique<TransformableVideoSenderFrame>(
      std::move(video_frame), ssrc_));
  return true;
}

void RTPSenderVideoFrameTransformerDelegate::OnTransformedFrame(
    std::unique_ptr<TransformableFrameInterface> frame) {
  MutexLock lock(&sender_lock_);

  if (!sender_) {
    return;
  }
  rtc::scoped_refptr<RTPSenderVideoFrameTransformerDelegate> delegate(this);
  transformation_queue_->PostTask(
      [delegate = std::move(delegate), frame = std::move(frame)]() mutable {
        RTC_DCHECK_RUN_ON(delegate->transformation_queue_.get());
        delegate->SendVideo(std::move(frame));
      });
}

void RTPSenderVideoFrameTransformerDelegate::StartShortCircuiting() {
  MutexLock lock(&sender_lock_);
  short_circuit_ = true;
}

void RTPSenderVideoFrameTransformerDelegate::SendVideo(
    std::unique_ptr<TransformableFrameInterface> transformed_frame) const {
  RTC_DCHECK_RUN_ON(transformation_queue_.get());
  MutexLock lock(&sender_lock_);
  if (!sender_)
    return;
  if (transformed_frame->GetDirection() ==
      TransformableFrameInterface::Direction::kSender) {
    auto* transformed_video_frame =
        static_cast<TransformableVideoSenderFrame*>(transformed_frame.get());
    sender_->Send(transformed_video_frame->ExtractVideoFrame());
  } else {
    auto* transformed_video_frame =
        static_cast<TransformableVideoFrameInterface*>(transformed_frame.get());
    VideoFrameMetadata metadata = transformed_video_frame->Metadata();
    // TODO(bugs.webrtc.org/14708): Use an actual RTT estimate for the
    // retransmission time instead of a const default, in the same way as a
    // locally encoded frame.
    rtc::ArrayView<const uint8_t> payload = transformed_video_frame->GetData();
    sender_->Send({
        .payload_type = transformed_video_frame->GetPayloadType(),
        .codec_type = metadata.GetCodec(),
        .rtp_timestamp = transformed_video_frame->GetTimestamp(),
        .payload = EncodedImageBuffer::Create(payload.data(), payload.size()),
        .encoded_output_size = payload.size(),
        .video_header = RTPVideoHeader::FromMetadata(metadata),
        .expected_retransmission_time = kDefaultRetransmissionsTime,
        .csrcs = metadata.GetCsrcs(),
    });
  }
}

void RTPSenderVideoFrameTransformerDelegate::SetVideoStructure(
    const FrameDependencyStructure* video_structure) {
  MutexLock lock(&sender_lock_);
  RTC_CHECK(sender_);
  sender_->SetVideoStructure(video_structure);
}

void RTPSenderVideoFrameTransformerDelegate::SetVideoLayersAllocation(
    VideoLayersAllocation allocation) {
  MutexLock lock(&sender_lock_);
  RTC_CHECK(sender_);
  sender_->SetVideoLayersAllocation(std::move(allocation));
}

void RTPSenderVideoFrameTransformerDelegate::Reset() {
  frame_transformer_->UnregisterTransformedFrameSinkCallback(ssrc_);
  frame_transformer_ = nullptr;
  {
    MutexLock lock(&sender_lock_);
    sender_ = nullptr;
  }
}

std::unique_ptr<TransformableVideoFrameInterface> CloneSenderVideoFrame(
    TransformableVideoFrameInterface* original) {
  rtc::ArrayView<const uint8_t> payload = original->GetData();
  VideoFrameMetadata metadata = original->Metadata();
  RTPVideoHeader new_header = RTPVideoHeader::FromMetadata(metadata);
  RTPVideoFrameSenderInterface::RtpVideoFrame video_frame = {
      .payload_type = original->GetPayloadType(),
      .codec_type = new_header.codec,
      .rtp_timestamp = original->GetTimestamp(),
      .payload = EncodedImageBuffer::Create(payload.data(), payload.size()),
      .video_header = new_header,
      .expected_retransmission_time = kDefaultRetransmissionsTime,
      .csrcs = metadata.GetCsrcs(),
  };

  // TODO(bugs.webrtc.org/14708): Fill in other EncodedImage parameters
  // TODO(bugs.webrtc.org/14708): Use an actual RTT estimate for the
  // retransmission time instead of a const default, in the same way as a
  // locally encoded frame.
  return std::make_unique<TransformableVideoSenderFrame>(std::move(video_frame),
                                                         original->GetSsrc());
}

}  // namespace webrtc
