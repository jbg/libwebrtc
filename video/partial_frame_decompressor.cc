/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/partial_frame_decompressor.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

PartialFrameDecompressor::PartialFrameDecompressor() = default;
PartialFrameDecompressor::~PartialFrameDecompressor() = default;

bool PartialFrameDecompressor::ApplyPartialUpdate(
    VideoFrameBuffer* input_buffer,
    VideoFrame* uncompressed_frame,
    const VideoFrame::PartialFrameDescription* partial_desc) {
  int changed_rect_width = input_buffer ? input_buffer->width() : 0;
  int changed_rect_height = input_buffer ? input_buffer->height() : 0;
  if (input_buffer && input_buffer->height() == partial_desc->full_height &&
      input_buffer->width() == partial_desc->full_width) {
    // Move whole current picture to the cached buffer. May need to resize
    // or create the cache buffer.
    if (!cached_frame_buffer_ ||
        cached_frame_buffer_->height() < partial_desc->full_height ||
        cached_frame_buffer_->width() < partial_desc->full_width) {
      cached_frame_buffer_ = I420Buffer::Create(partial_desc->full_width,
                                                partial_desc->full_height);
    }
    cached_frame_buffer_->PasteFrom(*input_buffer->ToI420().get(), 0, 0);
  } else {
    // Have to apply partial input picture to the cached buffer.
    // Check all possible error situations.
    if (!cached_frame_buffer_) {
      RTC_LOG(LS_ERROR) << "Partial picture received but no cached full picture"
                           "present.";
      return false;
    }
    if (cached_frame_buffer_->height() != partial_desc->full_height ||
        cached_frame_buffer_->width() != partial_desc->full_width) {
      RTC_LOG(LS_ERROR) << "Partial picture received but cached picture has "
                           "wrong dimensions. Cached: "
                        << cached_frame_buffer_->width() << "x"
                        << cached_frame_buffer_->height()
                        << ", New:" << partial_desc->full_width << "x"
                        << partial_desc->full_height << ".";
      // Reset cached buffer because we can't update it with a new picture.
      cached_frame_buffer_.release();
      return false;
    }
    if (partial_desc->offset_x % 2 != 0 || partial_desc->offset_y % 2 != 0) {
      RTC_LOG(LS_ERROR) << "Partial picture required to be at even offset."
                           " Actual: ("
                        << partial_desc->offset_x << ", "
                        << partial_desc->offset_y << ").";
      cached_frame_buffer_.release();
      return false;
    }
    if ((changed_rect_width % 2 != 0 &&
         changed_rect_width + partial_desc->offset_x <
             partial_desc->full_width) ||
        (changed_rect_height % 2 != 0 &&
         changed_rect_height + partial_desc->offset_y <
             partial_desc->full_height)) {
      RTC_LOG(LS_ERROR) << "Partial picture required to have even dimensions."
                           " Actual: "
                        << input_buffer->width() << "x"
                        << input_buffer->height() << ".";
      cached_frame_buffer_.release();
      return false;
    }
    if (partial_desc->offset_x + changed_rect_width >
            partial_desc->full_width ||
        partial_desc->offset_y + changed_rect_height >
            partial_desc->full_height) {
      RTC_LOG(LS_ERROR) << "Partial picture is outside of bounds.";
      cached_frame_buffer_.release();
      return false;
    }
    // No errors: apply new image to the cache and use the result.
    if (input_buffer) {
      cached_frame_buffer_->PasteFrom(*input_buffer->ToI420().get(),
                                      partial_desc->offset_x,
                                      partial_desc->offset_y);
      uncompressed_frame->set_changed(true);
    } else {
      uncompressed_frame->set_changed(false);
    }
    uncompressed_frame->set_video_frame_buffer(
        I420Buffer::Copy(*cached_frame_buffer_.get()));
  }
  return true;
}

void PartialFrameDecompressor::Reset() {
  cached_frame_buffer_.release();
}

}  // namespace webrtc
