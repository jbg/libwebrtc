/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/encoded_image.h"

#include <string.h>
#include <vector>

namespace webrtc {

namespace {

// Basic implementation of EncodedImageBufferInterface, using a std::vector for
// the underlying storage.
// TODO(nisse): Expose this utility class in some (internal?) header file, for
// use by encoders.
class EncodedImageBuffer : public EncodedImageBufferInterface {
 public:
  explicit EncodedImageBuffer(size_t size);
  EncodedImageBuffer(const uint8_t* data, size_t size);
  const uint8_t* data() const override;
  uint8_t* data() override;
  size_t size() const override;

 private:
  std::vector<uint8_t> buffer_;
};

EncodedImageBuffer::EncodedImageBuffer(size_t size) : buffer_(size) {}

EncodedImageBuffer::EncodedImageBuffer(const uint8_t* data, size_t size)
    : buffer_(data, data + size) {}

const uint8_t* EncodedImageBuffer::data() const {
  return buffer_.data();
}
uint8_t* EncodedImageBuffer::data() {
  return buffer_.data();
}
size_t EncodedImageBuffer::size() const {
  return buffer_.size();
}

}  // namespace

EncodedImage::EncodedImage() : EncodedImage(nullptr, 0, 0) {}

EncodedImage::EncodedImage(EncodedImage&&) = default;
EncodedImage::EncodedImage(const EncodedImage&) = default;

EncodedImage::EncodedImage(uint8_t* buffer, size_t size, size_t capacity)
    : size_(size), buffer_(buffer), capacity_(capacity) {}

EncodedImage::~EncodedImage() = default;

EncodedImage& EncodedImage::operator=(EncodedImage&&) = default;
EncodedImage& EncodedImage::operator=(const EncodedImage&) = default;

void EncodedImage::Retain() {
  if (buffer_) {
    encoded_data_ = new EncodedImageBuffer(buffer_, size_);
    buffer_ = nullptr;
  }
}

void EncodedImage::Allocate(size_t capacity) {
  encoded_data_ = new EncodedImageBuffer(capacity);
  buffer_ = nullptr;
}

void EncodedImage::SetEncodeTime(int64_t encode_start_ms,
                                 int64_t encode_finish_ms) {
  timing_.encode_start_ms = encode_start_ms;
  timing_.encode_finish_ms = encode_finish_ms;
}

absl::optional<size_t> EncodedImage::SpatialLayerFrameSize(
    int spatial_index) const {
  RTC_DCHECK_GE(spatial_index, 0);
  RTC_DCHECK_LE(spatial_index, spatial_index_.value_or(0));

  auto it = spatial_layer_frame_size_bytes_.find(spatial_index);
  if (it == spatial_layer_frame_size_bytes_.end()) {
    return absl::nullopt;
  }

  return it->second;
}

void EncodedImage::SetSpatialLayerFrameSize(int spatial_index,
                                            size_t size_bytes) {
  RTC_DCHECK_GE(spatial_index, 0);
  RTC_DCHECK_LE(spatial_index, spatial_index_.value_or(0));
  RTC_DCHECK_GE(size_bytes, 0);
  spatial_layer_frame_size_bytes_[spatial_index] = size_bytes;
}

}  // namespace webrtc
