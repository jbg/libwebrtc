/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/include/video_codec_interface.h"

#include <utility>

namespace webrtc {
GenericFrameInfo::GenericFrameInfo() = default;
GenericFrameInfo::GenericFrameInfo(const GenericFrameInfo&) = default;
GenericFrameInfo::~GenericFrameInfo() = default;

GenericFrameInfo::Builder::Builder() = default;
GenericFrameInfo::Builder::~Builder() = default;

GenericFrameInfo::Builder::operator GenericFrameInfo() const {
  return info_;
}

GenericFrameInfo::Builder& GenericFrameInfo::Builder::tl(int temporal_id) {
  info_.temporal_id = temporal_id;
  return *this;
}

GenericFrameInfo::Builder& GenericFrameInfo::Builder::sl(int spatial_id) {
  info_.spatial_id = spatial_id;
  return *this;
}

GenericFrameInfo::Builder& GenericFrameInfo::Builder::indications(
    const std::string& indication_symbols) {
  for (const auto& symbol : indication_symbols) {
    OperatingPointIndication indication;
    switch (symbol) {
      case '-': indication = OperatingPointIndication::kNotPresent; break;
      case 'D': indication = OperatingPointIndication::kDiscardable; break;
      case 'R': indication = OperatingPointIndication::kRequired; break;
      case 'S': indication = OperatingPointIndication::kSwitch; break;
      default: RTC_NOTREACHED();
    }

    info_.operating_points.push_back(indication);
  }

  return *this;
}

GenericFrameInfo::Builder& GenericFrameInfo::Builder::fdiffs(
    absl::InlinedVector<int, 10> frame_diffs) {
  info_.frame_diffs = std::move(frame_diffs);
  return *this;
}

GenericFrameInfo::Builder& GenericFrameInfo::Builder::cdiffs(
    absl::InlinedVector<int, 10> chain_diffs) {
  info_.chain_diffs = std::move(chain_diffs);
  return *this;
}

TemplateStructure::TemplateStructure() = default;
TemplateStructure::TemplateStructure(const TemplateStructure&) = default;
TemplateStructure::~TemplateStructure() = default;

CodecSpecificInfo::CodecSpecificInfo() : codecType(kVideoCodecGeneric) {
  memset(&codecSpecific, 0, sizeof(codecSpecific));
}

CodecSpecificInfo::~CodecSpecificInfo() = default;

}  // namespace webrtc
