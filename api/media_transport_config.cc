/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/media_transport_config.h"

#include "rtc_base/checks.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/strings/string_builder.h"

namespace webrtc {

MediaTransportConfig::MediaTransportConfig(
    MediaTransportInterface* media_transport,
    absl::optional<size_t> rtp_max_packet_size)
    : media_transport(media_transport),
      rtp_max_packet_size(rtp_max_packet_size) {
  RTC_DCHECK(!media_transport || !rtp_max_packet_size);
}

MediaTransportConfig::MediaTransportConfig(
    MediaTransportInterface* media_transport)
    : media_transport(media_transport) {
  RTC_DCHECK(media_transport != nullptr);
}

std::string MediaTransportConfig::DebugString() const {
  rtc::StringBuilder result;
  result << "{media_transport:"
         << (media_transport != nullptr ? "(Transport)" : "null")
         << ", rtp_max_packet_size:"
         << (rtp_max_packet_size ? rtc::ToString(rtp_max_packet_size.value())
                                 : "nullopt")
         << "}";
  return result.Release();
}

}  // namespace webrtc
