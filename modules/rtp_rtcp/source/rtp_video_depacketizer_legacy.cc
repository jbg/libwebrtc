/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_video_depacketizer_legacy.h"

#include "modules/rtp_rtcp/source/rtp_format.h"
#include "rtc_base/checks.h"

namespace webrtc {

absl::optional<RtpVideoDepacketizer::Parsed> RtpVideoDepacketizerLegacy::Parse(
    rtc::CopyOnWriteBuffer rtp_payload) {
  auto rtp_depacketizer = RtpDepacketizer::Create(codec_);
  RTC_CHECK(rtp_depacketizer);
  RtpDepacketizer::ParsedPayload parsed_payload;
  if (!rtp_depacketizer->Parse(&parsed_payload, rtp_payload.cdata(),
                               rtp_payload.size())) {
    return absl::nullopt;
  }
  absl::optional<RtpVideoDepacketizer::Parsed> result(absl::in_place);
  result->video_header = parsed_payload.video;
  result->video_payload.SetData(parsed_payload.payload,
                                parsed_payload.payload_length);
  return result;
}

}  // namespace webrtc
