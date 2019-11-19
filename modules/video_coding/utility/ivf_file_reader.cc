/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/ivf_file_reader.h"

#include <vector>

#include "api/video_codecs/video_codec.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr size_t kIvfHeaderSize = 32;
constexpr size_t kIvfFrameHeaderSize = 12;

}  // namespace

bool IvfFileReader::Reset() {
  // Set error to true while initialization.
  has_error_ = true;
  if (!file_.Rewind()) {
    RTC_LOG(LS_ERROR) << "Failed to rewind IVF file";
    return false;
  }

  uint8_t ivf_header[kIvfHeaderSize] = {0};
  size_t read = file_.Read(&ivf_header, kIvfHeaderSize);
  if (read != kIvfHeaderSize) {
    RTC_LOG(LS_ERROR) << "Failed to read IVF header";
    return false;
  }

  absl::optional<VideoCodecType> codec_type = ParseCodecType(ivf_header, 8);
  if (!codec_type) {
    return false;
  }
  codec_type_ = *codec_type;

  width_ = ByteReader<uint16_t>::ReadLittleEndian(&ivf_header[12]);
  height_ = ByteReader<uint16_t>::ReadLittleEndian(&ivf_header[14]);
  if (width_ == 0 || height_ == 0) {
    RTC_LOG(LS_ERROR) << "Invalid IVF header: width or height is 0";
    return false;
  }

  uint32_t time_scale = ByteReader<uint32_t>::ReadLittleEndian(&ivf_header[16]);
  if (time_scale == 1000) {
    using_capture_timestamps_ = true;
  } else if (time_scale == 90000) {
    using_capture_timestamps_ = false;
  } else {
    RTC_LOG(LS_ERROR) << "Invalid IVF header: Unknown time scale";
    return false;
  }

  num_frames_ = static_cast<size_t>(
      ByteReader<uint32_t>::ReadLittleEndian(&ivf_header[24]));
  if (num_frames_ <= 0) {
    RTC_LOG(LS_ERROR) << "Invalid IVF header: number of frames 0 or negative";
    return false;
  }

  num_read_frames_ = 0;
  next_frame_header_ = NextFrameHeader();
  if (!next_frame_header_) {
    RTC_LOG(LS_ERROR) << "Failed to read 1st frame header";
    return false;
  }
  // Initialization succeed: reset error.
  has_error_ = false;

  const char* codec_name = CodecTypeToPayloadString(codec_type_);
  RTC_LOG(INFO) << "Opened IVF file with codec data of type " << codec_name
                << " at resolution " << width_ << " x " << height_ << ", using "
                << (using_capture_timestamps_ ? "1" : "90")
                << "kHz clock resolution.";

  return true;
}

absl::optional<EncodedImage> IvfFileReader::NextFrame() {
  if (has_error_) {
    return absl::nullopt;
  }
  if (!HasMoreFrames()) {
    return absl::nullopt;
  }

  rtc::scoped_refptr<EncodedImageBuffer> payload = nullptr;
  std::vector<size_t> layers_size;
  int64_t current_timestamp = next_frame_header_->timestamp;
  while (next_frame_header_ &&
         current_timestamp == next_frame_header_->timestamp) {
    // Resize payload to fit next spatial layer.
    size_t current_layer_size = next_frame_header_->frame_size;
    size_t current_layer_start_pos = 0;
    if (payload == nullptr) {
      payload = EncodedImageBuffer::Create(current_layer_size);
    } else {
      current_layer_start_pos = payload->size();
      payload->Realloc(payload->size() + current_layer_size);
    }
    layers_size.push_back(current_layer_size);

    // Read next layer into payload
    size_t read = file_.Read(&payload->data()[current_layer_start_pos],
                             current_layer_size);
    if (read != current_layer_size) {
      RTC_LOG(LS_ERROR) << "Frame #" << num_read_frames_
                        << ": failed to read frame payload";
      has_error_ = true;
      return absl::nullopt;
    }

    current_timestamp = next_frame_header_->timestamp;
    next_frame_header_ = NextFrameHeader();
  }
  if (!next_frame_header_) {
    // We failed to read next frame header. It could happen because of EOF
    // reached or error acquired.
    if (!file_.ReadEof()) {
      RTC_LOG(LS_ERROR) << "Failed to read next frame header";
      has_error_ = true;
      return absl::nullopt;
    }
    // If EOF was reached, we need to check that all frames were met.
    if (num_read_frames_ != num_frames_ - 1) {
      RTC_LOG(LS_ERROR) << "Unexpected EOF";
      has_error_ = true;
      return absl::nullopt;
    }
  }

  EncodedImage image;
  if (using_capture_timestamps_) {
    image.capture_time_ms_ = current_timestamp;
  } else {
    image.SetTimestamp(static_cast<uint32_t>(current_timestamp));
  }
  image.SetEncodedData(payload);
  if (layers_size.size() > 1) {
    image.SetSpatialIndex(static_cast<int>(layers_size.size()));
    for (size_t i = 0; i < layers_size.size(); ++i) {
      image.SetSpatialLayerFrameSize(static_cast<int>(i), layers_size[i]);
    }
  }

  num_read_frames_++;
  return image;
}

absl::optional<VideoCodecType> IvfFileReader::ParseCodecType(uint8_t* buffer,
                                                             size_t start_pos) {
  if (buffer[start_pos] == 'V' && buffer[start_pos + 1] == 'P' &&
      buffer[start_pos + 2] == '8' && buffer[start_pos] == '0') {
    return VideoCodecType::kVideoCodecVP8;
  }
  if (buffer[start_pos] == 'V' && buffer[start_pos + 1] == 'P' &&
      buffer[start_pos + 2] == '9' && buffer[start_pos] == '0') {
    return VideoCodecType::kVideoCodecVP9;
  }
  if (buffer[start_pos] == 'H' && buffer[start_pos + 1] == '2' &&
      buffer[start_pos + 2] == '6' && buffer[start_pos] == '4') {
    return VideoCodecType::kVideoCodecH264;
  }
  RTC_LOG(LS_ERROR) << "Unknown codec type: " << buffer[start_pos]
                    << buffer[start_pos + 1] << buffer[start_pos + 2]
                    << buffer[start_pos + 3];
  return absl::nullopt;
}

absl::optional<IvfFileReader::FrameHeader> IvfFileReader::NextFrameHeader() {
  uint8_t ivf_frame_header[kIvfFrameHeaderSize] = {0};
  size_t read = file_.Read(&ivf_frame_header, kIvfFrameHeaderSize);
  if (read != kIvfFrameHeaderSize) {
    RTC_LOG(LS_ERROR) << "Frame #" << num_read_frames_
                      << ": failed to read IVF frame header";
    return absl::nullopt;
  }
  FrameHeader header;
  header.frame_size = static_cast<size_t>(
      ByteReader<uint32_t>::ReadLittleEndian(&ivf_frame_header[0]));
  header.timestamp =
      ByteReader<uint64_t>::ReadLittleEndian(&ivf_frame_header[4]);

  if (header.frame_size <= 0) {
    RTC_LOG(LS_ERROR) << "Frame #" << num_read_frames_
                      << ": invalid frame size";
    return absl::nullopt;
  }

  if (header.timestamp < 0) {
    RTC_LOG(LS_ERROR) << "Frame #" << num_read_frames_
                      << ": negative timestamp";
    return absl::nullopt;
  }

  return header;
}

}  // namespace webrtc
