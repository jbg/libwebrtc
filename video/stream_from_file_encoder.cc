#include <iostream>
/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "stream_from_file_encoder.h"

#include <string.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "api/task_queue/queued_task.h"
#include "api/video/video_content_type.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "system_wrappers/include/sleep.h"

namespace webrtc {

std::unique_ptr<VideoEncoder> StreamFromFileEncoder::Create(
    std::string filename) {
  return absl::make_unique<StreamFromFileEncoder>(filename);
}

StreamFromFileEncoder::StreamFromFileEncoder(std::string filename)
    : callback_(nullptr),
      configured_input_framerate_(-1),
      max_target_bitrate_kbps_(-1),
      pending_keyframe_(true),
      counter_(0) {
  ivf_file_reader_ = IvfFileReader::Wrap(FileWrapper::OpenReadOnly(filename));
  std::cout << "StreamFromFileEncoder created. Reading from file: " << filename
            << "\n";
}

void StreamFromFileEncoder::SetMaxBitrate(int max_kbps) {
  std::cout << "SetMaxBitrate: " << max_kbps << "\n";
  RTC_DCHECK_GE(max_kbps, -1);  // max_kbps == -1 disables it.
  rtc::CritScope cs(&crit_sect_);
  max_target_bitrate_kbps_ = max_kbps;
}

void StreamFromFileEncoder::SetRates(
    webrtc::VideoEncoder::RateControlParameters const&) {}

int32_t StreamFromFileEncoder::InitEncode(const VideoCodec* config,
                                          int32_t number_of_cores,
                                          size_t max_payload_size) {
  rtc::CritScope cs(&crit_sect_);
  config_ = *config;
  target_bitrate_.SetBitrate(0, 0, config_.startBitrate * 1000);
  configured_input_framerate_ = config_.maxFramerate;
  pending_keyframe_ = true;
  std::cout << "Max frame rate: " << config_.maxFramerate << "\n"
            << "Start bit rate: " << config_.startBitrate << "\n"
            << "Max payload size: " << max_payload_size << "\n";
  return 0;
}

int32_t StreamFromFileEncoder::Encode(
    const VideoFrame& input_image,
    const std::vector<VideoFrameType>* frame_types) {
  EncodedImageCallback* callback;
  bool keyframe;
  int counter;
  {
    rtc::CritScope cs(&crit_sect_);
    callback = callback_;
    keyframe = pending_keyframe_;
    pending_keyframe_ = false;
    counter = counter_++;
  }

  if (!frame_types->empty() &&
      (*frame_types)[0] == VideoFrameType::kVideoFrameKey) {
    keyframe = true;
  }

  if (last_timestamp_) {
    average_time_delta_ = 0.9 * average_time_delta_ +
                          0.1 * (input_image.timestamp() - *last_timestamp_);
  }
  last_timestamp_ = input_image.timestamp();

  if (counter % 150 == 0) {
    std::cout << "Encode is called w input image: " << input_image.width()
              << " x " << input_image.height()
              << "  Average time delta: " << average_time_delta_ << "\n";
  }
  VideoCodecType codec_type;
  EncodedImage encoded_image;
  do {
    if (!ivf_file_reader_->ReadFrame(&encoded_image, &codec_type)) {
      // Start from the beginning of the file.
      ivf_file_reader_->ReadHeader();
      if (!ivf_file_reader_->ReadFrame(&encoded_image, &codec_type)) {
        return false;
      }
    }
  } while (keyframe &&
           encoded_image._frameType != VideoFrameType::kVideoFrameKey);

  encoded_image.SetTimestamp(input_image.timestamp());
  encoded_image.capture_time_ms_ = input_image.render_time_ms();
  CodecSpecificInfo codec_specific;
  codec_specific.codecType = codec_type;
  switch (codec_type) {
    case kVideoCodecVP8:
      codec_specific.codecSpecific.VP8.keyIdx = kNoKeyIdx;
      codec_specific.codecSpecific.VP8.nonReference = false;
      break;
    case kVideoCodecVP9:
      codec_specific.codecSpecific.VP9.temporal_idx = 255;
      codec_specific.codecSpecific.VP9.num_spatial_layers = 1;
      codec_specific.codecSpecific.VP9.first_frame_in_picture = true;
      codec_specific.codecSpecific.VP9.end_of_picture = true;

      codec_specific.codecSpecific.VP9.inter_pic_predicted =
          encoded_image._frameType != VideoFrameType::kVideoFrameKey;
      break;
    case kVideoCodecH264: {
      codec_specific.codecSpecific.H264.packetization_mode =
          H264PacketizationMode::SingleNalUnit;
      codec_specific.codecSpecific.H264.temporal_idx = 255;
      codec_specific.codecSpecific.H264.base_layer_sync = false;
      codec_specific.codecSpecific.H264.idr_frame =
          encoded_image._frameType == VideoFrameType::kVideoFrameKey;
      break;
    }
    default:
      RTC_DCHECK(false);
  }
  if (callback->OnEncodedImage(encoded_image, &codec_specific, nullptr).error !=
      EncodedImageCallback::Result::OK) {
    return -1;
  }

  return 0;
}

int32_t StreamFromFileEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  rtc::CritScope cs(&crit_sect_);
  callback_ = callback;
  return 0;
}

int32_t StreamFromFileEncoder::Release() {
  return 0;
}

const char* StreamFromFileEncoder::kImplementationName = "file_encoder";
VideoEncoder::EncoderInfo StreamFromFileEncoder::GetEncoderInfo() const {
  EncoderInfo info;
  info.implementation_name = kImplementationName;
  return info;
}

int StreamFromFileEncoder::GetConfiguredInputFramerate() const {
  rtc::CritScope cs(&crit_sect_);
  return configured_input_framerate_;
}

}  // namespace webrtc