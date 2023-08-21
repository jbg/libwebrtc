/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_dumping_encoder.h"

#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/file_wrapper.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace {

class FrameDumpingEncoder : public VideoEncoder, public EncodedImageCallback {
 public:
  FrameDumpingEncoder(std::unique_ptr<VideoEncoder> wrapped, FileWrapper file)
      : wrapped_(std::move(wrapped)),
        writer_(IvfFileWriter::Wrap(std::move(file),
                                    /* byte_limit= */ 100000000)) {}

  // VideoEncoder overloads.
  void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override) override {
    wrapped_->SetFecControllerOverride(fec_controller_override);
  }
  int InitEncode(const VideoCodec* codec_settings,
                 const VideoEncoder::Settings& settings) override {
    codec_settings_ = *codec_settings;
    return wrapped_->InitEncode(codec_settings, settings);
  }
  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override {
    callback_ = callback;
    return wrapped_->RegisterEncodeCompleteCallback(this);
  }
  int32_t Release() override { return wrapped_->Release(); }
  int32_t Encode(const VideoFrame& frame,
                 const std::vector<VideoFrameType>* frame_types) override {
    return wrapped_->Encode(frame, frame_types);
  }
  void SetRates(const RateControlParameters& parameters) override {
    wrapped_->SetRates(parameters);
  }
  void OnPacketLossRateUpdate(float packet_loss_rate) override {
    wrapped_->OnPacketLossRateUpdate(packet_loss_rate);
  }
  void OnRttUpdate(int64_t rtt_ms) override { wrapped_->OnRttUpdate(rtt_ms); }
  void OnLossNotification(const LossNotification& loss_notification) override {
    wrapped_->OnLossNotification(loss_notification);
  }
  EncoderInfo GetEncoderInfo() const override {
    return wrapped_->GetEncoderInfo();
  }

  // EncodedImageCallback overrides.
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info) override {
    writer_->WriteFrame(encoded_image, codec_settings_.codecType);
    return callback_->OnEncodedImage(encoded_image, codec_specific_info);
  }
  void OnDroppedFrame(DropReason reason) override {
    callback_->OnDroppedFrame(reason);
  }

 private:
  std::unique_ptr<VideoEncoder> wrapped_;
  std::unique_ptr<IvfFileWriter> writer_;
  VideoCodec codec_settings_;
  EncodedImageCallback* callback_ = nullptr;
};

}  // namespace

std::unique_ptr<VideoEncoder> MaybeCreateFrameDumpingEncoderWrapper(
    std::unique_ptr<VideoEncoder> encoder,
    const FieldTrialsView& field_trials) {
  auto output_directory =
      field_trials.Lookup("WebRTC-EncoderDataDumpDirectory");
  if (output_directory.empty() || !encoder) {
    return encoder;
  }
  absl::c_replace(output_directory, ';', '/');
  char filename_buffer[1024];
  rtc::SimpleStringBuilder builder(filename_buffer);
  builder << output_directory << "/webrtc_encoded_frames"
          << "-" << rtc::TimeMicros() << ".ivf";
  return std::make_unique<FrameDumpingEncoder>(
      std::move(encoder), FileWrapper::OpenWriteOnly(builder.str()));
}

}  // namespace webrtc
