/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stddef.h>
#include <stdint.h>

#include "api/video/video_frame_type.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_packetizer_av1.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

void FetchAllPacketsAndValidateLimits(
    RtpPacketizerAv1* packetizer,
    const RtpPacketizer::PayloadSizeLimits& limits) {
  RtpPacketToSend rtp_packet(nullptr);
  size_t num_packets = packetizer->NumPackets();
  // Single packet.
  if (num_packets == 1) {
    RTC_CHECK(packetizer->NextPacket(&rtp_packet));
    RTC_CHECK_LE(rtp_packet.payload_size(),
                 limits.max_payload_len - limits.single_packet_reduction_len);
    return;
  }
  // First packet.
  RTC_CHECK(packetizer->NextPacket(&rtp_packet));
  RTC_CHECK_LE(rtp_packet.payload_size(),
               limits.max_payload_len - limits.first_packet_reduction_len);
  // Middle packets.
  for (size_t i = 1; i < num_packets - 1; ++i) {
    RTC_CHECK(packetizer->NextPacket(&rtp_packet))
        << "Failed to get packet#" << i;
    // Add 'i' to comparision to easiliy see from the error message which packet
    // it fails at.
    RTC_CHECK_LE(i * 10000 + rtp_packet.payload_size(),
                 i * 10000 + limits.max_payload_len);
  }
  // Last packet.
  RTC_CHECK(packetizer->NextPacket(&rtp_packet));
  RTC_CHECK_LE(rtp_packet.payload_size(),
               limits.max_payload_len - limits.last_packet_reduction_len);
}

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kConfigSize = 4;
  if (size < kConfigSize) {
    return;
  }
  RtpPacketizer::PayloadSizeLimits limits;
  limits.max_payload_len = 1200;
  limits.first_packet_reduction_len = data[0];
  limits.last_packet_reduction_len = data[1];
  limits.single_packet_reduction_len = data[2];
  VideoFrameType frame_type = (data[3] % 2) == 0
                                  ? VideoFrameType::kVideoFrameKey
                                  : VideoFrameType::kVideoFrameDelta;

  RtpPacketizerAv1 packetizer(
      rtc::MakeArrayView(data, size).subview(kConfigSize), limits, frame_type);

  size_t num_packets = packetizer.NumPackets();
  if (num_packets > 0) {
    // When packetization was successful, validate NextPacket function too.
    // While at it, check that packets respect the payload size limits.
    FetchAllPacketsAndValidateLimits(&packetizer, limits);
  }
}
}  // namespace webrtc
