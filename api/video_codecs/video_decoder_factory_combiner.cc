/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_decoder_factory_combiner.h"

#include <string>
#include <utility>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_decoder_factory_item.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

VideoDecoderFactoryCombiner::VideoDecoderFactoryCombiner(
    std::vector<std::vector<VideoDecoderFactoryItem>> formats)
    : field_trials_(default_field_trials_) {
  ConstructFromItems(std::move(formats));
}

VideoDecoderFactoryCombiner::VideoDecoderFactoryCombiner(
    std::vector<std::vector<VideoDecoderFactoryItem>> formats,
    const FieldTrialsView& field_trials ABSL_ATTRIBUTE_LIFETIME_BOUND)
    : field_trials_(field_trials) {
  ConstructFromItems(std::move(formats));
}

void VideoDecoderFactoryCombiner::ConstructFromItems(
    std::vector<std::vector<VideoDecoderFactoryItem>> items) {
  for (auto& array : items) {
    for (VideoDecoderFactoryItem& item : array) {
      if (Find(item.format()) == formats_.size()) {
        formats_.push_back(item.format());
        factories_.push_back(item.ExtractFactory());
      }
    }
  }
}

std::vector<SdpVideoFormat> VideoDecoderFactoryCombiner::GetSupportedFormats()
    const {
  return formats_;
}

std::unique_ptr<VideoDecoder> VideoDecoderFactoryCombiner::CreateVideoDecoder(
    const SdpVideoFormat& format) {
  if (size_t index = Find(format); index < factories_.size()) {
    return factories_[index](field_trials_);
  }
  return nullptr;
}

size_t VideoDecoderFactoryCombiner::Find(const SdpVideoFormat& format) const {
  return absl::c_find_if(formats_,
                         [&](const SdpVideoFormat& rhs) {
                           return std::tie(format.name, format.parameters) ==
                                  std::tie(rhs.name, rhs.parameters);
                         }) -
         formats_.begin();
}

}  // namespace webrtc
