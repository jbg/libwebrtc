/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_ITEM_H_
#define API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_ITEM_H_

#include <functional>
#include <memory>
#include <utility>

#include "api/field_trials_view.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"

namespace webrtc {

// Carries factory function for single video format.
// This class is very similar in functionality to VideoDecoderFactory, but
// implemented as a class to be easily copyable.
// This class is coupled with VideoDecoderFactoryCombiner that converts set of
// objects of this class into proper VideoDecoderFactory.
class VideoDecoderFactoryItem {
 public:
  VideoDecoderFactoryItem(
      SdpVideoFormat format,
      std::function<std::unique_ptr<VideoDecoder>(const FieldTrialsView&)>
          factory)
      : format_(std::move(format)), factory_(std::move(factory)) {}

  VideoDecoderFactoryItem(const VideoDecoderFactoryItem&) = default;
  VideoDecoderFactoryItem(VideoDecoderFactoryItem&&) = default;
  VideoDecoderFactoryItem& operator=(const VideoDecoderFactoryItem&) = default;
  VideoDecoderFactoryItem& operator=(VideoDecoderFactoryItem&&) = default;

  ~VideoDecoderFactoryItem() = default;

  const SdpVideoFormat& format() const { return format_; }
  std::function<std::unique_ptr<VideoDecoder>(const FieldTrialsView&)>
  ExtractFactory() {
    return std::move(factory_);
  }

 private:
  SdpVideoFormat format_;
  std::function<std::unique_ptr<VideoDecoder>(const FieldTrialsView&)> factory_;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_ITEM_H_
